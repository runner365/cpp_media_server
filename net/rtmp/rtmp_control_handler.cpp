#include "rtmp_control_handler.hpp"
#include "rtmp_server.hpp"
#include "data_buffer.hpp"

uint32_t g_config_chunk_size = 4096;

rtmp_control_handler::rtmp_control_handler(rtmp_session_base* session):session_(session)
{
}

rtmp_control_handler::~rtmp_control_handler()
{
}

int rtmp_control_handler::handle_server_command_message(CHUNK_STREAM_PTR cs_ptr, std::vector<AMF_ITERM*>& amf_vec) {
    uint8_t* data = (uint8_t*)cs_ptr->chunk_data_ptr_->data();
    int len = (int)cs_ptr->chunk_data_ptr_->data_len();

    while (len > 0) {
        AMF_ITERM* amf_item = new AMF_ITERM();
        AMF_Decoder::decode(data, len, *amf_item);

        amf_vec.push_back(amf_item);
    }

    if (amf_vec.size() < 1) {
        log_errorf("amf vector count error:%lu", amf_vec.size());
        return -1;
    }

    RTMP_CLIENT_SESSION_PHASE next_phase = session_->client_phase_;

    for (auto item : amf_vec) {
        if (item->get_amf_type() == AMF_DATA_TYPE_STRING) {
            if ((session_->client_phase_ == client_connect_phase) ||
                (session_->client_phase_ == client_connect_resp_phase) ||
                (session_->client_phase_ == client_create_stream_phase) ||
                (session_->client_phase_ == client_create_stream_resp_phase)) {
                if ((item->desc_str_ != "_result") && (item->desc_str_ != "onBWDone")) {
                    log_errorf("rtmp client connect return %s", item->desc_str_.c_str());
                    return -1;
                }
                if ((session_->client_phase_ == client_connect_phase) || 
                    (session_->client_phase_ == client_connect_resp_phase)) {
                    if (item->desc_str_ == "_result") {
                        //log_infof("rtmp client change connect to create stream.");
                        next_phase = client_create_stream_phase;
                    }
                }
                if ((session_->client_phase_ == client_create_stream_phase) ||
                    (session_->client_phase_ == client_create_stream_resp_phase)){
                    if (item->desc_str_ == "_result") {
                        log_debugf("rtmp client change create stream to %s", session_->is_publish_desc());
                        if (session_->is_publish()) {
                            next_phase = client_create_publish_phase;
                        } else {
                            next_phase = client_create_play_phase;
                        }
                    }
                }
            } else if ((session_->client_phase_ == client_create_publish_phase) ||
                    (session_->client_phase_ == client_create_play_phase)) {
                if ((item->desc_str_ != "_result") && (item->desc_str_ != "onStatus")) {
                    log_errorf("rtmp client %s return %s", session_->is_publish_desc(), item->desc_str_.c_str());
                    return -1;
                }
            }
        } else if (item->get_amf_type() == AMF_DATA_TYPE_NUMBER) {
            log_debugf("rtmp client phase:[%s], amf number:%f",
                get_client_phase_desc(session_->client_phase_), item->number_);
        } else if (item->get_amf_type() == AMF_DATA_TYPE_OBJECT) {
            log_debugf("rtmp client phase:[%s], amf object", get_client_phase_desc(session_->client_phase_));
            auto iter = item->amf_obj_.find("code");
            if (iter != item->amf_obj_.end()) {
                AMF_ITERM* obj_item = iter->second;
                if (obj_item->get_amf_type() == AMF_DATA_TYPE_STRING) {
                    log_debugf("client phase[%s] %s", get_client_phase_desc(session_->client_phase_),
                            obj_item->desc_str_.c_str());
                    if (session_->client_phase_ == client_connect_phase) {
                        if (obj_item->desc_str_ != "NetConnection.Connect.Success") {
                            log_errorf("rtmp client connect return %s", obj_item->desc_str_.c_str());
                            return -1;
                        }
                    } else if ((session_->client_phase_ == client_create_publish_phase) ||
                            (session_->client_phase_ == client_create_play_phase)) {
                        if ((obj_item->desc_str_ != "NetStream.Publish.Start") && (obj_item->desc_str_ != "NetStream.Play.Start")) {
                            log_errorf("rtmp client [%s] return %s",get_client_phase_desc(session_->client_phase_),
                                obj_item->desc_str_.c_str());
                            return -1;
                        }
                    }   
                }
            }
        }
    }

    if (session_->client_phase_ != next_phase) {
        //log_infof("rtmp client session phase change [%s] to [%s]",
        //        get_client_phase_desc(session_->client_phase_), get_client_phase_desc(next_phase));
        session_->client_phase_ = next_phase;
    }
    return 0;
}

