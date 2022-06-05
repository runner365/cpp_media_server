#include "rtmp_server.hpp"
#include "ws_server.hpp"
#include "httpflv_server.hpp"
#include "net/webrtc/webrtc_pub.hpp"
#include "net/webrtc/rtc_dtls.hpp"
#include "net/webrtc/srtp_session.hpp"
#include "net/webrtc/rtmp2rtc.hpp"
#include "net/hls/hls_writer.hpp"
#include "net/http/http_common.hpp"
#include "net/websocket/wsimple/flv_websocket.hpp"
#include "net/websocket/wsimple/protoo_server.hpp"
#include "utils/byte_crypto.hpp"
#include "utils/config.hpp"
#include "utils/av/media_stream_manager.hpp"
#include "logger.hpp"
#include "media_server.hpp"
#include <stdint.h>
#include <stddef.h>
#include <iostream>
#include <unistd.h>

uv_loop_t* MediaServer::loop_ = uv_default_loop();
uv_loop_t* MediaServer::hls_loop_ = uv_loop_new();

websocket_server* MediaServer::ws_p  = nullptr;
flv_websocket* MediaServer::ws_flv_server = nullptr;
protoo_server* MediaServer::ws_webrtc_server = nullptr;

rtmp2rtc_writer* MediaServer::r2r_output = nullptr;
std::shared_ptr<rtmp_server> MediaServer::rtmp_ptr;
std::shared_ptr<httpflv_server> MediaServer::httpflv_ptr;
std::shared_ptr<httpapi_server> MediaServer::httpapi_ptr;
hls_writer* MediaServer::hls_output = nullptr;
rtmp_relay_manager* MediaServer::relay_mgr_p = nullptr;

void on_play_callback(const std::string& key) {
    if (!Config::rtmp_is_enable()) {
        return;
    }
    std::string host = Config::rtmp_relay_host();
    if (host.empty()) {
        return;
    }

    if (MediaServer::relay_mgr_p == nullptr) {
        return;
    }

    log_infof("request a new stream:%s, host:%s", key.c_str(), host.c_str());
    MediaServer::relay_mgr_p->add_new_relay(host, key);

    return;
}

void MediaServer::create_webrtc() {
    if (!Config::webrtc_is_enable() || Config::tls_key().empty() || Config::tls_cert().empty()) {
        log_infof("webrtc is disable...");
        return;
    }

    byte_crypto::init();
    rtc_dtls::dtls_init(Config::tls_key(), Config::tls_cert());
    srtp_session::init();
    init_single_udp_server(loop_, Config::candidate_ip(), Config::webrtc_udp_port());
    init_webrtc_stream_manager_callback();

    if (Config::rtmp2rtc_is_enable()) {
        MediaServer::r2r_output = new rtmp2rtc_writer();
        media_stream_manager::set_rtc_writer(MediaServer::r2r_output);
    }
    
    if (Config::wss_is_enable()) {
        MediaServer::ws_webrtc_server = new protoo_server(loop_,
                                                    Config::webrtc_https_port(),
                                                    Config::tls_key(),
                                                    Config::tls_cert());
    } else {
        MediaServer::ws_webrtc_server = new protoo_server(loop_, Config::webrtc_https_port());
    }
    
    log_infof("webrtc is starting, websocket port:%d, wss enable:%s rtmp2rtc:%s, rtc2rtmp:%s",
            Config::webrtc_https_port(),
            Config::wss_is_enable() ? "true" : "false",
            Config::rtmp2rtc_is_enable() ? "true" : "false",
            Config::rtc2rtmp_is_enable() ? "true" : "false");
    return;
}

void MediaServer::create_rtmp() {
    if (!Config::rtmp_is_enable()) {
        log_infof("rtmp is disable...");
        return;
    }
    MediaServer::rtmp_ptr = std::make_shared<rtmp_server>(loop_, Config::rtmp_listen_port());
    log_infof("rtmp server is starting, listen port:%d", Config::rtmp_listen_port());

    if (Config::rtmp_relay_is_enable() && !Config::rtmp_relay_host().empty()) {
        MediaServer::relay_mgr_p = new rtmp_relay_manager(MediaServer::loop_);
        media_stream_manager::set_play_callback(on_play_callback);
    }
    return;
}

