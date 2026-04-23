#include "rtmp/RtmpServer.h"
#include "http-flv/HttpFlvServer.h"

using namespace lsy::net;

int main() {
    // 线程池线程数量
    constexpr uint32_t kThreadNum = 1;

    // 创建事件循环线程池
    EventLoopThreadPoolPtr rtmp_event_loops = std::make_shared<EventLoopThreadPool>(
        kThreadNum);
    EventLoopThreadPoolPtr http_event_loops = std::make_shared<EventLoopThreadPool>(
        kThreadNum);

    // 创建RTMP服务器(0.0.0.0:1935)
    rtmp::RtmpServerPtr rtmp_server = rtmp::RtmpServer::Create(
        rtmp_event_loops);
    rtmp_server->SetChunkSize(60000);
    rtmp_server->SetEventCallback(
        [](const std::string &type, const std::string &path) {
            fprintf(stderr, "[RtmpServer] %s: '%s'\n", type.c_str(),
                    path.c_str());
        }
    );
    constexpr uint16_t kRtmpPort = 1935;
    rtmp_server->Start("0.0.0.0", kRtmpPort);
    fprintf(stderr, "[RtmpServer] listening on port %d, "
                    "EventLoopThreadPool size is %u\n", kRtmpPort, kThreadNum);

    // 创建 HTTP-FLV服务器(0.0.0.0:9000)
    http::HttpFlvServerPtr http_server = http::HttpFlvServer::Create(
        rtmp_server, http_event_loops);
    constexpr uint16_t kHttpPort = 9000;
    http_server->Start("0.0.0.0", kHttpPort);
    fprintf(stderr, "[HttpFlvServer] listening on port %d, "
                    "EventLoopThreadPool size is %u\n", kHttpPort, kThreadNum);

    // 启动RTMP服务器
    std::thread rtmp_server_thread([rtmp_event_loops]() {
        rtmp_event_loops->Loop();
    });

    // 启动HTTP-FLV服务器
    http_event_loops->Loop();

    if (rtmp_server_thread.joinable()) {
        rtmp_server_thread.join();
    }
    return 0;
}
