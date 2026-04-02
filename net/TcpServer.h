#ifndef NET_TCPSERVER_H
#define NET_TCPSERVER_H

#include "Acceptor.h"
#include "TcpConnection.h"

namespace lsy::net {

class TcpServer {
public:
    explicit TcpServer(EventLoops *event_loop)
        : is_started_(false),
          event_loops_(event_loop),
          acceptor_(new Acceptor(event_loop)) {
        acceptor_->SetNewConnectionCallback([this](SocketFd sockfd) -> void {
            TcpConnectionPtr tcp_conn = this->OnConnect(sockfd);
            if (tcp_conn) {
                this->AddConnection(sockfd, tcp_conn);
                tcp_conn->SetDisconnectCallback(
                    [this](const TcpConnectionPtr &tcp_conn) -> void {
                        SocketFd fd = tcp_conn->GetSocket();
                        this->RemoveConnection(fd);
                    }
                );
            }
        });
    }

    virtual ~TcpServer() {
        Stop();
    }

    bool Start(const std::string &ip, uint16_t port) {
        Stop();
        if (!is_started_) {
            if (acceptor_->Listen(ip, port) < 0) {
                return false;
            }
            port_ = port;
            ip_ = ip;
            is_started_ = true;
        }
        return true;
    }

    void Stop() {
        if (!is_started_) {
            return;
        }
        for (auto &pair: connections_) {
            pair.second->Disconnect();
        }
        acceptor_->Close();
        is_started_ = false;
    }

    std::string GetIPAddress() const {
        return ip_;
    }

    uint16_t GetPort() const {
        return port_;
    }

protected:
    virtual TcpConnectionPtr OnConnect(int sockfd) {
        return std::make_shared<TcpConnection>(
            event_loops_->GetNextIoTaskScheduler(), sockfd);
    }

    virtual void AddConnection(int sockfd, const TcpConnectionPtr &tcp_conn) {
        connections_.emplace(sockfd, tcp_conn);
    }

    virtual void RemoveConnection(int sockfd) {
        connections_.erase(sockfd);
    }

    EventLoops *event_loops_{};
    std::string ip_;
    uint16_t port_{};
    std::unique_ptr<Acceptor> acceptor_;
    std::atomic_bool is_started_;
    std::unordered_map<SocketFd, TcpConnectionPtr> connections_;
};

} // lsy::net

#endif // NET_TCPSERVER_H
