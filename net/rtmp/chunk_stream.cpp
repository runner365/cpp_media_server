#include "chunk_stream.hpp"
#include "rtmp_session_base.hpp"
#include "rtmp_server_session.hpp"
#include "rtmp_client_session.hpp"
#include "rtmp_pub.hpp"
#include "logger.hpp"

static const int FORMAT0_HEADER_LEN = 11;
static const int FORMAT1_HEADER_LEN = 7;
static const int FORMAT2_HEADER_LEN = 3;
static const int EXT_TS_LEN = 4;

chunk_stream::chunk_stream(rtmp_session_base* session, uint8_t fmt, uint16_t csid, uint32_t chunk_size) {
    session_ = session;
    fmt_     = fmt;
    csid_    = csid;
    chunk_size_     = chunk_size;
    chunk_all_ptr_  = std::make_shared<data_buffer>();
    chunk_data_ptr_ = std::make_shared<data_buffer>();
}

chunk_stream::~chunk_stream() {

}

bool chunk_stream::is_ready() {
    return chunk_ready_;
}

int chunk_stream::gen_set_recorded_message() {
    uint8_t* data = new uint8_t[6];

    fmt_           = 0;
    csid_          = 2;
    type_id_       = (uint8_t)RTMP_CONTROL_USER_CTRL_MESSAGES;
    msg_stream_id_ = 1;
    msg_len_       = 6;

    data[0] = (((uint8_t)Event_streamIsRecorded) >> 8) & 0xff;
    data[1] = ((uint8_t)Event_streamIsRecorded) & 0xff;

    for (int i = 0; i < 4; i++) {
        data[2+i] = (1 >> (uint32_t)((3-i)*8)) & 0xff;
    }

    gen_data(data, 6);

    delete[] data;
    return RTMP_OK;
}

int chunk_stream::gen_set_begin_message() {
    uint8_t* data = new uint8_t[6];

    fmt_           = 0;
    csid_          = 2;
    type_id_       = (uint8_t)RTMP_CONTROL_USER_CTRL_MESSAGES;
    msg_stream_id_ = 1;
    msg_len_       = 6;

    data[0] = (((uint8_t)Event_streamBegin) >> 8) & 0xff;
    data[1] = ((uint8_t)Event_streamBegin) & 0xff;

    for (int i = 0; i < 4; i++) {
        data[2+i] = (1 >> (uint32_t)((3-i)*8)) & 0xff;
    }

    gen_data(data, 6);

    delete[] data;
    return RTMP_OK;
}

int chunk_stream::gen_control_message(RTMP_CONTROL_TYPE ctrl_type, uint32_t size, uint32_t value) {
    fmt_           = 0;
    type_id_       = (uint8_t)ctrl_type;
    msg_stream_id_ = 0;
    msg_len_       = size;

    uint8_t* data = new uint8_t[size];
    switch (size) {
        case 1:
        {
            data[0] = (uint8_t)value;
            break;
        }
        case 2:
        {
            write_2bytes(data, value);
            break;
        }
        case 3:
        {
            write_3bytes(data, value);
            break;
        }
        case 4:
        {
            write_4bytes(data, value);
            break;
        }
        case 5:
        {
            write_4bytes(data, value);
            data[4] = 2;
            break;
        }
        default:
        {
            log_errorf("unkown control size:%u", size);
            delete[] data;
            return -1;
        }
    }

    gen_data(data, size);

    delete[] data;
    return RTMP_OK;
}

int chunk_stream::read_msg_format0(uint8_t input_fmt, uint16_t input_csid) {
    data_buffer* buffer_p = session_->get_recv_buffer();

    if (!buffer_p->require(FORMAT0_HEADER_LEN)) {
        return RTMP_NEED_READ_MORE;
    }
    
    fmt_  = input_fmt;
    csid_ = input_csid;

    uint8_t* p  = (uint8_t*)buffer_p->data();
    uint32_t ts = read_3bytes(p);
    if (ts >= 0xffffff) {
        //require 4 bytes
        ext_ts_flag_ = true;
        if (!buffer_p->require(FORMAT0_HEADER_LEN + EXT_TS_LEN)) {
            return RTMP_NEED_READ_MORE;
        }
    }

    p += 3;
    uint32_t msg_len = read_3bytes(p);
    p += 3;
    uint8_t type_id = *p;
    p++;
    uint32_t msg_stream_id = read_4bytes(p);
    p += 4;

    buffer_p->consume_data(FORMAT0_HEADER_LEN);

    if (ts >= 0xffffff) {
        ts = read_4bytes(p);
        buffer_p->consume_data(EXT_TS_LEN);
    }

    timestamp32_     = ts;
    timestamp_delta_ = 0;

    msg_len_ = msg_len;
    type_id_ = type_id;
    msg_stream_id_ = msg_stream_id;

    remain_  = msg_len_;

    return RTMP_OK;
}

