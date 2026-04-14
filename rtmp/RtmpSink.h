#ifndef RTMP_RTMPSINK_H
#define RTMP_RTMPSINK_H

#include "amf.h"

namespace lsy::net::rtmp {

class RtmpSink;

using RtmpSinkWeakPtr = std::weak_ptr<RtmpSink>;
using RtmpSinkSharedPtr = std::shared_ptr<RtmpSink>;

class RtmpSink {
public:
    RtmpSink() = default;

    virtual ~RtmpSink() = default;

    virtual uint32_t GetId() = 0;

    virtual bool SendMediaData(uint8_t type, uint64_t timestamp,
                               std::shared_ptr<char[]> payload,
                               uint32_t payload_size) = 0;

    virtual bool SendMetaData(const AmfObjects &meta_data) = 0;

    virtual bool IsPlayer() {
        return false;
    }

    virtual bool IsPublisher() {
        return false;
    };

    virtual bool IsPlaying() {
        return false;
    }

    virtual bool IsPublishing() {
        return false;
    };
};

} // lsy::net::rtmp

#endif // RTMP_RTMPSINK_H
