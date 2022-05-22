#ifndef USER_INFO_HPP
#define USER_INFO_HPP
#include "net/websocket/wsimple/protoo_pub.hpp"
#include "net/webrtc/rtc_session_pub.hpp"
#include "transcode/transcode.hpp"
#include "utils/av/media_packet.hpp"
#include "utils/timeex.hpp"
#include "sdp_analyze.hpp"
#include "rtc_media_info.hpp"
#include "webrtc_session.hpp"
#include "udp/udp_server.hpp"
#include <string>
#include <stdlib.h>
#include <memory>
#include <map>
#include <queue>

std::string make_cname();

class user_info
{
public:
    user_info(const std::string& uid, const std::string& roomId, protoo_request_interface* fb);
    user_info(const std::string& uid, const std::string& roomId);
    ~user_info();

public:
    std::string uid() {return uid_;}
    std::string roomId() {return roomId_;}
    std::string user_type() {return user_type_;}
    protoo_request_interface* feedback() { return fb_; }
    int64_t active_last_ms() { return active_last_ms_; }
    void update_alive(int64_t now_ms) { active_last_ms_ = now_ms; }

public:
    json parse_remote_sdp(const std::string& sdp);
    rtc_media_info& parse_remote_media_info(json& sdp_json);
    void reset_media_info();

public:
    void get_support_media_info(rtc_media_info& input_info, rtc_media_info& support_info);
    std::string rtc_media_info_2_sdp(const rtc_media_info& input);
    void on_rtmp_callback(const std::string& stream_type, MEDIA_PACKET_PTR pkt_ptr);

private:
    void on_rtmp_camera_callback(MEDIA_PACKET_PTR pkt_ptr);
    void on_rtmp_screen_callback(MEDIA_PACKET_PTR pkt_ptr);
    void update_camera_video_dts(MEDIA_PACKET_PTR pkt_ptr);
    void update_camera_audio_dts(MEDIA_PACKET_PTR pkt_ptr);
    void send_buffer(MEDIA_PACKET_PTR pkt_ptr);

public:
    std::map<std::string, std::shared_ptr<webrtc_session>> publish_sessions_;//key: pc_id, value: webrtc_session
    std::map<std::string, std::shared_ptr<webrtc_session>> subscribe_sessions_;//key: pc_id, value: webrtc_session

private:
    std::multimap<int64_t, MEDIA_PACKET_PTR> send_buffer_;

private:
    std::string uid_;
    std::string roomId_;
    std::string user_type_ = "websocket";//1) websocket; 2) whip
    protoo_request_interface* fb_ = nullptr;
    int64_t active_last_ms_ = 0;

private:
    sdp_analyze remote_sdp_analyze_;

private:
    rtc_media_info remote_media_info_;

private:
    int64_t camera_video_dts_base_     = -1;
    int64_t camera_audio_dts_base_     = -1;
    int64_t camera_video_rtp_dts_base_ = -1;
    int64_t camera_audio_rtp_dts_base_ = -1;

    int64_t camera_last_video_rtp_dts_ = -1;
    int64_t camera_last_audio_rtp_dts_ = -1;
    int64_t camera_last_video_pkt_dts_ = -1;
    int64_t camera_last_audio_pkt_dts_ = -1;

private:
    transcode* trans_ = nullptr;
};

class live_user_info
{
public:
    //app: roomId, streamname: uid
    live_user_info(const std::string& uid, const std::string& roomId, room_callback_interface* room_callbacl_p);
    ~live_user_info();

public:
    std::string uid() { return uid_; }
    std::string roomId() { return roomId_; }

public:
    bool media_ready() { return media_ready_; }
    void set_media_ready(bool flag) { media_ready_ = flag; }

public:
    void set_video(bool flag) { has_video_ = flag; }
    void set_audio(bool flag) { has_audio_ = flag; }
    bool has_video() { return has_video_; }
    bool has_audio() { return has_audio_; }

