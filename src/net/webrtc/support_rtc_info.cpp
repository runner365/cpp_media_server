#include "rtc_media_info.hpp"
#include "logger.hpp"
#include <string>
#include <stdint.h>
#include <stddef.h>
#include <vector>
#include <sstream>

#ifndef _WIN32
static RTCP_FB support_rtcp_fb_list[] = {
    {
        .payload = 0,
        .type = "goog-remb",
        .subtype = ""
    },
    {
        .payload = 0,
        .type = "nack",
        .subtype = ""
    },
    {
        .payload = 0,
        .type = "rrtr",
        .subtype = ""
    },
    {
        .payload = 0,
        .type = "nack",
        .subtype = "pli"
    },
};

static HEADER_EXT support_header_ext_list[] = {
    {
        .uri = "urn:ietf:params:rtp-hdrext:sdes:mid",
        .value = 0
    },
    {
        .uri = "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
        .value = 0
    }
};

static RTP_ENCODING support_rtp_encoding_list[] = {
    {
        .codec = "VP8",
        .payload = 0,
        .clock_rate = 90000,
        .media_type = MEDIA_VIDEO_TYPE
    },
    {
        .codec = "H264",
        .payload = 0,
        .clock_rate = 90000,
        .media_type = MEDIA_VIDEO_TYPE
    },
    {
        .codec = "rtx",
        .payload = 0,
        .clock_rate = 90000,
        .media_type = MEDIA_UNKOWN_TYPE
    },
    {
        .codec = "opus",
        .payload = 0,
        .clock_rate = 48000,
        .media_type = MEDIA_AUDIO_TYPE
    }
};

static SSRC_INFO support_ssrc_info_list[] = {
    {
        .attribute = "cname",
        .value = "",
        .ssrc = 0
    }
};

static FMTP support_fmtp_list[] = {
    {
        .config  = "useinbandfec=1",
        .payload = 0
    },
    {
        .config  = "apt=",
        .payload = 0
    },
    {
        .config  = "x-google-start-bitrate",
        .payload = 0
    },
    {
        .config  = "profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1",
        .payload = 0
    },
    {
        .config  = "profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1",
        .payload = 0
    },
    {
        .config  = "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
        .payload = 0
    },
    {
        .config  = "level-asymmetry-allowed=1;profile-level-id=42e01f;packetization-mode=1",
        .payload = 0
    },
    {
        .config  = "packetization-mode=1;level-asymmetry-allowed=1;profile-level-id=42e01f",
        .payload = 0
    },
    {
        .config  = "packetization-mode=1;profile-level-id=42e01f;level-asymmetry-allowed=1",
        .payload = 0
    }
};
#else
static RTCP_FB support_rtcp_fb_list[] = {
	{
		0, //payload
		"goog-remb", //type
		"" //subtype
	},
	{
		0,
		"nack",
		""
	},
	{
		0,
		"rrtr",
		""
	},
	{
		0,
		"nack",
		"pli"
	},
};
static HEADER_EXT support_header_ext_list[] = {
	{
		"urn:ietf:params:rtp-hdrext:sdes:mid",  //uri
		 0		//value
	},
	{
		"http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
		 0
	}
};

static RTP_ENCODING support_rtp_encoding_list[] = {
	{
		"VP8",				//codec
		0,				//payload	
		90000,		//clock_rate
		"",				//encoding
		MEDIA_VIDEO_TYPE //media_type
	},
	{
		"H264",
		0,
		90000,
		"",
		MEDIA_VIDEO_TYPE
	},
	{
		"rtx",
		0,
		90000,
		"",
		MEDIA_UNKOWN_TYPE
	},
	{
		"opus",
		0,
		48000,
		"",
		MEDIA_AUDIO_TYPE
	}
};

static SSRC_INFO support_ssrc_info_list[] = {
	{
		"cname", //attribute
		"", //""
		0 //ssrc
	}
};

