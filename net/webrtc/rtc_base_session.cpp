#include "rtc_base_session.hpp"
#include "net/udp/udp_server.hpp"
#include "utils/logger.hpp"

rtc_base_session::rtc_base_session(int session_direction):direction_(session_direction) {
    log_infof("rtc_base_session construct direction:%d", direction_);
}
    
rtc_base_session::~rtc_base_session() {

}