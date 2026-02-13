#include "TcpSocket.h"

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <cstdio>

namespace lsy::net {

void SocketUtil::SetNonBlock(int sockfd) {
    if (::fcntl(sockfd, F_GETFL, 0) < 0) {
        fprintf(stderr, "fcntl F_GETFL error\n");
        return;
    }
}

void SocketUtil::SetBlock(int sockfd) {
    int ret = ::fcntl(sockfd, F_SETFL, O_NONBLOCK);
    if (ret < 0) {
        fprintf(stderr, "fcntl F_SETFL error\n");
        return;
    }
}

void SocketUtil::SetReuseAddr(int sockfd) {
    int optval = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval,
                         sizeof(optval));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_REUSEADDR error\n");
        return;
    }
}

void SocketUtil::SetReusePort(int sockfd) {
    int optval = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval,
                         sizeof(optval));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_REUSEPORT error\n");
        return;
    }
}

void SocketUtil::SetKeepAlive(int sockfd) {
    int optval = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval,
                         sizeof(optval));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_KEEPALIVE error\n");
        return;
    }
}

void SocketUtil::SetSendBufSize(int sockfd, int size) {
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_SNDBUF error\n");
        return;
    }
}

void SocketUtil::SetRecvBufSize(int sockfd, int size) {
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_RCVBUF error\n");
        return;
    }
}

int TcpSocket::Create() {
    sockfd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd_ < 0) {
        fprintf(stderr, "socket error\n");
        return -1;
    }
    return 0;
}

bool TcpSocket::Bind(const std::string& ip, short port) {

    return false;
}

bool TcpSocket::Listen(int backlog) {
    return false;
}

int TcpSocket::Accept() {
    return 0;
}

void TcpSocket::Close() {

}

void TcpSocket::ShutdownWrite() {

}
}
