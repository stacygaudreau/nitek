#include "gtest/gtest.h"
#include "llbase/tcp_socket.h"
#include "llbase/tcp_server.h"
#include "llbase/sockets.h"

#include <string>
#include <iostream>


using namespace LL;


// base tests for TCPServer module
class TCPServerBasics : public ::testing::Test {
protected:
    std::string logfile{ "tcp_server_tests.log" };
    std::unique_ptr<Logger> logger;
    static const int PORT{ 12345 };     // port to run test sockets on
    std::string IP{ "127.0.0.1" };   // ip address for test sockets
    std::string IFACE{ "lo" };        // interface name for test sockets

    void SetUp() override {
        logger = std::make_unique<Logger>(logfile);
    }

    void TearDown() override {
    }
};


TEST_F(TCPServerBasics, is_constructed) {
    // server is constructed and has basic properties
    auto server = std::make_unique<TCPServer>(*logger);
    ASSERT_NE(server, nullptr);
    ASSERT_NE(&server->get_socket(), nullptr);
}

TEST_F(TCPServerBasics, enters_listening_mode) {
    // server enters epoll listening mode and returns a valid fd
    auto server = std::make_unique<TCPServer>(*logger);
    server->listen(IFACE, PORT);
    ASSERT_NE(-1, server->get_fd_epoll());
}

TEST_F(TCPServerBasics, accepts_new_rx_client) {
    // server.poll() finds and adds new TCPSocket rx client
    using namespace std::literals::chrono_literals;
    auto server = std::make_unique<TCPServer>(*logger);
    server->listen(IFACE, PORT);
    ASSERT_EQ(server->get_rx_sockets().size(), 0);  // no rx client sockets yet
    auto client = std::make_unique<TCPSocket>(*logger);
    client->connect(IP, IFACE, PORT, false);
    server->poll(); // polling should find the client
    std::this_thread::sleep_for(50ms);
    ASSERT_EQ(server->get_rx_sockets().size(), 1);  // should be listening for client
}

TEST_F(TCPServerBasics, accepts_multiple_new_rx_clients) {
    // multiple new clients are discovered from server.poll()
    using namespace std::literals::chrono_literals;
    auto server = std::make_unique<TCPServer>(*logger);
    server->listen(IFACE, PORT);
    ASSERT_EQ(server->get_rx_sockets().size(), 0);  // no rx client sockets yet
    auto client1 = std::make_unique<TCPSocket>(*logger);
    auto client2 = std::make_unique<TCPSocket>(*logger);
    client1->connect(IP, IFACE, PORT, false);
    client2->connect(IP, IFACE, PORT, false);
    server->poll(); // polling should find the clients
    std::this_thread::sleep_for(50ms);
    ASSERT_EQ(server->get_rx_sockets().size(), 2);  // multiple clients should exist
}

TEST_F(TCPServerBasics, receives_data_from_client) {
    // the server receives some test data from a connected client socket
    //  and all respective rx callback methods are executed to indicate
    //  receive activity
    int res_server_rx{ 0 };
    int res_server_rx_done{ 0 };
    using namespace std::literals::chrono_literals;
    auto server = std::make_unique<TCPServer>(*logger);
    server->listen(IFACE, PORT);
    auto client = std::make_unique<TCPSocket>(*logger);
    ASSERT_EQ(server->get_rx_sockets().size(), 0);  // no listening sockets yet
    // set callback methods to set a return value under test
    server->set_rx_callback([&](TCPSocket* socket, Nanos t_rx) {
        std::cout << "Server: rx callback\n";
        logger->logf("<TCPServer::rx_callback> TCP message received socket: % size: % time: %\n",
                     socket->fd, socket->i_rx_next, t_rx);
        res_server_rx = 700;
    });
    server->set_rx_done_callback([&]() {
        res_server_rx_done = 800;
        std::cout << "Server: rx done callback\n";
        logger->logf("<TCPServer::rx_done_callback> server rx done\n");
    });
    client->connect(IP, IFACE, PORT, false);
    server->poll(); // polling should find the client
    ASSERT_EQ(server->get_rx_sockets().size(), 1);  // should be listening for client
    // send some test data
    std::string str_test{ "I am test data" };
    client->load_tx(str_test.data(), str_test.size());
    client->tx_and_rx();
    std::this_thread::sleep_for(50ms);
    // receive data at server
    server->poll();
    server->tx_and_rx();
    EXPECT_EQ(res_server_rx, 700);
    EXPECT_EQ(res_server_rx_done, 800);
}

