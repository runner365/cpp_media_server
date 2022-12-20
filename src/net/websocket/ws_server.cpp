#include "ws_server.hpp"
#include "ws_session.hpp"
#include "tcp/tcp_server.hpp"
#include "tcp/tcp_session.hpp"
#include "utils/logger.hpp"
#include <vector>

websocket_server::websocket_server(uv_loop_t* loop,
                            uint16_t port,
                            websocket_server_callbackI* cb):timer_interface(loop, 5000)
                                                        , cb_(cb)
{
    tcp_svr_ptr_ = std::make_unique<tcp_server>(loop, port, this);
    ssl_enable_ = false;
    start_timer();
    log_infof("websocket construct, port:%d, ssl disable", port);
}

websocket_server::websocket_server(uv_loop_t* loop, uint16_t port
                                , websocket_server_callbackI* cb
                                , const std::string& key_file
                                , const std::string& cert_file):timer_interface(loop, 5000)
                                                        , cb_(cb)
                                                        , key_file_(key_file)
                                                        , cert_file_(cert_file)
{
    tcp_svr_ptr_ = std::make_unique<tcp_server>(loop, port, this);
    ssl_enable_ = true;
    start_timer();
    log_infof("websocket construct, port:%d ssl enable", port);
}

websocket_server::~websocket_server()
{
    tcp_svr_ptr_->close();
    log_infof("websocket server destruct...");
}

void websocket_server::on_accept(int ret_code, uv_loop_t* loop, uv_stream_t* handle) {
    std::shared_ptr<websocket_session> session_ptr;

    if (ret_code != 0) {
        return;
    }

    if (ssl_enable_) {
        session_ptr = std::make_shared<websocket_session>(loop, handle,
                                                    this, cb_,
                                                    key_file_, cert_file_);
    } else {
        session_ptr = std::make_shared<websocket_session>(loop, handle, this, cb_);
    }

    sessions_[session_ptr->get_uuid()] = session_ptr;

    cb_->on_accept(session_ptr.get());
    return;
}

void websocket_server::on_timer() {
    std::vector<std::string> uuids;
    for (auto& item : sessions_) {
        if (item.second->get_die_count() > 5) {
            uuids.push_back(item.first);
        }
        item.second->incr_die_count();
    }

    for (auto& uuid : uuids) {
        log_infof("remove old websocket session uuid:%s", uuid.c_str());
        sessions_.erase(uuid);
    }

}

void websocket_server::session_close(websocket_session* session) {
    auto iter = sessions_.find(session->get_uuid());
    if (iter == sessions_.end()) {
        log_errorf("session not found in session map");
        return;
    }
    sessions_.erase(iter);
    return;
}
