#ifndef RTMP_RTMPMESSAGE_H
#define RTMP_RTMPMESSAGE_H

#include <memory>

namespace lsy::net::rtmp {

using ChunkStreamID = uint32_t;

enum MediaDataType : uint32_t {
    AVC_SEQUENCE_HEADER = 0x18,
    AAC_SEQUENCE_HEADER = 0x19,
};

struct RtmpMessage {
    enum Type : uint32_t {
        SET_CHUNK_SIZE = 1,     // 设置块大小
        ABORT = 2,              // 中止消息
        ACKNOWLEDGEMENT = 3,    // 确认接收
        USER_CONTROL = 4,       // 用户控制消息
        WINDOW_ACK_SIZE = 5,    // 窗口确认大小
        SET_PEER_BANDWIDTH = 6, // 设置对等端带宽
        AUDIO = 8,              // 音频数据
        VIDEO = 9,              // 视频数据
#if 0
        DATA_AMF0 = 16,         // AMF0 格式数据消息 (元数据)
        SHARED_OBJECT_AMF0 = 17,// AMF0 共享对象消息
        COMMAND_AMF0 = 18,      // AMF0 命令消息 (connect/play/publish)
        DATA_AMF3 = 19,         // AMF3 格式数据消息
        SHARED_OBJECT_AMF3 = 20,// AMF3 共享对象消息
        COMMAND_AMF3 = 21,      // AMF3 命令消息
        AGGREGATE = 22,         // 聚合消息
#else
        NOTIFY = 0x12,
        INVOKE = 0x14,
#endif
    };

    enum ChunkStream : ChunkStreamID {
        CONTROL_ID = 2,
        INVOKE_ID = 3,
        AUDIO_ID = 4,
        VIDEO_ID = 5,
        DATA_ID = 6,
    };

    enum Codec : uint32_t {
        AVC_ID = 7,
        AAC_ID = 10,
    };

    friend void swap(RtmpMessage &a, RtmpMessage &b) noexcept {
        std::swap(a.real_timestamp, b.real_timestamp);
        std::swap(a.timestamp, b.timestamp);
        std::swap(a.extend_timestamp, b.extend_timestamp);
        std::swap(a.has_extend_ts, b.has_extend_ts);
        std::swap(a.csid, b.csid);
        std::swap(a.type_id, b.type_id);
        std::swap(a.stream_id, b.stream_id);
        std::swap(a.payload, b.payload);
        std::swap(a.payload_len, b.payload_len);
        std::swap(a.payload_offset, b.payload_offset);
    }

    RtmpMessage() = default;

    RtmpMessage(RtmpMessage &&other) noexcept {
        swap(*this, other);
    }

    RtmpMessage &operator=(RtmpMessage other) noexcept {
        swap(*this, other);
        return *this;
    }

    void Clear() {
        RtmpMessage empty;
        swap(*this, empty);
    }

    uint64_t real_timestamp{};
    uint32_t timestamp{};
    uint32_t extend_timestamp{};
    bool has_extend_ts = false;
    uint8_t csid{};
    uint8_t type_id{};
    uint32_t stream_id{};
    std::unique_ptr<uint8_t[]> payload;
    uint32_t payload_len{};
    uint32_t payload_offset{};
};

} // lsy::net::rtmp

#endif // RTMP_RTMPMESSAGE_H
