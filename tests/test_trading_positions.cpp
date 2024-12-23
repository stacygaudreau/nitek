#include "gtest/gtest.h"
#include <vector>
#include "common/types.h"
#include "client/trading/feature_engine.h"
#include "client/trading/position_manager.h"
#include "client/orders/te_order_book.h"
#include "client/trading/trading_engine.h"

using namespace Client;
using namespace Common;
using Response = Exchange::OMEClientResponse;
using Update = Exchange::OMEMarketUpdate;

// trading feature and calculation tests
class FeatureEngineFeatures : public ::testing::Test {
protected:
    TickerID TICKER{ 3 };
    static constexpr size_t N_TRADES{ 10 };
    LL::Logger logger{ "feature_engine_tests.log" };
    FeatureEngine feng{ logger };
    TEOrderBook book{ TICKER, logger };
    std::vector<Update> updates;

    void SetUp() override {
        for (size_t i{}; i < N_TRADES; ++i) {
            auto side = i % 2 == 0 ? Side::BUY : Side::SELL;
            auto price = side == Side::BUY ? 100 : 80;
            updates.emplace_back(Update(Update::Type::ADD, i,
                                        TICKER, side, price, 50,
                                        1));
        }
    }
    void TearDown() override {
    }
};

TEST_F(FeatureEngineFeatures, computes_fair_market_price) {
    /*
     *  orders are added to the book such that a BBO is available.
     *  a market price is then determined as a simple BBO price
     *  weighted by volume
     */
    for (auto& update: updates) {
        book.on_market_update(update);
    }
    feng.on_order_book_update(TICKER, 80, Side::BUY, book);
    EXPECT_NE(feng.market_price, Feature_INVALID);
    EXPECT_DOUBLE_EQ(feng.market_price, 90.0);
}

TEST_F(FeatureEngineFeatures, computes_trade_pressure) {
    /*
     *  orders in the book are used to determine the simple trade
     *  pressure of an incoming trade
     */
    for (auto& update: updates) {
        book.on_market_update(update);
    }
    // there is QTY=250 on the book for both Sides of the BBO
    auto update1 = Update(Update::Type::TRADE, 0, TICKER,
                          Side::BUY, 100, 250, 1);
    // an incoming QTY of 250 should have a pressure ratio of 1.0
    feng.on_trade_update(update1, book);
    EXPECT_EQ(feng.aggressive_trade_qty_ratio, 1.0);
    // an incoming QTY of 125 should have a ratio of 0.5
    update1.qty = 125;
    feng.on_trade_update(update1, book);
    EXPECT_EQ(feng.aggressive_trade_qty_ratio, 0.5);
    // and an incoming QTY of 1000 should have a pressure ratio of 4.0
    update1.qty = 1000;
    update1.side = Side::SELL;
    feng.on_trade_update(update1, book);
    EXPECT_EQ(feng.aggressive_trade_qty_ratio, 4.0);
}


// tests for Positions and their calculations
class PositionsAndPnL : public ::testing::Test {
protected:
    Position p;
    Response response;
    LL::Logger logger{ "trading_positions_tests.log" };

    void SetUp() override {
        response.type = Response::Type::FILLED;
        response.ticker_id = 0;
        response.client_id = 0;
        response.client_order_id = 0;
    }

    void TearDown() override {
    }
};

TEST_F(PositionsAndPnL, buy_updates_position) {
    // a buy order is filled and the position qty is updated
    response.side = Side::BUY;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    EXPECT_EQ(p.position, 10);
    EXPECT_EQ(p.volume, 10);
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    EXPECT_EQ(p.position, 20);
    EXPECT_EQ(p.volume, 20);
}

TEST_F(PositionsAndPnL, sell_updates_position) {
    // a sell order is filled and the position qty is updated
    response.side = Side::SELL;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    EXPECT_EQ(p.position, -10);
    EXPECT_EQ(p.volume, 10);
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    EXPECT_EQ(p.position, -20);
    EXPECT_EQ(p.volume, 20);
}

TEST_F(PositionsAndPnL, fill_updates_open_vwap_buy) {
    // a buy order is filled and open VWAP on the buy side is updated
    response.side = Side::BUY;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    EXPECT_EQ(p.vwap_open.at(side_to_index(Side::BUY)), 1000);
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    EXPECT_EQ(p.vwap_open.at(side_to_index(Side::BUY)), 1900);
}

TEST_F(PositionsAndPnL, fill_updates_open_vwap_sell) {
    // a sell order is filled and open VWAP on the sell side is updated
    response.side = Side::SELL;
    response.qty_exec = 50;
    response.price = 100;
    p.add_fill(response, logger);
    EXPECT_EQ(p.vwap_open.at(side_to_index(Side::SELL)), 5000);
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    EXPECT_EQ(p.vwap_open.at(side_to_index(Side::SELL)), 5900);
}

