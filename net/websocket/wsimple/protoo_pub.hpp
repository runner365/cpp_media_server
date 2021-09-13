#ifndef PROTOO_PUB_HPP
#define PROTOO_PUB_HPP
#include <string>

using PROTOO_ACCEPT_FUNC = void(*)();
using PROTOO_REJECT_FUNC = void(*)();

class protoo_request_interface
{
public:
    virtual void accept(const std::string& id, const std::string& data) = 0;
    virtual void reject(int err_code, const std::string& err) = 0;
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