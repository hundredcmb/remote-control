#ifndef NET_RTMPAMF0_H
#define NET_RTMPAMF0_H

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
};

struct AmfObject {
    AmfObject() = default;

    explicit AmfObject(std::string str) {
        type = AMF_STRING;
        amf_string = std::move(str);
    }

    explicit AmfObject(double number) {
        type = AMF_NUMBER;
        amf_number = number;
    }

    explicit AmfObject(bool boolean) {
        type = AMF_BOOLEAN;
        amf_boolean = boolean;
    }

    AmfObjectType type = AMF_STRING;
    std::string amf_string;
    double amf_number{};
    bool amf_boolean{};
};

typedef std::unordered_map<std::string, AmfObject> AmfObjects;

class AmfDecoder {
public:
    /* n: 解码次数 */
    int Decode(const char *data, int size, int n = -1);

    void Reset() {
        m_obj_.amf_string = "";
        m_obj_.amf_number = 0;
        m_objs_.clear();
    }

    std::string GetString() const { return m_obj_.amf_string; }

    double GetNumber() const { return m_obj_.amf_number; }

    bool HasObject(const std::string &key) const {
        return (m_objs_.find(key) != m_objs_.end());
    }

    AmfObject GetObject(const std::string &key) { return m_objs_[key]; }

    AmfObject GetObject() { return m_obj_; }

    AmfObjects GetObjects() { return m_objs_; }

private:
    static int DecodeBoolean(const char *data, int size, bool &amf_boolean);

    static int DecodeNumber(const char *data, int size, double &amf_number);

    static int DecodeString(const char *data, int size, std::string &amf_str);

    static int DecodeObject(const char *data, int size, AmfObjects &amf_objs);

    static uint16_t DecodeInt16(const char *data, int size);

    static uint32_t DecodeInt24(const char *data, int size);

    static uint32_t DecodeInt32(const char *data, int size);

    AmfObject m_obj_;
    AmfObjects m_objs_;
};

class AmfEncoder {
public:
    AmfEncoder(uint32_t size = 1024);

    ~AmfEncoder() = default;

    void Reset() {
        m_index_ = 0;
    }

    std::shared_ptr<char> Data() {
        return m_data_;
    }

    [[nodiscard]] uint32_t Size() const {
        return m_index_;
    }

    void EncodeString(const char *str, int len, bool isObject = true);

    void EncodeNumber(double value);

    void EncodeBoolean(int value);

    void EncodeObjects(AmfObjects &objs);

    void EncodeECMA(AmfObjects &objs);

private:
    void EncodeInt8(int8_t value);

    void EncodeInt16(int16_t value);

    void EncodeInt24(int32_t value);

    void EncodeInt32(int32_t value);

    void Realloc(uint32_t size);

    std::shared_ptr<char> m_data_;
    uint32_t m_size_ = 0;
    uint32_t m_index_ = 0;
};

} // lsy::net::rtmp

#endif // NET_RTMPAMF0_H
