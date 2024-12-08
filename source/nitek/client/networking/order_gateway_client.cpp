#include "order_gateway_client.h"

namespace Client
{
OrderGatewayClient::OrderGatewayClient(ClientID client,
                                       Exchange::ClientRequestQueue& rx_requests,
                                       Exchange::ClientResponseQueue& tx_responses,
                                       std::string ip,
                                       const std::string& iface,
                                       int port)
        : client_id(client),
          rx_requests(rx_requests),
          tx_responses(tx_responses),
          ip(ip),
          iface(iface),
          port(port),
          logger("client_order_gateway_" + std::to_string(client) + ".log"),
          tcp_socket(logger) {
    tcp_socket.rx_callback = [this](auto socket, auto t_rx) { rx_callback(socket, t_rx); };
}

OrderGatewayClient::~OrderGatewayClient() {
    stop();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);
}

void OrderGatewayClient::start() {
    is_running = true;
    // establish connection to exchange order server and start worker thread
    auto fd = tcp_socket.connect(ip, iface, port, false);
    ASSERT(fd >= 0, "<OGC> failed to create gateway socket at: " + ip + ":" + std::to_string(port)
                    + " at iface: " + iface + ", error: " + std::string(std::strerror(errno)));
    thread = LL::create_and_start_thread(-1, "OrderGatewayClient",[this]() { run(); });
    ASSERT(thread != nullptr, "<OGC> failed to start thread for OrderGatewayClient");
}

void OrderGatewayClient::stop() {
    is_running = false;
    if (thread != nullptr && thread->joinable())
        thread->join();
}

void OrderGatewayClient::run() noexcept {
    logger.logf("% <OGC::%> running order gateway client...\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    while (is_running) {
        // receive order confirmations and transmit any outgoing TCP data
        tcp_socket.tx_and_rx();
        // process order requests received from the Trading Engine and push them to the
        // exchange order server over TCP
        for(auto request = rx_requests.get_next_to_read();
            request; request = rx_requests.get_next_to_read()) {
            logger.logf("% <OGC::%> tx request, client: %, n_seq: %, req: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__, client_id,
                        n_seq_next_request, request->to_str());
            tcp_socket.load_tx(&n_seq_next_request, sizeof(n_seq_next_request));
            tcp_socket.load_tx(request, sizeof(Exchange::OMEClientRequest));
            rx_requests.increment_read_index();
            n_seq_next_request++;
        }
    }
}

void OrderGatewayClient::rx_callback(LL::TCPSocket* socket, LL::Nanos t_rx) noexcept {
    using OrderResponse = Exchange::OGSClientResponse;
    // order response data is received and processed from the TCP socket
    logger.logf("% <OGC::%> rx at socket fd: %, len: %, t: %\n",
                LL::get_time_str(&t_str), __FUNCTION__, socket->fd, socket->i_rx_next, t_rx);
    if (socket->i_rx_next >= sizeof(OrderResponse)) {
        // one or more responses to handle
        size_t i{ };
        for (; i + sizeof(OrderResponse) <= socket->i_rx_next; i += sizeof(OrderResponse)) {
            auto response = reinterpret_cast<const OrderResponse*>(socket->rx_buffer.data() + i);
            logger.logf("% <OGC::%> response rx'd: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__, response->to_str());
            // sanity check for incorrect client ID (should not ever happen)
            if (response->ome_response.client_id != client_id) {
                logger.logf("% <OGC::%> ERROR received wrong client ID from exchange. Expected "
                            "% but got %\n",
                            LL::get_time_str(&t_str), __FUNCTION__, client_id,
                            response->ome_response.client_id);
                continue;
            }
            // sanity check for wrong sequence number
            if (response->n_seq != n_seq_next_expected) {
                logger.logf("% <OGC::%> ERROR received wrong response n_seq from exchange. "
                            "Expected % but got %\n", LL::get_time_str(&t_str), __FUNCTION__,
                            n_seq_next_expected, response->n_seq);
                continue;
            }
            // pass response to Trading Engine's queue
            ++n_seq_next_expected;
            auto tx_response = tx_responses.get_next_to_write();
            *tx_response = response->ome_response;
            tx_responses.increment_write_index();
        }
        // shift data in buffer now that blocks have been consumed
        memcpy(socket->rx_buffer.data(), socket->rx_buffer.data() + i, socket->i_rx_next - i);
        socket->i_rx_next -= i;
    }
}
}
