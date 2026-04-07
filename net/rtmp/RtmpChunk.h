#ifndef NET_RTMPCHUNK_H
#define NET_RTMPCHUNK_H

#include "net/BufferReader.h"
#include "net/rtmp/RtmpMessage.h"
#include "base/ByteIO.h"

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

    int CreateChunk(ChunkStreamID csid, const RtmpMessage &rtmp_msg,
                    uint8_t *buf, size_t buf_size) const {
        uint8_t *payload = rtmp_msg.payload.get();
        if (!payload) {
            return -1;
        }
        const bool need_extend_ts = rtmp_msg._timestamp > 0xFFFFFF;

        // 写入首个 Chunk 的 BasicHeader
        size_t buf_offset = 0, payload_offset = 0, payload_remaining = rtmp_msg.length;
        int basic_header_len = CreateBasicHeader(0, csid, buf, buf_size);
        if (basic_header_len <= 0) {
            return -1;
        }
        buf_offset += basic_header_len;

        // 写入首个 Chunk 的 MessageHeader
        int message_header_len = CreateMessageHeader(0, rtmp_msg,
                                                     buf + buf_offset,
                                                     buf_size - buf_offset);
        if (message_header_len <= 0) {
            return -1;
        }
        buf_offset += message_header_len;

        // 写入首个 Chunk 的扩展时间戳(大端)
        if (need_extend_ts) {
            if (!ByteIO::WriteUInt32BE(buf, buf_size, buf_offset,
                                       rtmp_msg._timestamp)) {
                return -1;
            }
        }

        while (payload_remaining > 0) {
            if (payload_remaining > out_chunk_size_) {
                if (!ByteIO::WriteBytes(buf, buf_size, buf_offset,
                                        payload + payload_offset,
                                        out_chunk_size_)) {
                    return -1;
                }
                payload_offset += out_chunk_size_;
                payload_remaining -= out_chunk_size_;

                // 写入后续 Chunk 的 BasicHeader, fmt=3 时无 MessageHeader
                basic_header_len = CreateBasicHeader(3, csid, buf + buf_offset,
                                                     buf_size - buf_offset);
                if (basic_header_len <= 0) {
                    return -1;
                }
                buf_offset += basic_header_len;

                // 写入后续 Chunk 的扩展时间戳
                if (need_extend_ts) {
                    uint32_t extend_ts = rtmp_msg._timestamp;
                    if (!ByteIO::WriteUInt32BE(buf, buf_size, buf_offset,
                                               extend_ts)) {
                        return -1;
                    }
                }
            } else {
                if (!ByteIO::WriteBytes(buf, buf_size, buf_offset,
                                        payload + payload_offset,
                                        payload_remaining)) {
                    return -1;
                }
                payload_offset += payload_remaining;
                payload_remaining = 0;
            }
        }
        return static_cast<int>(buf_offset);
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
    // 解析 Chunk 数据, 返回解析的字节数, 返回-1表示解析失败, 返回0表示数据不足
    int ParseChunkHeader(BufferReader &buffer) {
        auto *buf = reinterpret_cast<uint8_t *>(buffer.Peek());
        if (!buf) {
            return 0;
        }
        size_t offset = 0, buf_size = buffer.ReadableBytes();

        // 解析 BasicHeader
        uint8_t flags = 0;
        if (!ByteIO::ReadUInt8(buf, buf_size, offset, flags)) {
            return 0;
        }
        uint8_t fmt = flags >> 6;
        if (fmt > 3) {
            return -1;
        }
        ChunkStreamID csid = flags & 0x3F; // 1B, [2, 63]
        if (csid == 0) {
            // 2B, [64, 64+255]
            uint8_t v = 0;
            if (!ByteIO::ReadUInt8(buf, buf_size, offset, v)) return 0;
            csid = 64 + v;
        } else if (csid == 1) {
            // 3B, [64+256, 64+65535]
            uint8_t v1 = 0, v2 = 0;
            if (!ByteIO::ReadUInt8(buf, buf_size, offset, v1)) return 0;
            if (!ByteIO::ReadUInt8(buf, buf_size, offset, v2)) return 0;
            csid = 64 + (static_cast<uint32_t>(v2) << 8 | v1);
        }

        size_t header_len = kChunkMessageHeaderLen[fmt];
        if (buf_size < header_len + offset) {
            return 0;
        }

        auto it = rtmp_messages_.find(csid);
        if (it == rtmp_messages_.end()) {
            it = rtmp_messages_.emplace(csid, RtmpMessage()).first;
        }
        RtmpMessage &rtmp_msg = it->second;

        // 解析 MessageHeader
        uint32_t ts_value = 0, extend_ts = 0;
        bool current_need_extend_ts = false;
        if (fmt <= 2) {
            if (!ByteIO::ReadUInt24BE(buf, buf_size, offset, ts_value))
                return 0;
            current_need_extend_ts = (ts_value == 0xFFFFFF);
            rtmp_msg.has_extend_ts = current_need_extend_ts;
        } else {
            current_need_extend_ts = rtmp_msg.has_extend_ts;
        }
        if (fmt <= 1) {
            if (!ByteIO::ReadUInt24BE(buf, buf_size, offset,
                                      rtmp_msg.length))
                return 0;
            if (!ByteIO::ReadUInt8(buf, buf_size, offset,
                                   rtmp_msg.type_id))
                return 0;
        }
        if (fmt == 0) {
            if (!ByteIO::ReadUInt32LE(buf, buf_size, offset,
                                      rtmp_msg.stream_id))
                return 0;
            rtmp_msg.index = 0;
        }

        // 解析扩展时间戳
        if (current_need_extend_ts) {
            if (!ByteIO::ReadUInt32BE(buf, buf_size, offset, extend_ts)) {
                return 0;
            }
        }

        // 更新时间戳
        if (fmt == 0) {
            rtmp_msg.index = 0;
            rtmp_msg._timestamp = current_need_extend_ts ? extend_ts : ts_value;
            rtmp_msg.timestamp = ts_value;
            rtmp_msg.extend_timestamp = extend_ts;
            rtmp_msg.use_timestamp_delta = false;
            if (current_need_extend_ts) {
                rtmp_msg.extend_timestamp = extend_ts;
            } else {
                rtmp_msg.timestamp = ts_value;
            }
        } else {
            if (fmt == 1 || fmt == 2) {
                rtmp_msg.use_timestamp_delta = true;
                rtmp_msg.timestamp_delta = ts_value;
                rtmp_msg.extend_timestamp_delta = extend_ts;
                if (current_need_extend_ts) {
                    rtmp_msg.extend_timestamp += extend_ts;
                } else {
                    rtmp_msg.timestamp += ts_value;
                }
            }
            ++rtmp_msg.index;
        }

        return static_cast<int>(offset);
    }

    int ParseChunkBody(BufferReader &buffer) {

        return 0;
    }

    static int CreateBasicHeader(uint8_t fmt, ChunkStreamID csid, uint8_t *buf,
                                 size_t buf_size) {
        if (buf == nullptr || fmt > 3 || buf_size == 0) {
            return -1;
        }
        if (csid < 2 || csid > 65599) {
            return -1;
        }
        int len = 0;
        uint8_t first_byte = fmt << 6;
        if (csid > 319) {
            // 3B, [64+256, 64+65535]
            if (buf_size < 3) {
                return -1;
            }
            buf[len++] = first_byte | 1;        // 低6bit = 1，标识3字节头
            uint16_t csid_val = csid - 64;
            buf[len++] = csid_val & 0xFF;       // 小端: 低8位在前
            buf[len++] = (csid_val >> 8) & 0xFF;// 小端: 高8位在后
        } else if (csid >= 64) {
            // 2B, [64, 64+255]
            if (buf_size < 2) {
                return -1;
            }
            buf[len++] = first_byte | 0;        // 低6bit = 0，标识2字节头
            buf[len++] = (csid - 64) & 0xFF;
        } else {
            // 1B, [2,63]
            buf[len++] = first_byte | csid;     // 低6bit 直接存CSID
        }
        return len;
    }

    static int CreateMessageHeader(uint8_t fmt, const RtmpMessage &rtmp_msg,
                                   uint8_t *buf, size_t buf_size) {
        if (buf == nullptr || fmt > 3) {
            return -1;
        }
        size_t offset = 0;
        const bool need_extend_ts = (rtmp_msg._timestamp >= 0xFFFFFF);
        if (fmt <= 2) {
            // 写入 3B 时间戳(大端)
            uint32_t ts_field = need_extend_ts ? 0xFFFFFF : rtmp_msg._timestamp;
            if (!ByteIO::WriteUInt24BE(buf, buf_size, offset, ts_field)) {
                return -1;
            }
        }
        if (fmt <= 1) {
            // 写入 3B message长度(大端)
            if (!ByteIO::WriteUInt24BE(buf, buf_size, offset,
                                       rtmp_msg.length)) {
                return -1;
            }
            // 写入 1B message类型ID
            if (!ByteIO::WriteUInt8(buf, buf_size, offset,
                                    rtmp_msg.type_id)) {
                return -1;
            }
        }
        if (fmt == 0) {
            // 写入 4B message流ID(小端)
            if (!ByteIO::WriteUInt32LE(buf, buf_size, offset,
                                       rtmp_msg.stream_id)) {
                return -1;
            }
        }
        return static_cast<int>(offset);
    }

    State state_;
    int chunk_stream_id_;
    int stream_id_;
    uint32_t in_chunk_size_;
    uint32_t out_chunk_size_;
    std::map<ChunkStreamID, RtmpMessage> rtmp_messages_;
    static constexpr size_t kChunkMessageHeaderLen[4] = {11, 7, 3, 0};
};

} // lsy::net::rtmp

#endif // NET_RTMPCHUNK_H
