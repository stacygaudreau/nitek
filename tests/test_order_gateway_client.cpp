#include "gtest/gtest.h"
#include <memory>
#include <vector>
#include "exchange/networking/fifo_sequencer.h"
#include "exchange/data/ome_client_request.h"
#include "exchange/data/ome_client_response.h"
#include "llbase/timekeeping.h"
#include "llbase/tcp_socket.h"
#include "client/networking/order_gateway_client.h"
#include "exchange/networking/order_gateway_server.h"

using namespace Client;
using namespace Common;


// base tests for Order Gateway Client
class OrderGatewayClientBasics : public ::testing::Test {
protected:
    Exchange::ClientRequestQueue requests_from_TE{ Exchange::Limits::MAX_CLIENT_UPDATES };
    Exchange::ClientResponseQueue responses_to_TE{ Exchange::Limits::MAX_CLIENT_UPDATES };
    std::string IFACE{ "lo" };
    int PORT{ 12345 };
    std::string IP{ "127.0.0.1" };
    ClientID client_id{ 0 };

    void SetUp() override {
    }

    void TearDown() override {
    }
};


TEST_F(OrderGatewayClientBasics, is_constructed) {
    auto ogc = std::make_unique<OrderGatewayClient>(client_id,
                                                    requests_from_TE,responses_to_TE,
                                                    IP, IFACE, PORT);
    EXPECT_NE(ogc, nullptr);
    EXPECT_EQ(ogc->port, PORT);
    EXPECT_EQ(ogc->client_id, client_id);
    EXPECT_EQ(ogc->iface, IFACE);
    EXPECT_EQ(ogc->ip, IP);
}

TEST_F(OrderGatewayClientBasics, starts_and_stops_worker_thread) {
    // the OGC manages its worker thread's lifecycle
    auto ogc = std::make_unique<OrderGatewayClient>(client_id,
                                                    requests_from_TE,responses_to_TE,
                                                    IP, IFACE, PORT);
    EXPECT_NE(ogc, nullptr);
    EXPECT_FALSE(ogc->is_running);
    ogc->start();
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);
    EXPECT_TRUE(ogc->is_running);
    ogc->stop();
    std::this_thread::sleep_for(10ms);
    EXPECT_FALSE(ogc->is_running);
}


using namespace std::literals::chrono_literals;

// tests which pass orders and responses through the order gateway client to the exchange server
class OrderGatewayClientOrders : public ::testing::Test {
protected:
    std::string IFACE{ "lo" };
    int PORT{ 12345 };
    std::string IP{ "127.0.0.1" };
    ClientID client_id{ 0 };
    LL::Logger logger{ "order_gateway_client_server_socket_tests.log" };
    // client OrderGatewayClient under test
    std::unique_ptr<OrderGatewayClient> ogc;
    Exchange::ClientRequestQueue requests_from_TE{ Exchange::Limits::MAX_CLIENT_UPDATES };
    Exchange::ClientResponseQueue responses_to_TE{ Exchange::Limits::MAX_CLIENT_UPDATES };
    // exchange OrderGatewayServer
    std::unique_ptr<Exchange::OrderGatewayServer> ogs;
    Exchange::ClientRequestQueue requests_to_OME{ Exchange::Limits::MAX_CLIENT_UPDATES };
    Exchange::ClientResponseQueue responses_from_OME{ Exchange::Limits::MAX_CLIENT_UPDATES };

    LL::TCPSocket socket_srv{ logger };
    std::vector<Exchange::OMEClientRequest> requests; // request test data
    size_t N_REQS{ 10 };

    void SetUp() override {
        ogs = std::make_unique<Exchange::OrderGatewayServer>(requests_to_OME,
                                                             responses_from_OME, IFACE, PORT);
        std::this_thread::sleep_for(10ms);
        ogs->start();
        ogc = std::make_unique<OrderGatewayClient>(client_id, requests_from_TE,
                                                   responses_to_TE, IP, IFACE, PORT);
        ogc->start();
        for (size_t i{ }; i < N_REQS; ++i) {
            using Request = Exchange::OMEClientRequest;
            auto req = Request{ Request::Type::NEW, client_id,
                                (TickerID) i % 2, i + 1,
                                Side::BUY, 100 + Price(i), 50 + Qty(i) };
            requests.push_back(req);
        }
    }

    void TearDown() override {
    }
};


TEST_F(OrderGatewayClientOrders, forwards_order_request_to_exchange) {
    /*
     * an order request is received from the Trading Engine queue and forwarded
     * to the exchange server over TCP
     */
    // at first there are no requests outgoing to the matching engine
    EXPECT_EQ(requests_to_OME.size(), 0);
    // push an order request into the gateway client
    auto req = requests_from_TE.get_next_to_write();
    *req = requests.at(0);
    requests_from_TE.increment_write_index();
    std::this_thread::sleep_for(10ms);
    // the exchange should have received the request and forwarded to the matching engine queue
    EXPECT_NE(requests_to_OME.size(), 0);
    auto rx_req = requests_to_OME.get_next_to_read();
    EXPECT_EQ(*rx_req, *req);
}

TEST_F(OrderGatewayClientOrders, receives_response_from_exchange) {
    /*
     * a response for an order request is received from the exchange server and
     * forwarded to the Trading Engine queue
     */
    // there are no responses in the queue at first
    EXPECT_EQ(responses_to_TE.size(), 0);
    // dummy request data must be sent to the exchange; else the gateway server does
    // not know the client and will not create a socket for listening to the client on
    *requests_from_TE.get_next_to_write() = requests.at(0);
    requests_from_TE.increment_write_index();
    std::this_thread::sleep_for(10ms);
    // the exchange server transmits a response
    using Response = Exchange::OMEClientResponse;
    auto tx_res = Response{ Response::Type::ACCEPTED,
                            client_id, 0, 0,
                            0, Side::BUY,
                            100, 50, 0 };
    *responses_from_OME.get_next_to_write() = tx_res;
    responses_from_OME.increment_write_index();
    std::this_thread::sleep_for(10ms);
    // the gateway client should have received the appropriate response now
    auto rx_res = responses_to_TE.get_next_to_read();
    EXPECT_EQ(*rx_res, tx_res);
}

TEST_F(OrderGatewayClientOrders, sends_multiple_requests) {
    /*
     * a series of orders are sent to the exchange and they are processed by the gateway
     * in the correct order
     */


}