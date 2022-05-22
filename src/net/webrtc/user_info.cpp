#include "user_info.hpp"
#include "support_rtc_info.hpp"
#include "utils/logger.hpp"
#include "utils/timeex.hpp"
#include "net/webrtc/rtp_h264_pack.hpp"
#include "format/h264_header.hpp"
#include "format/audio_pub.hpp"
#include "media_stream_manager.hpp"
#include <stdlib.h>

static int64_t s_last_ts = 0;

std::string make_cname() {
    int64_t now_ms = now_millisec();
    std::string cname;

    if (s_last_ts == 0) {
        s_last_ts = now_ms;
        srand((unsigned int)now_ms);
    } else {
        if (s_last_ts != now_ms) {
            s_last_ts = now_ms;
            srand((unsigned int)now_ms);
        }
    }
    char base = 'a';

    for (int i = 0; i < 32; i++) {
        int index = rand()%26;
        char value = base + index;
        cname.push_back(value);
    }
    return cname;
}

user_info::user_info(const std::string& uid, const std::string& roomId, protoo_request_interface* fb):uid_(uid), roomId_(roomId)
{
    fb_ = fb;
    user_type_ = "websocket";
    active_last_ms_ = now_millisec();
}

user_info::user_info(const std::string& uid, const std::string& roomId):uid_(uid), roomId_(roomId)
{
    user_type_ = "whip";
    active_last_ms_ = now_millisec();
}

user_info::~user_info() {
    log_infof("user destruct, uid:%s, roomid:%s", uid_.c_str(), roomId_.c_str());

    for (auto item : publish_sessions_) {
        std::shared_ptr<webrtc_session> session_ptr = item.second;
        session_ptr->close_session();
    }
    if (trans_) {
        trans_->stop();
        delete trans_;
        trans_ = nullptr;
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
        log_errorf("rtmp callback stream type(%s) is unkown, roomId(%s), uid(%s)",
                stream_type.c_str(), roomId_.c_str(), uid_.c_str());
    }
}

void user_info::update_camera_video_dts(MEDIA_PACKET_PTR pkt_ptr) {
    const int64_t DTS_JUMP_MAX = 30*1000;

    if (camera_video_dts_base_ < 0) {//if video dts is not inited
        if (camera_audio_dts_base_ < 0) {//if audio dts is not inited as well
            camera_video_dts_base_ = 0;
        } else {//if audio dts is inited already
            camera_video_dts_base_ = camera_audio_dts_base_;
        }
        camera_last_video_rtp_dts_    = pkt_ptr->dts_;
        camera_video_rtp_dts_base_    = pkt_ptr->dts_;
        pkt_ptr->pts_ = pkt_ptr->dts_ = camera_video_dts_base_;
        camera_last_video_pkt_dts_    = camera_video_dts_base_;
    } else {//if video dts is inited already
        if (pkt_ptr->dts_ >= camera_last_video_rtp_dts_) {//DTS increase...
            int64_t diff_t = pkt_ptr->dts_ - camera_last_video_rtp_dts_;
            camera_last_video_rtp_dts_ = pkt_ptr->dts_;
            if (diff_t < DTS_JUMP_MAX) {//dts increase normally
                int64_t incr = pkt_ptr->dts_ - camera_video_rtp_dts_base_;
                pkt_ptr->dts_ = camera_video_dts_base_ + incr;
                pkt_ptr->pts_ = pkt_ptr->dts_;
                camera_last_video_pkt_dts_ = pkt_ptr->dts_;
            } else {//dts increase a lot
                log_infof("camera video dts increase a lot:%ld", diff_t);
                camera_video_dts_base_        = camera_last_video_pkt_dts_;
                camera_video_rtp_dts_base_    = pkt_ptr->dts_;
                pkt_ptr->dts_ = pkt_ptr->pts_ = camera_last_video_pkt_dts_;
            }
        } else {//DTS decrease
            int64_t diff_t = camera_last_video_rtp_dts_ - pkt_ptr->dts_;
            camera_last_video_rtp_dts_ = pkt_ptr->dts_;
            if (diff_t > (300 * 1000)) {//dts reverse
                log_infof("camera video dts decrease a lot:%ld", diff_t);
                camera_video_dts_base_        = camera_last_video_pkt_dts_;
                camera_video_rtp_dts_base_    = pkt_ptr->dts_;
                pkt_ptr->dts_ = pkt_ptr->pts_ = camera_last_video_pkt_dts_;
            } else {//decrease a little
                log_infof("camera video dts decrease a little:%ld", diff_t);
                pkt_ptr->dts_ = pkt_ptr->pts_ = camera_last_video_pkt_dts_;
            }
        }
    }
    return;
}

