/**
 *  
 *  Nitek - Exchange
 *
 *  @file ome_order.h
 *  @brief Data structures to represent orders in the Order Matching Engine
 *  @author Stacy Gaudreau
 *  @date 2024.05.08
 *
 */


#pragma once


#include <array>
#include <sstream>
#include "exchange/data/types.h"


namespace Exchange
{

/**
 * @brief A single order represented in the Order Matching Engine.
 */
class OMEOrder {
public:
    OMEOrder() = default;
    OMEOrder(TickerID ticker, ClientID client, OrderID client_order,
             OrderID market_order, Side side, Price price,
             Qty qty, Priority priority, OMEOrder* prev, OMEOrder* next)
            : ticker_id(ticker),
              client_id(client),
              client_order_id(client_order),
              market_order_id(market_order),
              side(side),
              price(price),
              qty(qty),
              priority(priority),
              prev(prev),
              next(next) { }

    TickerID ticker_id{ TickerID_INVALID };       // financial instrument
    ClientID client_id{ ClientID_INVALID };       // market participant
    OrderID client_order_id{ OrderID_INVALID };   // OID the client sent
    OrderID market_order_id{ OrderID_INVALID };   // market OID unique across all clients
    Side side{ Side::INVALID };                   // buy or sell
    Price price{ Price_INVALID };                 // ask or bid price
    Qty qty{ Qty_INVALID };                       // qty still active in the order book
    Priority priority{ Priority_INVALID };        // position in queue wrt same price & side

    OMEOrder* prev{ nullptr };  // prev. order at price level
    OMEOrder* next{ nullptr };  // next order at price level

    std::string to_str() const {
        std::stringstream ss;
        ss << "<OMEOrder>" << "["
           << "ticker: " << ticker_id_to_str(ticker_id)
           << ", client: " << client_id_to_str(client_id)
           << ", oid_client: " << order_id_to_str(client_order_id)
           << ", oid_market: " << order_id_to_str(market_order_id)
           << ", side: " << side_to_str(side)
           << ", price: " << price_to_str(price)
           << ", qty: " << qty_to_str(qty)
           << ", priority: " << priority_to_str(priority)
           << ", prev: "
           << order_id_to_str(prev ? prev->market_order_id : OrderID_INVALID)
           << ", next: "
           << order_id_to_str(next ? next->market_order_id : OrderID_INVALID)
           << "]";

        return ss.str();
    }
};

/**
 * @brief Mapping of OrderIDs -> OMEOrder entries in the matching engine
 */
using OrderMap = std::array<OMEOrder*, OME::MAX_ORDER_IDS>;

/**
 * @brief Mapping for client IDs -> OrderMaps -> OMEOrders
 */
using ClientOrderMap = std::array<OrderMap, OME::MAX_N_CLIENTS>;


/**
 * @brief Contains all OMEOrders listed at the same price, maintaining
 * them in FIFO order priority. Used by the matching engine to determine
 * matching priority when multiple exist at the same price.
 * @details Arranged in a doubly-linked list by the OME in order
 * to sort by price aggressiveness.
 */
class OMEOrdersAtPrice {
public:
    OMEOrdersAtPrice() = default;
    OMEOrdersAtPrice(Side side, Price price, OMEOrder* order_0,
                     OMEOrdersAtPrice* prev, OMEOrdersAtPrice* next)
            : side(side),
              price(price),
              order_0(order_0),
              prev(prev),
              next(next) { }

    Side side{ Side::INVALID };         // buy or sell
    Price price{ Price_INVALID };       // price level
    // orders as linked list, sorted highest -> lowest priority
    OMEOrder* order_0{ nullptr };
    OMEOrdersAtPrice* prev{ nullptr };  // previously aggressive price level
    OMEOrdersAtPrice* next{ nullptr };  // next most aggressive price level

    std::string to_str() const {
        std::stringstream ss;
        ss << "<OMEOrdersAtPrice>["
           << "side: " << side_to_str(side)
           << ", price: " << price_to_str(price)
           << ", order_0: " << (order_0 ? order_0->to_str() : "NULL")
           << ", prev: " << price_to_str(prev ? prev->price : Price_INVALID)
           << ", next: " << price_to_str(next ? next->price : Price_INVALID)
           << "]";
        return ss.str();
    }
};

/**
 * @brief Mapping of price -> OrdersAtPrice
 */
using OrdersAtPriceMap = std::array<OMEOrdersAtPrice*, OME::MAX_PRICE_LEVELS>;
}