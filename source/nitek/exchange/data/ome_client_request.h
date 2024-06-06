/**
 *  
 *  Nitek - Exchange
 *
 *  @file ome_client_request.h
 *  @brief Requests passed to the OME from the Order Server
 *  on behalf of market participants.
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
 * @brief Order requests passed from the Order Server to
 * the OME on behalf of a client/market participant.
 */
struct OMEClientRequest {
    enum class Type : uint8_t {
        INVALID = 0,
        NEW = 1,
        CANCEL = 2
    };

    Type type{ Type::INVALID };             // request message type
    ClientID client_id{ ClientID_INVALID }; // client the message is from
    TickerID ticker_id{ TickerID_INVALID }; // ticker of product
    OrderID order_id{ OrderID_INVALID };    // new or existing order id
    Side side{ Side::INVALID };             // buy or sell
    Price price{ Price_INVALID };           // price of order
    Qty qty{ Qty_INVALID };                 // quantity

    inline static std::string type_to_str(Type type) {
        switch (type) {
        case Type::NEW:
            return "NEW";
        case Type::CANCEL:
            return "CANCEL";
        case Type::INVALID:
            return "INVALID";
        }
        return "UNKNOWN";
    }

    auto to_str() const {
        std::stringstream ss;
        ss << "<OMEClientRequest>"
           << " ["
           << "type: " << type_to_str(type)
           << ", client: " << client_id_to_str(client_id)
           << ", ticker: " << ticker_id_to_str(ticker_id)
           << ", oid: " << order_id_to_str(order_id)
           << ", side: " << side_to_str(side)
           << ", qty: " << qty_to_str(qty)
           << ", price: " << price_to_str(price)
           << "]";
        return ss.str();
    }
};


/**
 * @brief An order request sent from a public
 * exchange client to the Order Gateway Server.
 */
struct OGSClientRequest {
    size_t n_seq{ 0 };
    OMEClientRequest ome_request;

    auto to_str() const {
        std::stringstream ss;
        ss << "<OGSClientRequest>"
           << " ["
           << "n: " << n_seq
           << " " << ome_request.to_str()
           << "]";
        return ss.str();
    }
};


#pragma pack(pop)       // back to default bit alignment

// OrderServer => OrderMatchingEngine
using ClientRequestQueue = LL::LFQueue<OMEClientRequest>;
}