static FMTP support_fmtp_list[] = {
	{
		"useinbandfec=1", //config
		0	//payload
	},
	{
		"apt=",
		0
	},
	{
		"x-google-start-bitrate",
		0
	},
	{
		"profile-level-id=42e01f;level-asymmetry-allowed=1;packetization-mode=1",
		0
	},
	{
		"profile-level-id=42e01f;packetization-mode=1;level-asymmetry-allowed=1",
		0
	},
	{
		"level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f",
		0
	},
	{
		"level-asymmetry-allowed=1;profile-level-id=42e01f;packetization-mode=1",
		0
	},
	{
		"packetization-mode=1;level-asymmetry-allowed=1;profile-level-id=42e01f",
		0
	},
	{
		"packetization-mode=1;profile-level-id=42e01f;level-asymmetry-allowed=1",
		 0
	}
};
#endif

static int get_apt_payload(const std::string& apt) {
    const std::string APT = "apt=";
    size_t pos = apt.find(APT);
    if (pos == std::string::npos) {
        return 0;
    }

    std::string payload = apt.substr(APT.length());

    return atoi(payload.c_str());
}

static bool fmtp_has_h264profile(const std::vector<FMTP>& input_fmtps, int& payload, int& apt_payload) {
    bool found = false;

    for (auto input_fmtp : input_fmtps) {
        size_t pos = input_fmtp.config.find("profile-level-id=");
        if (pos != std::string::npos) {
            payload = input_fmtp.payload;
            found = true;
            break;
        }
    }

    if (!found) {
        return false;
    }

    for (auto input_fmtp : input_fmtps) {
        int current_apt_payload = get_apt_payload(input_fmtp.config);
        if (current_apt_payload == payload) {
            apt_payload = input_fmtp.payload;
            return true;
        }
    }
    return false;
}

static void get_support_fmtp(const std::vector<FMTP>& input_fmtps,
        std::vector<FMTP>& support_input_fmtps) {
    for (auto input_fmtp : input_fmtps) {
        bool found = false;
        for (size_t index = 0; index < sizeof(support_fmtp_list)/sizeof(FMTP); index++) {
            auto pos = input_fmtp.config.find(support_fmtp_list[index].config);
            if (pos != std::string::npos) {
                found = true;
                break;
            }
        }
        if (found) {
            support_input_fmtps.push_back(input_fmtp);
        }
    }
/*
    std::vector<int> payload_vec;
    for (auto fmtp : support_input_fmtps) {
        if (fmtp.config.find("apt=") != 0) {
            payload_vec.push_back(fmtp.payload);
        }
    }

    auto fmtp_iter = support_input_fmtps.begin();
    while(fmtp_iter != support_input_fmtps.end()) {
        auto pos = fmtp_iter->config.find("apt=");
        if (pos == 0) {
            std::string payload_str = fmtp_iter->config.substr(4);
            int apt_payload = std::stoi(payload_str);

            bool found = false;
            for (auto media_payload : payload_vec) {
                if (apt_payload == media_payload) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                fmtp_iter = support_input_fmtps.erase(fmtp_iter);
                continue;
            }
        }
        fmtp_iter++;
    }
*/
    return;
}

static void get_support_ssrc_info(const std::vector<SSRC_INFO>& input_ssrc_infos,
        std::vector<SSRC_INFO>& support_ssrc_infos) {
    for (auto input_ssrc_info : input_ssrc_infos) {
        bool found = false;
        for (size_t index = 0; index < sizeof(support_ssrc_info_list)/sizeof(SSRC_INFO); index++) {
            if (input_ssrc_info.attribute == support_ssrc_info_list[index].attribute) {
                found = true;
                break;
            }
        }
        if (found) {
            support_ssrc_infos.push_back(input_ssrc_info);
        }
    }
}

