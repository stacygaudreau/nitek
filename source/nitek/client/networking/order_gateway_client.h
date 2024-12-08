/**
 *
 *  Nitek - Client
 *
 *  @file order_gateway_client.h
 *  @brief Client gateway module for sending and receiving order messages from the Exchange
 *  @author Stacy Gaudreau
 *  @date 2024.12.05
 *
 */


#pragma once

#include <functional>
#include "llbase/macros.h"
#include "nitek/common/types.h"
#include "llbase/threading.h"
#include "llbase/tcp_server.h"
#include "llbase/logging.h"
#include "exchange/data/ome_client_request.h"
#include "exchange/data/ome_client_response.h"

using namespace Common;

namespace Client
{
class OrderGatewayClient {
public:
    /**
     * @brief An order gateway client which connects to the Exchange over TCP in order to send and
     * receive order requests and confirmations, as well as receiving and responding to order
     * requests from the Trading Engine component.
     * @param client ID of the client Trading Engine
     * @param rx_requests Requests for orders incoming from the Trading Engine
     * @param tx_responses Order responses outgoing to the Trading Engine
     * @param ip IP address of the order gateway server to connect to
     * @param iface Interface to bind to for exchange order server connection
     * @param port TCP port to connect to the order gateway server through
     */
    OrderGatewayClient(ClientID client, Exchange::ClientRequestQueue& rx_requests,
                       Exchange::ClientResponseQueue& tx_responses, std::string ip,
                       const std::string& iface, int port);
    ~OrderGatewayClient();

    /**
     * @brief Start the order client thread.
     */
    void start();
    /**
     * @brief Stop the order client thread.
     */
    void stop();


PRIVATE_IN_PRODUCTION
    const ClientID client_id;
    Exchange::ClientRequestQueue& rx_requests;      // incoming requests <= trading engine
    Exchange::ClientResponseQueue& tx_responses;    // responses outgoing => trading engine
    std::string ip;
    const std::string iface;
    const int port{ 0 };
    LL::Logger logger;
    volatile bool is_running{ false };
    std::unique_ptr<std::thread> thread{ nullptr };
    std::string t_str{ };
    // tracks the sequence number for the next outgoing ClientRequest
    size_t n_seq_next_request{ 1 };
    // verifies the sequence of ClientResponse messages rx'd from exchange
    size_t n_seq_next_expected{ 1 };
    LL::TCPSocket tcp_socket;   // connection with exchange order gateway


PRIVATE_IN_PRODUCTION
    /**
     * @brief The gateway thread's main working method.
     */
    void run() noexcept;
    /**
     * @brief Called when order data received on the socket from the exchange gateway.
     */
    void rx_callback(LL::TCPSocket* socket, LL::Nanos t_rx) noexcept;


DELETE_DEFAULT_COPY_AND_MOVE(OrderGatewayClient)
};
}