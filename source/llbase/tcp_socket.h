/**
 *  
 *  Low-latency C++ Utilities
 *
 *  @file tcp_socket.h
 *  @brief Low latency TCP sockets for both sending and receiving
 *  @author Stacy Gaudreau
 *  @date 2024.04.14
 *
 */


#pragma once


#include <functional>
#include <vector>
#include "sockets.h"
#include "logging.h"


namespace LL
{
constexpr size_t TCP_BUFFER_SIZE{ 64 * 1024 * 1024 };


class TCPSocket {
public:
    /**
     * @brief Create a new low-latency TCP socket
     * @param logger Logging instance to write to
     * @details Whether to use stack (array) or heap (vector) memory for this
     * socket's tx/rx buffers should be determined for the specific application use case.
     * Stack memory may perform better, depending on the buffer and system parameters.
     */
    explicit TCPSocket(Logger& logger) : logger(logger) {
        tx_buffer.resize(TCP_BUFFER_SIZE);
        rx_buffer.resize(TCP_BUFFER_SIZE);
        rx_callback =
                [this](auto socket, auto t_rx) { default_rx_callback(socket, t_rx); };
    }
    /**
     * @brief Call to actually create the corresponding unix socket
     * @param ip IP address to connect to
     * @param iface Interface to connect to
     * @param port Port to connect to
     * @param is_listening Binds the socket to port for incoming connections when true
     * @return The file descriptor (fd) integer if successful, else -1
     */
    auto connect(const std::string& ip, const std::string& iface,
                 int port, bool is_listening) -> int;
    /**
     * @brief Call to load data into the send (tx) buffer for transmission
     * @param data Data to be sent
     * @param len Length of given data
     */
    void load_tx(const void* data, size_t len) noexcept;
    /**
     * @brief Publish tx and rx data to buffers, dispatching an rx_callback if needed
     * @return True when data is ready to read in rx_buffer
     */
    auto tx_and_rx() noexcept -> bool;

    ~TCPSocket() {
        close(fd);
    }

    int fd{ -1 };
    std::vector<char> tx_buffer{ };
    size_t i_tx_next{ };

    std::vector<char> rx_buffer{ };
    size_t i_rx_next{ };

    std::function<void(TCPSocket* s, Nanos t_rx)> rx_callback;

private:
    bool tx_connected{ true };
    bool rx_connected{ true };

    struct sockaddr_in in_inaddr{ };
    // callback fn when new data is received and available for consumption
    Logger& logger;
    std::string t_str;

    /**
     * @brief Default rx callback simply logs a message on receipt
     */
    void default_rx_callback(TCPSocket* socket, Nanos t_rx) noexcept {
        logger.logf("% <TCPSocket::%> socket: %, len: %, rx: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    socket->fd, socket->i_rx_next, t_rx);
    }

DELETE_DEFAULT_COPY_AND_MOVE(TCPSocket)
};
}
