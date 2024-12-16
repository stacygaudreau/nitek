/**
 *
 *  Nitek - Client
 *
 *  @file om_order.h
 *  @brief Orders within the Order Manager module
 *  @author Stacy Gaudreau
 *  @date 2024.12.08
 *
 */

#pragma once

#include <array>
#include <string>
#include <sstream>
#include "nitek/common/types.h"

using namespace Common;

namespace Client
{

struct OMOrder {
    enum class State: int8_t {
        INVALID = 0,
        PENDING_NEW = 1,        // new order send but not accepted by exchange yet
        LIVE = 2,               // order accepted at exchange
        PENDING_CANCEL = 3,     // cancellation sent but not confirmed yet
        DEAD = 4
    };

    TickerID ticker{ TickerID_INVALID };
    OrderID id{ OrderID_INVALID };
    Side side{ Side::INVALID };
    Price price{ Price_INVALID };
    Qty qty{ Qty_INVALID };
    State state{ State::INVALID };

    bool operator==(OMOrder const&) const = default;

    [[nodiscard]] inline static std::string state_to_str(State state) {
        switch (state) {
            case State::INVALID:
                return "INVALID";
            case State::PENDING_NEW:
                return "PENDING_NEW";
            case State::LIVE:
                return "LIVE";
            case State::PENDING_CANCEL:
                return "PENDING_CANCEL";
            case State::DEAD:
                return "DEAD";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] std::string to_str() const {
        std::stringstream ss;
        ss << "<OMOrder>" << "["
           << "ticker: " << ticker_id_to_str(ticker)
           << ", id: " << order_id_to_str(id)
           << ", side: " << side_to_str(side)
           << ", price: " << price_to_str(price)
           << ", qty: " << qty_to_str(qty)
           << ", state: " << state_to_str(state)
           << "]";
        return ss.str();
    }
};

/**
 * @brief Mapping of Sides to OMOrders
 */
using OMOrderBySide = std::array<OMOrder, side_to_index(Side::MAX) + 1>;

/**
 * @brief An OMOrderBySide mapping large enough for every ticker in the system
 */
using MapTickerToOMOrdersBySide = std::array<OMOrderBySide, Exchange::Limits::MAX_TICKERS>;
}
