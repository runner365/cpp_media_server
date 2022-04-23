#ifndef RTMP_SESSION_BASE_HPP
#define RTMP_SESSION_BASE_HPP
#include "data_buffer.hpp"
#include "chunk_stream.hpp"
#include "rtmp_pub.hpp"
#include "rtmp_request.hpp"
#include "rtmp_control_handler.hpp"
#include <memory>
#include <unordered_map>
#include <stdint.h>

typedef enum {
    initial_phase = -1,
    handshake_c2_phase,
    connect_phase,
    create_stream_phase,
    create_publish_play_phase,
    media_handle_phase
} RTMP_SERVER_SESSION_PHASE;

typedef enum {
    client_initial_phase = -1,
    client_c0c1_phase,
    client_connect_phase,
    client_connect_resp_phase,
    client_create_stream_phase,
    client_create_stream_resp_phase,
    client_create_play_phase,
    client_create_publish_phase,
    client_media_handle_phase
} RTMP_CLIENT_SESSION_PHASE;

const char* get_server_phase_desc(RTMP_SERVER_SESSION_PHASE phase);

const char* get_client_phase_desc(RTMP_CLIENT_SESSION_PHASE phase);

class rtmp_session_base
{
public:
    virtual data_buffer* get_recv_buffer() = 0;
    virtual int rtmp_send(char* data, int len) = 0;
    virtual int rtmp_send(std::shared_ptr<data_buffer> data_ptr) = 0;

public:
    void set_chunk_size(uint32_t chunk_size);
    uint32_t get_chunk_size();
    bool is_publish();
    const char* is_publish_desc();

protected:
    int read_fmt_csid();
    int read_chunk_stream(CHUNK_STREAM_PTR& cs_ptr);
    MEDIA_PACKET_PTR get_media_packet(CHUNK_STREAM_PTR cs_ptr);

public:
    data_buffer recv_buffer_;
    bool fmt_ready_ = false;
    uint8_t fmt_    = 0;
    uint16_t csid_  = 0;
    std::unordered_map<uint8_t, CHUNK_STREAM_PTR> cs_map_;
    uint32_t remote_window_acksize_ = 2500000;
    uint32_t ack_received_          = 0;
    rtmp_request req_;
    uint32_t stream_id_ = 1;
    RTMP_SERVER_SESSION_PHASE server_phase_ = initial_phase;
    RTMP_CLIENT_SESSION_PHASE client_phase_ = client_initial_phase;

protected:
    uint32_t chunk_size_ = CHUNK_DEF_SIZE;
};

#endif //RTMP_SESSION_BASE_HPP