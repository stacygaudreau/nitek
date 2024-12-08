#include "gtest/gtest.h"
#include <memory>
#include <vector>
#include "exchange/networking/order_gateway_server.h"
#include "exchange/networking/fifo_sequencer.h"
#include "exchange/data/ome_client_request.h"
#include "exchange/data/ome_client_response.h"
#include "llbase/timekeeping.h"
#include "llbase/tcp_socket.h"


using namespace Exchange;


// base tests for Order Gateway Server
class OrderGatewayServerBasics : public ::testing::Test {
protected:
    ClientRequestQueue client_request_queue{ Limits::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ Limits::MAX_CLIENT_UPDATES };
    std::string IFACE{ "lo" };
    int PORT{ 12345 };     // port to run tests on
    void SetUp() override {
    }
    void TearDown() override {
    }
};


TEST_F(OrderGatewayServerBasics, is_constructed) {
    auto ogs = std::make_unique<OrderGatewayServer>(client_request_queue,
                                                    client_response_queue,
                                                    IFACE, PORT);
    EXPECT_NE(ogs, nullptr);
}

TEST_F(OrderGatewayServerBasics, starts_and_stops_worker_thread) {
    // the OGS manages its worker thread's lifecycle
    auto ogs = std::make_unique<OrderGatewayServer>(client_request_queue,
                                                    client_response_queue,
                                                    IFACE, PORT);
    ASSERT_NE(ogs, nullptr);
    EXPECT_FALSE(ogs->get_is_running());
    ogs->start();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);
    ASSERT_TRUE(ogs->get_is_running());
    ogs->stop();
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(ogs->get_is_running());
}


// FIFO sequencer base tests
class FIFOSequencerBasics : public ::testing::Test {
protected:
    ClientRequestQueue client_request_queue{ Limits::MAX_CLIENT_UPDATES };
    LL::Logger logger{ "fifo_tests.log" };

    OMEClientRequest request{ OMEClientRequest::Type::NEW,
                              1, 3, 1, Side::BUY,
                              100, 50 };
    std::unique_ptr<FIFOSequencer> fifo;

    void SetUp() override {
        fifo = std::make_unique<FIFOSequencer>(client_request_queue, logger);
    }

    void TearDown() override {
    }
};


TEST_F(FIFOSequencerBasics, is_constructed) {
    EXPECT_NE(fifo, nullptr);
}

TEST_F(FIFOSequencerBasics, pushes_client_request) {
    // a client order request is pushed into the FIFO's request queue
    ASSERT_NE(fifo, nullptr);
    auto t = LL::get_time_nanos();
    fifo->push_client_request(request, t);
    auto rqs = fifo->get_pending_requests();
    auto req = rqs.at(0).request;
    EXPECT_EQ(req, request);
    EXPECT_EQ(rqs.at(0).t_rx, t);
}

TEST_F(FIFOSequencerBasics, sequences_and_publishes_single_request) {
    // a single request is sequenced and published onto the request queue
    ASSERT_NE(fifo, nullptr);
    auto t = LL::get_time_nanos();
    fifo->push_client_request(request, t);
    fifo->sequence_and_publish();
    auto req = client_request_queue.get_next_to_read();
    EXPECT_EQ(*req, request);
}

TEST_F(FIFOSequencerBasics, sequences_and_publishes_multiple_requests) {
    // multiple out-of-order requests are sequenced and
    // published in the correct order in the request queue
    ASSERT_NE(fifo, nullptr);
    auto t0 = LL::get_time_nanos();
    auto t1 = t0 + 2;
    auto t2 = t1 + 2;
    auto t3 = t2 + 2;
    OMEClientRequest request1{ OMEClientRequest::Type::NEW, 2, 4, 1, Side::BUY, 200, 50 };
    OMEClientRequest request2{ OMEClientRequest::Type::NEW, 3, 5, 1, Side::BUY, 300, 75 };
    OMEClientRequest request3{ OMEClientRequest::Type::NEW, 4, 6, 1, Side::BUY, 400, 100 };
    fifo->push_client_request(request2, t2);
    fifo->push_client_request(request, t0);
    fifo->push_client_request(request1, t1);
    fifo->push_client_request(request3, t3);
    fifo->sequence_and_publish();
    // request at t0
    auto req = client_request_queue.get_next_to_read();
    EXPECT_EQ(*req, request);
    client_request_queue.increment_read_index();
    // request1 at t1
    req = client_request_queue.get_next_to_read();
    EXPECT_EQ(*req, request1);
    client_request_queue.increment_read_index();
    // request2 at t2
    req = client_request_queue.get_next_to_read();
    EXPECT_EQ(*req, request2);
    client_request_queue.increment_read_index();
    // request3 at t3
    req = client_request_queue.get_next_to_read();
    EXPECT_EQ(*req, request3);
    client_request_queue.increment_read_index();
}


