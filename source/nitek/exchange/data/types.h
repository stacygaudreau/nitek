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


namespace Exchange
{
/**
 * @brief Order Matching Engine
 */
namespace OME
{
/*
 * Exchange system limits - can be modified and tuned
 */
constexpr size_t MAX_TICKERS{ 8 };                  // trading instruments supported
constexpr size_t MAX_CLIENT_UPDATES{ 256 * 1024 };  // matching requests & res. queued
constexpr size_t MAX_MARKET_UPDATES{ 256 * 1024 };  // market updates queued to publish
constexpr size_t MAX_N_CLIENTS{ 256 };              // market participants
constexpr size_t MAX_ORDER_IDS{ 1024 * 1024 };      // orders for a single trading instrument
constexpr size_t MAX_PRICE_LEVELS{ 256 };           // depth of price levels in the order book
}

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
    SELL = -1
};
inline std::string side_to_str(Side side) {
    switch (side) {
    case Side::BUY:
        return "BUY";
    case Side::SELL:
        return "SELL";
    case Side::INVALID:
        return "INVALID";
    }
    return "UNKNOWN";
}
}
