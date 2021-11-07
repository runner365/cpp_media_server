#ifndef RTC_BASC_SESSION_HPP
#define RTC_BASC_SESSION_HPP
#include "rtc_session_pub.hpp"
#include "rtc_base_session.hpp"
#include "rtc_media_info.hpp"
#include "net/udp/udp_server.hpp"
#include <vector>

class rtc_publisher;
class rtc_subscriber;
class room_callback_interface;

class rtc_base_session
{
public:
    rtc_base_session(const std::string& roomId, const std::string& uid, room_callback_interface* room, int session_direction, const rtc_media_info& media_info);
    virtual ~rtc_base_session();

public:
    void create_publisher(const MEDIA_RTC_INFO& media_info);
    void remove_publisher(const MEDIA_RTC_INFO& media_info);
    int remove_publisher(int mid, const std::string& media_type);
    std::vector<publisher_info> get_publishs_information();
    
    std::shared_ptr<rtc_publisher> get_publisher(uint32_t ssrc);
    std::shared_ptr<rtc_publisher> get_publisher(int mid);
    std::shared_ptr<rtc_publisher> get_publisher(std::string pid);
    size_t get_publisher_count() {return mid2publishers_.size();}//ssrc2publishers_ may have multple publish for rtx ssrc

public:
    std::shared_ptr<rtc_subscriber> create_subscriber(const std::string& remote_uid, const MEDIA_RTC_INFO& media_info, const std::string& pid);
    std::shared_ptr<rtc_subscriber> get_subscriber(uint32_t ssrc);

public:
    rtc_media_info get_media_info() {return media_info_;}
    udp_tuple get_remote_info() {return remote_address_;}

    void send_plaintext_data(uint8_t* data, size_t data_len);

public:
    virtual void send_rtp_data_in_dtls(uint8_t* data, size_t data_len) {};
    virtual void send_rtcp_data_in_dtls(uint8_t* data, size_t data_len) {};

public:
    std::string roomId_;
    std::string uid_;

protected:
    room_callback_interface* room_ = nullptr;
    int direction_ = 0;
    rtc_media_info media_info_;

protected:
    udp_tuple remote_address_;

private:
    std::map<std::string, std::shared_ptr<rtc_publisher>> pid2publishers_;//key: pid, value: rtc_publisher
    std::map<uint32_t, std::shared_ptr<rtc_publisher>> ssrc2publishers_;//key: ssrc, value: rtc_publisher
    std::map<int, std::shared_ptr<rtc_publisher>> mid2publishers_;//key: mid, value: rtc_publisher

private:
    std::map<int, std::shared_ptr<rtc_subscriber>> mid2subscribers_;         //key: mid, value: rtc_subscriber
    std::map<uint32_t, std::shared_ptr<rtc_subscriber>> ssrc2subscribers_;   //key: ssrc, value: rtc_subscriber
    std::map<std::string, std::shared_ptr<rtc_subscriber>> pid2subscribers_; //key: publisher_id, value: rtc_subscriber
};

#endif