#ifndef TIME_EX_HPP
#define TIME_EX_HPP
#include <chrono>
#include <stdint.h>
#include <string>
#include <cmath>

#define NTP_FRACT_UNIT (1LL << 32)

typedef struct NTP_TIMESTAMP_S
{
    uint32_t ntp_sec;
    uint32_t ntp_frac;
} NTP_TIMESTAMP;

inline int64_t now_millisec() {
    std::chrono::system_clock::duration d = std::chrono::system_clock::now().time_since_epoch();

    std::chrono::milliseconds mil = std::chrono::duration_cast<std::chrono::milliseconds>(d);

    return (int64_t)mil.count();
}

inline int64_t now_microsec() {
    std::chrono::steady_clock::duration d = std::chrono::steady_clock::now().time_since_epoch();

    std::chrono::microseconds mil = std::chrono::duration_cast<std::chrono::microseconds>(d);

    return int64_t(mil.count());
}

inline std::string get_now_str() {
    std::time_t t = std::time(nullptr);
    auto tm = std::localtime(&t);
    char dscr_sz[80];

    int now_ms = (int)(now_millisec()%1000);

    sprintf(dscr_sz, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
        tm->tm_year+1900, tm->tm_mon, tm->tm_mday, 
        tm->tm_hour, tm->tm_min, tm->tm_sec, now_ms%1000);
    std::string desc(dscr_sz);
    return desc;
}

inline NTP_TIMESTAMP millisec_to_ntp(int64_t ms) {
    NTP_TIMESTAMP ntp;
    ntp.ntp_sec = ms / 1000;
    ntp.ntp_frac =(uint32_t)(((double)(ms % 1000) / 1000) * NTP_FRACT_UNIT);

    return ntp;
}

inline int64_t ntp_to_millisec(const NTP_TIMESTAMP& ntp) {
    return (int64_t)(ntp.ntp_sec) * 1000 + (int64_t)(std::round(((double)(ntp.ntp_frac) * 1000) / NTP_FRACT_UNIT));
}

inline double abs_time_to_ms(uint32_t abs_time_24bits) {
    const int kAbsSendTimeInterArrivalUpshift = 8;
    const int kAbsSendTimeFraction = 18;
    const int kInterArrivalShift = kAbsSendTimeFraction + kAbsSendTimeInterArrivalUpshift;
    const double kTimestampToMs = 1000.0 / static_cast<double>(1 << kInterArrivalShift);
    
    uint32_t timestamp = abs_time_24bits << kAbsSendTimeInterArrivalUpshift;
    double send_time_ms = static_cast<int64_t>(timestamp) * kTimestampToMs;
    
    return send_time_ms;
}

#endif //TIME_EX_HPP