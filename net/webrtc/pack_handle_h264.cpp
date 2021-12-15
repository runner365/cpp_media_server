#include "pack_handle_h264.hpp"
#include "utils/av/media_packet.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "timeex.hpp"

static const uint8_t NAL_START_CODE[4] = {0, 0, 0, 1};
static const size_t H264_STAPA_FIELD_SIZE = 2;

pack_handle_h264::pack_handle_h264(pack_callbackI* cb, boost::asio::io_context& io_ctx):timer_interface(io_ctx, 100)
                                                    , cb_(cb)
{

}

pack_handle_h264::~pack_handle_h264() {

}

void pack_handle_h264::get_startend_bit(rtp_packet* pkt, bool& start, bool& end) {
    uint8_t* payload_data = pkt->get_payload();
    uint8_t fu_header = payload_data[1];

    start = false;
    end   = false;

    if ((fu_header & 0x80) != 0) {
        start = true;
    }

    if ((fu_header & 0x40) != 0) {
        end = true;
    }

    return;
}

void pack_handle_h264::on_timer() {
    check_fua_timeout();
}

void pack_handle_h264::input_rtp_packet(std::shared_ptr<rtp_packet_info> pkt_ptr) {
    if (!init_flag_) {
        init_flag_ = true;
        last_extend_seq_ = pkt_ptr->extend_seq_;
    } else {
        if ((last_extend_seq_ + 1) != pkt_ptr->extend_seq_) {
            packets_queue_.clear();
            start_flag_ = false;
            end_flag_   = false;

            cb_->pack_handle_reset(pkt_ptr);
            last_extend_seq_ = pkt_ptr->extend_seq_;
            return;
        }
        last_extend_seq_ = pkt_ptr->extend_seq_;
    }

    uint8_t* payload_data = pkt_ptr->pkt->get_payload();
    uint8_t nal_type = payload_data[0] & 0x1f;

    if ((nal_type >= 1) && (nal_type <= 23)) {//single nalu
        int64_t dts = (int64_t)pkt_ptr->pkt->get_timestamp();
        dts = dts * 1000 / (int64_t)pkt_ptr->clock_rate_;

        MEDIA_PACKET_PTR h264_pkt_ptr = std::make_shared<MEDIA_PACKET>();

        h264_pkt_ptr->buffer_ptr_->append_data((char*)NAL_START_CODE, sizeof(NAL_START_CODE));
        h264_pkt_ptr->buffer_ptr_->append_data((char*)payload_data, pkt_ptr->pkt->get_payload_length());

        h264_pkt_ptr->av_type_    = MEDIA_VIDEO_TYPE;
        h264_pkt_ptr->codec_type_ = MEDIA_CODEC_H264;
        h264_pkt_ptr->fmt_type_   = MEDIA_FORMAT_RAW;
        h264_pkt_ptr->dts_        = dts;
        h264_pkt_ptr->pts_        = dts;

        cb_->media_packet_output(h264_pkt_ptr);
        return;
    } else if (nal_type == 28) {//rtp fua
        bool start = false;
        bool end   = false;

        get_startend_bit(pkt_ptr->pkt, start, end);

        if (start && !end) {
            packets_queue_.clear();
            start_flag_ = start;
        } else if (start && end) {//exception happened
            log_errorf("rtp h264 pack error: both start and end flag are enable");
            reset_rtp_fua();
            return;
        }

        if (end && !start_flag_) {
            log_errorf("rtp h264 pack error: get end rtp packet but there is no start rtp packet");
            reset_rtp_fua();
            return;
        }

        if (end) {
            end_flag_ = true;
        }

        packets_queue_.push_back(pkt_ptr);

        if (start_flag_ && end_flag_) {
            MEDIA_PACKET_PTR h264_pkt_ptr = std::make_shared<MEDIA_PACKET>();
            int64_t dts = 0;
            bool ok = demux_fua(h264_pkt_ptr, dts);
            if (ok) {
                h264_pkt_ptr->av_type_    = MEDIA_VIDEO_TYPE;
                h264_pkt_ptr->codec_type_ = MEDIA_CODEC_H264;
                h264_pkt_ptr->fmt_type_   = MEDIA_FORMAT_RAW;
                h264_pkt_ptr->dts_        = dts;
                h264_pkt_ptr->pts_        = dts;
                cb_->media_packet_output(h264_pkt_ptr);
            }
            start_flag_ = false;
            end_flag_   = false;
            return;
        }

        check_fua_timeout();
        return;
    } else if (nal_type == 24) {//handle stapA
        bool ret = demux_stapA(pkt_ptr);
        if (!ret) {
            cb_->pack_handle_reset(pkt_ptr);
        }
    }

    return;
}

void pack_handle_h264::check_fua_timeout() {
    size_t queue_len = packets_queue_.size();
    int64_t now_ms = now_millisec();
    for (size_t index = 0; index < queue_len; index++) {
        std::shared_ptr<rtp_packet_info> pkt_ptr = packets_queue_.front();
        int64_t pkt_local_ms = pkt_ptr->pkt->get_local_ms();
        if ((now_ms - pkt_local_ms) < PACK_BUFFER_TIMEOUT) {
            break;
        }
        packets_queue_.pop_front();
        log_warnf("h264 packet pop seq:%d", pkt_ptr->pkt->get_seq());
    }
    return;
}

