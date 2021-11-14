#include "rtc_base_session.hpp"
#include "rtc_publisher.hpp"
#include "rtc_subscriber.hpp"
#include "webrtc_session.hpp"
#include "net/udp/udp_server.hpp"
#include "utils/logger.hpp"
#include "net/websocket/wsimple/protoo_pub.hpp"

extern std::shared_ptr<udp_server> single_udp_server_ptr;
extern single_udp_session_callback single_udp_cb;

//key: "srcip+srcport" or "username", value: webrtc_session*
extern std::unordered_map<std::string, webrtc_session*> single_webrtc_map;
extern std::string single_candidate_ip;
extern uint16_t single_candidate_port;

rtc_base_session::rtc_base_session(const std::string& roomId, const std::string& uid,
                    room_callback_interface* room, int session_direction, const rtc_media_info& media_info):roomId_(roomId)
                            , uid_(uid)
                            , room_(room)
                            , direction_(session_direction)
                            , media_info_(media_info) {
    id_ = make_uuid();
    log_infof("rtc_base_session construct id:%s direction:%s, roomId:%s, uid:%s",
            id_.c_str(), (direction_ == RTC_DIRECTION_SEND) ? "send" : "receive",
            roomId.c_str(), uid.c_str());
}
    
rtc_base_session::~rtc_base_session() {
    if (direction_ == RTC_DIRECTION_SEND) {
        for (auto item : mid2subscribers_) {
            room_->on_unsubscribe(item.second->get_publisher_id(),
                                item.second->get_subscirber_id());
        }
    }
    if (direction_ == RTC_DIRECTION_RECV) {
        for (auto item : mid2publishers_) {
            room_->on_unpublish(item.second->get_publisher_id());
        }
    }
    log_infof("rtc_base_session destruct id:%s, direction:%s, roomId:%s, uid:%s",
            id_.c_str(), (direction_ == RTC_DIRECTION_SEND) ? "send" : "receive",
            roomId_.c_str(), uid_.c_str());
}

std::shared_ptr<rtc_subscriber> rtc_base_session::create_subscriber(const std::string& remote_uid,
                                                                const MEDIA_RTC_INFO& media_info,
                                                                const std::string& pid,
                                                                room_callback_interface* room_cb) {
    std::shared_ptr<rtc_subscriber> subscriber_ptr;
    auto mid_iter = mid2subscribers_.find(media_info.mid);
    if (mid_iter != mid2subscribers_.end()) {
        MS_THROW_ERROR("find subscriber mid:%d, the subscriber has existed.", media_info.mid);
    }

    subscriber_ptr = std::make_shared<rtc_subscriber>(roomId_, uid_, remote_uid, pid, this, media_info, room_cb);
    
    mid2subscribers_.insert(std::make_pair(media_info.mid, subscriber_ptr));
    pid2subscribers_.insert(std::make_pair(media_info.publisher_id, subscriber_ptr));
    ssrc2subscribers_.insert(std::make_pair(subscriber_ptr->get_rtp_ssrc(), subscriber_ptr));
    return subscriber_ptr;
}

std::shared_ptr<rtc_subscriber> rtc_base_session::get_subscriber(uint32_t ssrc) {
    std::shared_ptr<rtc_subscriber> subscriber_ptr;

    auto sub_iter = ssrc2subscribers_.find(ssrc);
    if (sub_iter == ssrc2subscribers_.end()) {
        return subscriber_ptr;
    }

    return sub_iter->second;
}

void rtc_base_session::create_publisher(const MEDIA_RTC_INFO& media_info) {
    auto mid_iter = mid2publishers_.find(media_info.mid);
    if (mid_iter != mid2publishers_.end()) {
        return;
    }

    bool found = false;
    for (auto info_item : media_info.ssrc_infos) {
        auto ssrc_iter = ssrc2publishers_.find(info_item.ssrc);
        if (ssrc_iter != ssrc2publishers_.end()) {
            found = true;
            break;
        }
    }
    
    if (found) {
        log_infof("publisher exist");
        return;
    }

    std::shared_ptr<rtc_publisher> publisher_ptr = std::make_shared<rtc_publisher>(roomId_, uid_, room_, this, media_info);

    pid2publishers_[publisher_ptr->get_publisher_id()] = publisher_ptr;
    if (media_info.mid >= 0) {
        mid2publishers_[media_info.mid] = publisher_ptr;
    }
    
    for (auto info_item : media_info.ssrc_infos) {
        ssrc2publishers_[info_item.ssrc] = publisher_ptr;
    }
}

void rtc_base_session::remove_publisher(const MEDIA_RTC_INFO& media_info) {
    std::shared_ptr<rtc_publisher> publisher_ptr;

    auto mid_iter = mid2publishers_.find(media_info.mid);
    if (mid_iter != mid2publishers_.end()) {
        publisher_ptr = mid_iter->second;
        mid2publishers_.erase(mid_iter);
        log_infof("remove publisher by mid:%d", media_info.mid);
    }

    for (auto info_item : media_info.ssrc_infos) {
        auto ssrc_iter = ssrc2publishers_.find(info_item.ssrc);
        if (ssrc_iter != ssrc2publishers_.end()) {
            ssrc2publishers_.erase(ssrc_iter);
            log_infof("remove publisher by ssrc:%u", info_item.ssrc);
        }
    }

    if (!publisher_ptr) {
        return;
    }
    auto pid_iter = pid2publishers_.find(publisher_ptr->get_publisher_id());
    if (pid_iter != pid2publishers_.end()) {
        pid2publishers_.erase(pid_iter);
    }
}

