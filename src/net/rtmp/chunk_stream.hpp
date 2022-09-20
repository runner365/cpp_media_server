#ifndef CHUNK_STREAM_HPP
#define CHUNK_STREAM_HPP
#include "data_buffer.hpp"
#include "byte_stream.hpp"
#include "rtmp_pub.hpp"
#include <stdint.h>
#include <memory>

typedef enum {
    CHUNK_STREAM_PHASE_HEADER,
    CHUNK_STREAM_PHASE_PAYLOAD
} CHUNK_STREAM_PHASE;

class rtmp_session_base;
class chunk_stream
{
public:
    chunk_stream(rtmp_session_base* session, uint8_t fmt, uint16_t csid,
                uint32_t chunk_size, bool write_flag = false, size_t data_size = 64*1024);
    ~chunk_stream();

public:
    bool is_ready();
    int read_message_header(uint8_t input_fmt, uint16_t input_csid);
    int read_message_payload();

    void dump_header();
    void dump_payload();
    void dump_alldata();

    int gen_control_message(RTMP_CONTROL_TYPE ctrl_type, uint32_t size, uint32_t value);
    int gen_set_recorded_message();
    int gen_set_begin_message();
    int gen_data(uint8_t* data, int len);

    void reset();

private:
    int read_msg_format0(uint8_t input_fmt, uint16_t input_csid);
    int read_msg_format1(uint8_t input_fmt, uint16_t input_csid);
    int read_msg_format2(uint8_t input_fmt, uint16_t input_csid);
    int read_msg_format3(uint8_t input_fmt, uint16_t input_csid);

public:
    uint32_t timestamp_delta_ = 0;
    uint32_t timestamp32_     = 0;
    uint32_t msg_len_         = 0;
    uint8_t  type_id_         = 0;
    uint32_t msg_stream_id_   = 0;
    int64_t  remain_          = 0;
    int64_t  require_len_     = 0;
    uint32_t chunk_size_      = CHUNK_DEF_SIZE;
    std::shared_ptr<data_buffer> write_data_ptr_;
    std::shared_ptr<data_buffer> read_data_ptr_;

private:
    rtmp_session_base* session_ = nullptr;
    bool ext_ts_flag_ = false;
    uint8_t fmt_      = 0;
    uint16_t csid_    = 0;
    CHUNK_STREAM_PHASE phase_ = CHUNK_STREAM_PHASE_HEADER;
    bool chunk_ready_  = false;
    int64_t msg_count_ = 0;
};

using CHUNK_STREAM_PTR = std::shared_ptr<chunk_stream>;

int write_data_by_chunk_stream(rtmp_session_base* session, uint16_t csid,
                    uint32_t timestamp, uint8_t type_id,
                    uint32_t msg_stream_id, uint32_t chunk_size,
                    std::shared_ptr<data_buffer> input_buffer);

int write_data_by_chunk_stream(rtmp_session_base* session, uint16_t csid,
                    uint32_t timestamp, uint8_t type_id,
                    uint32_t msg_stream_id, uint32_t chunk_size,
                    data_buffer& input_buffer);
#endif