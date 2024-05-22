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
    explicit OMEOrderBook(TickerID ticker, LL::Logger& logger, OrderMatchingEngine& ome);
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
    void add(ClientID client, OrderID client_order, TickerID ticker, Side side, Price price,
             Qty qty) noexcept;
    void cancel(ClientID client, OrderID order, TickerID ticker) noexcept;

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
    TickerID ticker{ TickerID_INVALID };    // instrument this orderbook is for
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

DELETE_DEFAULT_COPY_AND_MOVE(OMEOrderBook)
};

/**
 * @brief Mapping of tickers to their limit order book
 */
using OrderBookMap = std::array<std::unique_ptr<OMEOrderBook>, OME::MAX_TICKERS>;
}
