#ifndef HTTP_HTTPCONNECTION_H
#define HTTP_HTTPCONNECTION_H

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

class HttpConnection : public TcpConnection, public rtmp::RtmpSink {
    HttpConnection(const std::shared_ptr<rtmp::RtmpServer> &server,
                   const TaskSchedulerPtr &scheduler, int sockfd);

    ~HttpConnection() override;

    bool IsPlayer() override;

    bool IsPublisher() override;

    bool IsPlaying() override;

    bool IsPublishing() override;

    uint32_t GetId() override;

private:
    bool SendMetaData(const rtmp::AmfObjects &meta_data) override;

    bool SendMediaData(uint8_t type, uint64_t timestamp,
                       std::shared_ptr<uint8_t[]> payload,
                       uint32_t payload_size) override;

    bool OnRead(BufferReader &buffer);

    void onRequest(const HttpRequest &req);

    void OnClose();

    rtmp::AmfObjects meta_data_;
    rtmp::AmfEncoder amf_encoder_;
    std::weak_ptr<rtmp::RtmpServer> rtmp_server_;
    std::weak_ptr<rtmp::RtmpSession> rtmp_session_;
    std::shared_ptr<HttpContext> http_context_;
    std::shared_ptr<uint8_t[]> avc_sequence_header_;
    std::shared_ptr<uint8_t[]> aac_sequence_header_;
    uint32_t avc_sequence_header_size_ = 0;
    uint32_t aac_sequence_header_size_ = 0;
    bool is_playing_ = false;
};

} // http

} // lsy::net

#endif // HTTP_HTTPCONNECTION_H
