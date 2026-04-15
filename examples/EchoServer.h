#ifndef NET_ECHOSERVER_H
#define NET_ECHOSERVER_H

#include "net/TcpServer.h"
#include "net/TcpConnection.h"
#include "net/EventLoopThreadPool.h"

#include <string>
#include <cstdio>

namespace lsy::net {

/**
 * @brief 回显服务器示例类
 * @details 继承自TcpServer，实现最简单的Echo业务逻辑：
 *          接收客户端数据后立即原样回写，用于测试网络库功能
 */
class EchoServer : public TcpServer {
public:
    explicit EchoServer(const EventLoopThreadPoolPtr &event_loops)
        : TcpServer(event_loops) {
        // 创建定时任务, 每秒打印连接数
        event_loops->AddTimer([this]() -> bool {
            std::lock_guard<std::mutex> lock(this->mutex_);
            printf("[EchoServer] connections: %lu\n", this->connections_.size());
            return true;
        }, 1000);
    }

protected:
    /**
     * @brief 重写新连接建立回调
     * @param conn 新建立的TCP连接智能指针
     * @details 为新连接设置【数据读取回调】和【连接关闭回调】，实现Echo回显功能
     */
    void OnConnect(const TcpConnectionPtr &conn) override {
        if (!conn) {
            return;
        }

        // 新连接提示
        printf("[EchoServer] new connection established, fd=%d\n",
               conn->GetSocket());

        // 收到数据并回显
        conn->SetReadCallback(
            [](const TcpConnectionPtr &conn_ptr, BufferReader &buffer) {
                std::string msg = buffer.RetrieveAllAsString();
                printf("[EchoServer] recv from fd=%d, data: %s\n",
                       conn_ptr->GetSocket(), msg.c_str());

                // Echo 回写
                conn_ptr->Send(msg.data(), static_cast<uint32_t>(msg.size()));
            }
        );

        // 连接关闭提示
        conn->SetCloseCallback(
            [](const TcpConnectionPtr &conn_ptr) {
                printf("[EchoServer] connection closed, fd=%d\n",
                       conn_ptr->GetSocket());
            }
        );
    }
};

} // lsy::net

#endif // NET_ECHOSERVER_H

#if 0
/**
 * @brief EchoServer 主函数示例
 * @details 初始化4线程事件循环池，启动Echo服务器监听8888端口
 * @return 程序退出状态码
 */
int main() {
    // 线程池线程数量
    constexpr uint32_t kThreadNum = 4;

    // 创建事件循环线程池
    EventLoopThreadPoolPtr event_loops = std::make_shared<EventLoopThreadPool>(
        kThreadNum);

    // 创建Echo服务器
    EchoServer server(event_loops);

    // 启动服务器，监听0.0.0.0:8888
    if (!server.Start("0.0.0.0", 8888)) {
        fprintf(stderr, "[EchoServer] failed to start server!\n");
        return -1;
    }
    printf("[EchoServer] server started, listening on port 8888\n");
    printf("[EchoServer] thread pool size: %u\n", kThreadNum);

    // 启动事件循环
    event_loops->Loop();
    return 0;
}
#endif