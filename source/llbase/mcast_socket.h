/**
 *
 *  Low-latency C++ Utilities
 *
 *  @file mcast_socket.h
 *  @brief Multicast UDP socket for efficiently broadcasting data to multiple clients.
 *  @author Stacy Gaudreau
 *  @date 2024.05.04
 *
 */


#pragma once


#include <functional>
#include <vector>
#include <string>
#include "sockets.h"
#include "logging.h"


namespace LL
{
constexpr size_t MCAST_BUFFER_SIZE{ 64 * 1024 * 1024 };


class McastSocket {
public:
    /**
     * @brief
     * @param logger
     */
    explicit McastSocket(Logger& logger) : logger(logger) {
        tx_buffer.resize(MCAST_BUFFER_SIZE);
        rx_buffer.resize(MCAST_BUFFER_SIZE);
    }
    ~McastSocket() {
        if (fd)
            close(fd);
    }

    /**
     * @brief Create initialised socket to read from or publish to a multicast stream
     * @details Doesn't join the stream. Use join_group() and leave_group() for membership
     * @param ip Multicast IP address identifier
     * @param iface Interface name to connect socket to
     * @param port Port to connect the socket to
     * @param is_listening Binds the socket to port for incoming connections when true
     * @return -1 for failure, else file descriptor (fd) of the created socket
     */
    auto init(const std::string& ip, const std::string& iface,
              int port, bool is_listening) -> int;
    /**
     * @brief Join a multicast group
     * @param ip Multicast IP address identifier
     * @return True if join is successful
     */
    auto join_group(const std::string& ip) -> bool;
    /**
     * @brief Terminate membership with a multicast group
     */
    void leave_group();
    /**
     * @brief Call to load data into the send (tx) buffer for transmission
     * @param data Data to be sent
     * @param len Length of given data
     */
    void load_tx(const void* data, size_t len) noexcept;
    /**
     * @brief Publish tx and rx data to buffers. Dispatches an rx_callback if needed.
     * @return True when data is ready to read in rx_buffer
     */
    auto tx_and_rx() noexcept -> bool;

    std::vector<char> tx_buffer{ };  // transmit data buffer
    size_t i_tx_next{ };             // index of next element to transmit

    std::vector<char> rx_buffer{ };  // receive data buffer
    size_t i_rx_next{ };             // index of next element to receive
    std::function<void(McastSocket* socket)> rx_callback{ nullptr };

    int fd{ -1 }; // file descriptor for socket

private:
    std::string t_str;
    Logger& logger;

DELETE_DEFAULT_COPY_AND_MOVE(McastSocket)
};
}


