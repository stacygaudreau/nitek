#include <iostream>

#include "nitek/exchange/data/ome_client_request.h"
#include "nitek/exchange/data/ome_client_response.h"
#include "nitek/exchange/data/ome_market_update.h"


using namespace Exchange;

int main() {
    std::cout << "nitek::main()\n";
    std::cout << "MEClientReq size: " << sizeof(OMEClientRequest) << "\n";
    std::cout << "MECReq Type size: " << sizeof(OMEClientRequest::Type) << "\n";
}