static void get_support_rtcp_fb(const std::vector<RTCP_FB>& input_rtcp_fbs,
        std::vector<RTCP_FB>& support_rtcp_fbs) {

    for (auto input_fb : input_rtcp_fbs) {
        bool found = false;
        for (size_t index = 0; index < sizeof(support_rtcp_fb_list)/sizeof(RTCP_FB); index++) {
            if (input_fb.type == support_rtcp_fb_list[index].type) {
                if (input_fb.subtype.empty()) {
                    found = true;
                    break;
                } else {
                    if (input_fb.subtype == support_rtcp_fb_list[index].subtype) {
                        found = true;
                        break;
                    }
                }
            }
        }
        if (found) {
            support_rtcp_fbs.push_back(input_fb);
        }
    }
}

static void get_support_header_ext(const std::vector<HEADER_EXT>& input_header_exts,
            std::vector<HEADER_EXT>& support_header_ext) {
    for (auto ext : input_header_exts) {
        bool found = false;
        for (size_t i = 0; i < sizeof(support_header_ext_list)/sizeof(HEADER_EXT); i++) {
            if (support_header_ext_list[i].uri == ext.uri) {
                found = true;
                break;
            }
        }

        if (found) {
            support_header_ext.push_back(ext);
        }
    }
    return;
}

static void get_support_rtp_encoding(const std::vector<RTP_ENCODING>& input_rtp_encodings,
            std::vector<RTP_ENCODING>& support_rtp_encodings) {
    for (auto enc : input_rtp_encodings) {
        for (size_t i = 0; i < sizeof(support_rtp_encoding_list)/sizeof(RTP_ENCODING); i++) {
            if ((support_rtp_encoding_list[i].codec == enc.codec) &&
                (support_rtp_encoding_list[i].clock_rate == enc.clock_rate)) {
                if (support_rtp_encoding_list[i].media_type == MEDIA_VIDEO_TYPE) {
                    enc.media_type = MEDIA_VIDEO_TYPE;
                    support_rtp_encodings.push_back(enc);
                } else if (support_rtp_encoding_list[i].media_type == MEDIA_AUDIO_TYPE) {
                    enc.media_type = MEDIA_AUDIO_TYPE;
                    support_rtp_encodings.push_back(enc);
                } else {
                    enc.media_type = MEDIA_UNKOWN_TYPE;
                    support_rtp_encodings.push_back(enc);
                }
            }
        }
    }

    /*
    auto iter = support_rtp_encodings.begin();
    while(iter != support_rtp_encodings.end()) {
        bool found = false;
        for (auto fmtp : support_input_fmtps) {
            if (fmtp.payload == iter->payload) {
                found = true;
                break;
            }
        }

        if (!found) {
            iter = support_rtp_encodings.erase(iter);
        } else {
            iter++;
        }
    }
    */
    return;
}

static void filter_fmts_and_encs_by_h264payload(std::vector<FMTP>& support_input_fmtps,
                            std::vector<RTP_ENCODING>& support_rtp_encodings,
                            int h264_payload, int h264_apt_payload) {
    for (std::vector<FMTP>::iterator iter = support_input_fmtps.begin();
        iter != support_input_fmtps.end();) {
        if ((iter->payload != h264_payload) && (iter->payload != h264_apt_payload)) {
            //log_infof("remove payload:%d frome fmtps", iter->payload);
            iter = support_input_fmtps.erase(iter);
        } else {
            iter++;
        }
    }

    for (std::vector<RTP_ENCODING>::iterator iter = support_rtp_encodings.begin();
        iter != support_rtp_encodings.end();) {
        if ((iter->payload != h264_payload) && (iter->payload != h264_apt_payload)) {
            //log_infof("remove payload:%d frome rtp encodings", iter->payload);
            iter = support_rtp_encodings.erase(iter);
        } else {
            iter++;
        }
    }
    return;
}

