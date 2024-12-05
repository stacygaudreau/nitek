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
    // to-do: fix buggy to_str() method.
//    logger.logf("% <OMEOrderBook::%>\n%\n",
//                LL::get_time_str(&t_str), __FUNCTION__,
//                to_str(false, true));
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(500ms);
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

void OMEOrderBook::match(TickerID ticker_id, ClientID client_id, Side side,
                         OrderID client_oid, OrderID new_market_oid,
                         OMEOrder* order_matched, Qty* qty_remains) noexcept {
    // determine the qty filled and if any remains
    const auto order = order_matched;
    const auto order_qty = order->qty;
    const auto fill_qty = std::min(*qty_remains, order_qty);

    *qty_remains -= fill_qty;
    order->qty -= fill_qty;

    // a client response is sent back to both sides of the trade
    // incoming order:
    client_response = { OMEClientResponse::Type::FILLED, client_id, ticker_id,
                        client_oid, new_market_oid, side,
                        order_matched->price, fill_qty, *qty_remains };
    ome.send_client_response(&client_response);

    // passive book order being matched against:
    client_response = { OMEClientResponse::Type::FILLED, order->client_id, ticker_id,
                        order->client_order_id, order->market_order_id, order->side,
                        order_matched->price, fill_qty, order->qty };
    ome.send_client_response(&client_response);

    // a market data update is published to all participants
    // RE: the executed trade
    market_update = { OMEMarketUpdate::Type::TRADE, OrderID_INVALID,
                      ticker_id, side, order_matched->price, fill_qty,
                      Priority_INVALID };
    ome.send_market_update(&market_update);

    // second market update refreshes status of existing order
    // that was matched to in the book
    if (!order->qty) {
        // all qty was taken; let others know the order is gone
        market_update = { OMEMarketUpdate::Type::CANCEL,
                          order->market_order_id, ticker_id,
                          order->side, order->price,
                          order_qty, Priority_INVALID };
        ome.send_market_update(&market_update);
        remove_order_from_book(order);
    }
    else {
        // some qty remains; modify and update order qty
        market_update = { OMEMarketUpdate::Type::MODIFY,
                          order->market_order_id, ticker_id, order->side,
                          order->price, order->qty, order->priority };
        ome.send_market_update(&market_update);
    }
}

Qty OMEOrderBook::find_match(ClientID client_id, OrderID client_oid,
                             TickerID ticker_id, Side side, Price price,
                             Qty qty, OrderID new_market_oid) noexcept {
    // matches are made from most to least aggressive price level,
    // applying FIFO priority to orders within the same price tier.
    // matching continues until all QTY in the incoming order is
    // successfully matched, no more orders at a matching price
    // exist, or the side of the order book is empty
    auto qty_remains = qty;
    if (side == Side::BUY) {
        while (qty_remains && asks_by_price) {
            const auto asks_list = asks_by_price->order_0;
            if (price < asks_list->price) [[likely]] {
                break;
            }
            match(ticker_id, client_id, side, client_oid,
                  new_market_oid, asks_list,
                  &qty_remains);
        }
    }
    if (side == Side::SELL) {
        while (qty_remains && bids_by_price) {
            const auto bids_list = bids_by_price->order_0;
            if (price > bids_list->price) [[likely]] {
                break;
            }
            match(ticker_id, client_id, side, client_oid,
                  new_market_oid, bids_list,
                  &qty_remains);
        }
    }
    return qty_remains;
}

