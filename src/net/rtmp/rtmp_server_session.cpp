#include "rtmp_server_session.hpp"
#include "flv_pub.hpp"
#include "rtmp_control_handler.hpp"

rtmp_server_session::rtmp_server_session(boost::asio::ip::tcp::socket socket, rtmp_server_callbackI* callback, std::string session_key) : session_key_(session_key)
    , callback_(callback)
    , hs_(this)
    , ctrl_handler_(this) {
    session_ptr_ = std::make_shared<tcp_session>(std::move(socket), this);
    try_read();
}

rtmp_server_session::~rtmp_server_session() {
    log_infof("rtmp session destruct:%s, publisher:%d",
        req_.key_.c_str(), req_.publish_flag_);
    close();
}

std::string rtmp_server_session::get_sesson_key() {
    return session_key_;
}

void rtmp_server_session::try_read() {
    session_ptr_->async_read();
}

data_buffer* rtmp_server_session::get_recv_buffer() {
    return &recv_buffer_;
}

int rtmp_server_session::rtmp_send(char* data, int len) {
    keep_alive();
    session_ptr_->async_write(data, len);
    return 0;
}

int rtmp_server_session::rtmp_send(std::shared_ptr<data_buffer> data_ptr) {
    keep_alive();
    session_ptr_->async_write(data_ptr);
    return 0;
}

void rtmp_server_session::close() {
    if (closed_flag_) {
        return;
    }
    closed_flag_ = true;
    log_infof("rtmp session close....");
    if (req_.is_ready_ && !req_.publish_flag_) {
        if (play_writer_) {
            media_stream_manager::remove_player(play_writer_);
            delete play_writer_;
        }
    } else {
        if (!req_.key_.empty()) {
            media_stream_manager::remove_publisher(req_.key_);
        }
    }

    callback_->on_close(session_key_);
    session_ptr_->close();
}

void rtmp_server_session::on_write(int ret_code, size_t sent_size) {
    if ((ret_code != 0) || (sent_size == 0)) {
        log_errorf("write callback code:%d, sent size:%lu", ret_code, sent_size);
        close();
        return;
    }
}

void rtmp_server_session::on_read(int ret_code, const char* data, size_t data_size) {
    if ((ret_code != 0) || (data == nullptr) || (data_size == 0)){
        log_errorf("read callback code:%d, sent size:%lu", ret_code, data_size);
        close();
        return;
    }

    recv_buffer_.append_data(data, data_size);
    int ret = handle_request();
    if (ret < 0) {
        close();
    } else if (ret == RTMP_NEED_READ_MORE) {
        try_read();
    } else {
        //need do nothing
    }
}

int rtmp_server_session::send_rtmp_ack(uint32_t size) {
    return ctrl_handler_.send_rtmp_ack(size);
}

int rtmp_server_session::receive_chunk_stream() {
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

        //check whether we need to send rtmp control ack
        (void)send_rtmp_ack(cs_ptr->chunk_data_ptr_->data_len());

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
            ret = ctrl_handler_.handle_client_command_message(cs_ptr, amf_vec);
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

            if (req_.is_ready_ && !req_.publish_flag_) {
                //rtmp play is ready.
                play_writer_ = new rtmp_writer(this);
                media_stream_manager::add_player(play_writer_);
            }
            if (recv_buffer_.data_len() > 0) {
                continue;
            }
            break;
        } else if (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_AMF3) {
            //TODO: support amf3
            log_warnf("does not support amf3");
            return -1;
        } else if ((cs_ptr->type_id_ == RTMP_MEDIA_PACKET_VIDEO) || (cs_ptr->type_id_ == RTMP_MEDIA_PACKET_AUDIO)
                || (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA0) || (cs_ptr->type_id_ == RTMP_COMMAND_MESSAGES_META_DATA3)) {
            MEDIA_PACKET_PTR pkt_ptr = get_media_packet(cs_ptr);
            if (pkt_ptr->buffer_ptr_->data_len() == 0) {
                return -1;
            }

            keep_alive();
            //handle video/audio
            media_stream_manager::writer_media_packet(pkt_ptr);

            cs_ptr->reset();
            if (recv_buffer_.data_len() > 0) {
                continue;
            }
        } else {
            log_warnf("unkown chunk type id:%d", cs_ptr->type_id_);
            cs_ptr->reset();
            if (recv_buffer_.data_len() > 0) {
                continue;
            }
        }
        break;
    }

    return ret;
}

int rtmp_server_session::handle_request() {
    int ret = -1;

    if (server_phase_ == initial_phase) {
        ret = hs_.handle_c0c1();
        if ((ret < 0) || (ret == RTMP_NEED_READ_MORE)) {
            return ret;
        } else if (ret == RTMP_SIMPLE_HANDSHAKE) {
            log_infof("try to rtmp handshake in simple mode");
        }
        recv_buffer_.reset();//be ready to receive c2;
        log_infof("rtmp session phase become c0c1.");
        ret = hs_.send_s0s1s2();
        server_phase_ = handshake_c2_phase;
        return ret;
    } else if (server_phase_ == handshake_c2_phase) {
        log_infof("start handle c2...");
        ret = hs_.handle_c2();
        if ((ret < 0) || (ret == RTMP_NEED_READ_MORE)){
            return ret;
        }

        log_infof("rtmp session phase become rtmp connect, buffer len:%lu", recv_buffer_.data_len());
        server_phase_ = connect_phase;
        if (recv_buffer_.data_len() == 0) {
            return RTMP_NEED_READ_MORE;
        } else {
            ret = receive_chunk_stream();
            if ((ret < 0) || (ret == RTMP_NEED_READ_MORE)) {
                return ret;
            }
        }
    } else if (server_phase_ >= connect_phase) {
        ret = receive_chunk_stream();
        if (ret < 0) {
            return ret;
        }

        if (ret == RTMP_OK) {
            ret = RTMP_NEED_READ_MORE;
        }
    }

    return ret;
}