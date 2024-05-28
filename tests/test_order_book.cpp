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
    TickerID ticker{ 3 };
    std::unique_ptr<OMEOrderBook> ob{ nullptr };

    void SetUp() override {
        ob = std::make_unique<OMEOrderBook>(ticker, logger, ome);
    }

    void TearDown() override {
    }
};


TEST_F(OrderBookBasics, is_constructed) {
    EXPECT_NE(nullptr, ob);
}

TEST_F(OrderBookBasics, adds_price_levels) {
    // three price levels are added to the book and it is verified
    // that they are ordered correctly

    // SELL side (asks)
    OMEOrdersAtPrice A{ Exchange::Side::SELL, 100, nullptr, nullptr, nullptr };
    OMEOrdersAtPrice B{ Exchange::Side::SELL, 125, nullptr, nullptr, nullptr };
    OMEOrdersAtPrice C{ Exchange::Side::SELL, 50, nullptr, nullptr, nullptr };
    ob->add_price_level_test(&A);
    ob->add_price_level_test(&B);
    ob->add_price_level_test(&C);
    auto asks = ob->get_ask_levels_by_price();
    EXPECT_EQ(*asks, C);    // the most aggressive ask level is the lowest price
    EXPECT_EQ(*(asks->next), A);  // second most aggressive ask level
    EXPECT_EQ(*(asks->next->next), B);  // least aggressive

    // BUY side (bids)
    A.side = Side::BUY;
    B.side = Side::BUY;
    C.side = Side::BUY;
    ob->add_price_level_test(&A);   // 100
    ob->add_price_level_test(&C);   // 50
    ob->add_price_level_test(&B);   // 125
    auto bids = ob->get_bid_levels_by_price();
    EXPECT_EQ(*bids, B);    // the most aggressive bid level is the highest price
    EXPECT_EQ(*(bids->next), A);  // second most aggressive
    EXPECT_EQ(*(bids->next->next), C);  // least aggressive
}

TEST_F(OrderBookBasics, removes_price_levels) {
    // price levels are removed from the book and the ordering
    // is maintained

    // SELL side (asks)
    // we allocate the levels in the book's mempool (normally this is
    // done by the add_order_to_book method as needed)
    auto& pool = ob->get_price_levels_mempool();
    auto a = pool.allocate(Exchange::Side::SELL, 25, nullptr, nullptr, nullptr);
    auto b = pool.allocate(Exchange::Side::SELL, 75, nullptr, nullptr, nullptr);
    auto c = pool.allocate(Exchange::Side::SELL, 125, nullptr, nullptr, nullptr);
    auto d = pool.allocate(Exchange::Side::SELL, 175, nullptr, nullptr, nullptr);
    ob->add_price_level_test(a);
    ob->add_price_level_test(b);
    ob->add_price_level_test(c);
    ob->add_price_level_test(d);
    ob->remove_price_level_test(Side::SELL, 75);
    ob->remove_price_level_test(Side::SELL, 175);
    auto asks = ob->get_ask_levels_by_price();
    EXPECT_EQ(*asks, *a);
    EXPECT_EQ(*asks->next, *c);

    // BUY side (bids)
    auto e = pool.allocate(Exchange::Side::BUY, 200, nullptr, nullptr, nullptr);
    auto f = pool.allocate(Exchange::Side::BUY, 150, nullptr, nullptr, nullptr);
    auto g = pool.allocate(Exchange::Side::BUY, 100, nullptr, nullptr, nullptr);
    auto h = pool.allocate(Exchange::Side::BUY, 50, nullptr, nullptr, nullptr);
    ob->add_price_level_test(e);
    ob->add_price_level_test(f);
    ob->add_price_level_test(g);
    ob->add_price_level_test(h);
    ob->remove_price_level_test(Side::BUY, 200);
    ob->remove_price_level_test(Side::BUY, 100);
    auto bids = ob->get_bid_levels_by_price();
    EXPECT_EQ(*bids, *f);
    EXPECT_EQ(*bids->next, *h);
}

