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

#ifdef IS_TEST_SUITE
    class MarketDataConsumerBasics;
    class MarketDataConsumerIntegration;
#endif


namespace Client
{
class MarketDataConsumer {
public:
    /**
     * @brief Client module for receiving and consuming data disseminated from the exchange.
     * @details Connects to the market exchange and receives incremental and snapshot updates of
     * the market state over UDP multicast.
     * @param client_id The unique identifier for this client in the exchange
     * @param updates Queue to push incoming updates to, for consumption by trading engine, etc.
     * @param iface Network interface name to bind to for receiving updates
     * @param ip_snapshot Multicast group IP for snapshot updates
     * @param port_snapshot Port for snapshot updates
     * @param ip_incremental Multicast group IP for incremental updates
     * @param port_incremental Port for incremental updates
     */
    MarketDataConsumer(Exchange::ClientID client_id, Exchange::MarketUpdateQueue& updates,
                       const std::string& iface, const std::string& ip_snapshot,
                       int port_snapshot, const std::string& ip_incremental,
                       int port_incremental);

    ~MarketDataConsumer();

    void start();

    void stop();

PRIVATE_IN_PRODUCTION
    size_t n_seq_inc_next{ 1 }; // next incremental update sequence number
    Exchange::MarketUpdateQueue& tx_updates;    // queue to push incoming updates to
    LL::Logger logger;
    const std::string iface;
    const std::string ip_snapshot;
    int port_snapshot;
    bool is_in_recovery{ false };   // true when packet drop occurs and re-syncing market state

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
     * Incoming queued updates to be processed.
     * Updates are ordered by their n_seq.
     * NB: std::map is inefficient which has insertion perf. of O(log(N)), but that's
     *  ok here because snapshot re-sync is expected to rarely occur, and to pause trading
     *  when it happens. If this is unacceptable, a better DS may exist.
     */
    using QueuedMarketUpdates = std::map<size_t, Exchange::OMEMarketUpdate>;
    QueuedMarketUpdates queued_incremental_updates;
    QueuedMarketUpdates queued_snapshot_updates;

    /**
     * @brief Main worker thread which consumes UDP network traffic from the exchange.
     */
    void run() noexcept;
    /**
     * @brief Called whenever there is data to receive on the incremental or snapshot streams.
     * @param socket The calling socket
     */
    void rx_callback(LL::McastSocket* socket) noexcept;
    /**
     * @brief Process and enqueue a given snapshot or incremental market update.
     * @param is_snapshot Set to false to process as an incremental update
     * @param update The update to place in the queue
     */
    void queue_update(bool is_snapshot, const Exchange::MDPMarketUpdate* update);
    /**
     * @brief Called to begin synchronising the data consumer with the exchange. This process
     * is generally slow and blocks trading until sync is complete.
     */
    void snapshot_sync_start();
    /**
     * @brief Check if recovery or sync can occur based on the present snapshot and incremental
     * update data.
     */
    void snapshot_sync_check();


DELETE_DEFAULT_COPY_AND_MOVE(MarketDataConsumer)

};
}
