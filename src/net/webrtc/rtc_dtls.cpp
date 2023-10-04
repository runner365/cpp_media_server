#include "rtc_dtls.hpp"
#include "logger.hpp"
#include "webrtc_session.hpp"
#include "srtp_session.hpp"

#include <string>
#include <stdio.h>
#include <openssl/asn1.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>


static const size_t SRTP_MASTER_KEY_LENGTH = 16;
static const size_t SRTP_MASTER_SALT_LENGTH = 14;
static const size_t SRTP_MASTER_LENGTH = SRTP_MASTER_KEY_LENGTH + SRTP_MASTER_SALT_LENGTH;

static const size_t SRTP_AESGCM_256_MASTER_KEY_LENGTH = 32;
static const size_t SRTP_AESGCM_256_MASTER_SALT_LENGTH = 12;
static const size_t SRTP_AESGCM_256_MASTER_LENGTH = SRTP_AESGCM_256_MASTER_KEY_LENGTH + SRTP_AESGCM_256_MASTER_SALT_LENGTH;

static const size_t SRTP_AESGCM_128_MASTER_KEY_LENGTH  = 16;
static const size_t SRTP_AESGCM_128_MASTER_SALT_LENGTH = 12;
static const size_t SRTP_AESGCM_128_MASTER_LENGTH = (SRTP_AESGCM_128_MASTER_KEY_LENGTH + SRTP_AESGCM_128_MASTER_SALT_LENGTH);

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

std::vector<SRTP_CRYPTO_SUITE_ENTRY> srtp_crypto_suites =
{
    { CRYPTO_SUITE_AEAD_AES_256_GCM, "SRTP_AEAD_AES_256_GCM" },
    { CRYPTO_SUITE_AEAD_AES_128_GCM, "SRTP_AEAD_AES_128_GCM" },
    { CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80, "SRTP_AES128_CM_SHA1_80" },
    { CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_32, "SRTP_AES128_CM_SHA1_32" }
};

std::vector<finger_print_info> rtc_dtls::s_local_fingerprint_vec;

X509* rtc_dtls::s_certificate    = nullptr;
EVP_PKEY* rtc_dtls::s_privatekey = nullptr;
SSL_CTX* rtc_dtls::s_ssl_ctx     = nullptr;

std::string get_dtls_mode_desc(DTLS_ROLE mode) {
    switch (mode)
    {
    case ROLE_AUTO:
        return "role_auto";
    case ROLE_CLIENT:
        return "role_client";
    case ROLE_SERVER:
        return "role_server";
    default:
        break;
    }
    return "role_none";
}

void ssl_info_callback(const SSL* ssl, int type, int value) {
    log_infof("ssl info callback type:0x%08x, value:0x%08x", type, value);
    if (static_cast<rtc_dtls*>(SSL_get_ex_data(ssl, 0)) != nullptr) {
        static_cast<rtc_dtls*>(SSL_get_ex_data(ssl, 0))->on_ssl_info(type, value);
    }
}

