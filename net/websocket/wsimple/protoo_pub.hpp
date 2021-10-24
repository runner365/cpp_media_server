#ifndef PROTOO_PUB_HPP
#define PROTOO_PUB_HPP
#include <string>

#define REQUEST_ID_ERROR          1000
#define RESPONSE_ID_ERROR         1001
#define NOTIFICATION_ID_ERROR     1002
#define REQUEST_HANDLE_ERROR      1003
#define RESPONSE_HANDLE_ERROR     1004
#define NOTIFICATION_HANDLE_ERROR 1005
#define UID_ERROR                 1006
#define ROOMID_ERROR              1007
#define SDP_ERROR                 1008
#define METHOD_ERROR              1009
#define REPEAT_PUBLISH_ERROR      1010
#define NO_PUBLISH_ERROR          1011
#define MIDS_ERROR                1012

class protoo_request_interface
{
public:
    virtual void accept(const std::string& id, const std::string& data) = 0;
    virtual void reject(const std::string& id, int err_code, const std::string& err) = 0;
    virtual void notification(const std::string& method, const std::string& data) = 0;
};

class protoo_event_callback
{
public:
    virtual void on_open() = 0;
    virtual void on_failed() = 0;
    virtual void on_disconnected() = 0;
    virtual void on_close() = 0;
    virtual void on_request(const std::string& id, const std::string& method, const std::string& data,
                        protoo_request_interface* feedback_p) = 0;
    virtual void on_response(int err_code, const std::string& err_message, const std::string& id, 
                        const std::string& data) = 0;
    virtual void on_notification(const std::string& method, const std::string& data) = 0;
};

#endif