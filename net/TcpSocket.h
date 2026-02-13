#ifndef REMOTE_CONTROL_TCPSOCKET_H
#define REMOTE_CONTROL_TCPSOCKET_H

#include <string>

namespace lsy::net {
class SocketUtil {
public:
    static void SetNonBlock(int sockfd);

    static void SetBlock(int sockfd);

    static void SetReuseAddr(int sockfd);

    static void SetReusePort(int sockfd);

    static void SetKeepAlive(int sockfd);

    static void SetSendBufSize(int sockfd, int size);

    static void SetRecvBufSize(int sockfd, int size);
};

class TcpSocket {
public:
    TcpSocket() = default;

    virtual ~TcpSocket() = default;

    int Create();

    bool Bind(const std::string& ip, short port);

    bool Listen(int backlog);

    int Accept();

    void Close();

    void ShutdownWrite();

    int GetSocket() const { return sockfd_; }

private:
    int sockfd_ = -1;
};
} // lsy::net

#endif //REMOTE_CONTROL_TCPSOCKET_H
