#include "room_service.hpp"
#include "utils/logger.hpp"
#include "json.hpp"
#include <unordered_map>
#include <sstream>

using json = nlohmann::json;

static std::unordered_map<std::string, std::shared_ptr<protoo_event_callback>> s_rooms;

std::shared_ptr<protoo_event_callback> GetorCreate_room_service(const std::string& roomId) {
    auto iter = s_rooms.find(roomId);
    if (iter != s_rooms.end()) {
        return iter->second;
    }
    auto room_ptr = std::make_shared<room_service>(roomId);
    s_rooms.insert(std::make_pair(roomId, room_ptr));
    return room_ptr;
}

void remove_room_service(const std::string& roomId) {
    auto iter = s_rooms.find(roomId);
    if (iter == s_rooms.end()) {
        return;
    }

    s_rooms.erase(iter);
    return;
}

room_service::room_service(const std::string& roomId):roomId_(roomId) {

}

room_service::~room_service() {

}

void room_service::on_open() {

}

void room_service::on_failed() {

}

void room_service::on_disconnected() {

}

void room_service::on_close() {

}

void room_service::on_request(const std::string& id, const std::string& method, const std::string& data,
            protoo_request_interface* feedback_p) {
    if (method == "join") {
        handle_join(id, method, data, feedback_p);
    } else if (method == "publish") {
        handle_publish(id, method, data, feedback_p);
    } else {
        log_infof("receive unkown method:%s", method.c_str());
        feedback_p->reject(id, METHOD_ERROR, "unkown method");
    }
}

void room_service::on_response(int err_code, const std::string& err_message, const std::string& id, 
            const std::string& data) {

}

void room_service::on_notification(const std::string& method, const std::string& data) {
    if (method == "close") {
        auto data_json = json::parse(data);
        std::string uid = data_json["uid"];
        std::string roomId = data_json["roomId"];
        if (roomId != roomId) {
            MS_THROW_ERROR("close notification input roomId:%s != roomId(%s) in service", roomId.c_str(), roomId_.c_str());
        }
        auto iter = users_.find(uid);
        if (iter == users_.end()) {
            MS_THROW_ERROR("close notification input uid:%s not found", uid.c_str());
        }
        users_.erase(iter);
        log_infof("user leave uid:%s, roomId:%s", uid.c_str(), roomId.c_str());
    }
}

std::shared_ptr<user_info> room_service::get_user_info(const std::string& uid) {
    std::shared_ptr<user_info> ret_ptr;
    auto iter = users_.find(uid);
    if (iter != users_.end()) {
        ret_ptr = iter->second; 
    }
    return ret_ptr;
}

std::string room_service::get_uid_by_json(json& data_json) {
    std::string uid;

    auto uid_json = data_json.find("uid");
    if (uid_json == data_json.end()) {
        return uid;
    }
    if (!uid_json->is_string()) {
        return uid;
    }

    uid = uid_json->get<std::string>();
    return uid;
}

void room_service::handle_publish(const std::string& id, const std::string& method,
                const std::string& data, protoo_request_interface* feedback_p) {
    std::shared_ptr<user_info> user_ptr;
    json data_json = json::parse(data);

    std::string uid = get_uid_by_json(data_json);
    if (uid.empty()) {
        feedback_p->reject(id, UID_ERROR, "uid field does not exist");
        return;
    }

    user_ptr = get_user_info(uid);
    if (!user_ptr) {
        feedback_p->reject(id, UID_ERROR, "uid doesn't exist");
        return;
    }

    auto sdp_json = data_json.find("sdp");
    if (sdp_json == data_json.end()) {
        feedback_p->reject(id, SDP_ERROR, "roomId does not exist");
        return;
    }
    if (!sdp_json->is_string()) {
        feedback_p->reject(id, SDP_ERROR, "roomId is not string");
        return;
    }

    std::string sdp = sdp_json->get<std::string>();

    json info_json = user_ptr->parse_remote_sdp(sdp);
    //log_infof("publish sdp json:%s", info_json.dump().c_str());

    rtc_media_info& info = user_ptr->parse_remote_media_info(info_json);
    //log_infof("get input sdp dump:\r\n%s", info.dump().c_str());
    rtc_media_info support_info;

    user_ptr->get_support_media_info(info, support_info);

    std::shared_ptr<webrtc_session> session_ptr = std::make_shared<webrtc_session>(RTC_DIRECTION_RECV);
    session_ptr->get_candidates_ip();

    support_info.ice.ice_pwd = session_ptr->get_user_pwd();
    support_info.ice.ice_ufrag = session_ptr->get_username_fragment();

    support_info.finger_print.type = info.finger_print.type;
    finger_print_info fingerprint = session_ptr->get_local_finger_print(info.finger_print.type);
    support_info.finger_print.hash = fingerprint.value;

    CANDIDATE_INFO candidate_data = {
        .foundation = "0",
        .component  = 1,
        .priority   = 2113667327,
        .transport  = "udp",
        .ip         = session_ptr->get_candidates_ip(),
        .port       = session_ptr->get_candidates_port(),
        .type       = "host"
    };

    support_info.candidates.push_back(candidate_data);

    //log_infof("support media info:\r\n%s", support_info.dump().c_str());
    std::string resp_sdp_str = user_ptr->rtc_media_info_2_sdp(support_info);

    user_ptr->publish_session_ptr_ = session_ptr;

    //log_infof("support response sdp:\r\n%s", resp_sdp_str.c_str());

    auto resp_json = json::object();
    resp_json["sdp"] = resp_sdp_str;
    
    std::string resp_data = resp_json.dump();
    //log_infof("publish response data:%s", resp_data.c_str());
    feedback_p->accept(id, resp_data);
    return;
}

void room_service::handle_join(const std::string& id, const std::string& method, const std::string& data,
                protoo_request_interface* feedback_p) {
    std::shared_ptr<user_info> user_ptr;
    json data_json = json::parse(data);

    std::string uid = get_uid_by_json(data_json);
    if (uid.empty()) {
        feedback_p->reject(id, UID_ERROR, "uid field does not exist");
        return;
    }

    user_ptr = get_user_info(uid);
    if (user_ptr.get() != nullptr) {
        feedback_p->reject(id, UID_ERROR, "uid has existed");
        return;
    }

    auto roomId_json = data_json.find("roomId");
    if (roomId_json == data_json.end()) {
        feedback_p->reject(id, ROOMID_ERROR, "roomId does not exist");
        return;
    }
    if (!roomId_json->is_string()) {
        feedback_p->reject(id, ROOMID_ERROR, "roomId is not string");
        return;
    }

    std::string roomId = roomId_json->get<std::string>();
    if (roomId != roomId_) {
        feedback_p->reject(id, ROOMID_ERROR, "roomId error");
        return;
    }

    user_ptr = std::make_shared<user_info>(uid, roomId, feedback_p);

    users_.insert(std::make_pair(uid, user_ptr));

    log_infof("user join uid:%s, roomId:%s", uid.c_str(), roomId.c_str());

    auto resp_json = json::object();
    resp_json["users"] = json::array();

    for (auto user_item : users_) {
        auto user_json = json::object();
        user_json["uid"] = user_item.first;
        resp_json["users"].emplace_back(user_json);
    }
    
    std::string resp_data = resp_json.dump();

    log_infof("join response data:%s", resp_data.c_str());
    feedback_p->accept(id, resp_data);
    return;
}