void user_info::update_camera_audio_dts(MEDIA_PACKET_PTR pkt_ptr) {
    const int64_t DTS_JUMP_MAX = 30*1000;

    if (camera_audio_dts_base_ < 0) {//if audio dts is not inited
        if (camera_video_dts_base_ < 0) {//if video dts is not inited as well
            camera_audio_dts_base_ = 0;
        } else {//if audio dts is inited already
            camera_audio_dts_base_ = camera_video_dts_base_;
        }
        camera_last_audio_rtp_dts_ = pkt_ptr->dts_;
        camera_audio_rtp_dts_base_ = pkt_ptr->dts_;
        pkt_ptr->pts_ = pkt_ptr->dts_ = camera_audio_dts_base_;
        camera_last_audio_pkt_dts_ = pkt_ptr->dts_;
    } else {//if audio dts is inited already
        if (pkt_ptr->dts_ >= camera_last_audio_rtp_dts_) {//DTS increase...
            int64_t diff_t = pkt_ptr->dts_ - camera_last_audio_rtp_dts_;
            camera_last_audio_rtp_dts_ = pkt_ptr->dts_;
            if (diff_t < DTS_JUMP_MAX) {//dts increase normally
                int64_t incr = pkt_ptr->dts_ - camera_audio_rtp_dts_base_;
                pkt_ptr->dts_ = camera_audio_dts_base_ + incr;
                pkt_ptr->pts_ = pkt_ptr->dts_;
                camera_last_audio_pkt_dts_ = pkt_ptr->dts_;
            } else {//dts increase a lot
                log_infof("camera audio dts increase a lot:%ld", diff_t);
                camera_audio_dts_base_        = camera_last_audio_pkt_dts_;
                camera_audio_rtp_dts_base_    = pkt_ptr->dts_;
                pkt_ptr->dts_ = pkt_ptr->pts_ = camera_last_audio_pkt_dts_;
            }
        } else {//DTS decrease
            int64_t diff_t = camera_last_audio_rtp_dts_ - pkt_ptr->dts_;
            camera_last_audio_rtp_dts_ = pkt_ptr->dts_;
            if (diff_t > (300 * 1000)) {//dts reverse
                log_infof("camera audio dts decrease a lot:%ld", diff_t);
                camera_audio_dts_base_        = camera_last_audio_pkt_dts_;
                camera_audio_rtp_dts_base_    = pkt_ptr->dts_;
                pkt_ptr->dts_ = pkt_ptr->pts_ = camera_last_audio_pkt_dts_;
            } else {//decrease a little
                log_infof("camera audio dts decrease a little:%ld", diff_t);
                pkt_ptr->dts_ = pkt_ptr->pts_ = camera_last_audio_pkt_dts_;
            }
        }
    }
    return;
}

