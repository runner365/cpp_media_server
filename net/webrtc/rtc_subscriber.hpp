#ifndef RTC_SUBSCRIBER_HPP
#define RTC_SUBSCRIBER_HPP
#include "utils/uuid.hpp"
#include "rtc_media_info.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>

class rtc_base_session;
class rtc_subscriber
{
public:
    rtc_subscriber(const std::string& roomId, const std::string& uid, const std::string& remote_uid, const std::string& pid
            , rtc_base_session* session, const MEDIA_RTC_INFO& media_info);
    ~rtc_subscriber();

private:
    std::string roomId_;
    std::string uid_;
    std::string remote_uid_;
    std::string pid_;
    std::string sid_;
    rtc_base_session* session_ = nullptr;
    MEDIA_RTC_INFO media_info_;
};



#endif