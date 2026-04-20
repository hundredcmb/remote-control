#ifndef RTMP_RTMPCONNECTION_H
#define RTMP_RTMPCONNECTION_H

#include "net/TcpConnection.h"
#include "rtmp/RtmpSink.h"

namespace lsy::net::rtmp {

class RtmpServer;

class RtmpSession;

class RtmpMessage;

class RtmpHandshake;

class RtmpConnection;

class RtmpMessageCodec;

using RtmpConnectionPtr = std::shared_ptr<RtmpConnection>;

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

    RtmpConnection(const std::shared_ptr<RtmpServer> &server,
                   TaskSchedulerPtr scheduler, int sockfd);

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

    bool HandleSetWindowAckSize(RtmpMessage &rtmp_msg);

    bool HandleUserControl(RtmpMessage &rtmp_msg);

    bool HandleConnect();

    bool HandleCreateStream();

    bool HandleReleaseStream();

    bool HandleFCPublish();

    bool HandleFCUnpublish();

    bool HandleGetStreamLength();

    bool HandlePublish();

    bool HandlePlay();

    bool HandleDeleteStream();

    bool SendRtmpChunks(uint32_t csid, RtmpMessage &rtmp_msg);

    bool SendSetPeerBandwidth();

    bool SendAcknowledgement();

    bool SendSetChunkSize();

    bool SendInvokeMessage(uint32_t csid,
                           std::unique_ptr<uint8_t[]> &&payload,
                           uint32_t payload_size);

    bool SendNotifyMessage(uint32_t csid,
                           std::unique_ptr<uint8_t[]> &&payload,
                           uint32_t payload_size);

    bool SendMetaData(const AmfObjects &meta_data) override;

    bool SendMediaData(uint8_t type, uint64_t timestamp,
                       std::shared_ptr<uint8_t[]> payload,
                       uint32_t payload_size) override;

    bool PayloadDecodeOne(const char *payload, size_t payload_len,
                          size_t &offset);

    bool PayloadDecode(const char *payload, size_t payload_len,
                       size_t &offset, int count);

    bool PayloadDecodeString(const char *payload, size_t payload_len,
                             size_t &offset, std::string &str);

    bool PayloadDecodeObjects(const char *payload, size_t payload_len,
                              size_t &offset, AmfObjects &objs);

    static bool IsAvcKeyFrame(std::unique_ptr<uint8_t[]> &payload,
                              uint32_t payload_size);

    State state_;
    std::shared_ptr<RtmpHandshake> handshake_;
    std::shared_ptr<RtmpMessageCodec> msg_codec_;

    uint32_t peer_bandwidth_ = 5000000;
    uint32_t acknowledgement_size_ = 5000000;
    uint32_t max_chunk_size_ = 128;
    uint32_t stream_id_ = 0;

    AmfObjects meta_data_;
    AmfDecoder amf_decoder_;
    AmfEncoder amf_encoder_;

    bool is_playing_ = false;
    bool is_publishing_ = false;
    bool has_key_frame_ = false;

    std::string app_;
    std::string stream_name_;
    std::string stream_path_;

    std::weak_ptr<RtmpServer> rtmp_server_;
    std::weak_ptr<RtmpSession> rtmp_session_;

    std::shared_ptr<uint8_t[]> avc_sequence_header_;
    std::shared_ptr<uint8_t[]> aac_sequence_header_;
    uint32_t avc_sequence_header_size_ = 0;
    uint32_t aac_sequence_header_size_ = 0;
};

} // lsy::net::rtmp

#endif // RTMP_RTMPCONNECTION_H
