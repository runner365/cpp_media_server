#include "rtc_subscriber.hpp"
#include "net/rtprtcp/rtp_packet.hpp"
#include "logger.hpp"

rtc_subscriber::rtc_subscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid, const std::string& pid
    , rtc_base_session* session, const MEDIA_RTC_INFO& media_info):roomId_(roomId)
            , uid_(uid)
            , remote_uid_(remote_uid)
            , pid_(pid)
            , session_(session)
{
    sid_ = make_uuid();

    media_info_ = media_info;
}

rtc_subscriber::~rtc_subscriber() {

}

void rtc_subscriber::send_rtp_packet(const std::string& roomId, const std::string& media_type,
                    const std::string& publish_id, rtp_packet* pkt) {
    log_infof("subscriber receive rtp packet roomid:%s, media type:%s, publisher:%s, pkt dump:\r\n%s",
        roomId.c_str(), media_type.c_str(), publish_id.c_str(), pkt->dump().c_str());
    return;
}