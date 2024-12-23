#include "market_maker.h"

namespace Client
{
MarketMaker::MarketMaker(TradingEngine& trading_engine, FeatureEngine& feature_engine,
                         OrderManager& order_manager, TradeEngineConfByTicker& ticker_to_te_conf,
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

void MarketMaker::on_order_book_update(TickerID ticker,
                                       Price price,
                                       Side side,
                                       TEOrderBook& ob) noexcept {
    logger.logf("% <MarketMaker::%> ticker: %, price: %, side: %\n",
                LL::get_time_str(&t_str), __FUNCTION__, ticker,
                price_to_str(price), side_to_str(side));

    const auto bbo = ob.get_bbo();
    const auto market_price = feng.get_market_price();
    if (bbo.bid != Price_INVALID && bbo.ask != Price_INVALID && market_price != Feature_INVALID) {
        logger.logf("% <MarketMaker::%> fair_market_price: %, ticker: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, ticker,
                    market_price);
        const auto trade_size = ticker_to_te_conf.at(ticker).trade_size;
        const auto threshold = ticker_to_te_conf.at(ticker).threshold;
        const auto bid = bbo.bid - (market_price - bbo.bid >= threshold ? 0 : 1);
        const auto ask = bbo.ask + (bbo.ask - market_price >= threshold ? 0 : 1);
        oman.manage_orders(ticker, bid, ask, trade_size);
    }
}

void MarketMaker::on_trade_update(const Exchange::OMEMarketUpdate& update,
                                  TEOrderBook& ob) noexcept {
    (void) update;
    (void) ob;
    // this algo does nothing on trade updates
}

void MarketMaker::on_order_response(const Exchange::OMEClientResponse& response) noexcept {
    // forward order response to the OrderManager
    logger.logf("% <MarketMaker::%> %\n",
                LL::get_time_str(&t_str), __FUNCTION__, response.to_str());
    oman.on_order_response(response);
}
}
