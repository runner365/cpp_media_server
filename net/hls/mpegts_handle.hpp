#ifndef MPEGTS_HANDLE_HPP
#define MPEGTS_HANDLE_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <queue>
#include <list>
#include <stdio.h>
#include "format/mpegts/mpegts_mux.hpp"
#include "media_packet.hpp"

class ts_item_info
{
public:
    ts_item_info():ts_buffer(5*1024*1024)
    {
    }
    ~ts_item_info()
    {
    }

    void write(int64_t dts, uint8_t* data, size_t data_len, const std::string& filename) {
        if (start_dts <= 0) {
            start_dts = dts;
        }
        end_dts = dts;

        if (end_dts > start_dts) {
            duration = end_dts - start_dts;
        }
        ts_buffer.append_data((char*)data, data_len);

        if (!filename.empty()) {
            FILE* file_p = fopen(filename.c_str(), "ab+");
            if (file_p) {
                fwrite(data, data_len, 1, file_p);
                fclose(file_p);
            }
        }
        ts_filename = filename;
        return;
    }

    void reset() {
        start_dts = -1;
        end_dts   = -1;
        duration  = -1;
        ts_buffer.reset();
    }

public:
    int64_t start_dts = -1;
    int64_t end_dts = -1;
    int64_t duration = -1;
    data_buffer ts_buffer;
    std::string ts_filename;
};

class mpegts_handle : public av_format_callback
{
public:
    mpegts_handle(const std::string& app, const std::string& streamname, const std::string& path, bool rec_enable);
    virtual ~mpegts_handle();

public:
    void handle_media_packet(MEDIA_PACKET_PTR pkt_ptr);
    bool gen_live_m3u8(std::string& m3u8_header);

protected:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override;

private:
    void handle_packet(MEDIA_PACKET_PTR pkt_ptr);
    int handle_video_h264(MEDIA_PACKET_PTR pkt_ptr);

    int handle_audio_aac(MEDIA_PACKET_PTR pkt_ptr);
    int handle_audio_opus(MEDIA_PACKET_PTR pkt_ptr);

private:
    bool rec_enable_ = false;
    std::string path_;
    std::string app_;
    std::string streamname_;
    std::string live_m3u8_filename_;
    std::string rec_m3u8_filename_;
    std::string ts_filename_;

private:
    mpegts_mux muxer_;
    std::queue<MEDIA_PACKET_PTR> wait_queue_;
    bool video_ready_ = false;
    bool audio_ready_ = false;
    bool ready_       = false;

private:
    uint8_t pps_[1024];
    uint8_t sps_[1024];
    size_t  pps_len_ = 0;
    size_t  sps_len_ = 0;

    uint8_t aac_type_ = 0;
    int sample_rate_  = 0;
    uint8_t channel_  = 0;

private:
    int64_t seq_ = 0;
    int64_t last_ts_ = -1;
    bool audio_first_flag_ = false;
    bool pat_pmt_flag_ = false;
    int64_t last_patpmt_ts_ = -1;
    std::shared_ptr<ts_item_info> ts_info_ptr_;
    std::list<std::shared_ptr<ts_item_info>> ts_list_;
    MEDIA_PACKET_PTR opus_seq_;
    std::string m3u8_header_;
};

#endif