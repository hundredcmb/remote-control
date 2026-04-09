#ifndef RTMP_RTMPSESSION_H
#define RTMP_RTMPSESSION_H

#include <mutex>

#include "amf.h"
#include "RtmpSink.h"

namespace lsy::net::rtmp {

class RtmpSession;

using RtmpSessionPtr = std::shared_ptr<RtmpSession>;

class RtmpSession {
public:
    RtmpSession();

    virtual ~RtmpSession();

    void SetAvcSequenceHeader(std::shared_ptr<char> avcSequenceHeader,
                              uint32_t avcSequenceHeaderSize) {
        std::lock_guard<std::mutex> lock(mutex_);
        avc_sequence_header_ = avcSequenceHeader;
        avc_sequence_header_size_ = avcSequenceHeaderSize;
    }

    void SetAacSequenceHeader(std::shared_ptr<char> aacSequenceHeader,
                              uint32_t aacSequenceHeaderSize) {
        std::lock_guard<std::mutex> lock(mutex_);
        aac_sequence_header_ = aacSequenceHeader;
        aac_sequence_header_size_ = aacSequenceHeaderSize;
    }

    void AddSink(std::shared_ptr<RtmpSink> sink);

    void RemoveSink(std::shared_ptr<RtmpSink> sink);

    int GetClients();

    void SendMetaData(AmfObjects &metaData);

    void SendMediaData(uint8_t type, uint64_t timestamp,
                       std::shared_ptr<char> data, uint32_t size);

private:
    std::mutex mutex_;
    bool has_publisher_ = false;
    std::weak_ptr<RtmpSink> publisher_;
    std::unordered_map<int, std::weak_ptr<RtmpSink>> rtmp_sinks_;
    std::shared_ptr<char> avc_sequence_header_;
    std::shared_ptr<char> aac_sequence_header_;
    uint32_t avc_sequence_header_size_ = 0;
    uint32_t aac_sequence_header_size_ = 0;
};

} // lsy::net::rtmp

#endif // RTMP_RTMPSESSION_H
