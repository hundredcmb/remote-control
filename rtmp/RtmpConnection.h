#ifndef RTMP_RTMPCONNECTION_H
#define RTMP_RTMPCONNECTION_H

#include "net/TcpConnection.h"
#include "rtmp/RtmpSink.h"
#include "rtmp/RtmpMessage.h"
#include "rtmp/RtmpHandshake.h"
#include "rtmp/RtmpMessageCodec.h"

namespace lsy::net::rtmp {

class RtmpConnection : public TcpConnection, public RtmpSink {
public:
    enum State {
        HANDSHAKE,
        START_CONNECT,
        START_CREATE_STREAM,
        START_DELETE_STREAM,
        START_PLAY,
        START_PUBLISH,
    };

    RtmpConnection(TaskSchedulerPtr scheduler, int sockfd);

    ~RtmpConnection() override;

    bool IsPlayer() override;

    bool IsPublisher() override;

    bool IsPlaying() override;

    bool IsPublishing() override;

    uint32_t GetId() override;

private:
    bool OnRead(BufferReader &buffer);

    void OnClose();

    bool HandleHandshake(BufferReader &buffer);

    bool HandleChunk(BufferReader &buffer);

    bool HandleMessage(RtmpMessage &rtmp_msg);

    bool HandleInvoke(RtmpMessage &rtmp_msg);

    bool HandleNotify(RtmpMessage &rtmp_msg);

    bool HandleVideo(RtmpMessage &rtmp_msg);

    bool HandleAudio(RtmpMessage &rtmp_msg);

    bool HandleSetChunkSize(RtmpMessage &rtmp_msg);

    bool HandleSetPeerBandwidth(RtmpMessage &rtmp_msg);

    bool HandleAcknowledgement(RtmpMessage &rtmp_msg);

    bool HandleConnect();

    bool HandleCreateStream();

    bool HandlePublish();

    bool HandlePlay();

    bool HandleDeleteStream();

    bool SendRtmpChunks(uint32_t csid, RtmpMessage &rtmp_msg);

    bool SendSetPeerBandwidth();

    bool SendAcknowledgement();

    bool SendSetChunkSize();

    bool SendInvokeMessage(uint32_t csid, const std::shared_ptr<char> &payload,
                           uint32_t payload_size);

    bool SendNotifyMessage(uint32_t csid, const std::shared_ptr<char> &payload,
                           uint32_t payload_size);

    bool SendMetaData(const AmfObjects &meta_data) override;

    bool SendMediaData(uint8_t type, uint64_t timestamp,
                       std::shared_ptr<char> payload,
                       uint32_t payload_size) override;

    std::shared_ptr<RtmpHandshake> handshake_;
    std::shared_ptr<RtmpMessageCodec> msg_codec_;
    State connection_state_;

    uint32_t peer_bandwidth_ = 5000000;
    uint32_t acknowledgement_size_ = 5000000;
    uint32_t max_chunk_size_ = 128;
    uint32_t stream_id_ = 0;

    AmfObjects meta_data_;
    AmfDecoder amf_decoder_;
    AmfEncoder amf_encoder_;

    bool is_playing_ = false;
    bool is_publishing_ = false;

    std::shared_ptr<char> avc_sequence_header_;
    std::shared_ptr<char> aac_sequence_header_;
    uint32_t avc_sequence_header_size_ = 0;
    uint32_t aac_sequence_header_size_ = 0;
};

} // lsy::net::rtmp

#endif // RTMP_RTMPCONNECTION_H
