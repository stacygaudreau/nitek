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