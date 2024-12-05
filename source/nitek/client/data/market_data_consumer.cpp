#include "market_data_consumer.h"
#include <vector>

using namespace Common;

namespace Client
{

MarketDataConsumer::MarketDataConsumer(ClientID client_id,
                                       Exchange::MarketUpdateQueue& updates,
                                       const std::string& iface,
                                       const std::string& ip_snapshot, int port_snapshot,
                                       const std::string& ip_incremental,
                                       int port_incremental)
        : tx_updates(updates),
          logger("client_market_data_consumer_"
                 + std::to_string(client_id) + ".log"),
          iface(iface),
          ip_snapshot(ip_snapshot),
          port_snapshot(port_snapshot),
          socket_incremental(logger),
          socket_snapshot(logger) {
    auto default_rx_callback = [this](auto socket) {
        rx_callback(socket);
    };
    socket_incremental.rx_callback = default_rx_callback;
    socket_snapshot.rx_callback = default_rx_callback;
    // incremental socket is fully initialised and joined to multicast group here but we leave
    // the snapshot socket initialisation for later, since sync occurs on an as-needed basis
    auto fd = socket_incremental.init(ip_incremental, iface,
                                      port_incremental, true);
    ASSERT(fd >= 0, "<MDC> error creating UDP socket for consuming incremental market data, "
                    "error: " + std::string(std::strerror(errno)));
    const auto is_joined = socket_incremental.join_group(ip_incremental);
    ASSERT(is_joined, "<MDC> multicast join failed! error: " + std::string(std::strerror(errno)));
}

MarketDataConsumer::~MarketDataConsumer() {
    stop();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);
}

void MarketDataConsumer::start() {
    is_running = true;
    thread = LL::create_and_start_thread(-1, "MarketDataConsumer", [this]() { run(); });
    ASSERT(thread != nullptr, "<MDC> failed to start thread for market data consumer");
}

void MarketDataConsumer::stop() {
    is_running = false;
    if (thread != nullptr && thread->joinable())
        thread->join();
}

void MarketDataConsumer::run() noexcept {
    logger.logf("% <MDC::%> running client data consumer...\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    while (is_running) {
        // rx from both udp sockets
        socket_incremental.tx_and_rx();
        socket_snapshot.tx_and_rx();
    }
}

void MarketDataConsumer::rx_callback(LL::McastSocket* socket) noexcept {
    // rx data on the snapshot socket only when in recovery mode and rebuilding a market snapshot
    const auto is_snapshot = socket->fd == socket_snapshot.fd;
    // log warning if for some reason we get data on snapshot socket while not in recovery
    if (is_snapshot && !is_in_recovery) [[unlikely]] {
        socket->i_rx_next = 0;
        logger.logf("% <MDC::%> WARNING rx'd snapshot message but not in recovery\n",
                    LL::get_time_str(&t_str), __FUNCTION__);
        return;
    }
    if (socket->i_rx_next >= sizeof(Exchange::MDPMarketUpdate)) {
        size_t i{ };
        for (; i + sizeof(Exchange::MDPMarketUpdate) <= socket->i_rx_next;
               i += sizeof(Exchange::MDPMarketUpdate)) {
            auto request = reinterpret_cast<const Exchange::MDPMarketUpdate*>(
                    socket->rx_buffer.data() + i);
            logger.logf("% <MDC::%> rx'd on % socket, len: %, request: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__,
                        (is_snapshot ? "SNAP" : "INC."), sizeof(Exchange::MDPMarketUpdate),
                        request->to_str());
            // recovery begins if we lose track of the sequence number
            const auto already_in_recovery = is_in_recovery;
            is_in_recovery = (already_in_recovery || request->n_seq != n_seq_inc_next);

            if (is_in_recovery) [[unlikely]] {
                if (!already_in_recovery) [[unlikely]] {
                    // if recovery just began, the sync process is started anew
                    logger.logf("% <MDC::%> lost packets on % socket. n_seq expected: %, "
                                "received: %\n", LL::get_time_str(&t_str), __FUNCTION__,
                                (is_snapshot ? "SNAP" : "INC."), n_seq_inc_next, request->n_seq);
                    snapshot_sync_start();
                }
                queue_update(is_snapshot, request); // enqueue the update; maybe snapshot is done
            } else if (!is_snapshot) {
                // incremental data was received in the expected order; process it normally
                logger.logf("% <MDC::%> %\n", LL::get_time_str(&t_str), __FUNCTION__,
                            request->to_str());
                ++n_seq_inc_next;
                auto update_out = tx_updates.get_next_to_write();
                *update_out = request->ome_update;
                tx_updates.increment_write_index();
            }
        }
        memcpy(socket->rx_buffer.data(), socket->rx_buffer.data() + i, socket->i_rx_next - i);
        socket->i_rx_next -= i;
    }
}

void MarketDataConsumer::queue_update(bool is_snapshot, const Exchange::MDPMarketUpdate* update) {
    // queue the update to the correct container for its type
    if (is_snapshot) {
        if (queued_snapshot_updates.find(update->n_seq) != queued_snapshot_updates.end()) {
            // this update n_seq already exists so it means we're receiving a new snapshot update
            // and the old one should be discarded
            logger.logf("% <MDC::%> dropped packets during snapshot recovery, received "
                        "update again: %\n", LL::get_time_str(&t_str),
                        __FUNCTION__, update->to_str());
            queued_snapshot_updates.clear();
        }
        queued_snapshot_updates[update->n_seq] = update->ome_update;
    } else {
        queued_incremental_updates[update->n_seq] = update->ome_update;
    }
    // can recovery/sync now occur?
    snapshot_sync_check();
}

