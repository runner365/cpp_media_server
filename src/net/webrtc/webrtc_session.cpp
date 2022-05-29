#include "webrtc_session.hpp"
#include "rtc_base_session.hpp"
#include "logger.hpp"
#include "stringex.hpp"
#include "net/udp/udp_server.hpp"
#include "net/stun/stun_packet.hpp"
#include "net/rtprtcp/rtprtcp_pub.hpp"
#include "net/rtprtcp/rtp_packet.hpp"
#include "net/rtprtcp/rtcp_sr.hpp"
#include "net/rtprtcp/rtcp_rr.hpp"
#include "net/rtprtcp/rtcp_fb_pub.hpp"
#include "net/rtprtcp/rtcpfb_nack.hpp"
#include "net/rtprtcp/rtcp_pspli.hpp"
#include "net/rtprtcp/rtcp_xr_dlrr.hpp"
#include "net/rtprtcp/rtcp_xr_rrt.hpp"
#include "net/rtprtcp/rtcpfb_remb.hpp"
#include "rtc_dtls.hpp"
#include "rtc_subscriber.hpp"
#include "srtp_session.hpp"
#include "utils/ipaddress.hpp"
#include "utils/byte_crypto.hpp"
#include "utils/timeex.hpp"
#include <unordered_map>
#include <map>
#include <vector>
#include <assert.h>
#include <stdio.h>
#include <openssl/asn1.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>

std::shared_ptr<udp_server> single_udp_server_ptr;
single_udp_session_callback single_udp_cb;

//key: "srcip+srcport" or "username", value: webrtc_session*
std::unordered_map<std::string, webrtc_session*> single_webrtc_map;
std::string single_candidate_ip;
uint16_t single_candidate_port = 0;

void init_single_udp_server(uv_loop_t* loop,
        const std::string& candidate_ip, uint16_t port) {
    if (!single_udp_server_ptr) {
        single_candidate_ip   = candidate_ip;
        single_candidate_port = port;
        log_infof("init udp server candidate_ip:%s, port:%d", candidate_ip.c_str(), port);
        single_udp_server_ptr = std::make_shared<udp_server>(loop, port, &single_udp_cb);
    }
}

void insert_webrtc_session(std::string key, webrtc_session* session) {
    if (key.empty() || (session == NULL)) {
        MS_THROW_ERROR("insert webrtc session error, key:%s", key.c_str());
    }
    log_infof("insert webrtc session by key:%s", key.c_str());
    single_webrtc_map.insert(std::make_pair(key, session));
}

webrtc_session* get_webrtc_session(const std::string& key) {
    webrtc_session* session = nullptr;
    
    auto iter = single_webrtc_map.find(key);
    if (iter == single_webrtc_map.end()) {
        return session;
    }
    session = (webrtc_session*)iter->second;
    return session;
}

int32_t remove_webrtc_session(std::string key) {
    auto iter = single_webrtc_map.find(key);
    if (iter == single_webrtc_map.end()) {
        return 0;
    }
    log_infof("remove webrtc session by key:%s", key.c_str());
    single_webrtc_map.erase(iter);
    return 1;
}

int remove_webrtc_session(webrtc_session* session) {
    int count = 0;
    auto iter = single_webrtc_map.begin();

    while(iter != single_webrtc_map.end()) {
        webrtc_session* item_session = iter->second;
        if (session == item_session) {
            iter = single_webrtc_map.erase(iter);
            count++;
        } else {
            iter++;
        }
    }
    return count;
}

//static unsigned int on_ssl_dtls_timer(SSL* /*ssl*/, unsigned int timer_us)
//{
//    if (timer_us == 0)
//        return 100000;
//    else if (timer_us >= 4000000)
//        return 4000000;
//    else
//        return 3 * timer_us / 2;
//}

void single_udp_session_callback::on_write(size_t sent_size, udp_tuple address) {
    //log_infof("udp write callback len:%lu, remote:%s",
    //    sent_size, address.to_string().c_str());
}

