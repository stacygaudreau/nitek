#include "gtest/gtest.h"
#include "exchange/data/market_data_publisher.h"
#include "exchange/orders/order_matching_engine.h"
#include "client/data/market_data_consumer.h"
#include "exchange/data/ome_market_update.h"
#include <string>
#include <vector>


using namespace Exchange;
using namespace Client;
using namespace LL;
using namespace std::literals::chrono_literals;


/*
 * Base tests for MarketDataConsumer
 */
class MarketDataConsumerBasics : public ::testing::Test {
protected:
    std::string IFACE{ "lo" };
    std::string IP_SNAPSHOT{ "239.0.0.2" }; // snapshots multicast group IP
    std::string IP_INCREMENTAL{ "239.0.0.1" };  // multicast group IP for incremental updates
    int PORT_SNAPSHOT{ 12345 };
    int PORT_INCREMENTAL{ 23456 };
    MarketUpdateQueue updates{ Limits::MAX_MARKET_UPDATES };

    void SetUp() override {

    }
    void TearDown() override {
    }
};


TEST_F(MarketDataConsumerBasics, is_constructed) {
    auto mdc = std::make_unique<MarketDataConsumer>(2, updates,
                                                    IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                    IP_INCREMENTAL, PORT_INCREMENTAL);
    EXPECT_NE(mdc, nullptr);
}

TEST_F(MarketDataConsumerBasics, starts_and_stops_worker_thread) {
    auto mdc = std::make_unique<MarketDataConsumer>(2, updates,
                                                    IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                    IP_INCREMENTAL, PORT_INCREMENTAL);
    mdc->start();
    EXPECT_TRUE(mdc->is_running);
    std::this_thread::sleep_for(20ms);
    mdc->stop();
    EXPECT_FALSE(mdc->is_running);
}

TEST_F(MarketDataConsumerBasics, snapshot_sync_is_started) {
    // a snapshot sync is started and the consumer connects to the stream
    auto mdc = std::make_unique<MarketDataConsumer>(2, updates,
                                                    IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                    IP_INCREMENTAL, PORT_INCREMENTAL);
    // should already be connected to incremental stream
    EXPECT_NE(-1, mdc->socket_incremental.fd);
    // snapshot stream starts out disconnected
    EXPECT_EQ(-1, mdc->socket_snapshot.fd);
    // after starting snapshot sync, the socket should have bound successfully
    mdc->snapshot_sync_start();
    std::this_thread::sleep_for(10ms);
    EXPECT_NE(-1, mdc->socket_snapshot.fd);
}

TEST_F(MarketDataConsumerBasics, incremental_update_is_queued) {
    // an incremental update is queued by the queue_update method
    auto mdc = std::make_unique<MarketDataConsumer>(2, updates,
                                                    IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                    IP_INCREMENTAL, PORT_INCREMENTAL);
    auto ome_update = OMEMarketUpdate{OMEMarketUpdate::Type::TRADE, 99, 3,
                                      Side::BUY, 29, 100, 4};
    auto update = MDPMarketUpdate{ 1,  ome_update};
    EXPECT_EQ(mdc->queued_incremental_updates.size(), 0);
    mdc->queue_update(false, &update);
    auto res = mdc->queued_incremental_updates.find(update.n_seq);
    EXPECT_EQ(res->second.order_id, ome_update.order_id);
    EXPECT_EQ(res->second.ticker_id, ome_update.ticker_id);
    EXPECT_EQ(res->second.side, ome_update.side);
    EXPECT_EQ(res->second.price, ome_update.price);
    EXPECT_EQ(res->second.qty, ome_update.qty);
    EXPECT_EQ(res->second.priority, ome_update.priority);
    EXPECT_EQ(res->second.type, ome_update.type);
}

