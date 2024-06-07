/**
 *  
 *  Nitek - Exchange
 *
 *  @file order_gateway_server.h
 *  @brief Gateway server for processing client order requests and responses
 *  over the network
 *  @author Stacy Gaudreau
 *  @date 2024.05.31
 *
 */


#pragma once


#include <functional>
#include <string>
#include <array>
#include "llbase/threading.h"
#include "llbase/macros.h"
#include "llbase/tcp_server.h"
#include "llbase/tcp_socket.h"
#include "llbase/timekeeping.h"
#include "exchange/networking/fifo_sequencer.h"
#include "exchange/data/ome_client_response.h"
#include "exchange/data/ome_client_request.h"


namespace Exchange
{
class OrderGatewayServer {
public:
    /**
     * @brief An exchange order server which acts as a gateway
     * to process client order requests and respond with
     * order messages from the matching engine.
     * @param tx_requests Queue for passing order requests to
     * the Order Matching Engine on behalf of participants
     * @param rx_responses Order responses received from the
     * matching engine are received on this queue and sent
     * to the corresponding exchange client
     * @param iface Network interface name to bind to
     * @param port Port the interface will listen on
     */
    OrderGatewayServer(ClientRequestQueue& tx_requests,
                       ClientResponseQueue& rx_responses,
                       const std::string& iface, int port);
    ~OrderGatewayServer();

    /**
     * @brief Start the order server thread.
     */
    void start();
    /**
     * @brief Stop the order server thread.
     */
    void stop();
    /**
     * @brief The server thread's main working method.
     */
    void run();

    void rx_callback(LL::TCPSocket* socket, LL::Nanos t_rx) noexcept {
        logger.logf("% <OGS::%> rx at socket: %, len: %, t: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    socket->fd, socket->i_rx_next, t_rx);

        // available rx data should be at least one client request in size
        // -> break it up into appropriate chunks and iterate
        if (socket->i_rx_next >= sizeof(OGSClientRequest)) {
            size_t i{ };
            for (; i + sizeof(OGSClientRequest) <= socket->i_rx_next;
                   i += sizeof(OGSClientRequest)) {
                auto req = reinterpret_cast<const OGSClientRequest*>(
                        socket->rx_buffer.data() + i);
                logger.logf("% <OGS::%> req: %\n",
                            LL::get_time_str(&t_str),
                            __FUNCTION__, req->to_str());

                // client's first order req; start tracking with a new socket mapping
                if (map_client_to_socket[req->ome_request.client_id] == nullptr) [[unlikely]] {
                    map_client_to_socket[req->ome_request.client_id] = socket;
                }

                // current req socket does not match the mapped one
                // -> log error and skip the order request
                if (map_client_to_socket[req->ome_request.client_id] != socket) {
                    // todo: this should send a response back to client
                    logger.logf("% <OGS::%> rx'd req from client: %"
                                " on socket: %! expected: %\n",
                                LL::get_time_str(&t_str),
                                __FUNCTION__,
                                req->ome_request.client_id, socket->fd,
                                map_client_to_socket[req->ome_request.client_id]->fd);
                    continue;
                }

                // sanity check for sequence number
                // if seq mismatch -> log error and ignore order request
                auto& n_seq_rx_next =
                        map_client_to_rx_n_seq[req->ome_request.client_id];
                if (req->n_seq != n_seq_rx_next) {
                    // todo: this should send a rejection back to client
                    logger.logf("% <OGS::%> seq number error! client: %, n_seq expected: % "
                                " but received: %\n", LL::get_time_str(&t_str),
                                __FUNCTION__,
                                req->ome_request.client_id, n_seq_rx_next, req->n_seq);
                    continue;
                }

                // increment client seq number and fwd order to the
                // exchange FIFO sequencer
                ++n_seq_rx_next;
                fifo.push_client_request(req->ome_request, t_rx);
            }
            memcpy(socket->rx_buffer.data(), socket->rx_buffer.data() + i,
                   socket->i_rx_next - i);
            socket->i_rx_next -= i;
        }
    }

    /**
     * @brief Publishes all pending order requests to the exchange
     */
    void rx_done_callback() noexcept {
        fifo.sequence_and_publish();
    };

private:
    const std::string iface;
    const int port{ 0 };
    ClientResponseQueue& rx_responses;  // order responses received from OME to send to clients
    volatile bool is_running{ false };
    std::unique_ptr<std::thread> thread{ nullptr };   // tracks the running thread
    std::string t_str;
    LL::Logger logger;
    LL::TCPServer server;   // server instance manages client connections
    // FIFO queue for processing client orderreq's in the proper sequence
    FIFOSequencer fifo;
    // map client ID -> next sequence number for that client's outgoing
    // response message
    std::array<size_t, OME::MAX_N_CLIENTS> map_client_to_tx_n_seq;
    // map client ID -> next _incoming_ sequence number expected
    // to be received from a client
    std::array<size_t, OME::MAX_N_CLIENTS> map_client_to_rx_n_seq;
    // mapping of client to their connection's corresponding TCPSocket
    std::array<LL::TCPSocket*, OME::MAX_N_CLIENTS> map_client_to_socket;

DELETE_DEFAULT_COPY_AND_MOVE(OrderGatewayServer)

#ifdef IS_TEST_SUITE
public:
    auto get_is_running() { return is_running; }
#endif
};
}

