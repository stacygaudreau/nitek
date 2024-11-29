/**
 *  
 *  Nitek - Exchange
 *
 *  @file ome_market_update.h
 *  @brief Provides market updates to the Market Data Publisher.
 *  @author Stacy Gaudreau
 *  @date 2024.05.08
 *
 */


#pragma once


#include <sstream>
#include <string>
#include "exchange/data/types.h"
#include "llbase/lfqueue.h"


namespace Exchange
{
#pragma pack(push, 1)   // 1-byte bit alignment


/**
 * @brief Market update message sent from the OME to
 * the Market Data Publisher for broadcasting to clients.
 */
struct OMEMarketUpdate {
    enum class Type : uint8_t {
        INVALID = 0,
        CLEAR = 1,
        ADD = 2,
        MODIFY = 3,
        CANCEL = 4,
        TRADE = 5,
        SNAPSHOT_START = 6,
        SNAPSHOT_END = 7
    };

    Type type{ Type::INVALID };             // message type
    OrderID order_id{ OrderID_INVALID };    // order id in the book
    TickerID ticker_id{ TickerID_INVALID }; // ticker of product
    Side side{ Side::INVALID };             // buy or sell
    Price price{ Price_INVALID };           // price of order
    Qty qty{ Qty_INVALID };                 // quantity
    Priority priority{ Priority_INVALID };  // priority in the FIFO queue

    inline static std::string type_to_str(Type type) {
        switch (type) {
        case Type::CLEAR:
            return "CLEAR";
        case Type::ADD:
            return "ADD";
        case Type::MODIFY:
            return "MODIFY";
        case Type::CANCEL:
            return "CANCEL";
        case Type::TRADE:
            return "TRADE";
        case Type::SNAPSHOT_START:
            return "SNAPSHOT_START";
        case Type::SNAPSHOT_END:
            return "SNAPSHOT_END";
        case Type::INVALID:
            return "INVALID";
        }
        return "UNKNOWN";
    }

    [[nodiscard]] auto to_str() const {
        std::stringstream ss;
        ss << "<OMEMarketUpdate>"
           << " ["
           << "type: " << type_to_str(type)
           << ", ticker: " << ticker_id_to_str(ticker_id)
           << ", oid: " << order_id_to_str(order_id)
           << ", side: " << side_to_str(side)
           << ", qty: " << qty_to_str(qty)
           << ", price: " << price_to_str(price)
           << ", priority: " << priority_to_str(priority)
           << "]";
        return ss.str();
    }

    bool operator==(const OMEMarketUpdate& other) const {
        return type == other.type
                && order_id == other.order_id
                && ticker_id == other.ticker_id
                && side == other.side
                && price == other.price
                && qty == other.qty
                && priority == other.priority;
    }
};


/**
 * @brief Market update format sent by the Market Data
 * Publisher for public exchange clients to consume.
 * @details Data is disseminated over UDP, so the
 * n_seq member keeps track of data sequence order so
 * clients can rebuild correct market data order on
 * their end
 */
struct MDPMarketUpdate {
    size_t n_seq{ 0 };
    OMEMarketUpdate ome_update;

    [[nodiscard]] auto to_str() const {
        std::stringstream ss;
        ss << "<MDPMarketUpdate>"
           << " ["
           << "n: " << n_seq
           << " " << ome_update.to_str()
           << "]";
        return ss.str();
    }
};


#pragma pack(pop)       // back to default bit alignment

// OrderMatchingEngine => MarketDataPublisher
using MarketUpdateQueue = LL::LFQueue<OMEMarketUpdate>;
// MarketDataPublisher => public exchange clients
using MDPMarketUpdateQueue = LL::LFQueue<MDPMarketUpdate>;
}