void user_info::on_rtmp_camera_callback(MEDIA_PACKET_PTR pkt_ptr) {
    //int64_t org_dts = pkt_ptr->dts_;

    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        update_camera_video_dts(pkt_ptr);
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        update_camera_audio_dts(pkt_ptr);
    } else {
        log_errorf("rtmp camera callback av type(%d) is unkown, roomId(%s), uid(%s)",
                pkt_ptr->av_type_, roomId_.c_str(), uid_.c_str());
        return;
    }
    if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        if (!trans_) {
            trans_ = new transcode();
            trans_->set_output_audio_fmt("libfdk_aac");
            trans_->set_output_format(MEDIA_FORMAT_FLV);
            trans_->set_output_audio_samplerate(44100);
            trans_->start();
        }
        trans_->send_transcode(pkt_ptr, 2);
        
        while(true) {
            MEDIA_PACKET_PTR ret_pkt_ptr = trans_->recv_transcode();
            if (!ret_pkt_ptr) {
                break;
            }
            ret_pkt_ptr->key_        = pkt_ptr->key_;
            ret_pkt_ptr->app_        = pkt_ptr->app_;
            if (ret_pkt_ptr->is_seq_hdr_) {
                log_infof("audio seq dts:%ld, pts:%ld",
                        ret_pkt_ptr->dts_, ret_pkt_ptr->pts_);
                log_info_data((uint8_t*)ret_pkt_ptr->buffer_ptr_->data(),
                        ret_pkt_ptr->buffer_ptr_->data_len(),
                        "audio seq data");
            }           
            ret_pkt_ptr->streamname_ = pkt_ptr->streamname_;
            //log_infof("media type:%s, origin dts:%ld, dts:%ld", 
            //        avtype_tostring(ret_pkt_ptr->av_type_).c_str(), org_dts, ret_pkt_ptr->dts_);
            media_stream_manager::writer_media_packet(ret_pkt_ptr);
            //send_buffer(ret_pkt_ptr);
        }
       return;
    }

    //log_infof("media type:%s, origin dts:%ld, dts:%ld", 
    //        avtype_tostring(pkt_ptr->av_type_).c_str(), org_dts, pkt_ptr->dts_);
    media_stream_manager::writer_media_packet(pkt_ptr);
    //send_buffer(pkt_ptr);
}

void user_info::send_buffer(MEDIA_PACKET_PTR pkt_ptr) {
    send_buffer_.insert(std::make_pair(pkt_ptr->dts_, pkt_ptr));

    while(send_buffer_.size() > 10) {
        auto iter = send_buffer_.begin();
        media_stream_manager::writer_media_packet(iter->second);
        iter = send_buffer_.erase(iter);
    }
}

void user_info::on_rtmp_screen_callback(MEDIA_PACKET_PTR pkt_ptr) {

}

live_user_info::live_user_info(const std::string& uid,
        const std::string& roomId,
        room_callback_interface* room_callbacl_p): uid_(uid)
                                                 , roomId_(roomId)
                                                 , room_cb_(room_callbacl_p)
{
    active_last_ms_ = now_millisec();
}

live_user_info::~live_user_info()
{
    if (trans_) {
        trans_->stop();
        delete trans_;
        trans_ = nullptr;
    }
}

void live_user_info::updata_sps(uint8_t* sps, size_t sps_len) {
    if (sps_.empty()) {
        sps_.reserve(sps_len);
    } else {
        sps_.resize(sps_len);
        sps_.clear();
    }

    sps_.insert(sps_.end(), (uint8_t*)sps, (uint8_t*)sps + sps_len);
    return;
}

void live_user_info::updata_pps(uint8_t* pps, size_t pps_len) {
    if (pps_.empty()) {
        pps_.reserve(pps_len);
    } else {
        pps_.resize(pps_len);
        pps_.clear();
    }

    pps_.insert(pps_.end(), (uint8_t*)pps, (uint8_t*)pps + pps_len);
    return;
}

