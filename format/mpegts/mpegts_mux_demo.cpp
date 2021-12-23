#include "mpegts_mux.hpp"
#include "format/flv/flv_pub.hpp"
#include "format/flv/flv_demux.hpp"
#include "format/h264_header.hpp"
#include "logger.hpp"
#include <string>
#include <memory>

std::string flv_filename;
std::string ts_filename;
muxer_callback* muxer_cb_p = nullptr;

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
        }
        return 0;
    }

private:
    int handle_video(MEDIA_PACKET_PTR pkt_ptr) {
        std::vector<data_buffer> nalus;

        bool ret = AnnexbToNalus((uint8_t*)pkt_ptr->buffer_ptr_->data(),
                        pkt_ptr->buffer_ptr_->data_len(), nalus);
        if (!ret) {
            return -1;
        }

        for (data_buffer item : nalus) {
            MEDIA_PACKET_PTR nalu_pkt_ptr = std::make_shared<MEDIA_PACKET>();
            uint8_t* data = (uint8_t*)item.data();
            size_t data_len = (size_t)item.data_len();

            nalu_pkt_ptr->av_type_ = pkt_ptr->av_type_;
            
            uint8_t nalu_type = data[4];

            if (H264_IS_PPS(nalu_type)) {
                memcpy(pps_, data, data_len);
            } else if (H264_IS_SPS(nalu_type)) {
                memcpy(sps_, data, data_len);
            } else if (H264_IS_KEYFRAME(nalu_type) || H264_IS_AUD(nalu_type)) {
                
            } else {
                //
            }

        }
        //muxer_.input_packet(pkt_ptr);
        return 0;
    }
public:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override {

        return 0;
    }

private:
    std::string filename_;
    mpegts_mux muxer_;
    uint8_t pps_[1024];
    uint8_t sps_[1024];
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