void OMEOrderBook::add_price_level(OMEOrdersAtPrice* new_orders_at_price) noexcept {
    // add new level to hashmap
    map_price_to_price_level.at(
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

void OMEOrderBook::remove_price_level(Side side, Price price) noexcept {
    // find and remove price level from the list
    const auto best_orders_by_price
            = (side == Side::BUY ? bids_by_price : asks_by_price);
    auto orders_at_price = get_level_for_price(price);

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
    map_price_to_price_level.at(price_to_index(price)) = nullptr;
    orders_at_price_pool.deallocate(orders_at_price);
}

void OMEOrderBook::add_order_to_book(OMEOrder* order) noexcept {
    const auto price_level = get_level_for_price(order->price);
    if (!price_level) {
        // price level and side doesn't exist; create one
        order->next = order->prev = order;
        auto new_price_level = orders_at_price_pool.allocate(
                order->side, order->price, order,
                nullptr, nullptr);
        add_price_level(new_price_level);
    }
    else {
        // price level already exists; append new order entry
        //  at end of doubly linked list
        auto first_order = price_level->order_0;
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
    auto orders_at_price = get_level_for_price(order->price);
    if (order->prev == order) {
        // it's the only order at the price level; we
        // remove the whole price level as none will
        // remain after this
        remove_price_level(order->side, order->price);
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

std::string OMEOrderBook::to_str(bool is_detailed, bool has_validity_check) {
    std::stringstream ss;

    auto printer = [&](std::stringstream& ss,
                       OMEOrdersAtPrice* levels, Side side, Price& last_price,
                       bool has_validity_check) {
        char buf[4096];
        Qty qty{ 0 };
        size_t n_orders{ 0 };

        // count the number of orders to print
        for (auto order = levels->order_0;; order = order->next) {
            qty += order->qty;
            ++n_orders;
            if (order->next == levels->order_0)
                break;
        }

        sprintf(buf, " { p:%3s [-]:%3s [+]:%3s } %-5s @ %-3s (%-4s)",
                price_to_str(levels->price).c_str(),
                price_to_str(levels->prev->price).c_str(),
                price_to_str(levels->next->price).c_str(),
                qty_to_str(qty).c_str(),
                price_to_str(levels->price).c_str(),
                std::to_string(n_orders).c_str());
        ss << buf;

        for (auto o_itr = levels->order_0;; o_itr = o_itr->next) {
            if (is_detailed) {
                sprintf(buf, "\n\t\t\t{ oid:%s, q:%s, p:%s, n:%s }",
                        order_id_to_str(o_itr->market_order_id).c_str(),
                        qty_to_str(o_itr->qty).c_str(),
                        order_id_to_str(
                                o_itr->prev ? o_itr->prev->market_order_id : OrderID_INVALID)
                                .c_str(),
                        order_id_to_str(
                                o_itr->next ? o_itr->next->market_order_id : OrderID_INVALID)
                                .c_str());
                ss << buf;
            }
            if (o_itr->next == levels->order_0)
                break;
        }

        ss << "\n";

        if (has_validity_check) {
            if ((side == Side::SELL && last_price >= levels->price)
                    || (side == Side::BUY && last_price <= levels->price)) {
                FATAL("Bid/ask price levels not sorted correctly: "
                              + price_to_str(last_price) + " levels:" + levels->to_str());
            }
            last_price = levels->price;
        }
    };

    ss << "\n----- ORDER BOOK FOR TICKER: " << ticker_id_to_str(assigned_ticker) << " -----\n";
    {
        auto asks = asks_by_price;
        auto last_ask_price = std::numeric_limits<Price>::min();
        if (asks == nullptr || asks->order_0 == nullptr)
            ss << "\n                  [NO ASKS ON BOOK]\n";
        else {
            for (size_t count{ }; asks; ++count) {
                ss << "ASKS[" << count << "] => ";
                auto next_ask_itr = (asks->next == asks_by_price ? nullptr : asks->next);
                printer(ss, asks, Side::SELL, last_ask_price, has_validity_check);
                asks = next_ask_itr;
            }
        }
    }

    ss << std::endl << "                          X" << std::endl << std::endl;

    {
        auto bids = bids_by_price;
        auto last_bid_price = std::numeric_limits<Price>::max();
        if (bids == nullptr || bids->order_0 == nullptr)
            ss << "\n                  [NO BIDS ON BOOK]\n";
        else {
            for (size_t count = 0; bids; ++count) {
                ss << "BIDS[" << count << "] => ";
                auto next_bid_itr = (bids->next == bids_by_price ? nullptr : bids->next);
                printer(ss, bids, Side::BUY, last_bid_price, has_validity_check);
                bids = next_bid_itr;
            }
        }
    }

    return ss.str();
}

}