#include "tcp_server.h"


namespace Utils
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
    ASSERT(status, "<TCPServer> epoll_ctl() failed! error: "
            + std::string(std::strerror(errno)));

}
auto TCPServer::epoll_add(TCPSocket* socket) -> int {
    epoll_event e_epoll{ EPOLLET | EPOLLIN,
                         { reinterpret_cast<void*>(socket) }};
    return epoll_ctl(fd_epoll, EPOLL_CTL_ADD, socket->fd, &e_epoll);
}

}