int rtmp_control_handler::handle_client_command_message(CHUNK_STREAM_PTR cs_ptr, std::vector<AMF_ITERM*>& amf_vec) {
    int ret = 0;
    uint8_t* data = (uint8_t*)cs_ptr->chunk_data_ptr_->data();
    int len = (int)cs_ptr->chunk_data_ptr_->data_len();

    while (len > 0) {
        AMF_ITERM* amf_item = new AMF_ITERM();
        AMF_Decoder::decode(data, len, *amf_item);

        amf_vec.push_back(amf_item);
    }

    if (amf_vec.size() < 1) {
        log_errorf("amf vector count error:%lu", amf_vec.size());
        return -1;
    }

    AMF_ITERM* item = amf_vec[0];
    std::string cmd_type;

    if (item->get_amf_type() != AMF_DATA_TYPE_STRING) {
        log_errorf("first amf type error:%d", (int)item->get_amf_type());
        return -1;
    }

    cmd_type = item->desc_str_;

    if (cmd_type == CMD_Connect) {
        ret = handle_rtmp_connect_command(cs_ptr->msg_stream_id_, amf_vec);
    } else if (cmd_type == CMD_CreateStream) {
        ret = handle_rtmp_createstream_command(cs_ptr->msg_stream_id_, amf_vec);
    } else if (cmd_type == CMD_Publish) {
        ret = handle_rtmp_publish_command(cs_ptr->msg_stream_id_, amf_vec);
    } else if (cmd_type == CMD_Play) {
        ret = handle_rtmp_play_command(cs_ptr->msg_stream_id_, amf_vec);
    }

    if (ret == RTMP_OK) {
        if (session_->req_.is_ready_ && !session_->req_.publish_flag_) {
            return ret;
        }
        ret = RTMP_NEED_READ_MORE;
    }
    return ret;
}

int rtmp_control_handler::handle_rtmp_connect_command(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec) {
    if (amf_vec.size() < 3) {
        log_errorf("rtmp connect amf vector count error:%lu", amf_vec.size());
        return -1;
    }

    double transactionId = 0;
    for (int index = 1; index < (int)amf_vec.size(); index++) {
        AMF_ITERM* item = amf_vec[index];
        switch (item->get_amf_type())
        {
            case AMF_DATA_TYPE_NUMBER:
            {
                transactionId = item->number_;
                log_debugf("rtmp transactionId:%f", transactionId);
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                std::unordered_map<std::string, AMF_ITERM*>::iterator iter = item->amf_obj_.find("app");
                if (iter != item->amf_obj_.end()) {
                    AMF_ITERM* item = iter->second;
                    if (item->get_amf_type() != AMF_DATA_TYPE_STRING) {
                        log_errorf("app type is not string:%d", (int)item->get_amf_type());
                        return -1;
                    }
                    session_->req_.app_ = item->desc_str_;
                }
                iter = item->amf_obj_.find("tcUrl");
                if (iter != item->amf_obj_.end()) {
                    AMF_ITERM* item = iter->second;
                    if (item->get_amf_type() != AMF_DATA_TYPE_STRING) {
                        log_errorf("tcUrl type is not string:%d", (int)item->get_amf_type());
                        return -1;
                    }
                    session_->req_.tcurl_ = item->desc_str_;
                }
                iter = item->amf_obj_.find("flashVer");
                if (iter != item->amf_obj_.end()) {
                    AMF_ITERM* item = iter->second;
                    if (item->get_amf_type() != AMF_DATA_TYPE_STRING) {
                        log_errorf("flash ver type is not string:%d", (int)item->get_amf_type());
                        return -1;
                    }
                    session_->req_.flash_ver_ = item->desc_str_;
                }

                break;
            }
            default:
                break;
        }
    }
    return send_rtmp_connect_resp(session_->stream_id_);
}

