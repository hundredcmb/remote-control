#ifndef NET_RTMPMESSAGE_H
#define NET_RTMPMESSAGE_H

#include <memory>

namespace lsy::net::rtmp {

struct RtmpMessageHeader {
    uint8_t timestamp[3]{};
    uint8_t length[3]{};
    uint8_t type_id{};
    uint8_t stream_id[4]{};
};

struct RtmpMessage {
    uint32_t timestamp{};
    uint32_t length{};
    uint8_t type_id{};
    uint32_t stream_id{};
    uint32_t extend_timestamp{};
    uint8_t csid{};
    uint32_t idx{};
    std::shared_ptr<char[]> payload;
    uint64_t _timestamp{};
    uint8_t code_id{};

    [[nodiscard]] bool Completed() const {
        return length == idx && length > 0 && payload;
    }
};

} // lsy::net::rtmp

#endif // NET_RTMPMESSAGE_H
