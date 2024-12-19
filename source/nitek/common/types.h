/**
 *  
 *  Nitek - Exchange
 *
 *  @file types.h
 *  @brief Various types and constants common to exchange and client modules.
 *  @author Stacy Gaudreau
 *  @date 2024.05.08
 *
 */


#pragma once


#include <cstdint>
#include <limits>
#include <cstddef>
#include <string>
#include <sstream>


// comment this out for release build
#ifndef IS_TEST_SUITE
#define IS_TEST_SUITE
#endif

namespace Exchange
{
/**
 * @brief Consts which set limits of the exchange mechanics
 */
namespace Limits
{
/*
 * Exchange system limits - can be modified and tuned
 * NB: we use different (smaller) limits for the test suite since
 * it takes a non-trivial amount of time to allocate/deallocate
 * for each unit test otherwise
 */
#ifdef IS_TEST_SUITE
constexpr size_t OME_SIZE{ 16 };
#else
constexpr size_t OME_SIZE{ 256 };
#endif
constexpr size_t MAX_TICKERS{ 8 };                  // trading instruments supported
constexpr size_t MAX_CLIENT_UPDATES{ OME_SIZE * 1024 };  // matching requests & res. queued
constexpr size_t MAX_MARKET_UPDATES{ OME_SIZE * 1024 };  // market updates queued to publish
constexpr size_t MAX_N_CLIENTS{ OME_SIZE };              // market participants
constexpr size_t MAX_ORDER_IDS{ 1024 * 1024 };      // orders for a single trading instrument
constexpr size_t MAX_PRICE_LEVELS{ OME_SIZE };           // depth of price levels in the order book
constexpr size_t MAX_PENDING_ORDER_REQUESTS{ 1024 }; // max pending req's on order gateway socket
}
}


namespace Common {
/**
 * @brief Const template for numeric aliases derived from
 * any type compatible with std::numeric_limits::max()
 */
template<typename T>
constexpr T ID_INVALID = std::numeric_limits<T>::max();
/**
 * @brief Convert a numeric literal to string
 * @tparam T Type of numeric to convert
 * @return String representation or "INVALID" if it's out of range
 */
template<typename T>
inline std::string numeric_to_str(T id) {
    if (id == ID_INVALID<T>) [[unlikely]] {
        return "INVALID";
    }
    return std::to_string(id);
}

/**
 * @brief Identifies unique orders
 */
using OrderID = uint64_t;
constexpr auto OrderID_INVALID = ID_INVALID<OrderID>;
inline std::string order_id_to_str(OrderID id) {
    return numeric_to_str(id);
}

/**
 * @brief Unique ID for a product's ticker
 */
using TickerID = uint32_t;
constexpr auto TickerID_INVALID = ID_INVALID<TickerID>;
inline std::string ticker_id_to_str(TickerID id) {
    return numeric_to_str(id);
}

/**
 * @brief Identifies a client in the exchange
 */
using ClientID = uint32_t;
constexpr auto ClientID_INVALID = ID_INVALID<ClientID>;
inline std::string client_id_to_str(ClientID id) {
    return numeric_to_str(id);
}

/**
 * @brief Price
 */
using Price = int64_t;
constexpr auto Price_INVALID = ID_INVALID<Price>;
inline std::string price_to_str(Price price) {
    return numeric_to_str(price);
}

/**
 * @brief Quantity
 */
using Qty = uint32_t;
constexpr auto Qty_INVALID = ID_INVALID<Qty>;
inline std::string qty_to_str(ClientID qty) {
    return numeric_to_str(qty);
}

/**
 * @brief Position of an order at a price level in the
 * FIFO matching queue
 */
using Priority = uint64_t;
constexpr auto Priority_INVALID = ID_INVALID<Priority>;
inline std::string priority_to_str(OrderID priority) {
    return numeric_to_str(priority);
}

/**
 * @brief Which side of a trade the order is on
 * @details a native-sized (eg: 64bit) int might run faster on
 * modern CPUs in terms of instructions to do arithmetic on it,
 * but in this case we are looking to optimise bit-packing since
 * the data will be travelling down the wire throughout the
 * entire system
 */
enum class Side : int8_t {
    INVALID = 0,
    BUY = 1,
    SELL = -1,
    MAX = 2
};
inline std::string side_to_str(Side side) {
    switch (side) {
    case Side::BUY:
        return "BUY";
    case Side::SELL:
        return "SELL";
    case Side::INVALID:
        return "INVALID";
    case Side::MAX:
        return "MAX";
    }
    return "UNKNOWN";
}
/**
 * @brief Convert a Side to an index which can be used to index an array.
 */
inline constexpr size_t side_to_index(Side side) noexcept {
    return static_cast<size_t>(side) + 1;
}
/**
 * @brief Convert a Side into a value which is helpful for fast
 * computation of PnL and other metrics.
 * @returns 1 for BUY and -1 for SELL
 */
inline constexpr int side_to_value(Side side) noexcept {
    return static_cast<int>(side);
}

/**
 * @brief Configuration containing trade risk parameters.
 */
struct RiskConf {
    Qty size_max{ };        // max order size to be sent
    Qty position_max{ };    // max position sizing
    double loss_max{ };     // total loss allowed

    [[nodiscard]] inline std::string to_str() {
        std::stringstream ss;
        ss << "<RiskConf>"
           << " ["
           << "size_max: " << qty_to_str(size_max)
           << ", position_max: " << qty_to_str(position_max)
           << ", loss_max: " << loss_max
           << "]";
        return ss.str();
    }
};

/**
 * @brief Configuration for a TradingEngine's high level trading parameters.
 */
struct TradingEngineConf {
    Qty trade_size{ };      // sizing for each order
    double threshold{ };    // used by strategy for making feature decisions
    RiskConf risk_conf;     // configuration for trading risk

    [[nodiscard]] inline std::string to_str() {
        std::stringstream ss;
        ss << "<TradingEngineConf>"
           << " ["
           << "trade_size: " << qty_to_str(trade_size)
           << ", threshold: " << threshold
           << ", risk: " << risk_conf.to_str()
           << "]";
        return ss.str();
    }
};

/**
 * @brief Mapping of ticker to TradeEngine configuration.
 */
using TradeEngineConfByTicker = std::array<TradingEngineConf, Exchange::Limits::MAX_TICKERS>;

/**
 * @brief Type of trading algorithm.
 */
enum class TradeAlgo: int8_t {
    INVALID = 0,
    RANDOM = 1,
    MARKET_MAKER = 2,
    LIQ_TAKER  = 3,
    MAX = 4
};
inline std::string trade_algo_to_str(TradeAlgo algo) {
    switch (algo) {
        case TradeAlgo::RANDOM:
            return "RANDOM";
        case TradeAlgo::MARKET_MAKER:
            return "MARKET_MAKER";
        case TradeAlgo::LIQ_TAKER:
            return "LIQUIDITY_TAKER";
        case TradeAlgo::INVALID:
            return "INVALID";
        case TradeAlgo::MAX:
            return "MAX";
    }
    return "UNKNOWN";
}
inline TradeAlgo str_to_trade_algo(const std::string &s) {
    for (auto i = static_cast<int>(TradeAlgo::INVALID);
            i <= static_cast<int>(TradeAlgo::MAX); ++i) {
        const auto algo = static_cast<TradeAlgo>(i);
        if (trade_algo_to_str(algo) == s)
            return algo;
    }
    return TradeAlgo::INVALID;
}

}
