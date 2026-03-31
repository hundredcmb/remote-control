#include "TcpSocket.h"

#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cerrno>
#include <cstring>

namespace lsy::net {

void SocketUtil::SetNonBlock(int sockfd) {
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        fprintf(stderr, "fcntl F_GETFL error: %s\n", strerror(errno));
        return;
    }
    // 添加非阻塞标志
    flags |= O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);
    if (ret < 0) {
        fprintf(stderr, "fcntl F_SETFL nonblock error: %s\n", strerror(errno));
    }
}

void SocketUtil::SetBlock(int sockfd) {
    int flags = ::fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        fprintf(stderr, "fcntl F_GETFL error: %s\n", strerror(errno));
        return;
    }
    // 清除非阻塞标志
    flags &= ~O_NONBLOCK;
    int ret = ::fcntl(sockfd, F_SETFL, flags);
    if (ret < 0) {
        fprintf(stderr, "fcntl F_SETFL block error: %s\n", strerror(errno));
    }
}

void SocketUtil::SetReuseAddr(int sockfd) {
    int optval = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval,
                         sizeof(optval));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_REUSEADDR error: %s\n", strerror(errno));
    }
}

void SocketUtil::SetReusePort(int sockfd) {
    int optval = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &optval,
                         sizeof(optval));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_REUSEPORT error: %s\n", strerror(errno));
    }
}

void SocketUtil::SetKeepAlive(int sockfd) {
    int optval = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &optval,
                         sizeof(optval));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_KEEPALIVE error: %s\n", strerror(errno));
    }
}

void SocketUtil::SetSendBufSize(int sockfd, int size) {
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_SNDBUF error: %s\n", strerror(errno));
    }
}

void SocketUtil::SetRecvBufSize(int sockfd, int size) {
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    if (ret < 0) {
        fprintf(stderr, "setsockopt SO_RCVBUF error: %s\n", strerror(errno));
    }
}

int TcpSocket::Create() {
    // 创建TCP套接字 + 非阻塞 + 执行exec时关闭（原子操作）
    sockfd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sockfd_ < 0) {
        fprintf(stderr, "socket create error: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

bool TcpSocket::Bind(const std::string& ip, short port) const {
    if (sockfd_ < 0) {
        fprintf(stderr, "bind error: socket not created\n");
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;                // IPv4
    addr.sin_port = htons(port);              // 主机字节序转网络字节序

    // IP字符串转网络字节序
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        fprintf(stderr, "inet_pton error: invalid ip=%s\n", ip.c_str());
        return false;
    }

    // 执行绑定
    int ret = ::bind(sockfd_, (struct sockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        fprintf(stderr, "bind error: %s\n", strerror(errno));
        return false;
    }
    return true;
}

bool TcpSocket::Listen(int backlog) const {
    if (sockfd_ < 0) {
        fprintf(stderr, "listen error: socket not created\n");
        return false;
    }
    int ret = ::listen(sockfd_, backlog);
    if (ret < 0) {
        fprintf(stderr, "listen error: %s\n", strerror(errno));
        return false;
    }
    return true;
}

int TcpSocket::Accept() const {
    if (sockfd_ < 0) {
        fprintf(stderr, "accept error: socket not created\n");
        return -1;
    }

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    // 非阻塞accept，内核直接返回，不等待
    int client_fd = ::accept4(sockfd_, (struct sockaddr*)&client_addr, &addr_len,
                              SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (client_fd < 0) {
        // 非阻塞下无连接是正常错误，不打印
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            fprintf(stderr, "accept error: %s\n", strerror(errno));
        }
        return -1;
    }
    return client_fd;
}

void TcpSocket::Close() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1; // 置为无效，防止重复关闭
    }
}

void TcpSocket::ShutdownWrite() const {
    if (sockfd_ >= 0) {
        ::shutdown(sockfd_, SHUT_WR);
    }
}

} // namespace lsy::net