#include "sdp_analyze.hpp"
#include "logger.hpp"

sdp_analyze::sdp_analyze() {

}

sdp_analyze::~sdp_analyze() {

}


json sdp_analyze::parse(const std::string& sdp_str) {
    sdp_json_ = sdptransform::parse(sdp_str);

    log_infof("receive publish sdp json:%s", sdp_json_.dump().c_str());

    return sdp_json_;
}

int sdp_analyze::encode(std::stringstream& sdp_io) {

}