/**
 *
 *  Nitek - Client
 *
 *  @file risk_manager.h
 *  @brief Evaluates potential and live risk of trades and orders made
 *  @author Stacy Gaudreau
 *  @date 2024.12.10
 *
 */


#pragma once

#include "llbase/macros.h"
#include "llbase/logging.h"
#include "common/types.h"
#include "nitek/client/trading/om_order.h"
#include "nitek/client/trading/position_manager.h"
#include "common/types.h"
#include <string>
#include <array>


using namespace Common;

namespace Client
{

class OrderManager;

/**
 * @brief Represents the trade risk of a single Position in the trading client. Handles
 * risk calculations based on the Position data.
 */
class Risk {
public:
    enum class Result : int8_t {
        INVALID = 0,
        SIZE_TOO_LARGE = 1,
        POSITION_TOO_LARGE = 2,
        LOSS_TOO_LARGE = 3,
        ALLOWED = 4
    };

    [[nodiscard]] static inline std::string result_to_str(Result result) {
        std::string s{ };
        switch (result) {
            case Result::INVALID:
                s = "INVALID";
                break;
            case Result::SIZE_TOO_LARGE:
                s = "SIZE_TOO_LARGE";
                break;
            case Result::POSITION_TOO_LARGE:
                s = "POSITION_TOO_LARGE";
                break;
            case Result::LOSS_TOO_LARGE:
                s = "LOSS_TOO_LARGE";
                break;
            case Result::ALLOWED:
                s = "ALLOWED";
                break;
        }
        return s;
    }

    Risk() = default;
    Position* position{ nullptr };
    RiskConf conf;

    [[nodiscard]] inline Result get_trade_risk(Side side, Qty qty) const noexcept {
        if (qty > conf.size_max) [[unlikely]]
            return Result::SIZE_TOO_LARGE;
        if (std::abs(position->position + side_to_value(side) * static_cast<int32_t>(qty))
                > static_cast<int32_t>(conf.position_max)) [[unlikely]]
            return Result::POSITION_TOO_LARGE;
        if (position->pnl_total < conf.loss_max) [[unlikely]]
            return Result::LOSS_TOO_LARGE;
        return Result::ALLOWED;
    }

    [[nodiscard]] inline std::string to_str() {
        std::stringstream ss;
        ss << "<Risk>"
           << " ["
           << "position: " << position->to_str()
           << ", config: " << conf.to_str()
           << "]";
        return ss.str();
    }
};

/**
 * @brief Mapping of ticker to Risk data structure for that ticker.
 */
using RiskByTicker = std::array<Risk, Exchange::Limits::MAX_TICKERS>;

/**
 * @brief Manages all Risk objects in the trading client. One Risk object exists for each
 * ticker in the system.
 */
class RiskManager {
public:
    RiskManager(PositionManager& position_manager,
                TradeEngineConfByTicker ticker_confs, LL::Logger& logger )
            : logger(logger) {
        for (TickerID ticker{ }; ticker < risk_by_ticker.size(); ++ticker) {
            risk_by_ticker.at(ticker).position = &position_manager.get_position(ticker);
            risk_by_ticker.at(ticker).conf = ticker_confs.at(ticker).risk_conf;
        }
    }

    [[nodiscard]] inline Risk::Result get_trade_risk(TickerID ticker,
                                                     Side side, Qty qty) const noexcept {
        return risk_by_ticker.at(ticker).get_trade_risk(side, qty);
    }


PRIVATE_IN_PRODUCTION
    LL::Logger& logger;
    RiskByTicker risk_by_ticker;

DELETE_DEFAULT_COPY_AND_MOVE(RiskManager)
};
}