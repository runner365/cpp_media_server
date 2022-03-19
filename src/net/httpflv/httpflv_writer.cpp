#include "httpflv_writer.hpp"
#include "flv_pub.hpp"
#include "byte_stream.hpp"

httpflv_writer::httpflv_writer(std::string key, std::string id,
    std::shared_ptr<http_response> resp, bool has_video, bool has_audio):resp_(resp)
    , key_(key)
    , writer_id_(id)
    , has_video_(has_video)
    , has_audio_(has_audio)
{

}

httpflv_writer::~httpflv_writer()
{
    close_writer();
}

int httpflv_writer::send_flv_header() {
    /*|'F'(8)|'L'(8)|'V'(8)|version(8)|TypeFlagsReserved(5)|TypeFlagsAudio(1)|TypeFlagsReserved(1)|TypeFlagsVideo(1)|DataOffset(32)|PreviousTagSize(32)|*/
    uint8_t flag = 0;

    if (flv_header_ready_) {
        return 0;
    }

    if (!has_audio_ && !has_video_) {
        return -1;
    }
    if (has_video_) {
        flag |= 0x01;
    }
    if (has_audio_) {
        flag |= 0x04;
    }

    uint8_t flv_header[] = {0x46, 0x4c, 0x56, 0x01, flag, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00};

    resp_->write((char*)flv_header, sizeof(flv_header), true);

    flv_header_ready_ = true;
    return 0;
}

int httpflv_writer::write_packet(MEDIA_PACKET_PTR pkt_ptr) {
    int ret = 0;
    uint8_t flv_header[11];
    uint8_t pre_size_data[4];
    uint32_t pre_size = 0;

    ret = send_flv_header();
    if (ret != 0) {
        return 0;
    }

    /*|Tagtype(8)|DataSize(24)|Timestamp(24)|TimestampExtended(8)|StreamID(24)|Data(...)|PreviousTagSize(32)|*/
    if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
        flv_header[0] = FLV_TAG_VIDEO;
    } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
        flv_header[0] = FLV_TAG_AUDIO;
    } else {
        log_warnf("httpflv writer does not suport av type:%d", pkt_ptr->av_type_);
        return 0;
    }
    uint32_t payload_size = pkt_ptr->buffer_ptr_->data_len();
    uint32_t timestamp_base = (uint32_t)(pkt_ptr->dts_ & 0xffffff);
    uint8_t timestamp_ext = (uint8_t)((pkt_ptr->dts_ >> 24) & 0xff);

    write_3bytes(flv_header + 1, payload_size);
    if (timestamp_base >= 0xffffff) {
        write_3bytes(flv_header + 4, 0xffffff);
    } else {
        write_3bytes(flv_header + 4, timestamp_base);
    }
    flv_header[7] = timestamp_ext;
    //Set StreamID(24) as 1
    flv_header[8] = 0;
    flv_header[9] = 0;
    flv_header[10] = 1;

    ret = resp_->write((char*)flv_header, sizeof(flv_header), true);
    if (ret < 0) {
        return ret;
    }

    ret = resp_->write(pkt_ptr->buffer_ptr_->data(), pkt_ptr->buffer_ptr_->data_len(), true);
    if (ret < 0) {
        return ret;
    }

    pre_size = sizeof(flv_header) + pkt_ptr->buffer_ptr_->data_len();
    write_4bytes(pre_size_data, pre_size);
    ret = resp_->write((char*)pre_size_data, sizeof(pre_size_data), true);
    if (ret < 0) {
        return ret;
    }

    keep_alive();
    return 0;
}

std::string httpflv_writer::get_key() {
    return key_;
}

std::string httpflv_writer::get_writerid() {
    return writer_id_;
}

void httpflv_writer::close_writer() {
    if (closed_flag_) {
        return;
    }
    closed_flag_ = true;
    resp_->close();
}

bool httpflv_writer::is_inited() {
    return init_flag_;
}

void httpflv_writer::set_init_flag(bool flag) {
    init_flag_ = flag;
}