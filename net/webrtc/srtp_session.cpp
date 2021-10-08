#include "srtp_session.hpp"
#include "logger.hpp"
#include <vector>

std::vector<const char*> srtp_session::errors =
{
    // From 0 (srtp_err_status_ok) to 24 (srtp_err_status_pfkey_err).
    "success (srtp_err_status_ok)",
    "unspecified failure (srtp_err_status_fail)",
    "unsupported parameter (srtp_err_status_bad_param)",
    "couldn't allocate memory (srtp_err_status_alloc_fail)",
    "couldn't deallocate memory (srtp_err_status_dealloc_fail)",
    "couldn't initialize (srtp_err_status_init_fail)",
    "can’t process as much data as requested (srtp_err_status_terminus)",
    "authentication failure (srtp_err_status_auth_fail)",
    "cipher failure (srtp_err_status_cipher_fail)",
    "replay check failed (bad index) (srtp_err_status_replay_fail)",
    "replay check failed (index too old) (srtp_err_status_replay_old)",
    "algorithm failed test routine (srtp_err_status_algo_fail)",
    "unsupported operation (srtp_err_status_no_such_op)",
    "no appropriate context found (srtp_err_status_no_ctx)",
    "unable to perform desired validation (srtp_err_status_cant_check)",
    "can’t use key any more (srtp_err_status_key_expired)",
    "error in use of socket (srtp_err_status_socket_err)",
    "error in use POSIX signals (srtp_err_status_signal_err)",
    "nonce check failed (srtp_err_status_nonce_bad)",
    "couldn’t read data (srtp_err_status_read_fail)",
    "couldn’t write data (srtp_err_status_write_fail)",
    "error parsing data (srtp_err_status_parse_err)",
    "error encoding data (srtp_err_status_encode_err)",
    "error while using semaphores (srtp_err_status_semaphore_err)",
    "error while using pfkey (srtp_err_status_pfkey_err)"
};

void srtp_session::init() {
    log_infof("libsrtp version: <%s>", srtp_get_version_string());

    srtp_err_status_t err = srtp_init();
    if ((err != srtp_err_status_ok)) {
        MS_THROW_ERROR("set srtp_init error: %s", srtp_session::errors.at(err));
    }

    err = srtp_install_event_handler(static_cast<srtp_event_handler_func_t*>(srtp_session::on_srtp_event));
    if ((err != srtp_err_status_ok)) {
        MS_THROW_ERROR("set srtp_install_event_handler error: %s", srtp_session::errors.at(err));
    }
    log_infof("srtp session init ok...");
}

void srtp_session::on_srtp_event(srtp_event_data_t* data) {
    switch (data->event)
    {
        case event_ssrc_collision:
            log_warnf("SSRC collision occurred");
            break;

        case event_key_soft_limit:
            log_warnf("stream reached the soft key usage limit and will expire soon");
            break;

        case event_key_hard_limit:
            log_warnf("stream reached the hard key usage limit and has expired");
            break;

        case event_packet_index_limit:
            log_warnf("stream reached the hard packet limit (2^48 packets)");
            break;
        default:
            log_errorf("unkown srtp event:%d", data->event);
    }
}