void MediaServer::create_httpflv() {
    if (!Config::httpflv_is_enable()) {
        log_infof("httpflv is disable...");
        return;
    }

    log_infof("httpflv server is starting, listen port:%d, ssl enable:%s, key file:%s, cert file:%s", 
            Config::httpflv_port(),
            Config::httpflv_ssl_enable() ? "true":"false",
            Config::httpflv_key_file().c_str(),
            Config::httpflv_cert_file().c_str());
    if (Config::httpflv_ssl_enable() && !Config::httpflv_cert_file().empty() && !Config::httpflv_key_file().empty()) {
        MediaServer::httpflv_ptr = std::make_shared<httpflv_server>(MediaServer::loop_,
                                                        Config::httpflv_port(),
                                                        Config::httpflv_key_file(),
                                                        Config::httpflv_cert_file());
    } else {
        MediaServer::httpflv_ptr = std::make_shared<httpflv_server>(MediaServer::loop_, Config::httpflv_port());
    }

    return;
}

void MediaServer::create_httpapi() {
    if (!Config::httpapi_is_enable()) {
        log_infof("httpapi is disable...");
        return;
    }
    if (Config::httpapi_ssl_enable() && !Config::httpapi_cert_file().empty() && !Config::httpapi_key_file().empty()) {
        MediaServer::httpapi_ptr = std::make_shared<httpapi_server>(MediaServer::loop_,
                                                        Config::httpapi_port(),
                                                        Config::httpapi_key_file(),
                                                        Config::httpapi_cert_file());
    } else {
        MediaServer::httpapi_ptr = std::make_shared<httpapi_server>(MediaServer::loop_, Config::httpapi_port());
    }

    log_infof("httpapi server is starting, listen port:%d", Config::httpapi_port());
    return;
}

void MediaServer::create_hls() {
    if (!Config::hls_is_enable()) {
        log_infof("hls is disable...");
        return;
    }
    MediaServer::hls_output = new hls_writer(MediaServer::hls_loop_, Config::hls_path(), true);//enable hls
    media_stream_manager::set_hls_writer(hls_output);
    MediaServer::hls_output->run();

    log_infof("hls server is starting, hls path:%s", Config::hls_path().c_str());
    return;
}

void MediaServer::create_websocket_flv() {
    if (!Config::websocket_is_enable()) {
        log_infof("websocket flv is disable...");
        return;
    }

    if (Config::websocket_wss_enable() &&
        !Config::websocket_key_file().empty() &&
        !Config::websocket_cert_file().empty()) {
        MediaServer::ws_flv_server = new flv_websocket(MediaServer::loop_,
                                                    Config::websocket_port(),
                                                    Config::websocket_key_file(),
                                                    Config::websocket_cert_file());
    } else {
        MediaServer::ws_flv_server = new flv_websocket(MediaServer::loop_, Config::websocket_port());
    }

    log_infof("websocket flv is starting, listen port:%d", Config::websocket_port());
    return;
}

void MediaServer::Run(const std::string& cfg_file) {
    if (cfg_file.empty()) {
        std::cerr << "configure file is needed.\r\n";
        return;
    }

    int ret = Config::load(cfg_file.c_str());
    if (ret < 0) {
        std::cout << "load config file: " << cfg_file << ", error:" << ret << "\r\n";
        return;
    }

    Logger::get_instance()->set_filename(Config::log_filename());
    Logger::get_instance()->set_level(Config::log_level());
    #ifdef __APPLE__
    //enable cosole in mac os
    Logger::get_instance()->enable_console();
    #endif
    log_infof("configuration file:%s", Config::dump().c_str());
    
    try {
        MediaServer::create_webrtc();
        MediaServer::create_rtmp();
        MediaServer::create_httpflv();
        MediaServer::create_hls();
        MediaServer::create_httpapi();
        MediaServer::create_websocket_flv();

        uv_run(MediaServer::loop_, UV_RUN_DEFAULT);
    }
    catch(const std::exception& e) {
        std::cerr << e.what() << '\n';
    }

    MediaServer::release_all();
    return;
}

void MediaServer::release_all() {
    if (MediaServer::ws_p) {
        delete MediaServer::ws_p;
        MediaServer::ws_p = nullptr;
    }

    if (MediaServer::ws_flv_server) {
        delete MediaServer::ws_flv_server;
        MediaServer::ws_flv_server = nullptr;
    }

    if (MediaServer::r2r_output) {
        delete MediaServer::r2r_output;
        MediaServer::r2r_output = nullptr;
    }


    if (MediaServer::hls_output) {
        delete MediaServer::hls_output;
        MediaServer::hls_output = nullptr;
    }

    return;
}

uv_loop_t* get_global_io_context() {
    return MediaServer::get_loop();
}

int main(int argn, char** argv) {
    std::string cfg_file;

    int opt = 0;
    while ((opt = getopt(argn, argv, "c:")) != -1) {
      switch (opt) {
        case 'c':
          cfg_file = std::string(optarg);
          break;
        default:
          std::cerr << "Usage: " << argv[0]
               << " [-c config-file] "
               << std::endl;
          return -1;
      }
    }

    MediaServer::Run(cfg_file);

    return 0;
}
