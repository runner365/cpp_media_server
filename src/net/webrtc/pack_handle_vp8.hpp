#ifndef VP8_PACK_HANDLE_HPP
#define VP8_PACK_HANDLE_HPP
#include "pack_handle_pub.hpp"
#include "rtp_packet.hpp"
#include "timer.hpp"
#include "media_packet.hpp"
#include <queue>

class pack_handle_vp8 : public pack_handle_base, public timer_interface
{
public:
    pack_handle_vp8(pack_callbackI* cb, boost::asio::io_context& io_ctx);
    virtual ~pack_handle_vp8();

public:
    virtual void input_rtp_packet(std::shared_ptr<rtp_packet_info> pkt_ptr) override;

public:
    virtual void on_timer() override;

private:
    void clear_packet_buffer();
    void output_packet();

private:
    pack_callbackI* cb_ = nullptr;
    MEDIA_PACKET_PTR buffer_pkt_ptr_;
    bool init_ = false;
    int64_t last_seq_ = 0;
};

#endif