TEST_F(MarketDataConsumerBasics, snapshot_update_is_queued) {
    // the queue_update method's snapshot processing is tested
    auto mdc = std::make_unique<MarketDataConsumer>(2, updates,
                                                    IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                    IP_INCREMENTAL, PORT_INCREMENTAL);
    auto ome_update = OMEMarketUpdate{OMEMarketUpdate::Type::TRADE, 99, 3,
                                      Side::BUY, 29, 100, 4};
    auto update = MDPMarketUpdate{ 1,  ome_update};
    EXPECT_EQ(mdc->queued_snapshot_updates.size(), 0);
    // sending a market order update before a SNAPSHOT_START refuses to queue the update
    mdc->queue_update(true, &update);
    EXPECT_EQ(mdc->queued_snapshot_updates.size(), 0); // the queue is still empty!
    // send a SNAPSHOT_START first
    auto snapshot_start = MDPMarketUpdate{ 0, {OMEMarketUpdate::Type::SNAPSHOT_START, 2, 3,
                                               Side::INVALID, 0, 0, 0}};
    mdc->queue_update(true, &snapshot_start);
    EXPECT_EQ(mdc->queued_snapshot_updates.size(), 1);
    // now, the update should actually be queued
    mdc->queue_update(true, &update);
    EXPECT_EQ(mdc->queued_snapshot_updates.size(), 2);
}

TEST_F(MarketDataConsumerBasics, starts_recovered) {
    // the consumer should begin in a recovered state when running
    auto mdc = std::make_unique<MarketDataConsumer>(2, updates,
                                                    IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                    IP_INCREMENTAL, PORT_INCREMENTAL);
    mdc->start();
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(mdc->is_in_recovery);
}

TEST_F(MarketDataConsumerBasics, incoming_update_is_pushed_out) {
    // a complete snapshot is queued and an encapsulated update is pushed out
    // of the consumer into what will ultimately be the trading engine
    auto mdc = std::make_unique<MarketDataConsumer>(2, updates,
                                                    IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                    IP_INCREMENTAL, PORT_INCREMENTAL);
    auto ome_update = OMEMarketUpdate{OMEMarketUpdate::Type::TRADE, 99, 3,
                                      Side::BUY, 29, 100, 4};
    auto update = MDPMarketUpdate{ 1,  ome_update};
    mdc->is_in_recovery = true; // force recovery mode
    EXPECT_EQ(mdc->queued_snapshot_updates.size(), 0);
    // SNAPSHOT_START
    auto snapshot_start = MDPMarketUpdate{ 0, {OMEMarketUpdate::Type::SNAPSHOT_START, 2, 3,
                                               Side::INVALID, 0, 0, 0}};
    mdc->queue_update(true, &snapshot_start);
    EXPECT_EQ(mdc->queued_snapshot_updates.size(), 1);
    // order update
    mdc->queue_update(true, &update);
    EXPECT_EQ(mdc->queued_snapshot_updates.size(), 2);
    // SNAPSHOT_END
    auto snapshot_end = MDPMarketUpdate{ 2, {OMEMarketUpdate::Type::SNAPSHOT_END, 2, 3,
                                               Side::INVALID, 0, 0, 0}};
    mdc->queue_update(true, &snapshot_end);
    // should no longer be in recovery and the queue empty after successful recovery flushed it
    EXPECT_FALSE(mdc->is_in_recovery);
    EXPECT_EQ(mdc->queued_snapshot_updates.size(), 0);
    // the update can now be found on the outgoing queue
    auto res = updates.get_next_to_read();
    EXPECT_EQ(res->order_id, ome_update.order_id);
    EXPECT_EQ(res->ticker_id, ome_update.ticker_id);
    EXPECT_EQ(res->side, ome_update.side);
    EXPECT_EQ(res->price, ome_update.price);
    EXPECT_EQ(res->qty, ome_update.qty);
    EXPECT_EQ(res->priority, ome_update.priority);
    EXPECT_EQ(res->type, ome_update.type);
}


/*
 * Tests for MarketDataConsumer which integrate with other modules
 */
class MarketDataConsumerIntegration : public ::testing::Test {
protected:
    std::string IFACE{ "lo" };
    std::string IP_SNAPSHOT{ "239.0.0.2" }; // snapshots multicast group IP
    std::string IP_INCREMENTAL{ "239.0.0.1" };  // multicast group IP for incremental updates
    int PORT_SNAPSHOT{ 12345 };
    int PORT_INCREMENTAL{ 23456 };
    // OME test members
    std::unique_ptr<OrderMatchingEngine> ome;
    MarketUpdateQueue updates_to_publisher{ Limits::MAX_MARKET_UPDATES }; // OME->MDP
    ClientRequestQueue client_request_queue{ Limits::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ Limits::MAX_CLIENT_UPDATES };
    // MDP test members
    std::unique_ptr<MarketDataConsumer> mdc;
    // MDC test members
    MarketUpdateQueue updates_to_client{ Limits::MAX_MARKET_UPDATES }; // MDC->client
    std::unique_ptr<MarketDataPublisher> mdp;
    // test data
    std::vector<OMEMarketUpdate> orders;

