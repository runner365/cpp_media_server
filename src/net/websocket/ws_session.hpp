#ifndef WEBSOCKET_SESSION_HPP
#define WEBSOCKET_SESSION_HPP
#include "ws28/Server.h"

class websocket_server;
class websocket_session
{
public:
    websocket_session(const std::string& method,
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
    ~websocket_session()
    {
    }

public:
    std::string method() {
        return method_;
    }
    std::string path() {
        return path_;
    }
    std::string remote_ip() {
        return ip_;
    }

    ws28::Client* get_client() {
        return client_;
    }

    websocket_server* get_server() {
        return server_;
    }

    void set_close(bool flag) {
        close_ = flag;
    }

    bool is_close() {
        return close_;
    }

private:
	std::string method_;
	std::string path_;
	std::string ip_;
    ws28::Client* client_ = nullptr;
    websocket_server* server_ = nullptr;
    bool close_ = false;
};

#endif //WEBSOCKET_SESSION_HPP
