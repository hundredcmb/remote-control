#ifndef BASE_BYTEIO_H
#define BASE_BYTEIO_H

#include <random>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <algorithm>

namespace lsy {

class ByteIO {
public:
    ByteIO() = delete;

    /**
     * @brief 写入8位无符号整数
     */
    static bool WriteUInt8(uint8_t *buf, size_t buf_len, size_t &offset,
                           const uint64_t value) {
        if (!buf) return false;
        if (offset + sizeof(uint8_t) > buf_len || value > UINT8_MAX)
            return false;
        buf[offset++] = static_cast<uint8_t>(value & 0xFF);
        return true;
    }

    /**
     * @brief 写入16位大端序无符号整数
     */
    static bool WriteUInt16BE(uint8_t *buf, size_t buf_len, size_t &offset,
                              const uint64_t value) {
        if (!buf) return false;
        if (offset + sizeof(uint16_t) > buf_len || value > UINT16_MAX)
            return false;
        buf[offset++] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buf[offset++] = static_cast<uint8_t>(value & 0xFF);
        return true;
    }

    /**
     * @brief 写入24位大端序无符号整数
     */
    static bool WriteUInt24BE(uint8_t *buf, size_t buf_len, size_t &offset,
                              const uint64_t value) {
        if (!buf) return false;
        constexpr uint32_t MAX_24BIT = 0xFFFFFF;
        if (offset + 3 > buf_len || value > MAX_24BIT) return false;
        buf[offset++] = static_cast<uint8_t>((value >> 16) & 0xFF);
        buf[offset++] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buf[offset++] = static_cast<uint8_t>(value & 0xFF);
        return true;
    }

    /**
     * @brief 写入32位大端序无符号整数
     */
    static bool WriteUInt32BE(uint8_t *buf, size_t buf_len, size_t &offset,
                              const uint64_t value) {
        if (!buf) return false;
        if (offset + sizeof(uint32_t) > buf_len || value > UINT32_MAX)
            return false;
        buf[offset++] = static_cast<uint8_t>((value >> 24) & 0xFF);
        buf[offset++] = static_cast<uint8_t>((value >> 16) & 0xFF);
        buf[offset++] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buf[offset++] = static_cast<uint8_t>(value & 0xFF);
        return true;
    }

    /**
     * @brief 写入32位小端序无符号整数
     */
    static bool WriteUInt32LE(uint8_t *buf, size_t buf_len, size_t &offset,
                              const uint64_t value) {
        if (!buf) return false;
        if (offset + sizeof(uint32_t) > buf_len || value > UINT32_MAX)
            return false;
        buf[offset++] = static_cast<uint8_t>(value & 0xFF);
        buf[offset++] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buf[offset++] = static_cast<uint8_t>((value >> 16) & 0xFF);
        buf[offset++] = static_cast<uint8_t>((value >> 24) & 0xFF);
        return true;
    }

