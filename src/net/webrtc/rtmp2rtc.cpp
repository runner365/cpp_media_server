#include "rtmp2rtc.hpp"
#include "room_service.hpp"
#include "utils/av/media_packet.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"


rtmp2rtc_writer::rtmp2rtc_writer() {

}

rtmp2rtc_writer::~rtmp2rtc_writer() {

}

int rtmp2rtc_writer::write_packet(MEDIA_PACKET_PTR pkt_ptr) {
    if (!Config::webrtc_is_enable()) {
        return 0;
    }
    if (pkt_ptr->app_.empty() || pkt_ptr->streamname_.empty()) {
        return 0;
    }
    if (room_has_rtc_uid(pkt_ptr->app_, pkt_ptr->streamname_)) {
        return 0;
    }
    std::shared_ptr<room_service> room_service_ptr = GetorCreate_room_service(pkt_ptr->app_);
    if (!room_service_ptr) {
        return 0;
    }

    room_service_ptr->rtmp_stream_ingest(pkt_ptr);
    return 0;
}

std::string rtmp2rtc_writer::get_key() {
    //ignore
    return "";
}

std::string rtmp2rtc_writer::get_writerid() {
    //ignore
    return "";
}
void rtmp2rtc_writer::close_writer() {
    //ignore
    return;
}

bool rtmp2rtc_writer::is_inited() {
    //ignore
    return true;
}

void rtmp2rtc_writer::set_init_flag(bool flag) {
    //ignore
    return;
}
