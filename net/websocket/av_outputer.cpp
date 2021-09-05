#include "av_outputer.hpp"
#include "media_stream_manager.hpp"
#include "flv_pub.hpp"
#include "flv_mux.hpp"
#include "logger.hpp"
#include <sstream>

int av_outputer::output_packet(MEDIA_PACKET_PTR pkt_ptr) {
    int ret = flv_muxer::add_flv_media_header(pkt_ptr);
    if (ret < 0) {
        return ret;
    }
    pkt_ptr->fmt_type_ = MEDIA_FORMAT_FLV;

    return media_stream_manager::writer_media_packet(pkt_ptr);
}