bool pack_handle_h264::demux_stapA(std::shared_ptr<rtp_packet_info> pkt_ptr) {
    uint8_t* payload_data = pkt_ptr->pkt->get_payload();
    size_t payload_length = pkt_ptr->pkt->get_payload_length();
    std::vector<size_t> offsets;

    if (payload_length <= (sizeof(uint8_t) + H264_STAPA_FIELD_SIZE)) {
        log_errorf("demux stapA error: payload length(%lu) is too short", payload_length);
        return false;
    }

    bool ret = parse_stapA_offsets(payload_data, payload_length, offsets);
    if (!ret) {
        return ret;
    }
    int64_t dts = (int64_t)pkt_ptr->pkt->get_timestamp();
    dts = dts * 1000 / (int64_t)pkt_ptr->clock_rate_;

    offsets.push_back(payload_length + H264_STAPA_FIELD_SIZE);//end offset.
    for (size_t index = 0; index < (offsets.size() - 1); index++) {
        size_t start_offset = offsets[index];
        size_t end_offset = offsets[index + 1] - H264_STAPA_FIELD_SIZE;
        if ((end_offset - start_offset) < sizeof(uint8_t)) {
            log_errorf("demux stapA error: start offset:%lu, end offset:%lu",
                start_offset,  end_offset);
            return false;
        }
        MEDIA_PACKET_PTR h264_pkt_ptr = std::make_shared<MEDIA_PACKET>();

        h264_pkt_ptr->buffer_ptr_->append_data((char*)NAL_START_CODE, sizeof(NAL_START_CODE));
        h264_pkt_ptr->buffer_ptr_->append_data((char*)payload_data + start_offset, end_offset - start_offset);
        h264_pkt_ptr->av_type_    = MEDIA_VIDEO_TYPE;
        h264_pkt_ptr->codec_type_ = MEDIA_CODEC_H264;
        h264_pkt_ptr->fmt_type_   = MEDIA_FORMAT_RAW;
        h264_pkt_ptr->dts_        = dts;
        h264_pkt_ptr->pts_        = dts;

        cb_->media_packet_output(h264_pkt_ptr);
    }
    return true;
}

bool pack_handle_h264::parse_stapA_offsets(const uint8_t* data, size_t data_len, std::vector<size_t> &offsets) {
    
    size_t offset = 1;
    size_t left_len = data_len;
    const uint8_t* p = data + offset;
    left_len -= offset;

    while (left_len > 0) {
        if (left_len < sizeof(uint16_t)) {
            log_errorf("h264 stapA nalu len error: left length(%lu) is not enough.", left_len);
            return false;
        }
        uint16_t nalu_len = read_2bytes(p);
        p += sizeof(uint16_t);
        left_len -= sizeof(uint16_t);
        if (nalu_len > left_len) {
            log_errorf("h264 stapA nalu len error: left length(%lu) is smaller than nalu length(%d).",
                    left_len, nalu_len);
            return false;
        }
        p += nalu_len;
        left_len -= nalu_len;
        offsets.push_back(offset + H264_STAPA_FIELD_SIZE);
        offset += H264_STAPA_FIELD_SIZE + nalu_len;
    }
    
    return true;
}

bool pack_handle_h264::demux_fua(MEDIA_PACKET_PTR h264_pkt_ptr, int64_t& timestamp) {
    size_t queue_len = packets_queue_.size();
    bool has_start = false;
    bool has_end   = false;

    std::shared_ptr<data_buffer> buffer_ptr = h264_pkt_ptr->buffer_ptr_;
    for (size_t index = 0; index < queue_len; index++) {
        bool start = false;
        bool end   = false;

        std::shared_ptr<rtp_packet_info> pkt_ptr = packets_queue_.front();
        packets_queue_.pop_front();

        timestamp = (int64_t)pkt_ptr->pkt->get_timestamp();
        timestamp = timestamp * 1000 / pkt_ptr->clock_rate_;
        get_startend_bit(pkt_ptr->pkt, start, end);

        uint8_t* payload   = pkt_ptr->pkt->get_payload();
        size_t payload_len = pkt_ptr->pkt->get_payload_length();
        if (index == 0) {
            if (start) {
                has_start = true;
                uint8_t fu_indicator = payload[0];
                uint8_t fu_header    = payload[1];
                uint8_t nalu_header  = (fu_indicator & 0xe0) | (fu_header & 0x1f);
                buffer_ptr->append_data((char*)NAL_START_CODE, sizeof(NAL_START_CODE));
                buffer_ptr->append_data((char*)&nalu_header, sizeof(nalu_header));
                buffer_ptr->append_data((char*)payload + 2, payload_len - 2);
            }
        } else {
            if (has_start) {
                buffer_ptr->append_data((char*)payload + 2, payload_len - 2);
            }
        }
        if (end) {
            has_end = true;
            break;
        }
    }
    if (!has_start || !has_end) {
        log_errorf("rtp h264 demux fua error: has start:%d, has end:%d", has_start, has_end);
        return false;
    }
    return true;
}
void pack_handle_h264::reset_rtp_fua() {
    start_flag_ = false;
    end_flag_   = false;
    packets_queue_.clear();
}


    