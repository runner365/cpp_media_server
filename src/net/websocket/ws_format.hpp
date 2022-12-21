#ifndef WS_FORMAT_HPP
#define WS_FORMAT_HPP
#include "utils/data_buffer.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>

#define WS_OP_CONTINUE_TYPE        0x00
#define WS_OP_TEXT_TYPE            0x01
#define WS_OP_BIN_TYPE             0x02
#define WS_OP_FURTHER_NO_CTRL_TYPE 0x03
#define WS_OP_CLOSE_TYPE           0x08
#define WS_OP_PING_TYPE            0x09
#define WS_OP_PONG_TYPE            0x0a
#define WS_OP_FURTHER_CTRL_TYPE    0x0b

typedef struct S_WS_PACKET_HEADER
{
    uint8_t opcode : 4;
    uint8_t rsv3 : 1;
    uint8_t rsv2 : 1;
    uint8_t rsv1 : 1;
    uint8_t fin : 1;

    uint8_t payload_len : 7;
    uint8_t mask : 1;
} WS_PACKET_HEADER;

class websocket_frame
{
public:
    websocket_frame();
    ~websocket_frame();

    int parse(uint8_t* data, size_t len);
    int get_payload_start() { return payload_start_; }
    int64_t get_payload_len() { return payload_len_; }
    uint8_t* get_payload_data() {
        return (uint8_t*)buffer_.data() + payload_start_;
    }

    size_t get_buffer_len() {
        return buffer_.data_len();
    }
    
    bool payload_is_ready() {
        return payload_len_ <= buffer_.data_len() - payload_start_;
    }

    uint8_t* consume(size_t len) {
        return (uint8_t*)buffer_.consume_data(len);
    }

    uint8_t get_oper_code() {
        return opcode_;
    }

    bool get_fin() {
        return fin_;
    }

    bool is_header_ready() {
        return header_ready_;
    }

    void reset() {
        payload_start_ = 0;
        payload_len_   = 0;
        header_ready_  = false;
    }
    
private:
    data_buffer buffer_;
    int payload_start_   = 0;
    int64_t payload_len_ = 0;
    uint8_t opcode_      = 0;
    bool mask_enable_    = false;
    bool fin_            = false;
    bool header_ready_   = false;
    uint8_t masking_key_[4];
};
#endif