void single_udp_session_callback::on_read(const char* data, size_t data_size, udp_tuple address) {
    webrtc_session* session = nullptr;
    
    std::string peerid = address.to_string();
    
    session = get_webrtc_session(peerid);
    if (session) {
        session->on_recv_packet((const uint8_t*)data, data_size, address);
        return;
    }

    if(stun_packet::is_stun((const uint8_t*)data, data_size)) {
        stun_packet* packet = nullptr;
        log_infof("receive first stun packet...");
        try
        {
            packet = stun_packet::parse((const uint8_t*)data, data_size);

            std::string username = packet->user_name;
            size_t pos = username.find(":");
            if (pos != username.npos) {
                username = username.substr(0, pos);
            }
            session = get_webrtc_session(username);
            if (!session) {
                log_errorf("fail to find session by username(%s)", username.c_str());
            if (packet) {
                delete packet;
                packet = nullptr;
            }
                return;
            }
            log_infof("insert new session, username:%s, remote address:%s",
                    username.c_str(), peerid.c_str());
            insert_webrtc_session(peerid, session);

            session->on_handle_stun_packet(packet, address);
        }
        catch(const std::exception& e) {
            log_errorf("handle stun exception:%s", e.what());
        }

        if (packet) {
            delete packet;
            packet = nullptr;
        }
        
        return;
    }
    log_warnf("fail to find session to handle packet, data len:%lu, remote address:%s",
        data_size, address.to_string().c_str());
}
extern uv_loop_t* get_global_io_context();

webrtc_session::webrtc_session(const std::string& roomId, const std::string& uid,
                room_callback_interface* room, int session_direction, const rtc_media_info& media_info,
                std::string id):rtc_base_session(roomId, uid, room, session_direction, media_info, id)
            , timer_interface(get_global_io_context(), 500)
            , bitrate_estimate_(this) {
    username_fragment_ = byte_crypto::get_random_string(16);
    user_pwd_          = byte_crypto::get_random_string(32);

    insert_webrtc_session(username_fragment_, this);

    dtls_trans_ = new rtc_dtls(this, single_udp_server_ptr->get_loop());

    close_session_ = false;
    start_timer();

    last_xr_ntp_.ntp_sec  = 0;
    last_xr_ntp_.ntp_frac = 0;
    log_infof("webrtc_session construct username fragement:%s, user password:%s, roomid:%s, uid:%s, direction:%s",
        username_fragment_.c_str(), user_pwd_.c_str(), roomId_.c_str(), uid_.c_str(),
        (direction_ == RTC_DIRECTION_SEND) ? "send" : "receive");
}

webrtc_session::~webrtc_session() {
    close_session();
    stop_timer();
    if (write_srtp_) {
        delete write_srtp_;
        write_srtp_ = nullptr;
    }

    if (read_srtp_) {
        delete read_srtp_;
        read_srtp_ = nullptr;
    }

    if (dtls_trans_) {
        delete dtls_trans_;
        dtls_trans_ = nullptr;
    }
    log_infof("webrtc_session destruct username fragement:%s, user password:%s, roomid:%s, uid:%s, direction:%s",
        username_fragment_.c_str(), user_pwd_.c_str(), roomId_.c_str(), uid_.c_str(),
        (direction_ == RTC_DIRECTION_SEND) ? "send" : "receive");
}

void webrtc_session::close_session() {
    if (close_session_) {
        return;
    }
    close_session_ = true;
    if (direction_ == RTC_DIRECTION_RECV) {
        for (auto item : media_info_.medias) {
            log_infof("start remove publish, media type:%s", item.media_type.c_str());
            rtc_base_session::remove_publisher(item);
        }
    }

    int ret = remove_webrtc_session(this);
    if (ret > 0) {
        log_infof("close webrtc session remove %d item from the global map", ret);
    }
}

