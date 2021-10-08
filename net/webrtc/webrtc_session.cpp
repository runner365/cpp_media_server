#include "webrtc_session.hpp"
#include "rtc_base_session.hpp"
#include "logger.hpp"
#include "stringex.hpp"
#include "net/udp/udp_server.hpp"
#include "net/stun/stun_packet.hpp"
#include "net/rtprtcp/rtprtcp_pub.hpp"
#include "rtc_dtls.hpp"
#include "srtp_session.hpp"
#include "utils/ipaddress.hpp"
#include "utils/byte_crypto.hpp"
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

static std::shared_ptr<udp_server> single_udp_server_ptr;
static single_udp_session_callback single_udp_cb;
//key: "srcip+srcport" or "username", value: webrtc_session*
static std::unordered_map<std::string, webrtc_session*> single_webrtc_map;
static std::string single_candidate_ip;
static uint16_t single_candidate_port = 0;

void init_single_udp_server(boost::asio::io_context& io_context,
        const std::string& candidate_ip, uint16_t port) {
    if (!single_udp_server_ptr) {
        single_candidate_ip   = candidate_ip;
        single_candidate_port = port;
        log_infof("init udp server candidate_ip:%s, port:%d", candidate_ip.c_str(), port);
        single_udp_server_ptr = std::make_shared<udp_server>(io_context, port, &single_udp_cb);
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

webrtc_session::webrtc_session(int session_direction):rtc_base_session(session_direction) {
    username_fragment_ = byte_crypto::get_random_string(16);
    user_pwd_          = byte_crypto::get_random_string(32);

    insert_webrtc_session(username_fragment_, this);

    dtls_trans_ = new rtc_dtls(this, single_udp_server_ptr->get_io_context());

    log_infof("webrtc_session construct username fragement:%s, user password:%s",
        username_fragment_.c_str(), user_pwd_.c_str());
}

webrtc_session::~webrtc_session() {
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
}

void webrtc_session::close_session() {
    int ret = remove_webrtc_session(this);
    if (ret > 0) {
        log_infof("close webrtc session remove %d item from the global map", ret);
    }
}

void webrtc_session::write_udp_data(uint8_t* data, size_t data_size, const udp_tuple& address) {
    if (!single_udp_server_ptr) {
        MS_THROW_ERROR("single udp server is not inited");
    }
    single_udp_server_ptr->write((char*)data, data_size, address);
}

void webrtc_session::send_data(uint8_t* data, size_t data_len) {
    if (!single_udp_server_ptr) {
        MS_THROW_ERROR("single udp server is not inited");
    }

    if (remote_address_.ip_address.empty() || remote_address_.port == 0) {
        MS_THROW_ERROR("remote address is not inited");
    }

    single_udp_server_ptr->write((char*)data, data_len, remote_address_);
}

void webrtc_session::on_recv_packet(const uint8_t* udp_data, size_t udp_data_len,
                            const udp_tuple& address) {
    memcpy(pkt_data_, udp_data, udp_data_len);
    if (stun_packet::is_stun(pkt_data_, udp_data_len)) {
        try {
            stun_packet* packet = stun_packet::parse((uint8_t*)pkt_data_, udp_data_len);
            on_handle_stun_packet(packet, address);
        }
        catch(const std::exception& e) {
            log_errorf("handle stun packet exception:%s", e.what());
        }
    } else if (is_rtcp(pkt_data_, udp_data_len)) {
        log_infof("receive rtcp packet len:%lu, remote:%s",
                udp_data_len, address.to_string().c_str());
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
        return;
    }

    //handle rtp packet

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

    return;
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

        //log_infof("stun packet response:\r\n %s", resp_pkt->dump().c_str());
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
