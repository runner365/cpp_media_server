#include "mpegts_mux.hpp"
#include "format/flv/flv_pub.hpp"
#include "format/flv/flv_demux.hpp"
#include "format/h264_header.hpp"
#include "logger.hpp"
#include <string>
#include <memory>

class muxer_callback;

std::string flv_filename;
std::string ts_filename;
muxer_callback* muxer_cb_p = nullptr;

static const uint8_t H264_AUD_DATA[] = {0x00, 0x00, 0x00, 0x01, 0x09, 0xff};

class muxer_callback : public av_format_callback
{
public:
    muxer_callback(const std::string& filename):filename_(filename)
            , muxer_(this) {
    }
    virtual ~muxer_callback(){}

public:
    int insert_packet(MEDIA_PACKET_PTR pkt_ptr) {
        int ret = 0;
        if (pkt_ptr->av_type_ == MEDIA_VIDEO_TYPE) {
            ret = handle_video(pkt_ptr);
        } else if (pkt_ptr->av_type_ == MEDIA_AUDIO_TYPE) {
            ret = handle_audio(pkt_ptr);
        } else {
            log_errorf("packet av type(%d) is unkown", pkt_ptr->av_type_);
        }
        return 0;
    }

private:
    int handle_video(MEDIA_PACKET_PTR pkt_ptr) {
        if (pkt_ptr->is_seq_hdr_) {
            uint8_t* data   = (uint8_t*)pkt_ptr->buffer_ptr_->data();
            size_t data_len = pkt_ptr->buffer_ptr_->data_len();

            int ret = get_sps_pps_from_extradata(pps_, pps_len_, sps_, sps_len_, 
                                data, data_len);
            if (ret == 0) {
                log_info_data(sps_, sps_len_, "sps data");
                log_info_data(pps_, pps_len_, "pps data");
            }
            return 0;
        }

        std::vector<std::shared_ptr<data_buffer>> nalus;
        bool ret = annexb_to_nalus((uint8_t*)pkt_ptr->buffer_ptr_->data(),
                        pkt_ptr->buffer_ptr_->data_len(), nalus);
        if (!ret) {
            return -1;
        }

        MEDIA_PACKET_PTR nalu_pkt_ptr = std::make_shared<MEDIA_PACKET>();
        for (std::shared_ptr<data_buffer> item : nalus) {
            uint8_t* data = (uint8_t*)item->data();
            size_t data_len = (size_t)item->data_len();

            uint8_t nalu_type = data[4] & 0x1f;
            if (H264_IS_AUD(nalu_type)) {
                continue;
            }
            if (H264_IS_PPS(nalu_type)) {
                pps_len_ = data_len;
                memcpy(pps_, data, pps_len_);
                continue;
            }
            if (H264_IS_SPS(nalu_type)) {
                sps_len_ = data_len;
                memcpy(sps_, data, sps_len_);
                continue;
            }
            nalu_pkt_ptr->copy_properties(pkt_ptr);
            nalu_pkt_ptr->buffer_ptr_->reset();
            
            nalu_pkt_ptr->buffer_ptr_->append_data((char*)H264_AUD_DATA, sizeof(H264_AUD_DATA));
            if (H264_IS_KEYFRAME(nalu_type)) {
                nalu_pkt_ptr->buffer_ptr_->append_data((char*)sps_, sps_len_);
                nalu_pkt_ptr->buffer_ptr_->append_data((char*)pps_, pps_len_);
            }
            nalu_pkt_ptr->buffer_ptr_->append_data((char*)data, data_len);
            
            muxer_.input_packet(nalu_pkt_ptr);
        }
        
        return 0;
    }

    int handle_audio(MEDIA_PACKET_PTR pkt_ptr) {
        //log_infof("audio is_seq:%d, len:%lu", pkt_ptr->is_seq_hdr_, pkt_ptr->buffer_ptr_->data_len());
        //log_info_data((uint8_t*)pkt_ptr->buffer_ptr_->data(),
        //        pkt_ptr->buffer_ptr_->data_len(), "audio data");
        //muxer_.input_packet(pkt_ptr);
        return 0;
    }
public:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override {
        //log_info_data((uint8_t*)pkt_ptr->buffer_ptr_->data(),
        //            pkt_ptr->buffer_ptr_->data_len(),
        //            pkt_ptr->dump().c_str());
        FILE* file_p = fopen(ts_filename.c_str(), "ab+");
        if (file_p) {
            fwrite(pkt_ptr->buffer_ptr_->data(),
                1,
                pkt_ptr->buffer_ptr_->data_len(),
                file_p);
            fclose(file_p);
        }
        return 0;
    }

private:
    std::string filename_;
    mpegts_mux muxer_;
    uint8_t pps_[1024];
    uint8_t sps_[1024];
    size_t  pps_len_ = 0;
    size_t  sps_len_ = 0;
};

class demuxer_callback : public av_format_callback
{
public:
    demuxer_callback(muxer_callback* cb):output_cb_(cb) {
    }
    virtual ~demuxer_callback() {
    }

public:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override {
        return output_cb_->insert_packet(pkt_ptr);
    }

private:
    muxer_callback* output_cb_ = nullptr;
};

int main(int argn, char** argv) {
    if (argn < 2) {
        return -1;
    }

    Logger::get_instance()->set_filename("mpegts_mux.log");

    flv_filename.assign(argv[1]);
    ts_filename.assign(argv[2]);

    log_infof("input flv file:%s", flv_filename.c_str());

    FILE* fh_p = fopen(flv_filename.c_str(), "r");

    if (!fh_p) {
        log_errorf("fail to open flv filename:%s\r\n", flv_filename.c_str());
        return -1;
    }

    char buffer[2048];
    int n = 0;

    muxer_cb_p = new muxer_callback(ts_filename);
    
    demuxer_callback cb(muxer_cb_p);
    flv_demuxer demuxer(&cb);
    do
    {
        n = fread(buffer, 1, sizeof(buffer), fh_p);
        MEDIA_PACKET_PTR pkt_ptr = std::make_shared<MEDIA_PACKET>();
        pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;
        pkt_ptr->key_ = "live/1000";
        pkt_ptr->buffer_ptr_->append_data(buffer, n);

        demuxer.input_packet(pkt_ptr);
    } while (n > 0);

    fclose(fh_p);

    delete muxer_cb_p;
}