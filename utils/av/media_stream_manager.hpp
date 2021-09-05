#ifndef RTMP_MEDIA_STREAM_HPP
#define RTMP_MEDIA_STREAM_HPP
#include "media_packet.hpp"
#include "gop_cache.hpp"
#include <stdint.h>
#include <unordered_map>
#include <string>
#include <memory>
#include <list>

class rtmp_server_session;
class rtmp_request;

typedef std::unordered_map<std::string, av_writer_base*> WRITER_MAP;

class media_stream
{
public:
    std::string stream_key_;//app/streamname
    bool publisher_exist_ = false;
    gop_cache cache_;
    WRITER_MAP writer_map_;//(session_key, av_writer_base*)
};

typedef std::shared_ptr<media_stream> MEDIA_STREAM_PTR;

class media_stream_manager
{
public:
    static int add_player(av_writer_base* writer_p);
    static void remove_player(av_writer_base* writer_p);

    static MEDIA_STREAM_PTR add_publisher(const std::string& stream_key);
    static void remove_publisher(const std::string& stream_key);

public:
    static int writer_media_packet(MEDIA_PACKET_PTR pkt_ptr);

private:
    static std::unordered_map<std::string, MEDIA_STREAM_PTR> media_streams_map_;//key("app/stream"), MEDIA_STREAM_PTR

};

#endif //RTMP_MEDIA_STREAM_HPP