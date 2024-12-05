/**
 *  
 *  Nitek - Exchange
 *
 *  @file fifo_sequencer.h
 *  @brief FIFO queue sequencer for the exchange Order Gateway Server
 *  @author Stacy Gaudreau
 *  @date 2024.05.31
 *
 */


#pragma once


#include <algorithm>
#include <array>
#include <memory>
#include "llbase/macros.h"
#include "llbase/logging.h"
#include "llbase/threading.h"
#include "exchange/data/ome_client_request.h"


namespace Exchange
{
class FIFOSequencer {
public:
    /**
     * @brief First-in, first-out queue for sequencing order events.
     * @details This component ensures client order request packets are
     * processed in the order they're received, irrespective of any TCP
     * multiplexing latencies. Handles forwarding order requests
     * from the gateway to the exchange matching engine.
     */
    FIFOSequencer(ClientRequestQueue& rx_requests, LL::Logger& logger)
            : rx_requests(rx_requests),
              logger(logger) { }

    /**
     * @brief Sequences and publishes all queued order requests
     * to the Order Matching Engine.
     */
    inline void sequence_and_publish() {
        if (!n_pending_requests) [[unlikely]] {
            return;
        }
        logger.logf("% <FIFOSequencer::%> pending requests: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    n_pending_requests);
        // sort pending requests by their timestamps
        // nb: measure and adjust this algo in reality.
        std::sort(pending_requests.begin(),
                  pending_requests.begin() + n_pending_requests);
        for (size_t i{ }; i < n_pending_requests; ++i) {
            const auto& req = pending_requests.at(i);
            logger.logf("% <FIFOSequencer::%> sequencing request: % at t_rx: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__,
                        req.request.to_str(), req.t_rx);
            // write out to the Matching Engine's request queue
            auto next = rx_requests.get_next_to_write();
            *next = req.request;
            rx_requests.increment_write_index();
        }
        n_pending_requests = 0;
    }
    /**
     * @brief Push a pending client order request onto the queue
     */
    inline void push_client_request(const OMEClientRequest& request,
                                    LL::Nanos t_rx) {
        if (n_pending_requests >= pending_requests.size()) [[unlikely]] {
            FATAL("<FIFOSequencer> too many pending requests!");
        }
        pending_requests.at(n_pending_requests++) = { t_rx, request };
    }

    /**
     * @brief Tracks a client request which is awaiting processing
     * by the order gateway
     */
    struct PendingClientRequest {
        LL::Nanos t_rx{ };
        OMEClientRequest request;
        bool operator<(const PendingClientRequest& B) const {
            return (t_rx < B.t_rx);
        }
    };

private:
    ClientRequestQueue& rx_requests;
    LL::Logger& logger;
    std::string t_str{ };

    std::array<PendingClientRequest,
            Limits::MAX_PENDING_ORDER_REQUESTS> pending_requests;
    size_t n_pending_requests{ 0 };

DELETE_DEFAULT_COPY_AND_MOVE(FIFOSequencer)

#ifdef IS_TEST_SUITE
    auto& get_pending_requests() { return pending_requests; }
#endif
};

}


