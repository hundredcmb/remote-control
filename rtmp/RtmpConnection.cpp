#include "RtmpConnection.h"

namespace lsy::net::rtmp {

RtmpConnection::RtmpConnection(TaskSchedulerPtr scheduler, int sockfd)
    : TcpConnection(std::move(scheduler), sockfd),
      msg_codec_(std::make_shared<RtmpMessageCodec>()),
      connection_state_(State::HANDSHAKE) {
    TcpConnection::SetReadCallback(
        [this](const TcpConnectionPtr &conn_ptr, BufferReader &buffer) {
            this->OnRead(buffer);
        }
    );
    TcpConnection::SetCloseCallback(
        [this](const TcpConnectionPtr &conn_ptr) {
            this->OnClose();
        }
    );
}

RtmpConnection::~RtmpConnection() = default;

bool RtmpConnection::IsPlayer() {
    return connection_state_ == START_PLAY;
}

bool RtmpConnection::IsPublisher() {
    return connection_state_ == START_PUBLISH;
}

bool RtmpConnection::IsPlaying() {
    return is_playing_;
}

bool RtmpConnection::IsPublishing() {
    return is_publishing_;
}

uint32_t RtmpConnection::GetId() {
    return static_cast<uint32_t>(TcpConnection::GetSocket());
}

bool RtmpConnection::OnRead(BufferReader &buffer) {
    if (!handshake_->Completed()) {
        if (!HandleHandshake(buffer)) {
            return false;
        }
    }
    if (handshake_->Completed()) {
        if (!HandleChunk(buffer)) {
            return false;
        }
    }
    return true;
}

void RtmpConnection::OnClose() {
    HandleDeleteStream();
}

bool RtmpConnection::HandleHandshake(BufferReader &buffer) {
    if (buffer.ReadableBytes() == 0) {
        return true;
    }
    constexpr size_t kHandshakeBufSize = 3200; // S0S1S2Total=3073
    std::shared_ptr<char[]> res(new char[kHandshakeBufSize]);
    int res_size = handshake_->Parse(buffer, res.get(), kHandshakeBufSize);
    if (res_size < 0) {
        fprintf(stderr, "Handshake Parse error\n");
        buffer.RetrieveAll();
        return false;
    } else if (res_size > 0) {
        TcpConnection::Send(res, res_size);
    }
    return true;
}

bool RtmpConnection::HandleChunk(BufferReader &buffer) {
    while (buffer.ReadableBytes() > 0) {
        RtmpMessage msg;
        int parsed = msg_codec_->Parse(buffer, msg);
        if (parsed < 0) {
            fprintf(stderr, "Message Parse error\n");
            buffer.RetrieveAll();
            return false;
        } else if (parsed == 0) {
            return true;
        }
        if (msg.Completed()) {
            if (!HandleMessage(msg)) {
                return false;
            }
        }
    }
    return true;
}

bool RtmpConnection::HandleMessage(RtmpMessage &rtmp_msg) {
    if (!rtmp_msg.Completed()) {
        return false;
    }
    bool ret = true;
    switch (rtmp_msg.Type()) {
        case RtmpMessage::Type::VIDEO:
            ret = HandleVideo(rtmp_msg);
            break;
        case RtmpMessage::Type::AUDIO:
            ret = HandleAudio(rtmp_msg);
            break;
        case RtmpMessage::Type::INVOKE:
            ret = HandleInvoke(rtmp_msg);
            break;
        case RtmpMessage::Type::NOTIFY:
            ret = HandleNotify(rtmp_msg);
            break;
        case RtmpMessage::Type::SET_CHUNK_SIZE:
            ret = HandleSetChunkSize(rtmp_msg);
            break;
        case RtmpMessage::Type::SET_PEER_BANDWIDTH:
            ret = HandleSetPeerBandwidth(rtmp_msg);
            break;
        case RtmpMessage::Type::ACKNOWLEDGEMENT:
            ret = HandleAcknowledgement(rtmp_msg);
            break;
        default:
            break;
    }
    return ret;
}

bool RtmpConnection::HandleSetChunkSize(RtmpMessage &rtmp_msg) {
    if (rtmp_msg.PayloadLen() != 4) {
        return false;
    }
    const uint8_t *payload = rtmp_msg.Payload();
    if (payload == nullptr) {
        return false;
    }
    uint32_t chunk_size = ByteIO::_ReadUInt32BE(payload);
    msg_codec_->SetInChunkSize(chunk_size);
    return true;
}

bool RtmpConnection::HandleSetPeerBandwidth(RtmpMessage &rtmp_msg) {
    return false;
}

bool RtmpConnection::HandleAcknowledgement(RtmpMessage &rtmp_msg) {
    return false;
}

bool RtmpConnection::HandleVideo(RtmpMessage &rtmp_msg) {
    return false;
}

bool RtmpConnection::HandleAudio(RtmpMessage &rtmp_msg) {
    return false;
}

bool RtmpConnection::HandleInvoke(RtmpMessage &rtmp_msg) {
    return false;
}

bool RtmpConnection::HandleNotify(RtmpMessage &rtmp_msg) {
    return false;
}

bool RtmpConnection::HandleCreateStream() {
    return false;
}

bool RtmpConnection::HandleDeleteStream() {
    return false;
}

bool RtmpConnection::HandleConnect() {
    return false;
}

bool RtmpConnection::HandlePublish() {
    return false;
}

bool RtmpConnection::HandlePlay() {
    return false;
}

bool RtmpConnection::SendRtmpChunks(uint32_t csid, RtmpMessage &rtmp_msg) {
    uint32_t payload_len = rtmp_msg.PayloadLen();
    uint32_t chunk_size = msg_codec_->OutChunkSize();

    // 计算Chunk分片数量
    uint32_t msg_total = 11 + payload_len;
    uint32_t chunk_num = (msg_total + chunk_size - 1) / chunk_size;
    // 计算Chunk Header总长度
    uint32_t base_header_len = 11 + chunk_num * 3;
    // 计算扩展时间戳长度
    uint32_t extended_ts_len = 4 * chunk_num;
    // 预估总长度
    uint32_t total_size = base_header_len + extended_ts_len + msg_total;

    std::shared_ptr<char[]> buffer(new char[total_size]);
    int size = msg_codec_->CreateChunks(csid, rtmp_msg,
                                        reinterpret_cast<uint8_t *>(buffer.get()),
                                        total_size);
    if (size <= 0) {
        return false;
    }
    TcpConnection::Send(buffer, size);
    return true;
}

bool RtmpConnection::SendSetPeerBandwidth() {
    constexpr size_t kPayloadSize = 5;
    std::unique_ptr<uint8_t[]> payload(new uint8_t[kPayloadSize]);
    ByteIO::_WriteUInt32BE(payload.get(), peer_bandwidth_);
    payload[4] = 2;
    RtmpMessage msg(RtmpMessage::Type::SET_PEER_BANDWIDTH, std::move(payload),
                    kPayloadSize);
    return SendRtmpChunks(RtmpMessage::ChunkStream::CONTROL_ID, msg);
}

bool RtmpConnection::SendAcknowledgement() {
    constexpr size_t kPayloadSize = 4;
    std::unique_ptr<uint8_t[]> payload(new uint8_t[kPayloadSize]);
    ByteIO::_WriteUInt32BE(payload.get(), acknowledgement_size_);
    RtmpMessage msg(RtmpMessage::Type::ACKNOWLEDGEMENT, std::move(payload),
                    kPayloadSize);
    return SendRtmpChunks(RtmpMessage::ChunkStream::CONTROL_ID, msg);
}

bool RtmpConnection::SendSetChunkSize() {
    msg_codec_->SetOutChunkSize(max_chunk_size_);
    constexpr size_t kPayloadSize = 4;
    std::unique_ptr<uint8_t[]> payload(new uint8_t[kPayloadSize]);
    ByteIO::_WriteUInt32BE(payload.get(), max_chunk_size_);
    RtmpMessage msg(RtmpMessage::Type::SET_CHUNK_SIZE, std::move(payload),
                    kPayloadSize);
    return SendRtmpChunks(RtmpMessage::ChunkStream::CONTROL_ID, msg);
}

bool RtmpConnection::SendInvokeMessage(uint32_t csid,
                                       const std::shared_ptr<char> &payload,
                                       uint32_t payload_size) {
    return false;
}

bool RtmpConnection::SendNotifyMessage(uint32_t csid,
                                       const std::shared_ptr<char> &payload,
                                       uint32_t payload_size) {
    return false;
}

bool RtmpConnection::SendMetaData(const AmfObjects &meta_data) {
    return false;
}

bool RtmpConnection::SendMediaData(uint8_t type, uint64_t timestamp,
                                   std::shared_ptr<char> payload,
                                   uint32_t payload_size) {
    return false;
}

} // lsy::net::rtmp
