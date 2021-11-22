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
    } else if (method == "unsubscribe") {
        handle_unsubscribe(id, method, data, feedback_p);
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
        log_infof("the publisher id:%s is to be subscribed firstly, we need to create one", publisher_id.c_str());
        SUBSCRIBER_MAP s_map;
        s_map[subscriber_ptr->get_subscirber_id()] = subscriber_ptr;
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

void room_service::on_unpublish(const std::string& pid) {
    auto iter = pid2subscribers_.find(pid);
    if (iter == pid2subscribers_.end()) {
        log_warnf("unsubscribe fail to get publisher id:%s", pid.c_str());
        return;
    }
    pid2subscribers_.erase(iter);
    log_warnf("unpublish remove publisher id:%s", pid.c_str());
}

void room_service::on_unsubscribe(const std::string& pid, const std::string& sid) {
    auto iter = pid2subscribers_.find(pid);
    if (iter == pid2subscribers_.end()) {
        log_warnf("unsubscribe fail to get publisher id:%s", pid.c_str());
        return;
    }
    auto subscriber_iter = iter->second.find(sid);
    if (subscriber_iter == iter->second.end()) {
        log_warnf("unsubscribe fail to get subscriber id:%s", sid.c_str());
        return;
    }
    iter->second.erase(subscriber_iter);
    log_warnf("unsubscribe remove subscriber id:%s from publisher id:%s",
        sid.c_str(), pid.c_str());
}

void room_service::on_request_keyframe(const std::string& pid, const std::string& sid, uint32_t media_ssrc) {
    log_infof("request keyframe publisherid:%s, subscriberid:%s, media ssrc:%u", pid.c_str(), sid.c_str(), media_ssrc);

    for(auto user : users_) {
        for (auto session_item : user.second->publish_sessions_) {
            auto session_ptr = session_item.second;

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
    log_debugf("publish sdp json:%s", info_json.dump().c_str());

    rtc_media_info& info = user_ptr->parse_remote_media_info(info_json);
    log_debugf("get input sdp dump:\r\n%s", info.dump().c_str());
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

    user_ptr->publish_sessions_[session_ptr->get_id()] = session_ptr;
    //log_infof("support response sdp:\r\n%s", resp_sdp_str.c_str());

    response_publish(id, feedback_p, 0, "ok", resp_sdp_str, session_ptr->get_id());

    //notify new publish infomation to other users
    std::vector<publisher_info> publishers_vec = session_ptr->get_publishs_information();
    notify_publisher_to_others(uid, session_ptr->get_id(), publishers_vec);
    return;
}

void room_service::response_publish(const std::string& id, protoo_request_interface* feedback_p,
                    int code, const std::string& desc, const std::string& sdp, const std::string& pc_id) {
    auto resp_json = json::object();
    
    resp_json["code"] = 0;
    resp_json["desc"] = desc;
    resp_json["pcid"] = pc_id;
    resp_json["sdp"]  = sdp;

    std::string resp_data = resp_json.dump();
    log_infof("publish response data:%s", resp_data.c_str());
    feedback_p->accept(id, resp_data);
}

void room_service::handle_unpublish(const std::string& id, const std::string& method,
                        const std::string& data, protoo_request_interface* feedback_p) {
    std::shared_ptr<user_info> user_ptr;
    json data_json = json::parse(data);

    log_infof("unpublish data: %s", data.c_str());
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

    auto pc_id_json = data_json.find("pcid");
    if (pc_id_json == data_json.end()) {
        return;
    }
    if (!pc_id_json->is_string()) {
        return;
    }

    std::string pc_id = pc_id_json->get<std::string>();
    auto session_iter = user_ptr->publish_sessions_.find(pc_id);
    if (session_iter == user_ptr->publish_sessions_.end()) {
        feedback_p->reject(id, UID_ERROR, "peer connection id doesn't exist");
        return;
    }
    auto unpublish_vec = session_iter->second->get_publishs_information();
    user_ptr->publish_sessions_.erase(session_iter);

    for(auto unpublish_item : unpublish_vec) {
        auto erase_iter = pid2subscribers_.find(unpublish_item.pid);
        if (erase_iter != pid2subscribers_.end()) {
            log_infof("unpublish remove publishid:%s, subscriber size:%lu",
                unpublish_item.pid.c_str(), erase_iter->second.size());
            pid2subscribers_.erase(erase_iter);
        }
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

void room_service::handle_unsubscribe(const std::string& id, const std::string& method,
                const std::string& data, protoo_request_interface* feedback_p) {
    log_infof("unsubscribe data: %s", data.c_str());
    std::shared_ptr<user_info> user_ptr;
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

    //remove pcid frome user_info's publisher_sessions: key<remote_pcid>, value<webrtc_session>
    std::string pcid = data_json["pcid"];
    auto subscriber_session_iter = user_ptr->subscribe_sessions_.find(pcid);
    if (subscriber_session_iter == user_ptr->subscribe_sessions_.end()) {
        log_infof("unsubscribe fail to get remote pcid:%s", pcid.c_str());
        feedback_p->reject(id, UID_ERROR, "unsubscribe's pcid doesn't exist");
        return;
    }
    user_ptr->subscribe_sessions_.erase(subscriber_session_iter);
    log_infof("unsubscribe remote pcid:%s, uid:%s", pcid.c_str(), uid.c_str());

    auto publishers_iter = data_json.find("publishers");
    if (publishers_iter == data_json.end()) {
        feedback_p->reject(id, SUBSCRIBE_JSON_ERROR, "publishers doesn't exist");
        return;
    }
    std::vector<publisher_info> publishers = get_publishers_info_by_json(*publishers_iter);

    for (auto info : publishers) {
        //pid2subscribers_;//key: publisher_id, value: rtc_subscriber
        auto iter = pid2subscribers_.find(info.pid);
        if (iter == pid2subscribers_.end()) {
            continue;
        }
        log_infof("remove publisher id:%s, subscribers size:%lu",
            info.pid.c_str(), iter->second.size());
        iter->second.clear();
        pid2subscribers_.erase(iter);
    }

    auto resp_json = json::object();
    resp_json["code"] = 0;
    resp_json["desc"] = "ok";
    std::string resp_data = resp_json.dump();
    feedback_p->accept(id, resp_data);

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
        feedback_p->reject(id, UID_ERROR, "publisher uid doesn't exist");
        return;
    }
    std::string remote_pcid = data_json["remotePcId"];
    auto remote_session_iter = remote_user_ptr->publish_sessions_.find(remote_pcid);
    if (remote_session_iter == remote_user_ptr->publish_sessions_.end()) {
        feedback_p->reject(id, UID_ERROR, "publisher pcid doesn't exist");
        return;
    }
    std::shared_ptr<webrtc_session> remote_session_ptr = remote_session_iter->second;

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
    for (auto& media : support_info.medias) {
        std::string pid;
        for (auto info : publishers) {
            if (info.media_type == media.media_type) {
                auto publisher_ptr =  remote_session_ptr->get_publisher(info.mid);
                if (!publisher_ptr) {
                    log_errorf("fail to get publisher by mid:%d", info.mid);
                    feedback_p->reject(id, SUBSCRIBE_JSON_ERROR, "publishers doesn't exist");
                    return;
                }
                
                media.ssrc_infos    = publisher_ptr->get_media_info().ssrc_infos;
                media.ssrc_groups   = publisher_ptr->get_media_info().ssrc_groups;
                media.msid          = publisher_ptr->get_media_info().msid;
                media.publisher_id  = publisher_ptr->get_publisher_id();
                media.src_mid       = info.mid;
                break;
            }
        }
    }

    std::shared_ptr<webrtc_session> session_ptr;
    /********************** create subscribe session **************************/
    session_ptr = std::make_shared<webrtc_session>(roomId_, uid, this,
                                            RTC_DIRECTION_SEND, support_info);
    session_ptr->set_remote_finger_print(info.finger_print);
    user_ptr->subscribe_sessions_[session_ptr->get_id()] = session_ptr;
    log_infof("subscribe session id:%s", session_ptr->get_id().c_str());

    /***************** create subscribers for the publisher *******************/
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

    auto resp_json = json::object();
    resp_json["code"] = 0;
    resp_json["desc"] = "ok";
    resp_json["pcid"] = session_ptr->get_id();
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
        auto resp_json       = json::object();
        auto publisher_array = json::array();
        bool publish_found   = false;
        std::string pc_id;

        for (auto session_item : other_user->publish_sessions_) {
            auto session_ptr = session_item.second;

            std::vector<publisher_info> publisher_vec = session_ptr->get_publishs_information();

            for (auto info : publisher_vec) {
                publish_found = true;
                pc_id = session_ptr->get_id();
                auto publiser_json    = json::object();
                publiser_json["ssrc"] = info.ssrc;
                publiser_json["type"] = info.media_type;
                publiser_json["mid"]  = info.mid;
                publiser_json["pid"]  = info.pid;
                publisher_array.push_back(publiser_json);
            }
        }


        if (publish_found) {
            resp_json["publishers"] = publisher_array;
            resp_json["uid"]        = item.first;
            resp_json["pcid"]       = pc_id;

            log_infof("send other publisher message: %s to me(%s)", resp_json.dump().c_str(), uid.c_str());
            me->feedback()->notification("publish", resp_json.dump());
        }
    }
}

void room_service::notify_publisher_to_others(const std::string& uid, const std::string& pc_id,
                                            const std::vector<publisher_info>& publisher_vec) {
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
        resp_json["pcid"]       = pc_id;

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
        
        resp_json["publishers"] = publisher_array;
        resp_json["uid"]       = uid;

        log_infof("send unpublish message: %s", resp_json.dump().c_str());
        other_user->feedback()->notification("unpublish", resp_json.dump());
    }
}