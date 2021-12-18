#ifndef USER_INFO_HPP
#define USER_INFO_HPP
#include "net/websocket/wsimple/protoo_pub.hpp"
#include "sdp_analyze.hpp"
#include "rtc_media_info.hpp"
#include "webrtc_session.hpp"
#include "udp/udp_server.hpp"
#include <string>
#include <memory>
#include <map>

class user_info
{
public:
    user_info(const std::string& uid, const std::string& roomId, protoo_request_interface* fb);
    ~user_info();

public:
    std::string uid() {return uid_;}
    std::string roomId() {return roomId_;}
    protoo_request_interface* feedback() {return feedback_;}

public:
    json parse_remote_sdp(const std::string& sdp);
    rtc_media_info& parse_remote_media_info(json& sdp_json);
    void reset_media_info();

public:
    void get_support_media_info(rtc_media_info& input_info, rtc_media_info& support_info);
    std::string rtc_media_info_2_sdp(const rtc_media_info& input);

public:
    void on_rtmp_callback(const std::string& stream_type, MEDIA_PACKET_PTR pkt_ptr);

private:
    void on_rtmp_camera_callback(MEDIA_PACKET_PTR pkt_ptr);
    void on_rtmp_screen_callback(MEDIA_PACKET_PTR pkt_ptr);
    void update_camera_video_dts(MEDIA_PACKET_PTR pkt_ptr);
    void update_camera_audio_dts(MEDIA_PACKET_PTR pkt_ptr);

public:
    std::map<std::string, std::shared_ptr<webrtc_session>> publish_sessions_;//key: pc_id, value: webrtc_session
    std::map<std::string, std::shared_ptr<webrtc_session>> subscribe_sessions_;//key: pc_id, value: webrtc_session

private:
    std::string uid_;
    std::string roomId_;
    protoo_request_interface* feedback_ = nullptr;

private:
    sdp_analyze remote_sdp_analyze_;

private:
    rtc_media_info remote_media_info_;

private:
    int64_t camera_video_dts_base_     = -1;
    int64_t camera_audio_dts_base_     = -1;
    int64_t camera_last_video_rtp_dts_ = -1;
    int64_t camera_last_audio_rtp_dts_ = -1;
};

#endif