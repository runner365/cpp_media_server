#include "httpapi_server.hpp"
#include "http_common.hpp"
#include "json.hpp"
#include "utils/stringex.hpp"
#include "utils/uuid.hpp"
#include <memory>
#include <sstream>

using json = nlohmann::json;

/********* for webrtc statics ********/
extern int get_publisher_statics(const std::string& roomId, const std::string& uid, json& data_json);
extern int get_subscriber_statics(const std::string& roomId, const std::string& uid, json& data_json);
extern int get_room_statics(json& data_json);

/********* for webrtc whip ********/
extern int whip_publisher(const std::string& roomId, const std::string& uid, const std::string& data,
                std::string& sdp, std::string& session_id, std::string& err_msg);
extern int whip_unpublisher(const std::string& roomId, const std::string& uid, std::string& err_msg);
extern int whip_unpublisher(const std::string& roomId, const std::string& uid, const std::string& sessionid, std::string& err_msg);
extern int whip_subscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid,
                const std::string& data, std::string& resp_sdp, std::string& session_id, std::string& err_msg);
extern int whip_unsubscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid, std::string& err_msg);
extern int whip_unsubscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid,
                const std::string& sessionid, std::string& err_msg);

void httpapi_response(int code, const std::string& desc, const json& data, std::shared_ptr<http_response> response) {
    auto resp_json = json::object();

    resp_json["code"] = code;
    resp_json["desc"] = desc;
    resp_json["data"] = data;

    response->add_header("Access-Control-Allow-Origin", "*");
    response->add_header("Access-Control-Allow-Headers", "*");
    response->add_header("Content-Type", "application/json;charset=utf-8");

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

/*
url: /api/webrtc/room
*/
void httpapi_webrtc_room_handle(const http_request* request, std::shared_ptr<http_response> response) {
    auto data_json = json::object();

    try {
        get_room_statics(data_json);
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

void whip_http_handle(const http_request* request, std::shared_ptr<http_response> response) {
    int ret = 0;
    std::string resp_sdp;
    std::string session_id;
    std::string err_msg;
    std::string uri = request->uri_;
    std::vector<std::string> path_vec;

    if (uri[0] == '/') {
        uri = uri.substr(1);
    }

    string_split(uri, "/", path_vec);

    if (path_vec.size() < 3) {
        std::string err_msg = "wrong http path";
        response->write(err_msg.c_str(), err_msg.length());
        return;
    }

    std::string oper   = path_vec[0];
    std::string roomId = path_vec[1];
    std::string uid    = path_vec[2];

    if (oper.empty() || roomId.empty() || uid.empty()) {
        std::string err_msg = "wrong http path";
        response->write(err_msg.c_str(), err_msg.length());
        return;
    }
    response->add_header("Vary", "Origin");
    response->add_header("Vary", "Access-Control-Request-Method");
    response->add_header("Vary", "Access-Control-Request-Headers");
    response->add_header("Access-Control-Allow-Origin", "*");
    response->add_header("Access-Control-Allow-Methods", "POST");
    response->add_header("Access-Control-Allow-Headers", "*");
    response->add_header("Allow", "GET, HEAD, POST, PUT, DELETE, TRACE, OPTIONS, PATCH");
    response->add_header("Content-Type", "text/plain;charset=utf-8");

    log_infof("http post uri:%s, method:%s", uri.c_str(), request->method_.c_str());
    if (oper == "publish") {
        std::string data(request->content_body_, request->content_length_);
        ret = whip_publisher(roomId, uid, data, resp_sdp, session_id, err_msg);
        if (ret == 0) {
            response->write(resp_sdp.c_str(), resp_sdp.length());
        } else {
            response->write(err_msg.c_str(), err_msg.length());
        }
    } else if (oper == "unpublish") {
        ret = whip_unpublisher(roomId, uid, err_msg);
        json resp_json = json::object();
        if (ret == 0) {
            resp_json["code"] = 0;
            resp_json["desc"] = err_msg;
        } else {
            resp_json["code"] = 400;
            resp_json["desc"] = err_msg;
        }
        std::string resp_data = resp_json.dump();
        log_infof("http unpublish response:%s", resp_data.c_str());
        response->write(resp_data.c_str(), resp_data.length());
    } else if (oper == "subscribe") {
        if (path_vec.size() != 4) {
            std::string err_msg = "wrong http path";
            response->write(err_msg.c_str(), err_msg.length());
            return;
        }
        std::string remote_uid = path_vec[3];
        std::string data(request->content_body_, request->content_length_);
        ret = whip_subscriber(roomId, uid, remote_uid, data, resp_sdp, session_id, err_msg);
        if (ret == 0) {
            response->write(resp_sdp.c_str(), resp_sdp.length());
        } else {
            response->write(err_msg.c_str(), err_msg.length());
        }
    } else if (oper == "unsubscribe") {
        if (path_vec.size() != 4) {
            std::string err_msg = "wrong http path";
            response->write(err_msg.c_str(), err_msg.length());
            return;
        }
        std::string remote_uid = path_vec[3];
        log_infof("http unsubscribe roomId:%s, uid:%s, remote uid:%s",
            roomId.c_str(), uid.c_str(), remote_uid.c_str());
        ret = whip_unsubscriber(roomId, uid, remote_uid, err_msg);
        json resp_json = json::object();
        if (ret == 0) {
            resp_json["code"] = 0;
            resp_json["desc"] = err_msg;
        } else {
            resp_json["code"] = 400;
            resp_json["desc"] = err_msg;
        }
        std::string resp_data = resp_json.dump();
        log_infof("http unsubscribe response:%s", resp_data.c_str());
        response->write(resp_data.c_str(), resp_data.length());
    } else {
        std::string err_msg = "wrong http path";
        response->write(err_msg.c_str(), err_msg.length());
        return;
    }
    return;
}

/* based on https://github.com/rtcdn/rtcdn-draft */
void rtcdn_publish_response(int code, const std::string& msg, const std::string& resp_sdp,
                        const std::string& session_id, std::shared_ptr<http_response> response) {
    json resp_json = json::object();
    json data_json = json::object();

    resp_json["code"]   = code;
    resp_json["server"] = "cpp_media_srver";
    resp_json["sdp"]    = resp_sdp;
    resp_json["sessionid"] = session_id;

    std::string resp_data = resp_json.dump();
    response->add_header("Vary", "Origin");
    response->add_header("Vary", "Access-Control-Request-Method");
    response->add_header("Vary", "Access-Control-Request-Headers");
    response->add_header("Access-Control-Allow-Origin", "*");
    response->add_header("Access-Control-Allow-Methods", "POST");
    response->add_header("Access-Control-Allow-Headers", "*");
    response->add_header("Allow", "GET, HEAD, POST, PUT, DELETE, TRACE, OPTIONS, PATCH");
    response->add_header("Content-Type", "text/plain;charset=utf-8");
    response->write(resp_data.c_str(), resp_data.length());
}

/* based on https://github.com/rtcdn/rtcdn-draft */
/*
post data: {
             streamurl: 'webrtc://domain/app/stream',
             sdp: string,  // offer sdp
             clientip: string // 可选项， 在实际接入过程中，该请求有可能是服务端发起，为了更好的做就近调度，可以把客户端的ip地址当做参数，如果没有此clientip参数，CDN放可以用请求方的ip来做就近接入。
           }
repsonse data: {
                 code:int,
                 msg:string,
                 data:{
                   sdp:string,   // answer sdp 
                   sessionid:string // 该路推流的唯一id
                 }
               }
*/
void rtcdn_publish_handle(const http_request* request, std::shared_ptr<http_response> response) {
    int ret = 0;
    std::string resp_sdp;
    std::string session_id;
    std::string err_msg = "ok";
    std::string data(request->content_body_, request->content_length_);

    log_infof("rtcnd publish data:%s", data.c_str());
    try {
        json post_json = json::parse(data);
        std::string streamurl = post_json["streamurl"];//'webrtc://domain/app/stream'
        std::string sdp = post_json["sdp"];

        size_t pos = streamurl.find("webrtc://");
        if (pos != 0) {
            err_msg = "no webrtc://";
            rtcdn_publish_response(400, err_msg, resp_sdp, session_id, response);
            return;
        }
        streamurl = streamurl.substr(9);

        std::vector<std::string> path_vec;
        string_split(streamurl, "/", path_vec);
    
        if (path_vec.size() != 3) {
            err_msg = "wrong http path";
            rtcdn_publish_response(400, err_msg, resp_sdp, session_id, response);
            return;
        }

        std::string roomId = path_vec[1];
        std::string uid    = path_vec[2];

        ret = whip_publisher(roomId, uid, sdp, resp_sdp, session_id, err_msg);
        if ((ret == 0) && !resp_sdp.empty() && !session_id.empty()) {
            rtcdn_publish_response(0, "ok", resp_sdp, session_id, response);
        } else {
            rtcdn_publish_response(400, err_msg, resp_sdp, session_id, response);
        }
    }
    catch(const std::exception& e) {
        log_errorf("rtcdn publish exception:%s", e.what());
        rtcdn_publish_response(400, e.what(), resp_sdp, session_id, response);
    }
}

/* based on https://github.com/rtcdn/rtcdn-draft */
/*
request body: {
                streamurl: 'webrtc://domain/app/stream',
                sessionid:string // 推流时返回的唯一id
              }
response body: {
                 code:int,
                 msg:string
               }
*/
void rtcdn_unpublish_handle(const http_request* request, std::shared_ptr<http_response> response) {
    int ret = 0;
    std::string session_id;
    std::string streamurl;
    std::string err_msg = "ok";
    std::string data(request->content_body_, request->content_length_);

    response->add_header("Vary", "Origin");
    response->add_header("Vary", "Access-Control-Request-Method");
    response->add_header("Vary", "Access-Control-Request-Headers");
    response->add_header("Access-Control-Allow-Origin", "*");
    response->add_header("Access-Control-Allow-Methods", "POST");
    response->add_header("Access-Control-Allow-Headers", "*");
    response->add_header("Allow", "GET, HEAD, POST, PUT, DELETE, TRACE, OPTIONS, PATCH");
    response->add_header("Content-Type", "text/plain;charset=utf-8");

    try {
        json post_json = json::parse(data);
        streamurl  = post_json["streamurl"];//'webrtc://domain/app/stream'
        session_id = post_json["sessionid"];
        size_t pos = streamurl.find("webrtc://");
        if (pos != 0) {
            json resp_json = json::object();
            resp_json["code"] = 400;
            resp_json["msg"]  = "no webrtc://";
            std::string resp_data = resp_json.dump();
            response->write(resp_data.c_str(), resp_data.length());
            return;
        }
        streamurl = streamurl.substr(9);

        std::vector<std::string> path_vec;
        string_split(streamurl, "/", path_vec);
    
        if (path_vec.size() != 3) {
            json resp_json = json::object();
            resp_json["code"] = 400;
            resp_json["msg"]  = "wrong http path";
            std::string resp_data = resp_json.dump();
            response->write(resp_data.c_str(), resp_data.length());
            return;
        }

        std::string roomId = path_vec[1];
        std::string uid    = path_vec[2];

        ret = whip_unpublisher(roomId, uid, session_id, err_msg);
        json resp_json = json::object();
        if (ret == 0) {
            resp_json["code"] = 0;
            resp_json["desc"] = err_msg;
        } else {
            resp_json["code"] = 400;
            resp_json["desc"] = err_msg;
        }
        std::string resp_data = resp_json.dump();
        log_infof("http unpublish response:%s", resp_data.c_str());
        response->write(resp_data.c_str(), resp_data.length());
    }
    catch(const std::exception& e) {
        json resp_json = json::object();
        resp_json["code"] = 400;
        resp_json["desc"] = e.what();
        std::string resp_data = resp_json.dump();
        log_infof("http unpublish exception:%s", e.what());
        response->write(resp_data.c_str(), resp_data.length());
    }
}

/* based on https://github.com/rtcdn/rtcdn-draft */
/*
post data:
{
  streamurl: 'webrtc://domain/app/stream',
  sdp: string,  // offer sdp
  clientip: string // 可选项， 在实际接入过程中，拉流请求有可能是服务端发起，为了更好的做就近调度，可以把客户端的ip地址当做参数，如果没有此clientip参数，CDN放可以用请求方的ip来做就近接入。
}
response data:
{
  code: int,
  msg:  string,
  data: {
    sdp:string,   // answer sdp 
    sessionid:string // 该路下行的唯一id
  }
}
*/
void rtcdn_subscribe_handle(const http_request* request, std::shared_ptr<http_response> response) {
    int ret = 0;
    std::string resp_sdp;
    std::string session_id;
    std::string err_msg = "ok";
    std::string data(request->content_body_, request->content_length_);
    std::string roomId;
    std::string uid;
    std::string remote_uid;
    std::string sdp;

    response->add_header("Vary", "Origin");
    response->add_header("Vary", "Access-Control-Request-Method");
    response->add_header("Vary", "Access-Control-Request-Headers");
    response->add_header("Access-Control-Allow-Origin", "*");
    response->add_header("Access-Control-Allow-Methods", "POST");
    response->add_header("Access-Control-Allow-Headers", "*");
    response->add_header("Allow", "GET, HEAD, POST, PUT, DELETE, TRACE, OPTIONS, PATCH");
    response->add_header("Content-Type", "text/plain;charset=utf-8");

    try {
        json post_json = json::parse(data);
        std::string streamurl = post_json["streamurl"];//'webrtc://domain/app/stream'
        sdp = post_json["sdp"];

        size_t pos = streamurl.find("webrtc://");
        if (pos != 0) {
            err_msg = "no webrtc://";
            rtcdn_publish_response(400, err_msg, resp_sdp, session_id, response);
            return;
        }
        streamurl = streamurl.substr(9);

        std::vector<std::string> path_vec;
        string_split(streamurl, "/", path_vec);
    
        if (path_vec.size() != 3) {
            err_msg = "wrong http path";
            rtcdn_publish_response(400, err_msg, resp_sdp, session_id, response);
            return;
        }

        roomId     = path_vec[1];
        remote_uid = path_vec[2];
        uid        = make_uuid();

        ret = whip_subscriber(roomId, uid, remote_uid, sdp, resp_sdp, session_id, err_msg);
        if (ret == 0) {
            rtcdn_publish_response(0, err_msg, resp_sdp, session_id, response);
        } else {
            rtcdn_publish_response(400, err_msg, resp_sdp, session_id, response);
        }
    }
    catch(const std::exception& e) {
        log_errorf("http subscribe exception:%s", e.what());
        rtcdn_publish_response(400, e.what(), resp_sdp, session_id, response);
    }
    
}

/* based on https://github.com/rtcdn/rtcdn-draft */
/*
request body:   {
                  streamurl: 'webrtc://domain/app/stream',
                  sessionid:string // 推流时返回的唯一id
                }
response body:  {
                  code:int,
                  msg:string
                }
*/
void rtcdn_unsubscribe_handle(const http_request* request, std::shared_ptr<http_response> response) {
    int ret = 0;
    std::string session_id;
    std::string streamurl;
    std::string err_msg = "ok";
    std::string data(request->content_body_, request->content_length_);

    response->add_header("Vary", "Origin");
    response->add_header("Vary", "Access-Control-Request-Method");
    response->add_header("Vary", "Access-Control-Request-Headers");
    response->add_header("Access-Control-Allow-Origin", "*");
    response->add_header("Access-Control-Allow-Methods", "POST");
    response->add_header("Access-Control-Allow-Headers", "*");
    response->add_header("Allow", "GET, HEAD, POST, PUT, DELETE, TRACE, OPTIONS, PATCH");
    response->add_header("Content-Type", "text/plain;charset=utf-8");

    try {
        json post_json = json::parse(data);
        streamurl  = post_json["streamurl"];//'webrtc://domain/app/stream'
        session_id = post_json["sessionid"];

        size_t pos = streamurl.find("webrtc://");
        if (pos != 0) {
            json resp_json = json::object();
            resp_json["code"] = 400;
            resp_json["desc"] = "no webrtc://";

            std::string resp_data = resp_json.dump();
            response->write(resp_data.c_str(), resp_data.length());
            return;
        }
        streamurl = streamurl.substr(9);

        std::vector<std::string> path_vec;
        string_split(streamurl, "/", path_vec);
    
        if (path_vec.size() != 3) {
            json resp_json = json::object();
            resp_json["code"] = 400;
            resp_json["desc"] = "wrong http path";

            std::string resp_data = resp_json.dump();
            response->write(resp_data.c_str(), resp_data.length());
            return;
        }

        std::string roomId     = path_vec[1];
        std::string remote_uid = path_vec[2];
        std::string uid        = session_id;

        ret = whip_unsubscriber(roomId, uid, remote_uid, session_id, err_msg);
        json resp_json = json::object();
        if (ret == 0) {
            resp_json["code"] = 0;
            resp_json["desc"] = err_msg;
        } else {
            resp_json["code"] = 400;
            resp_json["desc"] = err_msg;
        }
        std::string resp_data = resp_json.dump();
        log_infof("http unsubscribe(%s:%s) response:%s",
                roomId.c_str(), uid.c_str(), resp_data.c_str());
        response->write(resp_data.c_str(), resp_data.length());
    }
    catch(const std::exception& e) {
        json resp_json = json::object();
        resp_json["code"] = 400;
        resp_json["desc"] = e.what();
        std::string resp_data = resp_json.dump();
        log_infof("http unsubscribe exception:%s", e.what());
        response->write(resp_data.c_str(), resp_data.length());
    }
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
    server_.add_post_handle("/", whip_http_handle);

    /******* start: for rtcdn *******/
    /* https://github.com/rtcdn/rtcdn-draft */
    server_.add_post_handle("/rtc/v1/publish", rtcdn_publish_handle);
    server_.add_post_handle("/rtc/v1/unpublish", rtcdn_unpublish_handle);
    server_.add_post_handle("/rtc/v1/play", rtcdn_subscribe_handle);
    server_.add_post_handle("/rtc/v1/unplay", rtcdn_unsubscribe_handle);

    server_.add_post_handle("/rtc/v1/publish/", rtcdn_publish_handle);
    server_.add_post_handle("/rtc/v1/unpublish/", rtcdn_unpublish_handle);
    server_.add_post_handle("/rtc/v1/play/", rtcdn_subscribe_handle);
    server_.add_post_handle("/rtc/v1/unplay/", rtcdn_unsubscribe_handle);

    /******* start: only for webrtc statics *******/
    server_.add_get_handle("/api/webrtc/publisher", httpapi_webrtc_publisher_handle);
    server_.add_get_handle("/api/webrtc/subscriber", httpapi_webrtc_subscriber_handle);
    server_.add_get_handle("/api/webrtc/room", httpapi_webrtc_room_handle);
    /******* end: only for webrtc statics *******/
}
