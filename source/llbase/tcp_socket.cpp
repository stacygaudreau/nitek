#include "tcp_socket.h"


namespace LL
{

auto TCPSocket::connect(const std::string& ip, const std::string& iface, int port,
                        bool is_listening) -> int {
    // configure and create socket
    const SocketConfig conf{
            ip, iface, port, false, is_listening, true };
    fd = create_socket(conf, logger);
    // set connection attributes and return descriptor
    in_inaddr.sin_addr.s_addr = INADDR_ANY;
    in_inaddr.sin_port = htons(port);
    in_inaddr.sin_family = AF_INET;
    return fd;
}

void TCPSocket::load_tx(const void* data, size_t len) noexcept {
    // we simply copy the given data into the buffer and advance its index
    memcpy(tx_buffer.data() + i_tx_next, data, len);
    i_tx_next += len;
    ASSERT(i_tx_next < TCP_BUFFER_SIZE,
           "<TCPSocket> tx buffer overflow! Have you called tx_and_rx()?");
}

auto TCPSocket::tx_and_rx() noexcept -> bool {
    char ctrl[CMSG_SPACE(sizeof(struct timeval))];
    auto cmsg = reinterpret_cast<struct cmsghdr*>(&ctrl);

    iovec iov{ rx_buffer.data() + i_rx_next, TCP_BUFFER_SIZE - i_rx_next };
    msghdr msg{ &in_inaddr, sizeof(in_addr),
                &iov, 1, ctrl,
                sizeof(ctrl), 0 };

    // non-blocking read of data
    const auto rx_size = recvmsg(fd, &msg, MSG_DONTWAIT);
    if (rx_size > 0) {
        i_rx_next += rx_size;
        Nanos t_kernel{ };
        timeval kernel_timeval{ };
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_TIMESTAMP
                && cmsg->cmsg_len == CMSG_LEN(sizeof(kernel_timeval))) {
            memcpy(&kernel_timeval, CMSG_DATA(cmsg), sizeof(kernel_timeval));
            t_kernel = kernel_timeval.tv_sec * NANOS_TO_SECS
                    + kernel_timeval.tv_usec * NANOS_TO_MICROS; // timestamp converted to ns
        }
        const auto t_user = get_time_nanos();
        logger.logf("% <TCPSocket::%> RX at socket %, len: %, t_user: %, t_kernel: %, delta: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, fd, i_rx_next,
                    t_user, t_kernel, (t_user - t_kernel));
        rx_callback(this, t_kernel);
    }

    // non-blocking write out of data in tx buffer
    if (i_tx_next > 0) {
        const auto n = send(fd, tx_buffer.data(),
                            i_tx_next, MSG_DONTWAIT | MSG_NOSIGNAL);
        logger.logf("% <TCPSocket::%> TX at socket %, size: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__, fd, n);
    }
    i_tx_next = 0;
    return (rx_size > 0);
}

}