    /**
     * @brief 批量写入字节数组
     */
    static bool WriteBytes(uint8_t *buf, size_t buf_len, size_t &offset,
                           const uint8_t *data, size_t data_len) {
        if (!buf || !data) return false;
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
        if (!buf) return false;
        if (offset + random_len > buf_len) return false;
        static thread_local std::mt19937_64 rng(
            std::random_device{}() +
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
        std::generate_n(buf + offset, random_len, [&]() {
            return std::uniform_int_distribution<uint8_t>{0, 255}(rng);
        });
        offset += random_len;
        return true;
    }

    /**
     * @brief 读取8位无符号整数
     */
    static bool ReadUInt8(const uint8_t *buf, size_t buf_len, size_t &offset,
                          uint8_t &value) {
        if (!buf) return false;
        if (offset + sizeof(uint8_t) > buf_len)
            return false;
        value = buf[offset++];
        return true;
    }

    /**
     * @brief 读取16位大端序无符号整数
     */
    static bool ReadUInt16BE(const uint8_t *buf, size_t buf_len, size_t &offset,
                             uint16_t &value) {
        if (!buf) return false;
        if (offset + sizeof(uint16_t) > buf_len)
            return false;
        value = (static_cast<uint16_t>(buf[offset]) << 8)
                | static_cast<uint16_t>(buf[offset + 1]);
        offset += sizeof(uint16_t);
        return true;
    }

    /**
     * @brief 读取24位大端序无符号整数
     */
    static bool ReadUInt24BE(const uint8_t *buf, size_t buf_len, size_t &offset,
                             uint32_t &value) {
        if (!buf) return false;
        if (offset + 3 > buf_len)
            return false;
        value = (static_cast<uint32_t>(buf[offset]) << 16)
                | (static_cast<uint32_t>(buf[offset + 1]) << 8)
                | static_cast<uint32_t>(buf[offset + 2]);
        offset += 3;
        return true;
    }

    /**
     * @brief 读取32位大端序无符号整数
     */
    static bool ReadUInt32BE(const uint8_t *buf, size_t buf_len, size_t &offset,
                             uint32_t &value) {
        if (!buf) return false;
        if (offset + sizeof(uint32_t) > buf_len)
            return false;
        value = (static_cast<uint32_t>(buf[offset]) << 24)
                | (static_cast<uint32_t>(buf[offset + 1]) << 16)
                | (static_cast<uint32_t>(buf[offset + 2]) << 8)
                | static_cast<uint32_t>(buf[offset + 3]);
        offset += sizeof(uint32_t);
        return true;
    }

    /**
     * @brief 读取32位小端序无符号整数
     */
    static bool ReadUInt32LE(const uint8_t *buf, size_t buf_len, size_t &offset,
                             uint32_t &value) {
        if (!buf) return false;
        if (offset + sizeof(uint32_t) > buf_len)
            return false;
        value = static_cast<uint32_t>(buf[offset])
                | (static_cast<uint32_t>(buf[offset + 1]) << 8)
                | (static_cast<uint32_t>(buf[offset + 2]) << 16)
                | (static_cast<uint32_t>(buf[offset + 3]) << 24);
        offset += sizeof(uint32_t);
        return true;
    }

    /**
     * @brief 批量读取字节数组
     */
    static bool ReadBytes(const uint8_t *buf, size_t buf_len, size_t &offset,
                          uint8_t *out_data, size_t data_len) {
        if (!buf || !out_data) return false;
        if (offset + data_len > buf_len) return false;
        std::memcpy(out_data, buf + offset, data_len);
        offset += data_len;
        return true;
    }

    /**
     * @brief 【不安全】写入8位无符号整数，无检查、无返回
     */
    static void _WriteUInt8(uint8_t *buf, const uint8_t value) {
        *buf = value;
    }

    /**
     * @brief 【不安全】写入16位大端序无符号整数，无检查、无返回
     */
    static void _WriteUInt16BE(uint8_t *buf, const uint16_t value) {
        buf[0] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buf[1] = static_cast<uint8_t>(value & 0xFF);
    }

    /**
     * @brief 【不安全】写入24位大端序无符号整数，无检查、无返回
     */
    static void _WriteUInt24BE(uint8_t *buf, const uint32_t value) {
        buf[0] = static_cast<uint8_t>((value >> 16) & 0xFF);
        buf[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buf[2] = static_cast<uint8_t>(value & 0xFF);
    }

    /**
     * @brief 【不安全】写入32位大端序无符号整数，无检查、无返回
     */
    static void _WriteUInt32BE(uint8_t *buf, const uint32_t value) {
        buf[0] = static_cast<uint8_t>((value >> 24) & 0xFF);
        buf[1] = static_cast<uint8_t>((value >> 16) & 0xFF);
        buf[2] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buf[3] = static_cast<uint8_t>(value & 0xFF);
    }

    /**
     * @brief 【不安全】写入32位小端序无符号整数，无检查、无返回
     */
    static void _WriteUInt32LE(uint8_t *buf, const uint32_t value) {
        buf[0] = static_cast<uint8_t>(value & 0xFF);
        buf[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
        buf[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
        buf[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    }

    /**
     * @brief 【不安全】批量写入字节，无检查、无返回
     */
    static void _WriteBytes(uint8_t *buf, const uint8_t *data, size_t data_len) {
        std::memcpy(buf, data, data_len);
    }

    /**
     * @brief 【不安全】写入随机字节，无检查、无返回
     */
    static void _WriteRandomBytes(uint8_t *buf, size_t random_len) {
        static thread_local std::mt19937_64 rng(
            std::random_device{}() +
            std::chrono::steady_clock::now().time_since_epoch().count()
        );
        std::generate_n(buf, random_len, [&]() {
            return std::uniform_int_distribution<uint8_t>{0, 255}(rng);
        });
    }

    /**
     * @brief 【不安全】读取8位无符号整数，直接返回值，无检查
     */
    static uint8_t _ReadUInt8(const uint8_t *buf) {
        return *buf;
    }

    /**
     * @brief 【不安全】读取16位大端序无符号整数，直接返回值，无检查
     */
    static uint16_t _ReadUInt16BE(const uint8_t *buf) {
        return (static_cast<uint16_t>(buf[0]) << 8) | buf[1];
    }

    /**
     * @brief 【不安全】读取24位大端序无符号整数，直接返回值，无检查
     */
    static uint32_t _ReadUInt24BE(const uint8_t *buf) {
        return (static_cast<uint32_t>(buf[0]) << 16)
               | (static_cast<uint32_t>(buf[1]) << 8)
               | buf[2];
    }

    /**
     * @brief 【不安全】读取32位大端序无符号整数，直接返回值，无检查
     */
    static uint32_t _ReadUInt32BE(const uint8_t *buf) {
        return (static_cast<uint32_t>(buf[0]) << 24)
               | (static_cast<uint32_t>(buf[1]) << 16)
               | (static_cast<uint32_t>(buf[2]) << 8)
               | buf[3];
    }

    /**
     * @brief 【不安全】读取32位小端序无符号整数，直接返回值，无检查
     */
    static uint32_t _ReadUInt32LE(const uint8_t *buf) {
        return static_cast<uint32_t>(buf[0])
               | (static_cast<uint32_t>(buf[1]) << 8)
               | (static_cast<uint32_t>(buf[2]) << 16)
               | (static_cast<uint32_t>(buf[3]) << 24);
    }

    /**
     * @brief 【不安全】批量读取字节，无检查、无返回
     */
    static void _ReadBytes(const uint8_t *buf, uint8_t *out_data, size_t data_len) {
        std::memcpy(out_data, buf, data_len);
    }
};

} // lsy

#endif // BASE_BYTEIO_H
