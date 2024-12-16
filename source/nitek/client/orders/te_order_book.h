/**
 *
 *  Nitek - Client
 *
 *  @file te_order_book.h
 *  @brief Order book within the Trading Engine client
 *  @author Stacy Gaudreau
 *  @date 2024.12.01
 *
 */


#pragma once

#include "llbase/macros.h"
#include "common/types.h"
#include "llbase/mempool.h"
#include "llbase/logging.h"
#include "client/orders/te_order.h"
#include "exchange/data/ome_market_update.h"


using namespace Common;

namespace Client
{
class TradingEngine;

class TEOrderBook final {
public:
    TEOrderBook(TickerID ticker, LL::Logger& logger);
    ~TEOrderBook();

    /**
     * @brief Update the order book's state by processing a single exchange market update.
     */
    void on_market_update(const Exchange::OMEMarketUpdate &update) noexcept;
    void set_trading_engine(TradingEngine* new_engine);
    void update_bbo(bool should_update_bid, bool should_update_ask) noexcept;
    [[nodiscard]] inline const BBO& get_bbo() const noexcept { return bbo; }

PRIVATE_IN_PRODUCTION
    /**
     * @brief Add an order to the order book.
     */
    void add_order(TEOrder* order);
    /**
     * @brief Remove a given order from the book.
     */
    void remove_order(TEOrder* order);
    /**
     * @brief Clear the book by emptying all its members.
     */
    void clear_entire_book();

    [[nodiscard]] static inline size_t price_to_index(Price price) noexcept {
        return (price % Exchange::Limits::MAX_PRICE_LEVELS);
    }

    [[nodiscard]] TEOrdersAtPrice* get_level_for_price(Price price) const noexcept {
        return map_price_to_price_level.at(price_to_index(price));
    }

    /**
     * @brief Add a price level to the book
     */
    void add_price_level(TEOrdersAtPrice* new_orders_at_price) noexcept;
    /**
     * @brief Remove price level of orders at a given price and side
     */
    void remove_price_level(Side side, Price price) noexcept;


    const TickerID ticker;
    TradingEngine* engine{ nullptr };
    OrderMap id_to_order;
    // for runtime allocation of orders at price levels
    LL::MemPool<TEOrdersAtPrice> orders_at_price_pool{ Exchange::Limits::MAX_PRICE_LEVELS };
    TEOrdersAtPrice* bids_by_price{ nullptr };   // dbly. linked list of sorted bids
    TEOrdersAtPrice* asks_by_price{ nullptr };   // dbly. linked list of sorted asks
    OrdersAtPriceMap map_price_to_price_level{ nullptr };  // mapping of price to its level of
    // orders
    LL::MemPool<TEOrder> order_pool{ Exchange::Limits::MAX_ORDER_IDS };
    BBO bbo;
    std::string t_str{ };
    LL::Logger& logger;

DELETE_DEFAULT_COPY_AND_MOVE(TEOrderBook)
};

/**
 * @brief Mapping of tickers to their TE limit order book
 */
using OrderBookMap = std::array<std::unique_ptr<TEOrderBook>, Exchange::Limits::MAX_TICKERS>;
}