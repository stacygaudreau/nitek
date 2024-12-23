/**
 *
 *  Nitek - Client
 *
 *  @file trading_engine.h
 *  @brief Module which handles running trading algorithms
 *  @author Stacy Gaudreau
 *  @date 2024.12.09
 *
 */

#pragma once

#include "llbase/macros.h"
#include "llbase/logging.h"
#include "common/types.h"
#include "nitek/client/trading/om_order.h"
#include "nitek/client/trading/feature_engine.h"
#include "nitek/client/trading/position_manager.h"
#include "nitek/client/trading/order_manager.h"
#include "nitek/client/trading/risk_manager.h"
#include "nitek/client/trading/market_maker.h"
#include "exchange/data/ome_client_request.h"
#include "exchange/data/ome_client_response.h"
#include "exchange/data/ome_market_update.h"
#include "exchange/orders/ome_order.h"
#include "nitek/client/orders/te_order_book.h"
#include "llbase/timekeeping.h"

#include <string>
#include <sstream>
#include <array>
#include <functional>
#include <memory>

using namespace Common;

namespace Client
{
class MarketMaker;

class TradingEngine {
public:
    TradingEngine(ClientID client_id, TradeAlgo algo,
                  TradeEngineConfByTicker conf_by_ticker,
                  Exchange::ClientRequestQueue& tx_requests,
                  Exchange::ClientResponseQueue& rx_responses,
                  Exchange::MarketUpdateQueue& rx_updates);

    ~TradingEngine() {
        stop();
    }

    /**
     * @brief Start the trading engine thread.
     */
    void start();
    /**
     * @brief Stop the running thread and clean up.
     */
    void stop();
    /**
     * @brief Dispatch a given order request to the exchange (through the Order Gateway Client)
     */
    inline void send_order_request_to_exchange(const Exchange::OMEClientRequest& request) noexcept {
        logger.logf("% <TE::%> send request: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    request.to_str());
        auto req = tx_requests.get_next_to_write();
        *req = request;
        tx_requests.increment_write_index();
    }
    /**
     * @brief Handle changes to the order book. Updates positions/etc and informs trading
     * algorithm.
     */
    inline void on_order_book_update(TickerID ticker, Price price, Side side,
                                     TEOrderBook& ob) noexcept {
        logger.logf("% <TE::%> ticker: %, price: %, side: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    ticker_id_to_str(ticker), price_to_str(price),
                    side_to_str(side));
        auto bbo = ob.get_bbo();
        pman.on_bbo_update(ticker, &bbo);
        feng.on_order_book_update(ticker, price, side, ob);
        on_order_book_update_callback(ticker, price, side, ob);
    }
    /**
     * @brief Handle updates to a trade. Informs trading algorithm and updates trade features.
     */
    inline void on_trade_update(const Exchange::OMEMarketUpdate& update, TEOrderBook& ob) noexcept {
        logger.logf("% <TE::%> trade update: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    update.to_str());
        feng.on_trade_update(update, ob);
        on_trade_update_callback(update, ob);
    }
    /**
     * @brief Handle a response for an order from the Exchange. Updates positioning and informs
     * trading algorithm.
     */
    inline void on_order_response(const Exchange::OMEClientResponse& response) noexcept {
        logger.logf("% <TE::%> response: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    response.to_str());
        if (response.type == Exchange::OMEClientResponse::Type::FILLED) [[unlikely]] {
            pman.add_fill(response);
        }
        on_order_response_callback(response);
    }

    [[nodiscard]] inline ClientID get_client_id() const { return client_id; }

    // cb to forward book updates
    std::function<void(TickerID ticker, Price price,
                       Side side, TEOrderBook& ob)> on_order_book_update_callback;
    // cb to forward trade updates rx'd from exchange
    std::function<void(const Exchange::OMEMarketUpdate& update,
                       TEOrderBook& ob)> on_trade_update_callback;
    // cb to forward order responses rx'd from exchange
    std::function<void(const Exchange::OMEClientResponse& response)> on_order_response_callback;


PRIVATE_IN_PRODUCTION

    /**
     * @brief Main worker thread which runs trading operations.
     */
    void run() noexcept;

    ClientID client_id{ ClientID_INVALID };
    OrderBookMap book_for_ticker;

    Exchange::ClientRequestQueue& tx_requests;      // order requests sent to OGS
    Exchange::ClientResponseQueue& rx_responses;    // order responses received from OGS
    Exchange::MarketUpdateQueue& rx_updates;        // incoming market updates from MDC

    LL::Nanos t_last_rx_event{ };  // time last exchange message received
    volatile bool is_running{ false };
    std::unique_ptr<std::thread> thread{ nullptr };

    std::string t_str{ };
    LL::Logger logger;

    FeatureEngine feng;
    PositionManager pman;
    OrderManager oman;
    RiskManager rman;

    /*
     * Trading algorithms - only one will be instantiated per engine instance
     * NB: should probably change this to use a base class/CRTP without vtables
     */
    std::unique_ptr<MarketMaker> maker_algo{ nullptr };
//    std::unique_ptr<LiquidityTaker> liquidity_algo{ nullptr };


    void default_on_order_book_update_callback(TickerID ticker, Price price,
                                               Side side, TEOrderBook& ob) noexcept {
        logger.logf("% <TE::%> ticker: %, price: %, side: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    ticker_id_to_str(ticker), price_to_str(price), side_to_str(side));
        (void) ob;
    }

    void default_on_trade_update_callback(const Exchange::OMEMarketUpdate& update,
                                          TEOrderBook& ob) noexcept {
        logger.logf("% <TE::%> %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, update.to_str());
        (void) ob;
    }

    void default_on_order_response_callback(const Exchange::OMEClientResponse& response) noexcept {
        logger.logf("% <TE::%> %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, response.to_str());
    }


DELETE_DEFAULT_COPY_AND_MOVE(TradingEngine)
};
}