int live_user_info::handle_video_data(MEDIA_PACKET_PTR pkt_ptr) {
    int ret = 0;

    std::string publisher_id = uid_;
    publisher_id += "_";
    publisher_id += "video";

    if (pkt_ptr->codec_type_ == MEDIA_CODEC_H264) {
        uint8_t sps[1024];
        uint8_t pps[1024];
        size_t sps_len;
        size_t pps_len;
        uint8_t* p = (uint8_t*)pkt_ptr->buffer_ptr_->data() + 5;
        size_t data_len = pkt_ptr->buffer_ptr_->data_len() - 5;
        std::vector<std::shared_ptr<data_buffer>> nalus;

        active_last_ms_ = now_millisec();
        //update h264 sps/pps
        if (pkt_ptr->is_seq_hdr_) {
            ret = get_sps_pps_from_extradata(pps, pps_len,
                                sps, sps_len,
                                p, data_len);
            if (ret < 0) {
                log_errorf("live user receive wrong h264 extra data");
                log_info_data((uint8_t*)pkt_ptr->buffer_ptr_->data(),
                        pkt_ptr->buffer_ptr_->data_len(),
                        "wrong video h264 seq data");
                return ret;
            }

            updata_pps(pps, pps_len);
            updata_sps(sps, sps_len);
            return ret;
        }

        bool ok = annexb_to_nalus(p, data_len, nalus);
        if (!ok) {
            log_errorf("annexb to nalus error, data len:%lu", data_len);
            return -1;
        }

        for (auto& buffer_ptr : nalus) {
            uint8_t* data = (uint8_t*)buffer_ptr->data() + 4;
            int64_t data_len = buffer_ptr->data_len() - 4;

            if ((data[0] & 0x1f) == kSps) {
                updata_sps(data, data_len);
                continue;
            }
            if ((data[0] & 0x1f) == kPps) {
                continue;
            }

            if ((sps_.size() <= 0) || (pps_.size() <= 0)) {
                log_infof("sps size:%lu, pps size:%lu", sps_.size(), pps_.size());
                continue;
            }
            if ((data[0] & 0x1f) == kFiller) {
                continue;
            }
            bool isIdr = ((data[0] & 0x1f) == kIdr);
            if (isIdr) {
                //send sps/pps before key frame
                std::vector<std::pair<uint8_t*, int>> data_vec;
    
                data_vec.push_back({sps_.data(), sps_.size()});
                data_vec.push_back({pps_.data(), pps_.size()});
                rtp_packet* sps_pps_pkt = generate_stapA_packets(data_vec);
                sps_pps_pkt->set_seq(vseq_++);
                sps_pps_pkt->set_ssrc(video_ssrc_);
                sps_pps_pkt->set_timestamp((uint32_t)pkt_ptr->dts_);
                room_cb_->on_rtppacket_publisher2room(publisher_id, "video", sps_pps_pkt);
            }

            if (data_len > RTP_PAYLOAD_MAX_SIZE) {
                std::vector<rtp_packet*> fuA_vec = generate_fuA_packets(data, data_len);
                for (auto fuA_pkt : fuA_vec) {
                    fuA_pkt->set_seq(vseq_++);
                    fuA_pkt->set_ssrc(video_ssrc_);
                    fuA_pkt->set_timestamp((uint32_t)pkt_ptr->dts_);
                    room_cb_->on_rtppacket_publisher2room(publisher_id, "video", fuA_pkt);
                }
                fuA_vec.clear();
            } else {
                rtp_packet* single_pkt = generate_singlenalu_packets(data, data_len);
                single_pkt->set_seq(vseq_++);
                single_pkt->set_ssrc(video_ssrc_);
                single_pkt->set_marker(1);
                single_pkt->set_timestamp((uint32_t)pkt_ptr->dts_);
                room_cb_->on_rtppacket_publisher2room(publisher_id, "video", single_pkt);
            }
        }
    }

    return ret;
}

