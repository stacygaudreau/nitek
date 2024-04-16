/**
 *  
 *  Low-latency C++ Utilities
 *
 *  @file sockets.h
 *  @brief Socket utilities for low latency Linux applications
 *  @author Stacy Gaudreau
 *  @date 2024.04.13
 *
 */


#pragma once


#include <iostream>
#include <string>
#include <unordered_set>
#include <sstream>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <cstring>

#include "logging.h"
#include "macros.h"


namespace Utils
{

constexpr int MAX_TCP_BACKLOG{ 1024 };  // (server) max tcp connections pending/unaccepted

/**
 * @brief Get network interface IP address from its name
 * @param iface Assigned adapter name string, eg: "en0sp1"
 * @return String representation of the interface's IP address
 */
inline auto get_iface_ip(const std::string& iface) -> std::string {
    char buf[NI_MAXHOST]{ '\0' };
    ifaddrs* if_address{ nullptr };
    if (getifaddrs(&if_address) != -1) {
        // traverse linked-list of ifaddrs and find the element matching the given iface name
        for (ifaddrs* ifa = if_address; ifa; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && iface == ifa->ifa_name) {
                getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buf,
                            sizeof(buf), nullptr, 0, NI_NUMERICHOST);
                break;
            }
        }
        freeifaddrs(if_address);    // release allocation made with getifaddrs()
    }
    return buf;
}
/**
 * @brief Disable socket blocking; avoids kernel<>userspace inefficiencies and context switching
 * @param fd Socket file descriptor
 * @return True when successful
 */
inline auto set_non_blocking(int fd) -> bool {
    const auto flags = fcntl(fd, F_GETFL, 0);   // get flags for fd
    if (flags & O_NONBLOCK)
        return true;    // already in non-blocking mode
    return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1);  // set flag to non-blocking
}
/**
 * @brief Disable Nagle's Algorithm on the socket, improving latency by reducing
 * sent TCP packet delay
 * @param fd Socket file descriptor
 * @return True when successful
 */
inline auto set_no_delay(int fd) -> bool {
    int one{ 1 };
    return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
                       reinterpret_cast<void*>(&one), sizeof(one)) != -1);
}
/**
 * @brief Set the socket to use software packet timestamping. Don't set this when using an
 * interface that supports hardware timestamping.
 * @param fd Socket file descriptor
 * @return True when successful
 */
inline auto set_software_timestamps(int fd) -> bool {
    int one{ 1 };
    return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP,
                       reinterpret_cast<void*>(&one), sizeof(one)) != -1);
};
/**
 * @brief Query whether or not a socket operation will block right now
 * @return True if the socket blocks
 */
inline auto get_would_block() -> bool {
    return (errno == EWOULDBLOCK || errno == EINPROGRESS);
};
/**
 * @brief Set TTL on a given socket (non-multicast)
 * @param fd Socket file descriptor
 * @param ttl Time to live (TTL)
 * @return True when successful
 */
inline auto set_ttl(int fd, int ttl) -> bool {
    return (setsockopt(fd, IPPROTO_IP, IP_TTL,
                       reinterpret_cast<void*>(&ttl), sizeof(ttl)) != -1);
}
/**
 * @brief Set TTL on a given multicast socket
 * @param fd Socket file descriptor
 * @param ttl Time to live (TTL)
 * @return True when successful
 */
inline auto set_ttl_multicast(int fd, int ttl) -> bool {
    return (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
                       reinterpret_cast<void*>(&ttl), sizeof(ttl)) != -1);
}
/**
 * @brief Add join membership/subscription to the given multicast stream on the IP/iface specified
 * @param fd Socket file descriptor
 * @param ip String rep'n of iface's IP address
 * @return True when successful
 */
inline auto join(int fd, const std::string& ip) -> bool {
    const ip_mreq mreq{{ inet_addr(ip.c_str()) },
                       { htonl(INADDR_ANY) }};
    return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                       &mreq, sizeof(mreq)) != -1);
}


/**
 * @brief Configuration for a networking socket
 */
struct SocketConfig {
    std::string ip;
    std::string iface;
    int port{ -1 };
    bool is_udp{ false };
    bool is_listening{ false };
    bool has_software_timestamp{ false };

    [[nodiscard]] auto to_str() const {
        std::stringstream ss;
        ss << "SocketConfig: { ip: " << ip
           << ", iface: " << iface
           << ", port: " << port
           << ", is_udp: " << is_udp
           << ", is_listening: " << is_listening
           << ", has_software_timestamp: " << has_software_timestamp << " }\n";
        return ss.str();
    }
};


