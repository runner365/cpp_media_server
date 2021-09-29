#ifndef RTC_BASC_SESSION_HPP
#define RTC_BASC_SESSION_HPP
#include "rtc_session_pub.hpp"
#include "rtc_base_session.hpp"

class rtc_base_session
{
public:
    rtc_base_session(int session_direction);
    virtual ~rtc_base_session();

private:
    int direction_ = 0;
};

#endif