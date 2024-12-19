/**
 *
 *  Nitek - Client
 *
 *  @file market_maker.h
 *  @brief Trading algorithm for market making (earning the spread)
 *  @author Stacy Gaudreau
 *  @date 2024.12.18
 *
 */


#pragma once

#include "llbase/macros.h"
#include "llbase/logging.h"
#include "common/types.h"
#include "client/trading/trading_engine.h"
#include "client/trading/feature_engine.h"
#include "client/trading/order_manager.h"


using namespace Common;

namespace Client
{
class MarketMaker {
public:
    MarketMaker(TradingEngine& trading_engine, FeatureEngine& feature_engine,
                OrderManager& order_manager, TradeEngineConfByTicker& ticker_to_te_conf,
                LL::Logger& logger);

    void on_order_book_update(TickerID ticker, Price price, Side side,
                              TEOrderBook& ob) noexcept;
    void on_trade_update(const Exchange::OMEMarketUpdate& update,
                         TEOrderBook& ob) noexcept;
    void on_order_response(const Exchange::OMEClientResponse& response) noexcept;


PRIVATE_IN_PRODUCTION
    FeatureEngine& feng;
    OrderManager& oman;
    // map ticker to TradeEngineConf
    TradeEngineConfByTicker ticker_to_te_conf;

    std::string t_str{ };
    LL::Logger& logger;

DELETE_DEFAULT_COPY_AND_MOVE(MarketMaker)
};
}