void webrtc_session::on_timer() {
    const int64_t XR_RRT_COUNT = 2;
    const int64_t XR_DLRR_COUNT = 2;
    int64_t now_ms = now_millisec();
    timer_count_++;

    if ((timer_count_ % XR_RRT_COUNT) == 0) {
        send_xr_rrt(now_ms);//for publisher
    }

    if ((timer_count_ % XR_DLRR_COUNT) == 0) {
        send_xr_dlrr(now_ms);//for subscriber
    }

    return;
}

void webrtc_session::send_xr_dlrr(int64_t now_ms) {
    xr_dlrr dlrr_obj;

    if (last_xr_ms_ <= 0) {
        return;
    }

    if ((now_ms - last_xr_ms_) > 5000) {
        return;
    }

    dlrr_obj.set_ssrc(0x01);
    for (auto& item : ssrc2subscribers_) {
        std::shared_ptr<rtc_subscriber> subscriber_ptr = item.second;
        uint32_t rtp_ssrc = subscriber_ptr->get_rtp_ssrc();

        uint32_t lrr = (last_xr_ntp_.ntp_sec & 0xffff) << 16;
        lrr |= (last_xr_ntp_.ntp_frac & 0xffff0000) >> 16;

        int64_t diff_ms = now_ms - last_xr_ms_;
        uint32_t dlrr = (uint32_t)(diff_ms / 1000) << 16;
        dlrr |= (uint32_t)((diff_ms % 1000) * 65536 / 1000);
        dlrr_obj.addr_dlrr_block(rtp_ssrc, lrr, dlrr);
    }

    send_rtcp_data_in_dtls(dlrr_obj.get_data(), dlrr_obj.get_data_len());
}

void webrtc_session::send_xr_rrt(int64_t now_ms) {
    for (auto& item : ssrc2publishers_) {
        std::shared_ptr<rtc_publisher> publisher_ptr = item.second;
        xr_rrt rrt;

        publisher_ptr->get_xr_rrt(rrt, now_ms);
        send_rtcp_data_in_dtls(rrt.get_data(), rrt.get_data_len());
    }
}

void webrtc_session::send_rtp_data_in_dtls(uint8_t* data, size_t data_len) {
    if(!write_srtp_) {
        log_errorf("write_srtp is not ready, roomid:%s, uid:%s",
                roomId_.c_str(), uid_.c_str());
        return;
    }
    
    bool ret = write_srtp_->encrypt_rtp(const_cast<uint8_t**>(&data), &data_len);
    if (!ret) {
        log_errorf("encrypt_rtp error, roomid:%s, uid:%s",
                roomId_.c_str(), uid_.c_str());
        return;
    }
    //log_infof("sent srtp data len:%u, remote:%s", data_len, remote_address_.to_string().c_str());
    write_udp_data(data, data_len, remote_address_);
}

void webrtc_session::send_rtcp_data_in_dtls(uint8_t* data, size_t data_len) {
    if(!write_srtp_) {
        return;
    }
    bool ret = write_srtp_->encrypt_rtcp(const_cast<uint8_t**>(&data), &data_len);
    if (!ret) {
        log_errorf("encrypt rtcp error");
        return;
    }
    write_udp_data(data, data_len, remote_address_);
}

void webrtc_session::write_udp_data(uint8_t* data, size_t data_size, const udp_tuple& address) {
    if (!single_udp_server_ptr) {
        MS_THROW_ERROR("single udp server is not inited");
    }
    single_udp_server_ptr->write((char*)data, data_size, address);
}

