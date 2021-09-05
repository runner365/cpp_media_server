#ifndef RTMP_CONTROL_HANDLER_HPP
#define RTMP_CONTROL_HANDLER_HPP
#include "chunk_stream.hpp"
#include "amf/amf0.hpp"
#include "rtmp_pub.hpp"
#include <vector>

class rtmp_session_base;

class rtmp_control_handler
{
public:
    rtmp_control_handler(rtmp_session_base* session);
    ~rtmp_control_handler();

public:
    int handle_server_command_message(CHUNK_STREAM_PTR cs_ptr, std::vector<AMF_ITERM*>& amf_vec);
    int handle_client_command_message(CHUNK_STREAM_PTR cs_ptr, std::vector<AMF_ITERM*>& amf_vec);
    int handle_rtmp_publish_command(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec);
    int handle_rtmp_play_command(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec);
    int handle_rtmp_createstream_command(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec);
    int handle_rtmp_connect_command(uint32_t stream_id, std::vector<AMF_ITERM*>& amf_vec);
    int send_rtmp_publish_resp();
    int send_rtmp_play_resp();
    int send_rtmp_play_reset_resp();
    int send_rtmp_play_start_resp();
    int send_rtmp_play_data_resp();
    int send_rtmp_play_notify_resp();
    int send_rtmp_create_stream_resp(double transaction_id);
    int send_rtmp_connect_resp(uint32_t stream_id);
    int send_rtmp_ack(uint32_t size);
    int send_set_chunksize(uint32_t chunk_size);

public:
    int handle_rtmp_control_message(CHUNK_STREAM_PTR cs_ptr);

private:
    rtmp_session_base* session_;
};

#endif //RTMP_CONTROL_HANDLER_HPP