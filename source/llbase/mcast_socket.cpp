#include "mcast_socket.h"


namespace LL
{

auto McastSocket::init(const std::string& ip, const std::string& iface, int port,
                       bool is_listening) -> int {
    const SocketConfig conf{ ip, iface, port, true,
                             is_listening, false };
    fd = create_socket(conf, logger);
    return fd;
}

auto McastSocket::join_group(const std::string& ip) -> bool {
    return mcast_group_join(fd, ip);
}

void McastSocket::leave_group() {
    close(fd);
    fd = -1;
}

void McastSocket::load_tx(const void* data, size_t len) noexcept {
    memcpy(tx_buffer.data() + i_tx_next, data, len);
    i_tx_next += len;
    ASSERT(i_tx_next < MCAST_BUFFER_SIZE,
           "<McastSocket> tx buffer overflow! Have you called tx_and_rx()?");
}

auto McastSocket::tx_and_rx() noexcept -> bool {
    // non-blocking read
    const auto rx_size = recv(fd, rx_buffer.data() + i_rx_next,
                              MCAST_BUFFER_SIZE - i_rx_next, MSG_DONTWAIT);
    if (rx_size > 0) {
        i_rx_next += rx_size;
        logger.logf("% <McastSocket::%> RX at socket %, size: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, fd, i_rx_next);
        // callback when data is available to read
        rx_callback(this);
    }

    // transmit outgoing data to stream
    if (i_tx_next > 0) {
        // we don't have to use sendto() with this multicast socket since
        //  the call to create_socket() already calls connect() on the
        //  multicast group this socket belongs to. If the design changes
        //  it may be necessary to use sendto() here instead.
        const auto n = send(fd, tx_buffer.data(), i_tx_next,
                            MSG_DONTWAIT | MSG_NOSIGNAL);
        logger.logf("% <McastSocket::%> TX at socket %, size: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, fd, n);
    }

    // clear tx buffer and return number of rx waiting to be read
    i_tx_next = 0;
    return (rx_size > 0);
}

}
