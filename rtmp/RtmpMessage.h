#ifndef RTMP_RTMPMESSAGE_H
#define RTMP_RTMPMESSAGE_H

#include <memory>

namespace lsy::net::rtmp {

using ChunkStreamID = uint32_t;

enum MediaDataType : uint32_t {
    AVC_SEQUENCE_HEADER = 0x18,
    AAC_SEQUENCE_HEADER = 0x19,
    AAC_AUDIO = 8,
    AVC_VIDEO = 9,
};

struct Flv {
    static constexpr uint32_t FRAME_TYPE_I = 1;
    static constexpr uint32_t FRAME_TYPE_P = 2;
    static constexpr uint32_t FRAME_TYPE_B = 3;
    static constexpr uint32_t CODEC_ID_AVC = 7;
    static constexpr uint32_t CODEC_ID_HEVC = 12;
    static constexpr uint32_t SOUND_FORMAT_AAC = 10;
    static constexpr uint32_t AAC_PACKET_TYPE_SEQUENCE_HEADER = 0;
    static constexpr uint32_t AAC_PACKET_TYPE_RAW_DATA = 1;
    static constexpr uint32_t AVC_PACKET_TYPE_SEQUENCE_HEADER = 0;
    static constexpr uint32_t AVC_PACKET_TYPE_NALU = 1;
    static constexpr uint32_t AVC_PACKET_TYPE_SEQUENCE_END = 2;
    static constexpr uint32_t TAG_TYPE_AUDIO = 8;
    static constexpr uint32_t TAG_TYPE_VIDEO = 9;
    static constexpr uint32_t TAG_TYPE_SCRIPT = 18;

    Flv() = delete;
};

struct RtmpMessage : noncopyable {
    static constexpr ChunkStreamID CSID_CONTROL = 2;
    static constexpr ChunkStreamID CSID_INVOKE = 3;
    static constexpr ChunkStreamID CSID_AUDIO = 4;
    static constexpr ChunkStreamID CSID_VIDEO = 5;
    static constexpr ChunkStreamID CSID_DATA = 6;

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

    friend class RtmpMessageCodec;

    friend void swap(RtmpMessage &a, RtmpMessage &b) noexcept {
        std::swap(a.real_timestamp, b.real_timestamp);
        std::swap(a.timestamp, b.timestamp);
        std::swap(a.extend_timestamp, b.extend_timestamp);
        std::swap(a.is_completed_, b.is_completed_);
        std::swap(a.has_extend_ts_, b.has_extend_ts_);
        std::swap(a.is_delta_ts_, b.is_delta_ts_);
        std::swap(a.is_first_chunk_header_, b.is_first_chunk_header_);
        std::swap(a.csid_, b.csid_);
        std::swap(a.type_id_, b.type_id_);
        std::swap(a.stream_id_, b.stream_id_);
        std::swap(a.payload_, b.payload_);
        std::swap(a.payload_len_, b.payload_len_);
        std::swap(a.payload_offset_, b.payload_offset_);
    }

    RtmpMessage() = default;

    RtmpMessage(uint8_t type_id, std::unique_ptr<uint8_t[]> &&payload,
                uint32_t payload_len)
        : type_id_(type_id),
          payload_(std::move(payload)),
          payload_len_(payload_len),
          is_completed_(true) {
    }

    RtmpMessage(uint8_t type_id, std::unique_ptr<uint8_t[]> &&payload,
                uint32_t payload_len, uint32_t stream_id, uint64_t timestamp)
        : type_id_(type_id),
          payload_(std::move(payload)),
          payload_len_(payload_len),
          stream_id_(stream_id),
          real_timestamp(timestamp),
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
        return is_completed_ && payload_len_ > 0;
    }

    [[nodiscard]] uint32_t Type() const {
        return type_id_;
    }

    const uint8_t *Payload() {
        return payload_.get();
    }

    std::shared_ptr<uint8_t[]> PayloadSharedPtr() {
        std::shared_ptr<uint8_t[]> ret(new uint8_t[payload_len_]);
        std::memcpy(ret.get(), payload_.get(), payload_len_);
        return ret;
    }

    [[nodiscard]] uint32_t PayloadLen() const {
        return payload_len_;
    }

    [[nodiscard]] uint32_t StreamId() const {
        return stream_id_;
    }

    [[nodiscard]] uint64_t Timestamp() const {
        return real_timestamp;
    }

private:
    uint64_t real_timestamp{};
    uint32_t timestamp{};
    uint32_t extend_timestamp{};
    bool is_completed_ = false;
    bool has_extend_ts_ = false;
    bool is_delta_ts_ = false;
    bool is_first_chunk_header_ = true;
    uint8_t csid_{};
    uint8_t type_id_{};
    uint32_t stream_id_{};
    std::unique_ptr<uint8_t[]> payload_;
    uint32_t payload_len_{};
    uint32_t payload_offset_{};
};

} // lsy::net::rtmp

#endif // RTMP_RTMPMESSAGE_H