// tests to process order requests through the gateway server
class OrderGatewayServerOrders : public ::testing::Test {
protected:
    ClientRequestQueue client_request_queue{ Limits::MAX_CLIENT_UPDATES };
    ClientResponseQueue client_response_queue{ Limits::MAX_CLIENT_UPDATES };
    std::string IFACE{ "lo" };
    int PORT{ 12345 };     // port to run tests on
    std::string IP{ "127.0.0.1" };   // ip address to run tests on
    LL::Logger logger{ "order_gateway_server_orders_tests.log" };
    std::unique_ptr<OrderGatewayServer> ogs;
    std::vector<std::unique_ptr<LL::TCPSocket>> clients;
    std::vector<std::vector<OGSClientRequest>> requests;
    size_t N_REQS{ 5 }, N_CLIENTS{ 5 };

    void SetUp() override {
        ogs = std::make_unique<OrderGatewayServer>(client_request_queue,
                                                   client_response_queue, IFACE, PORT);
        ogs->start();
        for (size_t n{ }; n < N_CLIENTS; ++n) {
            clients.push_back(std::make_unique<LL::TCPSocket>(logger));
            std::vector<OGSClientRequest> reqs{ };
            for (size_t i{ }; i < N_REQS; ++i) {
                reqs.push_back({ i + 1,
                                 { OMEClientRequest::Type::NEW, (ClientID) n + 1,
                                   (TickerID) n + 1, i + 1,
                                   Side::BUY,
                                   100 + Price(i + n), 50 + Qty(i + n) }});
            }
            requests.push_back(reqs);
        }
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(10ms);
    }

    void TearDown() override {
    }
};


TEST_F(OrderGatewayServerOrders, receives_order_from_client) {
    // a client connects over TCP and sends an order request
    // to the gateway. the gateway sends the order down its
    // queue toward the matching engine.
    ASSERT_NE(ogs, nullptr);
    const auto& client = clients.at(0);
    auto fd = client->connect(IP, IFACE, PORT, false);
    ASSERT_GT(fd, 0);
    // transmit an order request from the client
    OGSClientRequest request{ 1, { OMEClientRequest::Type::NEW,
                                   1, 3, 1, Side::BUY,
                                   100, 50 }};
    client->load_tx(&request, sizeof(request));
    client->tx_and_rx();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(500ms);
    // verify request was pushed out of the fifo queue and
    // onto the client request queue
    auto req = client_request_queue.get_next_to_read();
    ASSERT_NE(req, nullptr);
    EXPECT_EQ(*req, request.ome_request);
}

TEST_F(OrderGatewayServerOrders, sends_order_response_to_client) {
    // an order response message is generated for a connected client
    ASSERT_NE(ogs, nullptr);
    const auto& client = clients.at(0);
    auto fd = client->connect(IP, IFACE, PORT, false);
    ASSERT_GT(fd, 0);
    // configure a callback on the client socket to read a response message
    OGSClientResponse* res{ nullptr };
    auto client_rx_callback = [&](LL::TCPSocket* socket, LL::Nanos t_rx) {
        (void) t_rx;
        res = reinterpret_cast<OGSClientResponse*> (socket->rx_buffer.data());
    };
    client->rx_callback = client_rx_callback;
    // transmit an order request from the client
    OGSClientRequest request{ 1, { OMEClientRequest::Type::NEW,
                                   1, 3, 1, Side::BUY,
                                   100, 50 }};
    client->load_tx(&request, sizeof(request));
    client->tx_and_rx();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(500ms);
    // load a response into the queue and send it back to the client
    OMEClientResponse response{ OMEClientResponse::Type::ACCEPTED, request.ome_request.client_id,
                                request.ome_request.ticker_id, request.ome_request.order_id,
                                1, Side::BUY, request.ome_request.price,
                                request.ome_request.qty, request.ome_request.qty };
    auto next = client_response_queue.get_next_to_write();
    *next = response;
    client_response_queue.increment_write_index();
    std::this_thread::sleep_for(500ms);
    client->tx_and_rx();
    std::this_thread::sleep_for(500ms);
    // verify that the response was received back at the client
    EXPECT_EQ(res->ome_response, response);
    EXPECT_EQ(res->n_seq, 1);
}

TEST_F(OrderGatewayServerOrders, receives_multiple_client_orders) {
    /*
     * Multiple clients transmit order requests and the gateway
     * server sequences and dispatches them all to the order
     * matching engine
     */
    ASSERT_NE(ogs, nullptr);
    for (const auto& c: clients) {
        auto fd = c->connect(IP, IFACE, PORT, false);
        EXPECT_GT(fd, 0);
    }
    // transmit all order requests, for each client
    for (size_t r{ }; r < N_REQS; ++r) {
        for (size_t c{ }; c < clients.size(); ++c) {
            clients.at(c)->load_tx(&requests.at(c).at(r), sizeof(requests.at(c).at(r)));
            clients.at(c)->tx_and_rx();
        }
    }
    // verify the correct number of requests are in the queue
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(500ms);
    EXPECT_EQ(client_request_queue.size(), N_REQS * N_CLIENTS);
}