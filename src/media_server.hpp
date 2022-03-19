#ifndef MEDIA_SERVER_H
#define MEDIA_SERVER_H
#include "rtmp_server.hpp"
#include "ws_server.hpp"
#include "httpflv_server.hpp"
#include "net/webrtc/webrtc_pub.hpp"
#include "net/webrtc/rtc_dtls.hpp"
#include "net/webrtc/srtp_session.hpp"
#include "net/webrtc/rtmp2rtc.hpp"
#include "net/hls/hls_writer.hpp"
#include "utils/byte_crypto.hpp"
#include "utils/config.hpp"
#include "utils/av/media_stream_manager.hpp"
#include "logger.hpp"
#include <stdint.h>
#include <stddef.h>
#include <iostream>


class MediaServer
{
public:
    static void Run(const std::string& cfg_file);
    static boost::asio::io_context& get_io_ctx() { return MediaServer::io_context; }

private:
    static void create_webrtc();
    static void create_rtmp();
    static void create_httpflv();
    static void create_hls();
    static void create_websocket_flv();

    static void release_all();

private:
    static boost::asio::io_context io_context;
    static boost::asio::io_context hls_io_context;

private:
    static websocket_server* ws_p;
    static rtmp2rtc_writer* r2r_output;

private:
    static std::shared_ptr<rtmp_server> rtmp_ptr;

private:
    static std::shared_ptr<httpflv_server> httpflv_ptr;

private:
    static hls_writer* hls_output;

private:
    static std::shared_ptr<websocket_server> ws_flv_ptr;
};

#endif

