#ifndef RTMP_RTMPHANDSHAKE_H
#define RTMP_RTMPHANDSHAKE_H

#include <ctime>
#include <string>
#include <memory>
#include <cstring>

#include "base/ByteIO.h"
#include "net/BufferReader.h"

namespace lsy::net::rtmp {

class RtmpHandshake;

using RtmpHandshakePtr = std::shared_ptr<RtmpHandshake>;

class RtmpHandshake {
public:
    static constexpr uint8_t kRtmpVersion = 0x03;
    static constexpr size_t kC0S0Size = 1;
    static constexpr size_t kC1C2S1S2Size = 1536;
    static constexpr size_t kC0C1Total = kC0S0Size + kC1C2S1S2Size;
    static constexpr size_t kS0S1S2Total = kC0S0Size + kC1C2S1S2Size * 2;

    enum State : uint8_t {
        WAIT_C0C1,
        WAIT_S0S1S2,
        WAIT_C2,
        HANDSHAKE_COMPLETE
    };

    ~RtmpHandshake() = default;

    static RtmpHandshakePtr CreateServer() {
        return RtmpHandshakePtr(new RtmpHandshake(WAIT_C0C1));
    }

    static RtmpHandshakePtr CreateClient(char *c0c1_buf, size_t c0c1_buf_size) {
        if (BuildC0C1(c0c1_buf, c0c1_buf_size) == 0) {
            return nullptr;
        }

        auto ret = RtmpHandshakePtr(new RtmpHandshake(WAIT_S0S1S2));

        // 缓存 C1 的数据
        ret->c1_data_ = std::make_unique<uint8_t[]>(kC1C2S1S2Size);
        std::memcpy(ret->c1_data_.get(), c0c1_buf + kC0S0Size, kC1C2S1S2Size);
        return ret;
    }

    // 解析握手包并生成响应数据
    // 返回响应数据的长度, 输入的握手数据不足返回0, 输入参数非法返回-2, 握手校验失败返回-1
    int Parse(BufferReader &in_buffer, char *out_buf,
              size_t out_buf_size) {
        if (handshake_state_ == HANDSHAKE_COMPLETE) {
            return 0;
        }
        if (!out_buf) {
            return -2;
        }
        size_t res_size = 0;
        if (handshake_state_ == WAIT_C0C1) {

            // ===================== 服务器: 解析 C0C1 并生成 S0S1S2 的数据 =====================

            if (in_buffer.ReadableBytes() < kC0C1Total) {
                return 0;
            }

            // 1. Peek出C0C1
            auto *c0c1 = reinterpret_cast<const uint8_t *>(in_buffer.Peek());
            if (!c0c1) {
                return -2;
            }
            const uint8_t *c1 = c0c1 + kC0S0Size;

            // 2. 版本校验
            if (c0c1[0] != kRtmpVersion) {
                return -1;
            }

            // 3. 构建合并包 S0+S1+S2 (3073字节)
            if (out_buf_size < kS0S1S2Total) {
                return -2;
            }
            size_t s0s1_len = BuildS0S1(out_buf, out_buf_size);
            size_t s2_len = BuildS2(out_buf + s0s1_len, out_buf_size - s0s1_len,
                                    c1);
            res_size = s0s1_len + s2_len;

            // 4. 缓存 S1 的数据
            s1_data_ = std::make_unique<uint8_t[]>(kC1C2S1S2Size);
            std::memcpy(s1_data_.get(), out_buf + 1, kC1C2S1S2Size);

            // 5. 消耗已处理的数据
            in_buffer.Retrieve(kC0C1Total);
            handshake_state_ = WAIT_C2;
            return static_cast<int>(res_size);
        } else if (handshake_state_ == WAIT_S0S1S2) {

            // ===================== 客户端: 解析 S0S1S2 并生成 C2 数据 =====================

            if (in_buffer.ReadableBytes() < kS0S1S2Total) {
                return 0;
            }

            // 1. Peek出S0S1S2
            auto *s0s1s2 = reinterpret_cast<const uint8_t *>(in_buffer.Peek());
            if (!s0s1s2) {
                return -2;
            }
            const uint8_t *s1 = s0s1s2 + kC0S0Size;
            const uint8_t *s2 = s1 + kC1C2S1S2Size;

            // 2. 版本校验
            uint8_t s0_version = s0s1s2[0];
            if (s0_version != kRtmpVersion) {
                return -1;
            }

            // 3. S2 校验
            if (!ValidateS2(s2)) {
                return -1;
            }

            // 4. 构建C2回显包
            res_size = BuildC2(out_buf, out_buf_size, s1);
            if (res_size == 0) {
                return -2;
            }

            // 5. 消耗合并包数据
            in_buffer.Retrieve(kS0S1S2Total);
            handshake_state_ = HANDSHAKE_COMPLETE;
            return static_cast<int>(res_size);
        } else if (handshake_state_ == WAIT_C2) {

            // ===================== 服务器：解析 C2 数据 =====================

            if (in_buffer.ReadableBytes() < kC1C2S1S2Size) {
                return 0;
            }
            auto *c2 = reinterpret_cast<const uint8_t *>(in_buffer.Peek());
            if (!c2) {
                return -2;
            }

            // 1. C2 校验
            if (!ValidateC2(c2)) {
                return -1;
            }

            in_buffer.Retrieve(kC1C2S1S2Size);
            handshake_state_ = HANDSHAKE_COMPLETE;
            return 0;
        }

        return static_cast<int>(res_size);
    }

