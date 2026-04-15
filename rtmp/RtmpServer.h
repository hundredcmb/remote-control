#ifndef RTMP_RTMPSERVER_H
#define RTMP_RTMPSERVER_H

#include <utility>

#include "net/TcpServer.h"
#include "rtmp/RtmpSession.h"

namespace lsy::net::rtmp {

class RtmpConfig {
public:
    RtmpConfig() = default;

    virtual ~RtmpConfig() = default;

    void SetChunkSize(uint32_t size) {
        if (size > 0 && size <= 60000) {
            max_chunk_size_ = size;
        }
    }

    void SetPeerBandwidth(uint32_t size) {
        peer_bandwidth_ = size;
    }

    [[nodiscard]] uint32_t GetChunkSize() const {
        return max_chunk_size_;
    }

    [[nodiscard]] uint32_t GetAcknowledgementSize() const {
        return acknowledgement_size_;
    }

    [[nodiscard]] uint32_t GetPeerBandwidth() const {
        return peer_bandwidth_;
    }

    [[nodiscard]] std::string GetStreamPath() const {
        return stream_path_;
    }

    [[nodiscard]] std::string GetApp() const {
        return app_;
    }

    [[nodiscard]] std::string GetStreamName() const {
        return stream_name_;
    }

    virtual int ParseRtmpUrl(const std::string &url) {
        // 1. 基础校验
        const std::string RTMP_PREFIX = "rtmp://";
        if (url.size() < RTMP_PREFIX.size() ||
            url.substr(0, RTMP_PREFIX.size()) != RTMP_PREFIX) {
            return -1;
        }
        std::string url_rest = url.substr(RTMP_PREFIX.size());

        // 2. 分割 主机地址 和 流路径
        size_t path_slash = url_rest.find('/');
        if (path_slash == std::string::npos) {
            return -1; // 无路径，非法URL
        }
        std::string host_part = url_rest.substr(0, path_slash);
        std::string stream_part = url_rest.substr(path_slash + 1);

        // 3. 解析主机地址: IP + 端口(默认1935)
        std::string ip;
        uint16_t port;
        size_t port_colon = host_part.find(':');
        if (port_colon != std::string::npos) {
            ip = host_part.substr(0, port_colon);
            std::string port_str = host_part.substr(port_colon + 1);
            try {
                size_t parse_idx;
                int port_num = std::stoi(port_str, &parse_idx);
                if (parse_idx != port_str.size() || port_num < 0 ||
                    port_num > UINT16_MAX) {
                    return -1;
                }
                port = static_cast<uint16_t>(port_num);
            } catch (...) {
                return -1;
            }
        } else {
            ip = host_part;
            port = 1935;
        }

        // 4. 解析流路径: 必须为 app/stream_name 格式
        size_t app_slash = stream_part.find('/');
        if (app_slash == std::string::npos) {
            return -1;
        }
        std::string app = stream_part.substr(0, app_slash);
        std::string stream_name = stream_part.substr(app_slash + 1);

        if (ip.empty() || app.empty() || stream_name.empty()) {
            return -1;
        }
        ip_ = std::move(ip);
        port_ = port;
        app_ = std::move(app);
        stream_name_ = std::move(stream_name);
        stream_path_ = "/" + std::move(stream_part);
        return 0;
    }

    uint16_t port_ = 1935;
    std::string ip_;
    std::string app_;
    std::string stream_name_;
    std::string stream_path_;

    uint32_t peer_bandwidth_ = 5000000;
    uint32_t acknowledgement_size_ = 5000000;
    uint32_t max_chunk_size_ = 128;
};

class RtmpServer;

using RtmpServerPtr = std::shared_ptr<RtmpServer>;

class RtmpServer
    : public TcpServer,
      public RtmpConfig,
      public std::enable_shared_from_this<RtmpServer> {
public:
    using EventCallback = std::function<void(std::string, std::string)>;

    static RtmpServerPtr
    Create(const EventLoopThreadPoolPtr &event_loops) {
        return std::shared_ptr<RtmpServer>(new RtmpServer(event_loops));
    }

    ~RtmpServer() override = default;

    void SetEventCallback(const EventCallback &event_cb) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        event_callbacks_.emplace_back(event_cb);
    }

private:
    friend class RtmpConnection;

    explicit RtmpServer(const EventLoopThreadPoolPtr &event_loops)
        : TcpServer(event_loops) {
        // 添加定时器, 用于清理无客户端的会话
        event_loops_->AddTimer([this]() -> bool {
            std::lock_guard<std::mutex> lock(this->session_mutex_);
            for (auto iter = this->rtmp_sessions_.begin();
                 iter != this->rtmp_sessions_.end();) {
                if (iter->second->GetClients() == 0) {
                    iter = this->rtmp_sessions_.erase(iter);
                } else {
                    ++iter;
                }
            }
            return true;
        }, 3000);
    }

    void AddSession(const std::string &stream_path) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (!rtmp_sessions_.count(stream_path)) {
            rtmp_sessions_[stream_path] = std::make_shared<RtmpSession>();
        }
    }

    void RemoveSession(const std::string &stream_path) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        rtmp_sessions_.erase(stream_path);
    }

    RtmpSessionPtr GetSession(const std::string &stream_path) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        auto it = rtmp_sessions_.find(stream_path);
        if (it != rtmp_sessions_.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool HasSession(const std::string &stream_path) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        return rtmp_sessions_.count(stream_path) > 0;
    }

    bool HasPublisher(const std::string &stream_path) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        RtmpSessionPtr session = GetSession(stream_path);
        if (!session) {
            return false;
        }
        return (session->GetPublisher() != nullptr);
    }

    void NotifyEvent(const std::string &event_type,
                     const std::string &stream_path) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        for (auto &event_cb: event_callbacks_) {
            event_cb(event_type, stream_path);
        }
    }

    TcpConnectionPtr CreateConnection(int sockfd) override {
        return std::make_shared<RtmpConnection>(
            event_loops_->GetNextIoTaskScheduler(), sockfd, this);
    }

    std::mutex session_mutex_;
    EventLoopThreadPoolPtr event_loops_;
    std::unordered_map<std::string, RtmpSessionPtr> rtmp_sessions_;
    std::vector<EventCallback> event_callbacks_;
};

} // lsy::net::rtmp

#endif // RTMP_RTMPSERVER_H