void webrtc_session::on_recv_packet(const uint8_t* udp_data, size_t udp_data_len,
                            const udp_tuple& address) {
    memcpy(pkt_data_, udp_data, udp_data_len);

    if (stun_packet::is_stun(pkt_data_, udp_data_len)) {
        try {
            stun_packet* packet = stun_packet::parse((uint8_t*)pkt_data_, udp_data_len);
            on_handle_stun_packet(packet, address);
            delete packet;
        }
        catch(const std::exception& e) {
            log_errorf("handle stun packet exception:%s", e.what());
        }
    } else if (is_rtcp(pkt_data_, udp_data_len)) {
        on_handle_rtcp_data(pkt_data_, udp_data_len, address);
    } else if (is_rtp(pkt_data_, udp_data_len)) {
        on_handle_rtp_data(pkt_data_, udp_data_len, address);
    } else if (rtc_dtls::is_dtls(pkt_data_, udp_data_len)) {
        log_infof("receive dtls packet len:%lu, remote:%s",
                udp_data_len, address.to_string().c_str());
        on_handle_dtls_data(pkt_data_, udp_data_len, address);
    } else {
        log_errorf("receive unkown packet len:%lu, remote:%s",
                udp_data_len, address.to_string().c_str());
    }
    return;
}

void webrtc_session::write_error_stun_packet(stun_packet* pkt, int err, const udp_tuple& address) {
   stun_packet* resp_pkt = pkt->create_error_response(err);

   resp_pkt->serialize();

   log_infof("write error stun, data len:%lu, error:%d, remote address:%s",
       resp_pkt->data_len, err, address.to_string().c_str());
   write_udp_data(resp_pkt->data, resp_pkt->data_len, address);
   delete resp_pkt;
   return;
}

void webrtc_session::on_dtls_connected(CRYPTO_SUITE_ENUM suite,
                uint8_t* local_key, size_t local_key_len,
                uint8_t* remote_key, size_t remote_key_len,
                std::string& remote_cert) {
    log_info_data(local_key, local_key_len, "on dtls connected srtp local key");

    log_info_data(remote_key, remote_key_len, "on dtls connected srtp remote key");

    log_infof("on dtls connected remote cert:\r\n%s", remote_cert.c_str());

    try {
        if (write_srtp_) {
            delete write_srtp_;
            write_srtp_ = nullptr;
        }
        if (read_srtp_) {
            delete read_srtp_;
            read_srtp_ = nullptr;
        }
        write_srtp_ = new srtp_session(SRTP_SESSION_OUT_TYPE, suite, local_key, local_key_len);
        read_srtp_  = new srtp_session(SRTP_SESSION_IN_TYPE, suite, remote_key, remote_key_len);
    }
    catch(const std::exception& e) {
        log_errorf("create srtp session error:%s", e.what());
    }
    
    return;
}

void webrtc_session::on_handle_rtp_data(const uint8_t* data, size_t data_len, const udp_tuple& address) {
    if (dtls_trans_->state != DTLS_CONNECTED) {
        log_errorf("dtls is not connected and discard rtp packet, stat:%d", (int)dtls_trans_->state);
        return;
    }

    if (!read_srtp_) {
        log_errorf("read srtp session is not ready and discard rtp packet");
        return;
    }

    bool ret = read_srtp_->decrypt_srtp(const_cast<uint8_t*>(data), &data_len);
    if (!ret) {
        //rtp_common_header* header = (rtp_common_header*)data;
        //log_errorf("rtp decrypt error, data len:%lu, version:%d, padding:%d, ext:%d, csrc count:%d, marker:%d, payload type:%d, seq:%d, timestamp:%u, ssrc:%u",
        //    data_len, header->version, header->padding, header->extension, header->csrc_count, header->marker, header->payload_type,
        //    ntohs(header->sequence), ntohl(header->timestamp), ntohl(header->ssrc));
        return;
    }

    //handle rtp packet
    rtp_packet* pkt = nullptr;
    try {
        pkt = rtp_packet::parse(const_cast<uint8_t*>(data), data_len);
    }
    catch(const std::exception& e) {
        log_errorf("rtp packet parse error:%s", e.what());
        return;
    }

    uint32_t ssrc = pkt->get_ssrc();
    auto publisher_ptr = rtc_base_session::get_publisher(ssrc);
    if (!publisher_ptr) {
        log_errorf("fail to get publisher object by ssrc:%u", pkt->get_ssrc());
        return;
    }

    uint32_t abs_time = 0;
    bool ret_abs_time = false;
    int abstime_id = publisher_ptr->get_abstime_id();
    pkt->set_abs_time_extension_id((uint8_t)abstime_id);
    ret_abs_time = pkt->read_abs_time(abs_time);
    publisher_ptr->on_handle_rtppacket(pkt);
    
    if (ret_abs_time) {
        int64_t arrivalTimeMs = now_millisec();
        bitrate_estimate_.IncomingPacket(arrivalTimeMs, pkt->get_payload_length(), ssrc, abs_time);
        log_debugf("rtp media:%s abs_time:%u, ssrc:%u",
            publisher_ptr->get_media_type().c_str(), abs_time, ssrc);
    }

    return;
}

