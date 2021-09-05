#include "rtmp_client_session.hpp"
#include "rtmp_control_handler.hpp"
#include "rtmp_pub.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "amf/amf0.hpp"

rtmp_client_session::rtmp_client_session(boost::asio::io_context& io_context, rtmp_client_callbackI* callback):conn_(io_context, this)
    , cb_(callback)
    , hs_(this)
    , ctrl_handler_(this)
{

}

rtmp_client_session::~rtmp_client_session() {

}

int rtmp_client_session::start(const std::string& url, bool is_publish) {
    int ret = 0;

    ret = get_rtmp_url_info(url, host_, port_, req_.tcurl_, req_.app_, req_.stream_name_);
    if (ret != 0) {
        log_errorf("fail to get rtmp url:%s, return:%d", url.c_str(), ret);
        return ret;
    }
    req_.key_ = req_.app_;
    req_.key_ += "/";
    req_.key_ += req_.stream_name_;

    req_.publish_flag_ = is_publish;

    log_infof("get rtmp info host:%s, port:%d, tcurl:%s, app:%s, streamname:%s, key:%s, method:%s",
        host_.c_str(), port_, req_.tcurl_.c_str(), req_.app_.c_str(), req_.stream_name_.c_str(),
        req_.key_.c_str(), is_publish_desc());
    
    conn_.connect(host_, port_);

    return 0;
}

int rtmp_client_session::rtmp_write(MEDIA_PACKET_PTR pkt_ptr) {
    uint16_t csid;
    uint8_t  type_id;

    if (!conn_.is_connect()) {
        log_infof("rtmp tcp connect is closed...");
        return -1;
    }
    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        csid = 6;
        type_id = RTMP_MEDIA_PACKET_VIDEO;
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        csid = 4;
        type_id = RTMP_MEDIA_PACKET_AUDIO;
    } else if (pkt_ptr->av_type_ == MEDIA_METADATA_TYPE) {
        csid = 6;
        type_id = pkt_ptr->typeid_;
    } else {
        log_errorf("doesn't support av type:%d", (int)pkt_ptr->av_type_);
        return -1;
    }
    write_data_by_chunk_stream(this, csid,
                    pkt_ptr->dts_, type_id,
                    pkt_ptr->streamid_, get_chunk_size(),
                    pkt_ptr->buffer_ptr_);
    return RTMP_OK;
}

int rtmp_client_session::rtmp_send(char* data, int len) {
    conn_.send(data, len);
    return 0;
}

int rtmp_client_session::rtmp_send(std::shared_ptr<data_buffer> data_ptr) {
    conn_.send(data_ptr->data(), data_ptr->data_len());
    return 0;
}

data_buffer* rtmp_client_session::get_recv_buffer() {
    return &recv_buffer_;
}

void rtmp_client_session::close() {
    conn_.close();
}

bool rtmp_client_session::is_ready() {
    return client_phase_ == client_media_handle_phase;
}

void rtmp_client_session::try_read() {
    conn_.async_read();
    return;
}

void rtmp_client_session::on_connect(int ret_code) {
    if (ret_code != 0) {
        log_errorf("rtmp tcp connect error:%d", ret_code);
        close();
        return;
    }

    client_phase_ = client_c0c1_phase;
    recv_buffer_.reset();
    (void)hs_.send_c0c1();
    try_read();
}

void rtmp_client_session::on_write(int ret_code, size_t sent_size) {
    if (ret_code != 0) {
        log_errorf("rtmp write error:%d", ret_code);
        close();
        return;
    }
}

void rtmp_client_session::on_read(int ret_code, const char* data, size_t data_size) {
    if (ret_code != 0) {
        log_errorf("rtmp on read error:%d", ret_code);
        close();
        return;
    }

    recv_buffer_.append_data(data, data_size);

    (void)handle_message();
}