TEST_F(OrderBookBasics, adds_order_to_book) {
    // the add_order_to_book_method --
    ClientID c{ 12 };
    OrderID c_oid{ 1 };
    Side side{ Side::BUY };
    Price price{ 100 };
    Qty qty{ 50 };
    OMEOrder order1{
            ticker, c, c_oid, 1ul,
            side, price, qty, 1, nullptr, nullptr };
    ob->add_order_to_book_test(&order1);
    // 1. creates a new price level on an otherwise empty order book
    auto first = ob->get_level_for_price_test(price)->order_0;
    EXPECT_EQ(first->client_id, c);
    EXPECT_EQ(first->client_order_id, c_oid);
    EXPECT_EQ(first->ticker_id, ticker);
    EXPECT_EQ(first->side, side);
    EXPECT_EQ(first->price, price);
    EXPECT_EQ(first->qty, qty);
    // 2. inserts a second order to the same price level when one already exists
    OMEOrder order2{
            ticker, 2, 2, 2ul,
            side, price, 100, 2, nullptr, nullptr };
    // the additional order should be in the last position, respecting
    // the FIFO order queue
    auto last = ob->get_level_for_price_test(price)->order_0->next;
    EXPECT_EQ(order1.client_order_id, last->client_order_id);
    EXPECT_EQ(order1.client_id, last->client_id);
    EXPECT_EQ(order1.qty, last->qty);
    EXPECT_EQ(order1.market_order_id, last->market_order_id);
}

TEST_F(OrderBookBasics, removes_order_from_book) {
    // the remove_order_from_book method correctly removes
    // a specific order from an order book (which has multiple
    // orders present)
    auto& pool = ob->get_orders_mempool();
    auto A = pool.allocate(
            ticker, 1, 1, 1ul,
            Side::BUY, 100, 100, 1,
            nullptr, nullptr);
    ob->add_order_to_book_test(A);
    auto B = pool.allocate(
            ticker, 2, 1, 2ul,
            Side::BUY, 100, 50, 2,
            nullptr, nullptr);
    ob->add_order_to_book_test(B);
    auto C = pool.allocate(
            ticker, 2, 1, 2ul,
            Side::BUY, 100, 50, 3,
            nullptr, nullptr);
    ob->add_order_to_book_test(C);
    // remove the center order (
    ob->remove_order_from_book_test(B);
    // verify that B remains in the price level at the first position
    auto orders = ob->get_level_for_price_test(100);
    EXPECT_EQ(*A, *orders->order_0);
    EXPECT_EQ(*C, *orders->order_0->next);
}

TEST_F(OrderBookBasics, adding_passive_order) {
    // adding a new passive order to the limit book --
    ClientID c{ 12 };
    OrderID c_oid{ 1 };
    Side side{ Side::BUY };
    Price price{ 100 };
    Qty qty{ 50 };
    ob->add(c, c_oid, ticker, side, price, qty);
    // 1. generates a client response
    auto res = ob->get_client_response();
    EXPECT_EQ(res->type, OMEClientResponse::Type::ACCEPTED);
    EXPECT_EQ(res->client_id, c);
    EXPECT_EQ(res->client_order_id, c_oid);
    EXPECT_EQ(res->ticker_id, ticker);
    EXPECT_EQ(res->side, side);
    EXPECT_EQ(res->price, price);
    EXPECT_EQ(res->qty_exec, 0);
    EXPECT_EQ(res->qty_remain, qty);
    // 2. adds to the order book at the correct price level
    auto order0 = ob->get_level_for_price_test(price)->order_0;
    EXPECT_EQ(order0->client_id, c);
    EXPECT_EQ(order0->client_order_id, c_oid);
    EXPECT_EQ(order0->ticker_id, ticker);
    EXPECT_EQ(order0->side, side);
    EXPECT_EQ(order0->price, price);
    EXPECT_EQ(order0->qty, qty);
    // 3. prepares a market update for participants
    auto update = ob->get_market_update();
    EXPECT_EQ(update->type, OMEMarketUpdate::Type::ADD);
    EXPECT_EQ(update->qty, qty);
    EXPECT_EQ(update->ticker_id, ticker);
    EXPECT_EQ(update->side, side);
    EXPECT_EQ(update->price, price);
    EXPECT_EQ(update->priority, 1);
    EXPECT_EQ(update->order_id, 1);
}



