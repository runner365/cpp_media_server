#include "sdp_analyze.hpp"
#include "logger.hpp"

sdp_analyze::sdp_analyze() {

}

sdp_analyze::~sdp_analyze() {

}

json sdp_analyze::parse(const std::string& sdp_str) {
    sdp_json_ = sdptransform::parse(sdp_str);

    return sdp_json_;
}

std::string sdp_analyze::encode(json& sdp_json) {
    std::string sdp_str = sdptransform::write(sdp_json);
    return sdp_str;
}