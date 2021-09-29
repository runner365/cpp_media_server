#include "webrtc_session.hpp"
#include "rtc_base_session.hpp"
#include "logger.hpp"
#include "stringex.hpp"
#include "net/udp/udp_server.hpp"
#include "net/stun/stun_packet.hpp"
#include "net/rtprtcp/rtprtcp_pub.hpp"
#include "utils/ipaddress.hpp"
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

static std::vector<finger_print_info> s_local_fingerprint_vec;

X509* s_certificate    = nullptr;
EVP_PKEY* s_privatekey = nullptr;
SSL_CTX* s_ssl_ctx     = nullptr;

std::vector<srtp_crypto_suite_map> srtp_crypto_suite_vec =
{
    { AEAD_AES_256_GCM, "SRTP_AEAD_AES_256_GCM" },
    { AEAD_AES_128_GCM, "SRTP_AEAD_AES_128_GCM" },
    { AES_CM_128_HMAC_SHA1_80, "SRTP_AES128_CM_SHA1_80" },
    { AES_CM_128_HMAC_SHA1_32, "SRTP_AES128_CM_SHA1_32" }
};

std::unordered_map<std::string, finger_print_algorithm_enum> string2finger_print_algorithm =
{
    { "sha-1",   FINGER_SHA1   },
    { "sha-224", FINGER_SHA224 },
    { "sha-256", FINGER_SHA256 },
    { "sha-384", FINGER_SHA384 },
    { "sha-512", FINGER_SHA512 }
};

std::map<finger_print_algorithm_enum, std::string> finger_print_algorithm2String =
{
    { FINGER_SHA1,   std::string("sha-1")   },
    { FINGER_SHA224, std::string("sha-224") },
    { FINGER_SHA256, std::string("sha-256") },
    { FINGER_SHA384, std::string("sha-384") },
    { FINGER_SHA512, std::string("sha-512") }
};


static int on_ssl_certificate_verify(int, X509_STORE_CTX*);
static void on_ssl_info(const SSL* ssl, int type, int value);

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

webrtc_session* get_webrtc_session(const std::string& key)
{
    webrtc_session* session = nullptr;
    
    auto iter = single_webrtc_map.find(key);
    if (iter == single_webrtc_map.end())
    {
        return session;
    }
    session = (webrtc_session*)iter->second;
    return session;
}

void remove_webrtc_session(std::string key)
{
    auto iter = single_webrtc_map.find(key);
    if (iter == single_webrtc_map.end())
    {
        return;
    }
    log_infof("remove webrtc session by key:%s", key.c_str());
    single_webrtc_map.erase(iter);
}

