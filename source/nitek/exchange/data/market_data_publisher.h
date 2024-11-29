/**
 *  
 *  Nitek - Exchange
 *
 *  @file market_data_publisher.h
 *  @brief Disseminates public market updates from the matching engine
 *  @author Stacy Gaudreau
 *  @date 2024.06.07
 *
 */


#pragma once


#include <functional>
#include "llbase/macros.h"
#include "llbase/logging.h"
#include "llbase/mcast_socket.h"
#include "exchange/data/types.h"
#include "exchange/data/ome_market_update.h"
#include "exchange/data/snapshot_synthesizer.h"


namespace Exchange
{
class MarketDataPublisher {
public:
    /**
     * @brief Publishes market update data in both incremental and snapshot-style to market
     * participants.
     * @param ome_market_updates Queue of market updates from the matching engine (OME)
     * @param iface Interface to bind to
     * @param ip_snapshot Multicast group IP address to bind to for snapshot-style updates
     * @param port_snapshot Port to bind to for snapshot-style updates
     * @param ip_incremental Multicast group IP address to bind to for incremental updates
     * @param port_incremental Port to bind to for incremental updates
     */
    MarketDataPublisher(MarketUpdateQueue& ome_market_updates,
                        const std::string& iface, const std::string& ip_snapshot,
                        int port_snapshot, const std::string& ip_incremental,
                        int port_incremental);
    ~MarketDataPublisher();

    /**
     * @brief Start the data publisher thread.
     */
    void start();
    /**
     * @brief Stop the data publisher thread.
     */
    void stop();
    /**
     * @brief The data publisher thread's main working method.
     */
    void run() noexcept;

PRIVATE_IN_PRODUCTION
    // incremental update queue from the OME
    MarketUpdateQueue& ome_market_updates;
    size_t n_seq_next{ 1 };    // next sequence number for outgoing incremental updates
    // snapshot update queue
    MDPMarketUpdateQueue tx_snapshot_updates{ OME::MAX_MARKET_UPDATES };
    volatile bool is_running{ false };
    std::unique_ptr<std::thread> thread{ nullptr };   // tracks the running thread
    std::string t_str{ };
    LL::Logger logger;
    LL::McastSocket socket_incremental;
    // generates snapshots of market data on its own thread
    std::unique_ptr<SnapshotSynthesizer> synthesizer;

DELETE_DEFAULT_COPY_AND_MOVE(MarketDataPublisher)

#ifdef IS_TEST_SUITE
public:
    auto get_is_running() { return is_running; }
    auto& get_snapshot_synthesizer() { return synthesizer; }
#endif
};
}
