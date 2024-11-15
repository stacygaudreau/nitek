#include "exchange_server.h"


namespace Exchange
{
ExchangeServer::ExchangeServer(const std::string& order_iface, int order_port,
                               const std::string& data_iface,
                               const std::string& data_incremental_ip,
                               int data_incremental_port, const std::string& data_snapshot_ip,
                               int data_snapshot_port)
        : order_iface(order_iface),
          order_port(order_port),
          data_iface(data_iface),
          data_incremental_ip(data_incremental_ip),
          data_incremental_port(data_incremental_port),
          data_snapshot_ip(data_snapshot_ip),
          data_snapshot_port(data_snapshot_port) {

}

ExchangeServer::~ExchangeServer() {
    stop();
}

void ExchangeServer::start() {
    logger.logf("% <ExchangeServer::%> starting Matching Engine\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    ome = std::make_unique<OrderMatchingEngine>(&client_requests, &client_responses,
                                                &market_updates);
    ome->start();
    logger.logf("% <ExchangeServer::%> starting Order Gateway\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    ogs = std::make_unique<OrderGatewayServer>(client_requests, client_responses, order_iface,
                                               order_port);
    ogs->start();
    logger.logf("% <ExchangeServer::%> starting Data Publisher\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    mdp = std::make_unique<MarketDataPublisher>(market_updates, data_iface, data_snapshot_ip,
                                                data_snapshot_port, data_incremental_ip,
                                                data_incremental_port);
    mdp->start();
    // all child modules started; now run main worker thread
    is_running = true;
    thread = LL::create_and_start_thread(-1, "ExchangeServer",
                                         [this]() { run(); });
    ASSERT(thread != nullptr, "<ExchangeServer> failed to start thread");
}

void ExchangeServer::stop() {
    if (is_running) {
        logger.logf("% <ExchangeServer::%> stopping all running exchange processes...\n",
                    LL::get_time_str(&t_str), __FUNCTION__);
        is_running = false;
    }
    if (thread != nullptr && thread->joinable())
        thread->join();
}

void ExchangeServer::run() {
    while (is_running) {
        // run the exchange until a signal is received
        logger.logf("% <ExchangeServer::%> Sleeping for %ms...\n",
                    LL::get_time_str(&t_str), __FUNCTION__, T_SLEEP_MS);
        usleep(T_SLEEP_MS * 1000); // sleep which is easily terminated by a SIGINT/other unix signal
    }
}

}

