#include "gtest/gtest.h"
#include "llbase/mcast_socket.h"
#include "llbase/sockets.h"

#include <string>
#include <unistd.h>
#include <sys/socket.h>
#include <vector>


using namespace LL;


// multicast socket module base tests
class MulticastSockets : public ::testing::Test {
protected:
    std::string logfile{ "mcast_socket_tests.log" };
    std::unique_ptr<Logger> logger;
    static const int PORT{ 12345 };     // port the test sockets will attach to
    std::string IP_MCAST_GROUP{ "239.0.0.1" };    // multicast group ip for testing
    std::string IP{ "127.0.0.1" };   // ip address for test sockets
    std::string IFACE{ "lo" };        // interface name for test sockets

    void SetUp() override {
        logger = std::make_unique<Logger>(logfile);
    }

    void TearDown() override {
    }
};


TEST_F(MulticastSockets, is_constructed) {
    // mcast socket is constructed
    auto socket = std::make_unique<McastSocket>(*logger);
    ASSERT_NE(socket, nullptr);
}

TEST_F(MulticastSockets, init_non_listening) {
    // a valid fd is returned during init
    // of a non-listening socket
    auto socket = std::make_unique<McastSocket>(*logger);
    auto fd = socket->init(IP, IFACE, 0, false);
    ASSERT_NE(fd, -1);
}

TEST_F(MulticastSockets, init_listening) {
    // a valid fd is returned during init
    // of a listening socket
    auto socket = std::make_unique<McastSocket>(*logger);
    auto fd = socket->init(IP, IFACE, 0, true);
    ASSERT_NE(fd, -1);
}

TEST_F(MulticastSockets, loading_tx_buffer) {
    // data is loaded into transmission buffer
    auto socket = std::make_unique<McastSocket>(*logger);
    char d{ 99 };
    socket->load_tx(reinterpret_cast<const void*>(&d), sizeof(d));
    EXPECT_EQ(99, (char) socket->tx_buffer[0]);
    EXPECT_EQ(1, socket->i_tx_next);
}

TEST_F(MulticastSockets, group_join_succeeds) {
    // joining a multicast group succeeds
    auto socket = std::make_unique<McastSocket>(*logger);
    socket->init(IP, IFACE, 0, false);
    auto socket_listen = std::make_unique<McastSocket>(*logger);
    socket_listen->init(IP, IFACE, 0, true);
    const std::string mcast_ip{ IP_MCAST_GROUP };
    const std::string mcast_ip2{ "239.255.0.1" };
    EXPECT_TRUE(socket->join_group(mcast_ip));
    EXPECT_TRUE(socket_listen->join_group(mcast_ip2));
}

TEST_F(MulticastSockets, group_leave_succeeds) {
    // leaving a multicast group succeeds
    auto socket = std::make_unique<McastSocket>(*logger);
    socket->init(IP, IFACE, 0, false);
    // join the group
    EXPECT_TRUE(socket->join_group(IP_MCAST_GROUP));
    EXPECT_NE(socket->fd, -1);
    socket->leave_group();
    EXPECT_EQ(socket->fd, -1);  // fd should now be non-initialised
}

TEST_F(MulticastSockets, tx_and_rx_transmits) {
    // a sending McastSocket transmits data to a multicast
    // group it is joined to
    auto tx_socket = std::make_unique<McastSocket>(*logger);
    tx_socket->init(IP_MCAST_GROUP, IFACE, PORT, false);
    // receiving socket
    auto rx_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in rx_addr{ };
    rx_addr.sin_family = AF_INET;
    rx_addr.sin_port = htons(PORT);
    rx_addr.sin_addr.s_addr = INADDR_ANY;
    // binding
    auto status = bind(rx_fd, (sockaddr*) &rx_addr, sizeof(rx_addr));
    EXPECT_NE(-1, status);  // bind should succeed
    // join rx to multicast group
    ip_mreq mreq{ };
    mreq.imr_multiaddr.s_addr = inet_addr(IP_MCAST_GROUP.c_str());
    mreq.imr_interface.s_addr = INADDR_ANY;
    setsockopt(rx_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq));
    // now join tx
    EXPECT_TRUE(tx_socket->join_group(IP_MCAST_GROUP));
    // send a test message
    std::string tx_msg{ "test" };
    tx_socket->load_tx(tx_msg.data(), tx_msg.size());
    EXPECT_NE(tx_socket->i_tx_next, 0);
    tx_socket->tx_and_rx();
    EXPECT_EQ(tx_socket->i_tx_next, 0);
    // receive the message
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);
    char buffer[1024];
    auto n = recv(rx_fd, buffer, sizeof(buffer), 0);
    EXPECT_EQ(n, tx_msg.size());
    // validate the message
    EXPECT_EQ(std::string(buffer, n), tx_msg);
    // clean up
    if (rx_fd)
        close(rx_fd);
}

TEST_F(MulticastSockets, tx_and_rx_receives) {
    // an McastSocket receives and verifies a multicast
    // group message from another socket
    auto rx_socket = std::make_unique<McastSocket>(*logger);
    auto rx_fd = rx_socket->init(IP_MCAST_GROUP, IFACE, PORT, true);
    EXPECT_NE(rx_fd, -1);
    // transmitting socket
    auto tx_fd = socket(AF_INET, SOCK_DGRAM, 0);
    // connect tx socket to multicast group
    sockaddr_in tx_addr{ };
    tx_addr.sin_family = AF_INET;
    tx_addr.sin_port = htons(PORT);
    tx_addr.sin_addr.s_addr = inet_addr(IP_MCAST_GROUP.c_str());
    auto status = connect(tx_fd, (sockaddr*) &tx_addr, sizeof(tx_addr));
    EXPECT_NE(-1, status);  // connection should succeed
    // now join rx_socket to the mcast group
    EXPECT_TRUE(rx_socket->join_group(IP_MCAST_GROUP));
    // send a test message
    std::string tx_msg{ "test" };
    auto n_sent = send(tx_fd, tx_msg.data(), tx_msg.size(), 0);
    EXPECT_EQ(n_sent, 4);
    // rx callback to validate
    bool callback_executed{ false };
    rx_socket->rx_callback = [&](McastSocket* socket) {
        (void) socket;
        callback_executed = true;
    };
    // receive msg
    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(10ms);
    rx_socket->tx_and_rx();
    // validate msg
    EXPECT_TRUE(callback_executed);
    EXPECT_EQ(rx_socket->i_rx_next, tx_msg.size());
    EXPECT_EQ(std::string(rx_socket->rx_buffer.begin(),
                          rx_socket->rx_buffer.begin() + tx_msg.size()), tx_msg);
    // clean up
    if (tx_fd)
        close(tx_fd);
}

