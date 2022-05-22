#include "room_service.hpp"
#include "rtc_subscriber.hpp"
#include "net/http/http_common.hpp"
#include "utils/logger.hpp"
#include "utils/av/media_stream_manager.hpp"
#include "json.hpp"
#include "utils/uuid.hpp"
#include <unordered_map>
#include <sstream>

using json = nlohmann::json;

extern boost::asio::io_context& get_global_io_context();

static std::unordered_map<std::string, std::shared_ptr<room_service>> s_rooms;

static uint32_t s_live_video_ssrc = 10000;
static uint32_t s_live_audio_ssrc = 20000;

static uint32_t make_live_video_ssrc() {
    uint32_t ssrc = ++s_live_video_ssrc;

    if ((ssrc % 10000) == 0) {
        s_live_video_ssrc = 10000;
        return ++s_live_video_ssrc;
    }

    return ssrc;
}

static uint32_t make_live_audio_ssrc() {
    uint32_t ssrc = ++s_live_audio_ssrc;

    if ((ssrc % 10000) == 0) {
        s_live_audio_ssrc = 20000;
        return ++s_live_audio_ssrc;
    }

    return ssrc;
}

std::shared_ptr<room_service> GetorCreate_room_service(const std::string& roomId) {
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

bool room_has_rtc_uid(const std::string& roomId, const std::string& uid) {
    auto iter = s_rooms.find(roomId);
    if (iter == s_rooms.end()) {
        return false;
    }

    std::shared_ptr<room_service> room_ptr = iter->second;
    if (!room_ptr) {
        return false;
    }

    return room_ptr->has_rtc_user(uid);
}

int whip_publisher(const std::string& roomId, const std::string& uid, const std::string& sdp,
                std::string& resp_sdp, std::string& session_id, std::string& err_msg) {
    std::shared_ptr<room_service> room_ptr = GetorCreate_room_service(roomId);
    if (!room_ptr) {
        err_msg = "get or create roomid ";
        err_msg += roomId;
        err_msg += " error";
        log_errorf("%s", err_msg.c_str());
        return -1;
    }

    return room_ptr->handle_http_publish(uid, sdp, resp_sdp, session_id, err_msg);
}

int whip_unpublisher(const std::string& roomId, const std::string& uid, std::string& err_msg) {
    auto iter = s_rooms.find(roomId);
    if (iter == s_rooms.end()) {
        err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }

    std::shared_ptr<room_service> room_ptr = iter->second;
    if (!room_ptr) {
        std::string err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }

    return room_ptr->handle_http_unpublish(uid, err_msg);
}

int whip_unpublisher(const std::string& roomId, const std::string& uid, const std::string& sessionid, std::string& err_msg) {
    auto iter = s_rooms.find(roomId);
    if (iter == s_rooms.end()) {
        err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }

    std::shared_ptr<room_service> room_ptr = iter->second;
    if (!room_ptr) {
        std::string err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }

    return room_ptr->handle_http_unpublish(uid, sessionid, err_msg);
}

int whip_subscriber(const std::string& roomId, const std::string& remote_uid,
            const std::string& data, std::string& resp_sdp, std::string& session_id, std::string& err_msg) {
    auto iter = s_rooms.find(roomId);
    if (iter == s_rooms.end()) {
        err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }

    std::shared_ptr<room_service> room_ptr = iter->second;
    if (!room_ptr) {
        std::string err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }

    std::string uid = make_uuid();
    return room_ptr->handle_http_subscribe(uid, remote_uid, data, resp_sdp, session_id, err_msg);
}

int whip_unsubscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid,
                    std::string& err_msg) {
    auto iter = s_rooms.find(roomId);
    if (iter == s_rooms.end()) {
        err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }

    std::shared_ptr<room_service> room_ptr = iter->second;
    if (!room_ptr) {
        err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }
    
    return room_ptr->handle_http_unsubscribe(uid, remote_uid, err_msg);
}

int whip_unsubscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid,
                    const std::string& sessionid, std::string& err_msg) {
    auto iter = s_rooms.find(roomId);
    if (iter == s_rooms.end()) {
        err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }

    std::shared_ptr<room_service> room_ptr = iter->second;
    if (!room_ptr) {
        err_msg = "get room error by roomid ";
        err_msg += roomId;
        return -1;
    }
    
    return room_ptr->handle_http_unsubscribe(uid, remote_uid, sessionid, err_msg);
}

int get_room_statics(json& data_json) {
    data_json["rtc_list"] = json::array();
    data_json["live_list"] = json::array();

    for (auto& room_item : s_rooms) {
        std::string roomId = room_item.first;
        std::shared_ptr<room_service> room_ptr = room_item.second;

        for (auto rtc_user_item : room_ptr->users_) {
            std::string uid = rtc_user_item.first;
            std::shared_ptr<user_info> user_ptr = rtc_user_item.second;

            json user_json = json::object();
            user_json["uid"] = uid;
            user_json["publishers"]  = user_ptr->publish_sessions_.size();;
            user_json["subscribers"] = user_ptr->subscribe_sessions_.size();;
            data_json["rtc_list"].emplace_back(user_json);
        }

        for (auto& live_user_item : room_ptr->live_users_) {
            std::string uid = live_user_item.first;

            json user_json = json::object();
            user_json["uid"] = uid;
            data_json["live_list"].emplace_back(user_json);
        }
    }
    return 0;
}

int get_subscriber_statics(const std::string& roomId, const std::string& uid, json& data_json) {
    auto iter = s_rooms.find(roomId);
    if (iter == s_rooms.end()) {
        log_infof("the roomid(%s) does not exist", roomId.c_str());
        return -1;
    }
    std::shared_ptr<room_service> room_ptr = iter->second;
    if (!room_ptr) {
        log_errorf("the room(%s) is null", roomId.c_str());
        return -1;
    }

    int count = 0;

    data_json["list"] = json::array();
    for (auto item : room_ptr->pid2subscribers_) {
        for (auto subscriber_item : item.second) {
            std::shared_ptr<rtc_subscriber> subscriber_ptr = subscriber_item.second;
            if (subscriber_ptr->get_uid() == uid) {
                json subscirber_data = json::object();
                subscriber_ptr->get_statics(subscirber_data);
                count++;
                data_json["list"].emplace_back(subscirber_data);   
            }
        }
    }
    data_json["count"] = count;
    return 0;
}

