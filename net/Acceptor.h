#ifndef NET_ACCEPTOR_H
#define NET_ACCEPTOR_H

#include <functional>
#include <memory>
#include <utility>

#include "Channel.h"
#include "TcpSocket.h"
#include "TaskScheduler.h"

namespace lsy::net {

using NewConnectionCallback = std::function<void(SocketFd)>;

class Acceptor {
public:
    explicit Acceptor(TaskSchedulerPtr eventLoop)
        : task_scheduler_(std::move(eventLoop)),
          tcp_socket_(new TcpSocket()) {
    }

    virtual ~Acceptor() = default;

    void SetNewConnectionCallback(const NewConnectionCallback &cb) {
        new_connection_callback_ = cb;
    }

    int Listen(const std::string &ip, uint16_t port) {
        if (tcp_socket_->GetFd() > 0) {
            tcp_socket_->Close();
        }
        SocketFd fd = tcp_socket_->Create();
        SocketUtil::SetNonBlock(fd);
        SocketUtil::SetReuseAddr(fd);
        SocketUtil::SetReusePort(fd);
        channel_ptr_ = std::make_shared<Channel>(fd);

        if (!tcp_socket_->Bind(ip, port)) {
            return -1;
        }
        if (!tcp_socket_->Listen(4096)) {
            return -2;
        }

        channel_ptr_->SetReadCallback([this]() -> void { this->OnAccept(); });
        channel_ptr_->EnableReading();
        task_scheduler_->UpdateChannel(channel_ptr_);
        return 0;
    }

    void Close() {
        if (tcp_socket_->GetFd() > 0) {
            task_scheduler_->RemoveChannel(channel_ptr_);
            tcp_socket_->Close();
        }
    }

private:
    void OnAccept() {
        SocketFd fd = tcp_socket_->Accept();
        if (fd >= 0) {
            if (new_connection_callback_) {
                new_connection_callback_(fd);
            }
        }
    }

    TaskSchedulerPtr task_scheduler_;
    ChannelPtr channel_ptr_;
    std::unique_ptr<TcpSocket> tcp_socket_;
    NewConnectionCallback new_connection_callback_;
};

} // lsy::net

#endif // NET_ACCEPTOR_H
