#ifndef HTTP_FLV_WRITER_HPP
#define HTTP_FLV_WRITER_HPP
#include "http_common.hpp"
#include "media_packet.hpp"
#include "session_aliver.hpp"

class httpflv_writer : public av_writer_base, public session_aliver
{
public:
    httpflv_writer(std::string key, std::string id, std::shared_ptr<http_response> resp, bool has_video = true, bool has_audio = true);
    virtual ~httpflv_writer();

public:
    virtual int write_packet(MEDIA_PACKET_PTR) override;
    virtual std::string get_key() override;
    virtual std::string get_writerid() override;
    virtual void close_writer() override;
    virtual bool is_inited() override;
    virtual void set_init_flag(bool flag) override;

private:
    int send_flv_header();

private:
    std::shared_ptr<http_response> resp_;
    std::string key_;
    std::string writer_id_;
    bool init_flag_ = false;
    bool has_video_ = true;
    bool has_audio_ = true;
    bool flv_header_ready_ = false;
    bool closed_flag_ = false;
};

#endif