int chunk_stream::read_msg_format1(uint8_t input_fmt, uint16_t input_csid) {
    data_buffer* buffer_p = session_->get_recv_buffer();

    if (!buffer_p->require(FORMAT1_HEADER_LEN)) {
        return RTMP_NEED_READ_MORE;
    }
    
    fmt_  = input_fmt;
    csid_ = input_csid;

    uint8_t* p  = (uint8_t*)buffer_p->data();
    uint32_t ts =  read_3bytes(p);
    if (ts >= 0xffffff) {
        //require 4 bytes
        ext_ts_flag_ = true;
        if (!buffer_p->require(FORMAT1_HEADER_LEN + EXT_TS_LEN)) {
            return RTMP_NEED_READ_MORE;
        }
    }

    p += 3;
    uint32_t msg_len = read_3bytes(p);
    p += 3;
    uint8_t type_id = *p;
    p++;

    buffer_p->consume_data(FORMAT1_HEADER_LEN);

    if (ts >= 0xffffff) {
        ts = read_4bytes(p);
        buffer_p->consume_data(EXT_TS_LEN);
    }

    timestamp_delta_ = ts;
    timestamp32_    += ts;

    msg_len_ = msg_len;
    type_id_ = type_id;

    remain_  = msg_len_;
    return RTMP_OK;
}

int chunk_stream::read_msg_format2(uint8_t input_fmt, uint16_t input_csid) {
    data_buffer* buffer_p = session_->get_recv_buffer();

    if (!buffer_p->require(FORMAT2_HEADER_LEN)) {
        return RTMP_NEED_READ_MORE;
    }

    fmt_  = input_fmt;
    csid_ = input_csid;

    uint8_t* p  = (uint8_t*)buffer_p->data();
    uint32_t ts =  read_3bytes(p);
    if (ts >= 0xffffff) {
        //require 4 bytes more
        ext_ts_flag_ = true;
        if (!buffer_p->require(FORMAT2_HEADER_LEN + EXT_TS_LEN)) {
            return RTMP_NEED_READ_MORE;
        }
    }

    p += 3;

    buffer_p->consume_data(FORMAT2_HEADER_LEN);

    if (ts >= 0xffffff) {
        ts = read_4bytes(p);
        buffer_p->consume_data(EXT_TS_LEN);
    }

    timestamp_delta_ = ts;
    timestamp32_    += ts;

    remain_  = msg_len_;
    return RTMP_OK;
}

int chunk_stream::read_msg_format3(uint8_t input_fmt, uint16_t input_csid) {
    data_buffer* buffer_p = session_->get_recv_buffer();
    uint8_t* p;

    if (remain_ == 0) {
        //it's a new one
        switch (fmt_) {
            case 0:
            {
                if (ext_ts_flag_) {
                    if (!buffer_p->require(EXT_TS_LEN)) {
                        return RTMP_NEED_READ_MORE;
                    }
                    p = (uint8_t*)buffer_p->data();
                    timestamp32_ = read_4bytes(p);
                    buffer_p->consume_data(EXT_TS_LEN);
                }
                break;
            }
            case 1:
            case 2:
            {
                uint32_t ts_delta = 0;
                if (ext_ts_flag_) {
                    if (!buffer_p->require(EXT_TS_LEN)) {
                        return RTMP_NEED_READ_MORE;
                    }
                    p = (uint8_t*)buffer_p->data();
                    ts_delta = read_4bytes(p);
                    buffer_p->consume_data(EXT_TS_LEN);
                } else {
                    ts_delta = timestamp_delta_;
                }
                timestamp32_ += ts_delta;
                break;
            }
        }
        //chunk stream data reset
        chunk_all_ptr_->reset();
        chunk_data_ptr_->reset();
        remain_      = msg_len_;
        chunk_ready_ = false;
    } else {
        if (ext_ts_flag_) {
            if (!buffer_p->require(EXT_TS_LEN)) {
                return RTMP_NEED_READ_MORE;
            }
            p = (uint8_t*)buffer_p->data();
            uint32_t ts = read_4bytes(p);
            if (ts == timestamp32_) {
                buffer_p->consume_data(EXT_TS_LEN);
            }
        }
    }
    fmt_  = input_fmt;
    csid_ = input_csid;

    return RTMP_OK;
}

