#include "rtc_base_session.hpp"
#include "rtc_publisher.hpp"
#include "webrtc_session.hpp"
#include "net/udp/udp_server.hpp"
#include "utils/logger.hpp"

extern std::shared_ptr<udp_server> single_udp_server_ptr;
extern single_udp_session_callback single_udp_cb;

//key: "srcip+srcport" or "username", value: webrtc_session*
extern std::unordered_map<std::string, webrtc_session*> single_webrtc_map;
extern std::string single_candidate_ip;
extern uint16_t single_candidate_port;

rtc_base_session::rtc_base_session(room_callback_interface* room,
                                int session_direction,
                                const rtc_media_info& media_info):room_(room)
                            , direction_(session_direction)
                            , media_info_(media_info) {
    log_infof("rtc_base_session construct direction:%d", direction_);
}
    
rtc_base_session::~rtc_base_session() {
    log_infof("rtc_base_session destruct direction:%d", direction_);
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

    std::shared_ptr<rtc_publisher> publisher_ptr = std::make_shared<rtc_publisher>(room_, this, media_info);

    if (media_info.mid >= 0) {
        mid2publishers_[media_info.mid] = publisher_ptr;
    }
    
    for (auto info_item : media_info.ssrc_infos) {
        ssrc2publishers_[info_item.ssrc] = publisher_ptr;
    }
}

void rtc_base_session::remove_publisher(const MEDIA_RTC_INFO& media_info) {
    auto mid_iter = mid2publishers_.find(media_info.mid);
    if (mid_iter != mid2publishers_.end()) {
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
}

void rtc_base_session::remove_publisher(int mid) {
    auto mid_iter = mid2publishers_.find(mid);
    if (mid_iter == mid2publishers_.end()) {
        return;
    }
    std::shared_ptr<rtc_publisher> publisher_ptr = mid_iter->second;
    uint32_t ssrc = publisher_ptr->get_rtp_ssrc();
    mid2publishers_.erase(mid_iter);

    auto ssrc_iter = ssrc2publishers_.find(ssrc);
    if (ssrc_iter == ssrc2publishers_.end()) {
        return;
    }
    ssrc2publishers_.erase(ssrc_iter);
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

void rtc_base_session::send_plaintext_data(uint8_t* data, size_t data_len) {
    if (!single_udp_server_ptr) {
        MS_THROW_ERROR("single udp server is not inited");
    }

    if (remote_address_.ip_address.empty() || remote_address_.port == 0) {
        MS_THROW_ERROR("remote address is not inited");
    }

    single_udp_server_ptr->write((char*)data, data_len, remote_address_);
}
