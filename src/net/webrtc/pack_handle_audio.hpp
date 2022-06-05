#ifndef PACK_HANDLE_AUDIO_HPP
#define PACK_HANDLE_AUDIO_HPP
#include "pack_handle_pub.hpp"
#include "utils/av/media_packet.hpp"
#include <memory>
#include <stdint.h>
#include <stddef.h>
#include <string>

class pack_handle_audio : public pack_handle_base
{
public:
    pack_handle_audio(pack_callbackI* cb):cb_(cb)
    {
    }
    virtual ~pack_handle_audio()
    {
    }

public:
    virtual void input_rtp_packet(std::shared_ptr<rtp_packet_info> pkt_ptr) override
    {
        size_t pkt_size = pkt_ptr->pkt->get_payload_length() + 1024;
        std::shared_ptr<MEDIA_PACKET> audio_pkt_ptr = std::make_shared<MEDIA_PACKET>(pkt_size);
        int64_t dts = (int64_t)pkt_ptr->pkt->get_timestamp();

        audio_pkt_ptr->av_type_    = MEDIA_AUDIO_TYPE;
        audio_pkt_ptr->codec_type_ = MEDIA_CODEC_OPUS;
        audio_pkt_ptr->fmt_type_   = MEDIA_FORMAT_RAW;
        audio_pkt_ptr->dts_        = dts;
        audio_pkt_ptr->pts_        = dts;

        audio_pkt_ptr->buffer_ptr_->append_data((char*)pkt_ptr->pkt->get_payload(), pkt_ptr->pkt->get_payload_length());
        cb_->media_packet_output(audio_pkt_ptr);
    }

private:
    pack_callbackI* cb_ = nullptr;
};

#endif