int rtmp_client_session::handle_message() {
    int ret = 0;

    if (client_phase_ == client_c0c1_phase) {
        if (!recv_buffer_.require(rtmp_client_handshake::s0s1s2_size)) {
            try_read();
            return RTMP_NEED_READ_MORE;
        }
        uint8_t* p = (uint8_t*)recv_buffer_.data();
        uint8_t* c2 = p + 1;
        size_t c2_len = 1536;
        uint8_t ver = read_4bytes(c2 + 4);
        log_debugf("rtmp server version:%u", ver);
        
        client_phase_ = client_connect_phase;
        conn_.send((char*)c2, c2_len);

        //send rtmp connect
        ret = rtmp_connect();
        if (ret < 0) {
            log_errorf("rtmp connect error:%d", ret);
            close();
            return -1;
        }
        client_phase_ = client_connect_resp_phase;
        recv_buffer_.reset();
        try_read();
        return RTMP_NEED_READ_MORE;
    }
    
    if (client_phase_ == client_connect_resp_phase) {
        ret = receive_resp_message();
        if (ret < 0) {
            log_errorf("rtmp connect resp error:%d", ret);
            close();
            return -1;
        } else if (ret == RTMP_NEED_READ_MORE) {
            return RTMP_NEED_READ_MORE;
        } else if (ret == 0) {
            if (client_phase_ == client_connect_resp_phase) {
                try_read();
            }
        } else {
            log_errorf("rtmp connect resp unkown return:%d", ret);
            close();
            return -1;
        }
    }
    
    if (client_phase_ == client_create_stream_phase) {
        //send create stream
        log_infof("client send create stream message...");
        ret = rtmp_createstream();
        if (ret < 0) {
            log_errorf("rtmp create stream error:%d", ret);
            close();
            return ret;
        }
        recv_buffer_.reset();
        log_infof("client phase change [%s] to [%s]",
            get_client_phase_desc(client_phase_),
            get_client_phase_desc(client_create_stream_resp_phase));
        client_phase_ = client_create_stream_resp_phase;
        try_read();
        return RTMP_NEED_READ_MORE;
    }

    if (client_phase_ == client_create_stream_resp_phase) {
        log_infof("client phase create_stream_resp receiving messages...");
        ret = receive_resp_message();
        if (ret < 0) {
            log_errorf("rtmp receive resp message error:%d", ret);
            close();
            return ret;
        } else if (ret == RTMP_NEED_READ_MORE) {
            try_read();
            return RTMP_NEED_READ_MORE;
        } else if (ret == 0) {
            if (req_.publish_flag_) {
                client_phase_ = client_create_publish_phase;
            } else {
                client_phase_ = client_create_play_phase;
            }
            recv_buffer_.reset();
        } else {
            log_errorf("rtmp connect unkown return:%d", ret);
            close();
            return -1;
        }
    }

    if (client_phase_ == client_create_play_phase) {
        ret = rtmp_play();
        if (ret < 0) {
            log_errorf("rtmp play error:%d", ret);
            close();
            return ret;
        }
        recv_buffer_.reset();
        try_read();
        client_phase_ = client_media_handle_phase;
        return 0;
    }

    if (client_phase_ == client_create_publish_phase) {
        ret = rtmp_publish();
        if (ret < 0) {
            log_errorf("rtmp publish error:%d", ret);
            close();
            return ret;
        }
        recv_buffer_.reset();
        try_read();
        client_phase_ = client_media_handle_phase;
        log_infof("client publish send done.");
        return 0;
    }

    if (client_phase_ == client_media_handle_phase) {
        log_debugf("rtmp client start media %s", is_publish_desc());
        ret = receive_resp_message();
        if (ret < 0) {
            close();
            return ret;
        }
        try_read();
    }
    return ret;
}

int rtmp_client_session::receive_resp_message() {
    CHUNK_STREAM_PTR cs_ptr;
    int ret = -1;

    while(true) {
        //receive fmt+csid | basic header | message header | data
        ret = read_chunk_stream(cs_ptr);
        if ((ret < RTMP_OK) || (ret == RTMP_NEED_READ_MORE)) {
            return ret;
        }

        //check whether chunk stream is ready(data is full)
        if (!cs_ptr || !cs_ptr->is_ready()) {
            if (recv_buffer_.data_len() > 0) {
                continue;
            }
            return RTMP_NEED_READ_MORE;
        }

        if ((cs_ptr->type_id_ >= RTMP_CONTROL_SET_CHUNK_SIZE) && (cs_ptr->type_id_ <= RTMP_CONTROL_SET_PEER_BANDWIDTH)) {
            ret = ctrl_handler_.handle_rtmp_control_message(cs_ptr);
            if (ret < RTMP_OK) {
                return ret;
            }
            cs_ptr->reset();
            if (recv_buffer_.data_len() > 0) {
                continue;
            }
            break;
        } else if (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_AMF0) {
            std::vector<AMF_ITERM*> amf_vec;
            ret = ctrl_handler_.handle_server_command_message(cs_ptr, amf_vec);
            if (ret < RTMP_OK) {
                for (auto iter : amf_vec) {
                    AMF_ITERM* temp = iter;
                    delete temp;
                }
                return ret;
            }
            for (auto iter : amf_vec) {
                AMF_ITERM* temp = iter;
                delete temp;
            }
            cs_ptr->reset();
            if (recv_buffer_.data_len() > 0) {
                continue;
            }
            break;
        }  else if ((cs_ptr->type_id_ == RTMP_MEDIA_PACKET_VIDEO) || (cs_ptr->type_id_ == RTMP_MEDIA_PACKET_AUDIO)
                || (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA0) || (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA3)) {
            MEDIA_PACKET_PTR pkt_ptr = get_media_packet(cs_ptr);
            if (pkt_ptr->buffer_ptr_->data_len() == 0) {
                return -1;
            }

            //handle video/audio
            //TODO: send media data to client callback....
            if (cb_) {
                cb_->on_message(RTMP_OK, pkt_ptr);
            }

            cs_ptr->reset();
            if (recv_buffer_.data_len() > 0) {
                continue;
            }
            ret = RTMP_NEED_READ_MORE;
        } else {
            log_warnf("rtmp client chunk typeid:%d is not supported.", cs_ptr->type_id_);
        }
    }
    return ret;
}

