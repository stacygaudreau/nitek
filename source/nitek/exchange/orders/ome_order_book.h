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
#include "llbase/logging.h"
#include "llbase/macros.h"
#include "exchange/data/types.h"
#include "exchange/data/ome_client_response.h"
#include "exchange/data/ome_market_update.h"
#include "exchange/orders/ome_order.h"


namespace Exchange
{
class OrderMatchingEngine;


class OMEOrderBook {
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

    void add(ClientID client, OrderID client_order, TickerID ticker, Side side, Price price,
             Qty qty) noexcept;
    void cancel(ClientID client, OrderID order, TickerID ticker) noexcept;

private:
    TickerID ticker{ TickerID_INVALID };    // instrument this orderbook is for
    LL::Logger& logger; // logging instance to write to
    OrderMatchingEngine& ome;   // matching engine which owns this book


DELETE_DEFAULT_COPY_AND_MOVE(OMEOrderBook)
};

/**
 * @brief Mapping of tickers to their order book
 */
using OrderBookMap = std::array<std::unique_ptr<OMEOrderBook>, OME::MAX_TICKERS>;
}
