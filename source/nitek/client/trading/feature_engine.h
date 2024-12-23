/**
 *
 *  Nitek - Client
 *
 *  @file feature_engine.h
 *  @brief Computation of trading algorithm features/signals which will drive trading strategies
 *  @author Stacy Gaudreau
 *  @date 2024.12.06
 *
 */


#pragma once

#include "llbase/macros.h"
#include "llbase/logging.h"
#include "common/types.h"
#include "client/orders/te_order_book.h"
#include <string>

using namespace Common;

namespace Client
{
/** @brief Uninitialised or invalid trading feature value. */
constexpr auto Feature_INVALID{ std::numeric_limits<double>::quiet_NaN() };

/**
 * @brief Handles computing trading algorithm signals/features, which are helpful in driving
 * trading strategies and other financial statistics.
 */
class FeatureEngine {
public:
    explicit FeatureEngine(LL::Logger& logger)
            : logger(logger) { }

    /**
     * @brief Called when there is an update to the Order Book in the Trading Engine to
     * compute and update trading features.
     */
    void on_order_book_update(TickerID ticker, Price price, Side side,
                              TEOrderBook& book) noexcept {
        /*
         *  Best Bid Offer is used to compute a fair market price
         *      P = (bid * qty_bid + ask * qty_ask) / (qty_bid + qty_ask)
         *  -> market price computed as book qty weighted price
         *  -> this method moves price closer to ask if there are more buy orders
         *     and closer to bid if there are more sell than buy orders
         *  -> a more sophisticated algo might compare to the midpoint price
         *     (ie: average of bid and ask) to determine an offset from it, as well as
         *     a potential confidence adjustment after looking at the spread between bid/ask
         */
        const auto bbo = book.get_bbo();
        if (bbo.bid != Price_INVALID && bbo.ask != Price_INVALID) [[likely]] {
            market_price = (bbo.bid * bbo.ask_qty + bbo.ask * bbo.bid_qty)
                           / static_cast<double>(bbo.bid_qty + bbo.ask_qty);
        }
        logger.logf("% <FE::%> ticker: %, price: %, side: %, mkt_price: %, agg_ratio: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    ticker_id_to_str(ticker), price_to_str(price), side_to_str(side),
                    market_price, aggressive_trade_qty_ratio);
    }

    /**
     * @brief Called when there is a trade event in the market data stream to update
     * trading features.
     */
    void on_trade_update(const Exchange::OMEMarketUpdate& update,
                         TEOrderBook& book) noexcept {
        // use BBO to compute simple trade pressure, ie: how large the incoming trade
        // update is relative to how much liquidity there is on the other side of the
        // trade
        const auto bbo = book.get_bbo();
        if (bbo.bid != Price_INVALID && bbo.ask != Price_INVALID) [[likely]] {
            aggressive_trade_qty_ratio = static_cast<double>(update.qty)
                    / (update.side == Side::BUY ? bbo.ask_qty : bbo.bid_qty);
        }
        logger.logf("% <FE::%> update: %, mkt_price: %, agg_ratio: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    update.to_str(), market_price, aggressive_trade_qty_ratio);
    }

    inline double get_market_price() const noexcept {
        return market_price;
    }

    inline double get_aggressive_trade_qty_ratio() const noexcept {
        return aggressive_trade_qty_ratio;
    }


PRIVATE_IN_PRODUCTION
    std::string t_str{ };
    LL::Logger& logger;
    double market_price{ Feature_INVALID }; // fair market price value
    double aggressive_trade_qty_ratio{ Feature_INVALID };   // aggressive trade QTY ratio

DELETE_DEFAULT_COPY_AND_MOVE(FeatureEngine)
};
}
