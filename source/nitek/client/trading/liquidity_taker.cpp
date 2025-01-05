#include "liquidity_taker.h"

namespace Client
{
LiquidityTaker::LiquidityTaker(TradingEngine& trading_engine,
                               FeatureEngine& feature_engine,
                               OrderManager& order_manager,
                               TradeEngineConfByTicker& ticker_to_te_conf,
                               LL::Logger& logger)
        : feng(feature_engine),
          oman(order_manager),
          ticker_to_te_conf(ticker_to_te_conf),
          logger(logger) {
    trading_engine.on_order_book_update_callback = [this](auto ticker, auto price, auto side,
                                                          auto& ob) {
        on_order_book_update(ticker, price, side, ob);
    };
    trading_engine.on_trade_update_callback = [this](auto& update, auto& ob) {
        on_trade_update(update, ob);
    };
    trading_engine.on_order_response_callback = [this](auto& response) {
        on_order_response(response);
    };
}
}