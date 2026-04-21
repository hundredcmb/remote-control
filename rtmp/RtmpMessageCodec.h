#ifndef RTMP_RTMPMESSAGECODEC_H
#define RTMP_RTMPMESSAGECODEC_H

#include "base/ByteIO.h"
#include "base/noncopyable.h"
#include "net/BufferReader.h"
#include "RtmpMessage.h"

#include <map>

namespace lsy::net::rtmp {

class RtmpMessageCodec : noncopyable {
public:
    enum State : uint8_t {
        PARSE_HEADER,
        PARSE_BODY,
    };

    explicit RtmpMessageCodec(int in_chunk_size = 128, int out_chunk_size = 128)
        : state_(State::PARSE_HEADER),
          current_csid_(0),
          in_chunk_size_(in_chunk_size),
          out_chunk_size_(out_chunk_size) {
        stream_id_ = next_stream_id_++;
    }

    ~RtmpMessageCodec() = default;

    [[nodiscard]] uint32_t StreamId() const {
        return stream_id_;
    }

    int Parse(BufferReader &in_buffer, RtmpMessage &out_rtmp_msg) {
        if (in_buffer.ReadableBytes() == 0) {
            return 0;
        }

        int ret = 0;
        size_t offset = 0;
        if (state_ == State::PARSE_HEADER) {
            ret = ParseChunkHeader(in_buffer);
            if (ret <= 0) {
                return ret;
            }
            offset += ret;
        } else {
            ret = ParseChunkBody(in_buffer);
            if (ret < 0) {
                return ret;
            }
            offset += ret;

            // 如果 Message 解析完毕, 就返回 Message 数据
            auto it = rtmp_messages_.find(current_csid_);
            if (it == rtmp_messages_.end()) {
                return -1;
            }
            RtmpMessage &rtmp_msg = it->second;
            if (ret == 0 && rtmp_msg.payload_len_ != 0) {
                return 0;
            }
            if (rtmp_msg.payload_offset_ == rtmp_msg.payload_len_) {
                rtmp_msg.is_completed_ = true;
                out_rtmp_msg = std::move(rtmp_msg);
                //fprintf(stderr, "End of message: csid=%d, len=%d, type=%d\n", out_rtmp_msg.csid_, out_rtmp_msg.payload_len_, out_rtmp_msg.type_id_);

                // fmt3 上下文全部继承; fmt1 时间戳和流ID继承
                rtmp_msg = RtmpMessage();
                rtmp_msg.payload_len_ = out_rtmp_msg.payload_len_;
                rtmp_msg.type_id_ = out_rtmp_msg.type_id_;
                rtmp_msg.timestamp = out_rtmp_msg.timestamp;
                rtmp_msg.extend_timestamp = out_rtmp_msg.extend_timestamp;
                rtmp_msg.real_timestamp = out_rtmp_msg.real_timestamp;
                rtmp_msg.stream_id_ = out_rtmp_msg.stream_id_;

                current_csid_ = 0;
                state_ = State::PARSE_HEADER;
            }
        }

        return static_cast<int>(offset);
    }

