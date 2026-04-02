#include "example/EchoServer.h"

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
