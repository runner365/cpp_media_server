#ifndef WEBRTC_SESSION_HPP
#define WEBRTC_SESSION_HPP
#include "rtc_base_session.hpp"
#include "rtc_session_pub.hpp"
#include "rtc_dtls.hpp"
#include "rtc_media_info.hpp"
#include "rtc_publisher.hpp"
#include "net/udp/udp_server.hpp"
#include "utils/timeex.hpp"
#include "modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"

#include <openssl/asn1.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <map>

class webrtc_session;
class stun_packet;
class rtc_dtls;
class srtp_session;
class room_callback_interface;

void insert_webrtc_session(std::string key, webrtc_session* session);
webrtc_session* get_webrtc_session(const std::string& key);

class single_udp_session_callback : public udp_session_callbackI
{
protected:
    virtual void on_write(size_t sent_size, udp_tuple address) override;
    virtual void on_read(const char* data, size_t data_size, udp_tuple address) override;
};

class webrtc_session : public rtc_base_session, public timer_interface, public webrtc::RemoteBitrateObserver
{
public:
    webrtc_session(const std::string& roomId, const std::string& uid,
        room_callback_interface* room, int session_direction, const rtc_media_info& media_info);
    virtual ~webrtc_session();

public:
    std::string get_username_fragment() {return username_fragment_;}
    std::string get_user_pwd() {return user_pwd_;}
    std::string get_candidates_ip();
    uint16_t get_candidates_port();
    finger_print_info get_local_finger_print(const std::string& algorithm_str);
    void set_remote_finger_print(const FINGER_PRINT& fingerprint);

    void close_session();

public:
    void on_recv_packet(const uint8_t* data, size_t data_size, const udp_tuple& address);
    void on_handle_stun_packet(stun_packet* pkt, const udp_tuple& address);
    void on_handle_dtls_data(const uint8_t* data, size_t data_len, const udp_tuple& address);
    void on_handle_rtp_data(const uint8_t* data, size_t data_len, const udp_tuple& address);
    void on_handle_rtcp_data(const uint8_t* data, size_t data_len, const udp_tuple& address);

public:
    void on_dtls_connected(CRYPTO_SUITE_ENUM srtpCryptoSuite,
                        uint8_t* srtpLocalKey, size_t srtpLocalKeyLen,
                        uint8_t* srtpRemoteKey, size_t srtpRemoteKeyLen,
                        std::string& remoteCert);

public://implement timer_interface
    virtual void on_timer() override;

public://implement webrtc::RemoteBitrateObserver
   virtual void OnRembServerAvailableBitrate(
       const webrtc::RemoteBitrateEstimator* remoteBitrateEstimator,
       const std::vector<uint32_t>& ssrcs,
       uint32_t availableBitrate) override;

private:
    virtual void send_rtp_data_in_dtls(uint8_t* data, size_t data_len) override;
    virtual void send_rtcp_data_in_dtls(uint8_t* data, size_t data_len) override;

private:
    void write_udp_data(uint8_t* data, size_t data_size, const udp_tuple& address);
    void write_error_stun_packet(stun_packet* pkt, int err, const udp_tuple& address);

private:
    void handle_rtcp_sr(uint8_t* data, size_t data_len);
    void handle_rtcp_rr(uint8_t* data, size_t data_len);
    void handle_rtcp_rtpfb(uint8_t* data, size_t data_len);
    void handle_rtcp_psfb(uint8_t* data, size_t data_len);
    void handle_rtcp_xr(uint8_t* data, size_t data_len);
    void handle_xr_dlrr(xr_dlrr_data* dlrr_block);
    
    void send_xr_rrt(int64_t now_ms);
    void send_xr_dlrr(int64_t now_ms);
    
private://for ice
    std::string username_fragment_;
    std::string user_pwd_;

private://for dtls
    rtc_dtls* dtls_trans_     = nullptr;
    srtp_session* write_srtp_ = nullptr;
    srtp_session* read_srtp_  = nullptr;
    uint8_t pkt_data_[2048];

private:
    bool close_session_ = false;

private:
    int64_t timer_count_ = 0;
    NTP_TIMESTAMP last_xr_ntp_;
    int64_t last_xr_ms_  = 0;//last xr rrt systime

private:
    webrtc::RemoteBitrateEstimatorAbsSendTime bitrate_estimate_;
};

#endif
