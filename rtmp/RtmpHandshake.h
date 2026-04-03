#ifndef RTMP_RTMPHANDSHAKE_H
#define RTMP_RTMPHANDSHAKE_H

#include <string>
#include <cstring>
#include <random>

#include "net/BufferReader.h"
#include "base/BigEndianBuffer.h"

namespace lsy::rtmp {

class RtmpHandshake {
public:
    static constexpr uint8_t kRtmpVersion = 0x03;
    static constexpr size_t kRtmpC0S0Size = 1;
    static constexpr size_t kRtmpC1S1Size = 1536;
    static constexpr size_t kRtmpC2S2Size = 1536;
    static constexpr size_t kRtmpC1RandomSize = kRtmpC1S1Size - 4 - 4;
    static constexpr size_t kRtmpC0C1Total = kRtmpC0S0Size + kRtmpC1S1Size;
    static constexpr size_t kRtmpS0S1S2Total =
        kRtmpC0S0Size + kRtmpC1S1Size + kRtmpC2S2Size;

    enum State : uint8_t {
        WAIT_C0C1,
        WAIT_S0S1S2,
        WAIT_C2,
        HANDSHAKE_COMPLETE
    };

    explicit RtmpHandshake(State state) : handshake_state_(state) {
    }

    virtual ~RtmpHandshake() = default;

    size_t Parse(net::BufferReader &in_buffer, char *res_buf,
                 size_t res_buf_size) {
        if (!res_buf) {
            return std::string::npos;
        }
        size_t res_size = 0;
        if (handshake_state_ == WAIT_C0C1) {

            // ===================== 服务器: 解析 C0C1 并生成 S0S1S2 的数据 =====================

            if (in_buffer.ReadableBytes() < kRtmpC0C1Total) {
                return 0;
            }

            // 1. Peek出C0C1
            auto *c0c1 = reinterpret_cast<const uint8_t *>(in_buffer.Peek());
            const uint8_t *c1 = c0c1 + 1;

            // 2. 版本校验
            if (c0c1[0] != kRtmpVersion) {
                return std::string::npos;
            }

            // 3. 构建合并包 S0+S1+S2 (3073字节)
            if (res_buf_size < kRtmpS0S1S2Total) {
                return std::string::npos;
            }
            size_t s0s1_len = BuildS0S1(res_buf, res_buf_size);
            size_t s2_len = BuildS2(res_buf + s0s1_len, res_buf_size - s0s1_len,
                                    c1);
            res_size = s0s1_len + s2_len;

            // 4. 消耗已处理的数据
            in_buffer.Retrieve(kRtmpC0C1Total);
            handshake_state_ = WAIT_C2;
            return res_size;
        } else if (handshake_state_ == WAIT_S0S1S2) {

            // ===================== 客户端: 解析 S0S1S2 并生成 C2 数据 =====================

            if (in_buffer.ReadableBytes() < kRtmpS0S1S2Total) {
                return 0;
            }

            // 1. Peek出S0S1S2
            auto *s0s1s2 = reinterpret_cast<const uint8_t *>(in_buffer.Peek());
            const uint8_t *s1 = s0s1s2 + 1;

            // 2. 版本校验
            uint8_t s0_version = s0s1s2[0];
            if (s0_version != kRtmpVersion) {
                return std::string::npos;
            }

            // 3. 构建C2回显包
            res_size = BuildC2(res_buf, res_buf_size, s1);
            if (res_size == 0) {
                return std::string::npos;
            }

            // 4. 消耗合并包数据
            in_buffer.Retrieve(kRtmpS0S1S2Total);
            handshake_state_ = HANDSHAKE_COMPLETE;
            return res_size;
        } else if (handshake_state_ == WAIT_C2) {

            // ===================== 服务器：解析 C2 数据 =====================

            if (in_buffer.ReadableBytes() < kRtmpC2S2Size) {
                return 0;
            }

            in_buffer.Retrieve(kRtmpC2S2Size);
            handshake_state_ = HANDSHAKE_COMPLETE;
            return 0;
        }

        return res_size;
    }

    bool IsCompleted() const {
        return handshake_state_ == HANDSHAKE_COMPLETE;
    }

private:
    static size_t BuildC0C1(char *buf, size_t buf_size) {
        if (buf_size < kRtmpC0C1Total) {
            return 0;
        }
        std::memset(buf, 0, kRtmpC0C1Total);
        auto *buffer = reinterpret_cast<uint8_t *>(buf);
        size_t offset = 0;

        // 1. 写入 C0 (1B 版本号)
        BigEndianBuffer::WriteUInt8(buffer, kRtmpC0C1Total, offset,
                                    kRtmpVersion);

        // 2. 写入 C1

        // 2.1. 4B: 时间戳
        uint32_t timestamp = static_cast<uint32_t>(std::random_device{}());
        BigEndianBuffer::WriteUInt32BE(buffer, kRtmpC0C1Total, offset,
                                       timestamp);
        // 2.2. 4B: 0
        offset += 4;
        // 2.3. 1528B: 随机数
        BigEndianBuffer::WriteRandomBytes(buffer, kRtmpC0C1Total, offset,
                                          kRtmpC1RandomSize);
        return kRtmpC0C1Total;
    }

    static size_t BuildS0S1(char *buf, size_t buf_size) {
        return BuildC0C1(buf, buf_size);
    }

    static size_t BuildS2(char *buf, size_t buf_size, const uint8_t *c1_data) {
        if (buf_size < kRtmpC2S2Size || !c1_data) return 0;
        std::memset(buf, 0, kRtmpC2S2Size);
        auto *buffer = reinterpret_cast<uint8_t *>(buf);
        size_t offset = 0;

        // 1. 4B: 回显客户端C1的时间戳(大端)
        BigEndianBuffer::WriteBytes(buffer, kRtmpC2S2Size, offset, c1_data, 4);
        // 2. 4B: 服务器自身时间戳(大端)
        uint32_t server_ts = static_cast<uint32_t>(std::random_device{}());
        BigEndianBuffer::WriteUInt32BE(buffer, kRtmpC2S2Size, offset,
                                       server_ts);
        // 3. 1528B: 回显客户端C1的随机数
        BigEndianBuffer::WriteBytes(buffer, kRtmpC2S2Size, offset, c1_data + 8,
                                    1528);
        return kRtmpC2S2Size;
    }

    static size_t BuildC2(char *buf, size_t buf_size, const uint8_t *s1_data) {
        if (buf_size < kRtmpC2S2Size || !s1_data) return 0;
        std::memset(buf, 0, kRtmpC2S2Size);
        auto *buffer = reinterpret_cast<uint8_t *>(buf);
        size_t offset = 0;

        // 1. 4字节: 回显服务器S1的时间戳(大端)
        BigEndianBuffer::WriteBytes(buffer, kRtmpC2S2Size, offset, s1_data, 4);
        // 2. 4字节: 客户端自身时间戳(大端)
        uint32_t client_ts = static_cast<uint32_t>(std::random_device{}());
        BigEndianBuffer::WriteUInt32BE(buffer, kRtmpC2S2Size, offset,
                                       client_ts);
        // 3. 1528字节: 回显服务器S1的随机数
        BigEndianBuffer::WriteBytes(buffer, kRtmpC2S2Size, offset, s1_data + 8,
                                    1528);
        return kRtmpC2S2Size;
    }

    State handshake_state_;
};

} // lsy::rtmp

#endif // RTMP_RTMPHANDSHAKE_H