void dtls_init(const std::string& key_file, const std::string& cert_file) {
    FILE* fh = fopen(cert_file.c_str(), "r");
    if (!fh) {
        MS_THROW_ERROR("open cert file(%s) error", cert_file.c_str());
    }

    s_certificate = PEM_read_X509(fh, nullptr, nullptr, nullptr);
    if (!s_certificate) {
        MS_THROW_ERROR("read certificate file error");
    }

    fclose(fh);

    fh = fopen(key_file.c_str(), "r");
    if (!fh) {
        MS_THROW_ERROR("open key file(%s) error", key_file.c_str());
    }

    s_privatekey = PEM_read_PrivateKey(fh, nullptr, nullptr, nullptr);
    if (!s_privatekey) {
        MS_THROW_ERROR("read key file error");
    }

    fclose(fh);

    s_ssl_ctx = SSL_CTX_new(DTLS_method());
    if (!s_ssl_ctx) {
        MS_THROW_ERROR("create ssl context error");
    }

    int ret = SSL_CTX_use_certificate(s_ssl_ctx, s_certificate);
    if (!ret) {
        MS_THROW_ERROR("use certificate error");
    }

    ret = SSL_CTX_use_PrivateKey(s_ssl_ctx, s_privatekey);
    if (!ret) {
        MS_THROW_ERROR("use privatekey error");
    }

    ret = SSL_CTX_check_private_key(s_ssl_ctx);
    if (!ret) {
        MS_THROW_ERROR("check privatekey error");
    }

    // Set options.
    SSL_CTX_set_options(s_ssl_ctx,
                    SSL_OP_CIPHER_SERVER_PREFERENCE | SSL_OP_NO_TICKET |
                    SSL_OP_SINGLE_ECDH_USE | SSL_OP_NO_QUERY_MTU);
    
    SSL_CTX_set_session_cache_mode(s_ssl_ctx, SSL_SESS_CACHE_OFF);

    SSL_CTX_set_read_ahead(s_ssl_ctx, 1);

    SSL_CTX_set_verify_depth(s_ssl_ctx, 4);

    SSL_CTX_set_verify(s_ssl_ctx, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                    on_ssl_certificate_verify);

    SSL_CTX_set_info_callback(s_ssl_ctx, on_ssl_info);

    ret = SSL_CTX_set_cipher_list(s_ssl_ctx, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");
    if (!ret) {
        MS_THROW_ERROR("set cipher list error");
    }

    SSL_CTX_set_ecdh_auto(s_ssl_ctx, 1);

    int index = 0;
    std::string dtls_srtp_crypto;
    for (auto item : srtp_crypto_suite_vec) {
        if (index != 0) {
            dtls_srtp_crypto += ":";
        }

        dtls_srtp_crypto += item.name;
        index++;
    }

    log_infof("dtls srtp crypto: %s", dtls_srtp_crypto.c_str());

    ret = SSL_CTX_set_tlsext_use_srtp(s_ssl_ctx, dtls_srtp_crypto.c_str());
    if (ret != 0) {
        MS_THROW_ERROR("init srtp tlsext error");
    }

    for (auto& item : string2finger_print_algorithm) {
        const std::string& algorithm_str      = item.first;
        finger_print_algorithm_enum algorithm = item.second;
        uint8_t bin_finger_print[EVP_MAX_MD_SIZE];
        char finger_print_sz[(EVP_MAX_MD_SIZE * 3) + 1];
        unsigned int size = 0;

        const EVP_MD* hash_function;

        switch(algorithm)
        {
            case FINGER_SHA1:
            {
                hash_function = EVP_sha1();
                break;
            }
            case FINGER_SHA224:
            {
                hash_function = EVP_sha224();
                break;
            }
            case FINGER_SHA256:
            {
                hash_function = EVP_sha256();
                break;
            }
            case FINGER_SHA384:
            {
                hash_function = EVP_sha384();
                break;
            }
            case FINGER_SHA512:
            {
                hash_function = EVP_sha512();
                break;
            }
            default:
            {
                MS_THROW_ERROR("unkown finger print type:%d", (int)algorithm);
            }
        }
        int ret = X509_digest(s_certificate, hash_function, bin_finger_print, &size);
        if (!ret) {
            MS_THROW_ERROR("X509_digest error");
        }
        for (unsigned int index = 0; index < size; index++)
        {
            std::sprintf(finger_print_sz + (index * 3), "%.2X:", bin_finger_print[index]);
        }
        finger_print_sz[(size * 3) - 1] = '\0';
        log_infof("dtls %s fingerprint: %s", algorithm_str.c_str(), finger_print_sz);

        finger_print_info info;

        info.algorithm = string2finger_print_algorithm[algorithm_str];
        info.value     = finger_print_sz;

        s_local_fingerprint_vec.push_back(info);
    }

    return;
}

static int on_ssl_certificate_verify(int, X509_STORE_CTX*) {
    log_infof("ssl certificate verify callback: enable");
    return 1;
}

static void on_ssl_info(const SSL* ssl, int type, int value) {
    log_infof("ssl info callback type:%d, value:%d", type, value);
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
    log_infof("udp write callback len:%lu, remote:%s",
        sent_size, address.to_string().c_str());
}

void single_udp_session_callback::on_read(const char* data, size_t data_size, udp_tuple address) {
    webrtc_session* session = nullptr;
    
    std::string peerid = address.to_string();
    session = get_webrtc_session(peerid);
    if (session) {
        session->on_recv_packet((uint8_t*)data, data_size, address);
        return;
    }

    if(stun_packet::is_stun((uint8_t*)data, data_size)) {
        stun_packet* packet = nullptr;
        log_infof("receive first stun packet...");
        try
        {
            packet = stun_packet::parse((uint8_t*)data, data_size);

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
    username_fragment_ = get_random_string(16);
    user_pwd_          = get_random_string(32);

    insert_webrtc_session(username_fragment_, this);

    log_infof("webrtc_session construct username fragement:%s, user password:%s",
        username_fragment_.c_str(), user_pwd_.c_str());
    
    //for dtls
    ssl_ = SSL_new(s_ssl_ctx);
    if (!ssl_) {
        MS_THROW_ERROR("ssl new error");
    }

    SSL_set_ex_data(ssl_, 0, static_cast<void*>(this));

    ssl_bio_read_ = BIO_new(BIO_s_mem());

    if (!ssl_bio_read_) {
        SSL_free(ssl_);
        MS_THROW_ERROR("init ssl bio read error");
    }

    ssl_bio_write_ = BIO_new(BIO_s_mem());
    if (!ssl_bio_write_) {
        BIO_free(ssl_bio_read_);
        SSL_free(ssl_);
        MS_THROW_ERROR("init ssl bio write error");
    }

    SSL_set_bio(ssl_, ssl_bio_read_, ssl_bio_write_);

    const int dtls_mtu = 1350;
    SSL_set_mtu(ssl_, dtls_mtu);
    DTLS_set_link_mtu(ssl_, dtls_mtu);

    //DTLS_set_timer_cb(ssl_, on_ssl_dtls_timer);
}

webrtc_session::~webrtc_session() {

}

void webrtc_session::write_udp_data(uint8_t* data, size_t data_size, const udp_tuple& address) {
    if (!single_udp_server_ptr) {
        MS_THROW_ERROR("single udp server is not inited");
    }
    single_udp_server_ptr->write((char*)data, data_size, address);
}

void webrtc_session::on_recv_packet(const uint8_t* data, size_t data_size,
                            const udp_tuple& address) {
    if (stun_packet::is_stun((uint8_t*)data, data_size)) {
        log_infof("receive stun packet len:%lu, remote:%s",
                data_size, address.to_string().c_str());
        try {
            stun_packet* packet = stun_packet::parse((uint8_t*)data, data_size);
            on_handle_stun_packet(packet, address);
        }
        catch(const std::exception& e) {
            log_errorf("handle stun packet exception:%s", e.what());
        }
    } else if (is_rtcp(data, data_size)) {
        log_infof("receive rtcp packet len:%lu, remote:%s",
                data_size, address.to_string().c_str());
    } else if (is_rtp(data, data_size)) {
        log_infof("receive rtp packet len:%lu, remote:%s",
                data_size, address.to_string().c_str());
    } else if (webrtc_session::is_dtls(data, data_size)) {
        log_infof("receive dtls packet len:%lu, remote:%s",
                data_size, address.to_string().c_str());
    } else {
        log_errorf("receive unkown packet len:%lu, remote:%s",
                data_size, address.to_string().c_str());
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
            log_infof("stun packet is authentication ok, remote:%s", address.to_string().c_str());
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

        log_infof("stun packet response:\r\n %s", resp_pkt->dump().c_str());
        write_udp_data(resp_pkt->data, resp_pkt->data_len, address);
        delete resp_pkt;
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
    for (auto item : s_local_fingerprint_vec) {
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

bool webrtc_session::is_dtls(const uint8_t* data, size_t len) {
    //https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
    return ((len >= 13) && (data[0] > 19 && data[0] < 64));
}