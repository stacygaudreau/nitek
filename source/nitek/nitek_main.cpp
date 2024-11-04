#include <iostream>
#include <memory>
#include <csignal>
#include "exchange/orders/order_matching_engine.h"
#include "llbase/logging.h"


std::unique_ptr<LL::Logger> logger{ nullptr };
std::unique_ptr<Exchange::OrderMatchingEngine> ome{ nullptr };

void shutdown_handler(int) {
    // unix-style SIGINT sends process here to do cleanup
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(5s);
    logger = nullptr;
    ome = nullptr;
    std::this_thread::sleep_for(5s);
    exit(EXIT_SUCCESS);
}

int main(int, char**) {
    using namespace Exchange;
    using namespace LL;

    std::signal(SIGINT, shutdown_handler);
    logger = std::make_unique<Logger>("nitek_main.log");
    ClientRequestQueue client_requests{ OME::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_responses{ OME::MAX_CLIENT_UPDATES };
    MarketUpdateQueue market_updates{ OME::MAX_MARKET_UPDATES };

    // start the matching engine
    std::string t_str;
    logger->logf("% <Exchange::%> Starting matching engine...\n",
                 get_time_str(&t_str), __FUNCTION__);
    ome = std::make_unique<OrderMatchingEngine>(&client_requests,
                                                &client_responses,
                                                &market_updates);
    ome->start();
    // main exchange superloop
    const int t_sleep{ 100 * 1000 };
    while (true) {
        logger->logf("% <Exchange::%> Sleeping for some ms...\n",
                     get_time_str(&t_str), __FUNCTION__);
        usleep(t_sleep);    // sleep which can be terminated by a SIGINT/etc.
    }
}
