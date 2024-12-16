/**
 *
 *  Nitek - Client
 *
 *  @file trading_engine.h
 *  @brief Module which handles running trading algorithms
 *  @author Stacy Gaudreau
 *  @date 2024.12.09
 *
 */

#pragma once

#include "llbase/macros.h"
#include "llbase/logging.h"
#include "common/types.h"
#include "nitek/client/trading/om_order.h"
#include "exchange/data/ome_client_request.h"
#include "exchange/data/ome_client_response.h"
#include <string>
#include <sstream>
#include <array>

using namespace Common;

namespace Client
{
class TradingEngine {
public:
    explicit TradingEngine(ClientID client_id)
            : client_id(client_id) {

    }

    [[nodiscard]] inline ClientID get_client_id() const { return client_id; }

PRIVATE_IN_PRODUCTION
    ClientID client_id{ ClientID_INVALID };

DELETE_DEFAULT_COPY_AND_MOVE(TradingEngine)
};
}