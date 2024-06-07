#include "tcp_server.h"


namespace LL
{

void TCPServer::listen(const std::string& iface, int port) {
    int status{ };
    fd_epoll = epoll_create(1);
    ASSERT(fd_epoll >= 0, "<TCPServer> epoll create failed! error: "
            + std::string(std::strerror(errno)));
    // connect the socket in listening mode
    status = listener_socket.connect({ }, iface, port, true);
    ASSERT(status >= 0, "<TCPServer> listener socket connect() failed at iface: "
            + iface + ", port: " + std::to_string(port) + ", error: "
            + std::string(std::strerror(errno)));
    status = epoll_add(&listener_socket);
    ASSERT(status != -1, "<TCPServer> epoll_ctl() failed! error: "
            + std::string(std::strerror(errno)));
}

auto TCPServer::epoll_add(TCPSocket* socket) -> int {
    // enable edge-triggered epoll -> ie: notify once when data needs reading
    //  (instead of constant polling reminders)
    epoll_event e{ EPOLLET | EPOLLIN | EPOLLOUT,
                   { reinterpret_cast<void*>(socket) }};
    return epoll_ctl(fd_epoll, EPOLL_CTL_ADD, socket->fd, &e);
}

void TCPServer::poll() noexcept {
    const int max_events = 1 + static_cast<int>(tx_sockets.size())
            + static_cast<int>(rx_sockets.size());
    const int n_events = epoll_wait(fd_epoll, events, max_events, 0);
    bool has_new_connection{ false };
    int status{ 0 }; // tracks function return status
    // iterate through each epoll socket event
    for (int i = 0; i < n_events; ++i) {
        const auto& e = events[i];
        auto socket = reinterpret_cast<TCPSocket*>(e.data.ptr);
        // socket with data to read from
        if (e.events & EPOLLIN) {
            if (socket == &listener_socket) {
                // data received on primary listener socket (ie: the TCPServer's socket)
                //  so, we have a new incoming connection socket to add
                logger.logf("% <TCPServer::%> EPOLLIN at listener_socket fd: %\n",
                            LL::get_time_str(&t_str), __FUNCTION__,
                            socket->fd);
                has_new_connection = true;
                continue;
            }
            // received on different socket; add to rx_sockets if it isn't
            //  already there
            logger.logf("% <TCPServer::%> EPOLLIN at socket fd: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__,
                        socket->fd);
            if (element_does_not_exist(rx_sockets, socket))
                rx_sockets.push_back(socket);
        }
        // socket we can write to
        if (e.events & EPOLLOUT) {
            // add to tx_sockets if it's not already there
            logger.logf("% <TCPServer::%> EPOLLOUT at socket fd: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__,
                        socket->fd);
            if (element_does_not_exist(tx_sockets, socket)) {
                tx_sockets.push_back(socket);
            }
        }
        // EPOLLERR or EPOLLHUP -> socket was disconnected
        //  (error or signal hang up) -> add to dx_sockets
        if (e.events & (EPOLLERR | EPOLLHUP | EPOLLRDHUP)) {
            logger.logf("% <TCPServer::%> EPOLLERR|HUP at socket fd: %\n",
                        LL::get_time_str(&t_str), __FUNCTION__,
                        socket->fd);
            if (element_does_not_exist(dx_sockets, socket))
                dx_sockets.push_back(socket);
        }
    }

    // accept new connections if any were found
    while (has_new_connection) {
        logger.logf("% <TCPServer::%> has_new_connection\n",
                    LL::get_time_str(&t_str), __FUNCTION__);
        sockaddr_storage addr{ };
        socklen_t addr_len = sizeof(addr);
        int fd = accept(listener_socket.fd, reinterpret_cast<sockaddr*>(&addr),
                        &addr_len);
        if (fd == -1)
            break;
        status = set_non_blocking(fd);
        ASSERT(status,
               "<TCPServer> error! failed to set non-blocking on socket fd: "
                       + std::to_string(fd));
        status = set_no_delay(fd);
        ASSERT(status,
               "<TCPServer> error! failed to set no delay mode on socket fd: "
                       + std::to_string(fd));
        logger.logf("% <TCPServer::%> accepted new socket fd: %\n",
                    LL::get_time_str(&t_str), __FUNCTION__,
                    fd);
        // create TCPSocket and add it to containers
        auto socket = new TCPSocket(logger);
        socket->fd = fd;
        socket->rx_callback = rx_callback;
        status = epoll_add(socket);
        ASSERT(status != -1,
               "<TCPServer> error! unable to add socket: "
                       + std::string(std::strerror(errno)));
        if (element_does_not_exist(rx_sockets, socket))
            rx_sockets.push_back(socket);
    }
}

void TCPServer::tx_and_rx() noexcept {
    // call socket.tx_and_rx() for each of the
    //  sockets on this server
    bool receiving{ false };
    for (auto s: rx_sockets) {
        if (s->tx_and_rx())
            receiving = true;
    }
    if (receiving)
        rx_done_callback();
    for (auto s: tx_sockets) {
        s->tx_and_rx();
    }
}

}