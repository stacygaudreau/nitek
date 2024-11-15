#include "gtest/gtest.h"
#include "exchange/exchange_server.h"
#include "llbase/logging.h"

#include <string>
#include <memory>


using namespace LL;
using namespace Exchange;


/*
 * Basic tests for the Exchange Server module
 */
class ExchangeServerBasics : public ::testing::Test {
protected:
    std::unique_ptr<ExchangeServer> exchange{ nullptr };
    const std::string order_iface{ "lo" };
    int order_port{ 9000 };
    const std::string data_iface{ "lo" };
    const std::string data_incremental_ip{ "239.0.0.1" };
    int data_incremental_port{ 9001 };
    const std::string data_snapshot_ip{ "239.0.0.2" };
    int data_snapshot_port{ 9002 };

    void SetUp() override {
    }

    void TearDown() override {
    }
};


TEST_F(ExchangeServerBasics, is_constructed) {
    exchange = std::make_unique<ExchangeServer>(order_iface, order_port, data_iface,
                                                data_incremental_ip, data_incremental_port,
                                                data_snapshot_ip, data_snapshot_port);
    EXPECT_NE(exchange, nullptr);
}

TEST_F(ExchangeServerBasics, starts_all_modules) {
    // all submodules of the exchange are started
    exchange = std::make_unique<ExchangeServer>(order_iface, order_port, data_iface,
                                                data_incremental_ip, data_incremental_port,
                                                data_snapshot_ip, data_snapshot_port);
    using namespace std::literals::chrono_literals;
    exchange->start();
    EXPECT_TRUE(exchange->get_is_OME_running());
    EXPECT_TRUE(exchange->get_is_OGS_running());
    EXPECT_TRUE(exchange->get_is_MDP_running());
    std::this_thread::sleep_for(100ms);
}

TEST_F(ExchangeServerBasics, runs_and_terminates) {
    // the main exchange thread runs and is terminated when the stop signal is received
    exchange = std::make_unique<ExchangeServer>(order_iface, order_port, data_iface,
                                                data_incremental_ip, data_incremental_port,
                                                data_snapshot_ip, data_snapshot_port);
    exchange->start();
    EXPECT_TRUE(exchange->get_is_running());
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(100ms);
    exchange->stop();
    EXPECT_FALSE(exchange->get_is_running());
}


/*
 * Integration tests which verify the inner workings of
 * the Exchange Server module
 */
class ExchangeServerIntegration : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};




