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
    } else if (method == "publishclose") {
        handle_publish_close(id, method, data, feedback_p);
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

        //notify 'close' other users in room
        notify_userout_to_others(uid);
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

void room_service::rtppacket_publisher2room(rtc_base_session* session, rtc_publisher* publisher, rtp_packet* pkt) {
    //log_infof("room receive rtp packet roomid:%s, publisher type:%s, publisher:%p, pkt dump:\r\n%s",
    //    roomId_.c_str(), publisher->get_media_type().c_str(), publisher, pkt->dump().c_str());

    delete pkt;
    
    return;
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

    if(user_ptr->publish_session_ptr_.get() != nullptr) {
        response_publish(id, feedback_p, REPEAT_PUBLISH_ERROR, "publish repeatedly error", "");
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

    std::shared_ptr<webrtc_session> session_ptr = std::make_shared<webrtc_session>(this, RTC_DIRECTION_RECV, support_info);
    session_ptr->set_remote_finger_print(info.finger_print);
    for (auto media_item : support_info.medias) {
        session_ptr->create_publisher(media_item);
    }

    support_info.ice.ice_pwd = session_ptr->get_user_pwd();
    support_info.ice.ice_ufrag = session_ptr->get_username_fragment();

    support_info.ice.ice_pwd = session_ptr->get_user_pwd();
    support_info.ice.ice_ufrag = session_ptr->get_username_fragment();

    support_info.finger_print.type = info.finger_print.type;
    finger_print_info fingerprint = session_ptr->get_local_finger_print(info.finger_print.type);
    support_info.finger_print.hash = fingerprint.value;

    CANDIDATE_INFO candidate_data = {
        .foundation = "0",
        .component  = 1,
        .transport  = "udp",
        .priority   = 2113667327,
        .ip         = session_ptr->get_candidates_ip(),
        .port       = session_ptr->get_candidates_port(),
        .type       = "host"
    };

    support_info.candidates.push_back(candidate_data);

    /********* suppot publish rtc information is ready ************/
    //log_infof("support media info:\r\n%s", support_info.dump().c_str());
    std::string resp_sdp_str = user_ptr->rtc_media_info_2_sdp(support_info);

    user_ptr->publish_session_ptr_ = session_ptr;
    //log_infof("support response sdp:\r\n%s", resp_sdp_str.c_str());

    response_publish(id, feedback_p, 0, "ok", resp_sdp_str);

    //notify new publish infomation to other users
    std::vector<publisher_info> publishers_vec = session_ptr->get_publishs_information();
    notify_publisher_to_others(uid, publishers_vec);
    return;
}

void room_service::response_publish(const std::string& id, protoo_request_interface* feedback_p,
                    int code, const std::string& desc, const std::string& sdp) {
    auto resp_json = json::object();
    resp_json["sdp"]  = sdp;
    resp_json["code"] = 0;
    resp_json["desc"] = "ok";
    
    std::string resp_data = resp_json.dump();
    log_infof("publish response data:%s", resp_data.c_str());
    feedback_p->accept(id, resp_data);
}

void room_service::handle_publish_close(const std::string& id, const std::string& method,
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

    if(!user_ptr->publish_session_ptr_) {
        response_publish(id, feedback_p, NO_PUBLISH_ERROR, "no publish exist", "");
        return;
    }

    auto mids_Iterjson = data_json.find("mids");
    if (mids_Iterjson == data_json.end()) {
        feedback_p->reject(id, MIDS_ERROR, "mids does not exist");
        return;
    }
    if (!mids_Iterjson->is_array()) {
        feedback_p->reject(id, MIDS_ERROR, "mids is not array");
        return;
    }

    for (auto& mid_item_json : *mids_Iterjson) {
        if (!mid_item_json.is_number()) {
            feedback_p->reject(id, MIDS_ERROR, "mid is not number");
            return;
        }
        int mid = mid_item_json.get<int>();
        user_ptr->publish_session_ptr_->remove_publisher(mid);
    }

    auto resp_json = json::object();
    resp_json["code"] = 0;
    resp_json["desc"] = "ok";
    
    std::string resp_data = resp_json.dump();
    log_infof("publish close response data:%s", resp_data.c_str());
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
    
    notify_userin_to_others(uid);

    std::string resp_data = resp_json.dump();
    log_infof("join response data:%s", resp_data.c_str());
    feedback_p->accept(id, resp_data);

    notify_others_publisher_to_me(uid, user_ptr);
    return;
}

void room_service::notify_userin_to_others(const std::string& uid) {

    for (auto iter : users_) {
        if (uid == iter.first) {
            continue;
        }
        std::shared_ptr<user_info> other_user = iter.second;
        auto resp_json = json::object();
        resp_json["uid"] = uid;
        log_infof("send newuser message: %s", resp_json.dump().c_str());
        other_user->feedback()->notification("newuser", resp_json.dump());
    }
}

void room_service::notify_userout_to_others(const std::string& uid) {
    for (auto iter : users_) {
        if (uid == iter.first) {
            continue;
        }
        std::shared_ptr<user_info> other_user = iter.second;
        auto resp_json = json::object();
        resp_json["uid"] = uid;
        log_infof("send userout message: %s", resp_json.dump().c_str());
        other_user->feedback()->notification("userout", resp_json.dump());
    }
}

void room_service::notify_others_publisher_to_me(const std::string& uid, std::shared_ptr<user_info> me) {
    //std::unordered_map<std::string, std::shared_ptr<user_info>> users_
    for (auto item : users_) {
        if (item.first == uid) {
            continue;
        }
        std::shared_ptr<user_info> other_user = item.second;
        if (!other_user->publish_session_ptr_) {
            continue;
        }
        std::vector<publisher_info> publisher_vec = other_user->publish_session_ptr_->get_publishs_information();
        auto resp_json       = json::object();
        auto publisher_array = json::array();

        for (auto info : publisher_vec) {
            auto publiser_json    = json::object();
            publiser_json["ssrc"] = info.ssrc;
            publiser_json["type"] = info.media_type;
            publisher_array.push_back(publiser_json);
        }

        resp_json["publisers"] = publisher_array;
        resp_json["uid"]       = item.first;

        log_infof("send other publisher message: %s to me(%s)", resp_json.dump().c_str(), uid.c_str());
        me->feedback()->notification("publisher", resp_json.dump());
    }
}

void room_service::notify_publisher_to_others(const std::string& uid, const std::vector<publisher_info>& publisher_vec) {
    for (auto iter : users_) {
        if (uid == iter.first) {
            continue;
        }
        std::shared_ptr<user_info> other_user = iter.second;
        auto resp_json       = json::object();
        auto publisher_array = json::array();

        for (auto info : publisher_vec) {
            auto publiser_json    = json::object();
            publiser_json["ssrc"] = info.ssrc;
            publiser_json["type"] = info.media_type;
            publisher_array.push_back(publiser_json);
        }
        
        resp_json["publisers"] = publisher_array;
        resp_json["uid"]       = uid;

        log_infof("send new publisher message: %s", resp_json.dump().c_str());
        other_user->feedback()->notification("publisher", resp_json.dump());
    }
}