int rtc_base_session::remove_publisher(int mid, const std::string& media_type) {
    auto mid_iter = mid2publishers_.find(mid);
    if (mid_iter == mid2publishers_.end()) {
        log_errorf("the session has not mid:%d", mid);
        return RM_PUBLISH_ERROR;
    }
    std::shared_ptr<rtc_publisher> publisher_ptr = mid_iter->second;
    if (publisher_ptr->get_media_type() != media_type) {
        log_errorf("the session mediatype(%s) is not match %s, mid:%d",
            publisher_ptr->get_media_type().c_str(), media_type.c_str(), mid);
        return RM_PUBLISH_ERROR;
    }
    uint32_t ssrc     = publisher_ptr->get_rtp_ssrc();
    uint32_t rtx_ssrc = publisher_ptr->get_rtx_ssrc();
    mid2publishers_.erase(mid_iter);
    log_infof("remove rtc publisher from mid2publishs, mid:%d, mediatype:%s, size:%lu",
        mid, publisher_ptr->get_media_type().c_str(), mid2publishers_.size());

    auto ssrc_iter = ssrc2publishers_.find(ssrc);
    if (ssrc_iter != ssrc2publishers_.end()) {
        ssrc2publishers_.erase(ssrc_iter);
        log_infof("remove rtc publisher from ssrc2publisers, mid:%d, rtp ssrc:%u, mediatype:%s, size:%lu",
            mid, ssrc, publisher_ptr->get_media_type().c_str(), ssrc2publishers_.size());
    }

    auto pid_iter = pid2publishers_.find(publisher_ptr->get_publisher_id());
    if (pid_iter != pid2publishers_.end()) {
        pid2publishers_.erase(pid_iter);
        log_infof("remove rtc publisher from pid2publishers, pid:%s, rtp ssrc:%u, mediatype:%s, size:%lu",
            publisher_ptr->get_publisher_id().c_str(), ssrc, publisher_ptr->get_media_type().c_str(), ssrc2publishers_.size());
    }

    if (publisher_ptr->has_rtx()) {
        log_infof("remove rtc publisher from ssrc2publisers, mid:%d, rtx ssrc:%u, mediatype:%s, size:%lu",
            mid, rtx_ssrc, publisher_ptr->get_media_type().c_str(), ssrc2publishers_.size());
        ssrc_iter = ssrc2publishers_.find(rtx_ssrc);
        if (ssrc_iter != ssrc2publishers_.end()) {
            ssrc2publishers_.erase(ssrc_iter);
        }
    }
    return 0;
}

std::vector<publisher_info> rtc_base_session::get_publishs_information() {
    std::vector<publisher_info> infos;

    for (auto item : ssrc2publishers_) {
        publisher_info info;
        
        info.ssrc = item.first;
        std::shared_ptr<rtc_publisher> publisher_ptr = item.second;
        if (publisher_ptr->get_rtp_ssrc() != info.ssrc) {
            //skip rtx ssrc
            continue;
        }
        info.media_type = publisher_ptr->get_media_type();
        info.mid        = publisher_ptr->get_mid();
        info.pid        = publisher_ptr->get_publisher_id();
        
        infos.push_back(info);
    }
    return infos;
}

std::shared_ptr<rtc_publisher> rtc_base_session::get_publisher(int mid) {
    std::shared_ptr<rtc_publisher> publisher_ptr;

    auto iter = mid2publishers_.find(mid);
    if (iter == mid2publishers_.end()) {
        return publisher_ptr;
    }

    publisher_ptr = iter->second;

    return publisher_ptr;
}

std::shared_ptr<rtc_publisher> rtc_base_session::get_publisher(uint32_t ssrc) {
    std::shared_ptr<rtc_publisher> publisher_ptr;

    auto iter = ssrc2publishers_.find(ssrc);
    if (iter == ssrc2publishers_.end()) {
        return publisher_ptr;
    }

    publisher_ptr = iter->second;

    return publisher_ptr;
}

std::shared_ptr<rtc_publisher> rtc_base_session::get_publisher(std::string pid) {
    std::shared_ptr<rtc_publisher> publisher_ptr;

    auto iter = pid2publishers_.find(pid);
    if (iter == pid2publishers_.end()) {
        return publisher_ptr;
    }

    publisher_ptr = iter->second;

    return publisher_ptr;
}

void rtc_base_session::send_plaintext_data(uint8_t* data, size_t data_len) {
    if (!single_udp_server_ptr) {
        MS_THROW_ERROR("single udp server is not inited");
    }

    if (remote_address_.ip_address.empty() || remote_address_.port == 0) {
        MS_THROW_ERROR("remote address is not inited");
    }

    single_udp_server_ptr->write((char*)data, data_len, remote_address_);
}
