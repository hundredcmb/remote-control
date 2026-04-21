#include "HttpConnection.h"

#include "HttpContext.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "rtmp/RtmpServer.h"
#include "rtmp/RtmpSession.h"

namespace lsy::net::http {

HttpConnection::HttpConnection(const std::shared_ptr<rtmp::RtmpServer> &server,
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

HttpConnection::~HttpConnection() = default;

bool HttpConnection::IsPlayer() {
    return true;
}

bool HttpConnection::IsPublisher() {
    return false;
}

bool HttpConnection::IsPlaying() {
    return is_playing_;
}

bool HttpConnection::IsPublishing() {
    return false;
}

uint32_t HttpConnection::GetId() {
    return static_cast<uint32_t>(TcpConnection::GetSocket());
}

bool HttpConnection::SendMetaData(const rtmp::AmfObjects &meta_data) {
    return false;
}

bool HttpConnection::SendMediaData(uint8_t type, uint64_t timestamp,
                                   std::shared_ptr<uint8_t[]> payload,
                                   uint32_t payload_size) {
    return false;
}

bool HttpConnection::OnRead(BufferReader &buffer) {
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

static void HandleRequest(const HttpRequest &req, HttpResponse &resp) {

}

void HttpConnection::onRequest(const HttpRequest &req) {
    const std::string &connection = req.getHeader("Connection");
    bool close = connection == "close" ||
                 (req.getVersion() == HttpRequest::kHttp10 &&
                  connection != "Keep-Alive");
    HttpResponse response(close);
    HandleRequest(req, response);
    std::vector<char> resp = response.getOutputBuffer();
    std::shared_ptr<char[]> resp_buf(new char[resp.size()]);
    std::memcpy(resp_buf.get(), resp.data(), resp.size());
    Send(resp_buf, static_cast<uint32_t>(resp.size()));
}

void HttpConnection::OnClose() {

}

} // lsy::net::http