srtp_session::srtp_session(SRTP_SESSION_TYPE session_type, CRYPTO_SUITE_ENUM suite, uint8_t* key, size_t key_len)
{
    srtp_policy_t policy;

    std::memset((void*)&policy, 0, sizeof(srtp_policy_t));

    switch (suite)
    {
        case CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_80:
        {
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
            break;
        }
        case CRYPTO_SUITE_AES_CM_128_HMAC_SHA1_32:
        {
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
            srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
            break;
        }
        case CRYPTO_SUITE_AEAD_AES_256_GCM:
        {
            srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
            srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
            break;
        }
        case CRYPTO_SUITE_AEAD_AES_128_GCM:
        {
            srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
            srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
            break;
        }
        default:
        {
            MS_THROW_ERROR("unknown srtp crypto suite:%d", (int)suite);
        }
    }

    if ((int)key_len != policy.rtp.cipher_key_len) {
        MS_THROW_ERROR("key length(%d) error, configure key length(%d)",
                (int)key_len, policy.rtp.cipher_key_len);
    }

    std::string session_desc;
    switch (session_type)
    {
        case SRTP_SESSION_IN_TYPE:
        {
            session_desc = "srtp read";
            policy.ssrc.type = ssrc_any_inbound;
            break;
        }
        case SRTP_SESSION_OUT_TYPE:
        {
            session_desc = "srtp write";
            policy.ssrc.type = ssrc_any_outbound;
            break;
        }
        default:
        {
            MS_THROW_ERROR("unknown srtp session type:%d", (int)session_type);
        }
    }

    policy.ssrc.value      = 0;
    policy.key             = key;
    policy.allow_repeat_tx = 1;
    policy.window_size     = 1024;
    policy.next            = nullptr;

    srtp_err_status_t err = srtp_create(&session_, &policy);

    if (err != srtp_err_status_ok)
        MS_THROW_ERROR("srtp_create error: %s", srtp_session::errors.at(err));
    log_infof("srtp session construct, type: %s", session_desc.c_str());
}

srtp_session::~srtp_session() {
    if (session_ != nullptr) {
        srtp_err_status_t err = srtp_dealloc(session_);

        if (err != srtp_err_status_ok)
            log_errorf("srtp_dealloc error: %s", srtp_session::errors.at(err));
    }
}

bool srtp_session::encrypt_rtp(const uint8_t** data, size_t* len) {
    //#define SRTP_MAX_TRAILER_LEN (SRTP_MAX_TAG_LEN + SRTP_MAX_MKI_LEN)
    //SRTP_MAX_TRAILER_LEN=-16+128
    if (*len + SRTP_MAX_TRAILER_LEN > SRTP_ENCRYPT_BUFFER_SIZE) {
        log_errorf("fail to encrypt RTP packet, size too big (%lu bytes)", *len);
        return false;
    }

    std::memcpy(encrypt_buffer_, *data, *len);

    srtp_err_status_t err = srtp_protect(session_, (void*)(encrypt_buffer_), (int*)(len));

    if (err != srtp_err_status_ok) {
        log_errorf("srtp_protect error: %s", srtp_session::errors.at(err));
        return false;
    }

    *data = encrypt_buffer_;
    return true;
}

bool srtp_session::decrypt_srtp(uint8_t* data, size_t* len) {
    srtp_err_status_t err = srtp_unprotect(session_, (void*)(data), (int*)(len));
    if (err != srtp_err_status_ok) {
        log_errorf("srtp_unprotect error: %s", srtp_session::errors.at(err));
        return false;
    }
    return true;
}

bool srtp_session::encrypt_rtcp(const uint8_t** data, size_t* len) {
    //#define SRTP_MAX_TRAILER_LEN (SRTP_MAX_TAG_LEN + SRTP_MAX_MKI_LEN)
    //SRTP_MAX_TRAILER_LEN=-16+128
    if (*len + SRTP_MAX_TRAILER_LEN > SRTP_ENCRYPT_BUFFER_SIZE) {
        log_errorf("fail to encrypt RTP packet, size too big (%lu bytes)", *len);
        return false;
    }

    std::memcpy(encrypt_buffer_, *data, *len);

    srtp_err_status_t err = srtp_protect_rtcp(session_, (void*)(encrypt_buffer_), (int*)(len));

    if (err != srtp_err_status_ok) {
        log_errorf("srtp_protect_rtcp error: %s", srtp_session::errors.at(err));
        return false;
    }

    *data = encrypt_buffer_;
    return true;
}

bool srtp_session::decrypt_srtcp(uint8_t* data, size_t* len) {
    srtp_err_status_t err = srtp_unprotect_rtcp(session_, (void*)(data), (int*)(len));
    if (err != srtp_err_status_ok) {
        log_errorf("srtp_unprotect_rtcp error: %s", srtp_session::errors.at(err));
        return false;
    }
    return true;
}

void srtp_session::remove_stream(uint32_t ssrc) {
    srtp_remove_stream(session_, (uint32_t)(htonl(ssrc)));
    return;
}