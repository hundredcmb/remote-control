#ifndef HTTP_HTTPCONTEXT_H
#define HTTP_HTTPCONTEXT_H

#include <algorithm>

#include "HttpRequest.h"
#include "net/BufferReader.h"

namespace lsy::net::http {

class HttpContext {
public:
    enum HttpRequestParseState {
        kExpectRequestLine,
        kExpectHeaders,
        kExpectBody,
        kGotAll,
    };

    HttpContext() : state_(kExpectRequestLine) {
    }

    bool parseRequest(BufferReader &buf, Timestamp receiveTime) {
        bool ok = true;
        bool hasMore = true;
        while (hasMore) {
            if (state_ == kExpectRequestLine) {
                const char *crlf = buf.FindCRLF();
                if (crlf) {
                    ok = processRequestLine(buf.Peek(), crlf);
                    if (ok) {
                        request_.setReceiveTime(receiveTime);
                        buf.RetrieveUntil(crlf + 2);
                        state_ = kExpectHeaders;
                    } else {
                        hasMore = false;
                    }
                } else {
                    hasMore = false;
                }
            } else if (state_ == kExpectHeaders) {
                const char *crlf = buf.FindCRLF();
                if (crlf) {
                    const char *colon = std::find(buf.Peek(), crlf, ':');
                    if (colon != crlf) {
                        request_.addHeader(buf.Peek(), colon, crlf);
                    } else {
                        state_ = kGotAll;
                        hasMore = false;
                    }
                    buf.RetrieveUntil(crlf + 2);
                } else {
                    hasMore = false;
                }
            } else if (state_ == kExpectBody) {
            }
        }
        return ok;
    }

    [[nodiscard]] bool gotAll() const {
        return state_ == kGotAll;
    }

    void reset() {
        state_ = kExpectRequestLine;
        HttpRequest dummy;
        request_.swap(dummy);
    }

    [[nodiscard]] const HttpRequest &request() const {
        return request_;
    }

    HttpRequest &request() {
        return request_;
    }

private:
    bool processRequestLine(const char *begin, const char *end) {
        bool succeed = false;
        const char *start = begin;
        const char *space = std::find(start, end, ' ');
        if (space != end && request_.setMethod(start, space)) {
            start = space + 1;
            space = std::find(start, end, ' ');
            if (space != end) {
                const char *question = std::find(start, space, '?');
                if (question != space) {
                    request_.setPath(start, question);
                    request_.setQuery(question, space);
                } else {
                    request_.setPath(start, space);
                }
                start = space + 1;
                succeed =
                    end - start == 8 && std::equal(start, end - 1, "HTTP/1.");
                if (succeed) {
                    if (*(end - 1) == '1') {
                        request_.setVersion(HttpRequest::kHttp11);
                    } else if (*(end - 1) == '0') {
                        request_.setVersion(HttpRequest::kHttp10);
                    } else {
                        succeed = false;
                    }
                }
            }
        }
        return succeed;
    }

    HttpRequestParseState state_;
    HttpRequest request_;
};

using HttpContextPtr = std::shared_ptr<HttpContext>;

} // lsy::net::http

#endif // HTTP_HTTPCONTEXT_H