    void set_video_ssrc(uint32_t ssrc) { video_ssrc_ = ssrc; }
    void set_video_rtx_ssrc(uint32_t ssrc) { video_rtx_ssrc_ = ssrc; }
    void set_audio_ssrc(uint32_t ssrc) { audio_ssrc_ = ssrc; }
    uint32_t video_ssrc() { return video_ssrc_; }
    uint32_t video_rtx_ssrc() { return video_rtx_ssrc_; }
    uint32_t audio_ssrc() { return audio_ssrc_; }
    uint8_t video_payload() { return video_payloadtype_; }
    uint8_t audio_payload() { return audio_payloadtype_; }
    uint8_t video_rtx_payload() { return video_rtx_payloadtype_; }
    void set_video_payload(uint8_t payload) { video_payloadtype_ = payload; }
    void set_audio_payload(uint8_t payload) { audio_payloadtype_ = payload; }
    void set_video_rtx_payload(uint8_t payload) { video_rtx_payloadtype_ = payload; }
    void set_video_mid(uint8_t mid) { video_mid_ = mid; }
    void set_audio_mid(uint8_t mid) { audio_mid_ = mid; }
    uint32_t video_mid() { return video_mid_; }
    uint32_t audio_mid() { return audio_mid_; }
    void set_video_clock(int64_t clock) { video_clock_ = clock; }
    void set_audio_clock(int64_t clock) { audio_clock_ = clock; }
    int64_t video_clock() { return video_clock_; }
    int64_t audio_clock() { return audio_clock_; }

    std::string video_msid() { return video_msid_; }
    std::string audio_msid() { return audio_msid_; }
    void set_video_msid(const std::string& msid) { video_msid_ = "-"; video_msid_ += " "; video_msid_ += msid; }
    void set_audio_msid(const std::string& msid) { audio_msid_ = "_"; audio_msid_ += " "; audio_msid_ += msid; }
    std::string make_video_cname() {
        video_cname_ = make_cname();
        return video_cname_;
    }
    std::string make_audio_cname() {
        audio_cname_ = make_cname();
        return audio_cname_;
    }
    std::string get_video_cname() { return video_cname_; }
    std::string get_audio_cname() { return audio_cname_; }

    int handle_video_data(MEDIA_PACKET_PTR pkt_ptr);
    int handle_audio_data(MEDIA_PACKET_PTR pkt_ptr);

    int64_t active_last_ms() { return active_last_ms_; }

public:
    std::queue<MEDIA_PACKET_PTR> pkt_queue_;

private:
    void updata_sps(uint8_t* sps, size_t sps_len);
    void updata_pps(uint8_t* pps, size_t pps_len);

private:
    std::string uid_;
    std::string roomId_;
    room_callback_interface* room_cb_ = nullptr;
    bool media_ready_ = false;
    bool has_video_ = false;
    bool has_audio_ = false;
    uint32_t video_ssrc_ = 0;
    uint32_t video_rtx_ssrc_ = 0;
    uint32_t audio_ssrc_ = 0;
    uint8_t video_payloadtype_     = 0;
    uint8_t video_rtx_payloadtype_ = 0;
    uint8_t audio_payloadtype_     = 0;
    int64_t video_clock_ = 0;
    int64_t audio_clock_ = 0;
    uint8_t video_mid_  = 0;
    uint8_t audio_mid_  = 0;
    std::string video_msid_;
    std::string audio_msid_;
    std::string video_cname_;
    std::string audio_cname_;

private:
    uint16_t vseq_ = 0;
    uint16_t aseq_ = 0;
    int64_t active_last_ms_ = 0;

private:
    std::vector<uint8_t> pps_;
    std::vector<uint8_t> sps_;

private:
    transcode* trans_ = nullptr;
    uint8_t aac_type_ = 0;
    int sample_rate_  = 0;
    uint8_t channel_  = 0;
};

#endif