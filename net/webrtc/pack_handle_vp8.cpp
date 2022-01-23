#include "pack_handle_vp8.hpp"

pack_handle_vp8::pack_handle_vp8(pack_callbackI* cb, boost::asio::io_context& io_ctx):timer_interface(io_ctx, 100)
                                                    , cb_(cb)
{
    start_timer();

    buffer_pkt_ptr_ = std::make_shared<MEDIA_PACKET>();
}

pack_handle_vp8::~pack_handle_vp8()
{
    stop_timer();
}


void pack_handle_vp8::input_rtp_packet(std::shared_ptr<rtp_packet_info> pkt_ptr) {
    bool key_frame = false;

    if (!init_) {
        init_ = true;
        last_seq_ = pkt_ptr->extend_seq_;
    } else {
        if ((((last_seq_ + 1) != pkt_ptr->extend_seq_) && (buffer_pkt_ptr_->buffer_ptr_->data_len() > 0)) ||
            (buffer_pkt_ptr_->dts_ != pkt_ptr->pkt->get_timestamp())) {
            last_seq_ = pkt_ptr->extend_seq_;
            clear_packet_buffer();
            return;
        }
        last_seq_ = pkt_ptr->extend_seq_;
    }

    uint8_t extended_control_bits;
    uint8_t start_of_vp8_partition;
    const uint8_t *ptr, *pend;

    ptr = (const uint8_t *)(pkt_ptr->pkt->get_payload());
    pend = ptr + pkt_ptr->pkt->get_payload_length();

    // VP8 payload descriptor
    extended_control_bits = ptr[0] & 0x80;  //X
    start_of_vp8_partition = ptr[0] & 0x10; //S
    ptr++;

    /*
         0 1 2 3 4 5 6 7
        +-+-+-+-+-+-+-+-+
        |X|R|N|S|R| PID | (REQUIRED)
        +-+-+-+-+-+-+-+-+
    X:  |I|L|T|K|   RSV | (OPTIONAL)
        +-+-+-+-+-+-+-+-+
    I:  |M|  PictureID  | (OPTIONAL)
        +-+-+-+-+-+-+-+-+
        |   PictureID   |
        +-+-+-+-+-+-+-+-+
    L:  |   TL0PICIDX   | (OPTIONAL)
        +-+-+-+-+-+-+-+-+
    T/K:|TID|Y|  KEYIDX | (OPTIONAL)
        +-+-+-+-+-+-+-+-+
    */
    if (extended_control_bits && ptr < pend) {
        uint8_t pictureid_present;
        uint8_t tl0picidx_present;
        uint8_t tid_present;
        uint8_t keyidx_present;

        pictureid_present = ptr[0] & 0x80;
        tl0picidx_present = ptr[0] & 0x40;
        tid_present = ptr[0] & 0x20;
        keyidx_present = ptr[0] & 0x10;
        ptr++;

        if (pictureid_present && ptr < pend) {
            uint16_t picture_id;
            picture_id = ptr[0] & 0x7F;
            if ((ptr[0] & 0x80) && ptr + 1 < pend) {
                picture_id = (picture_id << 8) | ptr[1];
                ptr++;
            }
            ptr++;
        }

        if (tl0picidx_present && ptr < pend) {
            // ignore temporal level zero index
            ptr++;
        }

        if ((tid_present || keyidx_present) && ptr < pend) {
            // ignore KEYIDX
            ptr++;
        }
    }

    if (ptr >= pend) {
        clear_packet_buffer();
        return;
    }

    // VP8 payload header (3 octets)
    if (start_of_vp8_partition) {
        /*
        0 1 2 3 4 5 6 7
        +-+-+-+-+-+-+-+-+
        |Size0|H| VER |P|
        +-+-+-+-+-+-+-+-+
        | Size1 |
        +-+-+-+-+-+-+-+-+
        | Size2 |
        +-+-+-+-+-+-+-+-+
        */
        // P: Inverse key frame flag. When set to 0, the current frame is a key
        //    frame. When set to 1, the current frame is an interframe. Defined in [RFC6386]
        if ((ptr[0] & 0x01) == 0) {// PID == 0 mean keyframe
            log_infof("vp8 get keyframe");
            key_frame = true;
        }
        // new frame begin
        output_packet();
    }

    if (key_frame) {
        buffer_pkt_ptr_->is_key_frame_ = true;
    }
    buffer_pkt_ptr_->dts_ = pkt_ptr->pkt->get_timestamp();
    buffer_pkt_ptr_->pts_ = pkt_ptr->pkt->get_timestamp();
    buffer_pkt_ptr_->av_type_    = MEDIA_VIDEO_TYPE;
    buffer_pkt_ptr_->codec_type_ = MEDIA_CODEC_VP8;
    buffer_pkt_ptr_->fmt_type_   = MEDIA_FORMAT_RAW;

    buffer_pkt_ptr_->buffer_ptr_->append_data((const char*)ptr, pend - ptr);

    if (pkt_ptr->pkt->get_marker()) {
        output_packet();
    }
    return;
}

void pack_handle_vp8::output_packet() {
    if (buffer_pkt_ptr_->buffer_ptr_->data_len() > 0) {
        cb_->media_packet_output(buffer_pkt_ptr_);
    }
    clear_packet_buffer();
}

void pack_handle_vp8::on_timer() {

    return;
}

void pack_handle_vp8::clear_packet_buffer() {
    buffer_pkt_ptr_->buffer_ptr_->reset();
    buffer_pkt_ptr_->dts_ = 0;
    buffer_pkt_ptr_->pts_ = 0;
    buffer_pkt_ptr_->is_key_frame_ = false;
    buffer_pkt_ptr_->is_seq_hdr_   = false;
    return;
}