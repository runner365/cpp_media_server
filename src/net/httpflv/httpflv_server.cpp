#include "httpflv_server.hpp"
#include "httpflv_writer.hpp"
#include "media_stream_manager.hpp"
#include "logger.hpp"
#include "uuid.hpp"
#include <string>
#include <unordered_map>

std::unordered_map<std::string, httpflv_writer*> s_httpflv_handle_map;

void httpflv_handle(const http_request* request, std::shared_ptr<http_response> response) {
    auto pos = request->uri_.find(".flv");
    if (pos == std::string::npos) {
        log_errorf("http flv request uri error:%s", request->uri_.c_str());
        response->close();
        return;
    }
    std::string key = request->uri_.substr(0, pos);
    if (key[0] == '/') {
        key = key.substr(1);
    }

    
    std::string uuid = make_uuid();

    log_infof("http flv request key:%s, uuid:%s", key.c_str(), uuid.c_str());

    httpflv_writer* writer_p = new httpflv_writer(key, uuid, response);

    s_httpflv_handle_map.insert(std::make_pair(uuid, writer_p));

    media_stream_manager::add_player(writer_p);

    return;
}

httpflv_server::httpflv_server(uv_loop_t* loop, uint16_t port):timer_interface(loop, 5000)
    , server_(loop, port)
{
    run();
    start_timer();
    log_infof("http flv server is listen:%d", port);
}

httpflv_server::~httpflv_server()
{
    stop_timer();
}

void httpflv_server::httpflv_writer_close(const std::string& id) {
    auto iter = s_httpflv_handle_map.find(id);
    if (iter == s_httpflv_handle_map.end()) {
        log_warnf("httpflv writer id error:%s", id.c_str());
        return;
    }
    httpflv_writer* writer_p = iter->second;
    s_httpflv_handle_map.erase(iter);
    delete writer_p;
}

void httpflv_server::run() {
    server_.add_get_handle("/", httpflv_handle);
    return;
}

void httpflv_server::on_timer() {
    on_check_alive();
}

void httpflv_server::on_check_alive() {
    auto iter = s_httpflv_handle_map.begin();

    while (iter != s_httpflv_handle_map.end()) {
        httpflv_writer* writer_p = iter->second;
        bool is_alive = writer_p->is_alive();
        if (!is_alive) {
            s_httpflv_handle_map.erase(iter++);
            media_stream_manager::remove_player(writer_p);
            delete writer_p;
            continue;
        }
        iter++;
    }
    return;
}

