/**
 *
 *  Nitek - Client
 *
 *  @file te_order.h
 *  @brief Orders in the Trading Engine (TE) client
 *  @author Stacy Gaudreau
 *  @date 2024.12.01
 *
 */


#pragma once


#include <array>
#include <sstream>
#include "nitek/common/types.h"


using namespace Common;

namespace Client
{
class TEOrder {
public:
    /**
     * @brief Represents a single order for use in the client-side Trading Engine.
     */
    TEOrder(OrderID id, Side side, Price price, Qty qty, Priority priority, TEOrder* prev,
            TEOrder* next)
            : id(id),
              side(side),
              price(price),
              qty(qty),
              priority(priority),
              prev(prev),
              next(next) { }

    TEOrder() = default;

    OrderID id{ OrderID_INVALID };
    Side side{ Side::INVALID };
    Price price{ Price_INVALID };
    Qty qty{ Qty_INVALID };
    Priority priority{ Priority_INVALID };
    TEOrder* prev{ nullptr };   // previous order at price level
    TEOrder* next{ nullptr };   // next order at price level

    [[nodiscard]] std::string to_str() const {
        std::stringstream ss;
        ss << "<TEOrder>" << "["
           << "id: " << order_id_to_str(id)
           << ", side: " << side_to_str(side)
           << ", price: " << price_to_str(price)
           << ", qty: " << qty_to_str(qty)
           << ", priority: " << priority_to_str(priority)
           << ", prev: "
           << order_id_to_str(prev ? prev->id : OrderID_INVALID)
           << ", next: "
           << order_id_to_str(next ? next->id : OrderID_INVALID)
           << "]";

        return ss.str();
    }
};

/**
 * @brief Mapping of OrderIDs -> TEOrder entries in the Trading Engine
 */
using OrderMap = std::array<TEOrder*, Exchange::Limits::MAX_ORDER_IDS>;


/**
 * @brief A price level which contains all TEOrders listed at the same price,
 * maintaining them in FIFO order priority.
 * @details Arranged in a doubly-linked list by the Trading Engine in order
 * to sort by price aggressiveness.
 */
class TEOrdersAtPrice {
public:
    TEOrdersAtPrice() = default;

    TEOrdersAtPrice(Side side, Price price, TEOrder* order_0,
                    TEOrdersAtPrice* prev, TEOrdersAtPrice* next)
            : side(side),
              price(price),
              order_0(order_0),
              prev(prev),
              next(next) { }

    bool operator==(TEOrdersAtPrice const&) const = default;

    Side side{ Side::INVALID };         // buy or sell
    Price price{ Price_INVALID };       // price level
    // orders as linked list, sorted highest -> lowest priority
    TEOrder* order_0{ nullptr };
    TEOrdersAtPrice* prev{ nullptr };  // previously aggressive price level
    TEOrdersAtPrice* next{ nullptr };  // next most aggressive price level

    [[nodiscard]] std::string to_str() const {
        std::stringstream ss;
        ss << "<TEOrdersAtPrice>["
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
using OrdersAtPriceMap = std::array<TEOrdersAtPrice*, Exchange::Limits::MAX_PRICE_LEVELS>;


/**
 * @brief Best Bid Offer (BBO) expresses the total qty available at the best bid and ask prices.
 * @details This is used by some Trading Engine modules which do not need an entire picture of
 * the order book, and only need the most important price information.
 */
struct BBO {
    Price bid{ Price_INVALID };
    Price ask{ Price_INVALID };
    Qty bid_qty{ Qty_INVALID };
    Qty ask_qty{ Qty_INVALID };

    [[nodiscard]] std::string to_str() const {
        std::stringstream ss;
        ss << "<BBO>["
           << qty_to_str(bid_qty) << "@" << price_to_str(bid)
           << " x "
           << qty_to_str(ask_qty) << "@" << price_to_str(ask)
           << "]";
        return ss.str();
    }
};
}
