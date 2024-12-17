#include "te_order_book.h"


namespace Client
{
TEOrderBook::TEOrderBook(TickerID ticker, LL::Logger& logger)
        : ticker(ticker),
          logger(logger) {

}

TEOrderBook::~TEOrderBook() {
    engine = nullptr;
    bids_by_price = asks_by_price = nullptr;
    id_to_order.fill(nullptr);
}

void TEOrderBook::on_market_update(const Exchange::OMEMarketUpdate& update) noexcept {
    // will this update change the best bid or ask price?
    const auto bid_is_updated = bids_by_price && update.side == Side::BUY
            && update.price >= bids_by_price->price;
    const auto ask_is_updated = asks_by_price && update.side == Side::SELL
            && update.price <= asks_by_price->price;
    using Type = Exchange::OMEMarketUpdate::Type;
    switch (update.type) {
        case Type::ADD: {
            // allocate and add a new order to the book
            auto order = order_pool.allocate(update.order_id, update.side,
                                             update.price, update.qty, update.priority,
                                             nullptr,nullptr);
            add_order(order);
            break;
        }
        case Type::MODIFY: {
            // find and update qty of existing order
            auto order = id_to_order.at(update.order_id);
            order->qty = update.qty;
            break;
        }
        case Type::CANCEL: {
            // find and remove existing order
            auto order = id_to_order.at(update.order_id);
            remove_order(order);
            break;
        }
        case Type::TRADE:
            // pass the update to the trading engine and skip the rest of this routine
            // since the order book need not be updated
//            engine->on_trade_update(update, this);
            return;
            break;
        case Type::CLEAR:
            // book needs clearing, likely we are recovering from a market snapshot stream
            clear_entire_book();
            break;
        case Type::INVALID:
        case Type::SNAPSHOT_START:
        case Type::SNAPSHOT_END:
            break;
    }

    // update best bid/ask offer and notify the trading engine that an update was processed
    update_bbo(bid_is_updated, ask_is_updated);
    logger.logf("% <TEOrderBook::%> % %\n",
                LL::get_time_str(&t_str), __FUNCTION__, update.to_str(), bbo.to_str());
//    engine->on_order_book_update(update.ticker_id, update.price, update.side, this);
}

void TEOrderBook::clear_entire_book() {
    // empty all members of the book
    for (auto o: id_to_order) {
        if (o) order_pool.deallocate(o);
    }

    id_to_order.fill(nullptr);

    if(bids_by_price) {
        for(auto bid = bids_by_price->next; bid != bids_by_price; bid = bid->next)
            orders_at_price_pool.deallocate(bid);
        orders_at_price_pool.deallocate(bids_by_price);
    }

    if(asks_by_price) {
        for(auto ask = asks_by_price->next; ask != asks_by_price; ask = ask->next)
            orders_at_price_pool.deallocate(ask);
        orders_at_price_pool.deallocate(asks_by_price);
    }

    bids_by_price = asks_by_price = nullptr;
}

void TEOrderBook::set_trading_engine(TradingEngine* new_engine) {
    engine = new_engine;
}

void TEOrderBook::update_bbo(bool should_update_bid, bool should_update_ask) noexcept {
    if (should_update_bid) {
        if (bids_by_price) {
            // set the BBO's bid price to the best offer
            bbo.bid = bids_by_price->price;
            bbo.bid_qty = bids_by_price->order_0->qty;
            // accumulate the QTY of all orders in the price level
            for (auto order = bids_by_price->order_0->next;
                 order != bids_by_price->order_0; order = order->next) {
                bbo.bid_qty += order->qty;
            }
        } else {
            bbo.bid = Price_INVALID;
            bbo.bid_qty = Qty_INVALID;
        }
    }
    if (should_update_ask) {
        if (asks_by_price) {
            // set the BBO's ask price to the best offer
            bbo.ask = asks_by_price->price;
            bbo.ask_qty = asks_by_price->order_0->qty;
            // accumulate the QTY of all orders in the price level
            for (auto order = asks_by_price->order_0->next;
                 order != asks_by_price->order_0; order = order->next) {
                bbo.ask_qty += order->qty;
            }
        } else {
            bbo.ask = Price_INVALID;
            bbo.ask_qty = Qty_INVALID;
        }
    }
}

void TEOrderBook::add_order(TEOrder* order) {
    /*
     * NB: implementation is necessarily almost identical to OMEOrderBook::add_order_to_book()
     */
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
    // add mapping for the order ID to the order hashmap
    id_to_order.at(order->id) = order;
}

void TEOrderBook::remove_order(TEOrder* order) {
    /*
     * NB: implementation is necessarily almost identical to OMEOrderBook::remove_order_from_book()
     */
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
    // remove from order hashmap and free block in mempool
    id_to_order.at(order->id) = nullptr;
    order_pool.deallocate(order);

}

void TEOrderBook::add_price_level(TEOrdersAtPrice* new_orders_at_price) noexcept {
    /*
     * NB: implementation is necessarily almost identical to OMEOrderBook::add_price_level()
     */
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

void TEOrderBook::remove_price_level(Side side, Price price) noexcept {
    /*
     * NB: implementation is necessarily almost identical to OMEOrderBook::remove_price_level()
     */
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


}