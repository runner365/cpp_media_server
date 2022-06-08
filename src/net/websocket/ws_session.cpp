#include "ws_session.hpp"
#include "wsimple/flv_websocket.hpp"
#include "format/flv/flv_demux.hpp"

websocket_session::websocket_session(const std::string& method,
                const std::string& path,
                const std::string& ip,
                ws28::Client* client,
                websocket_server* server):method_(method)
                        , path_(path)
                        , ip_(ip)
                        , client_(client)
                        , server_(server)
{
    close_ = false;
}

websocket_session::~websocket_session()
{
    if (demuxer_) {
        delete demuxer_;
        demuxer_ = nullptr;
    }
    if (outputer_) {
        delete outputer_;
        outputer_ = nullptr;
    }
}

std::string websocket_session::method() {
    return method_;
}

std::string websocket_session::path() {
    return path_;
}

std::string websocket_session::remote_ip() {
    return ip_;
}

ws28::Client* websocket_session::get_client() {
    return client_;
}

websocket_server* websocket_session::get_server() {
    return server_;
}

void websocket_session::set_close(bool flag) {
    close_ = flag;
}

bool websocket_session::is_close() {
    return close_;
}

std::string websocket_session::get_uri() {
    return uri_;
}

void websocket_session::set_uri(const std::string& uri) {
    uri_ = uri;
}