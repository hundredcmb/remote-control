#include "amf.h"

#include <cstring>

#include "base/ByteIO.h"

namespace lsy::net::rtmp {

int AmfDecoder::Decode(const char *data, int size, int max_decode_count) {
    if (!data || size <= 0) {
        return -1;
    }

    const auto *buf = reinterpret_cast<const uint8_t *>(data);
    const auto buf_len = static_cast<size_t>(size);
    size_t offset = 0;

    while (offset < buf_len) {
        uint8_t type = 0;
        if (!ByteIO::ReadUInt8(buf, buf_len, offset, type)) {
            return -1;
        }

        int ret = -1;
        switch (type) {
            case AMF0_NUMBER:
                m_obj.type = AMF_NUMBER;
                ret = DecodeNumber(buf, buf_len, offset, m_obj.amf_number);
                break;

            case AMF0_BOOLEAN:
                m_obj.type = AMF_BOOLEAN;
                ret = DecodeBoolean(buf, buf_len, offset, m_obj.amf_boolean);
                break;

            case AMF0_STRING:
                m_obj.type = AMF_STRING;
                ret = DecodeString(buf, buf_len, offset, m_obj.amf_string);
                break;

            case AMF0_OBJECT:
                ret = DecodeObject(buf, buf_len, offset, m_objs);
                break;

            case AMF0_ECMA_ARRAY: {
                uint32_t array_len = 0;
                if (!ByteIO::ReadUInt32BE(buf, buf_len, offset, array_len)) {
                    return -1;
                }
                ret = DecodeObject(buf, buf_len, offset, m_objs);
                break;
            }

            case AMF0_NULL:
                m_obj.type = AMF_NULL;
                ret = 0;
                break;

            default:
                ret = 0;
                break;
        }

        // 解码失败直接返回错误
        if (ret < 0) {
            return -1;
        }

        // 控制解码次数（-1 表示解码所有）
        if (max_decode_count > 0) {
            max_decode_count--;
            if (max_decode_count == 0) {
                break;
            }
        }
    }

    // 返回总消耗字节数
    return static_cast<int>(offset);
}

int AmfDecoder::DecodeBoolean(const uint8_t *buf, size_t buf_len,
                              size_t &offset, bool &amf_boolean) {
    uint8_t value = 0;
    if (!ByteIO::ReadUInt8(buf, buf_len, offset, value)) {
        return -1;
    }
    amf_boolean = (value != 0);
    return 1;
}

int AmfDecoder::DecodeNumber(const uint8_t *buf, size_t buf_len, size_t &offset,
                             double &amf_number) {
    // AMF0 数字是8字节大端序浮点数
    uint8_t double_bytes[8] = {0};
    if (!ByteIO::ReadBytes(buf, buf_len, offset, double_bytes, 8)) {
        return -1;
    }

    // 大端序转主机序（安全字节拷贝，无内存对齐崩溃）
    auto *dest = reinterpret_cast<uint8_t *>(&amf_number);
    for (int i = 0; i < 8; ++i) {
        dest[i] = double_bytes[7 - i];
    }

    return 8;
}

int AmfDecoder::DecodeString(const uint8_t *buf, size_t buf_len, size_t &offset,
                             std::string &amf_string) {
    // 读取2字节大端字符串长度
    uint16_t str_len = 0;
    if (!ByteIO::ReadUInt16BE(buf, buf_len, offset, str_len)) {
        return -1;
    }

    // 读取字符串内容
    auto *str_buf = new(std::nothrow) uint8_t[str_len]();
    if (!str_buf) {
        return -1;
    }

    if (!ByteIO::ReadBytes(buf, buf_len, offset, str_buf, str_len)) {
        delete[] str_buf;
        return -1;
    }

    amf_string = std::string(reinterpret_cast<const char *>(str_buf), str_len);
    delete[] str_buf;

    return 2 + str_len;
}

int AmfDecoder::DecodeObject(const uint8_t *buf, size_t buf_len, size_t &offset,
                             AmfObjects &amf_objs) {
    amf_objs.clear();
    const size_t start_offset = offset;

    while (true) {
        // 剩余空间不足3字节（结束符：00 00 09），退出
        if (offset + 3 > buf_len) {
            break;
        }

        // 读取键长度（2字节大端）
        uint16_t key_len = 0;
        if (!ByteIO::ReadUInt16BE(buf, buf_len, offset, key_len)) {
            return -1;
        }

        // ===================== AMF0 对象结束标记：00 00 09 =====================
        if (key_len == 0) {
            uint8_t end_type = 0;
            if (ByteIO::ReadUInt8(buf, buf_len, offset, end_type) &&
                end_type == AMF0_OBJECT_END) {
                break;
            }
            return -1;
        }

        // 读取键名
        auto *key_buf = new(std::nothrow) uint8_t[key_len]();
        if (!key_buf) {
            return -1;
        }
        if (!ByteIO::ReadBytes(buf, buf_len, offset, key_buf, key_len)) {
            delete[] key_buf;
            return -1;
        }
        std::string key(reinterpret_cast<const char *>(key_buf), key_len);
        delete[] key_buf;

        // 递归解码键对应的值
        AmfDecoder dec;
        int ret = dec.Decode(reinterpret_cast<const char *>(buf) + offset,
                             static_cast<int>(buf_len - offset), 1);
        if (ret <= 0) {
            return -1;
        }
        offset += ret;

        amf_objs.emplace(std::move(key), dec.GetObject());
    }

    // 返回对象总消耗字节数
    return static_cast<int>(offset - start_offset);
}

uint16_t AmfDecoder::DecodeInt16(const uint8_t *buf, size_t buf_len,
                                 size_t &offset) {
    uint16_t value = 0;
    ByteIO::ReadUInt16BE(buf, buf_len, offset, value);
    return value;
}

uint32_t AmfDecoder::DecodeInt24(const uint8_t *buf, size_t buf_len,
                                 size_t &offset) {
    uint32_t value = 0;
    ByteIO::ReadUInt24BE(buf, buf_len, offset, value);
    return value;
}

uint32_t AmfDecoder::DecodeInt32(const uint8_t *buf, size_t buf_len,
                                 size_t &offset) {
    uint32_t value = 0;
    ByteIO::ReadUInt32BE(buf, buf_len, offset, value);
    return value;
}

AmfEncoder::AmfEncoder(uint32_t size)
    : m_data_(std::make_unique<char[]>(size)),
      m_size_(size),
      m_index_(0) {
}

void AmfEncoder::ExpandBuffer(size_t required_size) {
    if (m_index_ + required_size <= m_size_) {
        return;
    }
    // 扩容策略：新大小 = 当前大小 + 所需空间 + 1KB冗余
    size_t new_size = m_index_ + required_size + 1024;
    auto new_data = std::make_unique<char[]>(new_size);
    std::memcpy(new_data.get(), m_data_.get(), m_index_);
    m_size_ = static_cast<uint32_t>(new_size);
    m_data_ = std::move(new_data);
}

bool AmfEncoder::SafeWriteUInt8(uint8_t value) {
    ExpandBuffer(sizeof(uint8_t));
    auto *buf = reinterpret_cast<uint8_t *>(m_data_.get());
    return ByteIO::WriteUInt8(buf, m_size_, m_index_, value);
}

bool AmfEncoder::SafeWriteUInt16BE(uint16_t value) {
    ExpandBuffer(sizeof(uint16_t));
    auto *buf = reinterpret_cast<uint8_t *>(m_data_.get());
    return ByteIO::WriteUInt16BE(buf, m_size_, m_index_, value);
}

bool AmfEncoder::SafeWriteUInt24BE(uint32_t value) {
    ExpandBuffer(3);
    auto *buf = reinterpret_cast<uint8_t *>(m_data_.get());
    return ByteIO::WriteUInt24BE(buf, m_size_, m_index_, value);
}

bool AmfEncoder::SafeWriteUInt32BE(uint32_t value) {
    ExpandBuffer(sizeof(uint32_t));
    auto *buf = reinterpret_cast<uint8_t *>(m_data_.get());
    return ByteIO::WriteUInt32BE(buf, m_size_, m_index_, value);
}

bool AmfEncoder::SafeWriteBytes(const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return true;
    }
    ExpandBuffer(len);
    auto *buf = reinterpret_cast<uint8_t *>(m_data_.get());
    return ByteIO::WriteBytes(buf, m_size_, m_index_, data, len);
}

