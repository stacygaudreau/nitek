/**
 *
 *  Nitek - Client
 *
 *  @file order_manager.h
 *  @brief Manages orders on behalf of trading strategies
 *  @author Stacy Gaudreau
 *  @date 2024.12.08
 *
 */


#pragma once

#include "llbase/macros.h"
#include "llbase/logging.h"
#include "common/types.h"
#include "nitek/client/trading/om_order.h"
#include "exchange/data/ome_client_request.h"
#include "exchange/data/ome_client_response.h"
#include "client/trading/risk_manager.h"
#include <string>
#include <sstream>
#include <array>

using namespace Common;

namespace Client
{

class TradingEngine;

class OrderManager {
public:
    /**
     * @brief Manages orders on behalf of trading strategies. Communicates with the RiskManager
     * and TradingEngine to place and modify orders with the exchange.
     */
    OrderManager(TradingEngine& trading_engine, RiskManager& risk_manager, LL::Logger& logger)
            : engine(trading_engine),
              risk_manager(risk_manager),
              logger(logger) {
    }

    /**
     * @brief Request a new order be created at the exchange.
     * @param order The local order object to request an exchange order for
     */
    void request_new_order(OMOrder& order, TickerID ticker, Price price,
                           Side side, Qty qty) noexcept;
    /**
     * @brief Request to cancel an existing order at the exchange
     * @param order The local order object to cancel an exchange order for
     */
    void request_cancel_order(OMOrder& order) noexcept;
    /**
     * @brief Places or replaces the exchange order related to a given local OMOrder object.
     * The arguments given are used to place or replace an exchange order with the desired
     * parameters.
     */
    void manage_order(OMOrder& order, TickerID ticker, Price price, Side side, Qty qty) noexcept {
        using State = OMOrder::State;
        switch (order.state) {
            case State::LIVE: {
                if (order.price != price)
                    // price mismatch; a new order will be created next iteration
                    request_cancel_order(order);
                break;
            }
            case State::INVALID:
            case State::DEAD: {
                if (price != Price_INVALID) {
                    // new order if passed risk check
                    const auto risk = risk_manager.get_trade_risk(ticker, side, qty);
                    if (risk == Risk::Result::ALLOWED) [[likely]] {
                        request_new_order(order, ticker, price, side, qty);
                    } else {
                        logger.logf("% <OM::%> risk check failed for ticker: %, %, qty: %, "
                                    "risk_result: %\n",
                                    LL::get_time_str(&t_str), __FUNCTION__,
                                    ticker_id_to_str(ticker), side_to_str(side),
                                    qty_to_str(qty), Risk::result_to_str(risk));
                    }
                }
                break;
            }
            case State::PENDING_NEW:
            case State::PENDING_CANCEL:
                break;
        }
    }
    /**
     * @brief Manage the orders needed to satisfy the given trade parameters.
     * @param ticker The financial instrument to trade
     * @param bid Bid price
     * @param ask Ask price
     * @param trade_size Number of units to trade
     */
    void manage_orders(TickerID ticker, Price bid, Price ask, Qty trade_size) noexcept {
        auto bid_order = ticker_to_order_by_side.at(ticker).at(side_to_index(Side::BUY));
        manage_order(bid_order, ticker, bid, Side::BUY, trade_size);
        auto ask_order = ticker_to_order_by_side.at(ticker).at(side_to_index(Side::SELL));
        manage_order(ask_order, ticker, ask, Side::SELL, trade_size);
    }

    using Response = Exchange::OMEClientResponse;
    /**
     * @brief Process an incoming order response message from the Exchange.
     */
    void on_order_response(const Response& response) noexcept {
        logger.logf("% <OM::%> %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, response.to_str());
        auto& order = ticker_to_order_by_side.at(response.ticker_id)
                                .at(side_to_index(response.side));
        logger.logf("% <OM::%> %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, order.to_str());
        using Type = Response::Type;
        using State = OMOrder::State;
        switch(response.type) {
            case Type::ACCEPTED:
                order.state = State::LIVE;
                break;
            case Type::CANCELLED:
                order.state = State::DEAD;
                break;
            case Type::FILLED:
                order.qty = response.qty_remain;
                if (order.qty == 0)
                    order.state = State::DEAD;
                break;
            case Type::CANCEL_REJECTED:
            case Type::INVALID:
                break;
        }
    }

    inline OMOrderBySide& get_order_by_side(TickerID ticker) {
        return ticker_to_order_by_side.at(ticker);
    }

PRIVATE_IN_PRODUCTION
    TradingEngine& engine;
    RiskManager& risk_manager;
    std::string t_str{ };
    LL::Logger& logger;
    MapTickerToOMOrdersBySide ticker_to_order_by_side;
    OrderID next_oid{ 1 };

DELETE_DEFAULT_COPY_AND_MOVE(OrderManager)
};
}