TEST_F(PositionsAndPnL, long_position_pnl) {
    // a long position is opened, accumulated, and PnLs are computed
    response.side = Side::BUY;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    // PnL should so far be zero as no last sell price
    // is recorded to get unrealized pnl from
    EXPECT_DOUBLE_EQ(p.pnl_unreal, 0.);
    EXPECT_DOUBLE_EQ(p.pnl_real, 0.);
    EXPECT_DOUBLE_EQ(p.pnl_total, 0.);
    // more buy order is filled
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    // now, unrealised PnL shouldbe known since there is now
    // a last fill price discovered
    EXPECT_DOUBLE_EQ(p.pnl_unreal, -100.);
    EXPECT_DOUBLE_EQ(p.pnl_real, 0.);
    EXPECT_DOUBLE_EQ(p.pnl_total, -100.);
    // some selling is done
    response.side = Side::SELL;
    response.qty_exec = 10;
    response.price = 92;
    p.add_fill(response, logger);
    // a realised PnL is now present
    EXPECT_DOUBLE_EQ(p.pnl_unreal, -30.);
    EXPECT_DOUBLE_EQ(p.pnl_real, -30.);
    EXPECT_DOUBLE_EQ(p.pnl_total, -60.);
}

TEST_F(PositionsAndPnL, position_flips_to_short_pnl) {
    // a long position flips to short, and PnL is computed properly
    response.side = Side::BUY;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    response.side = Side::SELL;
    response.qty_exec = 10;
    response.price = 92;
    p.add_fill(response, logger);
    // pnl before going short
    EXPECT_NE(p.vwap_open.at(side_to_index(Side::BUY)), 0);
    EXPECT_DOUBLE_EQ(p.pnl_unreal, -30.);
    EXPECT_DOUBLE_EQ(p.pnl_real, -30.);
    EXPECT_DOUBLE_EQ(p.pnl_total, -60.);
    // a sell order is added which flips the position short
    response.side = Side::SELL;
    response.qty_exec = 20;
    response.price = 97;
    p.add_fill(response, logger);
    EXPECT_EQ(p.position, -10);
    EXPECT_EQ(p.volume, 50);
    // the open vwap should be zeroed out on the buy side due to flipped position
    EXPECT_EQ(p.vwap_open.at(side_to_index(Side::BUY)), 0);
    // last execution price is 97 which should match the current sell
    // VWAP (970/10=97) therefore unrealised pnl is zero
    EXPECT_DOUBLE_EQ(p.pnl_unreal, 0.);
    // qty 20 sold at a profit of 97-95=2ea adds 20 to pnl
    // realized should be -30 + 20 = -10 now
    EXPECT_DOUBLE_EQ(p.pnl_real, -10.);
    EXPECT_DOUBLE_EQ(p.pnl_total, -10.);
}

TEST_F(PositionsAndPnL, position_handles_rational_vwap_pnl) {
    /*
     * up to this point, intermediary vwap calcs have been entirely integer-friendly
     * this test evaluates PnL when a fractional VWAP occurs in the calculation
     */
    // BUY 10 @ 100
    response.side = Side::BUY;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    // BUY 10 @ 90
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    // SELL 10 @ 92
    response.side = Side::SELL;
    response.qty_exec = 10;
    response.price = 92;
    p.add_fill(response, logger);
    // SELL 20 @ 97
    response.qty_exec = 20;
    response.price = 97;
    p.add_fill(response, logger);
    // SELL 20 @ 94
    response.qty_exec = 20;
    response.price = 94;
    p.add_fill(response, logger);
    EXPECT_EQ(p.position, -30);
    EXPECT_DOUBLE_EQ(p.vwap_open.at(side_to_index(Side::SELL)), 2850.);
    EXPECT_DOUBLE_EQ(p.pnl_real, -10.);
    EXPECT_DOUBLE_EQ(p.pnl_unreal, 30.);
    // SELL 10 @ 90
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    EXPECT_DOUBLE_EQ(p.vwap_open.at(side_to_index(Side::SELL)), 3750.);
    EXPECT_DOUBLE_EQ(p.pnl_real, -10.);
    EXPECT_DOUBLE_EQ(p.pnl_unreal, 150.);   // result depends on a fractional VWAP
}

