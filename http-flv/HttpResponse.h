#ifndef HTTP_HTTPRESPONSE_H
#define HTTP_HTTPRESPONSE_H

#include <map>
#include <string>

#include "net/BufferWriter.h"

namespace lsy::net::http {

class HttpResponse {
public:
    enum HttpStatusCode {
        kUnknown,
        k200Ok = 200,
        k301MovedPermanently = 301,
        k400BadRequest = 400,
        k404NotFound = 404,
    };

    explicit HttpResponse(bool close)
        : statusCode_(kUnknown),
          closeConnection_(close) {
    }

    void setStatusCode(HttpStatusCode code) {
        statusCode_ = code;
    }

    void setStatusMessage(const std::string &message) {
        statusMessage_ = message;
    }

    void setCloseConnection(bool on) {
        closeConnection_ = on;
    }

    [[nodiscard]] bool closeConnection() const {
        return closeConnection_;
    }

    void setContentType(const std::string &contentType) {
        addHeader("Content-Type", contentType);
    }

    void addHeader(const std::string &key, const std::string &value) {
        headers_[key] = value;
    }

    void setBody(const std::string &body) {
        body_ = body;
    }

    [[nodiscard]] std::vector<char> getOutputBuffer() const {
        std::vector<char> temp_buffer;
        char buf[32];

        int line_len = snprintf(buf, sizeof(buf), "HTTP/1.1 %d ", statusCode_);
        appendBufToVector(temp_buffer, buf, line_len);
        appendBufToVector(temp_buffer, statusMessage_.c_str(),
                             statusMessage_.size());
        appendBufToVector(temp_buffer, "\r\n", 2);

        if (closeConnection_) {
            appendBufToVector(temp_buffer, "Connection: close\r\n", 19);
        } else {
            int content_len = snprintf(buf, sizeof(buf),
                                       "Content-Length: %zd\r\n", body_.size());
            appendBufToVector(temp_buffer, buf, content_len);
            appendBufToVector(temp_buffer, "Connection: Keep-Alive\r\n", 24);
        }

        for (const auto &header: headers_) {
            appendBufToVector(temp_buffer, header.first.c_str(),
                                 header.first.size());
            appendBufToVector(temp_buffer, ": ", 2);
            appendBufToVector(temp_buffer, header.second.c_str(),
                                 header.second.size());
            appendBufToVector(temp_buffer, "\r\n", 2);
        }

        appendBufToVector(temp_buffer, "\r\n", 2);
        appendBufToVector(temp_buffer, body_.c_str(), body_.size());

        return temp_buffer;
    }

private:
    static void appendBufToVector(std::vector<char> &vec, const char *buf,
                                  size_t len) {
        if (buf == nullptr || len == 0) {
            return;
        }
        vec.insert(vec.end(), buf, buf + len);
    }

    std::map<std::string, std::string> headers_;
    HttpStatusCode statusCode_;
    std::string statusMessage_;
    bool closeConnection_;
    std::string body_;
};

} // lsy::net::http

#endif // HTTP_HTTPRESPONSE_H
