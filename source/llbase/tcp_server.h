/**
 *  
 *  Low-latency C++ Utilities
 *
 *  @file tcp_server.h
 *  @brief A TCP server which incorporates multiple TCPSockets to provide connections
 *  @author Stacy Gaudreau
 *  @date 2024.04.14
 *
 */

#include <utility>
#include <vector>
#include <sys/epoll.h>

#include "tcp_socket.h"


#pragma once

namespace LL
{

class TCPServer {
public:
    explicit TCPServer(Logger& logger) : listener_socket(logger),
                                         logger(logger) {
        rx_callback = [this](auto socket, auto t_rx) {
            default_rx_callback(socket, t_rx);
        };
        rx_done_callback = [this]() { default_rx_done_callback(); };
    }
    ~TCPServer() {
        close(fd_epoll);
    }

    /**
     * @brief Listen for connections on the iface/port specified
     * @param iface Interface name
     * @param port Port number
     */
    void listen(const std::string& iface, int port);
    /**
     * @brief Poll for new or dead connections, updating socket tracking containers
     */
    void poll() noexcept;
    /**
     * @brief Send and receive data from all tx and rx socket buffers
     */
    void tx_and_rx() noexcept;
    /**
     * @brief Set the function to be called when data is available to read from
     * the rx_buffer
     */
    inline void set_rx_callback(
            std::function<void(TCPSocket* s, Nanos t_rx)> fn) noexcept {
        rx_callback = std::move(fn);
    };
    /**
     * @brief Set the function to be called when all rx_sockets have completed
     * their read cycles
     */
    inline void set_rx_done_callback(std::function<void()> fn) noexcept {
        rx_done_callback = fn;
    }

    inline TCPSocket& get_socket() noexcept { return listener_socket; };
    inline int get_fd_epoll() noexcept { return fd_epoll; };
    inline auto& get_rx_sockets() noexcept { return rx_sockets; };
    inline auto& get_tx_sockets() noexcept { return tx_sockets; };
    inline auto& get_dx_sockets() noexcept { return dx_sockets; };

private:
    int fd_epoll{ -1 };                 // file descriptor for EPOLL
    TCPSocket listener_socket;          // listener for new incoming connections
    epoll_event events[1024];           // for monitoring the listener fd
    std::vector<TCPSocket*> rx_sockets; // receiving sockets
    std::vector<TCPSocket*> tx_sockets; // transmission sockets
    std::vector<TCPSocket*> dx_sockets; // disconnected sockets
    // rx data available callback
    std::function<void(TCPSocket* s, Nanos t_rx)> rx_callback;
    // callback when all rx sockets have completed read cycle
    std::function<void()> rx_done_callback;
    std::string t_str;
    Logger& logger;

    /**
     * @brief Add socket to the epolling list
     * @param socket Socket to add
     * @return 0 if success, -1 if failure
     */
    auto epoll_add(TCPSocket* socket) -> int;
    /**
     * @brief Default rx callback simply logs a message on receipt
     */
    void default_rx_callback(TCPSocket* socket, Nanos t_rx) noexcept {
        logger.logf("% <TCPServer::%> socket: %, len: %, rx: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    socket->fd, socket->i_rx_next, t_rx);
    }
    /**
    * @brief Default rx complete callback simply logs a message on receipt
    */
    void default_rx_done_callback() noexcept {
        logger.logf("% <TCPServer::%> server rx done\n",
                    LL::get_time_str(&t_str), __FUNCTION__);
    }
};

}
