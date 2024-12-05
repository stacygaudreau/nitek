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
#include "nitek/common/types.h"
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
    explicit OMEOrderBook(TickerID assigned_ticker, LL::Logger& logger,
                          OrderMatchingEngine& ome);
    ~OMEOrderBook();

    /**
     * @brief Add a new entry into the limit order book. Matches
     * with any existing orders and places the remainder into
     * the book for future matching.
     * @details Immediately generates an OMEClientResponse and
     * fires it back at the matching engine to notify the client,
     * then attempting to match the order for the request.
     * @param client Client ID making the request
     * @param client_order Client order ID
     * @param ticker Instrument's ticker
     * @param side Buy/sell side
     * @param price Price of the order
     * @param qty Quantity requested
     */
    void add(ClientID client_id, OrderID client_oid, TickerID ticker_id,
             Side side, Price price, Qty qty) noexcept;
    /**
     * @brief Cancel an existing order in the book, if it can
     * be cancelled.
     */
    void cancel(ClientID client_id, OrderID order_id,
                TickerID ticker_id) noexcept;

    /**
     * @brief Return a string rep'n of the order book contents
     */
    std::string to_str(bool is_detailed, bool has_validity_check);

private:
    TickerID assigned_ticker{ TickerID_INVALID };    // instrument this orderbook is for
    LL::Logger& logger; // logging instance to write to
    OrderMatchingEngine& ome;   // matching engine which owns this book

    ClientOrderMap map_client_id_to_order;  // maps client ID -> order ID -> orders

    // for runtime allocation of orders at price levels
    LL::MemPool<OMEOrdersAtPrice> orders_at_price_pool{ Limits::MAX_PRICE_LEVELS };
    OMEOrdersAtPrice* bids_by_price{ nullptr };   // dbly. linked list of sorted bids
    OMEOrdersAtPrice* asks_by_price{ nullptr };   // dbly. linked list of sorted asks

    OrdersAtPriceMap map_price_to_price_level;  // mapping of price to its level of orders

    // low latency runtime allocation of orders
    LL::MemPool<OMEOrder> order_pool{ Limits::MAX_ORDER_IDS };

    OMEClientResponse client_response;  // latest client order response message
    OMEMarketUpdate market_update;      // latest market update message
    OrderID next_market_oid{ 1 };       // next market order ID to assign
    std::string t_str;

    /**
     * @brief Find a partial or complete match for the given order.
     * @details Only *finds* a match. Does not actually execute
     * matching the order.
     * @return Zero when fully matched, else the remaining quantity
     * in the order after matching. If there is no match, the
     * original full order qty is returned.
     */
    Qty find_match(ClientID client_id, OrderID client_oid,
                   TickerID ticker_id, Side side, Price price,
                   Qty qty, OrderID new_market_oid) noexcept;
    /**
     * @brief Execute the matching of a given order.
     * @param order_matched Pointer to the order which is
     * being matched to by the given order.
     * @param qty_remains Pointer to remaining qty in calling
     * fn. This value is modified, to avoid reallocation.
     */
    void match(TickerID ticker_id, ClientID client_id, Side side,
               OrderID client_order_id, OrderID new_market_oid,
               OMEOrder* order_matched, Qty* qty_remains) noexcept;
    /**
     * @brief Get a new market OrderID in the sequence.
     */
    inline OrderID get_new_market_order_id() noexcept {
        return next_market_oid++;
    }
    /**
     * @brief Add a price level to the order book
     */
    void add_price_level(OMEOrdersAtPrice* new_orders_at_price) noexcept;
    /**
     * @brief Remove price level of orders at a given price and side.
     */
    void remove_price_level(Side side, Price price) noexcept;
    /**
     * @brief Get the next priority in a given price level
     */
    inline Priority get_next_priority(Price price) noexcept {
        // return 1 if a priority is not yet there, else
        //  we simply return the next priority level (+1)
        const auto orders_at_price = get_level_for_price(price);
        if (!orders_at_price)
            return 1ul;
        return orders_at_price->order_0->prev->priority + 1ul;
    }
    /**
    * @brief Hashes a price into an index suitable for
    * mapping to price levels
    */
    inline static size_t price_to_index(Price price) noexcept {
        return (price % Limits::MAX_PRICE_LEVELS);
    };
    /**
    * @brief Get OrdersAtPrice for a given price level
    */
    [[nodiscard]] inline OMEOrdersAtPrice* get_level_for_price(Price price)
    const noexcept {
        return map_price_to_price_level.at(price_to_index(price));
    }
    /**
     * @brief Adds a given order to the limit order book
     */
    void add_order_to_book(OMEOrder* order) noexcept;
    /**
     * @brief Removes a given order from the limit order book
     */
    void remove_order_from_book(OMEOrder* order) noexcept;

DELETE_DEFAULT_COPY_AND_MOVE(OMEOrderBook)

#ifdef IS_TEST_SUITE
public:
    inline auto add_order_to_book_test(OMEOrder* o) noexcept {
        add_order_to_book(o);
    }
    inline auto remove_order_from_book_test(OMEOrder* o) noexcept {
        remove_order_from_book(o);
    }
    inline auto get_client_response() noexcept {
        return &client_response;
    }
    inline auto get_market_update() noexcept {
        return &market_update;
    }
    inline auto get_level_for_price_test(Price price) {
        return get_level_for_price(price);
    }
    inline auto add_price_level_test(OMEOrdersAtPrice* level) noexcept {
        add_price_level(level);
    }
    inline auto remove_price_level_test(Side side, Price price) noexcept {
        remove_price_level(side, price);
    }
    inline auto get_bid_levels_by_price() noexcept {
        return bids_by_price;
    }
    inline auto get_ask_levels_by_price() noexcept {
        return asks_by_price;
    }
    inline auto& get_price_levels_mempool() noexcept {
        return orders_at_price_pool;
    }
    inline auto& get_orders_mempool() noexcept {
        return order_pool;
    }
    inline Qty find_match_test(ClientID client_id, OrderID client_oid,
                               TickerID ticker_id, Side side, Price price,
                               Qty qty, OrderID new_market_oid) noexcept {
        return find_match(client_id, client_oid, ticker_id,
                          side, price, qty, new_market_oid);
    }
    inline auto match_test(TickerID ticker_id, ClientID client_id, Side side,
                           OrderID client_oid, OrderID new_market_oid,
                           OMEOrder* order_matched, Qty* qty_remains) noexcept {
        match(ticker_id, client_id, side, client_oid, new_market_oid,
              order_matched, qty_remains);
    }
#endif
};

/**
 * @brief Mapping of tickers to their limit order book
 */
using OrderBookMap = std::array<std::unique_ptr<OMEOrderBook>, Limits::MAX_TICKERS>;
}