TEST_F(PositionsAndPnL, position_is_closed_pnl) {
    /*
     *  a long position is opened, accumulated, shorted and then flattened to zero.
     *  the PnL is computed and verified
     */
    // BUY 10 @ 100
    response.side = Side::BUY;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    // BUY 10 @ 90
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    // SELL 10 @ 92
    response.side = Side::SELL;
    response.qty_exec = 10;
    response.price = 92;
    p.add_fill(response, logger);
    // SELL 20 @ 97
    response.qty_exec = 20;
    response.price = 97;
    p.add_fill(response, logger);
    // SELL 20 @ 94
    response.qty_exec = 20;
    response.price = 94;
    p.add_fill(response, logger);
    // SELL 10 @ 90
    response.qty_exec = 10;
    response.price = 90;
    p.add_fill(response, logger);
    // BUY 40 @ 88
    response.side = Side::BUY;
    response.qty_exec = 40;
    response.price = 88;
    p.add_fill(response, logger);
    // nothing left of the position
    EXPECT_EQ(p.position, 0);
    EXPECT_EQ(p.volume, 120);
    // no unrealized pnl exists when closed
    EXPECT_DOUBLE_EQ(p.pnl_unreal, 0.);
    EXPECT_DOUBLE_EQ(p.pnl_real, 220.);
    EXPECT_DOUBLE_EQ(p.pnl_total, 220.);
    // both Side vwaps are zero
    EXPECT_DOUBLE_EQ(p.vwap_open.at(side_to_index(Side::SELL)), 0.);
    EXPECT_DOUBLE_EQ(p.vwap_open.at(side_to_index(Side::BUY)), 0.);
}

TEST_F(PositionsAndPnL, new_bbo_updates_long_pnl) {
    /*
     * a long position has its pnl updated by market changes
     */
    // BUY 10 @ 100
    response.side = Side::BUY;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    // BUY 10 @ 100
    response.side = Side::BUY;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    EXPECT_DOUBLE_EQ(p.pnl_unreal, 0.);
    EXPECT_EQ(p.position, 20);
    // a new BBO is received which produces a +ve PnL
    BBO bbo{ 105, 110, 100, 100 };
    p.on_bbo_update(&bbo, logger);
    EXPECT_EQ(p.pnl_unreal, 150.);
    // a new BBO produces a -ve PnL
    bbo.bid = 90;
    bbo.ask = 100;
    p.on_bbo_update(&bbo, logger);
    EXPECT_EQ(p.pnl_unreal, -100.);
}

TEST_F(PositionsAndPnL, new_bbo_updates_short_pnl) {
    /*
     * a short position has its pnl updated by market changes
     */
    // SELL 10 @ 100
    response.side = Side::SELL;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    // SELL 10 @ 100
    response.side = Side::SELL;
    response.qty_exec = 10;
    response.price = 100;
    p.add_fill(response, logger);
    EXPECT_DOUBLE_EQ(p.pnl_unreal, 0.);
    EXPECT_EQ(p.position, -20);
    // a new BBO is received which produces a +ve PnL
    BBO bbo{ 85, 95, 100, 100 };
    p.on_bbo_update(&bbo, logger);
    EXPECT_EQ(p.pnl_unreal, 200.);
    // a new BBO produces a -ve PnL
    bbo.bid = 120;
    bbo.ask = 125;
    p.on_bbo_update(&bbo, logger);
    EXPECT_EQ(p.pnl_unreal, -450.);
}


// tests for the PositionManager
class PositionManagement : public ::testing::Test {
protected:
    Response response;
    LL::Logger logger{ "trading_position_manager_tests.log" };
    PositionManager manager{ logger };

    void SetUp() override {
        response.type = Response::Type::FILLED;
        response.ticker_id = 3;
        response.client_id = 0;
        response.client_order_id = 0;
        response.price = 100;
        response.qty_exec = 10;
        response.side = Side::BUY;
        manager.add_fill(response);
    }

    void TearDown() override {
    }
};

TEST_F(PositionManagement, add_fill_modifies_position) {
    /*
     * an order is filled and passed on to the correct position
     * identified by the order response details
     */
    auto p = manager.positions.at(response.ticker_id);
    EXPECT_EQ(p.position, 10);
    EXPECT_EQ(p.volume, 10);
    EXPECT_DOUBLE_EQ(p.vwap_open.at(side_to_index(Side::BUY)), 1000.);
}

TEST_F(PositionManagement, on_bbo_update_modifies_position) {
    /*
     * a new BBO is passed on to a position through
     * on_bbo_update() and the position is updated
     */
    BBO bbo{ 110, 105, 100, 100 };
    manager.on_bbo_update(response.ticker_id, &bbo);
    auto p = manager.positions.at(response.ticker_id);
    EXPECT_DOUBLE_EQ(p.pnl_unreal, 75.);
}

TEST_F(PositionManagement, get_position_retrieves_position) {
    /*
     * the correct position is retrieved by the manager
     */
    manager.positions.at(5).position = 999;
    manager.positions.at(5).volume = 1200;
    auto p = manager.get_position(5);
    EXPECT_EQ(p.position, 999);
    EXPECT_EQ(p.volume, 1200);
}