int get_publisher_statics(const std::string& roomId, const std::string& uid, json& data_json) {
    auto iter = s_rooms.find(roomId);
    if (iter == s_rooms.end()) {
        log_infof("the roomid(%s) does not exist", roomId.c_str());
        return -1;
    }
    std::shared_ptr<room_service> room_ptr = iter->second;
    if (!room_ptr) {
        log_errorf("the room(%s) is null", roomId.c_str());
        return -1;
    }

    std::shared_ptr<user_info> rtc_user_ptr = room_ptr->get_rtc_user(uid);
    if (rtc_user_ptr.get() != 0) {
        auto iter = rtc_user_ptr->publish_sessions_.begin();
        if (iter == rtc_user_ptr->publish_sessions_.end()) {
            data_json["count"] = 0;
            return 0;
        }

        std::shared_ptr<webrtc_session> session_ptr = iter->second;
        if (!session_ptr) {
            data_json["count"] = 0;
            return 0;
        }
        if (session_ptr->ssrc2publishers_.size() == 0) {
            data_json["count"] = 0;
            return 0;
        }
        
        data_json["list"]  = json::array();
        int count = 0;
        for (auto item : session_ptr->ssrc2publishers_) {
            if (item.first == item.second->get_rtx_ssrc()) {
                continue;
            }
            json publisher_data = json::object();

            item.second->get_statics(publisher_data);
            data_json["list"].emplace_back(publisher_data);
            count++;
        }
        data_json["count"] = count;
    } else {
        log_infof("the rtc user(%s) does not exist", uid.c_str());
    }

    return 0;
}

webrtc_stream_manager_callback::webrtc_stream_manager_callback() {

}

webrtc_stream_manager_callback::~webrtc_stream_manager_callback() {

}

void webrtc_stream_manager_callback::on_publish(const std::string& app, const std::string& streamname) {
    log_infof("webrtc on publish app:%s, streamname:%s", app.c_str(), streamname.c_str());
}

void webrtc_stream_manager_callback::on_unpublish(const std::string& app, const std::string& streamname) {
    log_infof("webrtc on unpublish app(roomid):%s, streamname(uid):%s",
            app.c_str(), streamname.c_str());
    auto room_ptr = GetorCreate_room_service(app);
    if (!room_ptr) {
        log_errorf("fail to get room by roomid:%s", app.c_str());
        return;
    }
    room_ptr->remove_live_user(app, streamname);
}

static webrtc_stream_manager_callback s_webrtc_callback;

void init_webrtc_stream_manager_callback() {
    media_stream_manager::add_stream_callback(&s_webrtc_callback);
}

room_service::room_service(const std::string& roomId):timer_interface(get_global_io_context(), 2000)
                                                      , roomId_(roomId) {
    start_timer();
}

room_service::~room_service() {
    stop_timer();
    users_.clear();
    live_users_.clear();
    pid2subscribers_.clear();
}

void room_service::on_timer() {
    int64_t now_ms = now_millisec();
    for(auto iter = live_users_.begin();
        iter != live_users_.end();
        )
    {
        int64_t diff_t = now_ms - iter->second->active_last_ms();
        if (diff_t > 20*1000) {
            log_infof("live user(%s) is timeout", iter->second->uid().c_str());
            notify_userout_to_others(iter->second->uid());
            iter = live_users_.erase(iter);
            continue;
        }
        iter++;
    }

    for(auto user_iter = users_.begin();
        user_iter != users_.end();) {
        int64_t diff_t = now_ms - user_iter->second->active_last_ms();
        if (diff_t > 20*1000) {
            log_infof("rtc user(%s) type(%s) is timeout:%ld, now_ms:%ld",
                    user_iter->second->uid().c_str(),
                    user_iter->second->user_type().c_str(), diff_t, now_ms);
            notify_userout_to_others(user_iter->second->uid());
            user_iter = users_.erase(user_iter);
            continue;
        }
        user_iter++;
    }
}

std::shared_ptr<user_info> room_service::get_rtc_user(const std::string& uid) {
    std::shared_ptr<user_info> user_ptr;
    auto iter = users_.find(uid);
    if (iter == users_.end()) {
        return user_ptr;
    }

    return iter->second;
}

std::shared_ptr<live_user_info> room_service::get_live_user(const std::string& uid) {
    std::shared_ptr<live_user_info> user_ptr;
    auto iter = live_users_.find(uid);
    if (iter == live_users_.end()) {
        return user_ptr;
    }

    return iter->second;
}

bool room_service::has_rtc_user(const std::string& uid) {
    auto iter = users_.find(uid);
    if (iter != users_.end()) {
        return true;
    }

    return false;
}

