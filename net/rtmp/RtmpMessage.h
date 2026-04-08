#ifndef NET_RTMPMESSAGE_H
#define NET_RTMPMESSAGE_H

#include <memory>

namespace lsy::net::rtmp {

struct RtmpMessage {
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

#endif // NET_RTMPMESSAGE_H