static bool filter_fmts_and_encs_by_vp8(std::vector<FMTP>& support_input_fmtps,
                            std::vector<RTP_ENCODING>& support_rtp_encodings) {
    int vp8_payload = 0;
    for (const auto& enc_item : support_rtp_encodings) {
        size_t pos = enc_item.codec.find("VP8");
        if (pos != std::string::npos) {
            vp8_payload = enc_item.payload;
            break;
        }
    }
    if (vp8_payload == 0) {
        return false;
    }

    int apt_payload = 0;
    for (const auto& fmtp_item : support_input_fmtps) {
        int config_payload = get_apt_payload(fmtp_item.config);
        if (config_payload == vp8_payload) {
            apt_payload = fmtp_item.payload;
            break;
        }
    }
    if (apt_payload == 0) {
        return false;
    }

    for (std::vector<FMTP>::iterator iter = support_input_fmtps.begin();
        iter != support_input_fmtps.end();) {
        if (iter->payload != apt_payload) {
            iter = support_input_fmtps.erase(iter);
        } else {
            iter++;
        }
    }

    for (std::vector<RTP_ENCODING>::iterator iter = support_rtp_encodings.begin();
        iter != support_rtp_encodings.end();) {
        if ((iter->payload != vp8_payload) && (iter->payload == apt_payload)) {
            iter = support_rtp_encodings.erase(iter);
        } else {
            iter++;
        }
    }

    return true;
}
/*
static void filter_fmts_by_encs(std::vector<FMTP>& support_input_fmtps,
                            std::vector<RTP_ENCODING>& support_rtp_encodings) {
    int rtx_payload   = -1;
    std::string codec_type;

    for (auto item : support_rtp_encodings) {
        if (item.codec == "rtx") {
            rtx_payload = item.payload;
        } else {
            codec_type = item.codec;
        }
    }

    if (rtx_payload > 0) {
        std::vector<FMTP>::iterator iter = support_input_fmtps.begin();
        while(iter != support_input_fmtps.end()) {
            size_t pos = iter->config.find("apt=");
            if (pos == std::string::npos) {
                if (codec_type == "H264") {
                    iter++;
                } else {
                    iter = support_input_fmtps.erase(iter);
                }
                continue;
            }
            if (iter->payload == rtx_payload) {
                iter++;
            } else {
                iter = support_input_fmtps.erase(iter);
            }
        }
    }
    return;
}

static void filter_enc_by_fmts(std::vector<RTP_ENCODING>& support_rtp_encodings,
                            std::vector<FMTP>& support_input_fmtps) {
    int codec_payload = -1;
    int apt_payload   = -1;

    for (auto item : support_rtp_encodings) {
        if (item.codec != "rtx") {
            codec_payload = item.payload;
            break;
        }
    }

    for (auto item : support_rtp_encodings) {
        if (item.codec == "rtx") {
            for (const FMTP& fmtp_item : support_input_fmtps) {
                size_t pos = fmtp_item.config.find("apt=");
                if (pos == 0) {
                    int payload = get_apt_payload(fmtp_item.config);
                    if (codec_payload == payload) {
                        apt_payload = fmtp_item.payload;
                        break;
                    }
                }
            }
        }
    }

    log_infof("---- codec payload:%d, apt payload:%d", codec_payload, apt_payload);

    if (codec_payload > 0) {
        auto enc_iter = support_rtp_encodings.begin();
        while (enc_iter != support_rtp_encodings.end()) {
            if (enc_iter->codec != "rtx") {
                enc_iter++;
                continue;
            }

            if (enc_iter->payload != apt_payload) {
                enc_iter = support_rtp_encodings.erase(enc_iter);
            } else {
                enc_iter++;
            }
        }
    }

    return;
}
*/

