#ifndef NET_ACCEPTOR_H
#define NET_ACCEPTOR_H

#include <memory>
#include <functional>

#include "Channel.h"
#include "TcpSocket.h"
#include "TaskScheduler.h"

namespace lsy::net {

/**
 * @brief 新连接回调函数类型
 * @param SocketFd 新建立的客户端套接字文件描述符
 */
using NewConnectionCallback = std::function<void(SocketFd)>;

/**
 * @brief TCP连接接收器
 * @details 负责监听指定IP端口，接收客户端TCP连接，
 *          封装socket创建、绑定、监听、接受连接的完整流程，
 *          接收新连接后通过回调函数通知上层
 */
class Acceptor {
public:
    /**
     * @brief 构造函数
     * @param eventLoop 绑定的任务调度器（事件循环）
     */
    explicit Acceptor(TaskSchedulerPtr eventLoop)
        : task_scheduler_(std::move(eventLoop)),
          tcp_socket_(new TcpSocket()) {
    }

    /**
     * @brief 虚析构函数
     */
    virtual ~Acceptor() = default;

    /**
     * @brief 设置新连接回调函数
     * @param cb 接收到新客户端连接时触发的回调
     */
    void SetNewConnectionCallback(const NewConnectionCallback &cb) {
        new_connection_callback_ = cb;
    }

    /**
     * @brief 启动监听
     * @param ip 监听的IP地址
     * @param port 监听的端口号
     * @return 成功返回0，失败返回-1
     * @details 创建socket、设置非阻塞/地址复用、绑定、监听，注册读事件
     */
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
            return -1;
        }

        channel_ptr_->SetReadCallback([this]() -> void {
            this->OnAccept();
        });
        channel_ptr_->EnableReading();
        task_scheduler_->UpdateChannel(channel_ptr_);
        return 0;
    }

    /**
     * @brief 关闭接收器
     * @details 移除事件监听，关闭监听套接字，释放资源
     */
    void Close() {
        if (tcp_socket_->GetFd() > 0) {
            task_scheduler_->RemoveChannel(channel_ptr_);
            tcp_socket_->Close();
        }
    }

private:
    /**
     * @brief 处理接受连接事件
     * @details 当监听socket可读时调用，接受新连接并触发回调
     */
    void OnAccept() {
        SocketFd fd = tcp_socket_->Accept();
        if (fd >= 0) {
            if (new_connection_callback_) {
                new_connection_callback_(fd);
            }
        }
    }

    /// 绑定的任务调度器（事件循环）
    TaskSchedulerPtr task_scheduler_;
    /// 监听socket对应的事件通道
    ChannelPtr channel_ptr_;
    /// TCP套接字对象，负责底层网络操作
    std::unique_ptr<TcpSocket> tcp_socket_;
    /// 新连接到达时的回调函数
    NewConnectionCallback new_connection_callback_;
};

} // lsy::net

#endif // NET_ACCEPTOR_H