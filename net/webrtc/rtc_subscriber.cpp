#include "rtc_subscriber.hpp"

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