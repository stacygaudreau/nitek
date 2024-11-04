#include "snapshot_synthesizer.h"


namespace Exchange
{
SnapshotSynthesizer::SnapshotSynthesizer(MDPMarketUpdateQueue& tx_updates, const std::string& iface,
                                         const std::string& ip, int port)
        : tx_updates(tx_updates),
          logger("exchange_snapshot_synthesizer.log"),
          socket(logger) {
    auto fd = socket.init(ip, iface, port, false);
    ASSERT(fd >= 0, "<SnapshotSynthesizer> error creating UDP socket for snapshot data");

}

SnapshotSynthesizer::~SnapshotSynthesizer() {
    stop();
}

void SnapshotSynthesizer::start() {
    is_running = true;
    thread = LL::create_and_start_thread(-1, "SnapshotSynthesizer",
                                         [this]() { run(); });
    ASSERT(thread != nullptr, "<SnapshotSynthesizer> Failed to start thread for "
                              "snapshot synthesizer");
}

void SnapshotSynthesizer::stop() {
    is_running = false;
    if (thread != nullptr && thread->joinable())
        thread->join();
}

void SnapshotSynthesizer::run() {
    logger.logf("% <SS::%> running snapshot synthesizer...\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    while (is_running) {
        // process each incremental update rx'd from the MDP on the queue
        for (auto update = tx_updates.get_next_to_read();
             tx_updates.size() && update; update = tx_updates.get_next_to_read()) {
            logger.logf("% <SS::%> process update %\n",
                        LL::get_time_str(&t_str), __FUNCTION__, update->to_str());
            add_to_snapshot(update);
            tx_updates.increment_read_index();
        }
        // a snapshot is published at a defined interval
        if (LL::get_time_nanos() - t_last_snapshot
                > SECONDS_BETWEEN_SNAPSHOTS * LL::NANOS_TO_SECS) {
            t_last_snapshot = LL::get_time_nanos();
            publish_snapshot();
        }
    }
}

void SnapshotSynthesizer::add_to_snapshot(const MDPMarketUpdate* update_from_publisher) {
    // the update is handled similarly to updating the order book in the matching engine,
    //  except only the most recent picture of the market is maintained
    const auto& update = update_from_publisher->ome_update;
    auto* orders = &map_ticker_to_order.at(update.ticker_id);
    using OrderType = OMEMarketUpdate::Type;
    switch (update.type) {
    case OrderType::ADD: {
        // update is inserted at the correct location specified by OID
        auto order = orders->at(update.order_id);
        ASSERT(order == nullptr, "<SS> order already exists for update: " + update.to_str()
                + ", order: " + (order ? order->to_str() : ""));
        orders->at(update.order_id) = update_pool.allocate(update);
        break;
    }
    case OrderType::MODIFY: {
        // modify only the qty and price members on an existing OID
        auto order = orders->at(update.order_id);
        ASSERT(order != nullptr, "<SS> order does not exist for update: " + update.to_str());
        ASSERT(order->order_id == update.order_id, "<SS> expected existing order to match id!");
        ASSERT(order->side == update.side, "<SS> expected existing order to match side!");
        order->qty = update.qty;
        order->price = update.price;
        break;
    }
    case OrderType::CANCEL: {
        // find and deallocate existing OID from the pool
        auto order = orders->at(update.order_id);
        ASSERT(order != nullptr, "<SS> order does not exist for update: " + update.to_str());
        ASSERT(order->order_id == update.order_id, "<SS> expected existing order to match id!");
        ASSERT(order->side == update.side, "<SS> expected existing order to match side!");
        update_pool.deallocate(order);
        orders->at(update.order_id) = nullptr;
        break;
    }
    default:
        break;
    }
    // update sequence to reflect last seen in incremental updates
    ASSERT(update_from_publisher->n_seq == n_seq_last + 1,
           "<SS> expected an increase in update n_seq");
    n_seq_last = update_from_publisher->n_seq;
}

void SnapshotSynthesizer::publish_snapshot() {
    size_t size_snapshot{ };
    // snapshot begins with start message
    const MDPMarketUpdate SNAPSHOT_START{ size_snapshot++,
                                          { OMEMarketUpdate::Type::SNAPSHOT_START, n_seq_last }};
    logger.logf("% <SS::%> %\n",
                LL::get_time_str(&t_str), __FUNCTION__, SNAPSHOT_START.to_str());
    socket.load_tx(&SNAPSHOT_START, sizeof(MDPMarketUpdate));
    // each ticker in the order book is added to the snapshot
    for (size_t ticker{ }; ticker < map_ticker_to_order.size(); ++ticker) {
        const auto& orders = map_ticker_to_order.at(ticker);
        OMEMarketUpdate update;
        update.type = OMEMarketUpdate::Type::CLEAR;
        update.ticker_id = ticker;
        // client is told to clear book for the ticker
        const MDPMarketUpdate CLEAR_TICKER{ size_snapshot++, update };
        logger.logf("% <SS::%> %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, CLEAR_TICKER.to_str());
        socket.load_tx(&CLEAR_TICKER, sizeof(MDPMarketUpdate));
        // each order for the ticker is then updated
        for (const auto order: orders) {
            if (order != nullptr) {
                const MDPMarketUpdate TICKER_UPDATE{ size_snapshot++, *order };
                logger.logf("% <SS::%> %\n",
                            LL::get_time_str(&t_str), __FUNCTION__, TICKER_UPDATE.to_str());
                socket.load_tx(&TICKER_UPDATE, sizeof(MDPMarketUpdate));
                socket.tx_and_rx();
            }
        }
    }
    // snapshot ends
    const MDPMarketUpdate SNAPSHOT_END{ size_snapshot++,
                                        { OMEMarketUpdate::Type::SNAPSHOT_END, n_seq_last }};
    logger.logf("% <SS::%> %\n",
                LL::get_time_str(&t_str), __FUNCTION__, SNAPSHOT_END.to_str());
    socket.load_tx(&SNAPSHOT_END, sizeof(MDPMarketUpdate));
    socket.tx_and_rx();
    logger.logf("% <SS::%> snapshot published, size: % orders\n",
                LL::get_time_str(&t_str), __FUNCTION__, size_snapshot - 1);
}

}