#ifndef NET_TCPSERVER_H
#define NET_TCPSERVER_H

#include "Acceptor.h"
#include "TcpConnection.h"
#include "EventLoopThreadPool.h"

namespace lsy::net {

class TcpServer {
public:
    explicit TcpServer(const EventLoopThreadPoolPtr &event_loops)
        : is_started_(false),
          event_loops_(event_loops),
          acceptor_(new Acceptor(event_loops->GetAcceptTaskScheduler())),
          port_(0) {
        acceptor_->SetNewConnectionCallback([this](SocketFd sockfd) -> void {
            TcpConnectionPtr conn = CreateConnection(sockfd);
            AddConnection(sockfd, conn);
            this->OnConnect(conn);
            conn->SetDisconnectCallback(
                [this](const TcpConnectionPtr &tcp_conn) -> void {
                    this->RemoveConnection(tcp_conn->GetSocket());
                }
            );
        });
    }

    virtual ~TcpServer() {
        Stop();
    }

    bool Start(const std::string &ip, uint16_t port) {
        Stop();

        std::lock_guard<std::mutex> lock(mutex_);
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
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_started_) {
            return;
        }
        for (auto &pair: connections_) {
            pair.second->Disconnect();
        }
        acceptor_->Close();
        is_started_ = false;
    }

    std::string GetIpAddress() const {
        return ip_;
    }

    uint16_t GetPort() const {
        return port_;
    }

    TcpConnectionPtr CreateConnection(int sockfd) {
        return std::make_shared<TcpConnection>(
            event_loops_->GetNextIoTaskScheduler(), sockfd);
    }

    TcpConnectionPtr GetConnection(SocketFd sockfd) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(sockfd);
        return (it != connections_.end()) ? it->second : nullptr;
    }

    void AddConnection(int sockfd, const TcpConnectionPtr &conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.emplace(sockfd, conn);
    }

    void RemoveConnection(int sockfd) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.erase(sockfd);
    }

protected:
    virtual void OnConnect(const TcpConnectionPtr &conn) {
        // conn->SetReadCallback(...);
        // conn->SetCloseCallback(...);
    };

    EventLoopThreadPoolPtr event_loops_;
    std::string ip_;
    uint16_t port_;
    std::unique_ptr<Acceptor> acceptor_;
    bool is_started_;
    std::unordered_map<SocketFd, TcpConnectionPtr> connections_;
    std::mutex mutex_;
};

} // lsy::net

#endif // NET_TCPSERVER_H
