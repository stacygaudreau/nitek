#include "gtest/gtest.h"
#include "exchange/orders/order_matching_engine.h"
#include "exchange/orders/ome_order_book.h"
#include <memory>
#include "llbase/logging.h"


using namespace Exchange;


// base tests for order book module
class OrderBookBasics : public ::testing::Test {
protected:
    LL::Logger logger{ "order_book_tests.log" };
    ClientRequestQueue client_request_queue{ OME::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ OME::MAX_CLIENT_UPDATES };
    MarketUpdateQueue market_update_queue{ OME::MAX_MARKET_UPDATES };
    OrderMatchingEngine ome{ &client_request_queue,
                             &client_response_queue,
                             &market_update_queue };
    void SetUp() override {
    }

    void TearDown() override {
    }
};


TEST_F(OrderBookBasics, is_constructed) {
    auto ob =
            std::make_unique<OMEOrderBook>(1, logger, ome);
    EXPECT_NE(nullptr, ob);
}

TEST_F(OrderBookBasics, add_generates_client_response) {
    // adding a new order generates a corresponding client response
    ClientID c{ 12 };
    OrderID c_oid{ 1 };
    TickerID ticker{ 3 };
    Side side{ Side::BUY };
    Price price{ 100 };
    Qty qty{ 50 };
    auto ob =
            std::make_unique<OMEOrderBook>(1, logger, ome);
    ob->add(c, c_oid, ticker, side, price, qty);
    auto res = ob->get_client_response();
    EXPECT_EQ(res->client_id, c);
    EXPECT_EQ(res->client_order_id, c_oid);
    EXPECT_EQ(res->ticker_id, ticker);
    EXPECT_EQ(res->side, side);
    EXPECT_EQ(res->price, price);
    EXPECT_EQ(res->qty_exec, 0);
    EXPECT_EQ(res->qty_remain, qty);
}

