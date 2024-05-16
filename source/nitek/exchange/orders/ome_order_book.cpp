#include "ome_order_book.h"
#include "order_matching_engine.h"


namespace Exchange
{

OMEOrderBook::OMEOrderBook(TickerID ticker, LL::Logger& logger, OrderMatchingEngine& ome)
        : ticker(ticker),
          logger(logger),
          ome(ome) {
}
}