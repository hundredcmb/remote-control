#ifndef NET_TCPSERVER_H
#define NET_TCPSERVER_H

#include "Acceptor.h"
#include "TcpConnection.h"
#include "EventLoopThreadPool.h"

namespace lsy::net {

/**
 * @brief TCP服务器类
 * @details 网络库顶层服务类，整合接收器、事件循环线程池、连接管理，
 *          实现TCP服务的启动、停止、连接创建与销毁，支持派生类扩展业务逻辑
 */
class TcpServer {
public:
    /**
     * @brief 构造函数
     * @param event_loops 事件循环线程池智能指针
     * @details 初始化接收器，绑定新连接回调，管理所有TCP连接
     */
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

    /**
     * @brief 析构函数
     * @details 自动停止服务，释放所有连接与资源
     */
    virtual ~TcpServer() {
        Stop();
    }

    /**
     * @brief 启动TCP服务
     * @param ip 监听IP地址
     * @param port 监听端口
     * @return 成功返回true，失败返回false
     * @details 启动接收器监听，初始化服务状态
     */
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

    /**
     * @brief 停止TCP服务
     * @details 断开所有连接，关闭接收器，重置服务状态
     */
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

    /**
     * @brief 获取监听IP地址
     * @return 服务监听的IP字符串
     */
    std::string GetIpAddress() const {
        return ip_;
    }

    /**
     * @brief 获取监听端口
     * @return 服务监听的端口号
     */
    uint16_t GetPort() const {
        return port_;
    }

    /**
     * @brief 创建TCP连接
     * @param sockfd 客户端套接字文件描述符
     * @return 新创建的TCP连接智能指针
     * @details 从线程池获取调度器，创建TcpConnection实例
     */
    virtual TcpConnectionPtr CreateConnection(int sockfd) {
        return std::make_shared<TcpConnection>(
            event_loops_->GetNextIoTaskScheduler(), sockfd);
    }

    /**
     * @brief 根据套接字获取连接
     * @param sockfd 套接字文件描述符
     * @return 对应TCP连接智能指针，不存在返回nullptr
     */
    TcpConnectionPtr GetConnection(SocketFd sockfd) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = connections_.find(sockfd);
        return (it != connections_.end()) ? it->second : nullptr;
    }

    /**
     * @brief 添加新连接到管理容器
     * @param sockfd 套接字文件描述符
     * @param conn TCP连接智能指针
     */
    void AddConnection(int sockfd, const TcpConnectionPtr &conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.emplace(sockfd, conn);
    }

    /**
     * @brief 从管理容器移除连接
     * @param sockfd 套接字文件描述符
     */
    void RemoveConnection(int sockfd) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.erase(sockfd);
    }

protected:
    /**
     * @brief 新连接建立回调（可被子类重写）
     * @param conn 新建立的TCP连接智能指针
     * @details 派生类可重写此函数设置读写/关闭回调
     */
    virtual void OnConnect(const TcpConnectionPtr &conn) {
        // conn->SetReadCallback(...);
        // conn->SetCloseCallback(...);
    };

    /// 事件循环线程池智能指针
    EventLoopThreadPoolPtr event_loops_;
    /// 监听IP地址
    std::string ip_;
    /// 监听端口号
    uint16_t port_;
    /// TCP连接接收器，负责接收客户端连接
    std::unique_ptr<Acceptor> acceptor_;
    /// 服务启动状态标记
    bool is_started_;
    /// 套接字 -> TCP连接映射表，管理所有活跃连接
    std::unordered_map<SocketFd, TcpConnectionPtr> connections_;
    /// 互斥锁，保护连接容器与服务状态的线程安全
    std::mutex mutex_;
};

} // lsy::net

#endif // NET_TCPSERVER_H