    [[nodiscard]] bool Completed() const {
        return handshake_state_ == HANDSHAKE_COMPLETE;
    }

    static size_t BuildC0C1(char *buf, size_t buf_size) {
        if (buf_size < kC0C1Total) {
            return 0;
        }
        std::memset(buf, 0, kC0C1Total);
        auto *buffer = reinterpret_cast<uint8_t *>(buf);
        size_t offset = 0;

        // 1. 写入 C0 (1B 版本号)
        ByteIO::WriteUInt8(buffer, kC0C1Total, offset,
                           kRtmpVersion);

        // 2. 写入 C1

        // 2.1. 4B: 客户端自身的时间戳
        auto timestamp = static_cast<uint32_t>(::time(nullptr));
        ByteIO::WriteUInt32BE(buffer, kC0C1Total, offset,
                              timestamp);
        // 2.2. 4B: 0
        offset += 4;
        // 2.3. 1528B: 随机数
        ByteIO::WriteRandomBytes(buffer, kC0C1Total, offset,
                                          kC1C2S1S2Size - 8);

        return kC0C1Total;
    }

    static size_t BuildS0S1(char *buf, size_t buf_size) {
        return BuildC0C1(buf, buf_size);
    }

    static size_t BuildS2(char *buf, size_t buf_size, const uint8_t *c1_data) {
        if (buf_size < kC1C2S1S2Size || !c1_data) return 0;
        std::memset(buf, 0, kC1C2S1S2Size);
        auto *buffer = reinterpret_cast<uint8_t *>(buf);
        size_t offset = 0;

        // 1. 4B: 服务器自身时间戳(大端)
        auto server_ts = static_cast<uint32_t>(::time(nullptr));
        ByteIO::WriteUInt32BE(buffer, kC1C2S1S2Size, offset,
                              server_ts);
        // 2. 4B: 回显客户端C1的时间戳(大端)
        ByteIO::WriteBytes(buffer, kC1C2S1S2Size, offset, c1_data,
                           4);
        // 3. 1528B: 回显客户端C1的随机数
        ByteIO::WriteBytes(buffer, kC1C2S1S2Size, offset,
                                    c1_data + 8, kC1C2S1S2Size - 8);
        return kC1C2S1S2Size;
    }

    static size_t BuildC2(char *buf, size_t buf_size, const uint8_t *s1_data) {
        if (buf_size < kC1C2S1S2Size || !s1_data) return 0;
        std::memset(buf, 0, kC1C2S1S2Size);
        auto *buffer = reinterpret_cast<uint8_t *>(buf);
        size_t offset = 0;

        // 1. 4B: 客户端自身时间戳(大端)
        auto client_ts = static_cast<uint32_t>(::time(nullptr));
        ByteIO::WriteUInt32BE(buffer, kC1C2S1S2Size, offset,
                              client_ts);
        // 2. 4B: 回显服务器S1的时间戳(大端)
        ByteIO::WriteBytes(buffer, kC1C2S1S2Size, offset, s1_data,
                           4);
        // 3. 1528B: 回显服务器S1的随机数
        ByteIO::WriteBytes(buffer, kC1C2S1S2Size, offset,
                                    s1_data + 8, kC1C2S1S2Size - 8);
        return kC1C2S1S2Size;
    }

    bool ValidateS2(const uint8_t *s2) const {
        if (s2 == nullptr || !c1_data_) {
            return false;
        }

        // 1. 判断S2的时间戳是否与C1缓存中的时间戳相同
        if (std::memcmp(s2 + 4, c1_data_.get(), 4) != 0) {
            return false;
        }
        // 2. 判断S2的随机数是否与C1缓存中的随机数相同
        if (std::memcmp(s2 + 8, c1_data_.get() + 8, kC1C2S1S2Size - 8) != 0) {
            return false;
        }
        return true;
    }

    bool ValidateC2(const uint8_t *c2) const {
        if (c2 == nullptr || !s1_data_) {
            return false;
        }

        // 1. 判断C2的时间戳是否与S1缓存中的时间戳相同
        if (std::memcmp(c2 + 4, s1_data_.get(), 4) != 0) {
            return false;
        }
        // 2. 判断C2的随机数是否与S1缓存中的随机数相同
        if (std::memcmp(c2 + 8, s1_data_.get() + 8, kC1C2S1S2Size - 8) != 0) {
            return false;
        }
        return true;
    }

private:
    explicit RtmpHandshake(State state)
        : handshake_state_(state), c1_data_(), s1_data_() {
    }

    State handshake_state_;
    std::unique_ptr<uint8_t[]> c1_data_;
    std::unique_ptr<uint8_t[]> s1_data_;
};

} // lsy::net::rtmp

#endif // RTMP_RTMPHANDSHAKE_H
