#include "gtest/gtest.h"
#include "utils/tcp_socket.h"

#include <string>
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <cerrno>


using namespace Utils;


//class TCPSockets : public ::testing::Test {
//protected:
//    int socket_fd{ -1 };
//    int socket_fd_udp{ -1 };
//    std::string logfile{ "sockets.log" };
//
//    void SetUp() override {
//        // open a test TCP socket
//        struct sockaddr_in server_addr{ };
//        if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
//            std::cerr << "error opening test TCP socket!\n";
//            exit(EXIT_FAILURE);
//        }
//        std::cout << "Test TCP socket opened at fd: " << socket_fd << "\n";
//
//        memset(&server_addr, 0, sizeof(server_addr));
//        server_addr.sin_family = AF_INET;   // ipv4 socket
//        server_addr.sin_port = htons(0); // system assigns a free port
//        server_addr.sin_addr.s_addr = INADDR_ANY; // listen on any ip address
//
//        // open test UDP socket
//        if ((socket_fd_udp = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
//            std::cerr << "error opening test UDP socket!\n";
//            exit(EXIT_FAILURE);
//        }
//        std::cout << "Test UDP socket opened at fd: " << socket_fd_udp << "\n";
//    }
//
//    void TearDown() override {
//        close(socket_fd);
//        close(socket_fd_udp);
//    }
//};