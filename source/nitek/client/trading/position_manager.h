/**
 *
 *  Nitek - Client
 *
 *  @file position_manager.h
 *  @brief Maintains trading positions, computing and tracking PnL
 *  @author Stacy Gaudreau
 *  @date 2024.12.07
 *
 */


#pragma once

#include "llbase/macros.h"
#include "llbase/logging.h"
#include "common/types.h"
#include "exchange/data/ome_client_response.h"
#include "client/orders/te_order_book.h"
#include <string>
#include <array>

using namespace Common;

namespace Client
{

class Position {
public:
    /**
     * @brief Trade positioning for a single financial instrument in the PositionManager.
     */
    Position() = default;

    [[nodiscard]] inline std::string to_str() const {
        std::stringstream ss;
        ss << "<Position>"
           << " ["
           << "pos: " << position
           << " unreal: " << pnl_unreal
           << " real: " << pnl_real
           << " pnl: " << pnl_total
           << " vol: " << volume
           << " vwap: [" << (position ? vwap_open.at(side_to_index(Side::BUY))
                                                        / std::abs(position) : 0)
           << " x " << (position ? vwap_open.at(side_to_index(Side::SELL)) / std::abs(position) : 0)
           << "]"
           << (bbo ? " " + bbo->to_str() : "")
           << "]";
        return ss.str();
    }

    /**
     * @brief Call to update the Position when an order fill occurs.
     * @details Applies a single order fill response message directly to the position's status.
     */
    void add_fill(const Exchange::OMEClientResponse& response, LL::Logger& logger) {
        const auto position_old = position;
        const auto i_side = side_to_index(response.side);
        const auto i_side_opposite = side_to_index(response.side == Side::BUY
                                    ? Side::SELL :Side::BUY);
        const auto side_value = side_to_value(response.side);
        position += static_cast<int32_t>(response.qty_exec) * side_value;
        volume += response.qty_exec;
        const auto postion_opened_or_increased = position_old * side_value >= 0;
        if (postion_opened_or_increased) {
            // simple open VWAP update => no change in realized PnL
            vwap_open[i_side] += static_cast<double>(response.price * response.qty_exec);
        } else {
            // position size was decreased => realized PnL needs updating
            const auto vwap_opposite = vwap_open[i_side_opposite] / std::abs(position_old);
            vwap_open[i_side_opposite] = vwap_opposite * std::abs(position);
            pnl_real += std::min(static_cast<int32_t>(response.qty_exec), std::abs(position_old))
                        * side_value * (vwap_opposite - (double)response.price);
            if (position * position_old < 0) {
                // the position's sign was flipped so we handle that edge case here
                vwap_open[i_side] = (double)response.price * std::abs(position);
                vwap_open[i_side_opposite] = 0;
            }
        }

        if (!position) {
            // position is flat
            vwap_open[side_to_index(Side::BUY)] = vwap_open[side_to_index(Side::SELL)] = 0;
            pnl_unreal = 0;
        } else {
            if (position > 0) {
                // long
                pnl_unreal = ((double)response.price - vwap_open[side_to_index(Side::BUY)]
                            / std::abs (position)) * std::abs(position);
            } else {
                // short
                pnl_unreal = (vwap_open[side_to_index(Side::SELL)] / std::abs(position)
                            - (double)response.price) * std::abs(position);
            }
        }

        pnl_total = pnl_unreal + pnl_real;

        std::string t_str;
        logger.logf("% <Position::%> % %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    to_str(), response.to_str());
    }

    /**
     * @brief Update this Position by calling when the Best Bid Offer is updated.
     * @details Handles updating Positions based on market changes, eg: the unrealized
     * PnL must be updated as market prices fluctuate.
     */
    void on_bbo_update(const BBO* new_bbo, LL::Logger& logger) noexcept {
        bbo = new_bbo;
        // nothing to do if the position size is zero
        if (position != 0 && bbo->bid != Price_INVALID && bbo->ask != Price_INVALID) {
            // the mid price is used to update unrealized PnL as market prices change
            const auto mid = (double)(bbo->bid + bbo->ask) * 0.5;
            if (position > 0) {
                // long
                pnl_unreal = (mid - vwap_open[side_to_index(Side::BUY)] / std::abs(position))
                            * std::abs(position);
            } else {
                // short
                pnl_unreal = (vwap_open[side_to_index(Side::SELL)] / std::abs(position) - mid)
                            * std::abs(position);
            }
            const auto pnl_total_prev = pnl_total;
            pnl_total = pnl_unreal + pnl_real;
            if (pnl_total != pnl_total_prev) {
                std::string t_str;
                logger.logf("% <Position::%> % %\n",
                            LL::get_time_str(&t_str), __FUNCTION__,
                            to_str(), bbo->to_str());
            }
        }

    }

PRIVATE_IN_PRODUCTION
    int32_t position{ };    // current open position (QTY of instrument)
    double pnl_real{ };     // realised, closed PnL
    double pnl_unreal{ };   // unrealised, open PnL
    double pnl_total{ };    // sum of real. and unreal. PnL
    // VWAP of open position
    std::array<double, side_to_index(Side::MAX) + 1> vwap_open{ };
    Qty volume{ };  // total QTY traded in position
    const BBO* bbo{ nullptr };  // top of book prices
};


class PositionManager {
public:
    /**
     * @brief Maintains trading positions in the trading client. Tracks PnL and
     * executes orders on positions.
     */
    explicit PositionManager(LL::Logger& logger)
            : logger(logger) { }

    inline void add_fill(const Exchange::OMEClientResponse& response) noexcept {
        positions.at(response.ticker_id).add_fill(response, logger);
    }

    inline void on_bbo_update(TickerID ticker, const BBO* bbo) noexcept {
        positions.at(ticker).on_bbo_update(bbo, logger);
    }

    [[nodiscard]] inline Position& get_position(TickerID ticker_id) noexcept {
        return positions.at(ticker_id);
    }

    [[nodiscard]] std::string to_str() const {
        double pnl_total{ };
        Qty vol_total{ };
        std::stringstream ss;
        for (TickerID id{ }; id < positions.size(); ++id) {
            const auto p = positions.at(id);
            ss << "TickerID: " << ticker_id_to_str(id) << " " << p.to_str() << "\n";
            pnl_total += p.pnl_total;
            vol_total += p.volume;
        }
        ss << "TOTALS - PnL: " << pnl_total << " Volume: " << vol_total << "\n";
        return ss.str();
    }

PRIVATE_IN_PRODUCTION
    LL::Logger& logger;
    std::string t_str{};
    std::array<Position, Exchange::Limits::MAX_TICKERS> positions;

DELETE_DEFAULT_COPY_AND_MOVE(PositionManager)
};
}
