#include "order_manager.h"
#include "nitek/client/trading/trading_engine.h"

using Request = Exchange::OMEClientRequest;

namespace Client
{
void OrderManager::request_new_order(Client::OMOrder& order,
                                     TickerID ticker,
                                     Price price,
                                     Side side,
                                     Qty qty) noexcept {
    // generate a new order request and send it to the Trading Engine
    const Request req{ Request::Type::NEW, engine.get_client_id(),
                       ticker, next_oid, side, price, qty };
//    engine.send_request(req);
    order = { ticker, next_oid, side, price, qty, OMOrder::State::PENDING_NEW };
    ++next_oid;
    logger.logf("% <OM::%> order request: % for: %\n",
                LL::get_time_str(&t_str), __FUNCTION__, req.to_str(), order.to_str());
}

void OrderManager::request_cancel_order(OMOrder& order) noexcept {
    // create and send a cancellation request for the existing order
    const Request req{ Request::Type::CANCEL, engine.get_client_id(),
                       order.ticker, order.id, order.side, order.price, order.qty };
//    engine.send_request(req);
    order.state = OMOrder::State::PENDING_CANCEL;
    logger.logf("% <OM::%> cancel request: % for: %\n",
                LL::get_time_str(&t_str), __FUNCTION__, req.to_str(), order.to_str());
}
}
