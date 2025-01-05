#include "gtest/gtest.h"
#include "client/trading/market_maker.h"
#include "client/trading/liquidity_taker.h"
#include <memory>
#include "llbase/logging.h"


using namespace Exchange;
using namespace Common;


// test for the market making algorithm
class MarketMakerAlgorithm : public ::testing::Test {
protected:
    LL::Logger logger{ "market_maker_algorithm_tests.log" };

    void SetUp() override {
    }

    void TearDown() override {
    }
};
