#include "gtest/gtest.h"
#include "llbase/sockets.h"
#include "llbase/logging.h"

#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cerrno>


using namespace LL;


class SocketUtils : public ::testing::Test {
protected:
    int socket_fd{ -1 };
    int socket_fd_udp{ -1 };
    std::string logfile{ "sockets.log" };

    void SetUp() override {
        // open a test TCP socket
        struct sockaddr_in server_addr{ };
        if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            std::cerr << "error opening test TCP socket!\n";
            exit(EXIT_FAILURE);
        }
        std::cout << "Test TCP socket opened at fd: " << socket_fd << "\n";

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;   // ipv4 socket
        server_addr.sin_port = htons(0); // system assigns a free port
        server_addr.sin_addr.s_addr = INADDR_ANY; // listen on any ip address

        // open test UDP socket
        if ((socket_fd_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
            std::cerr << "error opening test UDP socket!\n";
            exit(EXIT_FAILURE);
        }
        std::cout << "Test UDP socket opened at fd: " << socket_fd_udp << "\n";
    }

    void TearDown() override {
        close(socket_fd);
        close(socket_fd_udp);
    }
};


TEST_F(SocketUtils, no_ip_returned_for_bogus_iface) {
    // a blank interface IP string is returned when given a bogus iface string
    EXPECT_EQ(get_iface_ip("garbage_iface"), std::string{ });
}

TEST_F(SocketUtils, get_loopback_ip_from_iface) {
    // the v4 loopback IP is returned, given iface "lo"
    EXPECT_EQ(get_iface_ip("lo"), "127.0.0.1");
}

TEST_F(SocketUtils, set_non_blocking_socket) {
    // test socket can have its non-blocking flag set
    auto flags = fcntl(socket_fd, F_GETFL, 0);
    EXPECT_FALSE(flags & O_NONBLOCK);   // default is blocking
    EXPECT_TRUE(set_non_blocking(socket_fd)); // function call succeeds
    flags = fcntl(socket_fd, F_GETFL, 0);
    EXPECT_TRUE(flags & O_NONBLOCK);    // blocking should be active now
}

TEST_F(SocketUtils, set_blocking_socket) {
    // test socket can toggle its blocking flag back on
    auto flags = fcntl(socket_fd, F_GETFL, 0);
    ASSERT_TRUE(set_non_blocking(socket_fd)); // function call succeeds
    flags = fcntl(socket_fd, F_GETFL, 0);
    ASSERT_TRUE(flags & O_NONBLOCK);    // blocking should be active now
    EXPECT_TRUE(set_blocking(socket_fd));
    flags = fcntl(socket_fd, F_GETFL, 0);
    EXPECT_FALSE(flags & O_NONBLOCK);   // blocking is disabled now
}

TEST_F(SocketUtils, set_no_tcp_send_delay) {
    int optval{ }; // result of reading socket options
    socklen_t optlen = sizeof(optval);
    // default has delay enabled
    getsockopt(socket_fd, IPPROTO_TCP,
               TCP_NODELAY, (void*) &optval, &optlen);
    EXPECT_EQ(0, optval);
    // fn call succeeds
    EXPECT_TRUE(set_no_delay(socket_fd));
    // nodelay option is now set
    getsockopt(socket_fd, IPPROTO_TCP,
               TCP_NODELAY, (void*) &optval, &optlen);
    EXPECT_EQ(1, optval);
}

TEST_F(SocketUtils, software_timestamps_are_set) {
    int optval{ }; // result of reading socket options
    socklen_t optlen = sizeof(optval);
    // default has software timestamps disabled
    getsockopt(socket_fd, SOL_SOCKET,
               SO_TIMESTAMP, (void*) &optval, &optlen);
    EXPECT_EQ(0, optval);
    // fn call succeeds
    EXPECT_TRUE(set_software_timestamps(socket_fd));
    // software timestamp option is now set
    getsockopt(socket_fd, SOL_SOCKET,
               SO_TIMESTAMP, (void*) &optval, &optlen);
    EXPECT_EQ(1, optval);
}

