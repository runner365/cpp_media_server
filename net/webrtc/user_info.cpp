#include "user_info.hpp"
#include "support_rtc_info.hpp"
#include "utils/logger.hpp"
#include "media_stream_manager.hpp"

user_info::user_info(const std::string& uid, const std::string& roomId, protoo_request_interface* fb):uid_(uid)
    , roomId_(roomId)
    , feedback_(fb)
{
}

user_info::~user_info() {
    log_infof("user destruct, uid:%s, roomid:%s", uid_.c_str(), roomId_.c_str());

    for (auto item : publish_sessions_) {
        std::shared_ptr<webrtc_session> session_ptr = item.second;
        session_ptr->close_session();
    }
}

void user_info::reset_media_info() {
    remote_media_info_.reset();
}

json user_info::parse_remote_sdp(const std::string& sdp) {
    return remote_sdp_analyze_.parse(sdp);
}

rtc_media_info& user_info::parse_remote_media_info(json& sdp_json) {
    remote_media_info_.reset();
    remote_media_info_.parse(sdp_json);

    return remote_media_info_;
}

void user_info::get_support_media_info(rtc_media_info& input_info, rtc_media_info& support_info) {
    get_support_rtc_media(input_info, support_info);
}

std::string user_info::rtc_media_info_2_sdp(const rtc_media_info& input) {
    json sdp_json = json::object();
    rtc_media_info_to_json(input, sdp_json);

    std::string sdp_str = remote_sdp_analyze_.encode(sdp_json);

    return sdp_str;
}

void user_info::on_rtmp_callback(const std::string& stream_type, MEDIA_PACKET_PTR pkt_ptr) {
    if (stream_type == "camera") {
        on_rtmp_camera_callback(pkt_ptr);
    } else if (stream_type == "screen") {
        on_rtmp_screen_callback(pkt_ptr);
    } else {
        log_infof("rtmp callback stream type(%s) is unkown, roomId(%s), uid(%s)",
                stream_type.c_str(), roomId_.c_str(), uid_.c_str());
    }
}

void user_info::update_camera_video_dts(MEDIA_PACKET_PTR pkt_ptr) {
    const int64_t DTS_JUMP_MAX = 30*1000;
    int64_t delta_ts = 0;
    int64_t last_ts  = 0;

    if (camera_video_dts_base_ < 0) {//if video dts is not inited
        if (camera_audio_dts_base_ < 0) {//if audio dts is not inited as well
            camera_video_dts_base_ = 0;
        } else {//if audio dts is inited already
            camera_video_dts_base_ = camera_audio_dts_base_;
        }
        camera_last_video_rtp_dts_ = pkt_ptr->dts_;
        last_ts = camera_video_dts_base_;
        delta_ts = 0;
    } else {//if video dts is inited already
        if (pkt_ptr->dts_ >= camera_last_video_rtp_dts_) {//DTS increase...
            int64_t diff_t = pkt_ptr->dts_ - camera_last_video_rtp_dts_;
            if (diff_t < DTS_JUMP_MAX) {//dts increase normally
                delta_ts = diff_t;
                last_ts  = camera_video_dts_base_;

                camera_last_video_rtp_dts_ = pkt_ptr->dts_;
                camera_video_dts_base_ += delta_ts;
            } else {//dts increase a lot
                last_ts  = camera_video_dts_base_;
                delta_ts = 30;

                camera_video_dts_base_ += delta_ts;//increase 30ms
                camera_last_video_rtp_dts_ = pkt_ptr->dts_;
            }
        } else {//DTS decrease
            int64_t diff_t = camera_last_video_rtp_dts_ - pkt_ptr->dts_;
            if (diff_t > (300 * 1000)) {//dts reverse
                last_ts  = camera_video_dts_base_;
                delta_ts = 30;

                camera_video_dts_base_ += delta_ts;//increase 30ms
                camera_last_video_rtp_dts_ = pkt_ptr->dts_;
            } else {//decrease a little
                (void)camera_video_dts_base_;
                camera_last_video_rtp_dts_ = pkt_ptr->dts_;

                last_ts = camera_video_dts_base_;
                delta_ts = 0;
            }
        }
    }
    if (delta_ts > 5) {
        delta_ts -= 5;
    }
    pkt_ptr->dts_ = last_ts + delta_ts;
    pkt_ptr->pts_ = last_ts + delta_ts;

    return;
}

void user_info::update_camera_audio_dts(MEDIA_PACKET_PTR pkt_ptr) {
    const int64_t DTS_JUMP_MAX = 30*1000;
    int64_t delta_ts = 0;
    int64_t last_ts  = 0;

    if (camera_audio_dts_base_ < 0) {//if audio dts is not inited
        if (camera_video_dts_base_ < 0) {//if video dts is not inited as well
            camera_audio_dts_base_ = 0;
        } else {//if audio dts is inited already
            camera_audio_dts_base_ = camera_video_dts_base_;
        }
        camera_last_audio_rtp_dts_ = pkt_ptr->dts_;
        last_ts = camera_audio_dts_base_;
        delta_ts = 0;
    } else {//if audio dts is inited already
        if (pkt_ptr->dts_ >= camera_last_audio_rtp_dts_) {//DTS increase...
            int64_t diff_t = pkt_ptr->dts_ - camera_last_audio_rtp_dts_;
            if (diff_t < DTS_JUMP_MAX) {//dts increase normally
                delta_ts = diff_t;
                last_ts  = camera_audio_dts_base_;

                camera_last_audio_rtp_dts_ = pkt_ptr->dts_;
                camera_audio_dts_base_ += delta_ts;
            } else {//dts increase a lot
                last_ts  = camera_audio_dts_base_;
                delta_ts = 30;

                camera_audio_dts_base_ += delta_ts;//increase 30ms
                camera_last_audio_rtp_dts_ = pkt_ptr->dts_;
            }
        } else {//DTS decrease
            int64_t diff_t = camera_last_audio_rtp_dts_ - pkt_ptr->dts_;
            if (diff_t > (300 * 1000)) {//dts reverse
                last_ts  = camera_audio_dts_base_;
                delta_ts = 30;

                camera_audio_dts_base_ += delta_ts;//increase 30ms
                camera_last_audio_rtp_dts_ = pkt_ptr->dts_;
            } else {//decrease a little
                (void)camera_audio_dts_base_;
                camera_last_audio_rtp_dts_ = pkt_ptr->dts_;

                last_ts = camera_audio_dts_base_;
                delta_ts = 0;
            }
        }
    }

    if (delta_ts > 5) {
        delta_ts -= 5;
    }
    pkt_ptr->dts_ = last_ts + delta_ts;
    pkt_ptr->pts_ = last_ts + delta_ts;
}

void user_info::on_rtmp_camera_callback(MEDIA_PACKET_PTR pkt_ptr) {

    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        update_camera_video_dts(pkt_ptr);
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        update_camera_audio_dts(pkt_ptr);
    } else {
        log_errorf("rtmp camera callback av type(%d) is unkown, roomId(%s), uid(%s)",
                pkt_ptr->av_type_, roomId_.c_str(), uid_.c_str());
        return;
    }

    //log_infof("rtmp camera write %s dts:%ld", (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) ? "video" : "audio",
    //        pkt_ptr->dts_);
    media_stream_manager::writer_media_packet(pkt_ptr);
}

void user_info::on_rtmp_screen_callback(MEDIA_PACKET_PTR pkt_ptr) {

}