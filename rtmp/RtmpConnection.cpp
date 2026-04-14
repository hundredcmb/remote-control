#include "RtmpConnection.h"

namespace lsy::net::rtmp {

uint32_t RtmpMessageCodec::next_stream_id_ = 0;

RtmpConnection::RtmpConnection(TaskSchedulerPtr scheduler, int sockfd)
    : TcpConnection(std::move(scheduler), sockfd),
      msg_codec_(std::make_shared<RtmpMessageCodec>()),
      state_(State::HANDSHAKE) {
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
    return state_ == START_PLAY;
}

bool RtmpConnection::IsPublisher() {
    return state_ == START_PUBLISH;
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
    const uint8_t *payload = rtmp_msg.Payload();
    size_t payload_len = rtmp_msg.PayloadLen();
    uint8_t frame_type = (payload[0] >> 4);
    uint8_t codec_id = payload[0] & 0x0f;
    uint8_t packet_type = payload[1];
    if (codec_id == Flv::CODEC_ID_AVC) {
        if (frame_type == Flv::FRAME_TYPE_I) {
            if (packet_type == Flv::AVC_PACKET_TYPE_SEQUENCE_HEADER) {
                avc_sequence_header_size_ = payload_len;
                avc_sequence_header_.reset(new uint8_t[payload_len]);
                memcpy(avc_sequence_header_.get(), payload, payload_len);
            } else if (packet_type == Flv::AVC_PACKET_TYPE_NALU) {

            } else {
                fprintf(stderr, "Unsupported video packet_type: %d\n",
                        packet_type);
                return false;
            }
        } else if (frame_type == Flv::FRAME_TYPE_P) {
            if (packet_type == Flv::AVC_PACKET_TYPE_NALU) {

            } else {
                fprintf(stderr, "Unsupported video packet_type: %d\n",
                        packet_type);
                return false;
            }
        } else {
            fprintf(stderr, "Unsupported video frame_type: %d\n", frame_type);
            return false;
        }
    } else {
        fprintf(stderr, "Unsupported video codec_id: %d\n", codec_id);
        return false;
    }
    return true;
}

bool RtmpConnection::HandleAudio(RtmpMessage &rtmp_msg) {
    const uint8_t *payload = rtmp_msg.Payload();
    size_t payload_len = rtmp_msg.PayloadLen();
    uint8_t sound_format = (payload[0] >> 4);
    //uint8_t codec_info = payload[0] & 0x0f;
    uint8_t packet_type = payload[1];
    if (sound_format == Flv::SOUND_FORMAT_AAC) {
        if (packet_type == Flv::AAC_PACKET_TYPE_SEQUENCE_HEADER) {
            aac_sequence_header_size_ = payload_len;
            aac_sequence_header_.reset(new uint8_t[payload_len]);
            memcpy(aac_sequence_header_.get(), payload, payload_len);

            // TODO: 获取session设置AAC序列

        } else if (packet_type == Flv::AAC_PACKET_TYPE_RAW_DATA) {

            // TODO: 获取session处理音频数据

        } else {
            fprintf(stderr, "Unsupported AAC packet_type: %d\n", packet_type);
        }
    } else {
        fprintf(stderr, "Unsupported sound_format: %d\n", sound_format);
        return false;
    }

    return true;
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
        if (!PayloadDecode(payload, payload_len, offset, -1)) {
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
        if (!PayloadDecode(payload, payload_len, offset, 3)) {
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
    printf("================= HandleNotify =================\n");
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

            // TODO: 获取session设置元数据

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

bool RtmpConnection::HandleConnect() {
    printf("================= HandleConnect =================\n");
    if (!amf_decoder_.HasObject("app")) {
        fprintf(stderr, "app not found\n");
        return false;
    }

    AmfObject obj = amf_decoder_.GetObject("app");
    app_ = obj.amf_string;
    if (app_.empty()) {
        fprintf(stderr, "app is empty\n");
        return false;
    }

    SendSetChunkSize();
    SendAcknowledgement();
    SendSetPeerBandwidth();

    AmfObjects objects;
    amf_encoder_.Reset();
    amf_encoder_.EncodeString("_result", 7);
    amf_encoder_.EncodeNumber(amf_decoder_.GetNumber());
    objects["fmsVer"] = AmfObject(std::string("FMS/4,5,0,297"));
    objects["capabilities"] = AmfObject(255.0);
    objects["mode"] = AmfObject(1.0);
    amf_encoder_.EncodeObjects(objects);
    objects.clear();
    objects["level"] = AmfObject(std::string("status"));
    objects["code"] = AmfObject(std::string("NetConnection.Connect.Success"));
    objects["description"] = AmfObject(std::string("Connection succeeded"));
    objects["objectEncoding"] = AmfObject(0.0);
    amf_encoder_.EncodeObjects(objects);

    return SendInvokeMessage(RtmpMessage::CSID_INVOKE, amf_encoder_.Data(),
                             amf_encoder_.Size());
}

bool RtmpConnection::HandleCreateStream() {
    printf("================= HandleCreateStream =================\n");
    uint32_t stream_id = msg_codec_->StreamId();
    AmfObjects objects;
    stream_id_ = stream_id;
    amf_encoder_.Reset();
    amf_encoder_.EncodeString("_result", 7);
    amf_encoder_.EncodeNumber(amf_decoder_.GetNumber());
    amf_encoder_.EncodeObjects(objects); // 空对象
    amf_encoder_.EncodeNumber(stream_id);
    return SendInvokeMessage(RtmpMessage::CSID_INVOKE, amf_encoder_.Data(),
                             amf_encoder_.Size());
}

bool RtmpConnection::HandleDeleteStream() {
    printf("================= HandleDeleteStream =================\n");
    if (!stream_path_.empty()) {
        // TODO: 移除 session
    }
    is_playing_ = false;
    is_publishing_ = false;
    msg_codec_->Clear();
    return true;
}

bool RtmpConnection::HandlePublish() {
    printf("================= HandlePublish =================\n");
    AmfObjects objects;
    amf_encoder_.Reset();
    amf_encoder_.EncodeString("onStatus", 8);
    amf_encoder_.EncodeNumber(0.0);
    amf_encoder_.EncodeObjects(objects);

    bool is_error = false;
    if (0) {
        printf("HasPublisher\n");
        is_error = true;
        objects["level"] = AmfObject(std::string("error"));
        objects["code"] = AmfObject(std::string("NetStream.Publish.BadName"));
        objects["description"] = AmfObject(
            std::string("Stream already publishing."));
    } else if (state_ == State::START_PUBLISH) {
        printf("START_PUBLISH\n");
        is_error = true;
        objects["level"] = AmfObject(std::string("error"));
        objects["code"] = AmfObject(
            std::string("NetStream.Publish.BadConnection"));
        objects["description"] = AmfObject(
            std::string("Stream already publishing."));
    } else {
        objects["level"] = AmfObject(std::string("status"));
        objects["code"] = AmfObject(std::string("NetStream.Publish.Start"));
        objects["description"] = AmfObject(std::string("Start publishing."));

        // TODO: 添加 session
    }
    amf_encoder_.EncodeObjects(objects);
    if (!SendInvokeMessage(RtmpMessage::CSID_INVOKE, amf_encoder_.Data(),
                           amf_encoder_.Size())) {
        return false;
    }
    if (is_error) {
        return false;
    }

    state_ = State::START_PUBLISH;
    is_publishing_ = true;
    return true;
}

bool RtmpConnection::HandlePlay() {
    printf("================= HandlePlay =================\n");

    // 响应 Reset
    AmfObjects objects;
    amf_encoder_.Reset();
    amf_encoder_.EncodeString("onStatus", 8);
    amf_encoder_.EncodeNumber(0.0);
    objects["level"] = AmfObject(std::string("status"));
    objects["code"] = AmfObject(std::string("NetStream.Play.Reset"));
    objects["description"] = AmfObject(
        std::string("Resetting ond playing stream."));
    amf_encoder_.EncodeObjects(objects);
    if (!SendInvokeMessage(RtmpMessage::CSID_INVOKE, amf_encoder_.Data(),
                           amf_encoder_.Size())) {
        return false;
    }

    // 响应 Start
    objects.clear();
    amf_encoder_.Reset();
    amf_encoder_.EncodeString("onStatus", 8);
    amf_encoder_.EncodeNumber(0);
    objects["level"] = AmfObject(std::string("status"));
    objects["code"] = AmfObject(std::string("NetStream.Play.Start"));
    objects["description"] = AmfObject(std::string("Started playing."));
    amf_encoder_.EncodeObjects(objects);
    if (!SendInvokeMessage(RtmpMessage::CSID_INVOKE, amf_encoder_.Data(),
                           amf_encoder_.Size())) {
        return false;
    }

    // 响应权限
    objects.clear();
    amf_encoder_.Reset();
    amf_encoder_.EncodeString("|RtmpSampleAccess", 17);
    objects["audioSampleAccess"] = AmfObject(true);
    objects["videoSampleAccess"] = AmfObject(true);
    amf_encoder_.EncodeObjects(objects);
    if (!SendNotifyMessage(RtmpMessage::CSID_DATA, amf_encoder_.Data(),
                           amf_encoder_.Size())) {
        return false;
    }

    state_ = State::START_PLAY;

    // TODO: session 添加客户端

    return true;
}

bool RtmpConnection::SendRtmpChunks(uint32_t csid, RtmpMessage &rtmp_msg) {
    uint32_t payload_len = rtmp_msg.PayloadLen();
    uint32_t chunk_size = msg_codec_->OutChunkSize();
    if (payload_len == 0 || chunk_size == 0) {
        fprintf(stderr, "Invalid rtmp_msg\n");
        return false;
    }

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
        fprintf(stderr, "CreateChunks failed\n");
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
    return SendRtmpChunks(RtmpMessage::CSID_CONTROL, msg);
}

bool RtmpConnection::SendAcknowledgement() {
    constexpr size_t kPayloadSize = 4;
    std::unique_ptr<uint8_t[]> payload(new uint8_t[kPayloadSize]);
    ByteIO::_WriteUInt32BE(payload.get(), acknowledgement_size_);
    RtmpMessage msg(RtmpMessage::Type::ACKNOWLEDGEMENT, std::move(payload),
                    kPayloadSize);
    return SendRtmpChunks(RtmpMessage::CSID_CONTROL, msg);
}

bool RtmpConnection::SendSetChunkSize() {
    msg_codec_->SetOutChunkSize(max_chunk_size_);
    constexpr size_t kPayloadSize = 4;
    std::unique_ptr<uint8_t[]> payload(new uint8_t[kPayloadSize]);
    ByteIO::_WriteUInt32BE(payload.get(), max_chunk_size_);
    RtmpMessage msg(RtmpMessage::Type::SET_CHUNK_SIZE, std::move(payload),
                    kPayloadSize);
    return SendRtmpChunks(RtmpMessage::CSID_CONTROL, msg);
}

bool RtmpConnection::SendInvokeMessage(uint32_t csid,
                                       std::unique_ptr<uint8_t[]> &&payload,
                                       uint32_t payload_size) {
    if (payload_size == 0 || IsClosed()) {
        fprintf(stderr, "Invalid payload_size or closed\n");
        return false;
    }
    RtmpMessage msg(RtmpMessage::Type::INVOKE, std::move(payload),
                    payload_size, stream_id_);
    return SendRtmpChunks(csid, msg);
}

bool RtmpConnection::SendNotifyMessage(uint32_t csid,
                                       std::unique_ptr<uint8_t[]> &&payload,
                                       uint32_t payload_size) {
    if (payload_size == 0 || IsClosed()) {
        fprintf(stderr, "Invalid payload_size or closed\n");
        return false;
    }
    RtmpMessage msg(RtmpMessage::Type::NOTIFY, std::move(payload),
                    payload_size, stream_id_);
    return SendRtmpChunks(csid, msg);
}

bool RtmpConnection::SendMetaData(const AmfObjects &meta_data) {
    if (meta_data.empty() || IsClosed()) {
        fprintf(stderr, "Empty metadata or closed\n");
        return false;
    }
    amf_encoder_.Reset();
    amf_encoder_.EncodeString("onMetaData", 10);
    amf_encoder_.EncodeECMA(meta_data_);
    if (!SendNotifyMessage(RtmpMessage::CSID_DATA, amf_encoder_.Data(),
                           amf_encoder_.Size())) {
        return false;
    }
    return true;
}

bool RtmpConnection::SendMediaData(uint8_t type, uint64_t timestamp,
                                   std::shared_ptr<uint8_t[]> payload,
                                   uint32_t payload_size) {
    if (payload_size == 0 || IsClosed()) {
        fprintf(stderr, "Invalid payload_size or closed\n");
        return false;
    }
    is_playing_ = true;

    if (type == MediaDataType::AVC_SEQUENCE_HEADER) {
        avc_sequence_header_ = payload;
        avc_sequence_header_size_ = payload_size;
    } else if (type == MediaDataType::AAC_SEQUENCE_HEADER) {
        aac_sequence_header_ = payload;
        aac_sequence_header_size_ = payload_size;
    }

    // 确保发送的首个视频帧是I帧
    std::unique_ptr<uint8_t[]> buffer(new uint8_t[payload_size]);
    memcpy(buffer.get(), payload.get(), payload_size);
    if (!has_key_frame_ && avc_sequence_header_size_ > 0 &&
        type == MediaDataType::AVC_VIDEO) {
        if (IsAvcKeyFrame(buffer, payload_size)) {
            has_key_frame_ = true;
        } else {
            return true;
        }
    }

    uint8_t msg_type = 0;
    if (type == MediaDataType::AAC_AUDIO ||
        type == MediaDataType::AAC_SEQUENCE_HEADER) {
        msg_type = RtmpMessage::Type::AUDIO;
        RtmpMessage msg(msg_type, std::move(buffer), payload_size, stream_id_);
        return SendRtmpChunks(RtmpMessage::CSID_AUDIO, msg);
    } else if (type == MediaDataType::AVC_VIDEO ||
               type == MediaDataType::AVC_SEQUENCE_HEADER) {
        msg_type = RtmpMessage::Type::VIDEO;
        RtmpMessage msg(msg_type, std::move(buffer), payload_size, stream_id_);
        return SendRtmpChunks(RtmpMessage::CSID_VIDEO, msg);
    } else {
        fprintf(stderr, "Invalid media type\n");
        return false;
    }
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

bool RtmpConnection::PayloadDecode(const char *payload, size_t payload_len,
                                   size_t &offset, int count) {
    if (count <= 0) {
        int ret = amf_decoder_.Decode(payload + offset,
                                      payload_len - offset, -1);
        if (ret <= 0) {
            fprintf(stderr, "AMF Decode error\n");
            return false;
        } else {
            offset += ret;
            return true;
        }
    }
    for (size_t i = 0; i < count; ++i) {
        if (!PayloadDecodeOne(payload, payload_len, offset)) {
            return false;
        }
    }
    return true;
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

bool RtmpConnection::IsAvcKeyFrame(std::unique_ptr<uint8_t[]> &payload,
                                   uint32_t payload_size) {
    if (payload_size < 1) {
        return false;
    }
    uint8_t frame_type = payload.get()[0] >> 4;
    uint8_t codec_id = payload.get()[0] & 0x0f;
    return (frame_type == Flv::FRAME_TYPE_I && codec_id == Flv::CODEC_ID_AVC);
}

} // lsy::net::rtmp