/**
 * @brief Create a TCP or UDP socket with the given configuration
 * @param conf SocketConfig() describing the socket to be created
 * @param logger The Logger() to report log entries to
 * @return File descriptor for the socket if successful, -1 if error
 */
[[nodiscard]] inline auto create_socket(const SocketConfig& conf, Logger& logger) -> int {
    // ensure an IP has been provided and if not, fetch it
    std::string t_str;
    int status{ };  // stores temp. return status of fn calls
    const auto ip = conf.ip.empty() ? get_iface_ip(conf.iface) : conf.ip;
    logger.logf("% <Sockets::%> %", Utils::get_time_str(&t_str),
                __FUNCTION__, conf.to_str());

    // prepare socket address struct for bind/listen
    addrinfo hints{ };    // memset() not req'd w/ braced initialiser syntax
    hints.ai_flags = (conf.is_listening ? AI_PASSIVE : 0)
            | (AI_NUMERICHOST | AI_NUMERICSERV); // server or client
    hints.ai_family = AF_INET;  // ipv4
    hints.ai_socktype = conf.is_udp ? SOCK_DGRAM : SOCK_STREAM; // datagram or stream (tcp) socket
    hints.ai_protocol = conf.is_udp ? IPPROTO_UDP : IPPROTO_TCP;    // udp or tcp protocol

    // find actual address from hints
    addrinfo* result{ nullptr };
    status = getaddrinfo(ip.c_str(), std::to_string(conf.port).c_str(),
                         &hints, &result);
    ASSERT(!status, "<Sockets> getaddrinfo() failed! error: "
            + std::string(gai_strerror(status)) + ", errno: " + strerror(errno));

    // create the socket
    int fd{ -1 };
    int one{ 1 };
    for (addrinfo* rp = result; rp; rp = rp->ai_next) {
        // todo: fix the potential memory leak from not calling close() on discarded sockets
        //  if there are >1 matches from getaddrinfo. Cannot however think of any reason why there
        //  would ever be >1 matches in our use case since we explicitly set protocol, ipv4, port
        //  and match to a specific IP address
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        ASSERT(fd != -1, "<Sockets> socket() failed! error: "
                + std::string(strerror(errno)));
        status = set_non_blocking(fd);
        ASSERT(status, "<Sockets> set_non_blocking() failed! error: "
                + std::string(strerror(errno)));
        if (!conf.is_udp) {
            // TCP sockets have no_delay set to disable nagle's algo
            status = set_no_delay(fd);
            ASSERT(status, "<Sockets> set_no_delay() failed! "
                    + std::string(strerror(errno)));
        }
        if (!conf.is_listening) {
            // client mode; connect to given IP address
            status = connect(fd, rp->ai_addr, rp->ai_addrlen);
            ASSERT(status != 1, "<Sockets> connect() failed! error: "
                    + std::string(strerror(errno)));
        }
        else {
            // server mode
            // allow reusing the address, ie: multiple ports/sockets can listen on the same IP for
            //   parallel connections, rebinding and rapid restarts
            status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                                reinterpret_cast<const char*>(&one), sizeof(one));
            ASSERT(status == 0, "<Sockets> setsockopt(SO_REUSEADDR) failed! error: "
                    + std::string(strerror(errno)));
            // bind/listen on the address and port
            const sockaddr_in addr{ AF_INET, htons(conf.port),
                                    { htonl(INADDR_ANY) }, { }};
            status = bind(fd, conf.is_udp ? reinterpret_cast<const struct sockaddr*>(&addr)
                                          : rp->ai_addr, sizeof(addr));
            ASSERT(status == 0, "<Sockets> bind() failed! error: "
                    + std::string(strerror(errno)));
        }
        if (!conf.is_udp && conf.is_listening) {
            // TCP server mode -> listen for tcp connections
            status = listen(fd, MAX_TCP_BACKLOG);
            ASSERT(status == 0, "<Sockets> listen() failed! error: "
                    + std::string(strerror(errno)));
        }
        if (conf.has_software_timestamp) {
            // enable software packet timestamps
            status = set_software_timestamps(fd);
            ASSERT(status, "<Sockets> set_software_timestamps() failed! error: "
                    + std::string(strerror(errno)));
        }
    }
    freeaddrinfo(result);
    return fd;
}
}