void rtc_dtls::dtls_init(const std::string& key_file, const std::string& cert_file) {
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

    SSL_CTX_set_info_callback(s_ssl_ctx, ssl_info_callback);

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

SSL_CTX* rtc_dtls::get_ssl_ctx() {
    return s_ssl_ctx;
}

int rtc_dtls::on_ssl_certificate_verify(int, X509_STORE_CTX*) {
    log_infof("ssl certificate verify callback: enable");
    return 1;
}

void rtc_dtls::on_ssl_info(int type, int value) {
    std::string role;

    if ((type & SSL_ST_CONNECT) != 0)
        role = "client";
    else if ((type & SSL_ST_ACCEPT) != 0)
        role = "server";
    else
        role = "undefined";

    if ((type & SSL_CB_LOOP) != 0) {
        log_infof("[role:%s, action:'%s']", role.c_str(), SSL_state_string_long(ssl_));
    }
    else if ((type & SSL_CB_ALERT) != 0) {
        std::string alert_type;

        switch (*SSL_alert_type_string(value))
        {
            case 'W':
                alert_type = "warning";
                break;

            case 'F':
                alert_type = "fatal";
                break;

            default:
                alert_type = "undefined";
        }

        if ((type & SSL_CB_READ) != 0) {
            log_infof("received DTLS %s alert: %s", alert_type.c_str(), SSL_alert_desc_string_long(value));
        }
        else if ((type & SSL_CB_WRITE) != 0) {
            log_infof("sending DTLS %s alert: %s", alert_type.c_str(), SSL_alert_desc_string_long(value));
        }
        else {
            log_infof("DTLS %s alert: %s", alert_type.c_str(), SSL_alert_desc_string_long(value));
        }
    } else if ((type & SSL_CB_EXIT) != 0) {
        if (value == 0) {
            log_infof("[role:%s, failed:'%s']", role.c_str(), SSL_state_string_long(ssl_));
        }
        else if (value < 0) {
            log_infof("role: %s, waiting:'%s']", role.c_str(), SSL_state_string_long(ssl_));
        }
    } else if ((type & SSL_CB_HANDSHAKE_START) != 0) {
        log_infof("DTLS handshake start");
    } else if ((type & SSL_CB_HANDSHAKE_DONE) != 0) {
        log_infof("open ssl handshake done.");
        handshake_done_ = true;
    }

}

rtc_dtls::rtc_dtls(webrtc_session* session, uv_loop_t* loop): timer_interface(loop, 3*1000)
    , session_(session) {
    state = DTLS_NEW;
    role  = ROLE_SERVER;//role must be "server" in ice-lite mode.

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

    ssl_read_buffer_ = new uint8_t[SSL_READ_BUFFER_SIZE];

    log_infof("rtc_dtls construct ok...");
    //DTLS_set_timer_cb(ssl_, on_ssl_dtls_timer);
}

rtc_dtls::~rtc_dtls() {
    if (ssl_) {
        SSL_free(ssl_);
        ssl_ = nullptr;
        ssl_bio_read_  = nullptr;
        ssl_bio_write_ = nullptr;
    }

    if (ssl_read_buffer_) {
        delete[] ssl_read_buffer_;
        ssl_read_buffer_ = nullptr;
    }
}

void rtc_dtls::start(DTLS_ROLE role_mode) {
    //assert(role_mode == ROLE_SERVER);

    if (this->state != DTLS_NEW) {
        return;
    }
    this->role  = role_mode;
    this->state = DTLS_CONNECTING;

    log_infof("rtc dtls start, role mode:%s", get_dtls_mode_desc(role_mode).c_str());

    if (role_mode == ROLE_SERVER) {
        SSL_set_accept_state(ssl_);
        SSL_do_handshake(ssl_);
    } else {//ROLE_CLIENT
        SSL_set_connect_state(ssl_);
        SSL_do_handshake(ssl_);
        send_pending_dtls_data();
        start_timer();
    }

    return;
}

void rtc_dtls::on_timer() {
    if (handshake_done_) {
        log_debugf("the rtc dtls handshake is done, so cancel the timer.");
        return;
    }

    log_infof("------------ rtc dtls on timer --------------");
    int ret = DTLSv1_handle_timeout(ssl_);

    if (ret == 1) {
        send_pending_dtls_data();
    } else if (ret == -1) {
        log_errorf("DTLSv1_handle_timeout failed.");
        this->state = DTLS_FAILED;
    }

    //start_timer();
}

void rtc_dtls::handle_dtls_data(const uint8_t* data, size_t data_len) {
    int written = BIO_write(this->ssl_bio_read_, (void*)data, (int)data_len);
    if (written != (int)data_len) {
        log_warnf("bio write len(%d) is not equal to input data len(%lu)", written, data_len);
    } else {
        log_infof("bio write len:%d ok", written);
    }

    int read = SSL_read(ssl_, (void*)ssl_read_buffer_, SSL_READ_BUFFER_SIZE);

    log_infof("ssl read buffer len:%d", read);

    send_pending_dtls_data();

    if (!check_status(read)) {
        return;
    }

    //start_timer();
}

void rtc_dtls::send_pending_dtls_data() {
    if (BIO_eof(this->ssl_bio_write_)) {
        log_infof("ssl bio write is eof...");
        return;
    }

    int64_t read = 0;
    char* data = nullptr;

    read = BIO_get_mem_data(ssl_bio_write_, &data);
    if (read <= 0) {
        log_errorf("BIO_get_mem_data return %ld", read);
        return;
    }

    log_infof("dtls data is ready to be sent to the client, length:%ld", read);

    //send to client
    try {
        session_->send_plaintext_data((uint8_t*)data, (size_t)read);
    } catch(const std::exception& e) {
        log_errorf("session send plaintext data exception:%s", e.what());
    }

    (void)BIO_reset(ssl_bio_write_);
}

bool rtc_dtls::check_status(int ret_code) {
    int err = SSL_get_error(ssl_, ret_code);

    switch (err)
    {
        case SSL_ERROR_NONE:
            log_errorf("SSL status: SSL_ERROR_NONE");
            break;

        case SSL_ERROR_SSL:
            log_errorf("SSL status: SSL_ERROR_SSL");
            break;

        case SSL_ERROR_WANT_READ:
            //log_errorf("SSL status: SSL_ERROR_WANT_READ");
            break;

        case SSL_ERROR_WANT_WRITE:
            log_errorf("SSL status: SSL_ERROR_WANT_WRITE");
            break;

        case SSL_ERROR_WANT_X509_LOOKUP:
            log_errorf("SSL status: SSL_ERROR_WANT_X509_LOOKUP");
            break;

        case SSL_ERROR_SYSCALL:
            log_errorf("SSL status: SSL_ERROR_SYSCALL");
            break;

        case SSL_ERROR_ZERO_RETURN:
            log_errorf("SSL status: SSL_ERROR_ZERO_RETURN");
            break;

        case SSL_ERROR_WANT_CONNECT:
            log_errorf("SSL status: SSL_ERROR_WANT_CONNECT");
            break;

        case SSL_ERROR_WANT_ACCEPT:
            log_errorf("SSL status: SSL_ERROR_WANT_ACCEPT");
            break;

        default:
            log_errorf("SSL status: unknown error");
    }

    if (handshake_done_) {
        if (this->remote_finger_print.algorithm != FINGER_NONE) {
            log_infof("start processing handshake...");
            return process_handshake();
        }
    }
    return true;
}

bool rtc_dtls::process_handshake() {
    if (!check_remote_fingerprint()) {
        log_errorf("check remote fingerprint error");
        this->state = DTLS_FAILED;
        return false;
    }

    CRYPTO_SUITE_ENUM srtp_suite = get_srtp_crypto_suite();

    if (srtp_suite == CRYPTO_SUITE_NONE) {
        this->state = DTLS_FAILED;
        log_errorf("get_srtp_crypto_suite fail");
        return false;
    }

    log_infof("get srtp crypto suite:%", get_crypto_suite_desc(srtp_suite).c_str());

    extract_srtp_keys(srtp_suite);

    return true;
}

void rtc_dtls::extract_srtp_keys(CRYPTO_SUITE_ENUM srtp_suite) {
    size_t srtp_keylength{ 0 };
    size_t srtp_saltlength{ 0 };
    size_t srtp_masterlength{ 0 };

    switch (srtp_suite)
    {
        case CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80:
        case CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_32:
        {
            srtp_keylength    = SRTP_MASTER_KEY_LENGTH;
            srtp_saltlength   = SRTP_MASTER_SALT_LENGTH;
            srtp_masterlength = SRTP_MASTER_LENGTH;
            break;
        }

        case CRYPTO_SUITE_AEAD_AES_256_GCM:
        {
            srtp_keylength    = SRTP_AESGCM_256_MASTER_KEY_LENGTH;
            srtp_saltlength   = SRTP_AESGCM_256_MASTER_SALT_LENGTH;
            srtp_masterlength = SRTP_AESGCM_256_MASTER_LENGTH;
            break;
        }

        case CRYPTO_SUITE_AEAD_AES_128_GCM:
        {
            srtp_keylength    = SRTP_AESGCM_128_MASTER_KEY_LENGTH;
            srtp_saltlength   = SRTP_AESGCM_128_MASTER_SALT_LENGTH;
            srtp_masterlength = SRTP_AESGCM_128_MASTER_LENGTH;

            break;
        }
        default:
        {
            MS_THROW_ERROR("unknown srtp crypto suite");
        }
    }

    uint8_t* srtp_material = new uint8_t[srtp_masterlength * 2];
    uint8_t* srtp_local_key  = nullptr;
    uint8_t* srtp_local_salt = nullptr;
    uint8_t* srtp_remote_key = nullptr;
    uint8_t* srtp_remote_salt = nullptr;
    
    uint8_t* srtp_local_masterkey  = new uint8_t[srtp_masterlength];
    uint8_t* srtp_remote_masterkey = new uint8_t[srtp_masterlength];

    int ret = SSL_export_keying_material(ssl_, srtp_material, srtp_masterlength * 2,
                            "EXTRACTOR-dtls_srtp", 19, nullptr, 0, 0);

    if (ret != 1) {
        log_errorf("SSL_export_keying_material error:%d", ret);
        delete[] srtp_material;
        delete[] srtp_local_masterkey;
        delete[] srtp_remote_masterkey;
        return;
    }

    if (role == ROLE_SERVER) {
        srtp_remote_key  = srtp_material;
        srtp_local_key   = srtp_remote_key + srtp_keylength;
        srtp_remote_salt = srtp_local_key + srtp_keylength;
        srtp_local_salt  = srtp_remote_salt + srtp_saltlength;
    } else if (role == ROLE_CLIENT) {
        srtp_local_key   = srtp_material;
        srtp_remote_key  = srtp_local_key + srtp_keylength;
        srtp_local_salt  = srtp_remote_key + srtp_keylength;
        srtp_remote_salt = srtp_local_salt + srtp_saltlength;
    } else {
        log_errorf("unkown dtls role:%s", get_dtls_mode_desc(role).c_str());
        return;
    }


    memcpy(srtp_local_masterkey, srtp_local_key, srtp_keylength);
    memcpy(srtp_local_masterkey + srtp_keylength, srtp_local_salt, srtp_saltlength);

    // Create the SRTP remote master key.
    memcpy(srtp_remote_masterkey, srtp_remote_key, srtp_keylength);
    memcpy(srtp_remote_masterkey + srtp_keylength, srtp_remote_salt, srtp_saltlength);


    this->state = DTLS_CONNECTED;

    log_infof("srtp call dtls connected....");

    //set srtp parameters
    this->session_->on_dtls_connected(srtp_suite,
                srtp_local_masterkey, srtp_masterlength,
                srtp_remote_masterkey, srtp_masterlength,
                this->remote_cert);

    delete[] srtp_material;
    delete[] srtp_local_masterkey;
    delete[] srtp_remote_masterkey;

    return;
}

CRYPTO_SUITE_ENUM rtc_dtls::get_srtp_crypto_suite() {
    CRYPTO_SUITE_ENUM srtp_suite = CRYPTO_SUITE_NONE;
    SRTP_PROTECTION_PROFILE* srtp_profile = SSL_get_selected_srtp_profile(ssl_);

    if (!srtp_profile) {
        return srtp_suite;
    }

    for (auto& item_suite : srtp_crypto_suites) {
        if (strcmp(item_suite.name, srtp_profile->name) == 0) {
            srtp_suite = item_suite.crypto_suite;
            break;
        }
    }

    log_infof("get srtp crypto suite:%d", (int)srtp_suite);
    return srtp_suite;
}

bool rtc_dtls::check_remote_fingerprint() {
    if (this->remote_finger_print.algorithm == FINGER_NONE) {
        log_errorf("remote fingerprint algorithm(%d) error", this->remote_finger_print.algorithm);
        return false;
    }

    X509* certificate = nullptr;
    uint8_t binary_fingerprint[EVP_MAX_MD_SIZE];
    unsigned int size{ 0 };
    char hex_fingerprint[(EVP_MAX_MD_SIZE * 3) + 1];
    const EVP_MD* hash_function;
    int ret;

    certificate = SSL_get_peer_certificate(ssl_);

    if (!certificate) {
        log_errorf("no certificate was provided by the peer");
        return false;
    }

    switch (remote_finger_print.algorithm)
    {
        case FINGER_SHA1:
            hash_function = EVP_sha1();
            break;

        case FINGER_SHA224:
            hash_function = EVP_sha224();
            break;

        case FINGER_SHA256:
            hash_function = EVP_sha256();
            break;

        case FINGER_SHA384:
            hash_function = EVP_sha384();
            break;

        case FINGER_SHA512:
            hash_function = EVP_sha512();
            break;

        default:
            MS_THROW_ERROR("unknown algorithm:%d", (int)remote_finger_print.algorithm);
    }

    // Compare the remote fingerprint with the value given via signaling.
    ret = X509_digest(certificate, hash_function, binary_fingerprint, &size);
    if (ret == 0) {
        log_errorf("X509_digest() failed");
        X509_free(certificate);
        return false;
    }

    // Convert to hexadecimal format in uppercase with colons.
    for (unsigned int i{ 0 }; i < size; ++i) {
        std::sprintf(hex_fingerprint + (i * 3), "%.2X:", binary_fingerprint[i]);
    }
    hex_fingerprint[(size * 3) - 1] = '\0';

    if (this->remote_finger_print.value != hex_fingerprint) {
        log_warnf("fingerprint in the remote certificate (%s) does not match the announced one (%s)",
                hex_fingerprint, this->remote_finger_print.value.c_str());
        X509_free(certificate);
        return false;
    }

    // Get the remote certificate in PEM format.
    BIO* bio = BIO_new(BIO_s_mem());

    (void)BIO_set_close(bio, BIO_CLOSE);

    ret = PEM_write_bio_X509(bio, certificate);

    if (ret != 1) {
        log_errorf("PEM_write_bio_X509() failed");
        X509_free(certificate);
        BIO_free(bio);
        return false;
    }

    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    if (!mem || !mem->data || mem->length == 0) {
        log_errorf("BIO_get_mem_ptr() failed");
        X509_free(certificate);
        BIO_free(bio);
        return false;
    }

    this->remote_cert = std::string(mem->data, mem->length);
    log_infof("udpate remote cert:\r\n%s", this->remote_cert.c_str());

    X509_free(certificate);
    BIO_free(bio);

    return true;
}

bool rtc_dtls::is_dtls(const uint8_t* data, size_t len) {
    //https://tools.ietf.org/html/draft-ietf-avtcore-rfc5764-mux-fixes
    return ((len >= 13) && (data[0] > 19 && data[0] < 64));
}
