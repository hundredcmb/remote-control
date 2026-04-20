#ifndef NET_TCPCONNECTION_H
#define NET_TCPCONNECTION_H

#include <unistd.h>

#include "TcpSocket.h"
#include "BufferReader.h"
#include "BufferWriter.h"
#include "TaskScheduler.h"

namespace lsy::net {
class TcpConnection;

/**
 * @brief TCP连接智能指针类型
 */
using TcpConnectionPtr = std::shared_ptr<TcpConnection>;

/**
 * @brief TCP连接管理类
 * @details 封装TCP连接的全生命周期管理，基于事件驱动实现非阻塞数据收发，
 *          提供读写/关闭/断开回调注册、套接字管理、缓冲区操作等核心功能，
 *          支持智能指针管理生命周期
 */
class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    /**
     * @brief 断开连接回调函数类型
     * @param TcpConnectionPtr 触发断开事件的TCP连接智能指针
     */
    using DisconnectCallback = std::function<void(TcpConnectionPtr)>;

    /**
     * @brief 关闭连接回调函数类型
     * @param TcpConnectionPtr 触发关闭事件的TCP连接智能指针
     */
    using CloseCallback = std::function<void(TcpConnectionPtr)>;

    /**
     * @brief 读数据回调函数类型
     * @param TcpConnectionPtr 触发读事件的TCP连接智能指针
     * @param BufferReader 读缓冲区对象，存储从套接字读取的数据
     */
    using ReadCallback = std::function<void(TcpConnectionPtr, BufferReader &)>;

    /**
     * @brief 构造函数
     * @param task_scheduler 任务调度器指针，负责连接的事件监听与调度
     * @param sockfd TCP连接对应的套接字文件描述符
     */
    TcpConnection(TaskSchedulerPtr task_scheduler, int sockfd)
        : task_scheduler_(std::move(task_scheduler)),
          read_buffer_(new BufferReader()),
          write_buffer_(new BufferWriter()),
          channel_(new Channel(sockfd)),
          is_closed_(false) {
        SocketUtil::SetNonBlock(sockfd);
        SocketUtil::SetSendBufSize(sockfd, kSendBufSize);
        SocketUtil::SetKeepAlive(sockfd);
    }

    /**
     * @brief 启用TCP连接事件监听
     * @details 创建Channel对象，并设置读、写、错误、关闭事件回调，
     *          并开启读事件监听，将Channel对象添加到任务调度器中
     */
    void EnableCallbacks() {
        TcpConnectionPtr self = shared_from_this();
        channel_->SetReadCallback([self]() {
            self->HandleRead();
        });
        channel_->SetWriteCallback([self]() {
            self->HandleWrite();
        });
        channel_->SetErrorCallback([self]() {
            self->HandleError();
        });
        channel_->SetCloseCallback([self]() {
            self->HandleClose();
        });
        channel_->EnableReading();
        task_scheduler_->UpdateChannel(channel_);
    }

    /**
     * @brief 禁用TCP连接事件监听
     * @details 禁用读、写、错误、关闭事件监听，并清空回调函数对象
     */
    void DisableCallbacks() {
        channel_->DisableAll();
        channel_->SetReadCallback(nullptr);
        channel_->SetWriteCallback(nullptr);
        channel_->SetErrorCallback(nullptr);
        channel_->SetCloseCallback(nullptr);
    }

    /**
     * @brief 析构函数
     * @details 关闭套接字文件描述符，释放TCP连接占用的系统资源
     */
    virtual ~TcpConnection() {
        FileDescriptor fd = GetSocket();
        if (fd >= 0) {
            ::close(fd);
        }
    }

    /**
     * @brief 获取任务调度器指针
     * @return 指向任务调度器的指针
     */
    TaskSchedulerPtr GetTaskScheduler() const {
        return task_scheduler_;
    }

    /**
     * @brief 设置读数据回调函数
     * @param cb 读事件触发时的回调函数对象
     */
    void SetReadCallback(const ReadCallback &cb) {
        read_cb_ = cb;
    }

    /**
     * @brief 设置关闭连接回调函数
     * @param cb 连接关闭时的回调函数对象
     */
    void SetCloseCallback(const CloseCallback &cb) {
        close_cb_ = cb;
    }

