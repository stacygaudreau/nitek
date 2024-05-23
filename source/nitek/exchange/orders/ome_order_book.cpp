#include "ome_order_book.h"
#include "order_matching_engine.h"


namespace Exchange
{

OMEOrderBook::OMEOrderBook(TickerID assigned_ticker, LL::Logger& logger, OrderMatchingEngine& ome)
        : assigned_ticker(assigned_ticker),
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
}

void OMEOrderBook::add(ClientID client_id, OrderID client_oid, TickerID ticker_id,
                       Side side, Price price, Qty qty) noexcept {
    const auto new_market_oid = get_new_market_order_id();
    client_response = {
            OMEClientResponse::Type::ACCEPTED, client_id, ticker_id,
            client_oid, new_market_oid, side,
            price, 0, qty };
    ome.send_client_response(&client_response);
    const auto qty_remains = find_match(client_id, client_oid,
                                        ticker_id, side, price, qty,
                                        new_market_oid);
    if (qty_remains) [[likely]] {
        // some qty is unfilled, so a new order is generated
        const auto priority = get_next_priority(price);
        auto order = order_pool.allocate(ticker_id, client_id,
                                         client_oid, new_market_oid,
                                         side, price, qty_remains,
                                         priority, nullptr, nullptr);
        add_order_to_book(order);
        // ...and a market update is published to
        // clients, reflecting the new order
        market_update = { OMEMarketUpdate::Type::ADD,
                          new_market_oid, ticker_id,
                          side, price, qty_remains,
                          priority };
        ome.send_market_update(&market_update);
    }
}

void OMEOrderBook::cancel(ClientID client_id, OrderID order_id, TickerID ticker_id) noexcept {
    // is the cancellation request valid?
    OMEOrder* exchange_order{ nullptr };
    auto can_be_canceled = (client_id < map_client_id_to_order.size());
    if (can_be_canceled) [[likely]] {
        // verify the corresponding order exists in the exchange
        auto& map_client_order = map_client_id_to_order.at(client_id);
        exchange_order = map_client_order.at(order_id);
        can_be_canceled = (exchange_order != nullptr);
    }
    if (!can_be_canceled) [[unlikely]] {
        // send a rejection message to the client
        client_response = { OMEClientResponse::Type::CANCEL_REJECTED,
                            client_id, ticker_id, order_id,
                            OrderID_INVALID, Side::INVALID,
                            Price_INVALID, Qty_INVALID,
                            Qty_INVALID };
    }
    else {
        // message confirms to the client the order has been cancelled
        client_response = { OMEClientResponse::Type::CANCELLED,
                            client_id, ticker_id, order_id,
                            exchange_order->market_order_id,
                            exchange_order->side, exchange_order->price,
                            Qty_INVALID, exchange_order->qty };
        market_update = { OMEMarketUpdate::Type::CANCEL,
                          exchange_order->market_order_id,
                          ticker_id, exchange_order->side, exchange_order->price,
                          0, exchange_order->priority };

        remove_order_from_book(exchange_order);
        // notify the market that the order is gone
        ome.send_market_update(&market_update);
    }
    // the client receives a response either way
    ome.send_client_response(&client_response);
}

std::string OMEOrderBook::to_str(bool is_detailed, bool has_validity_check) {
    (void) is_detailed;
    (void) has_validity_check;
    return std::string();
}

Qty OMEOrderBook::find_match(ClientID client_id, OrderID client_order_id,
                             TickerID ticker_id, Side side, Price price,
                             Qty qty, OrderID new_market_order_id) noexcept {
    (void) client_id;
    (void) client_order_id;
    (void) ticker_id;
    (void) side;
    (void) price;
    (void) qty;
    (void) new_market_order_id;
    return 0;
}

