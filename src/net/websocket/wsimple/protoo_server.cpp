#include "protoo_server.hpp"
#include "net/webrtc/room_service.hpp"
#include "net/websocket/ws_session.hpp"
#include "logger.hpp"
#include "json.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <sstream>

using json = nlohmann::json;

protoo_server::protoo_server(uv_loop_t* loop, uint16_t port):server_(loop, port, this)
{
}

protoo_server::~protoo_server() {

}

void protoo_server::accept(const std::string& id, const std::string& data, void* ws_session) {
    std::stringstream ss;
    websocket_session* session = (websocket_session*)ws_session;

    ss << "{";
    ss << "\"response\":true,";
    ss << "\"id\":" << id << ",";
    ss << "\"ok\":true,";
    ss << "\"data\":";
    ss << data;
    ss << "}";

    if (session && session->get_client()) {
        log_infof("protoo accept:%s", ss.str().c_str());
        if (!session->is_close()) {
            session->get_client()->Send(ss.str().c_str(), ss.str().size(), 1);
        }
    }
}

void protoo_server::reject(const std::string& id, int err_code, const std::string& err, void* ws_session) {
    std::stringstream ss;
    websocket_session* session = (websocket_session*)ws_session;

    ss << "{";
    ss << "\"response\":true,";
    ss << "\"id\":" << id << ",";
    ss << "\"ok\":false,";
    ss << "\"errorCode\":" << err_code << ",";
    ss << "\"errorReason\":" << "\"" << err << "\"";
    ss << "}";
    
    log_infof("response reject:%s", ss.str().c_str());
    if (session && session->get_client()) {
        if (!session->is_close()) {
            session->get_client()->Send(ss.str().c_str(), ss.str().size(), 1);
        }
    }
}

void protoo_server::notification(const std::string& method, const std::string& data, void* ws_session) {
    std::stringstream ss;
    websocket_session* session = (websocket_session*)ws_session;

    ss << "{";
    ss << "\"notification\":true,";
    ss << "\"method\":" << "\"" << method << "\",";
    ss << "\"data\":";
    ss << data;
    ss << "}";

    log_infof("notification: %s", ss.str().c_str());
    if (session && session->get_client()) {
        if (!session->is_close()) {
            session->get_client()->Send(ss.str().c_str(), ss.str().size(), 1);
        }
    }
}

void protoo_server::on_request(websocket_session* session, json& protooBodyJson) {
    auto idObj = protooBodyJson.find("id");
    if (idObj == protooBodyJson.end()) {
        MS_THROW_ERROR("the json has not 'id'.");
    }
    if (!idObj->is_number()) {
        MS_THROW_ERROR("the json id is not 'number'");
        return;
    }
    int id = idObj->get<int>();

    auto methodObj = protooBodyJson.find("method");
    if (methodObj == protooBodyJson.end()) {
        MS_THROW_ERROR("the json has not 'method'");
        return;
    }
    if (!methodObj->is_string()) {
        MS_THROW_ERROR("the json 'method' is not string");
        return;
    }
    std::string method = methodObj->get<std::string>();

    auto dataObj = protooBodyJson.find("data");
    if (dataObj == protooBodyJson.end()) {
        MS_THROW_ERROR("the json has not data");
        return;
    }
    if (!dataObj->is_object()) {
        MS_THROW_ERROR("the json 'data' is not object");
        return;
    }
    std::string body = dataObj->dump();

    if (method == "join") {
        auto roomIdObj = dataObj->find("roomId");
        if (roomIdObj == dataObj->end()) {
            MS_THROW_ERROR("the json has not 'roomId'");
            return;
        }
        if (!roomIdObj->is_string()) {
            MS_THROW_ERROR("the json 'roomId' is not string");
            return;
        }
        roomId_ = roomIdObj->get<std::string>();

        auto uidObj = dataObj->find("uid");
        if (uidObj == dataObj->end()) {
            MS_THROW_ERROR("the json has not 'uid'");
            return;
        }
        if (!uidObj->is_string()) {
            MS_THROW_ERROR("the json 'uid' is not string");
            return;
        }
        uid_ = uidObj->get<std::string>();

        ev_cb_ptr_ = GetorCreate_room_service(roomId_);
    }
    if (!ev_cb_ptr_) {
        MS_THROW_ERROR("the room has not been created, roomId:%s", roomId_.c_str());
        return;
    }
    ev_cb_ptr_->on_request(std::to_string(id), method, body, this, session);
    return;
}