void AmfEncoder::EncodeString(const char *str, int len, bool isObject) {
    if (len < 0) { len = 0; }
    if (len == 0 && !str) { str = ""; }

    auto str_len = static_cast<size_t>(len);
    if (isObject) {
        SafeWriteUInt8(str_len < 65536 ? AMF0_STRING : AMF0_LONG_STRING);
    }

    // 写入长度 + 字符串内容
    if (str_len < 65536) {
        SafeWriteUInt16BE(static_cast<uint16_t>(str_len));
    } else {
        SafeWriteUInt32BE(static_cast<uint32_t>(str_len));
    }

    SafeWriteBytes(reinterpret_cast<const uint8_t *>(str), str_len);
}

void AmfEncoder::EncodeNumber(double value) {
    SafeWriteUInt8(AMF0_NUMBER);
    ExpandBuffer(8);

    // AMF0 双精度浮点数 = 8字节大端序（安全拷贝，无对齐崩溃）
    uint8_t *dest = reinterpret_cast<uint8_t *>(m_data_.get()) + m_index_;
    const auto *src = reinterpret_cast<const uint8_t *>(&value);
    for (int i = 0; i < 8; ++i) {
        dest[i] = src[7 - i];
    }
    m_index_ += 8;
}

void AmfEncoder::EncodeBoolean(int value) {
    SafeWriteUInt8(AMF0_BOOLEAN);
    SafeWriteUInt8(value ? 0x01 : 0x00);
}

