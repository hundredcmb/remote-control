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
        NOTIFY = 0x12,          // AMF0通知
        INVOKE = 0x14,          // AMF0命令
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

    friend class RtmpMessageCodec;

    friend void swap(RtmpMessage &a, RtmpMessage &b) noexcept {
        std::swap(a.real_timestamp, b.real_timestamp);
        std::swap(a.timestamp, b.timestamp);
        std::swap(a.extend_timestamp, b.extend_timestamp);
        std::swap(a.is_completed_, b.is_completed_);
        std::swap(a.has_extend_ts_, b.has_extend_ts_);
        std::swap(a.csid_, b.csid_);
        std::swap(a.type_id_, b.type_id_);
        std::swap(a.stream_id_, b.stream_id_);
        std::swap(a.payload_, b.payload_);
        std::swap(a.payload_len_, b.payload_len_);
        std::swap(a.payload_offset_, b.payload_offset_);
    }

    RtmpMessage() = default;

    RtmpMessage(Type type_id, std::unique_ptr<uint8_t[]> &&payload,
                uint32_t payload_len)
        : type_id_(type_id),
          payload_(std::move(payload)),
          payload_len_(payload_len),
          is_completed_(true) {
    }

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

    [[nodiscard]] bool Completed() const {
        return is_completed_;
    }

    [[nodiscard]] uint32_t Type() const {
        return type_id_;
    }

    const uint8_t *Payload() {
        return payload_.get();
    }

    [[nodiscard]] uint32_t PayloadLen() const {
        return payload_len_;
    }

    [[nodiscard]] uint32_t StreamId() const {
        return stream_id_;
    }

private:
    uint64_t real_timestamp{};
    uint32_t timestamp{};
    uint32_t extend_timestamp{};
    bool is_completed_ = false;
    bool has_extend_ts_ = false;
    uint8_t csid_{};
    uint8_t type_id_{};
    uint32_t stream_id_{};
    std::unique_ptr<uint8_t[]> payload_;
    uint32_t payload_len_{};
    uint32_t payload_offset_{};
};

} // lsy::net::rtmp

#endif // RTMP_RTMPMESSAGE_H
