#include "order_matching_engine.h"


namespace Exchange
{
OrderMatchingEngine::OrderMatchingEngine(ClientRequestQueue* rx_requests,
                                         ClientResponseQueue* tx_responses,
                                         MarketUpdateQueue* tx_market_updates)
        : rx_requests(rx_requests),
          tx_responses(tx_responses),
          tx_market_updates(tx_market_updates),
          logger("exchange_order_matching_engine.log") {
    // an order book for each ticker in the hashmap
    for (size_t i{ }; i < order_book_for_ticker.size(); ++i) {
        order_book_for_ticker[i] =
                std::make_unique<OMEOrderBook>(i, logger, *this);
    }
}
OrderMatchingEngine::~OrderMatchingEngine() {
    is_running = false;
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);
    rx_requests = nullptr;
    tx_responses = nullptr;
    tx_market_updates = nullptr;
    for (auto& ob: order_book_for_ticker) {
        ob = nullptr;
    }
}

void OrderMatchingEngine::start() {
    thread = LL::create_and_start_thread(-1, "OME",
                                         [this]() { run(); });
    ASSERT(thread != nullptr, "<OME> Failed to start thread for matching engine.");
    thread->detach();
}

void OrderMatchingEngine::stop() {
    // the running thread halts its loop when is_running becomes false
    if (is_running && thread != nullptr && thread->joinable()) {
        is_running = false;
        thread->join();
    }
}

void OrderMatchingEngine::process_client_request(const OMEClientRequest* request) noexcept {
    (void) request;
}

void OrderMatchingEngine::run() noexcept {
    is_running = true;
    logger.logf("% <OME::%> accepting client order requests...\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    while (is_running) {
        const auto request = rx_requests->get_next_to_read();
        if (request != nullptr) [[likely]] {
            logger.logf("% <OME::%> rx request: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__,
                        request->to_str());
            process_client_request(request);
            rx_requests->increment_read_index();
        }
    }
}

}