int chunk_stream::read_message_header(uint8_t input_fmt, uint16_t input_csid) {
    int ret = -1;

    if (phase_ != CHUNK_STREAM_PHASE_HEADER) {
        return RTMP_OK;
    }

    if (input_fmt == 0) {
        ret = read_msg_format0(input_fmt, input_csid);
    } else if (input_fmt == 1) {
        ret = read_msg_format1(input_fmt, input_csid);
    } else if (input_fmt == 2) {
        ret = read_msg_format2(input_fmt, input_csid);
    } else if (input_fmt == 3) {
        ret = read_msg_format3(input_fmt, input_csid);
    } else {
        assert(0);
        return -1;
    }

    if (ret != RTMP_OK) {
        return ret;
    }

    phase_ = CHUNK_STREAM_PHASE_PAYLOAD;
    return RTMP_OK;
}

int chunk_stream::read_message_payload() {
    if (phase_ != CHUNK_STREAM_PHASE_PAYLOAD) {
        return RTMP_OK;
    }

    data_buffer* buffer_p = session_->get_recv_buffer();
    require_len_ = (remain_ > chunk_size_) ? chunk_size_ : remain_;
    if (!buffer_p->require(require_len_)) {
        return RTMP_NEED_READ_MORE;
    }
    chunk_data_ptr_->append_data(buffer_p->data(), require_len_);
    buffer_p->consume_data(require_len_);

    remain_ -= require_len_;
    msg_count_++;
    if (remain_ <= 0) {
        chunk_ready_ = true;
    }

    phase_ = CHUNK_STREAM_PHASE_HEADER;
    return RTMP_OK;
}

void chunk_stream::dump_header() {
    data_buffer* buffer_p = session_->get_recv_buffer();
    log_infof("basic header: fmt=%d, csid:%d; message header: timestamp=%lu, \
msglen=%u, typeid:%d, msg streamid:%u, remain:%ld, recv buffer len:%lu, chunk_size_:%u",
        fmt_, csid_, timestamp32_, msg_len_, type_id_, msg_stream_id_, remain_,
        buffer_p->data_len(), chunk_size_);
}

void chunk_stream::dump_payload() {
    char desc[128];

    snprintf(desc, sizeof(desc), "chunk stream payload:%lu", chunk_data_ptr_->data_len());
    log_info_data((uint8_t*)chunk_data_ptr_->data(), chunk_data_ptr_->data_len(), desc);
}

void chunk_stream::dump_alldata() {
    char desc[128];

    snprintf(desc, sizeof(desc), "chunk stream all data:%lu", chunk_all_ptr_->data_len());
    log_info_data((uint8_t*)chunk_all_ptr_->data(), chunk_all_ptr_->data_len(), desc);
}

