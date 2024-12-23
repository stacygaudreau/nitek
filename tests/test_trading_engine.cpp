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
    std::vector<Update> updates;

    void SetUp() override {
        te = std::make_unique<TradingEngine>(CLIENT, TradeAlgo::MARKET_MAKER, te_confs,
                                             tx_requests, rx_responses, rx_updates);
        for (size_t i{}; i < N_MESSAGES; ++i) {
            auto side = i % 2 == 0 ? Side::BUY : Side::SELL;
            auto price = side == Side::BUY ? 100 : 80;
            // add some market updates to the order book for the ticker
            updates.emplace_back(Update(Update::Type::ADD, i,
                                        TICKER, side, price, 50,
                                        1));
        }
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

TEST_F(TradingEngineBasics, on_order_book_update) {
    /*
     *  the on_order_book_update() method updates the feature engine
     *  and position manager. it also fires the order_book_update callback
     */
    bool callback_is_executed{ false };
    const auto& book = te->book_for_ticker.at(TICKER);
    for (auto& update: updates) {
        auto order = book->order_pool.allocate(update.order_id, update.side,
                                               update.price, update.qty, update.priority,
                                               nullptr,nullptr);
        book->add_order(order);
    }
    auto cb = [&](auto ticker, auto price, auto side, auto& ob) {
        (void) ticker;
        (void) price;
        (void) side;
        (void) ob;
        callback_is_executed = true;
    };
    te->on_order_book_update_callback = cb;
    auto& market_price = te->feng.market_price;
    auto price_prev = market_price;
    te->on_order_book_update(TICKER, 80, Side::BUY, *book);
    EXPECT_NE(te->feng.market_price, price_prev);
    EXPECT_TRUE(callback_is_executed);
}

TEST_F(TradingEngineBasics, on_trade_update) {
    /*
     *  the on_trade_update() method updates the feature engine
     *  and fires the on_trade_update callback
     */
    bool callback_is_executed{ false };
    const auto& book = te->book_for_ticker.at(TICKER);
    for (auto& update: updates) {
        auto order = book->order_pool.allocate(update.order_id, update.side,
                                               update.price, update.qty, update.priority,
                                               nullptr,nullptr);
        book->add_order(order);
    }
    auto cb = [&](auto& update, auto& ob) {
        (void) update;
        (void) ob;
        callback_is_executed = true;
    };
    te->on_trade_update_callback = cb;
    auto& trade_pressure = te->feng.aggressive_trade_qty_ratio;
    auto trade_pressure_prev = trade_pressure;
    te->on_trade_update(updates.at(1), *book);
    EXPECT_NE(trade_pressure, trade_pressure_prev);
    EXPECT_TRUE(callback_is_executed);
}

TEST_F(TradingEngineBasics, on_order_response) {
    /*
     *  the on_order_response() method updates adds a fill to a
     *  position in the position manager and fires the on_order_response
     *  callback
     */
    bool callback_is_executed{ false };
    auto cb = [&](auto& response) {
        (void) response;
        callback_is_executed = true;
    };
    te->on_order_response_callback = cb;
    auto& position = te->pman.get_position(TICKER).position;
    auto position_prev = position;
    auto response = Response(Response::Type::FILLED, CLIENT, TICKER,
                             0, 0, Side::BUY, 100, 100, 50);
    te->on_order_response(response);
    EXPECT_NE(position, position_prev);
    EXPECT_TRUE(callback_is_executed);
}
