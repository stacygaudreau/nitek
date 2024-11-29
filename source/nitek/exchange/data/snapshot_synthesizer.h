/**
 *  
 *  Nitek - Exchange
 *
 *  @file snapshot_synthesizer.h
 *  @brief Synthesizer for snapshot updates of market data
 *  @author Stacy Gaudreau
 *  @date 2024.06.07
 *
 */


#pragma once


#include <array>
#include "llbase/macros.h"
#include "llbase/logging.h"
#include "llbase/mcast_socket.h"
#include "llbase/mempool.h"
#include "llbase/timekeeping.h"
#include "exchange/data/ome_market_update.h"


namespace Exchange
{
class SnapshotSynthesizer {
public:
    /**
     * @brief Consumes incremental market updates from the publisher and synthesizes
     * them into combined snapshots of the market's current order book status.
     * @param tx_updates
     * @param iface Interface to bind to
     * @param ip Multicast group IP to bind to for snapshot dissemination
     * @param port UDP port to bind to
     */
    SnapshotSynthesizer(MDPMarketUpdateQueue& tx_updates, const std::string& iface,
                        const std::string& ip, int port);
    ~SnapshotSynthesizer();

    /**
     * @brief Start the worker thread.
     */
    void start();
    /**
     * @brief Stop the worker thread.
     */
    void stop();
    /**
     * @brief The snapshot synthesizer thread's main working method.
     */
    void run();
    /**
     * @brief Add a given market update to the current snapshot.
     */
    void add_to_snapshot(const MDPMarketUpdate* update_from_publisher);
    /**
     * @brief A complete snapshot of the order book's state is published.
     * @details The format of a full snapshot described:
     *  An MDPMarketUpdate is sent for each step with type --
     *  1. SNAPSHOT_START with n_seq=0 => beginning
     *  2. For each instrument:
     *      1. CLEAR => client should wipe the order book for the instrument
     *      2. ADD => for each order in the book
     *  3. SNAPSHOT_END => also includes the last n_seq used to construct the snapshot
     */
    void publish_snapshot();

PRIVATE_IN_PRODUCTION
    MDPMarketUpdateQueue& tx_updates;
    LL::Logger logger;
    volatile bool is_running{ false };
    std::unique_ptr<std::thread> thread{ nullptr };   // tracks the running thread
    std::string t_str{ };
    LL::McastSocket socket;
    std::array<std::array<OMEMarketUpdate*, OME::MAX_ORDER_IDS>,
               OME::MAX_TICKERS> map_ticker_to_order;
    size_t n_seq_last{ 0 };
    LL::Nanos t_last_snapshot{ 0 };
#ifdef IS_TEST_SUITE
    static constexpr LL::Nanos SECONDS_BETWEEN_SNAPSHOTS{ 1 };
#else
    static constexpr LL::Nanos SECONDS_BETWEEN_SNAPSHOTS{ 60 };
#endif

    LL::MemPool<OMEMarketUpdate> update_pool{ OME::MAX_ORDER_IDS };

DELETE_DEFAULT_COPY_AND_MOVE(SnapshotSynthesizer)

#ifdef IS_TEST_SUITE
public:
    auto get_is_running() { return is_running; }
#endif
};
}
