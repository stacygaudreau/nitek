#include "order_gateway_server.h"


namespace Exchange
{
OrderGatewayServer::OrderGatewayServer(ClientRequestQueue& tx_requests,
                                       ClientResponseQueue& rx_responses,
                                       const std::string& iface, int port)
        :
        iface(iface),
        port(port),
        rx_responses(rx_responses),
        logger("exchange_order_gateway_server.log"),
        server(logger),
        fifo(tx_requests, logger) {
    map_client_to_tx_n_seq.fill(1);
    map_client_to_rx_n_seq.fill(1);
    map_client_to_socket.fill(nullptr);
    server.set_rx_callback([this](auto socket, auto t_rx) {
        rx_callback(socket, t_rx);
    });
    server.set_rx_done_callback([this]() { rx_done_callback(); });
}

OrderGatewayServer::~OrderGatewayServer() {
    stop();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);
}

void OrderGatewayServer::start() {
    is_running = true;
    server.listen(iface, port);
    thread = LL::create_and_start_thread(-1, "OrderGatewayServer",
                                         [this]() { run(); });
    ASSERT(thread != nullptr, "<OGS> Failed to start thread for order gateway");
}

void OrderGatewayServer::stop() {
    // the thread is halted when is_running is false
    is_running = false;
    if (thread != nullptr && thread->joinable())
        thread->join();
}

void OrderGatewayServer::run() {
    logger.logf("% <OGS::%> running order gateway...\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    while (is_running) {
        server.poll();
        server.tx_and_rx();

        for (auto res = rx_responses.get_next_to_read(); rx_responses.size() && res;
             res = rx_responses.get_next_to_read()) {
            auto& n_seq_tx_next = map_client_to_tx_n_seq[res->client_id];
            logger.logf("% <OGS::%> processing cid: %, n_seq: %, response: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__,
                        res->client_id, n_seq_tx_next, res->to_str());
            ASSERT(map_client_to_socket[res->client_id] != nullptr,
                   "<OGS> missing socket for client: "
                           + client_id_to_str(res->client_id));
            map_client_to_socket[res->client_id]->load_tx(&n_seq_tx_next, sizeof(n_seq_tx_next));
            map_client_to_socket[res->client_id]->load_tx(res, sizeof(OMEClientResponse));

            rx_responses.increment_read_index();

            ++n_seq_tx_next;
        }
    }
}

}