#include "gtest/gtest.h"
#include "llbase/tcp_socket.h"
#include "llbase/sockets.h"

#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <vector>


using namespace LL;


// base tests for TCPSocket module
class TCPSocketBasics : public ::testing::Test {
protected:
    std::string logfile{ "tcp_socket_tests.log" };
    std::unique_ptr<Logger> logger;
    static const int PORT{ 12345 };     // port the test sockets will attach to
    std::string IP{ "127.0.0.1" };   // ip address for test sockets
    std::string IFACE{ "lo" };        // interface name for test sockets

    void SetUp() override {
        logger = std::make_unique<Logger>(logfile);
    }

    void TearDown() override {
    }
};


TEST_F(TCPSocketBasics, is_constructed) {
    // a TCPSocket is constructed
    auto socket = std::make_unique<TCPSocket>(*logger);
    ASSERT_NE(socket, nullptr);
}

TEST_F(TCPSocketBasics, non_listening_socket_connects) {
    // a valid file descriptor is returned by the socket
    // connection method when attaching to localhost in non-listen mode
    auto socket = std::make_unique<TCPSocket>(*logger);
    auto fd = socket->connect(IP, IFACE, 0, false);
    ASSERT_NE(-1, fd);
}

TEST_F(TCPSocketBasics, listening_socket_connects) {
    // a valid file descriptor is returned by the socket
    // connection method when attaching to localhost in listening mode
    auto socket = std::make_unique<TCPSocket>(*logger);
    auto fd = socket->connect(IP, IFACE, 0, true);
    ASSERT_NE(-1, fd);
}

TEST_F(TCPSocketBasics, loading_tx_buffer) {
    // data is loaded into transmission buffer
    auto socket = std::make_unique<TCPSocket>(*logger);
    char d{ 99 };
    socket->load_tx(reinterpret_cast<const void*>(&d), sizeof(d));
    ASSERT_EQ(99, (char) socket->tx_buffer[0]);
    ASSERT_EQ(1, socket->i_tx_next);
}

TEST_F(TCPSocketBasics, binds_and_listens) {
    /*
     * socket listens to local address/port
     */
    // server socket binding and listening for new connections
    auto socket_srv = std::make_unique<TCPSocket>(*logger);
    auto fd_srv = socket_srv->connect(IP, "lo", 0, true);
    // socket fd must be valid
    ASSERT_NE(-1, fd_srv);
    // start listening for connections on the bound socket
    int result = listen(fd_srv, 5);
    ASSERT_NE(-1, result);
}

TEST_F(TCPSocketBasics, accepts_new_connections) {
    /*
     * listening socket accepts new connections
     * a client socket is accepted by a listening and bound socket on a port
     */
    using namespace std::literals::chrono_literals;
    // server socket binding and listening for new connections
    auto socket_srv = std::make_unique<TCPSocket>(*logger);
    auto fd_srv = socket_srv->connect(IP, IFACE, PORT, true);
    ASSERT_NE(-1, fd_srv);
    // start listening for connections on the server socket
    int result = listen(fd_srv, 5);
    ASSERT_NE(-1, result);
    // client socket making a connection to the server
    auto socket_client = std::make_unique<TCPSocket>(*logger);
    auto fd_client = socket_client->connect(IP, IFACE, PORT, false);
    ASSERT_NE(-1, fd_client);
    std::this_thread::sleep_for(10ms);
    // accept pending client connection
    sockaddr_storage client_addr{ };
    socklen_t addr_size = sizeof(client_addr);
    auto fd_rx = accept(fd_srv, (sockaddr*) &client_addr,
                        &addr_size);
    EXPECT_NE(-1, fd_rx);
    if (fd_rx)
        close(fd_rx);
}

