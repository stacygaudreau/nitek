#include "gtest/gtest.h"
#include "exchange/data/market_data_publisher.h"
#include "exchange/data/snapshot_synthesizer.h"
#include "exchange/orders/order_matching_engine.h"
#include "llbase/tcp_socket.h"
#include <string>
#include <iostream>


using namespace Exchange;


/*
 * Base tests for SnapshotSynthesizer
 */
class SnapshotSynthesizerBasics : public ::testing::Test {
protected:
    std::string IFACE{ "lo" };
    std::string IP{ "127.0.0.1" };     // IP to run tests on
    int PORT{ 12345 };     // port to run tests on
    MDPMarketUpdateQueue updates{ OME::MAX_MARKET_UPDATES };

    void SetUp() override {
    }
    void TearDown() override {
    }
};


TEST_F(SnapshotSynthesizerBasics, is_constructed) {
    // snapshot synth. is constructed and has basic properties set
    auto ss = std::make_unique<SnapshotSynthesizer>(updates, IFACE, IP, PORT);
    EXPECT_NE(ss, nullptr);
}

TEST_F(SnapshotSynthesizerBasics, starts_and_stops_worker_thread) {
    // the SS manages its worker thread's lifecycle
    auto ss = std::make_unique<SnapshotSynthesizer>(updates, IFACE, IP, PORT);
    EXPECT_FALSE(ss->get_is_running());
    ss->start();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(ss->get_is_running());
    ss->stop();
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(ss->get_is_running());
}


/*
 * Base tests for MarketDataPublisher
 */
class MarketDataPublisherBasics : public ::testing::Test {
protected:
    std::string IFACE{ "lo" };
    std::string IP_SNAPSHOT{ "127.0.0.1" };
    std::string IP_INCREMENTAL{ "127.0.0.1" };
    int PORT_SNAPSHOT{ 12345 };
    int PORT_INCREMENTAL{ 23456 };
    MarketUpdateQueue updates{ OME::MAX_MARKET_UPDATES };

    void SetUp() override {
    }
    void TearDown() override {
    }
};


TEST_F(MarketDataPublisherBasics, is_constructed) {
    // data publisher is constructed and has basic properties set
    auto mdp = std::make_unique<MarketDataPublisher>(updates, IFACE,
                                                     IP_SNAPSHOT, PORT_SNAPSHOT,
                                                     IP_INCREMENTAL, PORT_INCREMENTAL);
    EXPECT_NE(mdp, nullptr);
}

TEST_F(MarketDataPublisherBasics, starts_and_stops_worker_thread) {
    // the MDP manages its worker thread's lifecycle
    auto mdp = std::make_unique<MarketDataPublisher>(updates, IFACE,
                                                     IP_SNAPSHOT, PORT_SNAPSHOT,
                                                     IP_INCREMENTAL, PORT_INCREMENTAL);
    EXPECT_FALSE(mdp->get_is_running());
    mdp->start();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(mdp->get_is_running());
    mdp->stop();
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(mdp->get_is_running());
}


/*
 * Test publishing market data updates
 */
class MarketDataPublisherUpdates : public ::testing::Test {
protected:
    // OME --> MDP updates queue
    MarketUpdateQueue market_updates{ OME::MAX_MARKET_UPDATES };
    // MDP test members
    std::unique_ptr<MarketDataPublisher> mdp;
    std::string IFACE{ "lo" };
    std::string IP_SNAPSHOT{ "239.0.0.2" }; // snapshots multicast group IP
    std::string IP_INCREMENTAL{ "239.0.0.1" };  // multicast group IP for incremental updates
    int PORT_SNAPSHOT{ 12345 };
    int PORT_INCREMENTAL{ 23456 };
    // OME test members
    std::unique_ptr<OrderMatchingEngine> ome;
    ClientRequestQueue client_request_queue{ OME::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ OME::MAX_CLIENT_UPDATES };
    // sockets for testing raw published data
    LL::Logger logger{ "mdp_test_socket.log" };
    std::unique_ptr<LL::McastSocket> socket_rx;

    void SetUp() override {
        mdp = std::make_unique<MarketDataPublisher>(market_updates, IFACE,
                                                    IP_SNAPSHOT, PORT_SNAPSHOT,
                                                    IP_INCREMENTAL, PORT_INCREMENTAL);
        ome = std::make_unique<OrderMatchingEngine>(&client_request_queue,
                                                    &client_response_queue,
                                                    &market_updates);
        socket_rx = std::make_unique<LL::McastSocket>(logger);
        ome->start();
    }
    void TearDown() override {
    }
};


TEST_F(MarketDataPublisherUpdates, publishes_incremental_update) {
    // an incremental update is sent down the wire over UDP multicast
    // and received on a UDP socket
    EXPECT_NE(mdp, nullptr);
    EXPECT_NE(ome, nullptr);
    // market update is dispatched into the queue by the OME
    OMEMarketUpdate update{ OMEMarketUpdate::Type::TRADE, 1, 1, Side::SELL, 95, 20, 23 };
    ome->send_market_update(&update);
    // verify there is data in the queue
    EXPECT_NE(nullptr, market_updates.get_next_to_read());
    // socket is listening over UDP for updates
    const std::string ip_rx{ "239.0.1.3" };
    auto fd = socket_rx->init(ip_rx, IFACE, PORT_INCREMENTAL, true);
    EXPECT_GT(fd, -1);
    EXPECT_TRUE(socket_rx->join_group(IP_INCREMENTAL));
    // publisher is started and should automatically publish the pending update over the wire
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);
    mdp->start();
    // callback to validate data received at socket
    bool some_data_was_received{ false };
    socket_rx->rx_callback = [&](LL::McastSocket* socket) {
        (void) socket;
        some_data_was_received = true;
    };
    // receive on the socket
    std::this_thread::sleep_for(10ms);
    socket_rx->tx_and_rx();
    // validate market data received
    EXPECT_TRUE(some_data_was_received);
    size_t n_seq{ };
    std::memcpy(&n_seq, socket_rx->rx_buffer.data(), sizeof(size_t));
    // the data sequence number should be 1
    EXPECT_EQ(n_seq, 1);
    // receive the remaining data as a market update and verify it
    OMEMarketUpdate rx_update{ };
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t),
                sizeof(OMEMarketUpdate));
    std::cout << rx_update.to_str();
    EXPECT_EQ(rx_update.order_id, update.order_id);
    EXPECT_EQ(rx_update.side, update.side);
    EXPECT_EQ(rx_update.type, update.type);
}

TEST_F(MarketDataPublisherUpdates, add_to_snapshot_adds) {
    // a new order update not yet in the snapshot is added

}

TEST_F(MarketDataPublisherUpdates, add_to_snapshot_modifies) {
    // an existing order is modified in place in the snapshot

}

TEST_F(MarketDataPublisherUpdates, add_to_snapshot_cancels) {
    // an existing order is cancelled an removed from the snapshot

}

TEST_F(MarketDataPublisherUpdates, publishes_snapshot_for_single_ticker) {
    // a snapshot is synthesized from a stream of order updates
    // for a single financial instrument and it is in the correct
    // format

}

TEST_F(MarketDataPublisherUpdates, publishes_snapshot_for_multiple_tickers) {
    // a snapshot is synthesized for multiple tickers from a stream of order
    // updates and it is in the correct format

}







