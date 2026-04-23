#include "HttpFlvConnection.h"

#include "base/ByteIO.h"
#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "rtmp/RtmpServer.h"
#include "rtmp/RtmpMessage.h"

namespace lsy::net::http {

HttpFlvConnection::HttpFlvConnection(
    const std::shared_ptr<rtmp::RtmpServer> &server,
    const TaskSchedulerPtr &scheduler, int sockfd)
    : TcpConnection(scheduler, sockfd),
      rtmp_server_(server),
      http_context_(new HttpContext()) {
    TcpConnection::SetReadCallback(
        [this](const TcpConnectionPtr &conn_ptr, BufferReader &buffer) {
            this->OnRead(buffer);
        }
    );
    TcpConnection::SetCloseCallback(
        [this](const TcpConnectionPtr &conn_ptr) {
            this->OnClose();
        }
    );
}

HttpFlvConnection::~HttpFlvConnection() = default;

bool HttpFlvConnection::IsPlayer() {
    return true;
}

bool HttpFlvConnection::IsPublisher() {
    return false;
}

bool HttpFlvConnection::IsPlaying() {
    return is_playing_;
}

bool HttpFlvConnection::IsPublishing() {
    return false;
}

uint32_t HttpFlvConnection::GetId() {
    return static_cast<uint32_t>(TcpConnection::GetSocket());
}

bool HttpFlvConnection::SendMetaData(const rtmp::AmfObjects &meta_data) {
    if (meta_data.empty()) {
        fprintf(stderr, "Empty MetaData\n");
        return false;
    } else if (IsClosed()) {
        return true;
    }
    amf_encoder_.Reset();
    amf_encoder_.EncodeString("onMetaData", 10);
    amf_encoder_.EncodeECMA(meta_data);
    if (!SendFlvScriptTag(amf_encoder_.Data(), amf_encoder_.Size())) {
        return false;
    }
    return true;
}

bool HttpFlvConnection::SendFlvScriptTag(std::unique_ptr<uint8_t[]> &&payload,
                                         uint32_t payload_size) {
    if (payload_size == 0) {
        return false;
    } else if (IsClosed()) {
        return true;
    }

    // 生成TAG数据
    size_t offset = 0, buffer_size = payload_size + 4 + kFlvTagHeaderSize;
    std::shared_ptr<char[]> buffer(new char[buffer_size]());
    if (!ByteIO::WriteUInt32BE((uint8_t *) buffer.get(), buffer_size, offset,
                               previous_tag_size_)) {
        return false;
    }
    if (!ByteIO::WriteUInt8((uint8_t *) buffer.get(), buffer_size, offset,
                            rtmp::Flv::TAG_TYPE_SCRIPT)) {
        return false;
    }
    if (!ByteIO::WriteUInt24BE((uint8_t *) buffer.get(), buffer_size, offset,
                               payload_size)) {
        return false;
    }
    offset += 7; // 4B时间戳=0, 3B流ID=0
    if (!ByteIO::WriteBytes((uint8_t *) buffer.get(), buffer_size, offset,
                            payload.get(), payload_size)) {
        return false;
    }

    // 发送数据
    TcpConnection::Send(buffer, buffer_size);
    previous_tag_size_ = static_cast<uint32_t>(buffer_size) - 4;
    return true;
}

bool HttpFlvConnection::SendMediaData(uint8_t type, uint64_t timestamp,
                                      std::shared_ptr<uint8_t[]> payload,
                                      uint32_t payload_size) {
    if (payload_size == 0) {
        fprintf(stderr, "Empty MediaData\n");
        return false;
    } else if (IsClosed()) {
        return true;
    }
    is_playing_ = true;

    // 缓存 sequence_header
    if (type == rtmp::MediaDataType::AVC_SEQUENCE_HEADER) {
        avc_sequence_header_ = payload;
        avc_sequence_header_size_ = payload_size;
    } else if (type == rtmp::MediaDataType::AAC_SEQUENCE_HEADER) {
        aac_sequence_header_ = payload;
        aac_sequence_header_size_ = payload_size;
    }

    // 确保发送的首个视频帧是I帧
    std::unique_ptr<uint8_t[]> buf(new uint8_t[payload_size]());
    memcpy(buf.get(), payload.get(), payload_size);
    if (!has_key_frame_ && avc_sequence_header_size_ > 0 &&
        type == rtmp::MediaDataType::AVC_VIDEO) {
        if (IsAvcKeyFrame(buf, payload_size)) {
            has_key_frame_ = true;
        } else {
            return true;
        }
    }

    // 映射 FLV Tag 类型
    uint8_t flv_tag_type;
    if (type == rtmp::MediaDataType::AAC_AUDIO ||
        type == rtmp::MediaDataType::AAC_SEQUENCE_HEADER) {
        flv_tag_type = rtmp::Flv::TAG_TYPE_AUDIO;
    } else if (type == rtmp::MediaDataType::AVC_VIDEO ||
               type == rtmp::MediaDataType::AVC_SEQUENCE_HEADER) {
        flv_tag_type = rtmp::Flv::TAG_TYPE_VIDEO;
    } else {
        return false;
    }

    // 生成TAG数据
    size_t offset = 0, buffer_size = 4 + kFlvTagHeaderSize + payload_size;
    std::shared_ptr<char[]> buffer(new char[buffer_size]());
    if (!ByteIO::WriteUInt32BE((uint8_t *) buffer.get(), buffer_size, offset,
                               previous_tag_size_)) {
        return false;
    }
    if (!ByteIO::WriteUInt8((uint8_t *) buffer.get(), buffer_size, offset,
                            flv_tag_type)) {
        return false;
    }
    if (!ByteIO::WriteUInt24BE((uint8_t *) buffer.get(), buffer_size, offset,
                               payload_size)) {
        return false;
    }
    auto ts = (uint32_t) (timestamp & 0xFFFFFFFF);
    uint8_t ts_ex = (ts >> 24) & 0xFF;  // 时间戳扩展位
    uint32_t ts_base = ts & 0xFFFFFF;   // 基础时间戳
    if (!ByteIO::WriteUInt24BE((uint8_t *) buffer.get(), buffer_size, offset,
                               ts_base)) {
        return false;
    }
    if (!ByteIO::WriteUInt8((uint8_t *) buffer.get(), buffer_size, offset,
                            ts_ex)) {
        return false;
    }
    offset += 3; // 3B流ID=0
    if (!ByteIO::WriteBytes((uint8_t *) buffer.get(), buffer_size, offset,
                            payload.get(), payload_size)) {
        return false;
    }
    TcpConnection::Send(buffer, buffer_size);
    previous_tag_size_ = static_cast<uint32_t>(buffer_size) - 4;
    return true;
}

bool HttpFlvConnection::IsAvcKeyFrame(std::unique_ptr<uint8_t[]> &payload,
                                      uint32_t payload_size) {
    if (payload_size < 1) {
        return false;
    }
    uint8_t frame_type = payload.get()[0] >> 4;
    uint8_t codec_id = payload.get()[0] & 0x0f;
    //printf("frame_type: %d, codec_id: %d\n", frame_type, codec_id);
    return (frame_type == rtmp::Flv::FRAME_TYPE_I &&
            codec_id == rtmp::Flv::CODEC_ID_AVC);
}

bool HttpFlvConnection::OnRead(BufferReader &buffer) {
    if (!http_context_) {
        return false;
    }
    if (!http_context_->parseRequest(buffer, Timestamp::now())) {
        const char *resp = "HTTP/1.1 400 Bad Request\r\n\r\n";
        Send(resp, strlen(resp));
        fprintf(stderr, "parse http request failed\n");
        return false;
    }

    if (http_context_->gotAll()) {
        onRequest(http_context_->request());
        http_context_->reset();
    }
    return true;
}

bool HttpFlvConnection::HandleRequest(const HttpRequest &req,
                                      HttpResponse &resp) {
    if (req.method() != HttpRequest::kGet) {
        resp.setStatusCode(HttpResponse::k405MethodNotAllowed);
        resp.setStatusMessage("Method Not Allowed");
        fprintf(stderr, "Method Not Allowed");
        return false;
    }

    // 获取请求参数
    std::string path = req.path();
    if (!ParseStreamPath(path, app_, stream_name_)) {
        resp.setStatusCode(HttpResponse::k404NotFound);
        resp.setStatusMessage("Stream Not Found");
        fprintf(stderr, "Invalid flv path: %s\n", path.c_str());
        return false;
    }
    stream_path_ = "/" + app_ + "/" + stream_name_;

    // 查找 RTMP 会话
    rtmp::RtmpServerPtr server = rtmp_server_.lock();
    if (!server) {
        resp.setStatusCode(HttpResponse::k404NotFound);
        resp.setStatusMessage("Server Expired");
        fprintf(stderr, "RtmpServer is expired\n");
        return false;
    }
    rtmp_session_ = server->GetSession(stream_path_);
    rtmp::RtmpSessionPtr session = rtmp_session_.lock();
    if (!session) {
        resp.setStatusCode(HttpResponse::k404NotFound);
        resp.setStatusMessage("Stream Not Found");
        fprintf(stderr, "Stream not found: %s\n", stream_path_.c_str());
        return false;
    }

    // 构建 HTTP-FLV 响应头
    resp.setStatusCode(HttpResponse::k200Ok);
    resp.setStatusMessage("OK");
    resp.addHeader("Content-Type", "video/x-flv");  // FLV 媒体类型
    resp.addHeader("Connection", "keep-alive");     // 长连接
    resp.addHeader("Cache-Control", "no-cache");    // 无缓存
    resp.addHeader("Pragma", "no-cache");

    // 发送响应
    std::vector<char> response = resp.getOutputBuffer();
    std::shared_ptr<char[]> resp_buf(new char[response.size()]);
    std::memcpy(resp_buf.get(), response.data(), response.size());
    TcpConnection::Send(resp_buf, static_cast<uint32_t>(response.size()));

    // 发送 FLV 文件头
    std::shared_ptr<char[]> flv_header(new char[kFlvHeaderSize]);
    std::memcpy(flv_header.get(), kFlvHeader, kFlvHeaderSize);
    TcpConnection::Send(flv_header, kFlvHeaderSize);

    // 订阅流
    session->AddSink(std::dynamic_pointer_cast<RtmpSink>(shared_from_this()));
    server->NotifyEvent("play.start(http-flv)", stream_path_);
    return true;
}

void HttpFlvConnection::onRequest(const HttpRequest &req) {
    const std::string &connection = req.getHeader("Connection");
    bool close = connection == "close" ||
                 (req.getVersion() == HttpRequest::kHttp10 &&
                  connection != "Keep-Alive");
    HttpResponse response(close);

    // 返回false表示没有在HandleRequest内部发送响应
    if (!HandleRequest(req, response)) {
        std::vector<char> resp_data = response.getOutputBuffer();
        if (!resp_data.empty()) {
            std::shared_ptr<char[]> buf(new char[resp_data.size()]);
            memcpy(buf.get(), resp_data.data(), resp_data.size());
            TcpConnection::Send(buf, (uint32_t)resp_data.size());
        }
    }
}

bool HttpFlvConnection::ParseStreamPath(const std::string &path,
                                        std::string &app,
                                        std::string &stream) {
    if (path.empty() || path[0] != '/') {
        return false;
    }
    size_t pos1 = path.find('/', 1);
    if (pos1 == std::string::npos) {
        return false;
    }
    app = path.substr(1, pos1 - 1);
    std::string stream_file = path.substr(pos1 + 1);
    size_t ext_pos = stream_file.find(".flv");
    if (ext_pos == std::string::npos) {
        stream = stream_file;
    } else {
        stream = stream_file.substr(0, ext_pos);
    }
    return !app.empty() && !stream.empty();
}

bool HttpFlvConnection::OnClose() {
    rtmp::RtmpServerPtr server = rtmp_server_.lock();
    if (!server) {
        fprintf(stderr, "RtmpServer is expired\n");
        return false;
    }
    if (!stream_path_.empty()) {
        rtmp::RtmpSessionPtr session = rtmp_session_.lock();
        if (!session) {
            fprintf(stderr, "RtmpSession is expired\n");
            return false;
        }
        // 在当前连接的事件循环中移除sink, 不影响其他人play拉流
        rtmp::RtmpSinkSharedPtr conn = std::dynamic_pointer_cast<RtmpSink>(
            shared_from_this());
        TcpConnection::GetTaskScheduler()->AddTimer([conn, session]() {
            session->RemoveSink(conn);
            return false;
        }, 0);
        if (is_playing_) {
            server->NotifyEvent("play.stop(http-flv)", stream_path_);
        }
    }
    is_playing_ = false;
    return true;
}

} // lsy::net::http
