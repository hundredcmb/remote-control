#ifndef BASE_BIGENDIANWRITER_H
#define BASE_BIGENDIANWRITER_H

#include <cstdint>
#include <cstring>
#include <random>

namespace lsy {

class BigEndianBuffer {
public:
    BigEndianBuffer() = delete;

    /**
     * @brief 写入 8位 无符号整数
     * @param buf 目标缓冲区
     * @param buf_len 缓冲区总长度
     * @param offset 写入偏移量（会自动更新）
     * @param value 写入的值
     * @return 成功true，失败false（越界）
     */
    static bool WriteUInt8(uint8_t *buf, size_t buf_len, size_t &offset,
                           uint8_t value) {
        if (offset + sizeof(uint8_t) > buf_len) return false;
        buf[offset++] = value;
        return true;
    }

    /**
     * @brief 写入 16位 大端序无符号整数
     */
    static bool WriteUInt16BE(uint8_t *buf, size_t buf_len, size_t &offset,
                              uint16_t value) {
        if (offset + sizeof(uint16_t) > buf_len) return false;
        buf[offset++] = (value >> 8) & 0xFF;
        buf[offset++] = value & 0xFF;
        return true;
    }

    /**
     * @brief 写入 32位 大端序无符号整数
     */
    static bool WriteUInt32BE(uint8_t *buf, size_t buf_len, size_t &offset,
                              uint32_t value) {
        if (offset + sizeof(uint32_t) > buf_len) return false;
        buf[offset++] = (value >> 24) & 0xFF;
        buf[offset++] = (value >> 16) & 0xFF;
        buf[offset++] = (value >> 8) & 0xFF;
        buf[offset++] = value & 0xFF;
        return true;
    }

    /**
     * @brief 批量写入字节数组
     */
    static bool WriteBytes(uint8_t *buf, size_t buf_len, size_t &offset,
                           const uint8_t *data, size_t data_len) {
        if (offset + data_len > buf_len) return false;
        std::memcpy(buf + offset, data, data_len);
        offset += data_len;
        return true;
    }

    /**
     * @brief 批量写入随机字节数组
     */
    static bool WriteRandomBytes(uint8_t *buf, size_t buf_len, size_t &offset,
                                 size_t random_len) {
        if (offset + random_len > buf_len) return false;
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        for (size_t i = 0; i < random_len; ++i) {
            buf[offset++] = dist(rng);
        }
        return true;
    }
};

} // lsy



#endif // BASE_BIGENDIANWRITER_H
