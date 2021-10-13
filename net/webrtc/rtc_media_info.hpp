#ifndef RTC_MEDIA_INFO_HPP
#define RTC_MEDIA_INFO_HPP
#include "sdptransform.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <vector>

#define SEND_ONLY 0
#define RECV_ONLY 1
#define SEND_RECV 2

typedef struct FingerPrint_S
{
    std::string hash;//eg: "16:B0:B4:16:1F:83:D9:CA:C6:7E:D5:08:3F:D6:67:7E:BB:0E:DB:4C:44:B3:CD:60:B3:D7:67:6E:CA:5D:70:3A"
    std::string type;//eg: "sha-256"
} FINGER_PRINT;

typedef struct ICE_INFO_S
{
    std::string ice_options;
    std::string ice_pwd;
    std::string ice_ufrag;
} ICE_INFO;

typedef struct HEADER_EXT_S
{
    std::string uri;
    int value;
} HEADER_EXT;

typedef struct RTCP_FB_S
{
    int payload;
    std::string type;//eg: "transport-cc", "goog-remb", "nack"
    std::string subtype;//eg: "fir", "pli"
} RTCP_FB;

typedef struct RTP_ENCODING_S
{
    std::string codec;
    int payload;
    int clock_rate;
    std::string encoding;//only for "opus", eg: "2"
} RTP_ENCODING;

typedef struct SSRC_INFO_S
{
    std::string attribute;
    std::string value;
    uint32_t    ssrc;
} SSRC_INFO;

typedef struct FMTP_S
{
    std::string config;//eg: "apt=96", "profile-id=0", "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42001f"
    int payload;
} FMTP;

typedef struct CONNECT_INFO_S
{
    std::string ip;
    int version;
} CONNECT_INFO;

typedef struct RTCP_BASIC_S
{
    std::string address;//"0.0.0.0"
    int ip_ver;//4
    std::string net_type;//"IN"
    int port;//as same as MEDIA_RTC_INFO::port
} RTCP_BASIC;

typedef struct SSRC_GROUPS_S
{
    std::string semantics;//FID
    std::vector<uint32_t> ssrcs;
} SSRC_GROUPS;

typedef struct BASIC_GROUP_S
{
    std::vector<int> mids;
    std::string type;//"BUNDLE"
} BASIC_GROUP;

typedef struct MSID_SEMANTIC_S
{
    std::string token;//"WMS"
} MSID_SEMANTIC;

typedef struct ORIGIN_S
{
    std::string address;//"127.0.0.1"
    int ip_ver;//4
    std::string net_type;//"IN"
    uint64_t session_id;//5281112952342324716
    int session_version;//2
    std::string user_name;//"-"
} ORIGIN;

typedef struct TIMING_BASIC_S
{
    int start;
    int stop;
} TIMING_BASIC;

typedef struct CANDIDATE_INFO_S
{
    std::string foundation;//"0"
    int component;//1
    std::string transport;//"udp" or "tcp"
    int priority;
    std::string ip;//server host ip
    int port;//server host port
    std::string type;//"host", "srflx"
} CANDIDATE_INFO;

typedef struct MEDIA_RTC_INFO_S
{
    int mid;
    std::string msid;
    std::string media_type;
    int port;
    std::string rtcp_mux;//"rtcp-mux"
    std::string rtcp_rsize;//"rtcp-rsize"
    std::string setup;//"actpass"

    RTCP_BASIC rtcp;
    std::vector<SSRC_GROUPS> ssrc_groups;
    std::string protocol;//"UDP/TLS/RTP/SAVPF"
    std::vector<int> payloads;
    std::vector<HEADER_EXT> header_extentions;
    std::vector<RTCP_FB> rtcp_fbs;
    std::vector<RTP_ENCODING> rtp_encodings;
    std::vector<SSRC_INFO> ssrc_infos;
    std::vector<FMTP> fmtps;
} MEDIA_RTC_INFO;

class rtc_media_info
{
public:
    rtc_media_info();
    ~rtc_media_info();

    int parse(json& sdp_json);
    std::string dump();
    void filter_payloads();

private:
    void get_basic_info(json& info_json);

private:
    void get_connection_info(json& info_json, CONNECT_INFO& connect);
    void get_rtcp_basic(json& info_json, RTCP_BASIC& rtcp);
    void get_ssrc_groups(json& info_json, std::vector<SSRC_GROUPS>& groups);
    int get_direction_type(json& info_json);
    FINGER_PRINT get_finger_print(json& info_json);
    ICE_INFO get_ice(json& info_json);
    void get_header_extersion(json& info_json, std::vector<HEADER_EXT>& ext_vec);
    void get_payloads(json& info_json, std::vector<int>& payload_vec);
    void get_rtcpfb(json& info_json, std::vector<RTCP_FB>& rtcp_fbs);
    void get_rtp_encodings(json& info_json, std::vector<RTP_ENCODING>& rtp_encodings);
    void get_ssrcs_info(json& info_json, std::vector<SSRC_INFO>& ssrc_infos);
    void get_fmtps(json& info_json, std::vector<FMTP>& fmtps);

public:
    int version;
    std::string extmap_allow_mixed;//"extmap-allow-mixed"
    std::string name;//"-"
    ORIGIN origin;
    std::vector<BASIC_GROUP> groups;
    MSID_SEMANTIC msid_semantic;
    TIMING_BASIC timing;
    std::vector<CANDIDATE_INFO> candidates;

public://for media party
    int direction_type;//SEND_ONLY, RECV_ONLY, SEND_RECV
    CONNECT_INFO connect_info;
    FINGER_PRINT finger_print;
    ICE_INFO ice;
    std::vector<MEDIA_RTC_INFO> medias;
};


#endif