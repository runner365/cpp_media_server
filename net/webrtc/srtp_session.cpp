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
    srtp_err_status_t err = srtp_install_event_handler(static_cast<srtp_event_handler_func_t*>(srtp_session::on_srtp_event));

    if ((err != srtp_err_status_ok))
    {
        MS_THROW_ERROR("set srtp_install_event_handler error: %s", srtp_session::errors.at(err));
    }
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

srtp_session::srtp_session(SRTP_SESSION_TYPE session_type, CRYPTO_SUITE_ENUM suite, uint8_t* key, size_t keyLen)
{

}

srtp_session::~srtp_session() {

}

bool srtp_session::encrypt_rtp(const uint8_t** data, size_t* len) {

    return true;
}

bool srtp_session::decrypt_srtp(uint8_t* data, size_t* len) {

    return true;
}

bool srtp_session::encrypt_rtcp(const uint8_t** data, size_t* len) {

    return true;
}

bool srtp_session::decrypt_srtcp(uint8_t* data, size_t* len) {

    return true;
}

void srtp_session::remove_stream(uint32_t ssrc) {
    srtp_remove_stream(session_, (uint32_t)(htonl(ssrc)));
    return;
}