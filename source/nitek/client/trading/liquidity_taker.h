/**
 *
 *  Nitek - Client
 *
 *  @file liquidity_taker.h
 *  @brief Liquidity taking trading algorithm which aggressively buys and sells to
 *  cross the spread
 *  @author Stacy Gaudreau
 *  @date 2025.01.02
 *
 */


#pragma once

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
class LiquidityTaker {
public:
    /**
     * @brief Trading algorithm which profits by crossing the spread and placing aggressive orders.
     * @details This strategy tries to predict the direction of the market - buying when it thinks
     * the price is increasing, and selling when it thinks the price will decrease.
     */
    LiquidityTaker(TradingEngine& trading_engine, FeatureEngine& feature_engine,
                   OrderManager& order_manager, TradeEngineConfByTicker& ticker_to_te_conf,
                   LL::Logger& logger);

    inline void on_order_book_update(TickerID ticker, Price price, Side side,
                              TEOrderBook& ob) noexcept {
        (void) ticker;
        (void) price;
        (void) side;
        (void) ob;
        // does nothing on order book updates
    }

    /**
     * @brief Process a trade event and generate aggressive market orders if the trading
     * threshold and other features are appropriate.
     */
    inline void on_trade_update(const Exchange::OMEMarketUpdate& update,
                         TEOrderBook& ob) noexcept {
        logger.logf("% <LiquidityTaker::%> trade update: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    update.to_str().c_str());
        const auto bbo = ob.get_bbo();
        const auto trade_qty_ratio = feng.get_aggressive_trade_qty_ratio();
        if (bbo.bid != Price_INVALID && bbo.ask != Price_INVALID
                    && trade_qty_ratio != Feature_INVALID) {
            logger.logf("% <LiquidityTaker::%> bbo: %, trade_qty_ratio: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__, bbo.to_str().c_str(),
                        trade_qty_ratio);
            const auto trade_size = ticker_to_te_conf.at(update.ticker_id).trade_size;
            const auto threshold = ticker_to_te_conf.at(update.ticker_id).threshold;
            // send/update a market order iff the threshold is crossed -> only one ASK or BUY is sent
            if (trade_qty_ratio >= threshold) {
                if (update.side == Side::BUY)
                    oman.manage_orders(update.ticker_id, bbo.ask, Price_INVALID,
                                       trade_size);
                else
                    oman.manage_orders(update.ticker_id, Price_INVALID, bbo.bid,
                                       trade_size);
            }
        }
    }

    inline void on_order_response(const Exchange::OMEClientResponse& response) noexcept {
        // pass the response on to the order manager
        logger.logf("% <LiquidityTaker::%> %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    response.to_str().c_str());
        oman.on_order_response(response);
    }

PRIVATE_IN_PRODUCTION
    FeatureEngine& feng;
    OrderManager& oman;
    // map ticker to TradeEngineConf
    TradeEngineConfByTicker ticker_to_te_conf;

    std::string t_str{ };
    LL::Logger& logger;

DELETE_DEFAULT_COPY_AND_MOVE(LiquidityTaker)
};
}