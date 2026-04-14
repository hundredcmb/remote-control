#ifndef RTMP_AMF0_H
#define RTMP_AMF0_H

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

namespace lsy::net::rtmp {

enum AMF0DataType : uint8_t {
    AMF0_NUMBER = 0,      // 数字类型（双精度浮点数）
    AMF0_BOOLEAN,         // 布尔类型
    AMF0_STRING,          // 字符串类型（短字符串）
    AMF0_OBJECT,          // 对象类型
    AMF0_MOVIECLIP,       // 保留字段，未使用
    AMF0_NULL,            // Null 空值
    AMF0_UNDEFINED,       // Undefined 未定义值
    AMF0_REFERENCE,       // 引用类型
    AMF0_ECMA_ARRAY,      // ECMA 数组（关联数组/键值对数组）
    AMF0_OBJECT_END,      // 对象结束标记
    AMF0_STRICT_ARRAY,    // 严格数组（索引数组）
    AMF0_DATE,            // 日期类型
    AMF0_LONG_STRING,     // 长字符串类型
    AMF0_UNSUPPORTED,     // 不支持的类型
    AMF0_RECORDSET,       // 保留字段，未使用
    AMF0_XML_DOC,         // XML 文档类型
    AMF0_TYPED_OBJECT,    // 带类型的对象
    AMF0_AVMPLUS,         // 协议切换标记（切换为 AMF3）
    AMF0_INVALID = 0xff   // 无效类型标记
};

enum AmfObjectType : uint8_t {
    AMF_NUMBER,
    AMF_BOOLEAN,
    AMF_STRING,
    AMF_NULL,
};

struct AmfObject {
    AmfObjectType type = AMF_NULL;
    std::string amf_string;
    double amf_number{};
    bool amf_boolean{};
};

typedef std::unordered_map<std::string, AmfObject> AmfObjects;

class AmfDecoder {
public:
    int Decode(const char *data, size_t size, int max_decode_count = 1);

    void Reset() {
        m_obj.type = AMF_NULL;
        m_obj.amf_string.clear();
        m_obj.amf_number = 0;
        m_obj.amf_boolean = false;
        m_objs.clear();
    }

    std::string GetString() const {
        return m_obj.amf_string;
    }

    double GetNumber() const {
        return m_obj.amf_number;
    }

    bool HasObject(const std::string &key) const {
        return m_objs.count(key) > 0;
    }

    AmfObject GetObject(const std::string &key) {
        return m_objs[key];
    }

    AmfObject GetObject() {
        return m_obj;
    }

    AmfObjects GetObjects() {
        return m_objs;
    }

private:
    static int DecodeBoolean(const uint8_t *buf, size_t buf_len, size_t &offset,
                             bool &amf_boolean);

    static int DecodeNumber(const uint8_t *buf, size_t buf_len, size_t &offset,
                            double &amf_number);

    static int DecodeString(const uint8_t *buf, size_t buf_len, size_t &offset,
                            std::string &amf_str);

    static int DecodeObject(const uint8_t *buf, size_t buf_len, size_t &offset,
                            AmfObjects &amf_objs);

    static uint16_t DecodeInt16(const uint8_t *buf, size_t buf_len,
                                size_t &offset);

    static uint32_t DecodeInt24(const uint8_t *buf, size_t buf_len,
                                size_t &offset);

    static uint32_t DecodeInt32(const uint8_t *buf, size_t buf_len,
                                size_t &offset);

    AmfObject m_obj;
    AmfObjects m_objs;
};

class AmfEncoder {
public:
    explicit AmfEncoder(uint32_t size = 1024);

    ~AmfEncoder() = default;

    [[nodiscard]] const char *Data() const { return m_data_.get(); }

    [[nodiscard]] uint32_t Size() const { return m_index_; }

    void EncodeString(const char *str, int len, bool isObject = true);

    void EncodeNumber(double value);

    void EncodeBoolean(int value);

    void EncodeObjects(AmfObjects &objs);

    void EncodeECMA(AmfObjects &objs);

private:
    bool SafeWriteUInt8(uint8_t value);

    bool SafeWriteUInt16BE(uint16_t value);

    bool SafeWriteUInt24BE(uint32_t value);

    bool SafeWriteUInt32BE(uint32_t value);

    bool SafeWriteBytes(const uint8_t *data, size_t len);

    void ExpandBuffer(size_t required_size);

    std::unique_ptr<char[]> m_data_;
    uint32_t m_size_;
    size_t m_index_;
};

} // lsy::net::rtmp

#endif // RTMP_AMF0_H