TEST_F(TCPServerBasics, multiple_clients_communicate) {
    /*
     * multiple clients connect, send and receive
     * messages from a TCPServer instance
    */
    // messages the server received
    std::vector<std::string> server_rx_messages{ };
    // messages each client received
    std::vector<std::vector<std::string>> client_rx_messages;
    client_rx_messages.resize(5, std::vector<std::string>{ });
    // mapping of client number to socket file descriptor
    std::vector<int> n_client_to_fd{ };
    // messages to send from clients->server
    std::vector<std::string> messages{ };
    for (int x{ }; x < 5; ++x) {
        for (int i{ }; i < 5; ++i) {
            messages.push_back({ "client[" + std::to_string(i) + "]: "
                                         + std::to_string(x * 100 + i) });
        }
    }

    // construct a server, its receive callback and bind to an address
    auto server = std::make_unique<TCPServer>(*logger);
    server->set_rx_callback([&](TCPSocket* socket, Nanos t_rx) {
        // the server responds by sending a message back after receiving from the client
        std::cout << "server: rx_callback\n";
        logger->logf("<Server::rx_callback> server received message at socket: % size: % "
                     "time: %\n",
                     socket->fd, socket->i_rx_next, t_rx);
        const auto msg = std::string(socket->rx_buffer.data(), socket->i_rx_next);
        server_rx_messages.push_back(msg);
        const auto reply = "server->" + msg;
        logger->logf("\t-> (server) message received: %\n", msg);
        socket->i_rx_next = 0;
        socket->load_tx(reply.data(), reply.length());
    });
    server->listen(IFACE, PORT);

    // client logs the message received in its callback
    auto client_rx_callback = [&](TCPSocket* socket, Nanos t_rx) {
        std::cout << "client: rx_callback\n";
        auto msg = std::string(socket->rx_buffer.data(), socket->i_rx_next);
        // find the client number matching this socket's fd
        size_t n{ };
        for (; n < n_client_to_fd.size(); ++n) {
            if (socket->fd == n_client_to_fd.at(n))
                break;
        }
        // log the message received
        client_rx_messages.at(n).push_back(msg);
        socket->i_rx_next = 0;
        logger->logf(
                "<TCPSocket::rx_callback> client received message at socket: % size: % time: %\n",
                socket->fd, socket->i_rx_next, t_rx);
        logger->logf("\t-> message received: %\n", msg);
    };
    // make 5 clients and connect them each to the server
    std::vector<std::unique_ptr<TCPSocket>> clients(5);
    for (size_t i{ 0 }; i < clients.size(); ++i) {
        clients[i] = std::make_unique<TCPSocket>(*logger);
        clients[i]->rx_callback = client_rx_callback;
        auto fd = clients[i]->connect(IP, IFACE, PORT, false);
        logger->logf("client[%] connected on fd: %, iface: %, address: %:%",
                     i, fd, IFACE, IP, PORT);
        n_client_to_fd.push_back(fd);   // cache fd for later recall
        server->poll(); // poll connects client socket on server
    }

    // send a message from each client to the server
    using namespace std::literals::chrono_literals;
    size_t i_msg{ 0 };
    for (int x{ }; x < 5; ++x) {
        for (size_t i{ }; i < clients.size(); ++i) {
            const auto msg_to_send = messages.at(i_msg);
            logger->logf("client[%] sending message: %\n", i, msg_to_send);
            clients[i]->load_tx(msg_to_send.data(), msg_to_send.length());
            clients[i]->tx_and_rx();
            // wait for tx/rx
            std::this_thread::sleep_for(25ms);
            server->poll();
            server->tx_and_rx();
            ++i_msg;
        }
    }

    // verify all messages received by server
    for (size_t i{ }; i < messages.size(); ++i) {
        EXPECT_EQ(messages.at(i), server_rx_messages.at(i));
    }

    // verify all messages received by each client
    //  the last 5 messages (eg: 400, 401, etc..) are not
    //  received in this test; hence x < 4
    for (size_t x{ }; x < 4; ++x) {
        for (size_t i{ }; i < clients.size(); ++i) {
            const auto expected = std::string{ "server->client["
                                                       + std::to_string(i) + "]: "
                                                       + std::to_string(x * 100 + i) };
            EXPECT_EQ(expected, client_rx_messages.at(i).at(x));
        }
    }
}