#ifndef HTTP_HTTPFLVSERVER_H
#define HTTP_HTTPFLVSERVER_H

#include "net/TcpServer.h"
#include "rtmp/RtmpServer.h"
#include "HttpFlvConnection.h"

namespace lsy::net::http {

class HttpFlvServer;

using HttpFlvServerPtr = std::shared_ptr<HttpFlvServer>;

class HttpFlvServer : public TcpServer,
                      public std::enable_shared_from_this<HttpFlvServer> {
public:
    ~HttpFlvServer() override = default;

    static HttpFlvServerPtr Create(rtmp::RtmpServerPtr &rtmp_server,
                                   const EventLoopThreadPoolPtr &event_loops) {
        return std::shared_ptr<HttpFlvServer>(
            new HttpFlvServer(rtmp_server, event_loops));
    }

private:
    HttpFlvServer(rtmp::RtmpServerPtr &rtmp_server,
                  const EventLoopThreadPoolPtr &event_loops)
        : TcpServer(event_loops), rtmp_server_(rtmp_server) {
    }

    TcpConnectionPtr CreateConnection(int sockfd) override {
        TcpConnectionPtr conn = std::make_shared<HttpFlvConnection>(
            rtmp_server_,
            TcpServer::event_loops_->GetNextIoTaskScheduler(),
            sockfd);
        conn->EnableCallbacks();
        return conn;
    }

    void OnConnect(const TcpConnectionPtr &conn) override {
        if (conn) {
            fprintf(stderr, "[HttpFlvServer] tcp OnConnect: fd=%d\n",
                    conn->GetSocket());
        }
    }

    void OnDisconnect(const TcpConnectionPtr &conn) override {
        if (conn) {
            fprintf(stderr, "[HttpFlvServer] tcp OnDisconnect: fd=%d\n",
                    conn->GetSocket());
        }
    }

    rtmp::RtmpServerPtr rtmp_server_;
};

} // lsy::net::http

#endif // HTTP_HTTPFLVSERVER_H
