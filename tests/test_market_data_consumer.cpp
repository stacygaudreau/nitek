#include "gtest/gtest.h"
#include "exchange/data/market_data_publisher.h"
#include "exchange/data/snapshot_synthesizer.h"
#include "exchange/orders/order_matching_engine.h"
#include "client/data/market_data_consumer.h"
#include "llbase/tcp_socket.h"
#include <string>


using namespace Exchange;
using namespace Client;
using namespace LL;


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
    MarketUpdateQueue updates{ OME::MAX_MARKET_UPDATES };
    std::unique_ptr<MarketDataConsumer> mdc;

    void SetUp() override {
        mdc = std::make_unique<MarketDataConsumer>(2, updates,
                                                       IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                       IP_INCREMENTAL, PORT_INCREMENTAL);
    }
    void TearDown() override {
    }
};


TEST_F(MarketDataConsumerBasics, is_constructed) {
    EXPECT_NE(mdc, nullptr);
}

TEST_F(MarketDataConsumerBasics, starts_and_stops_worker_thread) {
    using namespace std::literals::chrono_literals;
    mdc->start();
    EXPECT_TRUE(mdc->is_running);
    std::this_thread::sleep_for(100ms);
    mdc->stop();
    EXPECT_FALSE(mdc->is_running);
}

TEST_F(MarketDataConsumerBasics, snapshot_sync_is_started) {
    // a snapshot sync is started and the consumer connects to the stream
    auto mdc = std::make_unique<MarketDataConsumer>(2, updates,
                                                    IFACE, IP_SNAPSHOT, PORT_SNAPSHOT,
                                                    IP_INCREMENTAL, PORT_INCREMENTAL);
    using namespace std::literals::chrono_literals;
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
    // an incremental update is queued
}

TEST_F(MarketDataConsumerBasics, snapshot_update_is_queued) {
    // a snapshot update is queued
}

TEST_F(MarketDataConsumerBasics, starts_in_recovery_mode) {
    // the consumer should begin in recovery mode when running
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
    MarketUpdateQueue updates_to_client{ OME::MAX_MARKET_UPDATES };

    void SetUp() override {
    }
    void TearDown() override {
    }
};

TEST_F(MarketDataConsumerIntegration, receives_incremental_update) {
    /*
     * the data consumer connects to a stream from a publisher
     * and receives a single market order update which has been published
     */
}

TEST_F(MarketDataConsumerIntegration, receives_snapshot_and_resyncs) {
    /*
     * recovery mode is entered and a market snapshot is fully
     * received and captured, which re-syncs the consumer
     */
}

TEST_F(MarketDataConsumerIntegration, enters_recovery_from_lost_incremental_packets) {
    /*
     * recovery mode is entered after incremental packets are lost
     */
}