void get_support_rtc_media(const rtc_media_info& input, rtc_media_info& support_rtc_media) {
    support_rtc_media.version = input.version;
    support_rtc_media.extmap_allow_mixed = input.extmap_allow_mixed;
    support_rtc_media.name = input.name;
    support_rtc_media.origin = input.origin;
    support_rtc_media.origin.address = "0.0.0.0";
    support_rtc_media.groups = input.groups;
    support_rtc_media.msid_semantic = input.msid_semantic;
    support_rtc_media.timing = input.timing;

    support_rtc_media.connect_info = input.connect_info;

    if (input.direction_type == SEND_ONLY) {
        support_rtc_media.direction_type = RECV_ONLY;
    } else if (input.direction_type == RECV_ONLY) {
        support_rtc_media.direction_type = SEND_ONLY;
    } else {
        support_rtc_media.direction_type = SEND_RECV;
    }

    for (auto rtc_info : input.medias) {
        MEDIA_RTC_INFO support_rtc_info;

        support_rtc_info.mid = rtc_info.mid;
        support_rtc_info.msid = rtc_info.msid;
        support_rtc_info.media_type = rtc_info.media_type;
        support_rtc_info.port = rtc_info.port;
        if (support_rtc_info.port == 0) {
            support_rtc_info.port = 9;
        }
        support_rtc_info.rtcp_mux = rtc_info.rtcp_mux;
        support_rtc_info.rtcp_rsize = rtc_info.rtcp_rsize;
        support_rtc_info.setup = "active";//rtc_info.setup;
        support_rtc_info.rtcp = rtc_info.rtcp;
        support_rtc_info.protocol = rtc_info.protocol;
        support_rtc_info.payloads = rtc_info.payloads;

        get_support_header_ext(rtc_info.header_extentions, support_rtc_info.header_extentions);
        get_support_rtcp_fb(rtc_info.rtcp_fbs, support_rtc_info.rtcp_fbs);
        get_support_ssrc_info(rtc_info.ssrc_infos, support_rtc_info.ssrc_infos);
        get_support_rtp_encoding(rtc_info.rtp_encodings,
                                support_rtc_info.rtp_encodings);
        std::stringstream pre_enc_ss;
        for (auto enc_item : support_rtc_info.rtp_encodings) {
            pre_enc_ss << "codec:" << enc_item.codec << ", encoding:" << enc_item.encoding
                << ", payload:" << enc_item.payload << "\r\n";
        }
        //log_infof("(pre) support rtp encodes:\r\n%s", pre_enc_ss.str().c_str());
        get_support_fmtp(rtc_info.fmtps, support_rtc_info.fmtps);
        std::stringstream pre_fmtp_ss;
        for (auto item : support_rtc_info.fmtps) {
            pre_fmtp_ss << "config:" << item.config << ", payload:" << item.payload << "\r\n";
        }
        //log_infof("(pre)support fmtps:\r\n%s", pre_fmtp_ss.str().c_str());

        if (support_rtc_info.media_type == "video") {
            int h264_payload     = 0;
            int h264_apt_payload = 0;
            bool has_h264profile = fmtp_has_h264profile(support_rtc_info.fmtps,
                                                h264_payload, h264_apt_payload);
            if (has_h264profile && (h264_payload > 0) && (h264_apt_payload > 0)) {
                //log_infof("h264 payload:%d, apt payload:%d", h264_payload, h264_apt_payload);
                filter_fmts_and_encs_by_h264payload(support_rtc_info.fmtps,
                                                support_rtc_info.rtp_encodings,
                                                h264_payload, h264_apt_payload);
            } else {
                filter_fmts_and_encs_by_vp8(support_rtc_info.fmtps,
                                        support_rtc_info.rtp_encodings);
            }
        }

        std::stringstream enc_ss;
        for (auto enc_item : support_rtc_info.rtp_encodings) {
            enc_ss << "codec:" << enc_item.codec << ", encoding:" << enc_item.encoding
                << ", payload:" << enc_item.payload << "\r\n";
        }
        //log_infof("support rtp encodes:\r\n%s", enc_ss.str().c_str());
        std::stringstream fmtp_ss;
        for (auto item : support_rtc_info.fmtps) {
            fmtp_ss << "config:" << item.config << ", payload:" << item.payload << "\r\n";
        }
        //log_infof("support fmtps:\r\n%s", fmtp_ss.str().c_str());

        if (!rtc_info.ssrc_groups.empty()) {
            support_rtc_info.ssrc_groups = rtc_info.ssrc_groups;
        }
        support_rtc_media.medias.push_back(support_rtc_info);
    }
    support_rtc_media.filter_payloads();
    return;
}

