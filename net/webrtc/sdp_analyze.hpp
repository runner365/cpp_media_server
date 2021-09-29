#ifndef SDP_ANALYZE_HPP
#define SDP_ANALYZE_HPP
#include "sdptransform.hpp"
#include <string>
#include <sstream>

class sdp_analyze
{
public:
    sdp_analyze();
    ~sdp_analyze();

public:
    json parse(const std::string& sdp_str);
    std::string encode(json& sdp_json);

    json get_sdpjson() {return sdp_json_;}
    std::string get_sdp_string() {return sdp_;}

private:
    json sdp_json_;
    std::string sdp_;
};
#endif //SDP_ANALYZE_HPP