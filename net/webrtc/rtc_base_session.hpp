#ifndef RTC_BASC_SESSION_HPP
#define RTC_BASC_SESSION_HPP
#include "rtc_session_pub.hpp"
#include "rtc_base_session.hpp"
#include "rtc_media_info.hpp"
#include "net/udp/udp_server.hpp"
#include <vector>

class rtc_publisher;
class room_callback_interface;

class rtc_base_session
{
public:
    rtc_base_session(room_callback_interface* room, int session_direction, const rtc_media_info& media_info);
    virtual ~rtc_base_session();

public:
    void create_publisher(const MEDIA_RTC_INFO& media_info);
    void remove_publisher(const MEDIA_RTC_INFO& media_info);
    void remove_publisher(int mid);
    std::vector<publisher_info> get_publishs_information();
    
    std::shared_ptr<rtc_publisher> get_publisher(uint32_t ssrc);
    std::shared_ptr<rtc_publisher> get_publisher(int mid);

    void send_plaintext_data(uint8_t* data, size_t data_len);

public:
    virtual void send_rtp_data_in_dtls(uint8_t* data, size_t data_len) {};
    virtual void send_rtcp_data_in_dtls(uint8_t* data, size_t data_len) {};

protected:
    room_callback_interface* room_ = nullptr;
    int direction_ = 0;
    rtc_media_info media_info_;

protected:
    udp_tuple remote_address_;

private:
    std::map<uint32_t, std::shared_ptr<rtc_publisher>> ssrc2publishers_;//key: ssrc, value: rtc_publisher
    std::map<int, std::shared_ptr<rtc_publisher>> mid2publishers_;//key: mid, value: rtc_publisher
};

#endif