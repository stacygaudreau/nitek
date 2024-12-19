#include "trading_engine.h"

namespace Client
{

TradingEngine::TradingEngine(ClientID client_id,
                             TradeAlgo algo,
                             TradeEngineConfByTicker conf_by_ticker,
                             Exchange::ClientRequestQueue& tx_requests,
                             Exchange::ClientResponseQueue& rx_responses,
                             Exchange::MarketUpdateQueue& rx_updates)
        : client_id(client_id),
          tx_requests(tx_requests),
          rx_responses(rx_responses),
          rx_updates(rx_updates),
          logger("client_trading_engine_"
                 + client_id_to_str(client_id) + ".log"),
          feng(logger),
          pman(logger),
          oman(*this, rman, logger),
          rman(pman, conf_by_ticker, logger) {

    for (size_t ticker{ }; ticker < book_for_ticker.size(); ++ticker) {
        book_for_ticker[ticker] = std::make_unique<TEOrderBook>(ticker, logger);
        book_for_ticker[ticker]->set_trading_engine(this);
    }

    on_order_book_update_callback = [&](auto ticker, auto price, auto side, auto& book) {
        default_on_order_book_update_callback(ticker, price, side, book);
    };
    on_trade_update_callback = [&](auto& update, auto& ob) {
        default_on_trade_update_callback(update, ob);
    };
    on_order_response_callback = [&](auto& response) {
        default_on_order_response_callback(response);
    };

    // the algorithm type specified selects which to instantiate
    if (algo == Common::TradeAlgo::MARKET_MAKER) {
        maker_algo = std::make_unique<MarketMaker>(*this, feng, oman, conf_by_ticker, logger);
    } else if (algo == Common::TradeAlgo::LIQ_TAKER) {
        // todo: make liquidity taker algo
    }

    for (TickerID t{ }; t < conf_by_ticker.size(); ++t) {
        logger.logf("% <TE::%> init % algo for ticker: %, %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, trade_algo_to_str(algo),
                    ticker_id_to_str(t), conf_by_ticker.at(t).to_str());
    }
}

void TradingEngine::start() {
    is_running = true;
    thread = LL::create_and_start_thread(-1, "TradingEngine", [this]() { run(); });
    ASSERT(thread != nullptr, "<TE> failed to start thread for trading engine");
}

void TradingEngine::stop() {
    logger.logf("% <TE::%> stopping trading engine...\n",
                LL::get_time_str(&t_str), __FUNCTION__);

    while (rx_responses.size() || rx_updates.size()) {
        logger.logf("% <TE::%> process remaining order data before stop: rx_res: %, rx_update: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, rx_responses.size(),
                    rx_updates.size());
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(10ms);
    }

    logger.logf("% <TE::%> Position Manager\n%\n",
                LL::get_time_str(&t_str), __FUNCTION__, pman.to_str());

    is_running = false;
    if (thread != nullptr && thread->joinable())
        thread->join();
}

void TradingEngine::run() noexcept {
    // handle incoming order responses and market updates to generate trade order requests
    logger.logf("% <TE::%> run trading engine...\n",
                LL::get_time_str(&t_str), __FUNCTION__);
    while (is_running) {
        // order response handling
        for (auto response = rx_responses.get_next_to_read();
             response; response = rx_responses.get_next_to_read()) {
            logger.logf("% <TE::%> rx %\n",
                        LL::get_time_str(&t_str), __FUNCTION__,
                        response->to_str().c_str());
            on_order_response_callback(*response);
            rx_responses.increment_read_index();
            t_last_rx_event = LL::get_time_nanos();
        }
        // incoming market updates
        for (auto update = rx_updates.get_next_to_read();
             update; update = rx_updates.get_next_to_read()) {
            logger.logf("% <TE::%> rx %\n",
                        LL::get_time_str(&t_str), __FUNCTION__,
                        update->to_str().c_str());
            // assuming ASSERT() is removed at runtime, doing bounds checking in an assertion
            // instead of using book_for_ticker .at() saves a bit of runtime latency
            ASSERT(update->ticker_id < book_for_ticker.size(), "out of bounds ticker ID!");
            book_for_ticker[update->ticker_id]->on_market_update(*update);
            rx_updates.increment_read_index();
            t_last_rx_event = LL::get_time_nanos();
        }
    }
}
}