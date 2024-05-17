#include "order_matching_engine.h"
#include "exchange/data/ome_client_request.h"


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
    is_running = false;
    if (is_running && thread != nullptr && thread->joinable()) {
        thread->join();
    }
}

void OrderMatchingEngine::process_client_request(const OMEClientRequest* request) noexcept {
    switch (request->type) {
    case OMEClientRequest::Type::NEW:
        order_book_for_ticker[request->ticker_id]->add(
                request->client_id, request->order_id,
                request->ticker_id, request->side,
                request->price, request->qty
        );
        break;
    case OMEClientRequest::Type::CANCEL:
        order_book_for_ticker[request->ticker_id]->cancel(
                request->client_id, request->order_id,
                request->ticker_id);
        break;
    default:
        FATAL("<OME> Received invalid client request! "
                      + OMEClientRequest::type_to_str(request->type) + "\n");
        break;
    }
}

void OrderMatchingEngine::run() noexcept {
    is_running = true;
    logger.logf("% <OME::%> accepting client order requests...\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    // consume client order requests received on the queue
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
void OrderMatchingEngine::send_client_response(const OMEClientResponse* response) noexcept {
    logger.logf("% <OME::%> tx response: %\n",
                LL::get_time_str(&t_str), __FUNCTION__,
                response->to_str());
    auto next = tx_responses->get_next_to_write();
    *next = std::move(*response);
    tx_responses->increment_write_index();
}

void OrderMatchingEngine::send_market_update(const OMEMarketUpdate* update) noexcept {
    logger.logf("% <OME::%> tx update: %\n",
                LL::get_time_str(&t_str), __FUNCTION__,
                update->to_str());
    auto next = tx_market_updates->get_next_to_write();
    *next = std::move(*update);
    tx_market_updates->increment_write_index();
}

}
