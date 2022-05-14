#ifndef MPEGTS_HANDLE_HPP
#define MPEGTS_HANDLE_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <queue>
#include <list>
#include <stdio.h>
#include <vector>
#include "format/mpegts/mpegts_mux.hpp"
#include "media_packet.hpp"
#include "stringex.hpp"
#include "session_aliver.hpp"
#include "utils/logger.hpp"

class ts_item_info
{
public:
    ts_item_info()
    {
    }
    ~ts_item_info()
    {
    }

    void set_ts_filename(const std::string& filename) {
        ts_filename = filename;

        std::vector<std::string> output_vec;
        string_split(ts_filename, "/", output_vec);
        if (output_vec.size() >= 2) {
            ts_key += output_vec[output_vec.size() - 2];
            ts_key += "/";
            ts_key += output_vec[output_vec.size() - 1];
        } else {
            ts_key = filename;
        }
    }
    
    void write(int64_t dts, uint8_t* data, size_t data_len, const std::string& filename) {
        if (start_dts <= 0) {
            start_dts = dts;
        }
        end_dts = dts;

        if (end_dts > start_dts) {
            duration = end_dts - start_dts;
        }

        if (!filename.empty()) {
            FILE* file_p = fopen(filename.c_str(), "ab+");
            if (file_p) {
                fwrite(data, data_len, 1, file_p);
                fclose(file_p);
            }
        }

        return;
    }

    void reset() {
        start_dts = -1;
        end_dts   = -1;
        duration  = -1;
    }

public:
    int64_t start_dts = -1;
    int64_t end_dts = -1;
    int64_t duration = -1;
    std::string ts_filename;
    std::string ts_key;
};

class mpegts_handle : public av_format_callback, public session_aliver
{
public:
    mpegts_handle(const std::string& app, const std::string& streamname, const std::string& path, bool rec_enable);
    virtual ~mpegts_handle();

public:
    void handle_media_packet(MEDIA_PACKET_PTR pkt_ptr);
    bool gen_live_m3u8(std::string& m3u8_header);
    std::shared_ptr<ts_item_info> get_mpegts_item(const std::string& ts_name);
    void set_ts_list_max(size_t list_max);
    void set_ts_duration(size_t duration);
    void flush();

protected:
    virtual int output_packet(MEDIA_PACKET_PTR pkt_ptr) override;

private:
    void handle_packet(MEDIA_PACKET_PTR pkt_ptr);
    int handle_video_h264(MEDIA_PACKET_PTR pkt_ptr);
    int handle_video_h265(MEDIA_PACKET_PTR pkt_ptr);
    int handle_video_vp8(MEDIA_PACKET_PTR pkt_ptr);

    int handle_audio_aac(MEDIA_PACKET_PTR pkt_ptr);
    int handle_audio_opus(MEDIA_PACKET_PTR pkt_ptr);

private:
    void write_live_m3u8();
    void write_record_m3u8();

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
    uint8_t vps_[1024];
    uint8_t pps_[1024];
    uint8_t sps_[1024];
    size_t  vps_len_ = 0;
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
    bool record_init_  = false;
    int64_t last_patpmt_ts_ = -1;
    std::shared_ptr<ts_item_info> ts_info_ptr_;
    std::list<std::shared_ptr<ts_item_info>> ts_list_;
    size_t ts_list_max_ = 3;
    size_t mpegts_duration_ = 4;
    MEDIA_PACKET_PTR opus_seq_;
    std::string m3u8_header_;

};

#endif