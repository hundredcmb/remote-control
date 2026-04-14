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
    size_t offset = 0;
    const char *payload = reinterpret_cast<const char *>(rtmp_msg.Payload());
    size_t payload_len = rtmp_msg.PayloadLen();
    amf_decoder_.Reset();

    // 解析消息名称
    std::string method;
    if (!PayloadDecodeString(payload, payload_len, offset, method)) {
        return false;
    }

    // 处理不同的方法
    if (rtmp_msg.StreamId() == 0) {
        if (!PayloadDecodeOne(payload, payload_len, offset)) {
            return false;
        }
        if (method == "connect") {
            if (!HandleConnect()) {
                return false;
            }
        } else if (method == "createStream") {
            if (!HandleCreateStream()) {
                return false;
            }
        } else {
            fprintf(stderr, "Unsupported invoke method: '%s'\n",
                    method.c_str());
            return false;
        }
    } else if (rtmp_msg.StreamId() == stream_id_) {
        if (!PayloadDecodeString(payload, payload_len, offset, stream_name_)) {
            return false;
        }
        stream_path_ = "/" + app_ + "/" + stream_name_;
        if (!PayloadDecodeOne(payload, payload_len, offset)) {
            return false;
        }
        if (method == "publish") {
            if (!HandlePublish()) {
                return false;
            }
        } else if (method == "play") {
            if (!HandlePlay()) {
                return false;
            }
        } else if (method == "deleteStream") {
            if (!HandleDeleteStream()) {
                return false;
            }
        } else {
            fprintf(stderr, "Unsupported invoke method: '%s'\n",
                    method.c_str());
            return false;
        }
    } else {
        fprintf(stderr, "Invalid invoke method: '%s'\n", method.c_str());
        return false;
    }
    return true;
}

bool RtmpConnection::HandleNotify(RtmpMessage &rtmp_msg) {
    size_t offset = 0;
    const char *payload = reinterpret_cast<const char *>(rtmp_msg.Payload());
    size_t payload_len = rtmp_msg.PayloadLen();
    amf_decoder_.Reset();

    // 解析消息名称
    std::string method, method1;
    if (!PayloadDecodeString(payload, payload_len, offset, method)) {
        return false;
    }

    // 处理不同的方法
    if (method == "@setDataFrame") {
        amf_decoder_.Reset();
        if (!PayloadDecodeString(payload, payload_len, offset, method1)) {
            return false;
        }
        if (method1 == "onMetaData") {
            if (!PayloadDecodeObjects(payload, payload_len, offset,
                                      meta_data_)) {
                return false;
            }

            // 获取session设置元数据

        } else {
            fprintf(stderr, "Unsupported notify method: '%s'\n",
                    method1.c_str());
            return false;
        }
    } else {
        fprintf(stderr, "Unsupported notify method: '%s'\n", method.c_str());
        return false;
    }
    return true;
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
                                       const std::shared_ptr<char[]> &payload,
                                       uint32_t payload_size) {
    return false;
}

bool RtmpConnection::SendNotifyMessage(uint32_t csid,
                                       const std::shared_ptr<char[]> &payload,
                                       uint32_t payload_size) {
    return false;
}

bool RtmpConnection::SendMetaData(const AmfObjects &meta_data) {
    return false;
}

bool RtmpConnection::SendMediaData(uint8_t type, uint64_t timestamp,
                                   std::shared_ptr<char[]> payload,
                                   uint32_t payload_size) {
    return false;
}

bool RtmpConnection::PayloadDecodeOne(const char *payload, size_t payload_len,
                                      size_t &offset) {
    int ret = amf_decoder_.Decode(payload + offset,
                                  payload_len - offset, 1);
    if (ret <= 0) {
        fprintf(stderr, "AMF Decode error\n");
        return false;
    } else {
        offset += ret;
        return true;
    }
}

bool RtmpConnection::PayloadDecodeString(const char *payload,
                                         size_t payload_len,
                                         size_t &offset,
                                         std::string &str) {
    if (!PayloadDecodeOne(payload, payload_len, offset)) {
        return false;
    }
    str = amf_decoder_.GetString();
    if (str.empty()) {
        fprintf(stderr, "AMF Decode string error\n");
        return false;
    }
    return true;
}

bool RtmpConnection::PayloadDecodeObjects(const char *payload,
                                          size_t payload_len,
                                          size_t &offset,
                                          AmfObjects &objs) {
    if (!PayloadDecodeOne(payload, payload_len, offset)) {
        return false;
    }
    objs = amf_decoder_.GetObjects();
    if (objs.empty()) {
        fprintf(stderr, "AMF Decode objects error\n");
        return false;
    }
    return true;
}

} // lsy::net::rtmp
