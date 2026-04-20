#include "rtmp/RtmpServer.h"

using namespace lsy::net;

int main() {
    // 线程池线程数量
    constexpr uint32_t kThreadNum = 4;

    // 创建事件循环线程池
    EventLoopThreadPoolPtr event_loops = std::make_shared<EventLoopThreadPool>(
        kThreadNum);

    // 创建RTMP服务器
    rtmp::RtmpServerPtr server = rtmp::RtmpServer::Create(event_loops);
    server->SetChunkSize(60000);
    server->SetEventCallback(
        [](const std::string &type, const std::string &path) {
            fprintf(stderr, "[RtmpServer] %s: '%s'\n", type.c_str(),
                    path.c_str());
        }
    );

    // 服务器监听 0.0.0.0:1935
    constexpr uint16_t kPort = 1935;
    server->Start("0.0.0.0", kPort);
    fprintf(stderr, "[RtmpServer] listening on port %d, "
                    "EventLoopThreadPool size is %u\n", kPort, kThreadNum);

    // 启动事件循环
    event_loops->Loop();
    return 0;
}