void MarketDataConsumer::snapshot_sync_start() {
    // clear both incremental and snapshot update queues, then join the snapshot multicast stream
    queued_snapshot_updates.clear();
    queued_incremental_updates.clear();
    const auto fd = socket_snapshot.init(ip_snapshot, iface, port_snapshot, true);
    ASSERT(fd >= 0, "<MDC> ERROR creating socket for receiving snapshot stream: "
                    + std::string(strerror(errno)));
    const auto is_joined = socket_snapshot.join_group(ip_snapshot);
    ASSERT(is_joined, "<MDC> ERROR multicast socket join failed! "
                    + std::string(strerror(errno)));
    logger.logf("% <MDC::%> start sync, stream joined at socket fd: %\n",
                LL::get_time_str(&t_str), __FUNCTION__, socket_snapshot.fd);
}

void MarketDataConsumer::snapshot_sync_check() {
    if (queued_snapshot_updates.empty())
         return;

    // do not advance until a SNAPSHOT_START message is rx'd
    using UpdateType = Exchange::OMEMarketUpdate::Type;
    const auto& snapshot_0 = queued_snapshot_updates.begin()->second;
    if (snapshot_0.type != UpdateType::SNAPSHOT_START) {
        logger.logf("% <MDC::%> waiting for SNAPSHOT_START\n", LL::get_time_str(&t_str),
                    __FUNCTION__);
        queued_snapshot_updates.clear();
        return;
    }

    std::vector<Exchange::OMEMarketUpdate> updates_to_process;  // if recovered, process these

    // verify no snapshot packet loss via n_seq
    bool snapshot_is_complete{ true };
    size_t n_seq_snapshot_next{ 0 };
    for (auto& snapshot: queued_snapshot_updates) {
        logger.logf("% <MDC::%> % => %\n", LL::get_time_str(&t_str),
                    __FUNCTION__, snapshot.first, snapshot.second.to_str());
        if (snapshot.first != n_seq_snapshot_next) {
            // packet loss detected due to dropped n_seq in snapshot stream
            snapshot_is_complete = false;
            logger.logf("% <MDC::%> snapshot stream n_seq packet loss. Expected: %, found: %,"
                        " update: %\n", LL::get_time_str(&t_str),
                        __FUNCTION__, n_seq_snapshot_next, snapshot.first, snapshot.second.to_str());
            break;
        }
        if (snapshot.second.type != UpdateType::SNAPSHOT_START
            && snapshot.second.type != UpdateType::SNAPSHOT_END) {
            updates_to_process.push_back(snapshot.second);
        }
        ++n_seq_snapshot_next;
    }

    if (!snapshot_is_complete) {
        logger.logf("% <MDC::%> snapshot sync discarded due to snapshot packet loss\n",
                    LL::get_time_str(&t_str), __FUNCTION__);
        queued_snapshot_updates.clear();
        return;
    }

    // a SNAPSHOT_END message signifies a complete snapshot was rx'd
    const auto& last_snapshot = queued_snapshot_updates.rbegin()->second;
    if (last_snapshot.type != UpdateType::SNAPSHOT_END) {
        logger.logf("% <MDC::%> abandon snapshot sync. Expected SNAPSHOT_END but none found\n",
                    LL::get_time_str(&t_str), __FUNCTION__);
        return;
    }

    // now, check the incremental updates we've received. if we're synced up now, the next n_seq
    // must continue following the end of the last snapshot update
    bool incremental_is_complete{ true };
    size_t n_incrementals{ 0 };
    n_seq_inc_next = last_snapshot.order_id + 1;
    // verify that no incremental packets were dropped
    for (auto& update: queued_incremental_updates) {
        if (update.first < n_seq_inc_next)
            continue;
        if (update.first != n_seq_inc_next) {
            logger.logf("% <MDC::%> incremental stream packet loss. Expected: %, found: %, "
                        "update: %\n", LL::get_time_str(&t_str), __FUNCTION__,
                        n_seq_inc_next, update.first, update.second.to_str());
            incremental_is_complete = false;
            break;
        }

        logger.logf("% <MDC::%> % => %\n", LL::get_time_str(&t_str),
                    __FUNCTION__, update.first, update.second.to_str());

        if (update.second.type != UpdateType::SNAPSHOT_START
            && update.second.type != UpdateType::SNAPSHOT_END) {
            updates_to_process.push_back(update.second);
        }

        ++n_seq_inc_next;
        ++n_incrementals;
    }

    if (!incremental_is_complete) {
        logger.logf("% <MDC::%> snapshot sync discarded due to incremental update packet loss\n",
                   LL::get_time_str(&t_str), __FUNCTION__);
        queued_snapshot_updates.clear();
        return;
    }

    // if we've made it this far, snapshot sync is ok -> write out all updates to trading engine
    for (const auto& update: updates_to_process) {
        auto tx_update = tx_updates.get_next_to_write();
        *tx_update = update;
        tx_updates.increment_write_index();
    }

    logger.logf("% <MDC::%> snapshot recovery complete. Rx'd % snapshot and % incremental "
                "data\n", LL::get_time_str(&t_str), __FUNCTION__, queued_snapshot_updates.size()-2,
                n_incrementals);

    // recovery complete, so we leave the stream group and enter a stable recovered state
    queued_snapshot_updates.clear();
    queued_incremental_updates.clear();
    is_in_recovery = false;
    socket_snapshot.leave_group();
}

}