TEST_F(SocketUtils, detects_blocking_operation) {
    errno = 0;
    EXPECT_FALSE(get_would_block());
    errno = EWOULDBLOCK;
    EXPECT_TRUE(get_would_block());
    errno = EINPROGRESS;
    EXPECT_TRUE(get_would_block());
}

TEST_F(SocketUtils, multicast_group_join) {
    // multicast group membership added to socket options
    // multicast group address
    const std::string mcast_ip{ "239.0.0.1" };
    // fn call succeeds
    EXPECT_TRUE(mcast_group_join(socket_fd_udp, mcast_ip));
    // getsockopt() does not retrieve the multicast groups joined
    // so we just expect the fn call to succeed above
    // -> maybe there is a different way to verify it (to-do)
}

TEST_F(SocketUtils, ttl_is_set) {
    int optval{ }; // result of reading socket options
    socklen_t optlen = sizeof(optval);
    // default ttl is something other than -1 (system-dependent)
    getsockopt(socket_fd, IPPROTO_IP,
               IP_TTL, (void*) &optval, &optlen);
    EXPECT_NE(-1, optval);
    // fn call succeeds
    EXPECT_TRUE(set_ttl(socket_fd, 128));
    // TTL is now 100
    getsockopt(socket_fd, IPPROTO_IP,
               IP_TTL, (void*) &optval, &optlen);
    EXPECT_EQ(128, optval);
}

TEST_F(SocketUtils, multicast_ttl_is_set) {
    int optval{ }; // result of reading socket options
    socklen_t optlen = sizeof(optval);
    // default ttl is something other than -1 (system-dependent)
    getsockopt(socket_fd_udp, IPPROTO_IP,
               IP_MULTICAST_TTL, (void*) &optval, &optlen);
    EXPECT_NE(-1, optval);
    // fn call succeeds
    EXPECT_TRUE(set_ttl_multicast(socket_fd_udp, 128));
    // TTL is now 100
    getsockopt(socket_fd_udp, IPPROTO_IP,
               IP_MULTICAST_TTL, (void*) &optval, &optlen);
    EXPECT_EQ(128, optval);
}

TEST_F(SocketUtils, create_client_socket_succeeds) {
    // creating a tcp client socket
    Logger logger{ logfile };
    SocketConfig conf{ };
    int fd{ -1 };
    conf.ip = "127.0.0.1";
    conf.has_software_timestamp = false;
    conf.is_listening = false;
    conf.is_udp = false;
    conf.port = 0;
    fd = create_socket(conf, logger);
    EXPECT_NE(fd, -1);  // test for valid file descriptor
    // close the socket
    if (fd != -1)
        close(fd);
}

TEST_F(SocketUtils, create_server_socket_succeeds) {
    // creating a tcp server socket
    Logger logger{ logfile };
    SocketConfig conf{ };
    int fd{ -1 };
    conf.ip = "127.0.0.1";
    conf.has_software_timestamp = true;
    conf.is_listening = true;
    conf.is_udp = false;
    conf.port = 0;
    fd = create_socket(conf, logger);
    EXPECT_NE(fd, -1);  // test for valid file descriptor
    // verify that address reuse option is set
    int optval{ }; // result of reading socket options
    socklen_t optlen = sizeof(optval);
    getsockopt(fd, SOL_SOCKET,
               SO_REUSEADDR, (void*) &optval, &optlen);
    EXPECT_EQ(optval, 1);
    // close the socket
    if (fd != -1)
        close(fd);
}

TEST_F(SocketUtils, create_udp_server_socket_succeeds) {
    // creating a udp server socket with only iface specified
    Logger logger{ logfile };
    SocketConfig conf{ };
    int fd{ -1 };
    conf.iface = "lo";
    conf.has_software_timestamp = true;
    conf.is_listening = true;
    conf.is_udp = true;
    conf.port = 0;
    fd = create_socket(conf, logger);
    EXPECT_NE(fd, -1);  // test for valid file descriptor
    // close the socket
    if (fd != -1)
        close(fd);
}
