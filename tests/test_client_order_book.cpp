#include "gtest/gtest.h"
#include "exchange/orders/ome_order_book.h"
#include "client/orders/te_order_book.h"
#include <memory>
#include "llbase/logging.h"


using namespace Client;
using namespace Common;

using Response = Exchange::OMEClientResponse;
using Request = Exchange::OMEClientResponse;
using Update = Exchange::OMEMarketUpdate;
using namespace std::literals::chrono_literals;

//// base tests for Client-side order book module
class ClientOrderBookBasics : public ::testing::Test {
protected:
    LL::Logger logger{ "client_order_book_tests.log" };
    TickerID ticker{ 3 };
    std::unique_ptr<TEOrderBook> ob{ nullptr };
    std::vector<TEOrder> bid_orders;
    std::vector<TEOrder> ask_orders;

    void SetUp() override {
        ob = std::make_unique<TEOrderBook>(ticker, logger);
        for (size_t i{}; i < 5; ++i) {
            bid_orders.emplace_back(
                i, Side::BUY, (i % 2 == 0 ? 100 : 50),
                50, 0, nullptr, nullptr
            );
            ask_orders.emplace_back(
                    5+i, Side::SELL, (i % 2 == 0 ? 105 : 55),
                    50, 0, nullptr, nullptr
            );
        }
    }

    void TearDown() override {
    }
};


TEST_F(ClientOrderBookBasics, is_constructed) {
    EXPECT_NE(nullptr, ob);
}

TEST_F(ClientOrderBookBasics, adds_price_levels) {
    // three price levels are added to the book and it is verified
    // that they are ordered correctly

    // SELL side (asks)
    TEOrdersAtPrice A{ Side::SELL, 100, nullptr, nullptr, nullptr };
    TEOrdersAtPrice B{ Side::SELL, 125, nullptr, nullptr, nullptr };
    TEOrdersAtPrice C{ Side::SELL, 50, nullptr, nullptr, nullptr };
    ob->add_price_level(&A);
    ob->add_price_level(&B);
    ob->add_price_level(&C);
    auto asks = ob->asks_by_price;
    EXPECT_EQ(*asks, C);    // the most aggressive ask level is the lowest price
    EXPECT_EQ(*(asks->next), A);  // second most aggressive ask level
    EXPECT_EQ(*(asks->next->next), B);  // least aggressive

    // BUY side (bids)
    A.side = Side::BUY;
    B.side = Side::BUY;
    C.side = Side::BUY;
    ob->add_price_level(&A);   // 100
    ob->add_price_level(&C);   // 50
    ob->add_price_level(&B);   // 125
    auto bids = ob->bids_by_price;
    EXPECT_EQ(*bids, B);    // the most aggressive bid level is the highest price
    EXPECT_EQ(*(bids->next), A);  // second most aggressive
    EXPECT_EQ(*(bids->next->next), C);  // least aggressive
}

TEST_F(ClientOrderBookBasics, removes_price_levels) {
    // price levels are removed from the book and the ordering
    // is maintained

    // SELL side (asks)
    // we allocate the levels in the book's mempool (normally this is
    // done by the add order to book method as needed)
    auto& pool = ob->orders_at_price_pool;
    auto a = pool.allocate(Side::SELL, 25, nullptr, nullptr, nullptr);
    auto b = pool.allocate(Side::SELL, 75, nullptr, nullptr, nullptr);
    auto c = pool.allocate(Side::SELL, 125, nullptr, nullptr, nullptr);
    auto d = pool.allocate(Side::SELL, 175, nullptr, nullptr, nullptr);
    ob->add_price_level(a);
    ob->add_price_level(b);
    ob->add_price_level(c);
    ob->add_price_level(d);
    ob->remove_price_level(Side::SELL, 75);
    ob->remove_price_level(Side::SELL, 175);
    auto asks = ob->asks_by_price;
    EXPECT_EQ(*asks, *a);
    EXPECT_EQ(*asks->next, *c);

    // BUY side (bids)
    auto e = pool.allocate(Side::BUY, 200, nullptr, nullptr, nullptr);
    auto f = pool.allocate(Side::BUY, 150, nullptr, nullptr, nullptr);
    auto g = pool.allocate(Side::BUY, 100, nullptr, nullptr, nullptr);
    auto h = pool.allocate(Side::BUY, 50, nullptr, nullptr, nullptr);
    ob->add_price_level(e);
    ob->add_price_level(f);
    ob->add_price_level(g);
    ob->add_price_level(h);
    ob->remove_price_level(Side::BUY, 200);
    ob->remove_price_level(Side::BUY, 100);
    auto bids = ob->bids_by_price;
    EXPECT_EQ(*bids, *f);
    EXPECT_EQ(*bids->next, *h);
}

TEST_F(ClientOrderBookBasics, adds_bid_order_to_book) {
    OrderID c_oid{ 1 };
    Side side{ Side::BUY };
    Price price{ 100 };
    Qty qty{ 50 };
    TEOrder order1{ c_oid, side, price, qty, 1, nullptr, nullptr };
    ob->add_order(&order1);
    // 1. creates a new price level on an otherwise empty order book
    auto first = ob->get_level_for_price(price)->order_0;
    EXPECT_EQ(first->id, c_oid);
    EXPECT_EQ(first->side, side);
    EXPECT_EQ(first->price, price);
    EXPECT_EQ(first->qty, qty);
    // 2. inserts a second order to the same price level when one already exists
    TEOrder order2{ 2, side, price, 100, 2, nullptr, nullptr };
    // the additional order should be in the last position
    auto last = ob->get_level_for_price(price)->order_0->next;
    EXPECT_EQ(order1.id, last->id);
    EXPECT_EQ(order1.qty, last->qty);
}