    void SetUp() override {
        // initialise all modules
        ome = std::make_unique<OrderMatchingEngine>(&client_request_queue,
                                                    &client_response_queue,
                                                    &updates_to_publisher);
        mdp = std::make_unique<MarketDataPublisher>(updates_to_publisher, IFACE, IP_SNAPSHOT,
                                                    PORT_SNAPSHOT, IP_INCREMENTAL,
                                                    PORT_INCREMENTAL);
        mdc = std::make_unique<MarketDataConsumer>(1, updates_to_client,
                                                   IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                   IP_INCREMENTAL, PORT_INCREMENTAL);
        // generate some trade updates to use throughout testing
        for (uint i{}; i < 10; ++i) {
            auto order = OMEMarketUpdate{
                OMEMarketUpdate::Type::TRADE, i, 1,
                Side::BUY, 10 + i, 100 + i, i
            };
            orders.push_back(order);
        }

    }
    void TearDown() override {

    }
};

TEST_F(MarketDataConsumerIntegration, enters_recovery_from_lost_incremental_packets) {
    /*
     * an order update is received whose n_seq does not match the expected
     * one at the consumer. this should trigger entering recovery mode due
     * to lost packets.
     */
    mdc->is_in_recovery = false;
    // start consumer and verify socket connected
    mdc->start();
    EXPECT_NE(mdc->socket_incremental.fd, -1);
    // send first update and expected n_seq from publisher to consumer
    size_t one{ 1 };
    size_t three{ 3 };
    mdp->socket_incremental.load_tx(&one, sizeof(size_t));
    mdp->socket_incremental.load_tx(&orders.at(0), sizeof(OMEMarketUpdate));
    mdp->socket_incremental.tx_and_rx();
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(mdc->is_in_recovery);
    // the next update has an unexpected, out of order n_seq (3 instead of expected 2)
    mdp->socket_incremental.load_tx(&three, sizeof(size_t));
    mdp->socket_incremental.load_tx(&orders.at(1), sizeof(OMEMarketUpdate));
    mdp->socket_incremental.tx_and_rx();
    std::this_thread::sleep_for(10ms);
    // consumer enters recovery now
    EXPECT_TRUE(mdc->is_in_recovery);
}

TEST_F(MarketDataConsumerIntegration, receives_incremental_updates) {
    /*
     * the data consumer connects to a stream from a publisher
     * and receives a series of market order updates which have been published
     * by the order matching engine
     */
    // start consumer, matching engine and publisher and verify socket connected
    mdc->start();
    ome->start();
    mdp->start();
    EXPECT_NE(mdc->socket_incremental.fd, -1);
    EXPECT_EQ(mdc->tx_updates.size(), 0);
    // publish a series of incremental updates
    for (auto& o: orders) {
        ome->send_market_update(&o);
    }
    std::this_thread::sleep_for(50ms);
    // each update in the sequence should have been received by
    // the consumer and pushed out its queue to the client
    size_t n{ 0 };
    while (mdc->tx_updates.get_next_to_read() != nullptr) {
        auto o = mdc->tx_updates.get_next_to_read();
        auto expected = orders.at(n);
        EXPECT_EQ(*o, expected);
        mdc->tx_updates.increment_read_index();
        ++n;
    }
    EXPECT_FALSE(mdc->is_in_recovery);
}

TEST_F(MarketDataConsumerIntegration, receives_and_recovers_via_combined_data) {
    /*
     * recovers via a combination of snapshot and incremental market data,
     * forming a complete picture of the market state in the consumer
     */
    mdc->start();
    ome->start();
    mdp->start();
    EXPECT_NE(mdc->socket_incremental.fd, -1);
    EXPECT_EQ(mdc->tx_updates.size(), 0);
    // force recovery mode
    mdc->is_in_recovery = true;
    mdc->snapshot_sync_start();
    // publish a sequence of updates
    for (auto& o: orders) {
        ome->send_market_update(&o);
    }
    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(mdc->is_in_recovery);
    for (auto& o: orders) {
        ome->send_market_update(&o);
    }
    // wait long enough for a snapshot to be published and received
    std::this_thread::sleep_for(2s);
    // recovery should have completed
    EXPECT_FALSE(mdc->is_in_recovery);
}
