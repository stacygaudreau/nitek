#include "gtest/gtest.h"
#include "exchange/orders/order_matching_engine.h"


using namespace Exchange;


// base tests for Order Matching Engine module
class OrderMatchingEngineBasics : public ::testing::Test {
protected:
    size_t N_BLOCKS{ 1024 };
    std::string logfile{ "order_matching_engine_tests.log" };
    ClientRequestQueue client_request_queue{ N_BLOCKS };
    ClientResponseQueue client_response_queue{ N_BLOCKS };
    MarketUpdateQueue market_update_queue{ N_BLOCKS };

    void SetUp() override {
    }

    void TearDown() override {
    }
};


TEST_F(OrderMatchingEngineBasics, is_constructed) {
    // matching engine constructed and has base properties
    auto ome = std::make_unique<OrderMatchingEngine>(
            &client_request_queue,
            &client_response_queue,
            &market_update_queue
    );
    EXPECT_NE(ome, nullptr);
}

TEST_F(OrderMatchingEngineBasics, starts_worker_thread) {
    // running the matching engine with start()
    auto ome = std::make_unique<OrderMatchingEngine>(
            &client_request_queue,
            &client_response_queue,
            &market_update_queue
    );
    ASSERT_NE(ome, nullptr);
    ome->start();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(ome->get_is_running());
}