int rtmp_control_handler::handle_rtmp_createstream_command(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec) {
    if (amf_vec.size() < 3) {
        log_errorf("rtmp create stream amf vector count error:%lu", amf_vec.size());
        return -1;
    }
    double transactionId = 0;
    
    session_->req_.stream_id_ = stream_id;
    for (int index = 1; index < (int)amf_vec.size(); index++) {
        AMF_ITERM* item = amf_vec[index];
        switch (item->get_amf_type())
        {
            case AMF_DATA_TYPE_NUMBER:
            {
                //log_infof("rtmp create stream transaction id:%f", item->number_);
                transactionId = item->number_;
                session_->req_.transaction_id_ = (int64_t)transactionId;
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                //log_infof("rtmp create stream object:");
                item->dump_amf();
                break;
            }
            default:
                break;
        }
    }
    return send_rtmp_create_stream_resp(transactionId);
}

int rtmp_control_handler::handle_rtmp_play_command(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec) {
    if (amf_vec.size() < 3) {
        log_errorf("rtmp play amf vector count error:%lu", amf_vec.size());
        return -1;
    }
    double transactionId = 0;
    std::string stream_name;
    for (int index = 1; index < (int)amf_vec.size(); index++) {
        AMF_ITERM* item = amf_vec[index];
        switch (item->get_amf_type())
        {
            case AMF_DATA_TYPE_NUMBER:
            {
                //log_infof("rtmp play transaction id:%f", item->number_);
                transactionId = item->number_;
                session_->req_.transaction_id_ = (int64_t)transactionId;
                break;
            }
            case AMF_DATA_TYPE_STRING:
            {
                //log_infof("rtmp play string:%s", item->desc_str_.c_str());
                if (stream_name.empty()) {
                    stream_name = item->desc_str_;
                }
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                //log_infof("rtmp play object:");
                item->dump_amf();
                break;
            }
            default:
                break;
        }
    }
    session_->req_.stream_name_  = stream_name;
    session_->req_.publish_flag_ = false;
    session_->req_.is_ready_     = true;
    session_->req_.key_ = session_->req_.app_;
    session_->req_.key_ += "/";
    session_->req_.key_ += stream_name;
    
    //log_infof("rtmp play is ready, key:%s", session_->req_.key_.c_str());
    return send_rtmp_play_resp();
}

int rtmp_control_handler::handle_rtmp_publish_command(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec) {
    if (amf_vec.size() < 3) {
        log_errorf("rtmp publish amf vector count error:%lu", amf_vec.size());
        return -1;
    }
    double transactionId = 0;
    std::string stream_name;
    for (int index = 1; index < (int)amf_vec.size(); index++) {
        AMF_ITERM* item = amf_vec[index];
        switch (item->get_amf_type())
        {
            case AMF_DATA_TYPE_NUMBER:
            {
                //log_infof("rtmp publish transaction id:%f", item->number_);
                transactionId = item->number_;
                session_->req_.transaction_id_ = (int64_t)transactionId;
                break;
            }
            case AMF_DATA_TYPE_STRING:
            {
                if (stream_name.empty()) {
                    stream_name = item->desc_str_;
                }
                break;
            }
            case AMF_DATA_TYPE_OBJECT:
            {
                //log_infof("rtmp publish object:");
                item->dump_amf();
                break;
            }
            default:
                break;
        }
    }
    session_->req_.stream_name_  = stream_name;
    session_->req_.publish_flag_ = true;
    session_->req_.is_ready_     = true;
    session_->req_.key_ = session_->req_.app_;
    session_->req_.key_ += "/";
    session_->req_.key_ += stream_name;
    return send_rtmp_publish_resp();
}

