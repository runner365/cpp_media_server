#include "ws_server.hpp"
#include "tcp_server.hpp"
#include "ws_session.hpp"
#include "ws28/Server.h"
#include "logger.hpp"

void on_connected_callback(ws28::Client* client, ws28::HTTPRequest& req, void* user_data) {
    websocket_server* server = (websocket_server*)user_data;
    websocket_session* new_session = new websocket_session(std::string(req.method),
                                                        std::string(req.path),
                                                        std::string(req.ip),
                                                        client, server);
    log_infof("connected method:%s, path:%s, ip:%s", new_session->method().c_str(),
            new_session->path().c_str(), new_session->remote_ip().c_str());
	client->SetUserData((void*)new_session);
    if (server->cb_) {
        server->cb_->on_accept(new_session);
    }
}

void on_disconnect_callback(ws28::Client* client, void* user_data) {
	websocket_session* del_session = (websocket_session*)client->GetUserData();

    client->SetCloseFlag(true);
    if (del_session) {
        websocket_server* server = (websocket_server*)del_session->get_server();
        del_session->set_close(true);
        if (server && server->cb_) {
            log_infof("websocket is disconnect, server cb:%p", server->cb_);
            server->cb_->on_close(del_session);
        }
    }
    delete del_session;
}

void on_client_data_callback(ws28::Client* client, char* data, size_t len, int opcode, void* user_data) {
    websocket_server* server = (websocket_server*)user_data;
	websocket_session* session = (websocket_session*)client->GetUserData();
    if (session && server->cb_) {
        server->cb_->on_read(session, data, len);
    }
}

websocket_server::websocket_server(uv_loop_t* loop, uint16_t port, websocket_server_callbackI* cb):ws_server_(loop)
                                                            , cb_(cb)
{
    ws_server_.SetMaxMessageSize(256 * 1024 * 1024);
    ws_server_.SetUserData(this);

	ws_server_.SetClientConnectedCallback(on_connected_callback);
	ws_server_.SetClientDisconnectedCallback(on_disconnect_callback);
	ws_server_.SetClientDataCallback(on_client_data_callback);
	
	ws_server_.SetHTTPCallback([](ws28::HTTPRequest& req, ws28::HTTPResponse& res){
		std::stringstream ss;
		ss << "Hi, you issued a " << req.method << " to " << req.path << "\r\n";
		ss << "Headers:\r\n";
		
		log_infof("http callback: %s", ss.str().c_str());
	});

    bool ret = ws_server_.Listen(port);
    log_infof("websocket construct, port:%d return:%d", port, ret);
}

websocket_server::~websocket_server()
{
}
