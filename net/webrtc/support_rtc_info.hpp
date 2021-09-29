#ifndef SUPPORT_RTC_INFO_HPP
#define SUPPORT_RTC_INFO_HPP

#include "rtc_media_info.hpp"
#include "sdptransform.hpp"

void get_support_rtc_media(const rtc_media_info& input, rtc_media_info& support_rtc_media);
void rtc_media_info_to_json(const rtc_media_info& input, json& sdp_json);

#endif