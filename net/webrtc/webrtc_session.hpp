#ifndef WEBRTC_SESSION_HPP
#define WEBRTC_SESSION_HPP
#include "rtc_base_session.hpp"
#include "rtc_session_pub.hpp"
#include "net/udp/udp_server.hpp"
#include <openssl/asn1.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/x509.h>

class webrtc_session;
class stun_packet;

void insert_webrtc_session(std::string key, webrtc_session* session);
webrtc_session* get_webrtc_session(const std::string& key);

class single_udp_session_callback : public udp_session_callbackI
{
protected:
    virtual void on_write(size_t sent_size, udp_tuple address) override;
    virtual void on_read(const char* data, size_t data_size, udp_tuple address) override;
};

class webrtc_session : public rtc_base_session
{
public:
    webrtc_session(int session_direction);
    virtual ~webrtc_session();

public:
    std::string get_username_fragment() {return username_fragment_;}
    std::string get_user_pwd() {return user_pwd_;}
    std::string get_candidates_ip();
    uint16_t get_candidates_port();
    finger_print_info get_local_finger_print(const std::string& algorithm_str);
    void set_remote_finger_print(const finger_print_info& fingerprint) {remote_finger_print_ = fingerprint;}

public:
    void on_recv_packet(const uint8_t* data, size_t data_size, const udp_tuple& address);
    void on_handle_stun_packet(stun_packet* pkt, const udp_tuple& address);

public:
    static bool is_dtls(const uint8_t* data, size_t len);

private:
    void write_udp_data(uint8_t* data, size_t data_size, const udp_tuple& address);
    void write_error_stun_packet(stun_packet* pkt, int err, const udp_tuple& address);

private://for ice
    std::string username_fragment_;
    std::string user_pwd_;

private://for dtls
    SSL* ssl_ = nullptr;
    BIO* ssl_bio_read_  = nullptr; // The BIO from which ssl reads.
    BIO* ssl_bio_write_ = nullptr; // The BIO in which ssl writes.
    finger_print_info remote_finger_print_;
};

#endif