void webrtc_session::OnRembServerAvailableBitrate(
       const webrtc::RemoteBitrateEstimator* remoteBitrateEstimator,
       const std::vector<uint32_t>& ssrcs,
       uint32_t availableBitrate) {
    rtcpfb_remb remb_pkt(0, 0);
    remb_pkt.set_ssrcs(ssrcs);
    remb_pkt.set_bitrate((int64_t)availableBitrate);

    size_t data_len = 0;
    uint8_t* remb_data = remb_pkt.serial(data_len);

    if (remb_data && (data_len > 0)) {
        //std::stringstream ss;

        //ss << "ssrcs:[";
        //for (auto& ssrc : ssrcs) {
        //    ss << " " << ssrc;
        //}
        //ss << " ]";
        //ss << ", bitrate:" << availableBitrate;
        //ss << ", datalen:" << data_len;
        //log_infof("send remb %s", ss.str().c_str());
        //log_info_data(remb_data, data_len, "remb data");
        send_rtcp_data_in_dtls(remb_data, data_len);
    }

    return;
}

void webrtc_session::on_handle_rtcp_data(const uint8_t* data, size_t data_len, const udp_tuple& address) {
    if (dtls_trans_->state != DTLS_CONNECTED) {
        log_errorf("dtls is not connected and discard rtcp packet");
        return;
    }

    if (!read_srtp_) {
        log_errorf("read srtp session is not ready and discard rtcp packet");
        return;
    }

    bool ret = read_srtp_->decrypt_srtcp(const_cast<uint8_t*>(data), &data_len);
    if (!ret) {
        return;
    }

    //handle rtcp packet
    int left_len = (int)data_len;
    uint8_t* p = const_cast<uint8_t*>(data);

    log_debugf("handle rtcp direction:%s, total len:%lu",
        (direction_ == RTC_DIRECTION_SEND) ? "send" : "recv", data_len);
    //log_info_data(data, data_len, "rtcp packet");
    while (left_len > 0) {
        rtcp_common_header* header = (rtcp_common_header*)p;
        uint16_t payload_length = get_rtcp_length(header);
        size_t item_total = sizeof(rtcp_common_header) + payload_length;

        switch (header->packet_type)
        {
            case RTCP_SR:
            {
                handle_rtcp_sr(p, item_total);
                break;
            }
            case RTCP_RR:
            {
                handle_rtcp_rr(p, item_total);
                break;
            }
            case RTCP_SDES:
            {
                //rtcp sdes packet needn't to be handled.
                break;
            }
            case RTCP_BYE:
            {
                break;
            }
            case RTCP_APP:
            {
                break;
            }
            case RTCP_RTPFB:
            {
                handle_rtcp_rtpfb(p, item_total);
                break;
            }
            case RTCP_PSFB:
            {
                handle_rtcp_psfb(p, item_total);
                break;
            }
            case RTCP_XR:
            {
                handle_rtcp_xr(p, item_total);
                break;
            }
            default:
            {
                log_errorf("unkown rtcp type:%d", header->packet_type);
            }
        }
        p        += item_total;
        left_len -= item_total;
    }

    return;
}

