/**
 *  
 *  Nitek - Exchange
 *
 *  @file order_matching_engine.h
 *  @brief Exchange component which handles matching client orders
 *  @author Stacy Gaudreau
 *  @date 2024.05.08
 *
 */


#pragma once


#include "llbase/macros.h"
#include "llbase/lfqueue.h"
#include "llbase/threading.h"
#include "llbase/logging.h"
#include "exchange/data/ome_client_request.h"
#include "exchange/data/ome_client_response.h"
#include "exchange/data/ome_market_update.h"
#include "exchange/orders/ome_order_book.h"


namespace Exchange
{
class OrderMatchingEngine final {
public:
    /**
     * @brief Primary exchange component which handles matching bid and ask
     * orders from market participants.
     * @details Runs on a dedicated thread. Maintains order books for each
     * supported instrument. Receives and responds to client orders
     * via the Order Gateway, and publishes data by dispatching to
     * the market data publisher.
     * @param rx_requests Queue which client order requests are received on
     * @param tx_responses Queue which responses to client orders are
     * transmitted
     * @param tx_market_updates Queue which market updates are pushed
     * to the publisher through
     */
    OrderMatchingEngine(ClientRequestQueue* rx_requests,
                        ClientResponseQueue* tx_responses,
                        MarketUpdateQueue* tx_market_updates);
    ~OrderMatchingEngine();
    /**
     * @brief Start the matching thread.
     */
    void start();
    /**
     * @brief Stop the matching thread.
     */
    void stop();
    /**
     * @brief Handle a given client request received from the order
     * gateway server.
     */
    void process_client_request(const OMEClientRequest* request) noexcept;
    /**
     * @brief Dispatch a given response to a client to the order
     * gateway server.
     */
    void send_client_response(const OMEClientResponse* response) noexcept;
    /**
     * @brief Dispatch a given market update to the market data
     * publisher.
     */
    void send_market_update(const OMEMarketUpdate* update) noexcept;
    /**
     * @brief Run the main matching engine loop. Processes client
     * requests received from the rx_requests queue.
     */
    void run() noexcept;

    /**
     * @brief True when the matching engine worker thread is running
     */
    inline bool get_is_running() const noexcept { return is_running; };

private:
    // an order book mapped from each ticker
    OrderBookMap order_book_for_ticker;
    // incoming client requests from OGW
    ClientRequestQueue* rx_requests{ nullptr };
    // outgoing responses to OGW
    ClientResponseQueue* tx_responses{ nullptr };
    // outgoing market updates to data publisher
    MarketUpdateQueue* tx_market_updates{ nullptr };
    std::unique_ptr<std::thread> thread{ nullptr };   // tracks the running thread
    volatile bool is_running{ false };  // tracks running thread state
    std::string t_str;
    LL::Logger logger;

DELETE_DEFAULT_COPY_AND_MOVE(OrderMatchingEngine)
};
}
