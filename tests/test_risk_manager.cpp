#include "gtest/gtest.h"
#include <vector>
#include "common/types.h"
#include "client/trading/risk_manager.h"

using namespace Client;
using namespace Common;

// tests for the risk management module
class RiskManagement : public ::testing::Test {
protected:
    LL::Logger logger{ "risk_manager_tests.log" };
    TradeEngineConfByTicker te_confs;
    PositionManager pman{ logger };
    RiskManager rman{ pman, te_confs, logger };
    TickerID TICKER{ 4 };
    Position& p{ pman.positions.at(TICKER) };
    Risk& risk{ rman.risk_by_ticker.at(TICKER) };
    RiskConf& rconf{ risk.conf };

    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(RiskManagement, trade_risk_allowed) {
    // a trade Risk is computed which allows a trade to occur
    p.position = 100;
    p.vwap_open.at(side_to_index(Side::BUY)) = 1000;
    p.volume = 100;
    rconf.loss_max = -1000;
    rconf.position_max = 1000;
    rconf.size_max = 1000;
    auto risk_result = risk.get_trade_risk(Side::BUY, 100);
    EXPECT_EQ(risk_result, Risk::Result::ALLOWED);
}

TEST_F(RiskManagement, trade_risk_size_too_large) {
    // a trade with too large of sizing is denied by risk management
    p.position = 100;
    p.vwap_open.at(side_to_index(Side::BUY)) = 1000;
    p.volume = 100;
    rconf.loss_max = -1000;
    rconf.position_max = 1000;
    rconf.size_max = 50;
    auto risk_result = risk.get_trade_risk(Side::BUY, 100);
    EXPECT_EQ(risk_result, Risk::Result::SIZE_TOO_LARGE);
}

TEST_F(RiskManagement, trade_risk_position_too_large_long) {
    // a long trade which will exceed position sizing is blocked
    p.position = 100;
    p.vwap_open.at(side_to_index(Side::BUY)) = 1000;
    p.volume = 100;
    rconf.loss_max = -1000;
    rconf.position_max = 150;
    rconf.size_max = 100;
    auto risk_result = risk.get_trade_risk(Side::BUY, 100);
    EXPECT_EQ(risk_result, Risk::Result::POSITION_TOO_LARGE);
}

TEST_F(RiskManagement, trade_risk_position_too_large_short) {
    // a short trade which will exceed position sizing is blocked
    p.position = 100;
    p.vwap_open.at(side_to_index(Side::BUY)) = 1000;
    p.volume = 100;
    rconf.loss_max = 1000;
    rconf.position_max = 150;
    rconf.size_max = 5000;
    auto risk_result = risk.get_trade_risk(Side::SELL, 1000);
    EXPECT_EQ(risk_result, Risk::Result::POSITION_TOO_LARGE);
}

TEST_F(RiskManagement, trade_risk_loss_exceeded) {
    // a trade exceeds losses and risk management blocks it
    p.position = 100;
    p.vwap_open.at(side_to_index(Side::BUY)) = 1000;
    p.volume = 100;
    p.pnl_total = -1100;
    rconf.loss_max = -1000;
    rconf.position_max = 5000;
    rconf.size_max = 5000;
    auto risk_result = risk.get_trade_risk(Side::BUY, 50);
    EXPECT_EQ(risk_result, Risk::Result::LOSS_TOO_LARGE);
}

TEST_F(RiskManagement, manager_get_trade_risk_accepted) {
    // a trade is passed through the manager module and permitted
    p.position = 100;
    p.vwap_open.at(side_to_index(Side::BUY)) = 1000;
    p.volume = 100;
    rconf.loss_max = -1000;
    rconf.position_max = 1000;
    rconf.size_max = 1000;
    auto risk_result = rman.get_trade_risk(TICKER, Side::BUY, 100);
    EXPECT_EQ(risk_result, Risk::Result::ALLOWED);
}

