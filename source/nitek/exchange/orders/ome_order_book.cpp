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

}
void OMEOrderBook::add(ClientID client, OrderID client_order, TickerID ticker, Side side,
                       Price price, Qty qty) noexcept {
    (void) client;
    (void) client_order;
    (void) ticker;
    (void) side;
    (void) price;
    (void) qty;
}
void OMEOrderBook::cancel(ClientID client, OrderID order, TickerID ticker) noexcept {
    (void) client;
    (void) order;
    (void) ticker;
}
}