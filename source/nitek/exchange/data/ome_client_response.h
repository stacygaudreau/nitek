/**
 *  
 *  Nitek - Exchange
 *
 *  @file ome_client_response.h
 *  @brief Messages passed from the OME to Order Server
 *  @author Stacy Gaudreau
 *  @date 2024.05.08
 *
 */

#pragma once


#include <sstream>
#include <string>
#include "nitek/common/types.h"
#include "llbase/lfqueue.h"

using namespace Common;

namespace Exchange
{
/*
 * Tightly packed bit alignment is forced
 * with the use of #pragma pack(). Our priority here
 * is not to have potential compiler speed optimisations
 * (the default) but rather to pack everything as tight
 * as possible so that orders etc. travel through queues
 * and the network faster, using less bandwidth.
 */
#pragma pack(push, 1)   // 1-byte bit alignment


/**
 * @brief Message from the OME to a client. This
 * response is passed to the Order Server which encodes
 * and forwards to the market participant.
 */
struct OMEClientResponse {
    enum class Type : uint8_t {
        INVALID = 0,
        ACCEPTED = 1,
        CANCELLED = 2,
        FILLED = 3,
        CANCEL_REJECTED = 4,
    };

    Type type{ Type::INVALID };                 // message type
    ClientID client_id{ ClientID_INVALID };     // client the message is for
    TickerID ticker_id{ TickerID_INVALID };     // ticker of product
    OrderID client_order_id{ OrderID_INVALID }; // order id from orig. request
    OrderID market_order_id{ OrderID_INVALID }; // market-wide published order id
    Side side{ Side::INVALID };                 // buy or sell
    Price price{ Price_INVALID };               // price of order
    Qty qty_exec{ Qty_INVALID };                // executed quantity
    Qty qty_remain{ Qty_INVALID };              // remaining quantity

    bool operator==(OMEClientResponse const&) const = default;

    inline static std::string type_to_str(Type type) {
        switch (type) {
        case Type::ACCEPTED:
            return "ACCEPTED";
        case Type::CANCELLED:
            return "CANCELLED";
        case Type::FILLED:
            return "FILLED";
        case Type::CANCEL_REJECTED:
            return "CANCEL_REJECTED";
        case Type::INVALID:
            return "INVALID";
        }
        return "UNKNOWN";
    }

    auto to_str() const {
        std::stringstream ss;
        ss << "<OMEClientResponse>"
           << " ["
           << "type: " << type_to_str(type)
           << ", client: " << client_id_to_str(client_id)
           << ", ticker: " << ticker_id_to_str(ticker_id)
           << ", oid_client: " << order_id_to_str(client_order_id)
           << ", oid_market: " << order_id_to_str(market_order_id)
           << ", side: " << side_to_str(side)
           << ", qty_exec: " << qty_to_str(qty_exec)
           << ", qty_remain: " << qty_to_str(qty_remain)
           << ", price: " << price_to_str(price)
           << "]";
        return ss.str();
    }
};


/**
 * @brief An order response sent from the Order
 * Gateway Server to a market participant.
 */
struct OGSClientResponse {
    size_t n_seq{ 0 };
    OMEClientResponse ome_response;

    auto to_str() const {
        std::stringstream ss;
        ss << "<OGSClientResponse>"
           << " ["
           << "n: " << n_seq
           << " " << ome_response.to_str()
           << "]";
        return ss.str();
    }
};


#pragma pack(pop)       // back to default bit alignment

// OrderMatchingEngine => OrderServer
using ClientResponseQueue = LL::LFQueue<OMEClientResponse>;
}
