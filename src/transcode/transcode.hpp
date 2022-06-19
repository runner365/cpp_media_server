#ifndef TRANSCODE_H
#define TRANSCODE_H

#include "utils/data_buffer.hpp"
#include "utils/av/media_packet.hpp"
#include "transcode_pub.hpp"
#include "decode.hpp"
#include "encode.hpp"
#include <memory>
#include <queue>
#include <thread>
#include <mutex>

class transcode : public decode_callback, public encode_callback
{
    class rtp_packet_info
    {
    public:
        AVPacket* pkt = nullptr;
        MEDIA_CODEC_TYPE codec_type = MEDIA_CODEC_UNKOWN;
    };
public:
    transcode();
    virtual ~transcode();

public:
    void set_output_audio_fmt(const std::string& fmt);
    void set_output_audio_samplerate(int samplerate);
    void set_output_format(MEDIA_FORMAT_TYPE format);
    void set_video_bitrate(int bitrate);
    void set_audio_bitrate(int bitrate);
    void start();
    void stop();
    int send_transcode(MEDIA_PACKET_PTR pkt_ptr, int pos = 0);
    MEDIA_PACKET_PTR recv_transcode();

public:
    virtual void on_avframe_callback(int stream_index, AVFrame* frame) override;

public:
    virtual void on_avpacket_callback(AVPacket* pkt, MEDIA_CODEC_TYPE codec_type,
                                      uint8_t* extra_data, size_t extra_data_size) override;

private:
    MEDIA_PACKET_PTR get_media_packet(AVPacket* pkt, MEDIA_CODEC_TYPE codec_type);

private:
    void insert_send_queue(rtp_packet_info info);
    AVPacket* get_send_queue(MEDIA_CODEC_TYPE& codec_type);
    void on_work();
    void insert_recv_queue(MEDIA_PACKET_PTR pkt_ptr);
    MEDIA_PACKET_PTR get_recv_queue();
    void clear();
    
private:
    bool run_ = false;
    MEDIA_FORMAT_TYPE output_format_ = MEDIA_FORMAT_RAW;
    std::string output_audio_fmt_;
    int audio_samplerate_ = 48000;
    int video_bitrate_ = 1000;//1000kbps
    int audio_bitrate_ = 64;//64kbps
    std::shared_ptr<std::thread> thread_ptr_;
    std::mutex send_mutex_;
    std::mutex recv_mutex_;
    std::queue<rtp_packet_info> send_queue_;
    std::queue<MEDIA_PACKET_PTR> recv_queue_;
    
private:
    decode_oper* dec_ = nullptr;
    encode_base* venc_ = nullptr;
    encode_base* aenc_ = nullptr;
    bool first_audio_ = true;
    bool first_video_ = true;
};


#endif