TEST_F(TCPSocketBasics, tx_and_rx_transmits) {
    /*
     * a client TCPSocket sends to a listening TCPSocket with its tx_and_rx method
     */
    using namespace std::literals::chrono_literals;
    // server socket binding and listening for new connections
    auto socket_srv = std::make_unique<TCPSocket>(*logger);
    auto fd_srv = socket_srv->connect(IP, IFACE, PORT, true);
    ASSERT_NE(-1, fd_srv);
    // start listening for connections on the server socket
    int result = listen(fd_srv, 5);
    ASSERT_NE(-1, result);
    // client socket making a connection to the server
    auto socket_client = std::make_unique<TCPSocket>(*logger);
    auto fd_client = socket_client->connect(IP, IFACE, PORT, false);
    ASSERT_NE(-1, fd_client);
    std::this_thread::sleep_for(10ms);
    // accept pending client connection
    sockaddr_storage client_addr{ };
    socklen_t addr_size = sizeof(client_addr);
    auto fd_rx = accept(fd_srv, (sockaddr*) &client_addr,
                        &addr_size);
    ASSERT_NE(-1, fd_rx);
    // client sends test data to receiving socket
    std::string s{ "test data" };
    socket_client->load_tx(s.data(), s.size());
    ASSERT_NE(socket_client->i_tx_next, 0);
    socket_client->tx_and_rx();
    std::this_thread::sleep_for(10ms);
    ASSERT_EQ(socket_client->i_tx_next, 0); // tx buffer should be empty
    // validate test data received
    char rx_buffer[MAX_TCP_BACKLOG];
    auto bytes_received = recv(fd_rx, rx_buffer, MAX_TCP_BACKLOG - 1, 0);
    ASSERT_EQ(bytes_received, s.length());
    rx_buffer[bytes_received] = '\0';
    EXPECT_EQ(std::string{ rx_buffer }, s); // received test string should match the one sent
    // cleanup
    if (fd_rx)
        close(fd_rx);
}

TEST_F(TCPSocketBasics, tx_and_rx_receives) {
    /*
     * a client TCPSocket receives a response from a TCPSocket
     * which is acting as server
     */
    using namespace std::literals::chrono_literals;
    // server socket binding and listening for new connections
    auto socket_srv = std::make_unique<TCPSocket>(*logger);
    auto fd_srv = socket_srv->connect(IP, IFACE, PORT, true);
    ASSERT_NE(-1, fd_srv);
    // start listening for connections on the server socket
    int result = listen(fd_srv, 5);
    ASSERT_NE(-1, result);
    // client socket making a connection to the server
    auto socket_client = std::make_unique<TCPSocket>(*logger);
    auto fd_client = socket_client->connect(IP, IFACE, PORT, false);
    ASSERT_NE(-1, fd_client);
    std::this_thread::sleep_for(10ms);
    // accept pending client connection
    sockaddr_storage client_addr{ };
    socklen_t addr_size = sizeof(client_addr);
    auto fd_rx = accept(fd_srv, (sockaddr*) &client_addr,
                        &addr_size);
    ASSERT_NE(-1, fd_rx);
    // the new socket maintaining the client/server
    // connection sends a response to connected client socket
    std::string s{ "test data" };
    auto bytes_sent = send(fd_rx, s.data(), s.size(), 0);
    std::this_thread::sleep_for(10ms);
    ASSERT_EQ(bytes_sent, s.size());    // anything under ~1kb should be sent in a single trip
    // a call to the client's tx_and_rx() should now receive the data
    ASSERT_EQ(socket_client->i_rx_next, 0); // the rx buffer is empty at first
    socket_client->tx_and_rx();
    std::this_thread::sleep_for(10ms);
    ASSERT_NE(socket_client->i_rx_next, 0); // ... and now the rx buffer has data in it
    // validate the received data is the same as was sent
    EXPECT_EQ(socket_client->i_rx_next, s.length());
    auto s_rx = std::string{ socket_client->rx_buffer.begin(),
                             socket_client->rx_buffer.begin() + (int) socket_client->i_rx_next };
    EXPECT_EQ(s_rx, s);
    // cleanup
    if (fd_rx)
        close(fd_rx);
}