void webrtc_session::handle_rtcp_xr(uint8_t* data, size_t data_len) {
    rtcp_common_header* header = (rtcp_common_header*)data;
    uint32_t* ssrc_p          = (uint32_t*)(header + 1);
    rtcp_xr_header* xr_hdr    = (rtcp_xr_header*)(ssrc_p + 1);
    int64_t xr_len            = data_len - sizeof(rtcp_common_header) - 4;

    while(xr_len > 0) {
        switch(xr_hdr->bt)
        {
            case XR_DLRR:
            {
                xr_dlrr_data* dlrr_block = (xr_dlrr_data*)xr_hdr;
                handle_xr_dlrr(dlrr_block);
                break;
            }
            case XR_RRT:
            {
                xr_rrt_data* rrt_block = (xr_rrt_data*)xr_hdr;
                last_xr_ntp_.ntp_sec  = ntohl(rrt_block->ntp_sec);
                last_xr_ntp_.ntp_frac = ntohl(rrt_block->ntp_frac);
                last_xr_ms_ = now_millisec();
                break;
            }
            default:
            {
                log_errorf("handle unkown xr type:%d", xr_hdr->bt);
            }
        }
        int64_t offset = 4 + ntohs(xr_hdr->block_length)*4;
        xr_len -= offset;
        data   += offset;
        xr_hdr = (rtcp_xr_header*)data;
    }
    return;
}

void webrtc_session::handle_xr_dlrr(xr_dlrr_data* dlrr_block) {
    uint32_t media_ssrc = ntohl(dlrr_block->ssrc);
    std::shared_ptr<rtc_publisher> publisher_ptr = get_publisher(media_ssrc);
    
    if (!publisher_ptr) {
        log_errorf("fail to get publisher by ssrc:%u", media_ssrc);
        return;
    }

    publisher_ptr->handle_xr_dlrr(dlrr_block);
    return;
}

void webrtc_session::handle_rtcp_psfb(uint8_t* data, size_t data_len) {
    if (data_len <=sizeof(rtcp_fb_common_header)) {
        return;
    }

    int64_t now_ms = now_millisec();
    try {
        rtcp_fb_common_header* header = (rtcp_fb_common_header*)data;
        switch (header->fmt)
        {
            case FB_PS_PLI:
            {
                rtcp_pspli* pspli_pkt = rtcp_pspli::parse(data, data_len);
                if (!pspli_pkt) {
                    log_errorf("parse rtcp ps pli error");
                    return;
                }
                log_infof("receive keyframe request: %s", pspli_pkt->dump().c_str());
                if (direction_ != RTC_DIRECTION_SEND) {
                    log_errorf("webrtc session recv direction get rtcp ps pli....");
                    return;
                }
                std::shared_ptr<rtc_subscriber> subscriber_ptr = get_subscriber(pspli_pkt->get_media_ssrc());
                if (!subscriber_ptr) {
                    log_errorf("get subscriber error, media ssrc:%u, sender ssrc:%u",
                            pspli_pkt->get_media_ssrc(), pspli_pkt->get_sender_ssrc());
                    return;
                }
                subscriber_ptr->update_alive(now_ms);
                subscriber_ptr->request_keyframe();
                
                break;
            }
            case FB_PS_AFB:
            {
                rtcpfb_remb* remb_pkt = rtcpfb_remb::parse(data, data_len);
                if (!remb_pkt) {
                    break;
                }
                //log_infof("subscriber bitrate:%ld", remb_pkt->get_bitrate());
                remb_bitrate_ = remb_pkt->get_bitrate();
                std::vector<uint32_t> ssrcs = remb_pkt->get_ssrcs();

                for (auto ssrc : ssrcs) {
                    auto iter = ssrc2subscribers_.find(ssrc);
                    if (iter != ssrc2subscribers_.end()) {
                        iter->second->set_remb_bitrate(remb_bitrate_);
                    }
                }
                delete remb_pkt;
                break;
            }
            default:
            {
                log_debugf("rtcp psfb doesn't handle fmt:%d", header->fmt);
                break;
            }
        }
    }
    catch(const std::exception& e) {
        log_errorf("handle rtcp ps feedback error:%s", e.what());
    }
    
}

