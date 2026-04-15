#include "examples/EchoServer.h"

using namespace lsy::net;

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
