#include "room_service.hpp"
#include "rtc_subscriber.hpp"
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
    } else if (method == "unpublish") {
        handle_unpublish(id, method, data, feedback_p);
    } else if (method == "subscribe") {
        handle_subscribe(id, method, data, feedback_p);
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
/*
    std::string media_type;
    uint32_t ssrc;
    int mid;
    std::string pid;
*/
std::vector<publisher_info> room_service::get_publishers_info_by_json(const json& publishers_json) {
    std::vector<publisher_info> ret_publishers;

    if (!publishers_json.is_array()) {
        return ret_publishers;
    }

    for (auto item : publishers_json) {
        publisher_info info;
        auto pid_iter = item.find("pid");
        if (pid_iter == item.end()) {
            return ret_publishers;
        }
        if (!pid_iter->is_string()) {
            return ret_publishers;
        }
        info.pid = pid_iter->get<std::string>();

        auto mediatype_iter = item.find("type");
        if (mediatype_iter == item.end()) {
            return ret_publishers;
        }
        if (!mediatype_iter->is_string()) {
            return ret_publishers;
        }
        info.media_type = mediatype_iter->get<std::string>();

        auto ssrc_iter = item.find("ssrc");
        if (ssrc_iter == item.end()) {
            return ret_publishers;
        }
        if (!ssrc_iter->is_number()) {
            return ret_publishers;
        }
        info.ssrc = (uint32_t)ssrc_iter->get<int>();

        auto mid_iter = item.find("mid");
        if (mid_iter == item.end()) {
            return ret_publishers;
        }
        if (!mid_iter->is_number()) {
            return ret_publishers;
        }
        info.mid = mid_iter->get<int>();

        ret_publishers.push_back(info);
    }
    return ret_publishers;
}

void room_service::insert_subscriber(const std::string& publisher_id, std::shared_ptr<rtc_subscriber> subscriber_ptr) {
    auto subs_map_it = pid2subscribers_.find(publisher_id);
    if (subs_map_it == pid2subscribers_.end()) {
        log_infof("the publisher id:%s doesn't exist, we need to create one", publisher_id.c_str());
        SUBSCRIBER_MAP s_map;
        s_map[subscriber_ptr->get_publisher_id()] = subscriber_ptr;
        pid2subscribers_[publisher_id] = s_map;
        return;
    }
    std::string sid = subscriber_ptr->get_subscirber_id();
    auto sub_it = subs_map_it->second.find(sid);
    if (sub_it != subs_map_it->second.end()) {
        MS_THROW_ERROR("the subscriber id:%s has already existed", subscriber_ptr->get_subscirber_id().c_str());
    }
    subs_map_it->second[sid] = subscriber_ptr;
    log_infof("the subscriber id(%s) is insert in publisher id(%s) of roomid(%s), uid:%s, remote uid:%s",
        sid.c_str(), publisher_id.c_str(), roomId_.c_str(),
        subscriber_ptr->get_uid().c_str(), subscriber_ptr->get_remote_uid().c_str());
}

void room_service::on_rtppacket_publisher2room(rtc_base_session* session, rtc_publisher* publisher, rtp_packet* pkt) {
    std::string publish_id = publisher->get_publisher_id();
    std::string mediatype = publisher->get_media_type();

    auto subs_map_it = pid2subscribers_.find(publish_id);
    if (subs_map_it != pid2subscribers_.end()) {
        for (auto subscribe_item : subs_map_it->second) {
            subscribe_item.second->send_rtp_packet(roomId_, mediatype, publish_id, pkt);
        }
    }
    delete pkt;
    
    return;
}

void room_service::on_request_keyframe(const std::string& pid, const std::string& sid, uint32_t media_ssrc) {
    log_infof("request keyframe publisherid:%s, subscriberid:%s, media ssrc:%u", pid.c_str(), sid.c_str(), media_ssrc);

    for(auto user : users_) {
        auto session_ptr = user.second->publish_session_ptr_;
        if (!session_ptr) {
            continue;
        }
        auto publisher_ptr = session_ptr->get_publisher(media_ssrc);
        if (!publisher_ptr) {
            continue;
        }
        if (publisher_ptr->get_publisher_id() == pid) {
            log_infof("request keyframe from uid(%s), pid(%s)", user.first.c_str(), pid.c_str());
            publisher_ptr->request_keyframe(media_ssrc);
            break;
        }
    }
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

    std::shared_ptr<webrtc_session> session_ptr = std::make_shared<webrtc_session>(roomId_, uid,
                                                    this, RTC_DIRECTION_RECV, support_info);
    session_ptr->set_remote_finger_print(info.finger_print);
    for (auto media_item : support_info.medias) {
        session_ptr->create_publisher(media_item);
    }

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
    log_infof("publish support media info:\r\n%s", support_info.dump().c_str());
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

void room_service::handle_unpublish(const std::string& id, const std::string& method,
                        const std::string& data, protoo_request_interface* feedback_p) {
    std::shared_ptr<user_info> user_ptr;
    json data_json = json::parse(data);

    log_infof("handle publish close: %s", data.c_str());
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

    std::vector<publisher_info> all_publishers_vec = user_ptr->publish_session_ptr_->get_publishs_information();
    std::vector<publisher_info> unpublish_vec;
    for (auto& mid_item_json : *mids_Iterjson) {
        if (!mid_item_json.is_object()) {
            feedback_p->reject(id, MIDS_ERROR, "mids is not json object");
            return;
        }
        auto mid_iter = mid_item_json.find("mid");
        if (mid_iter == mid_item_json.end()) {
            feedback_p->reject(id, MIDS_ERROR, "haven't mid field");
            return;
        }
        if (!mid_iter->is_number()) {
            feedback_p->reject(id, MIDS_ERROR, "mid is not number");
            return;
        }
        int mid = mid_iter->get<int>();

        auto type_iter = mid_item_json.find("type");
        if (type_iter == mid_item_json.end()) {
            feedback_p->reject(id, MIDS_ERROR, "haven't type field");
            return;
        }
        if (!type_iter->is_string()) {
            feedback_p->reject(id, MIDS_ERROR, "type is not number");
            return;
        }
        std::string media_type = type_iter->get<std::string>();

        int ret = user_ptr->publish_session_ptr_->remove_publisher(mid, media_type);
        if (ret != 0) {
            feedback_p->reject(id, ret, "remove publish error");
            return;
        }

        for (auto publisher_item : all_publishers_vec) {
            if (publisher_item.mid == mid) {
                unpublish_vec.push_back(publisher_item);
                break;
            }
        }
    }

    if (user_ptr->publish_session_ptr_->get_publisher_count() == 0) {
        user_ptr->publish_session_ptr_->close_session();
        user_ptr->reset_media_info();
        user_ptr->publish_session_ptr_ = nullptr;
    }

    auto resp_json = json::object();
    resp_json["code"] = 0;
    resp_json["desc"] = "ok";
    
    std::string resp_data = resp_json.dump();
    log_infof("publish close response data:%s", resp_data.c_str());
    feedback_p->accept(id, resp_data);

    //notify unpublish to others
    notify_unpublisher_to_others(uid, unpublish_vec);

    return;
}

void room_service::handle_subscribe(const std::string& id, const std::string& method,
                const std::string& data, protoo_request_interface* feedback_p) {
    log_infof("subscribe data: %s", data.c_str());
    std::shared_ptr<user_info> user_ptr;
    std::shared_ptr<user_info> remote_user_ptr;
    std::string remote_uid;
    json data_json = json::parse(data);

    std::string uid = get_uid_by_json(data_json);
    if (uid.empty()) {
        feedback_p->reject(id, UID_ERROR, "subscribe uid field does not exist");
        return;
    }

    user_ptr = get_user_info(uid);
    if (!user_ptr) {
        feedback_p->reject(id, UID_ERROR, "subscribe uid doesn't exist");
        return;
    }

    remote_uid = data_json["remoteUid"];
    remote_user_ptr = get_user_info(remote_uid);
    if (!remote_user_ptr) {
        feedback_p->reject(id, UID_ERROR, "subscribe uid doesn't exist");
        return;
    }

    auto publishers_iter = data_json.find("publishers");
    if (publishers_iter == data_json.end()) {
        feedback_p->reject(id, SUBSCRIBE_JSON_ERROR, "publishers doesn't exist");
        return;
    }
    std::vector<publisher_info> publishers = get_publishers_info_by_json(*publishers_iter);

    auto sdp_iter = data_json.find("sdp");
    if (sdp_iter == data_json.end()) {
        feedback_p->reject(id, SUBSCRIBE_JSON_ERROR, "sdp doesn't exist");
        return;
    }
    if (!sdp_iter->is_string()) {
        feedback_p->reject(id, SUBSCRIBE_JSON_ERROR, "sdp isn't string");
        return;
    }
    std::string sdp = sdp_iter->get<std::string>();

    json info_json = user_ptr->parse_remote_sdp(sdp);
    log_infof("subscribe sdp json:%s", info_json.dump().c_str());

    rtc_media_info& info = user_ptr->parse_remote_media_info(info_json);
    log_infof("get subscribe input media info:\r\n%s", info.dump().c_str());

    rtc_media_info support_info;
    user_ptr->get_support_media_info(info, support_info);

    /******************* add ssrc info from publisher *************************/
    std::shared_ptr<webrtc_session> publish_session_ptr = remote_user_ptr->publish_session_ptr_;
    for (auto& media : support_info.medias) {
        std::string pid;
        for (auto info : publishers) {
            if (info.mid == media.mid) {
                pid = info.pid;
                break;
            }
        }
        std::shared_ptr<rtc_publisher> publisher_ptr = publish_session_ptr->get_publisher(pid);
        MEDIA_RTC_INFO info = publisher_ptr->get_media_info();
        media.ssrc_infos    = info.ssrc_infos;
        media.ssrc_groups   = info.ssrc_groups;
        media.msid          = info.msid;
        media.publisher_id  = pid;
    }

    /********************** create subscribe session **************************/
    std::shared_ptr<webrtc_session> session_ptr = std::make_shared<webrtc_session>(roomId_, uid, this,
                                                            RTC_DIRECTION_SEND, support_info);
    session_ptr->set_remote_finger_print(info.finger_print);
    for (auto media_item : support_info.medias) {
        auto subscirber_ptr = session_ptr->create_subscriber(remote_uid, media_item, media_item.publisher_id, this);
        insert_subscriber(media_item.publisher_id, subscirber_ptr);
    }

    /********************** add ice and finger print **************************/
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
    log_infof("get subscribe support media info:\r\n%s", support_info.dump().c_str());

    std::string resp_sdp_str = user_ptr->rtc_media_info_2_sdp(support_info);
    user_ptr->subscribe_session_ptr_ = session_ptr;

    auto resp_json = json::object();
    resp_json["code"] = 0;
    resp_json["desc"] = "ok";
    resp_json["sdp"]  = resp_sdp_str;
    
    std::string resp_data = resp_json.dump();
    log_infof("subscirbe response data:%s", resp_data.c_str());
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
        other_user->feedback()->notification("userin", resp_json.dump());
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
            publiser_json["mid"]  = info.mid;
            publiser_json["pid"]  = info.pid;
            publisher_array.push_back(publiser_json);
        }

        resp_json["publishers"] = publisher_array;
        resp_json["uid"]        = item.first;

        log_infof("send other publisher message: %s to me(%s)", resp_json.dump().c_str(), uid.c_str());
        me->feedback()->notification("publish", resp_json.dump());
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
            publiser_json["mid"]  = info.mid;
            publiser_json["pid"]  = info.pid;
            publisher_array.push_back(publiser_json);
        }
        
        resp_json["publishers"] = publisher_array;
        resp_json["uid"]        = uid;

        log_infof("send publish message: %s", resp_json.dump().c_str());
        other_user->feedback()->notification("publish", resp_json.dump());
    }
}

void room_service::notify_unpublisher_to_others(const std::string& uid, const std::vector<publisher_info>& publisher_vec) {
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
            publiser_json["mid"]  = info.mid;
            publiser_json["pid"]  = info.pid;
            publisher_array.push_back(publiser_json);
        }
        
        resp_json["unpublisers"] = publisher_array;
        resp_json["uid"]       = uid;

        log_infof("send unpublish message: %s", resp_json.dump().c_str());
        other_user->feedback()->notification("unpublish", resp_json.dump());
    }
}