    /**
     * @brief 发送共享指针管理的数据
     * @param data 待发送数据的共享指针
     * @param size 待发送数据的长度（字节）
     */
    void Send(const std::shared_ptr<char[]> &data, uint32_t size) {
        if (!is_closed_) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (write_buffer_->Append(data, size)) {
                lock.unlock();
                HandleWrite();
            }
        }
    }

    /**
     * @brief 发送常量数据
     * @param data 待发送数据的常量指针
     * @param size 待发送数据的长度（字节）
     */
    void Send(const char *data, size_t size) {
        if (!is_closed_) {
            std::unique_lock<std::mutex> lock(mutex_);
            if (write_buffer_->Append(data, size)) {
                lock.unlock();
                HandleWrite();
            }
        }
    }

    /**
     * @brief 主动断开TCP连接
     */
    void Disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);
        Close();
    }

    /**
     * @brief 判断连接是否已关闭
     * @return 已关闭返回true，未关闭返回false
     */
    inline bool IsClosed() const {
        return is_closed_;
    }

    /**
     * @brief 获取TCP连接的套接字文件描述符
     * @return 套接字文件描述符
     */
    inline FileDescriptor GetSocket() const {
        return channel_->GetFd();
    }

protected:
    /**
     * @brief 友元类声明
     * @details TcpServer为友元类，可访问TcpConnection的非公开成员
     */
    friend class TcpServer;

    /**
     * @brief 处理套接字读事件
     * @details 从套接字读取数据到读缓冲区，读取成功后触发读回调
     */
    virtual void HandleRead() {
        if (is_closed_) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            int64_t ret = read_buffer_->ReadFd(GetSocket());
            if (ret <= 0) {
                Close();
                return;
            }
        }
        if (read_cb_) {
            read_cb_(shared_from_this(), *read_buffer_);
        }
    }

    /**
     * @brief 处理套接字写事件
     * @details 将写缓冲区的数据发送到套接字，根据发送结果更新写事件监听
     */
    virtual void HandleWrite() {
        if (is_closed_) {
            return;
        }
        if (!mutex_.try_lock()) {
            return;
        }
        int64_t ret = write_buffer_->Send(GetSocket());
        if (ret < 0) {
            Close();
        } else if (ret == 0 && !write_buffer_->Empty()) {
            // 数据没发完, 监听写事件
            channel_->EnableWriting();
            task_scheduler_->UpdateChannel(channel_);
        } else if (channel_->IsWriting()) {
            // 数据已发完, 取消监听写事件
            channel_->DisableWriting();
            task_scheduler_->UpdateChannel(channel_);
        }
        mutex_.unlock();
    }

    /**
     * @brief 处理连接关闭事件
     */
    virtual void HandleClose() {
        std::lock_guard<std::mutex> lock(mutex_);
        Close();
    }

    /**
     * @brief 处理连接错误事件
     */
    virtual void HandleError() {
        Close();
    }

    /**
     * @brief 设置断开连接回调函数
     * @param cb 连接断开时的回调函数对象
     */
    void SetDisconnectCallback(const DisconnectCallback &cb) {
        disconnect_cb_ = cb;
    }

    /// 任务调度器指针，负责连接的事件调度
    TaskSchedulerPtr task_scheduler_;
    /// 读缓冲区智能指针，存储从套接字读取的数据
    std::unique_ptr<BufferReader> read_buffer_;
    /// 写缓冲区智能指针，存储待发送到套接字的数据
    std::unique_ptr<BufferWriter> write_buffer_;
    /// 连接关闭状态原子标记，保证多线程安全
    std::atomic_bool is_closed_;

private:
    /**
     * @brief 关闭TCP连接
     * @details 标记连接为关闭状态，移除事件监听，触发关闭/断开回调
     */
    void Close() {
        if (is_closed_) {
            return;
        }
        is_closed_ = true;
        task_scheduler_->RemoveChannel(channel_);
        TcpConnectionPtr self = shared_from_this();
        if (close_cb_) {
            close_cb_(self);
        }
        if (disconnect_cb_) {
            disconnect_cb_(self);
        }

        // 在事件循环中清理回调函数中的self引用计数(安全释放self内存)
        task_scheduler_->AddTimer([self]() {
            self->DisableCallbacks();
            return false;
        }, 0);
    }

    /// 事件通道智能指针，管理套接字的读写/错误/关闭事件
    std::shared_ptr<Channel> channel_;
    /// 断开连接回调函数对象
    DisconnectCallback disconnect_cb_;
    /// 关闭连接回调函数对象
    CloseCallback close_cb_;
    /// 读数据回调函数对象
    ReadCallback read_cb_;
    /// TCP连接默认发送缓冲区大小（32*4096字节）
    static constexpr int kSendBufSize = 32 * 4096;
    std::mutex mutex_;
};
} // lsy::net

#endif // NET_TCPCONNECTION_H