void rtc_media_info_to_json(const rtc_media_info& input, json& sdp_json) {
    sdp_json["version"] = input.version;
    sdp_json["extmapAllowMixed"] = input.extmap_allow_mixed;

    sdp_json["groups"] = json::array();
    for (auto group : input.groups) {
        json group_json = json::object();
        group_json["type"] = group.type;

        std::string mid_str;
        int index = 0;
        for (auto mid : group.mids) {
            if (index != 0) {
                mid_str += " ";
            }

            mid_str += std::to_string(mid);

            index++;
        }
        group_json["mids"] = mid_str;
        sdp_json["groups"].push_back(group_json);
    }

    sdp_json["msidSemantic"] = json::object();
    sdp_json["msidSemantic"]["token"] = input.msid_semantic.token;

    sdp_json["name"] = input.name;

    sdp_json["origin"] = json::object();
    sdp_json["origin"]["address"]        = input.origin.address;
    sdp_json["origin"]["ipVer"]          = input.origin.ip_ver;
    sdp_json["origin"]["netType"]        = input.origin.net_type;
    sdp_json["origin"]["sessionId"]      = input.origin.session_id;
    sdp_json["origin"]["sessionVersion"] = input.origin.session_version;
    sdp_json["origin"]["username"]       = input.origin.user_name;

    sdp_json["timing"] = json::object();
    sdp_json["timing"]["start"] = input.timing.start;
    sdp_json["timing"]["stop"]  = input.timing.stop;

    //media information

    sdp_json["media"] = json::array();
    for (const auto& media_info : input.medias) {
        json media_json = json::object();

        media_json["type"] = media_info.media_type;
        media_json["mid"]  = std::to_string(media_info.mid);
        if (!media_info.msid.empty()) {
            media_json["msid"] = media_info.msid;
        }

        std::string payloads_str;
        int index = 0;
        for (auto& payload : media_info.payloads) {
            if (index != 0) {
                payloads_str += " ";
            }
            payloads_str += std::to_string(payload);
            index++;
        }
        media_json["payloads"] = payloads_str;
        media_json["port"] = media_info.port;
        media_json["protocol"] = media_info.protocol;

        media_json["rtcp"] = json::object();
        media_json["rtcp"]["address"] = media_info.rtcp.address;
        media_json["rtcp"]["ipVer"] = media_info.rtcp.ip_ver;
        media_json["rtcp"]["netType"] = media_info.rtcp.net_type;
        media_json["rtcp"]["port"] = media_info.rtcp.port;

        media_json["connection"] = json::object();
        media_json["connection"]["ip"]      = input.connect_info.ip;
        media_json["connection"]["version"] = input.connect_info.version;

        if (input.direction_type == SEND_ONLY) {
            media_json["direction"] = "sendonly";
        } else if (input.direction_type == RECV_ONLY) {
            media_json["direction"] = "recvonly";
        } else {
            media_json["direction"] = "sendrecv";
        }

        if (!input.candidates.empty()) {
            media_json["candidates"] = json::array();
            for (auto candidate_item : input.candidates) {
                json candidate_json = json::object();
                candidate_json["component"]  = candidate_item.component;
                candidate_json["foundation"] = candidate_item.foundation;
                candidate_json["ip"]         = candidate_item.ip;
                candidate_json["port"]       = candidate_item.port;
                candidate_json["priority"]   = candidate_item.priority;
                candidate_json["transport"]  = candidate_item.transport;
                candidate_json["type"]       = candidate_item.type;
                media_json["candidates"].push_back(candidate_json);
            }
        }
        media_json["fingerprint"] = json::object();
        media_json["fingerprint"]["hash"] = input.finger_print.hash;
        media_json["fingerprint"]["type"] = input.finger_print.type;

        media_json["iceOptions"] = "renomination";//input.ice.ice_options;
        media_json["icePwd"]     = input.ice.ice_pwd;
        media_json["iceUfrag"]   = input.ice.ice_ufrag;
        media_json["icelite"]    = "ice-lite";

        if (!media_info.rtcp_mux.empty()) {
            media_json["rtcpMux"]   = media_info.rtcp_mux;
        }
        
        if (!media_info.rtcp_rsize.empty()) {
            media_json["rtcpRsize"] = media_info.rtcp_rsize;
        }
        //ice-lite: the server must be active
        media_json["setup"] = "active";

        if (!media_info.ssrc_groups.empty()) {
            media_json["ssrcGroups"] = json::array();
            for (const auto &ssrc_group : media_info.ssrc_groups) {
                json ssrc_json = json::object();
                ssrc_json["semantics"] = ssrc_group.semantics;
    
                std::string ssrcs_str;
                int index = 0;
                for (auto ssrc : ssrc_group.ssrcs) {
                    if (index != 0) {
                        ssrcs_str += " ";
                    }
                    ssrcs_str += std::to_string(ssrc);
                    index++;
                }
                ssrc_json["ssrcs"] = ssrcs_str;
    
                media_json["ssrcGroups"].push_back(ssrc_json);
            }
        }

        media_json["rtp"] = json::array();
        for (const auto& rtp : media_info.rtp_encodings) {
            json rtp_json = json::object();
            rtp_json["codec"]   = rtp.codec;
            rtp_json["payload"] = rtp.payload;
            rtp_json["rate"]    = rtp.clock_rate;
            if (!rtp.encoding.empty()) {
                rtp_json["encoding"] = rtp.encoding;
            }

            media_json["rtp"].push_back(rtp_json);
        }

        if (!media_info.rtcp_fbs.empty()) {
            media_json["rtcpFb"] = json::array();
            for (const auto& rtcp_fb : media_info.rtcp_fbs) {
                json rtcp_fb_json = json::object();
                rtcp_fb_json["payload"] = std::to_string(rtcp_fb.payload);
                rtcp_fb_json["type"]    = rtcp_fb.type;
                if (!rtcp_fb.subtype.empty()) {
                    rtcp_fb_json["subtype"] = rtcp_fb.subtype;
                }

                media_json["rtcpFb"].push_back(rtcp_fb_json);
            }
        }

        media_json["fmtp"] = json::array();
        for (const auto& fmtp : media_info.fmtps) {
            json fmtp_json = json::object();
            fmtp_json["config"]  = fmtp.config;
            fmtp_json["payload"] = fmtp.payload;
            media_json["fmtp"].push_back(fmtp_json);
        }

        media_json["ext"] = json::array();
        for (const auto& ext : media_info.header_extentions) {
            json ext_object = json::object();
            ext_object["uri"] = ext.uri;
            ext_object["value"] = ext.value;

            media_json["ext"].push_back(ext_object);
        }

        media_json["ssrcs"] = json::array();
        for (const auto& ssrc_info : media_info.ssrc_infos) {
            json ssrc_object = json::object();
            ssrc_object["attribute"] = ssrc_info.attribute;
            ssrc_object["id"] = ssrc_info.ssrc;
            ssrc_object["value"] = ssrc_info.value;

            media_json["ssrcs"].push_back(ssrc_object);
        }

        sdp_json["media"].push_back(media_json);
    }
    return;
}

