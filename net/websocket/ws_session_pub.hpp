#ifndef WS_SESSION_PUB_HPP
#define WS_SESSION_PUB_HPP

class ws_session_callback
{
public:
    ws_session_callback() {};
    virtual ~ws_session_callback() {};

public:
    virtual void on_accept() = 0;
    virtual void on_read(const char* data, size_t len) = 0;
    virtual void on_writen(int len) = 0;
    virtual void on_close(int err_code) = 0;
};


#endif