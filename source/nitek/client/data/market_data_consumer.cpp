#include "market_data_consumer.h"


namespace Client
{

MarketDataConsumer::MarketDataConsumer(Exchange::ClientID client_id,
                                       Exchange::MarketUpdateQueue& updates,
                                       const std::string& iface,
                                       const std::string& ip_snapshot, int port_snapshot,
                                       const std::string& ip_incremental,
                                       int port_incremental)
        : rx_updates(updates),
          logger("client_market_data_consumer_"
                         + std::to_string(client_id) + ".log"),
          iface(iface),
          ip_snapshot(ip_snapshot),
          port_snapshot(port_snapshot),
          socket_incremental(logger),
          socket_snapshot(logger) {
    auto default_rx_callback = [this](auto socket) {
        rx_callback(socket);
    };
    socket_incremental.rx_callback = default_rx_callback;
    auto fd = socket_incremental.init(ip_incremental, iface,
                                      port_incremental, true);
    ASSERT(fd >= 0, "<MDC> error creating UDP socket for consuming incremental market data, "
                    "error: " + std::string(std::strerror(errno)));
    const auto is_joined = socket_incremental.join_group(ip_incremental);
    ASSERT(is_joined, "<MDC> multicast join failed! error: " + std::string(std::strerror(errno)));
    socket_snapshot.rx_callback = default_rx_callback;
}

MarketDataConsumer::~MarketDataConsumer() {

}

void MarketDataConsumer::start() {

}

void MarketDataConsumer::stop() {

}

void MarketDataConsumer::run() noexcept {

}

void MarketDataConsumer::rx_callback(LL::McastSocket* socket) noexcept {
    (void) socket;

}

void MarketDataConsumer::queue_message(bool is_snapshot, const Exchange::MDPMarketUpdate* request) {
    (void) is_snapshot;
    (void) request;

}

void MarketDataConsumer::snapshot_sync_start() {

}

void MarketDataConsumer::snapshot_sync_check() {

}

}

