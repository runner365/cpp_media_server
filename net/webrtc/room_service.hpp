#ifndef ROOM_SERVICE_HPP
#define ROOM_SERVICE_HPP
#include "net/websocket/wsimple/protoo_pub.hpp"
#include "rtc_session_pub.hpp"
#include "user_info.hpp"
#include "json.hpp"
#include <string>
#include <memory>
#include <unordered_map>

using json = nlohmann::json;

class room_service : public protoo_event_callback, public room_callback_interface
{
public:
    room_service(const std::string& roomId);
    virtual ~room_service();

public:
    virtual void on_open() override;
    virtual void on_failed() override;
    virtual void on_disconnected() override;
    virtual void on_close() override;
    virtual void on_request(const std::string& id, const std::string& method, const std::string& data,
                        protoo_request_interface* feedback_p) override;
    virtual void on_response(int err_code, const std::string& err_message, const std::string& id, 
                        const std::string& data) override;
    virtual void on_notification(const std::string& method, const std::string& data) override;

public:
    virtual void rtppacket_publisher2room(rtc_base_session* session, rtc_publisher* publisher, rtp_packet* pkt) override;

private:
    void handle_join(const std::string& id, const std::string& method,
                const std::string& data, protoo_request_interface* feedback_p);
    void handle_publish(const std::string& id, const std::string& method,
                const std::string& data, protoo_request_interface* feedback_p);

private:
    std::shared_ptr<user_info> get_user_info(const std::string& uid);
    std::string get_uid_by_json(json& json_obj);

private:
    std::string roomId_;
    std::unordered_map<std::string, std::shared_ptr<user_info>> users_;//key: uid, value: user_info
};

std::shared_ptr<protoo_event_callback> GetorCreate_room_service(const std::string& roomId);
void remove_room_service(const std::string& roomId);

#endif