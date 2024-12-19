#include "gtest/gtest.h"
#include <vector>
#include "common/types.h"
#include "nitek/client/trading/trading_engine.h"

using namespace Client;
using namespace Common;
using Response = Exchange::OMEClientResponse;
using Update = Exchange::OMEMarketUpdate;
using Request = Exchange::OMEClientRequest;

using namespace std::literals::chrono_literals;

// basic tests for the Trading Engine module
class TradingEngineBasics : public ::testing::Test {
protected:
    ClientID CLIENT{ 1 };
    TickerID TICKER{ 3 };
    static constexpr size_t N_MESSAGES{ 5 };
    Exchange::ClientRequestQueue tx_requests{ Exchange::Limits::MAX_CLIENT_UPDATES };
    Exchange::ClientResponseQueue rx_responses{ Exchange::Limits::MAX_CLIENT_UPDATES };
    Exchange::MarketUpdateQueue rx_updates{ Exchange::Limits::MAX_MARKET_UPDATES };
    TradeEngineConfByTicker te_confs;
    std::unique_ptr<TradingEngine> te{ nullptr };

    void SetUp() override {
        te = std::make_unique<TradingEngine>(CLIENT, TradeAlgo::MARKET_MAKER, te_confs,
                                             tx_requests, rx_responses, rx_updates);
    }

    void TearDown() override {
    }
};

TEST_F(TradingEngineBasics, is_constructed) {
    EXPECT_NE(te, nullptr);
}

TEST_F(TradingEngineBasics, starts_and_stops_worker_thread) {
    te->start();
    EXPECT_TRUE(te->is_running);
    std::this_thread::sleep_for(20ms);
    te->stop();
    EXPECT_FALSE(te->is_running);
}

TEST_F(TradingEngineBasics, run_consumes_responses) {
    // order responses are consumed by the worker thread and passed to the callback
    for (size_t i{}; i < N_MESSAGES; ++i) {
        auto res = te->rx_responses.get_next_to_write();
        *res = Response(Response::Type::FILLED, CLIENT, TICKER, i, i, Side::BUY, 100 + i,
                        i+10, 50 - i+10);
        te->rx_responses.increment_write_index();
    }
    size_t n_rx_msgs{ 0 };
    te->on_order_response_callback = [&](auto& response) {
        (void) response;
        n_rx_msgs++;
    };
    te->start();
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(n_rx_msgs, N_MESSAGES);
    EXPECT_EQ(te->rx_responses.size(), 0);
}

TEST_F(TradingEngineBasics, run_consumes_updates) {
    // order updates are consumed by the worker thread and added to the ticker's book
    for (size_t i{}; i < N_MESSAGES; ++i) {
        auto update = te->rx_updates.get_next_to_write();
        *update = Update(Update::Type::ADD, i, TICKER,
                         Side::BUY, 100 + i, 50 + i, i);
        te->rx_updates.increment_write_index();
    }
    te->start();
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(te->rx_updates.size(), 0);
    EXPECT_EQ(te->book_for_ticker.at(TICKER)->order_pool.get_n_blocks_used(), N_MESSAGES);
}
