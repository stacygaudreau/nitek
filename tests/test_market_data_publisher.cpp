#include "gtest/gtest.h"
#include "exchange/data/market_data_publisher.h"
#include "exchange/data/snapshot_synthesizer.h"
#include "exchange/orders/order_matching_engine.h"
#include "llbase/tcp_socket.h"
#include <string>


using namespace Exchange;


/*
 * Base tests for SnapshotSynthesizer
 */
class SnapshotSynthesizerBasics : public ::testing::Test {
protected:
    std::string IFACE{ "lo" };
    std::string IP{ "127.0.0.1" };     // IP to run tests on
    int PORT{ 12345 };     // port to run tests on
    MDPMarketUpdateQueue updates{ Limits::MAX_MARKET_UPDATES };

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
    MarketUpdateQueue updates{ Limits::MAX_MARKET_UPDATES };

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
    MarketUpdateQueue market_updates{ Limits::MAX_MARKET_UPDATES };
    // MDP test members
    std::unique_ptr<MarketDataPublisher> mdp;
    std::string IFACE{ "lo" };
    std::string IP_SNAPSHOT{ "239.0.0.2" }; // snapshots multicast group IP
    std::string IP_INCREMENTAL{ "239.0.0.1" };  // multicast group IP for incremental updates
    int PORT_SNAPSHOT{ 12345 };
    int PORT_INCREMENTAL{ 23456 };
    // OME test members
    std::unique_ptr<OrderMatchingEngine> ome;
    ClientRequestQueue client_request_queue{ Limits::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ Limits::MAX_CLIENT_UPDATES };
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
    // socket is listening over UDP for updates
    const std::string ip_rx{ "239.0.1.3" };
    auto fd = socket_rx->init(ip_rx, IFACE, PORT_INCREMENTAL, true);
    EXPECT_GT(fd, -1);
    EXPECT_TRUE(socket_rx->join_group(IP_INCREMENTAL));
    // publisher is started
    using namespace std::literals::chrono_literals;
    mdp->start();
    std::this_thread::sleep_for(10ms);
    // market update is dispatched into the queue by the OME
    OMEMarketUpdate update{ OMEMarketUpdate::Type::TRADE, 1, 1, Side::SELL, 95, 20, 23 };
    ome->send_market_update(&update);
    // verify there is data in the queue
    EXPECT_NE(nullptr, market_updates.get_next_to_read());
    // callback to validate data received at socket
    bool some_data_was_received{ false };
    socket_rx->rx_callback = [&](LL::McastSocket* socket) {
        (void) socket;
        some_data_was_received = true;
    };
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
    EXPECT_EQ(rx_update.order_id, update.order_id);
    EXPECT_EQ(rx_update.side, update.side);
    EXPECT_EQ(rx_update.type, update.type);
}

