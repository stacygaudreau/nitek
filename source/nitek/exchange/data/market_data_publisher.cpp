#include "market_data_publisher.h"


namespace Exchange
{
MarketDataPublisher::MarketDataPublisher(
        MarketUpdateQueue& ome_market_updates, const std::string& iface,
        const std::string& ip_snapshot, int port_snapshot, const std::string& ip_incremental,
        int port_incremental)
        : ome_market_updates(ome_market_updates),
          logger("exchange_market_data_publisher.log"),
          socket_incremental(logger) {
    auto fd = socket_incremental.init(ip_incremental, iface,
                                      port_incremental, false);
    ASSERT(fd >= 0, "<MDP> error creating UDP socket for incremental market data");
    synthesizer = std::make_unique<SnapshotSynthesizer>(tx_snapshot_updates,
                                                        iface, ip_snapshot,
                                                        port_snapshot);
}

MarketDataPublisher::~MarketDataPublisher() {
    stop();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);
}

void MarketDataPublisher::start() {
    is_running = true;
    thread = LL::create_and_start_thread(-1, "MarketDataPublisher",
                                         [this]() { run(); });
    ASSERT(thread != nullptr, "<MDP> Failed to start thread for market data publisher");
    synthesizer->start();
}

void MarketDataPublisher::stop() {
    is_running = false;
    if (thread != nullptr && thread->joinable())
        thread->join();
    synthesizer->stop();
}

void MarketDataPublisher::run() noexcept {
    logger.logf("% <MDP::%> running data publisher...\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    while (is_running) {
        // read and disseminate the matching engine's updates from the queue
        for (auto u = ome_market_updates.get_next_to_read();
             ome_market_updates.size() && u;
             u = ome_market_updates.get_next_to_read()) {
            logger.logf("% <MDP::%> sending n_seq: %, update: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__, n_seq_next, u->to_str());
            // the correct publisher data format has a sequence number prepended to the update
            socket_incremental.load_tx(&n_seq_next, sizeof(n_seq_next));
            socket_incremental.load_tx(u, sizeof(OMEMarketUpdate));
            ome_market_updates.increment_read_index();
            // update must also be sent to the SS so that it can update its market snapshot
            auto write_next = tx_snapshot_updates.get_next_to_write();
            write_next->n_seq = n_seq_next;
            write_next->ome_update = *u;
            tx_snapshot_updates.increment_write_index();
            ++n_seq_next;
        }
        socket_incremental.tx_and_rx();
    }
}
}
