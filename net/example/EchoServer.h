#include "TcpServer.h"
#include "TcpConnection.h"
#include "EventLoopThreadPool.h"

#include <string>
#include <cstdio>

using namespace lsy::net;

class EchoServer : public TcpServer {
public:
    using TcpServer::TcpServer;

protected:
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

#if 0
int main() {
    const uint32_t kThreadNum = 4;

    EventLoopThreadPoolPtr loop_pool = std::make_shared<EventLoopThreadPool>(
        kThreadNum);
    EchoServer server(loop_pool);

    if (!server.Start("0.0.0.0", 8888)) {
        fprintf(stderr, "[EchoServer] failed to start server!\n");
        return -1;
    }

    printf("[EchoServer] server started, listening on port 8888\n");
    printf("[EchoServer] thread pool size: %u\n", kThreadNum);

    loop_pool->Loop();
    return 0;
}
#endif