    int CreateChunks(ChunkStreamID csid, const RtmpMessage &rtmp_msg,
                     uint8_t *buf, size_t buf_size) const {
        uint8_t *payload = rtmp_msg.payload_.get();
        if (!payload) {
            return -1;
        }
        const bool need_extend_ts = rtmp_msg.real_timestamp > 0xFFFFFF;

        // 写入首个 Chunk 的 BasicHeader
        size_t buf_offset = 0, payload_offset = 0, payload_remaining = rtmp_msg.payload_len_;
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
                                       rtmp_msg.real_timestamp)) {
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
                    uint32_t extend_ts = rtmp_msg.real_timestamp;
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

    [[nodiscard]] uint32_t OutChunkSize() const {
        return out_chunk_size_;
    }

    [[nodiscard]] uint32_t InChunkSize() const {
        return in_chunk_size_;
    }

    void Clear() {
        rtmp_messages_.clear();
    }

private:
    // 解析一个 Chunk 头, 返回解析的字节数, 返回-1表示解析失败, 返回0表示数据不足
    int ParseChunkHeader(BufferReader &buffer) {
        if (state_ == PARSE_BODY) {
            return -1;
        }
        auto *buf = reinterpret_cast<const uint8_t *>(buffer.Peek());
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
        if (fmt == 0) {
            rtmp_msg.Clear();
        }
        rtmp_msg.csid_ = csid;

        // 解析 MessageHeader
        uint32_t ts_value = 0, extend_ts = 0;
        bool need_extend_ts = false;
        if (fmt <= 2) {
            ByteIO::ReadUInt24BE(buf, buf_size, offset, ts_value);
            need_extend_ts = (ts_value == 0xFFFFFF);
            rtmp_msg.has_extend_ts_ = need_extend_ts;
        } else {
            need_extend_ts = rtmp_msg.has_extend_ts_;
        }
        if (fmt <= 1) {
            ByteIO::ReadUInt24BE(buf, buf_size, offset, rtmp_msg.payload_len_);
            ByteIO::ReadUInt8(buf, buf_size, offset, rtmp_msg.type_id_);
        }
        if (fmt == 0) {
            ByteIO::ReadUInt32LE(buf, buf_size, offset, rtmp_msg.stream_id_);
        }

        // 解析扩展时间戳
        if (need_extend_ts) {
            if (!ByteIO::ReadUInt32BE(buf, buf_size, offset, extend_ts)) {
                return 0;
            }
        }
        //fprintf(stderr, "fmt=%d, csid=%d, len=%d, type=%d\n", fmt, csid, rtmp_msg.payload_len_, rtmp_msg.type_id_);

        // 更新时间戳
        if (fmt == 0) {
            rtmp_msg.payload_offset_ = 0;
            if (need_extend_ts) {
                rtmp_msg.extend_timestamp = extend_ts;
                rtmp_msg.real_timestamp = extend_ts;
            } else {
                rtmp_msg.timestamp = ts_value;
                rtmp_msg.real_timestamp = ts_value;
            }
        } else if (fmt == 1 || fmt == 2) {
            if (need_extend_ts) {
                rtmp_msg.extend_timestamp += extend_ts;
                rtmp_msg.real_timestamp += extend_ts;
            } else {
                rtmp_msg.timestamp += ts_value;
                rtmp_msg.real_timestamp += ts_value;
            }
        } else if (fmt == 3) {
            if (rtmp_msg.payload_len_ == 0) {
                return -1;
            }
        }

        state_ = State::PARSE_BODY;
        current_csid_ = rtmp_msg.csid_ = csid;

        buffer.Retrieve(offset);
        return static_cast<int>(offset);
    }

    // 解析一个 Chunk 载荷, 返回解析的字节数, 返回-1表示解析失败, 返回0表示数据不足
    int ParseChunkBody(BufferReader &buffer) {
        if (state_ == State::PARSE_HEADER) {
            return -1;
        }
        auto *buf = reinterpret_cast<const uint8_t *>(buffer.Peek());
        if (!buf) {
            return 0;
        }
        if (current_csid_ == 0) {
            return -1;
        }

        auto it = rtmp_messages_.find(current_csid_);
        if (it == rtmp_messages_.end()) {
            return -1;
        }
        RtmpMessage &rtmp_msg = it->second;

        // 读取当前 Chunk 的 payload_ 数据
        size_t offset = 0, buf_size = buffer.ReadableBytes();
        uint32_t chunk_size = rtmp_msg.payload_len_ - rtmp_msg.payload_offset_;
        if (chunk_size > in_chunk_size_) {
            chunk_size = in_chunk_size_;
        }
        if (chunk_size > 0) {
            if (!rtmp_msg.payload_) {
                rtmp_msg.payload_ = std::make_unique<uint8_t[]>(
                    rtmp_msg.payload_len_);
            }
            uint8_t *payload =
                rtmp_msg.payload_.get() + rtmp_msg.payload_offset_;
            if (!ByteIO::ReadBytes(buf, buf_size, offset, payload,
                                   chunk_size)) {
                return 0;
            }
            rtmp_msg.payload_offset_ += chunk_size;
        }
        state_ = State::PARSE_HEADER;
        if (offset > 0) {
            buffer.Retrieve(offset);
        }
        return static_cast<int>(offset);
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
        const bool need_extend_ts = (rtmp_msg.real_timestamp >= 0xFFFFFF);
        if (fmt <= 2) {
            // 写入 3B 时间戳(大端)
            uint32_t ts_field = need_extend_ts ? 0xFFFFFF
                                               : rtmp_msg.real_timestamp;
            if (!ByteIO::WriteUInt24BE(buf, buf_size, offset, ts_field)) {
                return -1;
            }
        }
        if (fmt <= 1) {
            // 写入 3B message长度(大端)
            if (!ByteIO::WriteUInt24BE(buf, buf_size, offset,
                                       rtmp_msg.payload_len_)) {
                return -1;
            }
            // 写入 1B message类型ID
            if (!ByteIO::WriteUInt8(buf, buf_size, offset,
                                    rtmp_msg.type_id_)) {
                return -1;
            }
        }
        if (fmt == 0) {
            // 写入 4B message流ID(小端)
            if (!ByteIO::WriteUInt32LE(buf, buf_size, offset,
                                       rtmp_msg.stream_id_)) {
                return -1;
            }
        }
        return static_cast<int>(offset);
    }

    State state_;
    static uint32_t next_stream_id_;
    uint32_t stream_id_;
    ChunkStreamID current_csid_;
    uint32_t in_chunk_size_;
    uint32_t out_chunk_size_;
    std::map<ChunkStreamID, RtmpMessage> rtmp_messages_;
    static constexpr size_t kChunkMessageHeaderLen[4] = {11, 7, 3, 0};
};

} // lsy::net::rtmp

#endif // RTMP_RTMPMESSAGECODEC_H