void room_service::remove_live_user(const std::string& roomid, const std::string& uid) {
    notify_userout_to_others(uid);

    auto iter = live_users_.find(uid);
    if (iter != live_users_.end()) {
        live_users_.erase(iter);
        log_infof("remove live uid:%s in roomid:%s",
                uid.c_str(), roomid.c_str());
    } else {
        log_errorf("fail to get live user by uid:%s in roomid:%s",
                uid.c_str(), roomid.c_str());
    }
    return;
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

std::shared_ptr<live_user_info> room_service::get_live_user_info(const std::string& uid) {
    std::shared_ptr<live_user_info> ret_ptr;

    auto iter = live_users_.find(uid);
    if (iter != live_users_.end()) {
        ret_ptr = iter->second; 
    }
    return ret_ptr;
}

std::shared_ptr<user_info> room_service::get_user_info(const std::string& uid) {
    std::shared_ptr<user_info> ret_ptr;
    auto iter = users_.find(uid);
    if (iter != users_.end()) {
        ret_ptr = iter->second; 
    }
    return ret_ptr;
}

std::string room_service::get_uid_by_json(const json& data_json) {
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

void room_service::on_rtmp_callback(const std::string& roomId, const std::string& uid,
                                const std::string& stream_type, MEDIA_PACKET_PTR pkt_ptr) {
    auto iter = users_.find(uid);
    if (iter == users_.end()) {
        log_errorf("fail to find uid:%s, roomId:%s", uid.c_str(), roomId.c_str());
        return;
    }
    std::shared_ptr<user_info> user_ptr = iter->second;

    if (!user_ptr) {
        return;
    }
    user_ptr->on_rtmp_callback(stream_type, pkt_ptr);
}

void room_service::on_rtppacket_publisher2room(rtc_publisher* publisher, rtp_packet* pkt) {
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

void room_service::on_rtppacket_publisher2room(const std::string& publisher_id,
        const std::string& media_type, rtp_packet* pkt) {
    auto subs_map_it = pid2subscribers_.find(publisher_id);
    if (subs_map_it != pid2subscribers_.end()) {
        for (auto subscribe_item : subs_map_it->second) {
            subscribe_item.second->send_rtp_packet(roomId_, media_type, publisher_id, pkt);
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

void room_service::on_update_alive(const std::string& roomId, const std::string& uid, int64_t now_ms) {
    auto user_iter = users_.find(uid);
    if (user_iter != users_.end()) {
        user_iter->second->update_alive(now_ms);
        return;
    }
    return;
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

int room_service::live_publish(const std::string& uid,
                            const std::string& publisher_id,
                            bool has_video, bool has_audio,
                            uint32_t video_ssrc, uint32_t audio_ssrc,
                            uint32_t video_mid, uint32_t audio_mid) {
    std::vector<publisher_info> publishers_vec;
    publisher_info video_info;
    publisher_info audio_info;

    if (has_video) {
        video_info.media_type   = "video";
        video_info.mid          = video_mid;
        video_info.pid          = publisher_id;
        video_info.publish_type = PUBLISH_LIVE;
        video_info.ssrc         = video_ssrc;

        publishers_vec.push_back(video_info);
    }

    if (has_audio) {
        audio_info.media_type   = "audio";
        audio_info.mid          = audio_mid;
        audio_info.pid          = publisher_id;
        audio_info.publish_type = PUBLISH_LIVE;
        audio_info.ssrc         = audio_ssrc;

        publishers_vec.push_back(audio_info);
    }

    //notify new publish infomation to other users
    notify_publisher_to_others(uid, "live", publisher_id, publishers_vec);

    return 0;
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
    //log_debugf("publish sdp json:%s", info_json.dump().c_str());

    rtc_media_info& info = user_ptr->parse_remote_media_info(info_json);
    //log_debugf("get input sdp dump:\r\n%s", info.dump().c_str());
    rtc_media_info support_info;

    user_ptr->get_support_media_info(info, support_info);
    //log_debugf("support info sdp dump:\r\n%s", support_info.dump().c_str());

    std::shared_ptr<webrtc_session> session_ptr = std::make_shared<webrtc_session>(roomId_, uid,
                                                    this, RTC_DIRECTION_RECV, support_info);
    session_ptr->set_remote_finger_print(info.finger_print);
    for (auto media_item : support_info.medias) {
        if (media_item.rtp_encodings.empty()) {
            std::stringstream ss;
            ss << "publish media rtp encodings is empty, mid:" << media_item.mid
               << ", media type:" << media_item.media_type << ", protocal:" << media_item.protocol;
            log_errorf("%s", ss.str().c_str());
            feedback_p->reject(id, SDP_ERROR, ss.str());
            return;
        }
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
    std::string resp_sdp_str = user_ptr->rtc_media_info_2_sdp(support_info);

    user_ptr->publish_sessions_[session_ptr->get_id()] = session_ptr;
    //log_infof("support response sdp:\r\n%s", resp_sdp_str.c_str());

    response_publish(id, feedback_p, 0, "ok", resp_sdp_str, session_ptr->get_id());

    //notify new publish infomation to other users
    std::vector<publisher_info> publishers_vec = session_ptr->get_publishs_information();
    notify_publisher_to_others(uid, "webrtc", session_ptr->get_id(), publishers_vec);
    
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
        feedback_p->reject(id, UID_ERROR, "peer connection id can't be found");
        return;
    }
    if (!pc_id_json->is_string()) {
        feedback_p->reject(id, UID_ERROR, "peer connection id is not string");
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
    //log_infof("subscribe data: %s", data.c_str());
    std::string user_type;
    json data_json = json::parse(data);

    auto user_type_json = data_json.find("user_type");
    if (user_type_json == data_json.end()) {
        user_type = "webrtc";
    } else {
        if (!user_type_json->is_string()) {
            feedback_p->reject(id, UID_ERROR, "subscribe user_type field is not string");
            return;
        }
        user_type = user_type_json->get<std::string>();
    }

    if (user_type == "webrtc") {
        handle_webrtc_subscribe(id, data_json, feedback_p);
    } else if (user_type == "live") {
        handle_live_subscribe(id, data_json, feedback_p);
    } else {
        std::stringstream error_desc;
        error_desc << "user type is unkown:" << user_type;
        feedback_p->reject(id, UID_ERROR, error_desc.str());
    }

    return;
}

void room_service::handle_live_subscribe(const std::string& id, const json& data_json, protoo_request_interface* feedback_p) {
    std::string uid = get_uid_by_json(data_json);
    std::shared_ptr<user_info> user_ptr;
    std::shared_ptr<live_user_info> remote_user_ptr;

    if (uid.empty()) {
        feedback_p->reject(id, UID_ERROR, "live subscribe uid field does not exist");
        return;
    }

    user_ptr = get_user_info(uid);
    if (!user_ptr) {
        feedback_p->reject(id, UID_ERROR, "live subscribe uid doesn't exist");
        return;
    }

    std::string remote_uid = data_json["remoteUid"];
    remote_user_ptr = get_live_user_info(remote_uid);
    if (!remote_user_ptr) {
        feedback_p->reject(id, UID_ERROR, "publisher live uid doesn't exist");
        return;
    }

    auto publishers_iter = data_json.find("publishers");
    if (publishers_iter == data_json.end()) {
        feedback_p->reject(id, SUBSCRIBE_JSON_ERROR, "live publishers doesn't exist");
        return;
    }
    std::vector<publisher_info> publishers = get_publishers_info_by_json(*publishers_iter);

    auto sdp_iter = data_json.find("sdp");
    if (sdp_iter == data_json.end()) {
        feedback_p->reject(id, SUBSCRIBE_JSON_ERROR, "live subscribe sdp doesn't exist");
        return;
    }
    if (!sdp_iter->is_string()) {
        feedback_p->reject(id, SUBSCRIBE_JSON_ERROR, "live subscribe sdp isn't string");
        return;
    }
    std::string sdp = sdp_iter->get<std::string>();

    json info_json = user_ptr->parse_remote_sdp(sdp);
    //log_infof("subscribe sdp json:%s", info_json.dump().c_str());

    rtc_media_info& info = user_ptr->parse_remote_media_info(info_json);
    //log_infof("live get subscribe input media info:\r\n%s", info.dump().c_str());

    rtc_media_info support_info;
    user_ptr->get_support_media_info(info, support_info);

    remote_user_ptr->make_video_cname();
    remote_user_ptr->make_audio_cname();

    /******************* add ssrc info from publisher *************************/
    for (auto& media : support_info.medias) {
        std::string pid;
        for (auto info : publishers) {
            if (info.media_type == media.media_type) {
                SSRC_INFO ssrc_group_item;
                SSRC_INFO rtx_ssrc_group_item;
                SSRC_GROUPS group;
                group.semantics = "FID";
                std::string cname;

                if (media.media_type == "video") {
                    cname = remote_user_ptr->get_video_cname();
                } else if (media.media_type == "audio") {
                    cname = remote_user_ptr->get_audio_cname();
                }
                ssrc_group_item.attribute = "cname";
                ssrc_group_item.value = cname;
                rtx_ssrc_group_item.attribute = "cname";
                rtx_ssrc_group_item.value = cname;

                if (info.media_type == "video") {
                    ssrc_group_item.ssrc = remote_user_ptr->video_ssrc();
                    rtx_ssrc_group_item.ssrc = remote_user_ptr->video_rtx_ssrc();
                    media.ssrc_infos.push_back(ssrc_group_item);
                    media.ssrc_infos.push_back(rtx_ssrc_group_item);

                    group.ssrcs.push_back(remote_user_ptr->video_ssrc());
                    group.ssrcs.push_back(remote_user_ptr->video_rtx_ssrc());
                    media.msid = remote_user_ptr->video_msid();
                } else if (info.media_type == "audio") {
                    ssrc_group_item.ssrc = remote_user_ptr->audio_ssrc();
                    media.ssrc_infos.push_back(ssrc_group_item);

                    group.ssrcs.push_back(remote_user_ptr->audio_ssrc());
                    media.msid = remote_user_ptr->audio_msid();
                } else {
                    log_errorf("media type(%s) error", info.media_type.c_str());
                }

                std::stringstream ss_debug;
                ss_debug << "ssrc infos:" << "\r\n";
                for (auto ssrc_info_item : media.ssrc_infos) {
                    ss_debug << "attribute:" << ssrc_info_item.attribute
                            << ", value:" << ssrc_info_item.value
                            << ", ssrc:" << ssrc_info_item.ssrc
                            << "\r\n";
                }

                media.ssrc_groups.push_back(group);
                for (auto ssrc_group_item : media.ssrc_groups) {
                    ss_debug << "semantics:" << ssrc_group_item.semantics << " ";
                    ss_debug << "ssrcs: ";
                    for (auto ssrc : ssrc_group_item.ssrcs) {
                        ss_debug << ssrc << " ";
                    }
                    ss_debug << "\r\n";
                }

                media.publisher_id  = remote_user_ptr->uid();
                media.publisher_id += "_";
                media.publisher_id += media.media_type;
                media.src_mid       = info.mid;
                ss_debug << "msid: " << media.msid << "\r\n";
                ss_debug << "publisher id: " << media.publisher_id << "\r\n";
                ss_debug << "src mid: " << media.src_mid << "\r\n";

                //log_infof("live media type:%s, debug info:%s", info.media_type.c_str(), ss_debug.str().c_str());
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
    //log_infof("live subscribe session id:%s", session_ptr->get_id().c_str());

    /***************** create subscribers for the publisher *******************/
    for (auto& media_item : support_info.medias) {
        media_item.header_extentions.clear();

        if (media_item.media_type == "video") {
            remote_user_ptr->set_video_mid(media_item.mid);
        } else if (media_item.media_type == "audio") {
            remote_user_ptr->set_audio_mid(media_item.mid);
            media_item.fmtps.clear();
        }
        auto subscirber_ptr = session_ptr->create_subscriber(remote_uid, media_item, media_item.publisher_id, this);
        insert_subscriber(media_item.publisher_id, subscirber_ptr);
        
        subscirber_ptr->set_stream_type(LIVE_STREAM_TYPE);
        if (media_item.media_type == "video") {
            //log_infof("update video payload type:%d", subscirber_ptr->get_payload());
            remote_user_ptr->set_video_payload(subscirber_ptr->get_payload());
            remote_user_ptr->set_video_rtx_payload(subscirber_ptr->get_rtx_payload());
            remote_user_ptr->set_video_clock(90000);
        } else if (media_item.media_type == "audio") {
            //log_infof("update audio payload type:%d", subscirber_ptr->get_payload());
            remote_user_ptr->set_audio_payload(subscirber_ptr->get_payload());
            remote_user_ptr->set_audio_clock(48000);
        }
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
    //log_infof("get live subscribe support media info:\r\n%s", support_info.dump().c_str());

    std::string resp_sdp_str = user_ptr->rtc_media_info_2_sdp(support_info);

    auto resp_json = json::object();
    resp_json["code"] = 0;
    resp_json["desc"] = "ok";
    resp_json["pcid"] = session_ptr->get_id();
    resp_json["sdp"]  = resp_sdp_str;

    std::string resp_data = resp_json.dump();
    //log_infof("subscirbe response data:%s", resp_data.c_str());
    feedback_p->accept(id, resp_data);

    return;
}

void room_service::handle_webrtc_subscribe(const std::string& id, const json& data_json, protoo_request_interface* feedback_p) {
    std::string uid = get_uid_by_json(data_json);
    std::shared_ptr<user_info> user_ptr;
    std::shared_ptr<user_info> remote_user_ptr;

    if (uid.empty()) {
        feedback_p->reject(id, UID_ERROR, "subscribe uid field does not exist");
        return;
    }

    user_ptr = get_user_info(uid);
    if (!user_ptr) {
        feedback_p->reject(id, UID_ERROR, "subscribe uid doesn't exist");
        return;
    }

    std::string remote_uid = data_json["remoteUid"];
    remote_user_ptr = get_user_info(remote_uid);
    if (!remote_user_ptr) {
        feedback_p->reject(id, UID_ERROR, "subscribe uid doesn't exist");
        return;
    }

    std::string remote_pcid = data_json["remotePcId"];
    auto remote_session_iter = remote_user_ptr->publish_sessions_.find(remote_pcid);
    if (remote_session_iter == remote_user_ptr->publish_sessions_.end()) {
        feedback_p->reject(id, UID_ERROR, "subscribe pcid doesn't exist");
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
    //log_infof("subscribe sdp json:%s", info_json.dump().c_str());

    rtc_media_info& info = user_ptr->parse_remote_media_info(info_json);
    //log_infof("get subscribe input media info:\r\n%s", info.dump().c_str());

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
                std::stringstream ss_debug;
                media.ssrc_infos    = publisher_ptr->get_media_info().ssrc_infos;
                ss_debug << "ssrc infos:" << "\r\n";
                for (auto ssrc_info_item : media.ssrc_infos) {
                    ss_debug << "attribute:" << ssrc_info_item.attribute
                    << ", value:" << ssrc_info_item.value
                    << ", ssrc:" << ssrc_info_item.ssrc
                    << "\r\n";
                }

                media.ssrc_groups   = publisher_ptr->get_media_info().ssrc_groups;
                for (auto ssrc_group_item : media.ssrc_groups) {
                    ss_debug << "semantics:" << ssrc_group_item.semantics << " ";
                    ss_debug << "ssrcs: ";
                    for (auto ssrc : ssrc_group_item.ssrcs) {
                        ss_debug << ssrc << " ";
                    }
                    ss_debug << "\r\n";
                }
                media.msid          = publisher_ptr->get_media_info().msid;
                media.publisher_id  = publisher_ptr->get_publisher_id();
                media.src_mid       = info.mid;
                ss_debug << "msid: " << media.msid << "\r\n";
                ss_debug << "publisher id: " << media.publisher_id << "\r\n";
                ss_debug << "src mid: " << media.src_mid << "\r\n";
                
                //log_infof("media type:%s, debug info:%s", info.media_type.c_str(), ss_debug.str().c_str());
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
    //log_infof("subscribe session id:%s", session_ptr->get_id().c_str());

    /***************** create subscribers for the publisher *******************/
    for (auto media_item : support_info.medias) {
        auto subscirber_ptr = session_ptr->create_subscriber(remote_uid, media_item, media_item.publisher_id, this);
        subscirber_ptr->set_stream_type(RTC_STREAM_TYPE);
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
    //log_infof("get subscribe support media info:\r\n%s", support_info.dump().c_str());

    std::string resp_sdp_str = user_ptr->rtc_media_info_2_sdp(support_info);

    auto resp_json = json::object();
    resp_json["code"] = 0;
    resp_json["desc"] = "ok";
    resp_json["pcid"] = session_ptr->get_id();
    resp_json["sdp"]  = resp_sdp_str;
    
    std::string resp_data = resp_json.dump();
    //log_infof("subscirbe response data:%s", resp_data.c_str());
    feedback_p->accept(id, resp_data);
}

std::shared_ptr<live_user_info> room_service::live_user_join(const std::string& roomId, const std::string& uid) {
    std::shared_ptr<live_user_info> user_ptr;

    user_ptr = get_live_user_info(uid);
    if (!user_ptr) {
        user_ptr = std::make_shared<live_user_info>(uid, roomId, this);
        live_users_.insert(std::make_pair(uid, user_ptr));
        log_infof("live user join uid:%s, roomId:%s", uid.c_str(), roomId.c_str());
    }

    notify_userin_to_others(uid, "live");

    return user_ptr;
}

int room_service::handle_http_publish(const std::string& uid, const std::string& data,
                        std::string& resp_sdp, std::string& session_id, std::string& err_msg) {
    std::string sdp = data;
    std::shared_ptr<user_info> user_ptr;

    err_msg = "ok";

    handle_http_join(uid);
    user_ptr = get_user_info(uid);

    json info_json = user_ptr->parse_remote_sdp(sdp);
    //log_infof("publish sdp json:%s", info_json.dump().c_str());

    rtc_media_info& info = user_ptr->parse_remote_media_info(info_json);
    //log_infof("http publish get input sdp dump:\r\n%s", info.dump().c_str());
    rtc_media_info support_info;

    user_ptr->get_support_media_info(info, support_info);
    //log_infof("http publish support info sdp dump:\r\n%s", support_info.dump().c_str());

    std::shared_ptr<webrtc_session> session_ptr = std::make_shared<webrtc_session>(roomId_, uid,
                                                    this, RTC_DIRECTION_RECV, support_info);
    session_ptr->set_remote_finger_print(info.finger_print);
    for (auto media_item : support_info.medias) {
        if (media_item.rtp_encodings.empty()) {
            std::stringstream ss;
            ss << "publish media rtp encodings is empty, mid:" << media_item.mid
               << ", media type:" << media_item.media_type << ", protocal:" << media_item.protocol;
            err_msg = ss.str();
            log_errorf("%s", ss.str().c_str());
            return -1;
        }
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
    resp_sdp = user_ptr->rtc_media_info_2_sdp(support_info);

    user_ptr->publish_sessions_[session_ptr->get_id()] = session_ptr;
    session_id = session_ptr->get_id();
    log_infof("http publish response sdp:\r\n%s", resp_sdp.c_str());
    log_infof("http publish response sessionid:\r\n%s", session_id.c_str());

    //notify new publish infomation to other users
    std::vector<publisher_info> publishers_vec = session_ptr->get_publishs_information();
    notify_publisher_to_others(uid, "whip", session_ptr->get_id(), publishers_vec);

    return 0;
}

int room_service::handle_http_unpublish(const std::string& uid, const std::string& sessionid, std::string& err_msg) {
    std::shared_ptr<user_info> user_ptr;
    err_msg = "ok";

    user_ptr = get_user_info(uid);
    if (!user_ptr) {
        err_msg = "uid doesn't exist";
        return -1;
    }

    auto session_iter = user_ptr->publish_sessions_.find(sessionid);
    if (session_iter == user_ptr->publish_sessions_.end()) {
        err_msg = "peer connection id doesn't exist";
        return -1;
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

    //notify unpublish to others
    notify_unpublisher_to_others(uid, unpublish_vec);

    return 0;
}

int room_service::handle_http_unpublish(const std::string& uid, std::string& err_msg) {
    std::shared_ptr<user_info> user_ptr;
    err_msg = "ok";

    user_ptr = get_user_info(uid);
    if (!user_ptr) {
        err_msg = "uid doesn't exist";
        return -1;
    }

    auto session_iter = user_ptr->publish_sessions_.begin();
    if (session_iter == user_ptr->publish_sessions_.end()) {
        err_msg = "peer connection id doesn't exist";
        return -1;
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

    //notify unpublish to others
    notify_unpublisher_to_others(uid, unpublish_vec);

    return 0;
}

int room_service::handle_http_webrtc_subscribe(std::shared_ptr<user_info> user_ptr, std::shared_ptr<user_info> remote_user_ptr, const std::string& data,
                    std::string& resp_sdp, std::string& session_id, std::string& err_msg) {
    std::string uid = user_ptr->uid();
    std::string remote_uid = remote_user_ptr->uid();

    if (remote_user_ptr->publish_sessions_.empty()) {
        err_msg = "subscriber pcid doesn't exist";
        log_errorf("subscribe uid(%s) doesn't exist", uid.c_str());
        return -1;
    }

    auto remote_session_iter = remote_user_ptr->publish_sessions_.begin();
    std::shared_ptr<webrtc_session> remote_session_ptr = remote_session_iter->second;
    std::vector<publisher_info> publishers = remote_session_ptr->get_publishs_information();

    std::string sdp = data;

    json info_json = user_ptr->parse_remote_sdp(sdp);
    //log_infof("http subscribe sdp json:%s", info_json.dump().c_str());

    rtc_media_info& info = user_ptr->parse_remote_media_info(info_json);
    //log_infof("http subscribe input media info:\r\n%s", info.dump().c_str());

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
                    err_msg = "fail to get publisher by mid";
                    return -1;
                }
                std::stringstream ss_debug;
                media.ssrc_infos    = publisher_ptr->get_media_info().ssrc_infos;
                ss_debug << "ssrc infos:" << "\r\n";
                for (auto ssrc_info_item : media.ssrc_infos) {
                    ss_debug << "attribute:" << ssrc_info_item.attribute
                    << ", value:" << ssrc_info_item.value
                    << ", ssrc:" << ssrc_info_item.ssrc
                    << "\r\n";
                }

                media.ssrc_groups   = publisher_ptr->get_media_info().ssrc_groups;
                for (auto ssrc_group_item : media.ssrc_groups) {
                    ss_debug << "semantics:" << ssrc_group_item.semantics << " ";
                    ss_debug << "ssrcs: ";
                    for (auto ssrc : ssrc_group_item.ssrcs) {
                        ss_debug << ssrc << " ";
                    }
                    ss_debug << "\r\n";
                }
                media.msid          = publisher_ptr->get_media_info().msid;
                media.publisher_id  = publisher_ptr->get_publisher_id();
                media.src_mid       = info.mid;
                ss_debug << "msid: " << media.msid << "\r\n";
                ss_debug << "publisher id: " << media.publisher_id << "\r\n";
                ss_debug << "src mid: " << media.src_mid << "\r\n";
                
                //log_infof("http subscribe media type:%s, debug info:%s", info.media_type.c_str(), ss_debug.str().c_str());
                break;
            }
        }
    }

    std::shared_ptr<webrtc_session> session_ptr;
    /********************** create subscribe session **************************/
    session_ptr = std::make_shared<webrtc_session>(roomId_, uid, this,
                                            RTC_DIRECTION_SEND, support_info, uid);
    session_ptr->set_remote_finger_print(info.finger_print);
    user_ptr->subscribe_sessions_[session_ptr->get_id()] = session_ptr;
    //log_infof("http subscribe session id:%s", session_ptr->get_id().c_str());

    /***************** create subscribers for the publisher *******************/
    for (auto media_item : support_info.medias) {
        auto subscirber_ptr = session_ptr->create_subscriber(remote_uid, media_item, media_item.publisher_id, this);
        subscirber_ptr->set_stream_type(RTC_STREAM_TYPE);
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
    //log_infof("http subscribe support media info:\r\n%s", support_info.dump().c_str());

    resp_sdp = user_ptr->rtc_media_info_2_sdp(support_info);
    session_id = session_ptr->get_id();
    
    log_infof("subscribe sdp:%s", resp_sdp.c_str());
    
    return 0;
}

int room_service::handle_http_live_subscribe(std::shared_ptr<user_info> user_ptr, std::shared_ptr<live_user_info> remote_user_ptr, const std::string& data,
                    std::string& resp_sdp, std::string& session_id, std::string& err_msg) {
    std::string uid = user_ptr->uid();
    std::string remote_uid = remote_user_ptr->uid();

    std::string sdp = data;

    json info_json = user_ptr->parse_remote_sdp(sdp);
    //log_infof("http subscribe sdp json:%s", info_json.dump().c_str());

    rtc_media_info& info = user_ptr->parse_remote_media_info(info_json);
    //log_infof("http subscribe input media info:\r\n%s", info.dump().c_str());

    rtc_media_info support_info;
    user_ptr->get_support_media_info(info, support_info);

    remote_user_ptr->make_video_cname();
    remote_user_ptr->make_audio_cname();

    /******************* add ssrc info from publisher *************************/
    for (auto& media : support_info.medias) {
        std::string pid;

        //index == 0, video; index == 1, audio
        for (int index = 0; index < 2; index++) {
           SSRC_INFO ssrc_group_item;
           SSRC_INFO rtx_ssrc_group_item;
           SSRC_GROUPS group;
           group.semantics = "FID";
           std::string cname;

           if ((index == 0) && (media.media_type != "video")) {
               continue;
           }

           if ((index == 1) && (media.media_type != "audio")) {
               continue;
           }
           if (index == 0) {
               cname = remote_user_ptr->get_video_cname();
           } else if (index == 1) {
               cname = remote_user_ptr->get_audio_cname();
           }
           ssrc_group_item.attribute = "cname";
           ssrc_group_item.value = cname;
           rtx_ssrc_group_item.attribute = "cname";
           rtx_ssrc_group_item.value = cname;

           if (index == 0) {
               ssrc_group_item.ssrc = remote_user_ptr->video_ssrc();
               rtx_ssrc_group_item.ssrc = remote_user_ptr->video_rtx_ssrc();
               media.ssrc_infos.push_back(ssrc_group_item);
               media.ssrc_infos.push_back(rtx_ssrc_group_item);

               group.ssrcs.push_back(remote_user_ptr->video_ssrc());
               group.ssrcs.push_back(remote_user_ptr->video_rtx_ssrc());
               media.msid = remote_user_ptr->video_msid();
           } else if (index == 1) {
               ssrc_group_item.ssrc = remote_user_ptr->audio_ssrc();
               media.ssrc_infos.push_back(ssrc_group_item);

               group.ssrcs.push_back(remote_user_ptr->audio_ssrc());
               media.msid = remote_user_ptr->audio_msid();
           }

           std::stringstream ss_debug;
           ss_debug << "ssrc infos:" << "\r\n";
           for (auto ssrc_info_item : media.ssrc_infos) {
               ss_debug << "attribute:" << ssrc_info_item.attribute
                       << ", value:" << ssrc_info_item.value
                       << ", ssrc:" << ssrc_info_item.ssrc
                       << "\r\n";
           }

           media.ssrc_groups.push_back(group);
           for (auto ssrc_group_item : media.ssrc_groups) {
               ss_debug << "semantics:" << ssrc_group_item.semantics << " ";
               ss_debug << "ssrcs: ";
               for (auto ssrc : ssrc_group_item.ssrcs) {
                   ss_debug << ssrc << " ";
               }
               ss_debug << "\r\n";
           }

           media.publisher_id  = remote_user_ptr->uid();
           media.publisher_id += "_";
           media.publisher_id += media.media_type;
           if (index == 0) {
               media.src_mid = remote_user_ptr->video_mid();
           } else {
               media.src_mid = remote_user_ptr->audio_mid();
           }
           ss_debug << "msid: " << media.msid << "\r\n";
           ss_debug << "publisher id: " << media.publisher_id << "\r\n";
           ss_debug << "src mid: " << media.src_mid << "\r\n";

           //log_infof("live media type:%s, debug info:%s", info.media_type.c_str(), ss_debug.str().c_str());
           break;
        }
    }

    std::shared_ptr<webrtc_session> session_ptr;
    /********************** create subscribe session **************************/
    session_ptr = std::make_shared<webrtc_session>(roomId_, uid, this,
                                            RTC_DIRECTION_SEND, support_info);
    session_ptr->set_remote_finger_print(info.finger_print);
    user_ptr->subscribe_sessions_[session_ptr->get_id()] = session_ptr;
    //log_infof("live subscribe session id:%s", session_ptr->get_id().c_str());

    /***************** create subscribers for the publisher *******************/
    for (auto& media_item : support_info.medias) {
        media_item.header_extentions.clear();

        if (media_item.media_type == "video") {
            remote_user_ptr->set_video_mid(media_item.mid);
        } else if (media_item.media_type == "audio") {
            remote_user_ptr->set_audio_mid(media_item.mid);
            media_item.fmtps.clear();
        }
        auto subscirber_ptr = session_ptr->create_subscriber(remote_uid, media_item, media_item.publisher_id, this);
        insert_subscriber(media_item.publisher_id, subscirber_ptr);
        
        subscirber_ptr->set_stream_type(LIVE_STREAM_TYPE);
        if (media_item.media_type == "video") {
            //log_infof("update video payload type:%d", subscirber_ptr->get_payload());
            remote_user_ptr->set_video_payload(subscirber_ptr->get_payload());
            remote_user_ptr->set_video_rtx_payload(subscirber_ptr->get_rtx_payload());
            remote_user_ptr->set_video_clock(90000);
        } else if (media_item.media_type == "audio") {
            //log_infof("update audio payload type:%d", subscirber_ptr->get_payload());
            remote_user_ptr->set_audio_payload(subscirber_ptr->get_payload());
            remote_user_ptr->set_audio_clock(48000);
        }
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
    //log_infof("get live subscribe support media info:\r\n%s", support_info.dump().c_str());

    resp_sdp = user_ptr->rtc_media_info_2_sdp(support_info);
    session_id = session_ptr->get_id();
    
    //log_infof("subscribe sdp:%s", resp_sdp.c_str());

    return 0;
}

int room_service::handle_http_subscribe(const std::string& uid, const std::string& remote_uid, const std::string& data,
                        std::string& resp_sdp, std::string& session_id, std::string& err_msg) {
    std::shared_ptr<user_info> user_ptr;
    std::shared_ptr<user_info> remote_user_ptr;
    std::shared_ptr<live_user_info> live_remote_user_ptr;

    err_msg = "ok";

    handle_http_join(uid);

    user_ptr = get_user_info(uid);
    if (!user_ptr) {
        err_msg = "subscribe uid doesn't exist";
        log_errorf("subscribe uid(%s) doesn't exist", uid.c_str());
        return -1;
    }

    remote_user_ptr = get_user_info(remote_uid);
    if (!remote_user_ptr) {
        err_msg = "webrtc subscriber uid doesn't exist";
        log_errorf("webrtc subscribe uid(%s) doesn't exist", remote_uid.c_str());

        live_remote_user_ptr = get_live_user_info(remote_uid);
        if (!live_remote_user_ptr) {
            err_msg = "live subscriber uid doesn't exist";
            log_errorf("live subscribe uid(%s) doesn't exist", remote_uid.c_str());
            return -1;
        }
        log_infof("get live subscriber remote uid:%s", remote_uid.c_str());
        return handle_http_live_subscribe(user_ptr, live_remote_user_ptr, data,
                        resp_sdp, session_id, err_msg);
    }

    return handle_http_webrtc_subscribe(user_ptr, remote_user_ptr, data,
                        resp_sdp, session_id, err_msg);
}

int room_service::handle_http_unsubscribe(const std::string& uid, const std::string& remote_uid,
                                    const std::string& sessionid, std::string& err_msg) {
    std::shared_ptr<user_info> user_ptr;
    std::shared_ptr<user_info> remote_user_ptr;

    user_ptr = get_user_info(uid);
    if (!user_ptr) {
        err_msg = "subscribe uid doesn't exist";
        return -1;
    }

    //remove pcid frome user_info's publisher_sessions: key<remote_pcid>, value<webrtc_session>
    if (user_ptr->subscribe_sessions_.empty()) {
        log_infof("unsubscribe fail to get remote publishers");
        err_msg = "unsubscribe doesn't exist";
        return -1;
    }
    auto subscriber_session_iter = user_ptr->subscribe_sessions_.find(sessionid);
    if (subscriber_session_iter == user_ptr->subscribe_sessions_.end()) {
        log_infof("http unsubscribe fail to find sessionid:%s", sessionid.c_str());
        err_msg = "unsubscribe fail to find sessionid";
        return -1;
    }

    user_ptr->subscribe_sessions_.erase(subscriber_session_iter);
    log_infof("unsubscribe remote uid:%s", uid.c_str());

    remote_user_ptr = get_user_info(remote_uid);
    if (!remote_user_ptr) {
        err_msg = "subscriber uid doesn't exist";
        return -1;
    }
    if (remote_user_ptr->publish_sessions_.empty()) {
        err_msg = "subscriber pcid doesn't exist";
        return -1;
    }

    auto remote_session_iter = remote_user_ptr->publish_sessions_.begin();
    std::shared_ptr<webrtc_session> remote_session_ptr = remote_session_iter->second;
    std::vector<publisher_info> publishers = remote_session_ptr->get_publishs_information();

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

    return 0;
}

int room_service::handle_http_unsubscribe(const std::string& uid, const std::string& remote_uid,
                                        std::string& err_msg) {
    std::shared_ptr<user_info> user_ptr;
    std::shared_ptr<user_info> remote_user_ptr;

    user_ptr = get_user_info(uid);
    if (!user_ptr) {
        err_msg = "subscribe uid doesn't exist";
        return -1;
    }

    //remove pcid frome user_info's publisher_sessions: key<remote_pcid>, value<webrtc_session>
    if (user_ptr->subscribe_sessions_.empty()) {
        log_infof("unsubscribe fail to get remote publishers");
        err_msg = "unsubscribe doesn't exist";
        return -1;
    }
    auto subscriber_session_iter = user_ptr->subscribe_sessions_.begin();

    user_ptr->subscribe_sessions_.erase(subscriber_session_iter);
    log_infof("unsubscribe remote uid:%s", uid.c_str());

    remote_user_ptr = get_user_info(remote_uid);
    if (!remote_user_ptr) {
        err_msg = "subscriber uid doesn't exist";
        return -1;
    }
    if (remote_user_ptr->publish_sessions_.empty()) {
        err_msg = "subscriber pcid doesn't exist";
        return -1;
    }

    auto remote_session_iter = remote_user_ptr->publish_sessions_.begin();
    std::shared_ptr<webrtc_session> remote_session_ptr = remote_session_iter->second;
    std::vector<publisher_info> publishers = remote_session_ptr->get_publishs_information();

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

    return 0;
}

void room_service::handle_http_join(const std::string& uid) {
    std::shared_ptr<user_info> user_ptr = get_user_info(uid);
    if (user_ptr.get() != nullptr) {
        return;
    }

    user_ptr = std::make_shared<user_info>(uid, roomId_);
    users_.insert(std::make_pair(uid, user_ptr));

    log_infof("http user join uid:%s, roomId:%s", uid.c_str(), roomId_.c_str());

    auto resp_json = json::object();
    resp_json["users"] = json::array();

    for (auto& user_item : users_) {
        auto user_json = json::object();
        user_json["uid"] = user_item.first;
        resp_json["users"].emplace_back(user_json);
    }

    for (auto& user_item : live_users_) {
        auto user_json = json::object();
        user_json["uid"] = user_item.first;
        resp_json["users"].emplace_back(user_json);
    }

    notify_userin_to_others(uid, "whip");

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

    for (auto& user_item : users_) {
        auto user_json = json::object();
        user_json["uid"] = user_item.first;
        resp_json["users"].emplace_back(user_json);
    }

    for (auto& user_item : live_users_) {
        auto user_json = json::object();
        user_json["uid"] = user_item.first;
        resp_json["users"].emplace_back(user_json);
    }

    notify_userin_to_others(uid, "webrtc");

    std::string resp_data = resp_json.dump();
    log_infof("join response data:%s", resp_data.c_str());
    feedback_p->accept(id, resp_data);

    notify_others_publisher_to_me(uid, user_ptr);
    return;
}

void room_service::notify_userin_to_others(const std::string& uid, const std::string& user_type) {

    for (auto iter : users_) {
        if (uid == iter.first) {
            continue;
        }
        std::shared_ptr<user_info> other_user = iter.second;
        if (other_user->user_type() != "websocket") {
            continue;
        }
        auto resp_json = json::object();
        resp_json["uid"] = uid;
        resp_json["user_type"] = user_type;
        log_infof("send newuser message: %s", resp_json.dump().c_str());
        if (other_user->feedback()) {
            other_user->feedback()->notification("userin", resp_json.dump());
        }
    }
}

void room_service::notify_userout_to_others(const std::string& uid) {
    for (auto iter : users_) {
        if (uid == iter.first) {
            continue;
        }
        std::shared_ptr<user_info> other_user = iter.second;
        if (other_user->user_type() != "websocket") {
            continue;
        }
        auto resp_json = json::object();
        resp_json["uid"] = uid;
        log_infof("send userout message: %s", resp_json.dump().c_str());
        if (other_user->feedback()) {
            other_user->feedback()->notification("userout", resp_json.dump());
        }
        
    }
}

void room_service::notify_others_publisher_to_me(const std::string& uid, std::shared_ptr<user_info> me) {
    for (auto& item : users_) {
        if (item.first == uid) {
            continue;
        }
        std::shared_ptr<user_info> other_user = item.second;
        auto resp_json       = json::object();
        auto publisher_array = json::array();
        bool publish_found   = false;
        std::string pc_id;

        for (auto& session_item : other_user->publish_sessions_) {
            auto session_ptr = session_item.second;

            std::vector<publisher_info> publisher_vec = session_ptr->get_publishs_information();

            for (auto& info : publisher_vec) {
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
            resp_json["user_type"]  = "webrtc";

            log_infof("send other publisher message: %s to me(%s)", resp_json.dump().c_str(), uid.c_str());
            me->feedback()->notification("publish", resp_json.dump());
        }
    }

    for (auto& item : live_users_) {
        auto user_ptr = item.second;
        std::string publisher_id = roomId_;
        publisher_id += "/";
        publisher_id += item.first;

        auto resp_json       = json::object();
        auto publisher_array = json::array();
        bool publish_found   = false;

        if (user_ptr->has_video()) {
            auto publiser_json    = json::object();
            publish_found = true;

            publiser_json["type"]   = "video";
            publiser_json["mid"]    = user_ptr->video_mid();
            publiser_json["pid"]    = publisher_id;
            publiser_json["ssrc"]   = user_ptr->video_ssrc();
            publisher_array.push_back(publiser_json);
        }
        if (user_ptr->has_audio()) {
            auto publiser_json    = json::object();
            publish_found = true;

            publiser_json["type"]   = "audio";
            publiser_json["mid"]    = user_ptr->audio_mid();
            publiser_json["pid"]    = publisher_id;
            publiser_json["ssrc"]   = user_ptr->audio_ssrc();
            publisher_array.push_back(publiser_json);
        }
        if (publish_found) {
            resp_json["publishers"] = publisher_array;
            resp_json["uid"]        = item.first;
            resp_json["pcid"]       = publisher_id;
            resp_json["user_type"]  = "live";
            log_infof("send other live publisher message: %s to me(%s)", resp_json.dump().c_str(), uid.c_str());
            me->feedback()->notification("publish", resp_json.dump());
        }
    }
}

void room_service::notify_publisher_to_others(const std::string& uid, const std::string& user_type, const std::string& pc_id,
                                            const std::vector<publisher_info>& publisher_vec) {
    for (auto iter : users_) {
        if (uid == iter.first) {
            continue;
        }
        std::shared_ptr<user_info> other_user = iter.second;
        if (other_user->user_type() != "websocket") {
            continue;
        }
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
        resp_json["user_type"]  = user_type;
        resp_json["pcid"]       = pc_id;

        log_infof("send publish message: %s", resp_json.dump().c_str());
        if (other_user->feedback()) {
            other_user->feedback()->notification("publish", resp_json.dump());
        }
    }
}

void room_service::notify_unpublisher_to_others(const std::string& uid, const std::vector<publisher_info>& publisher_vec) {
    for (auto iter : users_) {
        if (uid == iter.first) {
            continue;
        }
        std::shared_ptr<user_info> other_user = iter.second;
        if (other_user->user_type() != "websocket") {
            continue;
        }
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
        if (other_user->feedback()) {
            other_user->feedback()->notification("unpublish", resp_json.dump());
        }
    }
}

void room_service::rtmp_stream_ingest(MEDIA_PACKET_PTR pkt_ptr) {
    std::shared_ptr<live_user_info> user_ptr;

    auto iter = live_users_.find(pkt_ptr->streamname_);
    if (iter == live_users_.end()) {
        user_ptr = live_user_join(pkt_ptr->app_, pkt_ptr->streamname_);
    } else {
        user_ptr = iter->second;
    }

    if (!user_ptr->media_ready()) {
        user_ptr->pkt_queue_.push(pkt_ptr);
        if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
            user_ptr->set_video(true);
        } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
            user_ptr->set_audio(true);
        }

        if (user_ptr->pkt_queue_.size() > 20) {
            user_ptr->set_media_ready(true);

            //start publish stream
            uint32_t video_ssrc = 0;
            uint32_t video_rtx_ssrc = 0;
            uint32_t audio_ssrc = 0;


            if (user_ptr->has_video()) {
                std::string video_msid = make_uuid();
                video_ssrc = make_live_video_ssrc();
                video_rtx_ssrc = make_live_video_ssrc();
                user_ptr->set_video_ssrc(video_ssrc);
                user_ptr->set_video_rtx_ssrc(video_rtx_ssrc);
                user_ptr->set_video_msid(video_msid);
            }
            if (user_ptr->has_audio()) {
                std::string audio_msid = make_uuid();
                audio_ssrc = make_live_audio_ssrc();
                user_ptr->set_audio_ssrc(audio_ssrc);
                user_ptr->set_audio_msid(audio_msid);
            }
            std::string publisher_id = pkt_ptr->app_;
            publisher_id += "/";
            publisher_id += pkt_ptr->streamname_;
            live_publish(pkt_ptr->streamname_, publisher_id,
                       user_ptr->has_video(), user_ptr->has_audio(),
                       user_ptr->video_ssrc(), user_ptr->audio_ssrc(),
                       user_ptr->video_mid(), user_ptr->audio_mid());
            //output media packet queue in room
            while (user_ptr->pkt_queue_.size() > 0) {
                MEDIA_PACKET_PTR temp_pkt_ptr = user_ptr->pkt_queue_.front();
                user_ptr->pkt_queue_.pop();
                if (temp_pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
                    user_ptr->handle_video_data(temp_pkt_ptr);
                } else if (temp_pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
                    user_ptr->handle_audio_data(temp_pkt_ptr);
                } else {
                    log_debugf("skip packet av type:%d", temp_pkt_ptr->av_type_);
                }
            }
        }
        return;
    }

    //send media packet in room
    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        user_ptr->handle_video_data(pkt_ptr);
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        user_ptr->handle_audio_data(pkt_ptr);
    } else {
        log_debugf("skip packet av type:%d", pkt_ptr->av_type_);
    }
 
}
