#include "gtest/gtest.h"
#include "exchange/orders/order_matching_engine.h"
#include "exchange/orders/ome_order_book.h"
#include <memory>
#include "llbase/logging.h"


using namespace Exchange;
using namespace Common;


// base tests for Exchange order book module
class ExchangeOrderBookBasics : public ::testing::Test {
protected:
    LL::Logger logger{ "exchange_order_book_tests.log" };
    ClientRequestQueue client_request_queue{ Limits::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ Limits::MAX_CLIENT_UPDATES };
    MarketUpdateQueue market_update_queue{ Limits::MAX_MARKET_UPDATES };
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


TEST_F(ExchangeOrderBookBasics, is_constructed) {
    EXPECT_NE(nullptr, ob);
}

TEST_F(ExchangeOrderBookBasics, adds_price_levels) {
    // three price levels are added to the book and it is verified
    // that they are ordered correctly

    // SELL side (asks)
    OMEOrdersAtPrice A{ Side::SELL, 100, nullptr, nullptr, nullptr };
    OMEOrdersAtPrice B{ Side::SELL, 125, nullptr, nullptr, nullptr };
    OMEOrdersAtPrice C{ Side::SELL, 50, nullptr, nullptr, nullptr };
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

TEST_F(ExchangeOrderBookBasics, removes_price_levels) {
    // price levels are removed from the book and the ordering
    // is maintained

    // SELL side (asks)
    // we allocate the levels in the book's mempool (normally this is
    // done by the add_order_to_book method as needed)
    auto& pool = ob->get_price_levels_mempool();
    auto a = pool.allocate(Side::SELL, 25, nullptr, nullptr, nullptr);
    auto b = pool.allocate(Side::SELL, 75, nullptr, nullptr, nullptr);
    auto c = pool.allocate(Side::SELL, 125, nullptr, nullptr, nullptr);
    auto d = pool.allocate(Side::SELL, 175, nullptr, nullptr, nullptr);
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
    auto e = pool.allocate(Side::BUY, 200, nullptr, nullptr, nullptr);
    auto f = pool.allocate(Side::BUY, 150, nullptr, nullptr, nullptr);
    auto g = pool.allocate(Side::BUY, 100, nullptr, nullptr, nullptr);
    auto h = pool.allocate(Side::BUY, 50, nullptr, nullptr, nullptr);
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

TEST_F(ExchangeOrderBookBasics, adds_order_to_book) {
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

TEST_F(ExchangeOrderBookBasics, removes_order_from_book) {
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

TEST_F(ExchangeOrderBookBasics, adding_passive_order) {
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

TEST_F(ExchangeOrderBookBasics, cancels_passive_order) {
    // cancelling a passive order which was added --
    ClientID c{ 12 };
    OrderID c_oid{ 1 };
    Side side{ Side::BUY };
    Price price{ 100 };
    Qty qty{ 50 };
    ob->add(c, c_oid, ticker, side, price, qty);
    // cancel the same order
    ob->cancel(c, c_oid, ticker);
    // 1. generates a client response confirming cancellation
    auto res = ob->get_client_response();
    EXPECT_EQ(res->type, OMEClientResponse::Type::CANCELLED);
    EXPECT_EQ(res->client_id, c);
    EXPECT_EQ(res->client_order_id, c_oid);
    EXPECT_EQ(res->ticker_id, ticker);
    // 2. removed from the order book at the correct price level
    // -> the price level itself should be gone now
    auto level = ob->get_level_for_price_test(price);
    EXPECT_EQ(level, nullptr);
    // 3. dispatches a market update for participants
    auto update = ob->get_market_update();
    EXPECT_EQ(update->type, OMEMarketUpdate::Type::CANCEL);
    EXPECT_EQ(update->qty, 0);
    EXPECT_EQ(update->ticker_id, ticker);
    EXPECT_EQ(update->side, side);
    EXPECT_EQ(update->price, price);
}


// matching tests for order book module
class ExchangeOrderBookMatching : public ::testing::Test {
protected:
    LL::Logger logger{ "order_book_matching_tests.log" };
    ClientRequestQueue client_request_queue{ Limits::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ Limits::MAX_CLIENT_UPDATES };
    MarketUpdateQueue market_update_queue{ Limits::MAX_MARKET_UPDATES };
    OrderMatchingEngine ome{ &client_request_queue,
                             &client_response_queue,
                             &market_update_queue };
    TickerID ticker{ 3 };
    std::unique_ptr<OMEOrderBook> ob{ nullptr };

    // market maker1 order details
    OMEOrder maker1{
            ticker, 1, 1, 1,
            Side::SELL, 100, 100,
            Priority_INVALID, nullptr, nullptr
    };
    // market maker2 order details
    OMEOrder maker2{
            ticker, 2, 1, 2,
            Side::SELL, 102, 100,
            Priority_INVALID, nullptr, nullptr
    };

    void SetUp() override {
        ob = std::make_unique<OMEOrderBook>(ticker, logger, ome);
        ob->add(maker1.client_id, maker1.client_order_id,
                ticker, maker1.side, maker1.price, maker1.qty);
        ob->add(maker2.client_id, maker2.client_order_id,
                ticker, maker2.side, maker2.price, maker2.qty);
    }

    void TearDown() override {
    }
};


TEST_F(ExchangeOrderBookMatching, executes_partial_match) {
    // the match() method executes a given order against
    // a passive one, partially consuming the passive
    // order in the book
    auto matched = ob->get_ask_levels_by_price()->order_0;
    Qty qty_remains{ 50 };    // the bid is to buy 50 units
    ob->match_test(ticker, 3, Side::BUY, 1, 3, matched, &qty_remains);
    EXPECT_EQ(qty_remains, 0);  // the entire order should be matched
    // an order filled response is sent to the passive book order client
    // and it indicates the qty remaining in the book
    auto res = ob->get_client_response();
    EXPECT_EQ(res->type, OMEClientResponse::Type::FILLED);
    EXPECT_EQ(res->qty_exec, 50);
    EXPECT_EQ(res->qty_remain, 50);
    // a market update is created to update the left over
    // passive order in the book
    auto update = ob->get_market_update();
    EXPECT_EQ(update->type, OMEMarketUpdate::Type::MODIFY);
    EXPECT_EQ(update->qty, 50);
}

TEST_F(ExchangeOrderBookMatching, executes_full_match) {
    // the match() method executes a given order against
    // a passive one, entirely consuming the passive order
    auto matched = ob->get_ask_levels_by_price()->order_0;
    Qty qty_remains{ 100 };    // the bid is to buy 100 units
    ob->match_test(ticker, 3, Side::BUY, 1, 3, matched, &qty_remains);
    EXPECT_EQ(qty_remains, 0);  // the entire order is matched
    // an order filled response is sent to the passive book order client
    // and it indicates the correct qty remaining in the book
    auto res = ob->get_client_response();
    EXPECT_EQ(res->type, OMEClientResponse::Type::FILLED);
    EXPECT_EQ(res->qty_exec, 100);
    EXPECT_EQ(res->qty_remain, 0);
    // a market update is created to publish that the
    // passive order was completely consumed
    auto update = ob->get_market_update();
    EXPECT_EQ(update->type, OMEMarketUpdate::Type::CANCEL);
    EXPECT_EQ(update->qty, 100);
    EXPECT_EQ(update->price, 100);
}

TEST_F(ExchangeOrderBookMatching, finds_match_for_bid) {
    // an incoming bid is matched to the two existing asks
    // on the order book, consuming them both and leaving
    // some remainder
    auto remainder = ob->find_match_test(3, 1,
                                         ticker, Side::BUY,
                                         102, 225, 3);
    EXPECT_EQ(remainder, 25);
}

TEST_F(ExchangeOrderBookMatching, finds_match_for_ask) {
    // an incoming ask is matched by two existing bids
    // on the order book. the bids are buying less product
    // than the ask is offering, so a remaining bid is left behind
    ob->add(3, 1, ticker, Side::BUY, 90, 45);
    ob->add(4, 1, ticker, Side::BUY, 89, 25);
    auto remainder = ob->find_match_test(5, 1,
                                         ticker, Side::SELL,
                                         89, 75, 5);
    EXPECT_EQ(remainder, 5);
}

TEST_F(ExchangeOrderBookMatching, incoming_bid_order_matches_passive_ask) {
    // an incoming bid is matched to one of the existing
    // passive asks on the order book. there is still qty unfilled, so
    // a new passive order is created and added to the bid side of the book.

    // an aggressive bid for 177 units at price 100
    ob->add(3, 1, ticker, Side::BUY, 100, 177);
    // a market update is created to notify of the new passive order
    // left remaining on the book
    auto update = ob->get_market_update();
    EXPECT_EQ(update->type, OMEMarketUpdate::Type::ADD);
    EXPECT_EQ(update->price, 100);
    EXPECT_EQ(update->qty, 77);
    // the passive bid was added to the book
    ASSERT_NE(ob->get_bid_levels_by_price(), nullptr);
    auto bid = ob->get_bid_levels_by_price()->order_0;
    EXPECT_EQ(bid->qty, 77);
    EXPECT_EQ(bid->price, 100);
    // ... and the ask order was correctly removed after being consumed
    ASSERT_NE(ob->get_ask_levels_by_price(), nullptr);
    auto ask = ob->get_ask_levels_by_price()->order_0;
    EXPECT_EQ(ask->price, maker2.price);
    EXPECT_EQ(ask->qty, maker2.qty);
    // only the one price level should exist now (the other was deleted with the ask order)
    EXPECT_EQ(ob->get_ask_levels_by_price()->next, ob->get_ask_levels_by_price());
}

TEST_F(ExchangeOrderBookMatching, incoming_ask_order_matches_passive_bids) {
    // an incoming ask is matched to an existing bid on the
    // order book. the ask is for the same amount of product the bids
    // are for, so all matched orders are fulfilled completely and removed from the book.
    ob->add(3, 1, ticker, Side::BUY, 90, 45);
    ob->add(4, 1, ticker, Side::BUY, 89, 25);
    // an incoming ask to sell exactly 70 units at limit 89
    ob->add(5, 1, ticker, Side::SELL, 89, 70);
    // no bids should exist on the book any longer
    EXPECT_EQ(ob->get_bid_levels_by_price(), nullptr);
    // only the two asks for limit 100 and 102 should remain
    EXPECT_EQ(ob->get_ask_levels_by_price()->price, maker1.price);
    EXPECT_EQ(ob->get_ask_levels_by_price()->next->price, maker2.price);
}
