#ifndef NET_RTMPMESSAGE_H
#define NET_RTMPMESSAGE_H

#include <memory>

namespace lsy::net::rtmp {

using ChunkStreamID = uint32_t;

struct RtmpMessageHeader {
    uint8_t timestamp[3]{};
    uint8_t length[3]{};
    uint8_t type_id{};
    uint8_t stream_id[4]{};
};

struct RtmpMessage {
    uint32_t timestamp{};
    uint32_t timestamp_delta = 0;
    uint32_t length{};
    uint8_t type_id{};
    uint32_t stream_id{};
    uint32_t extend_timestamp{};
    uint32_t extend_timestamp_delta{};
    uint8_t csid{};
    uint32_t index{};
    std::shared_ptr<uint8_t[]> payload;
    uint64_t _timestamp{};              // 内部时间戳
    uint8_t code_id{};

    bool has_extend_ts = false;
    bool use_timestamp_delta = false;

    [[nodiscard]] bool Completed() const {
        return length == index && length > 0 && payload;
    }
};

} // lsy::net::rtmp

#endif // NET_RTMPMESSAGE_H
