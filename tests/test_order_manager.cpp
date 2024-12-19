#include "gtest/gtest.h"
#include <vector>
#include "common/types.h"
#include "client/trading/trading_engine.h"
#include "client/trading/order_manager.h"
#

using namespace Client;
using namespace Common;

using Response = Exchange::OMEClientResponse;
using Request = Exchange::OMEClientResponse;

// test suite for the OrderManager module
class OrderManagement : public ::testing::Test {
protected:
    ClientID CLIENT{ 1 };
    TickerID TICKER{ 4 };
    Exchange::ClientRequestQueue tx_requests{ Exchange::Limits::MAX_PENDING_ORDER_REQUESTS };
    Exchange::ClientResponseQueue rx_responses{ Exchange::Limits::MAX_CLIENT_UPDATES };
    Exchange::MarketUpdateQueue rx_updates{ Exchange::Limits::MAX_MARKET_UPDATES };
    LL::Logger logger{ "order_manager_tests.log" };
    TradeEngineConfByTicker te_confs;
    PositionManager pman{ logger };
    TradingEngine engine{ CLIENT, TradeAlgo::MARKET_MAKER,
                          te_confs, tx_requests,
                          rx_responses, rx_updates };
    RiskManager rman{ pman, te_confs, logger };
    OrderManager oman{ engine, rman, logger };

    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(OrderManagement, manager_basic_properties) {
    EXPECT_EQ(oman.next_oid, 1);
}

TEST_F(OrderManagement, order_basic_properties) {
    OMOrder order{};
    EXPECT_EQ(order.qty, Qty_INVALID);
    EXPECT_EQ(order.state, OMOrder::State::INVALID);
    EXPECT_EQ(order.ticker, TickerID_INVALID);
    EXPECT_EQ(order.id, OrderID_INVALID);
    EXPECT_EQ(order.side, Side::INVALID);
    EXPECT_EQ(order.price, Price_INVALID);
}

TEST_F(OrderManagement, gets_order_by_ticker) {
    auto order = OMOrder{ TICKER, 999, Side::BUY,
                          100, 10, OMOrder::State::PENDING_NEW };
    oman.ticker_to_order_by_side.at(TICKER).at(side_to_index(Side::BUY)) = order;
    auto res = oman.get_order_by_side(TICKER).at(side_to_index(Side::BUY));
    EXPECT_EQ(order, res);
}

TEST_F(OrderManagement, manage_cancels_order_on_price_mismatch) {
    // a price mismatch on a live order causes it to be cancelled by the manager
    auto order = oman.ticker_to_order_by_side.at(TICKER).at(side_to_index(Side::BUY));
    order.price = 100;
    order.state = OMOrder::State::LIVE;
    oman.manage_order(order, TICKER, 101, Side::BUY, 10);
    // the order should now be pending cancellation
    EXPECT_EQ(order.state, OMOrder::State::PENDING_CANCEL);
}

TEST_F(OrderManagement, manage_creates_new_order) {
    // a new order is created (PENDING_NEW) when the risk manager passes the request
    auto order = oman.ticker_to_order_by_side.at(TICKER).at(side_to_index(Side::BUY));
    order.price = 100;
    // risk must be permissible
    auto& risk_conf = rman.risk_by_ticker.at(TICKER).conf;
    risk_conf.size_max = 1000;
    risk_conf.position_max = 1000;
    risk_conf.loss_max = -1000;
    // the order should now be pending cancellation
    oman.manage_order(order, TICKER, 100, Side::BUY, 20);
    EXPECT_EQ(order.state, OMOrder::State::PENDING_NEW);
}

TEST_F(OrderManagement, order_response_accepted) {
    // an ACCEPTED order response is received from the exchange
    Response response {Response::Type::ACCEPTED, CLIENT, TICKER,
                       0, 0, Side::BUY,
                       100, 50, 50 };
    auto& order = oman.ticker_to_order_by_side.at(TICKER).at(side_to_index(Side::BUY));
    oman.on_order_response(response);
    // the order should now be LIVE
    EXPECT_EQ(order.state, OMOrder::State::LIVE);
}

TEST_F(OrderManagement, order_response_cancelled) {
    // a CANCELLED order response is received from the exchange
    Response response {Response::Type::CANCELLED, CLIENT, TICKER,
                       0, 0, Side::BUY,
                       100, 50, 50 };
    auto& order = oman.ticker_to_order_by_side.at(TICKER).at(side_to_index(Side::BUY));
    oman.on_order_response(response);
    // the order should now be LIVE
    EXPECT_EQ(order.state, OMOrder::State::DEAD);
}

TEST_F(OrderManagement, order_response_partially_filled) {
    // a partially FILLED order response is received from the exchange
    Response response {Response::Type::FILLED, CLIENT, TICKER,
                       0, 0, Side::BUY,
                       100, 25, 75 };
    auto& order = oman.ticker_to_order_by_side.at(TICKER).at(side_to_index(Side::BUY));
    order.qty = 100;
    order.state = OMOrder::State::LIVE;
    oman.on_order_response(response);
    // the order should still be live and it should have its QTY modified
    EXPECT_EQ(order.state, OMOrder::State::LIVE);
    EXPECT_EQ(order.qty, 75);
}

TEST_F(OrderManagement, order_response_completely_filled) {
    // a completely FILLED order response is received from the exchange
    Response response {Response::Type::FILLED, CLIENT, TICKER,
                       0, 0, Side::BUY,
                       100, 25, 0 };
    auto& order = oman.ticker_to_order_by_side.at(TICKER).at(side_to_index(Side::BUY));
    order.qty = 25;
    order.state = OMOrder::State::LIVE;
    oman.on_order_response(response);
    // the order should now be dead since it was completely filled
    EXPECT_EQ(order.state, OMOrder::State::DEAD);
    EXPECT_EQ(order.qty, 0);
}
