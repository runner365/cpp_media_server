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
    log_infof("room request id:%s, method:%s, data:%s", id.c_str(), method.c_str(), data.c_str());
    if (method == "join") {
        handle_join(id, method, data, feedback_p);
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

void room_service::handle_join(const std::string& id, const std::string& method, const std::string& data,
                protoo_request_interface* feedback_p) {
    json data_json = json::parse(data);
    auto uid_json = data_json.find("uid");
    if (uid_json == data_json.end()) {
        feedback_p->reject(id, UID_ERROR, "uid field does not exist");
        return;
    }
    if (!uid_json->is_string()) {
        feedback_p->reject(id, UID_ERROR, "uid field is not string");
        return;
    }

    std::string uid = uid_json->get<std::string>();

    auto iter = users_.find(uid);
    if (iter != users_.end()) {
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

    std::shared_ptr<user_info> user_ptr = std::make_shared<user_info>(uid, roomId, feedback_p);

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