void OMEOrderBook::add_orders_at_price(
        OMEOrdersAtPrice* new_orders_at_price) noexcept {
    // add new level to hashmap
    map_price_to_orders_at_price.at(
            price_to_index(new_orders_at_price->price))
            = new_orders_at_price;
    // walk through price levels from most to least aggressive
    // to find correct level for insertion
    // nb: an alternate algo is possible and may perform better
    const auto best_orders_by_price
            = (new_orders_at_price->side == Side::BUY ? bids_by_price : asks_by_price);
    if (!best_orders_by_price) [[unlikely]] {
        // edge case where a side of the book is empty
        (new_orders_at_price->side == Side::BUY ? bids_by_price : asks_by_price)
                = new_orders_at_price;
        new_orders_at_price->prev = new_orders_at_price->next
                = new_orders_at_price;
    }
    else {
        // find correct entry to insert @ in DLL of price levels
        // target tracks price level before or after the new one
        auto target = best_orders_by_price;
        // should insertion be before or after the found element?
        bool should_add_after = ((new_orders_at_price->side == Side::SELL
                && new_orders_at_price->price > target->price)
                || (new_orders_at_price->side == Side::BUY && new_orders_at_price->price
                        < target->price));
        // find insertion location
        if (should_add_after) {
            target = target->next;
            should_add_after =
                    ((new_orders_at_price->side == Side::SELL && new_orders_at_price->price
                            > target->price) || (new_orders_at_price->side == Side::BUY
                            && new_orders_at_price->price < target->price));
        }
        while (should_add_after && target != best_orders_by_price) {
            should_add_after = ((new_orders_at_price->side == Side::SELL
                    && new_orders_at_price->price > target->price)
                    || (new_orders_at_price->side == Side::BUY
                            && new_orders_at_price->price < target->price));
            if (should_add_after)
                target = target->next;
        }
        // append new price level around target location
        if (should_add_after) {
            // insert after target
            if (target == best_orders_by_price) {
                target = best_orders_by_price->prev;
            }
            new_orders_at_price->prev = target;
            target->next->prev = new_orders_at_price;
            new_orders_at_price->next = target->next;
            target->next = new_orders_at_price;
        }
        else {
            // insert before target
            new_orders_at_price->prev = target->prev;
            new_orders_at_price->next = target;
            target->prev->next = new_orders_at_price;
            target->prev = new_orders_at_price;
            // check whether prepending the level has changed
            //  the order of bids_by_price or asks_by_price
            if ((new_orders_at_price->side == Side::BUY
                    && new_orders_at_price->price > best_orders_by_price->price)
                    || (new_orders_at_price->side == Side::SELL
                            && new_orders_at_price->price < best_orders_by_price->price)) {
                target->next = (target->next == best_orders_by_price
                                ? new_orders_at_price : target->next);
                (new_orders_at_price->side == Side::BUY ? bids_by_price : asks_by_price)
                        = new_orders_at_price;
            }
        }

    }
}

void OMEOrderBook::remove_orders_at_price(Side side, Price price) noexcept {
    // find and remove price level from the list
    const auto best_orders_by_price
            = (side == Side::BUY ? bids_by_price : asks_by_price);
    auto orders_at_price = get_orders_at_price(price);

    if (orders_at_price->next == orders_at_price) [[unlikely]] {
        // the book is empty on this side
        (side == Side::BUY ? bids_by_price : asks_by_price) = nullptr;
    }
    else {
        // side is not empty; update adjacent pointers
        orders_at_price->prev->next = orders_at_price->next;
        orders_at_price->next->prev = orders_at_price->prev;

        if (orders_at_price == best_orders_by_price) {
            (side == Side::BUY ? bids_by_price : asks_by_price)
                    = orders_at_price->next;
        }

        orders_at_price->prev = orders_at_price->next = nullptr;
    }
    // remove from hashmap and deallocate block in mempool
    map_price_to_orders_at_price.at(price_to_index(price)) = nullptr;
    orders_at_price_pool.deallocate(orders_at_price);
}

void OMEOrderBook::add_order_to_book(OMEOrder* order) noexcept {
    const auto orders_at_price = get_orders_at_price(order->price);
    if (!orders_at_price) {
        // price level and side doesn't exist; create one
        order->next = order->prev = order;
        auto new_orders_at_price = orders_at_price_pool.allocate(
                order->side, order->price, order,
                nullptr, nullptr);
        add_orders_at_price(new_orders_at_price);
    }
    else {
        // price level already exists; append new order entry
        //  at end of doubly linked list
        auto first_order = orders_at_price->order_0;
        first_order->prev->next = order;
        order->prev = first_order->prev;
        order->next = first_order;
        first_order->prev = order;
    }
    // add mapping to order hashmap for client ID
    map_client_id_to_order.at(
            order->client_id).at(order->client_order_id) = order;
}

void OMEOrderBook::remove_order_from_book(OMEOrder* order) noexcept {
    // find price level the order belongs to
    auto orders_at_price = get_orders_at_price(order->price);
    if (order->prev == order) {
        // it's the only order at the price level; we
        // remove the whole price level as none will
        // remain after this
        remove_orders_at_price(order->side, order->price);
    }
    else {
        // remove link
        const auto order_before = order->prev;
        const auto order_after = order->next;
        order_before->next = order_after;
        order_after->prev = order_before;
        if (orders_at_price->order_0 == order) {
            orders_at_price->order_0 = order_after;
        }
        order->prev = order->next = nullptr;
    }
    // remove from hashmap and free block in mempool
    map_client_id_to_order.at(
            order->client_id).at(
            order->client_order_id) = nullptr;
    order_pool.deallocate(order);
}

}