void protoo_server::on_reponse(websocket_session* session, json& protooBodyJson) {
    auto okObj = protooBodyJson.find("ok");
    if (okObj == protooBodyJson.end()) {
        MS_THROW_ERROR("the json has not 'ok'.");
        return;
    }
    if (!okObj->is_boolean()) {
        MS_THROW_ERROR("the json 'ok' is not bool.");
        return;
    }
    bool ok = okObj->get<bool>();

    auto idObj = protooBodyJson.find("id");
    if (idObj == protooBodyJson.end()) {
        MS_THROW_ERROR("the json has not 'id'");
        return;
    }
    if (!idObj->is_number()) {
        MS_THROW_ERROR("the json 'id' is not number");
        return;
    }
    int id = idObj->get<int>();
    int err_code = 0;
    std::string err_message;
    std::string body;

    if (ok) {
        auto dataObj = protooBodyJson.find("data");
        if (dataObj == protooBodyJson.end()) {
            MS_THROW_ERROR("the json has not 'data'");
            return;
        }
        if (!dataObj->is_object()) {
            MS_THROW_ERROR("the json 'data' is not object");
            return;
        }
        body = dataObj->get<std::string>();
    } else {
        auto errorCodeObj = protooBodyJson.find("errorCode");
        if (errorCodeObj == protooBodyJson.end()) {
            MS_THROW_ERROR("the json has not 'errorCode'");
            return;
        }
        if (!errorCodeObj->is_number()) {
            MS_THROW_ERROR("the json 'errorCode' is not number.");
            return;
        }
        err_code = errorCodeObj->get<int>();

        auto errorReasonObj = protooBodyJson.find("errorReason");
        if (errorReasonObj == protooBodyJson.end()) {
            MS_THROW_ERROR("the json has not 'errorReason'");
            return;
        }
        if (!errorReasonObj->is_string()) {
            MS_THROW_ERROR("the json 'errorReason' is not string");
            return;
        }
        err_message = errorReasonObj->get<std::string>();
    }
    if (!ev_cb_ptr_) {
        MS_THROW_ERROR("the room has not been created, roomId:%s", roomId_.c_str());
        return;
    }
    ev_cb_ptr_->on_response(err_code, err_message, std::to_string(id), body);
    return;
}

void protoo_server::on_notification(websocket_session* session, json& protooBodyJson) {
    auto methodObj = protooBodyJson.find("method");
    if (methodObj == protooBodyJson.end()) {
        MS_THROW_ERROR("the json has not 'method'.");
        return;
    }
    if (!methodObj->is_string()) {
        MS_THROW_ERROR("the json 'method' is not string");
        return;
    }
    std::string method = methodObj->get<std::string>();

    auto dataObj = protooBodyJson.find("data");
    if (dataObj == protooBodyJson.end()) {
        MS_THROW_ERROR("the json has not 'data'.");
        return;
    }
    if (!dataObj->is_object()) {
        MS_THROW_ERROR("the json 'data' is not object.");
        return;
    }
    std::string body = dataObj->dump();

    if (!ev_cb_ptr_) {
        MS_THROW_ERROR("the room has not been created, roomId:%s", roomId_.c_str());
        return;
    }
    ev_cb_ptr_->on_notification(method, body);
}

void protoo_server::on_accept(websocket_session* session) {
    log_infof("protoo server accept...");
}

void protoo_server::on_read(websocket_session* session, const char* data, size_t len) {
    std::string body(data, len);
    //log_infof("protoo server read body:%s, len:%lu", body.c_str(), len);

    json protooBodyJson = json::parse(body);

    auto requestObj = protooBodyJson.find("request");
    if (requestObj != protooBodyJson.end()) {
        if (!requestObj->is_boolean()) {
            MS_THROW_ERROR("request is not bool type.");
            return;
        }
        if (!requestObj->get<bool>()) {
            MS_THROW_ERROR("request is not bool type.");
            return;
        }

        try
        {
            on_request(session, protooBodyJson);
        }
        catch(const std::exception& e)
        {
            log_infof("handle request exception:%s", e.what());
            MS_THROW_ERROR("handle request exception:%s", e.what());
            return;
        }
        return;
    }

    auto responseObj = protooBodyJson.find("response");
    if (responseObj != protooBodyJson.end()) {
        if (!responseObj->is_boolean()) {
            MS_THROW_ERROR("response is not bool type.");
            return;
        }
        
        if (!responseObj->get<bool>()) {
            MS_THROW_ERROR("response is not bool type.");
            return;
        }

        try
        {
            on_reponse(session, protooBodyJson);
        }
        catch(const std::exception& e)
        {
            log_infof("handle response exception:%s", e.what());
            MS_THROW_ERROR("handle response exception:%s", e.what());
        }
        return;
    }

    auto notificationObj = protooBodyJson.find("notification");
    if (notificationObj != protooBodyJson.end()) {
        if (!notificationObj->is_boolean()) {
            MS_THROW_ERROR("notification is not bool type.");
            return;
        }
        
        if (!notificationObj->get<bool>()) {
            MS_THROW_ERROR("notification is not bool type.");
            return;
        }

        try
        {
            on_notification(session, protooBodyJson);
        }
        catch(const std::exception& e)
        {
            log_infof("handle notification exception:%s", e.what());
            MS_THROW_ERROR("handle notification exception:%s", e.what());
        }
        return;
    }

    return;
}

void protoo_server::on_close(websocket_session* session) {
    log_infof("protoo server on close, roomId:%s, uid:%s",
        roomId_.c_str(), uid_.c_str());

    if (uid_.empty() || roomId_.empty()) {
        log_infof("user has not joined.");
        return;
    }
    auto body_json = json::object();
    body_json["uid"] = uid_;
    body_json["roomId"] = roomId_;

    try
    {
        ev_cb_ptr_->on_notification("close", body_json.dump());
    }
    catch(const std::exception& e)
    {
        log_errorf("close exception:%s", e.what());
    }
}