void AmfEncoder::EncodeObjects(AmfObjects &objs) {
    // 空对象也编码标准OBJECT
    SafeWriteUInt8(AMF0_OBJECT);

    // 编码键值对
    for (const auto &iter: objs) {
        // 编码键（不带类型）
        EncodeString(iter.first.c_str(), (int) iter.first.size(), false);
        // 编码值
        switch (iter.second.type) {
            case AMF_NUMBER:
                EncodeNumber(iter.second.amf_number);
                break;
            case AMF_STRING:
                EncodeString(iter.second.amf_string.c_str(),
                             (int) iter.second.amf_string.size());
                break;
            case AMF_BOOLEAN:
                EncodeBoolean(iter.second.amf_boolean);
                break;
            default:
                break;
        }
    }

    // 标准AMF0对象结束符：0x0000 + 0x09
    EncodeString("", 0, false);
    SafeWriteUInt8(AMF0_OBJECT_END);
}

void AmfEncoder::EncodeECMA(AmfObjects &objs) {
    // ECMA数组：类型(1) + 4字节数组长度(默认0)
    SafeWriteUInt8(AMF0_ECMA_ARRAY);
    SafeWriteUInt32BE(0);

    // 编码键值对
    for (const auto &iter: objs) {
        EncodeString(iter.first.c_str(), (int) iter.first.size(), false);
        switch (iter.second.type) {
            case AMF_NUMBER:
                EncodeNumber(iter.second.amf_number);
                break;
            case AMF_STRING:
                EncodeString(iter.second.amf_string.c_str(),
                             (int) iter.second.amf_string.size());
                break;
            case AMF_BOOLEAN:
                EncodeBoolean(iter.second.amf_boolean);
                break;
            default:
                break;
        }
    }

    // 结束符
    EncodeString("", 0, false);
    SafeWriteUInt8(AMF0_OBJECT_END);
}

} // lsy::net::rtmp