TEST_F(ClientOrderBookBasics, removes_order_from_book) {
    // the order removal method correctly removes
    // a specific order from an order book (which has multiple
    // orders present)
    auto& pool = ob->order_pool;
    auto A = pool.allocate(1, Side::BUY, 100,
                           100, 1, nullptr, nullptr);
    ob->add_order(A);
    auto B = pool.allocate(2, Side::BUY, 100, 50, 2, nullptr, nullptr);
    ob->add_order(B);
    auto C = pool.allocate(2, Side::BUY, 100, 50, 3, nullptr, nullptr);
    ob->add_order(C);
    // remove the center order
    ob->remove_order(B);
    // verify that B is now missing in the linked list of the price level
    auto orders = ob->get_level_for_price(100);
    EXPECT_EQ(*A, *orders->order_0);
    EXPECT_EQ(*C, *orders->order_0->next);
    EXPECT_EQ(*A, *orders->order_0->next->next);
}

TEST_F(ClientOrderBookBasics, updates_bbo_ask) {
    // a new ask BBO is calculated after some orders were added
    for (auto& o: ask_orders) {
        ob->add_order(&o);
    }
    ob->update_bbo(false, true);
    EXPECT_EQ(ob->bbo.ask, 55);    // 55 is top of book (lowest) ask price
    EXPECT_EQ(ob->bbo.ask_qty, 100);    // 2 * 50
}

TEST_F(ClientOrderBookBasics, updates_bbo_bid) {
    // a new bid BBO is calculated after some orders were added
    for (auto& o: bid_orders) {
        ob->add_order(&o);
    }
    ob->update_bbo(true, false);
    EXPECT_EQ(ob->bbo.bid, 100);    // 100 is top of book bid price
    EXPECT_EQ(ob->bbo.bid_qty, 150);    // 3 * 50
}

TEST_F(ClientOrderBookBasics, updates_both_ask_and_bid) {
    // both ask and bid BBO updated in one call to method
    for (auto& o: bid_orders) {
        ob->add_order(&o);
    }
    for (auto& o: ask_orders) {
        ob->add_order(&o);
    }
    ob->update_bbo(true, true);
    EXPECT_EQ(ob->bbo.bid, 100);    // 100 is top of book bid price
    EXPECT_EQ(ob->bbo.bid_qty, 150);    // 3 * 50
    EXPECT_EQ(ob->bbo.ask, 55);    // 50 is top of book ask price
    EXPECT_EQ(ob->bbo.ask_qty, 100);    // 2 * 50
}

TEST_F(ClientOrderBookBasics, book_is_cleared) {
    for (auto o: bid_orders) {
        auto new_order = ob->order_pool.allocate(o.id, o.side,
                                                 o.price, o.qty, o.priority,
                                                 nullptr, nullptr);
        ob->id_to_order.at(o.id) = new_order;
    }
    for (auto o: ask_orders) {
        auto new_order = ob->order_pool.allocate(o.id, o.side,
                                                 o.price,o.qty, o.priority,
                                                 nullptr, nullptr);
        ob->id_to_order.at(o.id) = new_order;
    }

    ob->clear_entire_book();
    EXPECT_EQ(ob->asks_by_price, nullptr);
    EXPECT_EQ(ob->bids_by_price, nullptr);
    EXPECT_EQ(ob->id_to_order.at(0), nullptr);
}

TEST_F(ClientOrderBookBasics, market_update_adds) {
    Update update{ Update::Type::ADD, 0, ticker,
                   Side::BUY, 100, 50, 1 };
    ob->on_market_update(update);
    auto order = ob->bids_by_price->order_0;
    EXPECT_EQ(update.order_id, order->id);
    EXPECT_EQ(update.price, order->price);
    EXPECT_EQ(update.side, order->side);
    EXPECT_EQ(update.qty, order->qty);
    EXPECT_EQ(update.priority, order->priority);
    // a matching order should also have been allocated in the mapping
    EXPECT_EQ(ob->id_to_order.at(update.order_id), order);
}

TEST_F(ClientOrderBookBasics, market_update_modifies) {
    // an existing order has its qty modified on market update
    auto order = bid_orders.at(0);
    Update update{ Update::Type::ADD, order.id, ticker,
                   order.side, order.price, 150, order.priority };
    ob->on_market_update(update);
    EXPECT_NE(ob->id_to_order.at(order.id), nullptr);
}

TEST_F(ClientOrderBookBasics, market_update_cancels) {
    // an existing order is cancelled and removed
    auto order = bid_orders.at(0);
    Update update{ Update::Type::ADD, order.id, ticker,
                   order.side, order.price, order.qty, order.priority };
    ob->on_market_update(update);
    // at first the order exists
    EXPECT_NE(ob->id_to_order.at(order.id), nullptr);
    // now it is cancelled
    update.type = Update::Type::CANCEL;
    ob->on_market_update(update);
    EXPECT_EQ(ob->id_to_order.at(order.id), nullptr);
}

TEST_F(ClientOrderBookBasics, market_update_clears) {
    // a CLEAR update wipes the order book
    for (auto o: bid_orders) {
        auto new_order = ob->order_pool.allocate(o.id, o.side,
                                                 o.price, o.qty, o.priority,
                                                 nullptr, nullptr);
        ob->id_to_order.at(o.id) = new_order;
    }
    for (auto o: bid_orders) {
        EXPECT_NE(ob->id_to_order.at(o.id), nullptr);
    }
    auto order = bid_orders.at(0);
    Update update{ Update::Type::CLEAR, order.id, ticker,
                   order.side, order.price, order.qty, order.priority };
    ob->on_market_update(update);
    for (auto o: bid_orders) {
        EXPECT_EQ(ob->id_to_order.at(o.id), nullptr);
    }
}