void webrtc_session::handle_rtcp_rtpfb(uint8_t* data, size_t data_len) {
    if (data_len <=sizeof(rtcp_fb_common_header)) {
        return;
    }
    int64_t now_ms = now_millisec();
    try {
        rtcp_fb_common_header* header = (rtcp_fb_common_header*)data;
        switch (header->fmt)
        {
            case FB_RTP_NACK:
            {
                rtcp_fb_nack* nack_pkt = rtcp_fb_nack::parse(data, data_len);
                uint32_t media_ssrc = nack_pkt->get_media_ssrc();
                std::shared_ptr<rtc_subscriber> subscriber_ptr = get_subscriber(media_ssrc);
                if (!subscriber_ptr) {
                    return;
                }
                subscriber_ptr->update_alive(now_ms);
                subscriber_ptr->handle_fb_rtp_nack(nack_pkt);
                break;
            }
            default:
            {
                log_warnf("receive rtcp psfb format(%d) is not handled.", header->fmt);
                break;
            }
        }
    }
    catch(const std::exception& e) {
        log_errorf("rtcp feedback parse error:%s", e.what());
    }
    return;
}

void webrtc_session::handle_rtcp_sr(uint8_t* data, size_t data_len) {
    if (data_len <=sizeof(rtcp_common_header)) {
        return;
    }
    try {
        rtcp_sr_packet* sr_pkt = rtcp_sr_packet::parse(data, data_len);
        if (sr_pkt) {
            std::shared_ptr<rtc_publisher> publisher_ptr = get_publisher(sr_pkt->get_ssrc());
            if (!publisher_ptr) {
                log_errorf("fail to get publisher by ssrc:%u for rtcp sr", sr_pkt->get_ssrc());
            } else {
                publisher_ptr->on_handle_rtcp_sr(sr_pkt);
            }
            delete sr_pkt;
        }
    }
    catch(const std::exception& e) {
        log_errorf("rtcp sr parse error:%s", e.what());
    }
}

void webrtc_session::handle_rtcp_rr(uint8_t* data, size_t data_len) {
    if (data_len <=sizeof(rtcp_common_header)) {
        return;
    }
    
    int64_t now_ms = now_millisec();
    try {
        rtcp_rr_packet* rr_pkt = rtcp_rr_packet::parse(data, data_len);
        std::shared_ptr<rtc_subscriber> subscriber_ptr = get_subscriber(rr_pkt->get_reportee_ssrc());
        if (!subscriber_ptr) {
            log_errorf("fail to get subscribe by ssrc:%u for rtcp rr", rr_pkt->get_reportee_ssrc());
        } else {
            subscriber_ptr->update_alive(now_ms);
            subscriber_ptr->handle_rtcp_rr(rr_pkt);
            //handl rtcp rr;
        }
        delete rr_pkt;
    }
    catch(const std::exception& e) {
        //log_errorf("rtcp rr parse error:%s", e.what());
    }
}

void webrtc_session::on_handle_dtls_data(const uint8_t* data, size_t data_len, const udp_tuple& address) {
    if ((remote_address_.ip_address != address.ip_address) && (remote_address_.port != address.port)) {
        log_warnf("remote address(%s) is not equal to the address(%s)",
            remote_address_.to_string().c_str(), address.to_string().c_str());
    }

    //update remote address
    remote_address_ = address;

    if ((dtls_trans_->state != DTLS_CONNECTING) && (dtls_trans_->state != DTLS_CONNECTED)) {
        log_errorf("dtls state(%d) is not ready.", dtls_trans_->state);
        return;
    }
    dtls_trans_->handle_dtls_data(data, data_len);
    return;
}