TEST_F(MarketDataPublisherUpdates, publishes_empty_snapshot) {
    // a snapshot is published and received with the correct
    // format for an empty snapshot
    EXPECT_NE(mdp, nullptr);
    EXPECT_NE(ome, nullptr);
    // socket is listening over UDP for updates
    const std::string ip_rx{ "239.0.1.3" };
    auto fd = socket_rx->init(ip_rx, IFACE, PORT_SNAPSHOT, true);
    EXPECT_GT(fd, -1);
    EXPECT_TRUE(socket_rx->join_group(IP_SNAPSHOT));
    // publisher is started and should automatically publish a snapshot
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
    // at this point some data must have been received
    EXPECT_TRUE(some_data_was_received);
    // the first update message should be a START_SNAPSHOT and its sequence number zero
    size_t i_rx{ };  // read index of rx buffer
    size_t n_seq{ };
    std::memcpy(&n_seq, socket_rx->rx_buffer.data(), sizeof(size_t));
    EXPECT_EQ(n_seq, 0);
    OMEMarketUpdate rx_update{ };
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t),
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::SNAPSHOT_START);
    // the next message will be a CLEAR on ticker 0 with n_seq = 1
    i_rx = sizeof(size_t) + sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 1);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 0);
    // and a CLEAR on ticker 1 with n_seq = 2
    i_rx = 2 * sizeof(size_t) + 2 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 2);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 1);
    // CLEAR ticker 2, n_seq = 3
    i_rx = 3 * sizeof(size_t) + 3 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 3);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 2);
    // CLEAR ticker 3, n_seq = 4
    i_rx = 4 * sizeof(size_t) + 4 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 4);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 3);
    // CLEAR ticker 4, n_seq = 5
    i_rx = 5 * sizeof(size_t) + 5 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 5);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 4);
    // CLEAR ticker 5, n_seq = 6
    i_rx = 6 * sizeof(size_t) + 6 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 6);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 5);
    // CLEAR ticker 6, n_seq = 7
    i_rx = 7 * sizeof(size_t) + 7 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 7);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 6);
    // CLEAR ticker 7, n_seq = 8
    i_rx = 8 * sizeof(size_t) + 8 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 8);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 7);
    // a SNAPSHOT_END message to finalize the snapshot
    i_rx = 9 * sizeof(size_t) + 9 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 9);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::SNAPSHOT_END);
}

TEST_F(MarketDataPublisherUpdates, publishes_snapshot_for_single_ticker) {
    // a snapshot is synthesized from a stream of order updates
    // for a single financial instrument and it is received in the expected
    // format
    EXPECT_NE(mdp, nullptr);
    EXPECT_NE(ome, nullptr);
    // market update is added to the queue by the OME
    OMEMarketUpdate update{ OMEMarketUpdate::Type::ADD, 1, 1, Side::SELL, 95, 20, 23 };
    ome->send_market_update(&update);
    // verify there is data in the queue
    EXPECT_NE(nullptr, market_updates.get_next_to_read());
    // socket is listening over UDP for updates
    const std::string ip_rx{ "239.0.1.3" };
    auto fd = socket_rx->init(ip_rx, IFACE, PORT_SNAPSHOT, true);
    EXPECT_GT(fd, -1);
    EXPECT_TRUE(socket_rx->join_group(IP_SNAPSHOT));
    // publisher is started and should automatically publish a snapshot
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
    // at this point some data must have been received
    EXPECT_TRUE(some_data_was_received);
    // the first update message should be a START_SNAPSHOT and its sequence number zero
    size_t i_rx{ };  // read index of rx buffer
    size_t n_seq{ };
    std::memcpy(&n_seq, socket_rx->rx_buffer.data(), sizeof(size_t));
    EXPECT_EQ(n_seq, 0);
    OMEMarketUpdate rx_update{ };
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t),
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::SNAPSHOT_START);
    // the next message will be a CLEAR on ticker 0 with n_seq = 1
    i_rx = sizeof(size_t) + sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 1);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 0);
    // and a CLEAR on ticker 1 with n_seq = 2
    i_rx = 2 * sizeof(size_t) + 2 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 2);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::CLEAR);
    EXPECT_EQ(rx_update.ticker_id, 1);
    // the trade update added was on ticker 1, so now we should see that reflected here
    i_rx = 3 * sizeof(size_t) + 3 * sizeof(OMEMarketUpdate);
    std::memcpy(&n_seq, socket_rx->rx_buffer.data() + i_rx, sizeof(size_t));
    EXPECT_EQ(n_seq, 3);
    std::memcpy(&rx_update, socket_rx->rx_buffer.data() + sizeof(size_t) + i_rx,
                sizeof(OMEMarketUpdate));
    EXPECT_EQ(rx_update.type, OMEMarketUpdate::Type::ADD);
    EXPECT_EQ(rx_update.side, update.side);
    EXPECT_EQ(rx_update.ticker_id, update.ticker_id);
    EXPECT_EQ(rx_update.price, update.price);
    EXPECT_EQ(rx_update.qty, update.qty);
    EXPECT_EQ(rx_update.priority, update.priority);
}
