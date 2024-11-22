/**
 *  
 *  Nitek - Client
 *
 *  @file market_data_consumer.h
 *  @brief Ingress which consumes market data fromm the exchange
 *  @author Stacy Gaudreau
 *  @date 2024.11.22
 *
 */


#pragma once


#include <functional>
#include <map>
#include <memory>

#include "llbase/macros.h"
#include "llbase/logging.h"
#include "llbase/lfqueue.h"
#include "llbase/threading.h"
#include "llbase/mcast_socket.h"
#include "llbase/timekeeping.h"
#include "exchange/data/ome_market_update.h"


namespace Client
{
class MarketDataConsumer {
public:
    MarketDataConsumer(Exchange::ClientID client_id, Exchange::MarketUpdateQueue& updates,
                       const std::string& iface, const std::string& ip_snapshot,
                       int port_snapshot, const std::string& ip_incremental,
                       int port_incremental);
    ~MarketDataConsumer();

    void start();
    void stop();

private:
    size_t n_seq_inc_next{ 1 }; // next incremental update sequence number
    Exchange::MarketUpdateQueue& rx_updates;    // incoming market updates
    LL::Logger logger;
    const std::string iface;
    const std::string ip_snapshot;
    int port_snapshot;

    /*
     * Dedicated worker thread
     */
    volatile bool is_running{ false };
    std::unique_ptr<std::thread> thread{ nullptr }; // the running thread
    std::string t_str{ };
    /*
     * UDP sockets to receive incremental and snapshot updates on
     */
    LL::McastSocket socket_incremental;
    LL::McastSocket socket_snapshot;
    /*
     * Incoming queued updates to be processed
     */
    using QueuedMarketUpdates = std::map<size_t, Exchange::OMEMarketUpdate>;
    QueuedMarketUpdates queued_market_updates;
    QueuedMarketUpdates queued_snapshots_updates;

    void run() noexcept;
    void rx_callback(LL::McastSocket* socket) noexcept;
    void queue_message(bool is_snapshot, const Exchange::MDPMarketUpdate* request);
    void snapshot_sync_start();
    void snapshot_sync_check();

DELETE_DEFAULT_COPY_AND_MOVE(MarketDataConsumer)
};
}