int rtmp_client_session::rtmp_connect() {
    data_buffer amf_buffer;

    AMF_Encoder::encode(std::string("connect"), amf_buffer);
    double transid = (double)req_.transaction_id_;
    AMF_Encoder::encode(transid, amf_buffer);

    std::unordered_map<std::string, AMF_ITERM*> event_amf_obj;
    AMF_ITERM* app_item = new AMF_ITERM();
    app_item->set_amf_type(AMF_DATA_TYPE_STRING);
    app_item->desc_str_ = req_.app_;
    event_amf_obj.insert(std::make_pair("app", app_item));
    log_debugf("rtmp connect app:%s", req_.app_.c_str());

    AMF_ITERM* type_item = new AMF_ITERM();
    type_item->set_amf_type(AMF_DATA_TYPE_STRING);
    type_item->desc_str_ = "nonprivate";
    event_amf_obj.insert(std::make_pair("type", type_item));

    AMF_ITERM* ver_item = new AMF_ITERM();
    ver_item->set_amf_type(AMF_DATA_TYPE_STRING);
    ver_item->desc_str_ = "FMS.3.1";
    event_amf_obj.insert(std::make_pair("flashVer", ver_item));

    AMF_ITERM* tcurl_item = new AMF_ITERM();
    tcurl_item->set_amf_type(AMF_DATA_TYPE_STRING);
    tcurl_item->desc_str_ = req_.tcurl_;
    event_amf_obj.insert(std::make_pair("tcUrl", tcurl_item));
    log_debugf("rtmp connect tcurl:%s", req_.tcurl_.c_str());

    AMF_Encoder::encode(event_amf_obj, amf_buffer);

    delete app_item;
    delete type_item;
    delete ver_item;
    delete tcurl_item;

    log_debugf("rtmp connect start chunk_size:%u", get_chunk_size());

    uint32_t stream_id = 0;
    int ret = write_data_by_chunk_stream(this, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return ret;
}

int rtmp_client_session::rtmp_createstream() {
    data_buffer amf_buffer;

    AMF_Encoder::encode(std::string("createStream"), amf_buffer);
    double transid = (double)req_.transaction_id_;
    AMF_Encoder::encode(transid, amf_buffer);
    AMF_Encoder::encode_null(amf_buffer);

    uint32_t stream_id = 0;
    int ret = write_data_by_chunk_stream(this, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return ret;
}

int rtmp_client_session::rtmp_play() {
    data_buffer amf_buffer;

    AMF_Encoder::encode(std::string("play"), amf_buffer);
    double transid = 0.0;
    AMF_Encoder::encode(transid, amf_buffer);
    AMF_Encoder::encode_null(amf_buffer);
    AMF_Encoder::encode(req_.stream_name_, amf_buffer);

    uint32_t stream_id = 0;
    int ret = write_data_by_chunk_stream(this, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return ret;
}

int rtmp_client_session::rtmp_publish() {
    data_buffer amf_buffer;

    AMF_Encoder::encode(std::string("publish"), amf_buffer);
    double transid = 0.0;
    AMF_Encoder::encode(transid, amf_buffer);
    AMF_Encoder::encode_null(amf_buffer);
    AMF_Encoder::encode(req_.stream_name_, amf_buffer);
    AMF_Encoder::encode(std::string("live"), amf_buffer);

    uint32_t stream_id = 0;
    int ret = write_data_by_chunk_stream(this, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return ret;
}