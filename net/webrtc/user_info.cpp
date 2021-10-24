#include "user_info.hpp"
#include "support_rtc_info.hpp"
#include "utils/logger.hpp"

user_info::user_info(const std::string& uid, const std::string& roomId, protoo_request_interface* fb):uid_(uid)
    , roomId_(roomId)
    , feedback_(fb)
{
}

user_info::~user_info() {
    log_infof("user destruct, uid:%s, roomid:%s", uid_.c_str(), roomId_.c_str());
    if (publish_session_ptr_.get() != nullptr) {
        publish_session_ptr_->close_session();
    }
}

json user_info::parse_remote_sdp(const std::string& sdp) {
    return remote_sdp_analyze_.parse(sdp);
}

rtc_media_info& user_info::parse_remote_media_info(json& sdp_json) {
    remote_media_info_.parse(sdp_json);

    return remote_media_info_;
}

void user_info::get_support_media_info(rtc_media_info& input_info, rtc_media_info& support_info) {
    get_support_rtc_media(input_info, support_info);
}

std::string user_info::rtc_media_info_2_sdp(const rtc_media_info& input) {
    json sdp_json = json::object();
    rtc_media_info_to_json(input, sdp_json);

    std::string sdp_str = remote_sdp_analyze_.encode(sdp_json);

    return sdp_str;
}