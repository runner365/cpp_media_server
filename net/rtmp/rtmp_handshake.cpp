#include "rtmp_handshake.hpp"
#include "rtmp_client_session.hpp"
#include "rtmp_server_session.hpp"

rtmp_server_handshake::rtmp_server_handshake(rtmp_server_session* session):session_(session)
{
}

rtmp_server_handshake::~rtmp_server_handshake()
{
}

int rtmp_server_handshake::parse_c0c1(char* c0c1) {
    c0_version_ = c0c1[0];
    log_infof("handshake version:%d", c0_version_);

    return c1s1_.parse_c1(c0c1 + 1, (size_t)1536);
}

char* rtmp_server_handshake::make_s1_data(int& s1_len) {
    int ret = c1s1_.make_s1((char*)s1_body_);
    
    if (ret != 0) {
        s1_len = 0;
        log_errorf("make s1 error:%d", ret);
        return nullptr;
    }
    s1_len = sizeof(s1_body_);
    return s1_body_;
}

char* rtmp_server_handshake::make_s2_data(int& s2_len) {
    int ret;
    
    ret = c2s2_.create_by_digest(c1s1_.get_c1_digest());
    if (ret != 0) {
        log_errorf("c2s2 create by digest s2 error...");
        return nullptr;
    }
    bool is_valid = c2s2_.validate_s2(c1s1_.get_c1_digest());
    if (!is_valid) {
        log_errorf("c2s2 validate s2 error...");
        return nullptr;
    }
    c2s2_.generate(s2_body_);
    s2_len = sizeof(s2_body_);
    return s2_body_;
};

uint32_t rtmp_server_handshake::get_c1_time() {
    return c1s1_.get_c1_time();
}

char* rtmp_server_handshake::get_c1_data() {
    return c1s1_.get_c1_data();
}

int rtmp_server_handshake::handle_c0c1() {
    const size_t c0_size = 1;
    const size_t c1_size = 1536;

    if (!session_->recv_buffer_.require(c0_size + c1_size)) {
        return RTMP_NEED_READ_MORE;
    }
    return parse_c0c1(session_->recv_buffer_.data());
}

int rtmp_server_handshake::handle_c2() {
    const size_t c2_size = 1536;

    if (!session_->recv_buffer_.require(c2_size)) {
        return RTMP_NEED_READ_MORE;
    }
    //TODO_JOB: handle c2 data.
    session_->recv_buffer_.consume_data(c2_size);
    return RTMP_OK;
}


int rtmp_server_handshake::send_s0s1s2() {
    char s0s1s2[3073];
    char* s1_data;
    int s1_len;
    char* s2_data;
    int s2_len;
    uint8_t* p = (uint8_t*)s0s1s2;

    /* ++++++ s0 ++++++*/
    rtmp_random_generate(s0s1s2, sizeof(s0s1s2));
    p[0] = RTMP_HANDSHAKE_VERSION;
    p++;

    /* ++++++ s1 ++++++*/
    s1_data = make_s1_data(s1_len);
    if (!s1_data) {
        log_errorf("make s1 data error...");
        return -1;
    }
    assert(s1_len == 1536);
    memcpy(p, s1_data, s1_len);
    p += s1_len;

    /* ++++++ s2 ++++++*/
    s2_data = make_s2_data(s2_len);
    if (!s2_data) {
        log_errorf("make s2 data error...");
        return -1;
    }
    assert(s2_len == 1536);
    memcpy(p, s2_data, s2_len);
    session_->rtmp_send(s0s1s2, (int)sizeof(s0s1s2));

    return RTMP_NEED_READ_MORE;
}

size_t rtmp_client_handshake::s0s1s2_size = 1536*2+1;

rtmp_client_handshake::rtmp_client_handshake(rtmp_client_session* session):session_(session)
{

}

rtmp_client_handshake::~rtmp_client_handshake()
{

}

int rtmp_client_handshake::send_c0c1() {
    int ret;
    uint8_t* c0c1 = random_;
    size_t c0c1_len = 1536 + 1;

    c0c1[0] = 3;
    ret = session_->rtmp_send((char*)c0c1, c0c1_len);
    if (ret != 0) {
        log_errorf("send c0c1 error.");
        return ret;
    }

    return 0;
}