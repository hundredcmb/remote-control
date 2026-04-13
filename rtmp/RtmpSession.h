#ifndef RTMP_RTMPSESSION_H
#define RTMP_RTMPSESSION_H

#include <mutex>
#include <utility>

#include "amf.h"
#include "RtmpSink.h"
#include "RtmpMessage.h"

namespace lsy::net::rtmp {

class RtmpSession;

using RtmpSessionPtr = std::shared_ptr<RtmpSession>;

class RtmpSession {
public:
    RtmpSession() = default;

    virtual ~RtmpSession() = default;

    void SetAvcSequenceHeader(std::shared_ptr<char> avcSequenceHeader,
                              uint32_t avcSequenceHeaderSize) {
        std::lock_guard<std::mutex> lock(mutex_);
        avc_sequence_header_ = std::move(avcSequenceHeader);
        avc_sequence_header_size_ = avcSequenceHeaderSize;
    }

    void SetAacSequenceHeader(std::shared_ptr<char> aacSequenceHeader,
                              uint32_t aacSequenceHeaderSize) {
        std::lock_guard<std::mutex> lock(mutex_);
        aac_sequence_header_ = std::move(aacSequenceHeader);
        aac_sequence_header_size_ = aacSequenceHeaderSize;
    }

    void AddSink(const std::shared_ptr<RtmpSink> &sink) {
        std::lock_guard<std::mutex> lock(mutex_);
        rtmp_sinks_[sink->GetId()] = sink;
        if (sink->IsPublisher()) {
            avc_sequence_header_ = nullptr;
            avc_sequence_header_size_ = 0;
            aac_sequence_header_ = nullptr;
            aac_sequence_header_size_ = 0;
            publisher_ = sink;
        }
    }

    void RemoveSink(const std::shared_ptr<RtmpSink> &sink) {
        std::lock_guard<std::mutex> lock(mutex_);
        rtmp_sinks_.erase(sink->GetId());
    }

    size_t GetClients() {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t count = 0;
        for (const auto &it: rtmp_sinks_) {
            if (it.second.lock()) {
                count++;
            }
        }
        return count;
    }

    void SendMetaData(AmfObjects &meta_data) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = rtmp_sinks_.begin(); it != rtmp_sinks_.end();) {
            RtmpSinkSharedPtr sink = it->second.lock();
            if (sink) {
                if (sink->IsPlayer()) {
                    sink->SendMetaData(meta_data);
                }
                ++it;
            } else {
                it = rtmp_sinks_.erase(it);
            }
        }
    }

    void SendMediaData(uint8_t type, uint64_t timestamp,
                       const std::shared_ptr<char> &data, uint32_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = rtmp_sinks_.begin(); it != rtmp_sinks_.end();) {
            RtmpSinkSharedPtr sink = it->second.lock();
            if (!sink) {
                it = rtmp_sinks_.erase(it);
                continue;
            }
            if (sink->IsPlayer()) {
                if (!sink->IsPlaying()) {
                    sink->SendMediaData(MediaDataType::AVC_SEQUENCE_HEADER,
                                        timestamp, avc_sequence_header_,
                                        avc_sequence_header_size_);
                    sink->SendMediaData(MediaDataType::AAC_SEQUENCE_HEADER,
                                        timestamp, aac_sequence_header_,
                                        aac_sequence_header_size_);
                }
                sink->SendMediaData(type, timestamp, data, size);
            }
            ++it;
        }
    }

private:
    std::mutex mutex_;
    std::weak_ptr<RtmpSink> publisher_;
    std::unordered_map<uint32_t, RtmpSinkWeakPtr> rtmp_sinks_;
    std::shared_ptr<char> avc_sequence_header_;
    std::shared_ptr<char> aac_sequence_header_;
    uint32_t avc_sequence_header_size_ = 0;
    uint32_t aac_sequence_header_size_ = 0;
};

} // lsy::net::rtmp

#endif // RTMP_RTMPSESSION_H
