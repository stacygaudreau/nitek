/**
 *  
 *  Nitek - Exchange
 *
 *  @file ome_order_book.h
 *  @brief Order book within the Order Matching Engine
 *  @author Stacy Gaudreau
 *  @date 2024.05.08
 *
 */


#pragma once


#include <array>
#include <memory>
#include <sstream>
#include "llbase/logging.h"
#include "llbase/macros.h"
#include "llbase/mempool.h"
#include "exchange/data/types.h"
#include "exchange/data/ome_client_response.h"
#include "exchange/data/ome_market_update.h"
#include "exchange/orders/ome_order.h"


namespace Exchange
{
class OrderMatchingEngine;


class OMEOrderBook final {
public:
    /**
     * @brief A limit order book which maintains and matches bids and asks
     * for a single financial instrument/ticker.
     * @param ticker Financial instrument ID
     * @param logger Logging instance to write to
     * @param ome Parent Order Matching Engine instance the book belongs to
     */
    explicit OMEOrderBook(TickerID assigned_ticker, LL::Logger& logger, OrderMatchingEngine& ome);
    ~OMEOrderBook();

    /**
     * @brief Enter a new order into the limit order book.
     * @details Immediately generates an OMEClientResponse and fires it back at the
     * matching engine to notify the client, then attempting to match the order for
     * the request.
     * @param client Client ID making the request
     * @param client_order Client order ID
     * @param ticker Instrument's ticker
     * @param side Buy/sell side
     * @param price Price of the order
     * @param qty Quantity requested
     */
    void add(ClientID client_id, OrderID client_oid, TickerID ticker_id, Side side, Price price,
             Qty qty) noexcept;
    /**
     * @brief Cancel an existing order in the book, if it can
     * be cancelled.
     */
    void cancel(ClientID client_id, OrderID order_id, TickerID ticker_id) noexcept;

    /**
     * @brief Return a string rep'n of the order book contents
     */
    std::string to_str(bool is_detailed, bool has_validity_check);

    inline OMEClientResponse* get_client_response() noexcept {
        return &client_response;
    }
    inline OMEMarketUpdate* get_market_update() noexcept {
        return &market_update;
    }

private:
    TickerID assigned_ticker{ TickerID_INVALID };    // instrument this orderbook is for
    LL::Logger& logger; // logging instance to write to
    OrderMatchingEngine& ome;   // matching engine which owns this book

    ClientOrderMap map_client_id_to_order;  // maps client ID -> order ID -> orders

    // for runtime allocation of orders at price levels
    LL::MemPool<OMEOrdersAtPrice> orders_at_price_pool{ OME::MAX_PRICE_LEVELS };
    OMEOrdersAtPrice* bids_by_price{ nullptr };   // dbly. linked list of sorted bids
    OMEOrdersAtPrice* asks_by_price{ nullptr };   // dbly. linked list of sorted asks

    OrdersAtPriceMap map_price_to_orders_at_price;  // mapping of price level to its orders

    // low latency runtime allocation of orders
    LL::MemPool<OMEOrder> order_pool{ OME::MAX_ORDER_IDS };

    OMEClientResponse client_response;  // latest client order response message
    OMEMarketUpdate market_update;      // latest market update message
    OrderID next_market_oid{ 1 };       // next market order ID to assign
    std::string t_str;

    /**
     * @brief Find a partial or complete match for the given order.
     * @return Zero when fully matched, else the remaining quantity
     * in the order after matching. If there is no match, the
     * original full order qty is returned.
     */
    Qty find_match(ClientID client_id, OrderID client_order_id,
                   TickerID ticker_id, Side side, Price price,
                   Qty qty, OrderID new_market_order_id) noexcept;

    /**
     * @brief Get a new market OrderID in the sequence.
     */
    inline OrderID get_new_market_order_id() noexcept {
        return next_market_oid++;
    }
    /**
     * @brief Convert a price into an index suitable for
     * mapping to price levels
     */
    inline static auto price_to_index(Price price) noexcept {
        return (price % OME::MAX_PRICE_LEVELS);
    }
    /**
     * @brief Get OrdersAtPrice for a given price level
     */
    inline OMEOrdersAtPrice* get_orders_at_price(Price price) const noexcept {
        return map_price_to_orders_at_price.at(price_to_index(price));
    }
    /**
     * @brief Add new orders at a given price level to the order book
     */
    void add_orders_at_price(OMEOrdersAtPrice* new_orders_at_price) noexcept;
    /**
     * @brief Remove orders at a given price level and side from
     * the order book
     */
    void remove_orders_at_price(Side side, Price price) noexcept;
    /**
     * @brief Get the next priority in a given price level
     */
    inline Priority get_next_priority(Price price) noexcept {
        // return 1 if a priority is not yet there, else
        //  we simply return the next priority level (+1)
        const auto orders_at_price = get_orders_at_price(price);
        if (!orders_at_price)
            return 1ul;
        return orders_at_price->order_0->prev->priority + 1ul;
    }
    /**
     * @brief Adds a given order to the limit order book
     */
    inline void add_order_to_book(OMEOrder* order) noexcept;
    /**
     * @brief Removes a given order from the limit order book
     */
    inline void remove_order_from_book(OMEOrder* order) noexcept;

DELETE_DEFAULT_COPY_AND_MOVE(OMEOrderBook)
};

/**
 * @brief Mapping of tickers to their limit order book
 */
using OrderBookMap = std::array<std::unique_ptr<OMEOrderBook>, OME::MAX_TICKERS>;
}
