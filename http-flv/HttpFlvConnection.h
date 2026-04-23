#ifndef HTTP_HTTPFLVCONNECTION_H
#define HTTP_HTTPFLVCONNECTION_H

#include "net/TcpConnection.h"
#include "rtmp/RtmpSink.h"


namespace lsy::net {

namespace rtmp {

class RtmpSession;

class RtmpServer;

}

namespace http {

class HttpContext;

class HttpRequest;

class HttpResponse;

class HttpFlvConnection : public TcpConnection, public rtmp::RtmpSink {
public:
    HttpFlvConnection(const std::shared_ptr<rtmp::RtmpServer> &server,
                      const TaskSchedulerPtr &scheduler, int sockfd);

    ~HttpFlvConnection() override;

    bool IsPlayer() override;

    bool IsPublisher() override;

    bool IsPlaying() override;

    bool IsPublishing() override;

    uint32_t GetId() override;

private:
    bool SendFlvScriptTag(std::unique_ptr<uint8_t[]> &&payload,
                          uint32_t payload_size);

    bool SendMetaData(const rtmp::AmfObjects &meta_data) override;

    bool SendMediaData(uint8_t type, uint64_t timestamp,
                       std::shared_ptr<uint8_t[]> payload,
                       uint32_t payload_size) override;

    bool OnRead(BufferReader &buffer);

    void onRequest(const HttpRequest &req);

    bool HandleRequest(const HttpRequest &req, HttpResponse &resp);

    bool OnClose();

    static bool IsAvcKeyFrame(std::unique_ptr<uint8_t[]> &payload,
                              uint32_t payload_size);

    static bool ParseStreamPath(const std::string &path, std::string &app,
                                std::string &stream);

    rtmp::AmfObjects meta_data_;
    rtmp::AmfEncoder amf_encoder_;
    std::weak_ptr<rtmp::RtmpServer> rtmp_server_;
    std::weak_ptr<rtmp::RtmpSession> rtmp_session_;
    std::shared_ptr<HttpContext> http_context_;
    std::shared_ptr<uint8_t[]> avc_sequence_header_;
    std::shared_ptr<uint8_t[]> aac_sequence_header_;
    uint32_t avc_sequence_header_size_ = 0;
    uint32_t aac_sequence_header_size_ = 0;
    uint32_t previous_tag_size_ = 0;
    std::string app_;
    std::string stream_name_;
    std::string stream_path_;
    bool is_playing_ = false;
    bool has_key_frame_ = false;
    static constexpr size_t kFlvTagHeaderSize = 11;
    const uint32_t kFlvHeaderSize = 9;
    static constexpr char kFlvHeader[] = {'F', 'L', 'V', 0x01, 0x05, 0x00, 0x00,
                                          0x00, 0x09}; // 9字节FLV文件头
};

} // http

} // lsy::net

#endif // HTTP_HTTPFLVCONNECTION_H
