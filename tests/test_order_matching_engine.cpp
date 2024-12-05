#include "gtest/gtest.h"
#include "exchange/orders/order_matching_engine.h"


using namespace Exchange;


// base tests for Order Matching Engine module
class OrderMatchingEngineBasics : public ::testing::Test {
protected:
    ClientRequestQueue client_request_queue{ Limits::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ Limits::MAX_CLIENT_UPDATES };
    MarketUpdateQueue market_update_queue{ Limits::MAX_MARKET_UPDATES };

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

TEST_F(OrderMatchingEngineBasics, starts_and_stops_worker_thread) {
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
    ASSERT_TRUE(ome->get_is_running());
    ome->stop();
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(ome->get_is_running());
}


// tests for receiving and sending messages to/from the OME's queues
class OrderMatchingEngineMessages : public ::testing::Test {
protected:
    ClientRequestQueue client_request_queue{ Limits::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ Limits::MAX_CLIENT_UPDATES };
    MarketUpdateQueue market_update_queue{ Limits::MAX_MARKET_UPDATES };
    OrderMatchingEngine ome{ &client_request_queue,
                             &client_response_queue,
                             &market_update_queue };

    void SetUp() override {
    }

    void TearDown() override {
    }
};


TEST_F(OrderMatchingEngineMessages, receives_client_request) {
    // OME receives client request messages from the attached queue
    // dispatch mock client requests to the OME
    OMEClientRequest req1{ OMEClientRequest::Type::NEW, 1, 1, 1, Side::BUY, 100, 100 };
    OMEClientRequest req2{ OMEClientRequest::Type::NEW, 2, 1, 2, Side::BUY, 110, 100 };
    OMEClientRequest req3{ OMEClientRequest::Type::NEW, 3, 1, 3, Side::BUY, 100, 50 };
    // load the queue
    auto next = client_request_queue.get_next_to_write();
    *next = req1;
    client_request_queue.increment_write_index();
    next = client_request_queue.get_next_to_write();
    *next = req2;
    client_request_queue.increment_write_index();
    next = client_request_queue.get_next_to_write();
    *next = req3;
    client_request_queue.increment_write_index();
    ASSERT_EQ(client_request_queue.size(), 3);
    // start the matching engine
    ome.start();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);
    ASSERT_TRUE(ome.get_is_running());
    std::this_thread::sleep_for(10ms);
    // the 3 requests should have been consumed
    EXPECT_EQ(client_request_queue.size(), 0);
}

TEST_F(OrderMatchingEngineMessages, sends_client_response) {
    // client response is dispatched into the queue by the OME
    OMEClientResponse res1{ OMEClientResponse::Type::ACCEPTED, 1, 1, 1, 1, Side::BUY, 100, 50, 50 };
    // send response
    ome.send_client_response(&res1);
    // read the queue & verify message
    auto res = client_response_queue.get_next_to_read();
    EXPECT_EQ(res->client_id, res1.client_id);
    EXPECT_EQ(res->type, res1.type);
    EXPECT_EQ(res->market_order_id, res1.market_order_id);
}

TEST_F(OrderMatchingEngineMessages, sends_market_update) {
    // market update is dispatched into the queue by the OME
    OMEMarketUpdate update{ OMEMarketUpdate::Type::TRADE, 1, 1, Side::SELL, 95, 20, 23 };
    // dispatch update
    ome.send_market_update(&update);
    // read the queue & verify message
    auto rx = market_update_queue.get_next_to_read();
    EXPECT_EQ(rx->order_id, update.order_id);
    EXPECT_EQ(rx->side, update.side);
    EXPECT_EQ(rx->type, update.type);
}

