#include "ome_order_book.h"
#include "order_matching_engine.h"


namespace Exchange
{

OMEOrderBook::OMEOrderBook(TickerID ticker, LL::Logger& logger, OrderMatchingEngine& ome)
        : ticker(ticker),
          logger(logger),
          ome(ome) {
}

OMEOrderBook::~OMEOrderBook() {
    logger.logf("% <OMEOrderBook::%>\n%\n",
                LL::get_time_str(&t_str), __FUNCTION__,
                to_str(false, true));
    bids_by_price = asks_by_price = nullptr;
    for (auto& oids: map_client_id_to_order) {
        oids.fill(nullptr);
    }
    std::cout << "Orderbook " << ticker << " DESTRUCTED";
}

void OMEOrderBook::add(ClientID client, OrderID client_order, TickerID ticker, Side side,
                       Price price, Qty qty) noexcept {
    const auto new_market_oid = get_new_market_order_id();
    client_response = {
            OMEClientResponse::Type::ACCEPTED, client, ticker, client_order,
            new_market_oid, side,
            price, 0, qty };
//    matching_engine_->sendClientResponse(&client_response_);
//
//    const auto leaves_qty = checkForMatch(client_id, client_order_id, ticker_id, side, price, qty, new_market_order_id);
//
//    if (LIKELY(leaves_qty)) {
//        const auto priority = getNextPriority(price);
//
//        auto order = order_pool_.allocate(ticker_id, client_id, client_order_id, new_market_order_id, side, price, leaves_qty, priority, nullptr,
//                                          nullptr);
//        addOrder(order);
//
//        market_update_ = {MarketUpdateType::ADD, new_market_order_id, ticker_id, side, price, leaves_qty, priority};
//        matching_engine_->sendMarketUpdate(&market_update_);
//    }
}

void OMEOrderBook::cancel(ClientID client, OrderID order, TickerID ticker) noexcept {
    (void) client;
    (void) order;
    (void) ticker;
}

std::string OMEOrderBook::to_str(bool is_detailed, bool has_validity_check) {
    (void) is_detailed;
    (void) has_validity_check;
    return std::string();
}
}