int rtmp_control_handler::send_set_chunksize(uint32_t chunk_size) {
    chunk_stream set_chunk_size_cs(session_, 0, 3, session_->get_chunk_size());
    set_chunk_size_cs.gen_control_message(RTMP_CONTROL_SET_CHUNK_SIZE, 4, chunk_size);

    //log_infof("send ctrl message chunk size:%u", chunk_size);
    session_->rtmp_send(set_chunk_size_cs.chunk_all_ptr_);
    return 0;
}

int rtmp_control_handler::send_rtmp_connect_resp(uint32_t stream_id) {
    data_buffer amf_buffer;
    chunk_stream win_size_cs(session_, 0, 3, session_->get_chunk_size());
    chunk_stream peer_bw_cs(session_, 0, 3, session_->get_chunk_size());
    chunk_stream set_chunk_size_cs(session_, 0, 3, session_->get_chunk_size());

    win_size_cs.gen_control_message(RTMP_CONTROL_WINDOW_ACK_SIZE, 4, 2500000);
    peer_bw_cs.gen_control_message(RTMP_CONTROL_SET_PEER_BANDWIDTH, 5, 2500000);
    set_chunk_size_cs.gen_control_message(RTMP_CONTROL_SET_CHUNK_SIZE, 4, g_config_chunk_size);
    session_->set_chunk_size(g_config_chunk_size);

    session_->rtmp_send(win_size_cs.chunk_all_ptr_);
    //log_infof("rtmp send windows size");
    session_->rtmp_send(peer_bw_cs.chunk_all_ptr_);
    //log_infof("rtmp send peer bandwidth");
    session_->rtmp_send(set_chunk_size_cs.chunk_all_ptr_);
    //log_infof("rtmp send set chunk size");

    //encode resp amf
    std::unordered_map<std::string, AMF_ITERM*> resp_amf_obj;
    AMF_ITERM* fms_ver_item = new AMF_ITERM();
    fms_ver_item->set_amf_type(AMF_DATA_TYPE_STRING);
    fms_ver_item->desc_str_ = "FMS/3,0,1,123";
    resp_amf_obj.insert(std::make_pair("fmsVer", fms_ver_item));

    AMF_ITERM* capability_item = new AMF_ITERM();
    capability_item->set_amf_type(AMF_DATA_TYPE_NUMBER);
    capability_item->number_ = 31.0;
    resp_amf_obj.insert(std::make_pair("capabilities", capability_item));
    std::unordered_map<std::string, AMF_ITERM*> event_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->set_amf_type(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    event_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->set_amf_type(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetConnection.Connect.Success";
    event_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->set_amf_type(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Connection succeeded.";
    event_amf_obj.insert(std::make_pair("description", desc_item));

    std::string result_str = "_result";
    AMF_Encoder::encode(result_str, amf_buffer);
    double transaction_id = 1.0;
    AMF_Encoder::encode(transaction_id, amf_buffer);
    AMF_Encoder::encode(resp_amf_obj, amf_buffer);
    AMF_Encoder::encode(event_amf_obj, amf_buffer);
    delete fms_ver_item;
    delete capability_item;
    delete level_item;
    delete code_item;
    delete desc_item;

    //log_infof("rtmp connection resp, data len:%lu", amf_buffer.data_len());
    int ret = write_data_by_chunk_stream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    stream_id, session_->get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    //log_infof("rtmp connect done, tcurl:%s", session_->req_.tcurl_.c_str());
    session_->server_phase_ = create_stream_phase;
    return RTMP_OK;
}

int rtmp_control_handler::send_rtmp_create_stream_resp(double transaction_id) {
    data_buffer amf_buffer;
    std::string result_str = "_result";
    double stream_id = (double)session_->stream_id_;

    AMF_Encoder::encode(result_str, amf_buffer);
    AMF_Encoder::encode(transaction_id, amf_buffer);
    AMF_Encoder::encode_null(amf_buffer);
    AMF_Encoder::encode(stream_id, amf_buffer);

    int ret = write_data_by_chunk_stream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    //log_infof("rtmp create stream done tcurl:%s", session_->req_.tcurl_.c_str());
    session_->server_phase_ = create_publish_play_phase;
    return RTMP_OK;
}

int rtmp_control_handler::send_rtmp_play_resp() {
    uint32_t chunk_size = 6;

    //send rtmp control recorded
    chunk_stream ctrl_recorded(session_, 0, 3, chunk_size);
    ctrl_recorded.gen_set_recorded_message();
    session_->rtmp_send(ctrl_recorded.chunk_all_ptr_);

    //send rtmp control begin
    chunk_stream ctrl_begin(session_, 0, 3, chunk_size);
    ctrl_begin.gen_set_begin_message();
    session_->rtmp_send(ctrl_begin.chunk_all_ptr_);

    send_rtmp_play_reset_resp();
    send_rtmp_play_start_resp();
    send_rtmp_play_data_resp();
    send_rtmp_play_notify_resp();

    //log_infof("rtmp play start key:%s", session_->req_.key_.c_str());

    return RTMP_OK;
}

int rtmp_control_handler::send_rtmp_play_reset_resp() {
    data_buffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::encode(result_str, amf_buffer);
    AMF_Encoder::encode(transaction_id, amf_buffer);
    AMF_Encoder::encode_null(amf_buffer);

    std::unordered_map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->set_amf_type(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->set_amf_type(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Play.Reset";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->set_amf_type(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Playing and resetting stream.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = write_data_by_chunk_stream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return RTMP_OK;
}

int rtmp_control_handler::send_rtmp_play_start_resp() {
    data_buffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::encode(result_str, amf_buffer);
    AMF_Encoder::encode(transaction_id, amf_buffer);
    AMF_Encoder::encode_null(amf_buffer);

    std::unordered_map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->set_amf_type(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->set_amf_type(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Play.Start";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->set_amf_type(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Started playing stream.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = write_data_by_chunk_stream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return RTMP_OK;
}

int rtmp_control_handler::send_rtmp_play_data_resp() {
    data_buffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::encode(result_str, amf_buffer);
    AMF_Encoder::encode(transaction_id, amf_buffer);
    AMF_Encoder::encode_null(amf_buffer);

    std::unordered_map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->set_amf_type(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->set_amf_type(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Data.Start";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->set_amf_type(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Started playing stream.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = write_data_by_chunk_stream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return RTMP_OK;
}

int rtmp_control_handler::send_rtmp_play_notify_resp() {
    data_buffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::encode(result_str, amf_buffer);
    AMF_Encoder::encode(transaction_id, amf_buffer);
    AMF_Encoder::encode_null(amf_buffer);

    std::unordered_map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->set_amf_type(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->set_amf_type(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Play.PublishNotify";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->set_amf_type(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Started playing notify.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = write_data_by_chunk_stream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    return RTMP_OK;
}

int rtmp_control_handler::send_rtmp_publish_resp() {
    data_buffer amf_buffer;
    double transaction_id = 0.0;

    std::string result_str = "onStatus";
    AMF_Encoder::encode(result_str, amf_buffer);
    AMF_Encoder::encode(transaction_id, amf_buffer);
    AMF_Encoder::encode_null(amf_buffer);

    std::unordered_map<std::string, AMF_ITERM*> resp_amf_obj;

    AMF_ITERM* level_item = new AMF_ITERM();
    level_item->set_amf_type(AMF_DATA_TYPE_STRING);
    level_item->desc_str_ = "status";
    resp_amf_obj.insert(std::make_pair("level", level_item));

    AMF_ITERM* code_item = new AMF_ITERM();
    code_item->set_amf_type(AMF_DATA_TYPE_STRING);
    code_item->desc_str_ = "NetStream.Publish.Start";
    resp_amf_obj.insert(std::make_pair("code", code_item));

    AMF_ITERM* desc_item = new AMF_ITERM();
    desc_item->set_amf_type(AMF_DATA_TYPE_STRING);
    desc_item->desc_str_ = "Start publising.";
    resp_amf_obj.insert(std::make_pair("description", desc_item));
    AMF_Encoder::encode(resp_amf_obj, amf_buffer);

    delete level_item;
    delete code_item;
    delete desc_item;

    int ret = write_data_by_chunk_stream(session_, 3, 0, RTMP_COMMAND_MESSAGES_AMF0,
                                    session_->stream_id_, session_->get_chunk_size(),
                                    amf_buffer);
    if (ret != RTMP_OK) {
        return ret;
    }

    //log_infof("rtmp publish start key:%s", session_->req_.key_.c_str());
    session_->server_phase_ = media_handle_phase;
    return RTMP_OK;
}

int rtmp_control_handler::send_rtmp_ack(uint32_t size) {
    int ret = 0;
    session_->ack_received_ += size;

    if (session_->ack_received_ > session_->remote_window_acksize_) {
        chunk_stream ack(session_, 0, 3, session_->get_chunk_size());
        ret += ack.gen_control_message(RTMP_CONTROL_ACK, 4, session_->get_chunk_size());
        session_->rtmp_send(ack.chunk_all_ptr_);
        session_->ack_received_ = 0;
    }
    return RTMP_OK;
}

int rtmp_control_handler::handle_rtmp_control_message(CHUNK_STREAM_PTR cs_ptr, bool is_server) {
    if (cs_ptr->type_id_ == RTMP_CONTROL_SET_CHUNK_SIZE) {
        if (cs_ptr->chunk_data_ptr_->data_len() < 4) {
            log_errorf("set chunk size control message size error:%d", cs_ptr->chunk_data_ptr_->data_len());
            return -1;
        }
        session_->set_chunk_size(read_4bytes((uint8_t*)cs_ptr->chunk_data_ptr_->data()));
        //log_infof("update chunk size:%u", session_->get_chunk_size());
        if (is_server) {
            send_set_chunksize(session_->get_chunk_size());
        }
    } else if (cs_ptr->type_id_ == RTMP_CONTROL_WINDOW_ACK_SIZE) {
        if (cs_ptr->chunk_data_ptr_->data_len() < 4) {
            log_errorf("window ack size control message size error:%d", cs_ptr->chunk_data_ptr_->data_len());
            return -1;
        }
        session_->remote_window_acksize_ = read_4bytes((uint8_t*)cs_ptr->chunk_data_ptr_->data());
        //log_infof("update remote window ack size:%u", session_->remote_window_acksize_);
    } else if (cs_ptr->type_id_ == RTMP_CONTROL_SET_PEER_BANDWIDTH) {
        //log_infof("update peer bandwidth:%u", read_4bytes((uint8_t*)cs_ptr->chunk_data_ptr_->data()));
    } else if (cs_ptr->type_id_ == RTMP_CONTROL_ACK) {
        //log_infof("receive rtmp control ack...");
    } else {
        //log_infof("don't handle rtmp control message:%d", cs_ptr->type_id_);
    }
    return RTMP_OK;
}