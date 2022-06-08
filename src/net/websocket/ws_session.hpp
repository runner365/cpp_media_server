#ifndef WEBSOCKET_SESSION_HPP
#define WEBSOCKET_SESSION_HPP
#include "ws28/Server.h"

class websocket_server;
class flv_demuxer;
class av_outputer;

class websocket_session
{
public:
    websocket_session(const std::string& method,
                const std::string& path,
                const std::string& ip,
                ws28::Client* client,
                websocket_server* server);
    ~websocket_session();

public:
    std::string method();
    std::string path();
    std::string remote_ip();
    ws28::Client* get_client();
    websocket_server* get_server();
    void set_close(bool flag);
    bool is_close();
    std::string get_uri();
    void set_uri(const std::string& uri);

public:
    flv_demuxer* demuxer_  = nullptr;
    av_outputer* outputer_ = nullptr;

private:
	std::string method_;
	std::string path_;
	std::string ip_;
    ws28::Client* client_ = nullptr;
    websocket_server* server_ = nullptr;
    bool close_ = false;

    std::string uri_;

};

#endif //WEBSOCKET_SESSION_HPP
