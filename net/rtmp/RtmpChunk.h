#ifndef NET_RTMPCHUNK_H
#define NET_RTMPCHUNK_H

#include "net/BufferReader.h"
#include "net/rtmp/RtmpMessage.h"

#include <map>

namespace lsy::net::rtmp {

class RtmpChunk {
public:
    enum State {
        PARSE_HEADER,
        PARSE_BODY,
    };

    explicit RtmpChunk(int stream_id, int in_chunk_size = 128,
                       int out_chunk_size = 128)
        : state_(PARSE_HEADER),
          chunk_stream_id_(0),
          stream_id_(stream_id),
          in_chunk_size_(in_chunk_size),
          out_chunk_size_(out_chunk_size) {
    }

    ~RtmpChunk() = default;

    int Parse(BufferReader &in_buffer, RtmpMessage &out_rtmp_msg) {
        return 0;
    }

    int CreateChunk(uint32_t csid, RtmpMessage &rtmp_msg, char *buf,
                    uint32_t buf_size) {
        return 0;
    }

    void SetInChunkSize(uint32_t in_chunk_size) {
        in_chunk_size_ = in_chunk_size;
    }

    void SetOutChunkSize(uint32_t out_chunk_size) {
        out_chunk_size_ = out_chunk_size;
    }

    void Clear() {
        rtmp_messages_.clear();
    }

    [[nodiscard]] int GetStreamId() const {
        return stream_id_;
    }

private:
    int ParseChunkHeader(BufferReader &buffer) {
        return 0;
    }

    int ParseChunkBody(BufferReader &buffer) {
        return 0;
    }

    static int CreateBasicHeader(uint8_t fmt, uint32_t csid, uint8_t *buf) {
        if (buf == nullptr) {
            return -1;
        }
        if (fmt > 3) {
            return -1;
        }
        if (csid < 2 || csid > 65599) {
            return -1;
        }
        int len = 0;
        uint8_t first_byte = fmt << 6;
        if (csid > 319) {
            // 3B, (64+255, 65599]
            buf[len++] = first_byte | 1;        // 低6bit = 1，标识3字节头
            uint16_t csid_val = csid - 64;
            buf[len++] = csid_val & 0xFF;       // 小端: 低8位在前
            buf[len++] = (csid_val >> 8) & 0xFF;// 小端: 高8位在后
        } else if (csid >= 64) {
            // 2B, [64, 64+255]
            buf[len++] = first_byte | 0;        // 低6bit = 0，标识2字节头
            buf[len++] = (csid - 64) & 0xFF;
        } else {
            // 1B, [2,63]
            buf[len++] = first_byte | csid;     // 低6bit 直接存CSID
        }
        return len;
    }

    static int CreateMessageHeader(uint8_t fmt, RtmpMessage &rtmp_msg,
                                   char *buf) {
        return 0;
    }

    State state_;
    int chunk_stream_id_;
    int stream_id_;
    uint32_t in_chunk_size_;
    uint32_t out_chunk_size_;
    std::map<int, RtmpMessage> rtmp_messages_;
    static constexpr int kChunkMessageHeaderLen[4] = {11, 7, 3, 0};
};

} // lsy::net::rtmp

#endif // NET_RTMPCHUNK_H