int live_user_info::handle_audio_data(MEDIA_PACKET_PTR pkt_ptr) {

    if (pkt_ptr->codec_type_ == MEDIA_CODEC_OPUS) {
        active_last_ms_ = now_millisec();
        if (pkt_ptr->buffer_ptr_->data_len() > RTP_PAYLOAD_MAX_SIZE) {
            log_errorf("audio opus size is too large, %lu", pkt_ptr->buffer_ptr_->data_len());
            return -1;
        }
        rtp_packet* single_pkt = generate_singlenalu_packets((uint8_t*)pkt_ptr->buffer_ptr_->data() + 2,
                                                             pkt_ptr->buffer_ptr_->data_len() - 2);

        single_pkt->set_seq(aseq_++);
        single_pkt->set_ssrc(audio_ssrc_);
        single_pkt->set_timestamp((uint32_t)pkt_ptr->dts_);
        single_pkt->set_marker(1);

        std::string publisher_id = uid_;
        publisher_id += "_";
        publisher_id += "audio";
        room_cb_->on_rtppacket_publisher2room(publisher_id, "audio", single_pkt);
    }

    if (pkt_ptr->codec_type_ == MEDIA_CODEC_AAC) {
        MEDIA_PACKET_PTR raw_pkt_ptr = pkt_ptr->copy();
        if (!trans_) {
            trans_ = new transcode();
            trans_->set_output_audio_fmt("libopus");
            trans_->set_output_format(MEDIA_FORMAT_RAW);
            trans_->set_audio_bitrate(32);
            trans_->start();
        }
        raw_pkt_ptr->buffer_ptr_->consume_data(2);
        if (raw_pkt_ptr->is_seq_hdr_) {
            bool ok = get_audioinfo_by_asc((uint8_t*)raw_pkt_ptr->buffer_ptr_->data(),
                                        raw_pkt_ptr->buffer_ptr_->data_len(),
                                        aac_type_, sample_rate_, channel_);
            if (!ok) {
                log_errorf("get audio info error");
                return -1;
            }
            log_infof("get audio info from aac asc, aac type:%d, sample rate:%d, channel:%d",
                    aac_type_, sample_rate_, channel_);
            return 0;
        }
        if ((aac_type_ == 0) || (sample_rate_ == 0) || (channel_ == 0)) {
            log_infof("audio config is not ready");
            return 0;
        }
        uint8_t adts_data[32];
        int adts_len = make_adts(adts_data, aac_type_,
                sample_rate_, channel_, pkt_ptr->buffer_ptr_->data_len());
        assert(adts_len == 7);

        raw_pkt_ptr->buffer_ptr_->consume_data(0 - adts_len);
        uint8_t* p = (uint8_t*)raw_pkt_ptr->buffer_ptr_->data();
        memcpy(p, adts_data, adts_len);
 
        trans_->send_transcode(raw_pkt_ptr, 0);

        while(true) {
            MEDIA_PACKET_PTR ret_pkt_ptr = trans_->recv_transcode();
            if (!ret_pkt_ptr) {
                break;
            }
            if (ret_pkt_ptr->buffer_ptr_->data_len() > RTP_PAYLOAD_MAX_SIZE) {
                log_infof("opus packet size:%lu is too large",
                        ret_pkt_ptr->buffer_ptr_->data_len());
                continue;
            }
            ret_pkt_ptr->key_        = pkt_ptr->key_;
            ret_pkt_ptr->app_        = pkt_ptr->app_;
            ret_pkt_ptr->streamname_ = pkt_ptr->streamname_;

            rtp_packet* single_pkt = generate_singlenalu_packets((uint8_t*)ret_pkt_ptr->buffer_ptr_->data(),
                                                                 ret_pkt_ptr->buffer_ptr_->data_len());

            single_pkt->set_seq(aseq_++);
            single_pkt->set_ssrc(audio_ssrc_);
            single_pkt->set_timestamp((uint32_t)ret_pkt_ptr->dts_);
            single_pkt->set_marker(1);

            std::string publisher_id = uid_;
            publisher_id += "_";
            publisher_id += "audio";
            room_cb_->on_rtppacket_publisher2room(publisher_id, "audio", single_pkt);

        }
    }
    return 0;
}

