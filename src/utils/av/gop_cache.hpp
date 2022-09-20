#ifndef GOP_CACHE_HPP
#define GOP_CACHE_HPP
#include "media_packet.hpp"
#include <list>

class gop_cache
{
public:
    gop_cache(uint32_t min_gop = 1);
    ~gop_cache();

    void insert_packet(MEDIA_PACKET_PTR pkt_ptr);
    int writer_gop(av_writer_base* writer_p);

private:
    std::list<MEDIA_PACKET_PTR> packet_list_;
    MEDIA_PACKET_PTR video_hdr_;
    MEDIA_PACKET_PTR audio_hdr_;
    MEDIA_PACKET_PTR metadata_hdr_;
    uint32_t min_gop_ = 1;
    uint32_t gop_count_ = 0;
};
#endif