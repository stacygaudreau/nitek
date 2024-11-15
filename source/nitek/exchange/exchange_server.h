/**
 *  
 *  Nitek - Exchange
 *
 *  @file exchange.h
 *  @brief The main server-side exchange application
 *  @author Stacy Gaudreau
 *  @date 2024.11.06
 *
 */

#pragma once


#include <iostream>
#include <string>
#include <memory>
#include <atomic>
#include "llbase/logging.h"
#include "exchange/orders/order_matching_engine.h"
#include "exchange/data/market_data_publisher.h"
#include "exchange/networking/order_gateway_server.h"
#include "exchange/data/types.h"


namespace Exchange
{
class ExchangeServer {
public:
    /**
     * @brief The server-side exchange application which includes all the components
     * needed to provide a low latency market for trading financial instruments.
     * @details At the heart of the Exchange is an order matching engine (OME) which maintains
     * a limit order book and handles matching client orders with one-another. Additionally, an
     * order gateway (OGS), market data server (MDP) and all necessary networking components
     * for the exchange are encapsulated in this module.
     * @param order_iface Network interface for the Order Gateway
     * @param order_port Port for the Order Gateway to bind to
     * @param data_iface Network interface for the UDP Market Data server
     * @param data_incremental_ip Multicast group IP for incremental market data
     * @param data_incremental_port Port for incremental market data
     * @param data_snapshot_ip Multicast group IP for snapshot market data
     * @param data_snapshot_port Port for market data snapshots
     */
    ExchangeServer(const std::string& order_iface, int order_port,
                   const std::string& data_iface, const std::string& data_incremental_ip,
                   int data_incremental_port, const std::string& data_snapshot_ip,
                   int data_snapshot_port);
    ~ExchangeServer();

    /**
     * @brief Start the Exchange Server's worker thread.
     * @details A Unix-style SIGINT will gracefully terminate the running process.
     */
    void start();
    /**
     * @brief The server's main working method.
     */
    void stop();
    /**
     * @brief Stop the server, killing all child threads.
     */
    void run();

private:
    /*
     * Primary exchange modules
     */
    std::unique_ptr<Exchange::OrderMatchingEngine> ome{ nullptr };
    std::unique_ptr<Exchange::MarketDataPublisher> mdp{ nullptr };
    std::unique_ptr<Exchange::OrderGatewayServer> ogs{ nullptr };
    LL::Logger logger{ "exchange_server.log" };
    /*
     * Market data queues
     */
    Exchange::ClientRequestQueue client_requests{ OME::MAX_CLIENT_UPDATES };
    Exchange::ClientResponseQueue client_responses{ OME::MAX_CLIENT_UPDATES };
    Exchange::MarketUpdateQueue market_updates{ OME::MAX_MARKET_UPDATES };
    /*
     * Networking parameters
     */
    std::string order_iface{ "lo" };
    int order_port{ };
    std::string data_iface{ "lo" };
    std::string data_incremental_ip{ };
    int data_incremental_port{ };
    const std::string data_snapshot_ip{ };
    int data_snapshot_port{ };

    // flag to gracefully terminate the server thread via SIGINT
    std::atomic<bool> is_running{ false };
    std::unique_ptr<std::thread> thread{ nullptr };   // tracks the running thread
    static constexpr int T_SLEEP_MS{ 100 }; // time (in ms) the main server thread sleeps for
    std::string t_str{ };

DELETE_DEFAULT_COPY_AND_MOVE(ExchangeServer)

#ifdef IS_TEST_SUITE
public:
    auto get_is_running() { return is_running.load(); }
    auto get_is_OME_running() { return ome->get_is_running(); }
    auto get_is_OGS_running() { return ogs->get_is_running(); }
    auto get_is_MDP_running() { return mdp->get_is_running(); }
#endif
};
}