int chunk_stream::gen_data(uint8_t* data, int len) {
    if (csid_ < 64) {
        uint8_t fmt_csid_data[1];
        fmt_csid_data[0] = (fmt_ << 6) | (csid_ & 0x3f);
        chunk_all_ptr_->append_data((char*)fmt_csid_data, sizeof(fmt_csid_data));
    } else if ((csid_ - 64) < 256) {
        uint8_t fmt_csid_data[2];
        fmt_csid_data[0] = (fmt_ << 6) & 0xc0;
        fmt_csid_data[1] = csid_;
        chunk_all_ptr_->append_data((char*)fmt_csid_data, sizeof(fmt_csid_data));
    } else if ((csid_ - 64) < 65536) {
        uint8_t fmt_csid_data[3];
        fmt_csid_data[0] = ((fmt_ << 6) & 0xc0) | 0x01;
        write_2bytes(fmt_csid_data + 1, csid_);
        chunk_all_ptr_->append_data((char*)fmt_csid_data, sizeof(fmt_csid_data));
    } else {
        log_errorf("csid error:%d", csid_);
        return -1;
    }

    if (fmt_ == 0) {
        uint8_t header_data[11];
        uint8_t* p = header_data;
        write_3bytes(p, timestamp32_);
        p += 3;
        write_3bytes(p, msg_len_);
        p += 3;
        *p = type_id_;
        p++;
        write_4bytes(p, msg_stream_id_);
        chunk_all_ptr_->append_data((char*)header_data, sizeof(header_data));
    } else if (fmt_ == 1) {
        uint8_t header_data[7];
        uint8_t* p = header_data;
        write_3bytes(p, timestamp32_);
        p += 3;
        write_3bytes(p, msg_len_);
        p += 3;
        *p = type_id_;
        chunk_all_ptr_->append_data((char*)header_data, sizeof(header_data));
    } else if (fmt_ == 2) {
        uint8_t header_data[3];
        uint8_t* p = header_data;
        write_3bytes(p, timestamp32_);
        chunk_all_ptr_->append_data((char*)header_data, sizeof(header_data));
    } else {
        //need do nothing
    }

    chunk_all_ptr_->append_data((char*)data, (size_t)len);
    return RTMP_OK;
}

void chunk_stream::reset() {
    phase_ = CHUNK_STREAM_PHASE_HEADER;
    chunk_ready_ = false;
    chunk_all_ptr_->reset();
    chunk_data_ptr_->reset();
}

int write_data_by_chunk_stream(rtmp_session_base* session, uint16_t csid,
                    uint32_t timestamp, uint8_t type_id,
                    uint32_t msg_stream_id, uint32_t chunk_size,
                    data_buffer& input_buffer)
{
    int cs_count = input_buffer.data_len()/chunk_size;
    int fmt = 0;
    uint8_t* p;
    int len;

    if ((input_buffer.data_len()%chunk_size) > 0) {
        cs_count++;
    }

    for (int index = 0; index < cs_count; index++) {
        if (index == 0) {
            fmt = 0;
        } else {
            fmt = 3;
        }
        p = (uint8_t*)input_buffer.data() + index * chunk_size;
        if (index == (cs_count-1)) {
            if ((input_buffer.data_len()%chunk_size) > 0) {
                len = input_buffer.data_len()%chunk_size;
            } else {
                len = chunk_size;
            }
        } else {
            len = chunk_size;
        }
        chunk_stream* c = new chunk_stream(session, fmt, csid, chunk_size);

        c->timestamp32_ = timestamp;
        c->msg_len_     = input_buffer.data_len();
        c->type_id_     = type_id;
        c->msg_stream_id_ = msg_stream_id;
        c->gen_data(p, len);
        
        session->rtmp_send(c->chunk_all_ptr_);

        delete c;
    }
    return RTMP_OK;
}

int write_data_by_chunk_stream(rtmp_session_base* session, uint16_t csid,
                    uint32_t timestamp, uint8_t type_id,
                    uint32_t msg_stream_id, uint32_t chunk_size,
                    std::shared_ptr<data_buffer> input_buffer_ptr)
{
    int cs_count = input_buffer_ptr->data_len()/chunk_size;
    int fmt = 0;
    uint8_t* p;
    int len;

    if ((input_buffer_ptr->data_len()%chunk_size) > 0) {
        cs_count++;
    }

    for (int index = 0; index < cs_count; index++) {
        if (index == 0) {
            fmt = 0;
        } else {
            fmt = 3;
        }
        p = (uint8_t*)input_buffer_ptr->data() + index * chunk_size;
        if (index == (cs_count-1)) {
            if ((input_buffer_ptr->data_len()%chunk_size) > 0) {
                len = input_buffer_ptr->data_len()%chunk_size;
            } else {
                len = chunk_size;
            }
        } else {
            len = chunk_size;
        }
        chunk_stream* c = new chunk_stream(session, fmt, csid, chunk_size);

        c->timestamp32_ = timestamp;
        c->msg_len_     = input_buffer_ptr->data_len();
        c->type_id_     = type_id;
        c->msg_stream_id_ = msg_stream_id;
        c->gen_data(p, len);
        
        //c->dump_header();
        //c->dump_alldata();
        session->rtmp_send(c->chunk_all_ptr_);

        delete c;
    }
    return RTMP_OK;
}