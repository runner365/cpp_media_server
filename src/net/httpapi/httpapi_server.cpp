#include "httpapi_server.hpp"
#include "http_common.hpp"
#include "json.hpp"
#include <memory>
#include <sstream>

using json = nlohmann::json;

extern int get_publisher_statics(const std::string& roomId, const std::string& uid, json& data_json);
extern int get_subscriber_statics(const std::string& roomId, const std::string& uid, json& data_json);

void httpapi_response(int code, const std::string& desc, const json& data, std::shared_ptr<http_response> response) {
    auto resp_json = json::object();

    resp_json["code"] = code;
    resp_json["desc"] = desc;
    resp_json["data"] = data;

    response->write(resp_json.dump().c_str(), resp_json.dump().length());
}

/*
url: /api/webrtc/publisher?roomId=xxxx&uid=xxxx
*/
void httpapi_webrtc_publisher_handle(const http_request* request, std::shared_ptr<http_response> response) {
    auto data_json = json::object();

    try {
        std::string roomId = request->params.at("roomId");
        std::string uid    = request->params.at("uid");

        log_infof("publisher request uri:%s, roomId:%s, uid:%s",
                request->uri_.c_str(), roomId.c_str(), uid.c_str());
        get_publisher_statics(roomId, uid, data_json);
    }
    catch(const std::exception& e) {
        std::stringstream ss;
        ss << "parameter error: " << e.what();
        httpapi_response(501, ss.str().c_str(), data_json, response);
        return;
    }
    
    httpapi_response(0, "ok", data_json, response);
    return;
}

/*
url: /api/webrtc/subscriber?roomId=xxxx&uid=xxxx
*/
void httpapi_webrtc_subscriber_handle(const http_request* request, std::shared_ptr<http_response> response) {
    auto data_json = json::object();

    try {
        std::string roomId = request->params.at("roomId");
        std::string uid    = request->params.at("uid");

        log_infof("subscriber request uri:%s, roomId:%s, uid:%s",
                request->uri_.c_str(), roomId.c_str(), uid.c_str());
        get_subscriber_statics(roomId, uid, data_json);
    }
    catch(const std::exception& e) {
        std::stringstream ss;
        ss << "parameter error: " << e.what();
        httpapi_response(501, ss.str().c_str(), data_json, response);
        return;
    }
    
    httpapi_response(0, "ok", data_json, response);
    return;
}

httpapi_server::httpapi_server(boost::asio::io_context& io_ctx, uint16_t port):server_(io_ctx, port)
{
    run();
    log_infof("http api server is listen:%d", port);
}

httpapi_server::~httpapi_server()
{
}


void httpapi_server::run() {
    server_.add_get_handle("/api/webrtc/publisher", httpapi_webrtc_publisher_handle);
    server_.add_get_handle("/api/webrtc/subscriber", httpapi_webrtc_subscriber_handle);
}