void webrtc_session::on_handle_stun_packet(stun_packet* pkt, const udp_tuple& address) {
    if (pkt->stun_method != STUN_METHOD_ENUM::BINDING) {
        log_errorf("stun packet method is not binding");
        write_error_stun_packet(pkt, 400, address);
        return;
    }

    if (!pkt->has_fingerprint && pkt->stun_class != STUN_CLASS_ENUM::STUN_INDICATION) {
        log_errorf("stun packet class(%d) has no fingerprint.", (int)pkt->stun_class);
        write_error_stun_packet(pkt, 400, address);
        return;
    }

    if (pkt->stun_class == STUN_CLASS_ENUM::STUN_REQUEST) {
        if (!pkt->message_integrity || !pkt->priority || pkt->user_name.empty()) {
            log_errorf("receive stun packet missing attribute, message_integrity:%d, priority:%d, username:%s",
                    pkt->message_integrity, pkt->priority, pkt->user_name.c_str());
            write_error_stun_packet(pkt, 400, address);
        }

        STUN_AUTHENTICATION ret_auth = pkt->check_auth(this->username_fragment_, this->user_pwd_);
        if (ret_auth == STUN_AUTHENTICATION::OK) {
            //log_infof("stun packet is authentication ok, remote:%s", address.to_string().c_str());
        } else if (ret_auth == STUN_AUTHENTICATION::UNAUTHORIZED) {
            log_errorf("stun packet is unauthorized, remote:%s", address.to_string().c_str());
            write_error_stun_packet(pkt, 401, address);
            return;
        } else if (ret_auth == STUN_AUTHENTICATION::BAD_REQUEST) {
            log_errorf("stun packet authoriz return bad request, remote:%s", address.to_string().c_str());
            write_error_stun_packet(pkt, 400, address);
            return;
        } else {
            assert(0);
        }

        if (pkt->ice_controlled) {
            log_errorf("peer ice controlled...");
            write_error_stun_packet(pkt, 487, address);
            return;
        }

        struct sockaddr src_address;
        get_ipv4_sockaddr(address.ip_address, address.port, &src_address);

        stun_packet* resp_pkt = pkt->create_success_response();
        resp_pkt->xor_address = &src_address;
        resp_pkt->password    = this->user_pwd_;
        resp_pkt->serialize();
        
        remote_address_ = address;
        write_udp_data(resp_pkt->data, resp_pkt->data_len, address);
        delete resp_pkt;

        dtls_trans_->start(ROLE_SERVER);
    } else {
        log_warnf("the server doesn't handle stun class:%d", (int)pkt->stun_class);
    }
}

std::string webrtc_session::get_candidates_ip() {
    return single_candidate_ip;
}

uint16_t webrtc_session::get_candidates_port() {
    return single_candidate_port;
}

finger_print_info webrtc_session::get_local_finger_print(const std::string& algorithm_str) {
    finger_print_info info;

    finger_print_algorithm_enum algorithm = string2finger_print_algorithm[algorithm_str];
    
    std::string value;
    for (auto item : rtc_dtls::s_local_fingerprint_vec) {
        if (item.algorithm == algorithm) {
            value = item.value;
            break;
        }
    }

    if (value.empty()) {
        MS_THROW_ERROR("fail to find algorithm:%s", algorithm_str.c_str());
    }
    info.algorithm = algorithm;
    info.value = value;
    
    return info;
}

void webrtc_session::set_remote_finger_print(const FINGER_PRINT& fingerprint) {
    dtls_trans_->remote_finger_print.value = fingerprint.hash;
    dtls_trans_->remote_finger_print.algorithm = string2finger_print_algorithm[fingerprint.type];

    log_infof("set remote fingerprint algorithm:%d, value:%s",
        (int)dtls_trans_->remote_finger_print.algorithm,
        dtls_trans_->remote_finger_print.value.c_str());
}
