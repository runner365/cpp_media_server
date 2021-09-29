#include "rtc_media_info.hpp"
#include "logger.hpp"
#include "utils/stringex.hpp"
#include <string>
#include <sstream>
#include <iostream>

rtc_media_info::rtc_media_info()
{

}

rtc_media_info::~rtc_media_info()
{

}

int rtc_media_info::parse(json& sdp_json) {
    int ret = -1;

    get_basic_info(sdp_json);

    auto media_iterJson = sdp_json.find("media");
    if (media_iterJson == sdp_json.end()) {
        MS_THROW_ERROR("no media field in json");
    }
    
    if (!media_iterJson->is_array()) {
        MS_THROW_ERROR("the media field is not array");
    }

    for (auto& media_item_json : *media_iterJson) {
        this->direction_type = get_direction_type(media_item_json);
        this->finger_print   = get_finger_print(media_item_json);
        this->ice            = get_ice(media_item_json);

        MEDIA_RTC_INFO rtc_info;

        auto mid_iterJson = media_item_json.find("mid");
        if ((mid_iterJson == media_item_json.end()) || (!mid_iterJson->is_string())) {
            MS_THROW_ERROR("no mid field in json");
        }
        rtc_info.mid = std::stoi(mid_iterJson->get<std::string>());

        auto msid_iterJson = media_item_json.find("msid");
        if ((msid_iterJson == media_item_json.end()) || (!msid_iterJson->is_string())) {
            MS_THROW_ERROR("no msid field in json");
        }
        rtc_info.msid = msid_iterJson->get<std::string>();

        auto port_iterJson = media_item_json.find("port");
        if ((port_iterJson == media_item_json.end()) || (!port_iterJson->is_number())) {
            MS_THROW_ERROR("no port field in json");
        }
        rtc_info.port = port_iterJson->get<int>();

        auto protocol_iterJson = media_item_json.find("protocol");
        if ((protocol_iterJson == media_item_json.end()) || (!protocol_iterJson->is_string())) {
            MS_THROW_ERROR("no protocol field in json");
        }
        rtc_info.protocol = protocol_iterJson->get<std::string>();

        auto rtcp_mux_iterJson = media_item_json.find("rtcpMux");
        if ((rtcp_mux_iterJson == media_item_json.end()) || (!rtcp_mux_iterJson->is_string())) {
            MS_THROW_ERROR("no rtcpMux field in json");
        }
        rtc_info.rtcp_mux = rtcp_mux_iterJson->get<std::string>();

        auto setup_iterJson = media_item_json.find("setup");
        if ((setup_iterJson == media_item_json.end()) || (!setup_iterJson->is_string())) {
            MS_THROW_ERROR("no setup field in json");
        }
        rtc_info.setup = setup_iterJson->get<std::string>();

        auto mediatype_iter = media_item_json.find("type");
        if ((mediatype_iter == media_item_json.end()) || (!mediatype_iter->is_string())) {
            MS_THROW_ERROR("no media type field in json");
        }
        rtc_info.media_type = mediatype_iter->get<std::string>();

        get_connection_info(media_item_json, this->connect_info);
        get_rtcp_basic(media_item_json, rtc_info.rtcp);
        get_ssrc_groups(media_item_json, rtc_info.ssrc_groups);
        get_payloads(media_item_json, rtc_info.payloads);
        get_header_extersion(media_item_json, rtc_info.header_extentions);
        get_rtp_encodings(media_item_json, rtc_info.rtp_encodings);
        get_ssrcs_info(media_item_json, rtc_info.ssrc_infos);
        get_fmtps(media_item_json, rtc_info.fmtps);

        this->medias.push_back(rtc_info);
    }
    return ret;
}

void rtc_media_info::get_basic_info(json& info_json) {
    auto extMixed_iterJson = info_json.find("extmapAllowMixed");
    if ((extMixed_iterJson != info_json.end()) && (extMixed_iterJson->is_string())) {
        this->extmap_allow_mixed = extMixed_iterJson->get<std::string>();
    }
    
    auto version_iterJson = info_json.find("version");
    if ((version_iterJson != info_json.end()) && (version_iterJson->is_number())) {
        this->version = version_iterJson->get<int>();
    } else {
        MS_THROW_ERROR("the version field error");
    }
    
    auto groups_iterJson = info_json.find("groups");
    if ((groups_iterJson != info_json.end()) && (groups_iterJson->is_array())) {
        for (auto item : *groups_iterJson) {
            BASIC_GROUP group_item;
            std::vector<std::string> mid_str_vec;
            std::string mids = item["mids"];

            group_item.type  = item["type"];

            string_split(mids, " ", mid_str_vec);
            for(auto mid_str : mid_str_vec) {
                group_item.mids.push_back(std::stoi(mid_str));
            }
            this->groups.push_back(group_item);
        }
    }
    
    auto msid_semantic_iterJson = info_json.find("msidSemantic");
    if ((msid_semantic_iterJson != info_json.end()) && (msid_semantic_iterJson->is_object())) {
        this->msid_semantic.token = (*msid_semantic_iterJson)["token"];
    }
    
    auto name_iterJson = info_json.find("name");
    if ((name_iterJson != info_json.end()) && (name_iterJson->is_string())) {
        this->name = name_iterJson->get<std::string>();
    }
    
    auto origin_iterJson = info_json.find("origin");
    if ((origin_iterJson != info_json.end()) && (origin_iterJson->is_object())) {
        this->origin.address         = (*origin_iterJson)["address"];
        this->origin.ip_ver          = (*origin_iterJson)["ipVer"];
        this->origin.net_type        = (*origin_iterJson)["netType"];
        this->origin.session_id      = (*origin_iterJson)["sessionId"];
        this->origin.session_version = (*origin_iterJson)["sessionVersion"];
        this->origin.user_name       = (*origin_iterJson)["username"];
    }
    
    auto timing_iterJson = info_json.find("timing");
    if ((timing_iterJson != info_json.end()) && (timing_iterJson->is_object())) {
        this->timing.start = (*timing_iterJson)["start"];
        this->timing.stop  = (*timing_iterJson)["stop"];
    }
    
    return;
}

void rtc_media_info::get_connection_info(json& info_json, CONNECT_INFO& connect) {
    auto connection_iter = info_json.find("connection");

    if ((connection_iter == info_json.end()) || (!connection_iter->is_object())) {
        MS_THROW_ERROR("no connection field in json");
    }

    connect.ip      = (*connection_iter)["ip"];
    connect.version = (*connection_iter)["version"];
    return;
}

void rtc_media_info::get_rtcp_basic(json& info_json, RTCP_BASIC& rtcp) {
    auto rtcp_iter = info_json.find("rtcp");

    if ((rtcp_iter == info_json.end()) || (!rtcp_iter->is_object())) {
        MS_THROW_ERROR("no rtcp field in json");
    }

    rtcp.address  = (*rtcp_iter)["address"];
    rtcp.ip_ver   = (*rtcp_iter)["ipVer"];
    rtcp.net_type = (*rtcp_iter)["netType"];
    rtcp.port     = (*rtcp_iter)["port"];

    return;
}

void rtc_media_info::get_ssrc_groups(json& info_json, std::vector<SSRC_GROUPS>& groups) {
    auto ssrc_groups_iter = info_json.find("ssrcGroups");

    if ((ssrc_groups_iter == info_json.end()) || (!ssrc_groups_iter->is_array())) {
        //it's option
        return;
    }

    for (auto ssrc_item : *ssrc_groups_iter) {
        SSRC_GROUPS info;
        std::string ssrcs;
        std::vector<std::string> ssrc_vec;

        info.semantics = ssrc_item["semantics"];
        ssrcs = ssrc_item["ssrcs"];

        string_split(ssrcs, " ", ssrc_vec);
        for (const std::string& ssrc : ssrc_vec) {
            info.ssrcs.push_back((uint32_t)std::stoul(ssrc));
        }
        groups.push_back(info);
    }

    return;
}

int rtc_media_info::get_direction_type(json& info_json) {
    auto direction_iter = info_json.find("direction");

    if (direction_iter == info_json.end()) {
        MS_THROW_ERROR("no direction field in json");
    }

    if (!direction_iter->is_string()) {
        MS_THROW_ERROR("direction field is not number");
    }

    std::string direction = direction_iter->get<std::string>();
    int ret = SEND_RECV;

    if (direction == "sendonly") {
        ret = SEND_ONLY;
    } else if (direction == "recvonly") {
        ret = RECV_ONLY;
    } else {
        ret = SEND_RECV;
    }
    return ret;
}

FINGER_PRINT rtc_media_info::get_finger_print(json& info_json) {
    auto finger_iter = info_json.find("fingerprint");

    if (finger_iter == info_json.end()) {
        MS_THROW_ERROR("no fingerprint field in json");
    }

    if (!finger_iter->is_object()) {
        MS_THROW_ERROR("fingerprint field is not number");
    }

    FINGER_PRINT ret;

    ret.hash = (*finger_iter)["hash"];
    ret.type = (*finger_iter)["type"];

    return ret;
}

void rtc_media_info::get_payloads(json& info_json, std::vector<int>& payload_vec) {
    auto payloads_iter = info_json.find("payloads");

    if ((payloads_iter == info_json.end()) || (!payloads_iter->is_string())) {
        MS_THROW_ERROR("payloads field error");
    }

    std::vector<std::string> payload_str_vec;
    string_split(payloads_iter->get<std::string>(), " ", payload_str_vec);

    if (payload_str_vec.empty()) {
        MS_THROW_ERROR("payloads field has no payload number");
    }

    for (auto payload_str : payload_str_vec) {
        payload_vec.push_back(std::stoi(payload_str));
    }
}

ICE_INFO rtc_media_info::get_ice(json& info_json) {
    ICE_INFO ret;

    ret.ice_options = info_json["iceOptions"];
    ret.ice_pwd     = info_json["icePwd"];
    ret.ice_ufrag   = info_json["iceUfrag"];

    return ret;
}

void rtc_media_info::get_header_extersion(json& info_json, std::vector<HEADER_EXT>& ext_vec) {
    auto exts_iterJson = info_json.find("ext");
    if (exts_iterJson == info_json.end()) {
        MS_THROW_ERROR("no ext field in json");
    }
    
    if (!exts_iterJson->is_array()) {
        MS_THROW_ERROR("the ext field is not array");
    }

    for (auto& ext_json : *exts_iterJson) {
        HEADER_EXT ext;

        ext.uri   = ext_json["uri"];
        ext.value = ext_json["value"];

        ext_vec.push_back(ext);
    }
    return;
}

void rtc_media_info::get_rtcpfb(json& info_json, std::vector<RTCP_FB>& rtcp_fbs) {
    auto rtcp_fb_iterJson = info_json.find("rtcpFb");
    if (rtcp_fb_iterJson == info_json.end()) {
        MS_THROW_ERROR("no rtcpFb field in json");
    }
    
    if (!rtcp_fb_iterJson->is_array()) {
        MS_THROW_ERROR("the rtcpFb field is not array");
    }

    for (auto& rtcp_fb_json : *rtcp_fb_iterJson) {
        RTCP_FB rtcp_fb;

        rtcp_fb.payload   = rtcp_fb_json["payload"];
        rtcp_fb.type      = rtcp_fb_json["type"];

        auto subtype_iter = rtcp_fb_json.find("subtype");
        if (subtype_iter != rtcp_fb_json.end()) {
            rtcp_fb.subtype = rtcp_fb_json["subtype"];
        }

        rtcp_fbs.push_back(rtcp_fb);
    }
    return;
}

void rtc_media_info::get_ssrcs_info(json& info_json, std::vector<SSRC_INFO>& ssrc_infos) {
    auto ssrcs_iterJson = info_json.find("ssrcs");
    if (ssrcs_iterJson == info_json.end()) {
        MS_THROW_ERROR("no ssrcs field in json");
    }
    
    if (!ssrcs_iterJson->is_array()) {
        MS_THROW_ERROR("the ssrcs field is not array");
    }

    for (auto& ssrc_json : *ssrcs_iterJson) {
        SSRC_INFO ssrc;

        ssrc.attribute = ssrc_json["attribute"];
        ssrc.value     = ssrc_json["value"];
        ssrc.ssrc      = ssrc_json["id"];

        ssrc_infos.push_back(ssrc);
    }
    return;
}

void rtc_media_info::get_rtp_encodings(json& info_json, std::vector<RTP_ENCODING>& rtp_encodings) {
    auto rtp_iterJson = info_json.find("rtp");
    if (rtp_iterJson == info_json.end()) {
        MS_THROW_ERROR("no rtp field in json");
    }
    
    if (!rtp_iterJson->is_array()) {
        MS_THROW_ERROR("the rtp field is not array");
    }

    for (auto& rtp_json : *rtp_iterJson) {
        RTP_ENCODING enc;

        enc.codec      = rtp_json["codec"];
        enc.payload    = rtp_json["payload"];
        enc.clock_rate = rtp_json["rate"];
        auto encoding_iter = rtp_json.find("encoding");
        if (encoding_iter != rtp_json.end() && encoding_iter->is_string()) {
            enc.encoding = rtp_json["encoding"];
        }
        rtp_encodings.push_back(enc);
    }

    return;
}

void rtc_media_info::get_fmtps(json& info_json, std::vector<FMTP>& fmtps) {
    auto fmtp_iterJson = info_json.find("fmtp");
    if (fmtp_iterJson == info_json.end()) {
        MS_THROW_ERROR("no fmtp field in json");
    }
    
    if (!fmtp_iterJson->is_array()) {
        MS_THROW_ERROR("the fmtp field is not array");
    }

    for (auto& fmtp_json : *fmtp_iterJson) {
        FMTP fmtp;

        fmtp.config  = fmtp_json["config"];
        fmtp.payload = fmtp_json["payload"];

        fmtps.push_back(fmtp);
    }
    return;
}

void rtc_media_info::filter_payloads() {
    for (auto& media_item : medias) {
        std::vector<int> ret_payloads;
        for (auto payload : media_item.payloads) {
            bool found = false;

            for (auto rtp_item : media_item.rtp_encodings) {
                if (payload == rtp_item.payload) {
                    found = true;
                    break;
                }
            }
            if (found) {
                ret_payloads.push_back(payload);
                continue;
            }
            for (auto rtcp_fb_item: media_item.rtcp_fbs) {
                if (payload == rtcp_fb_item.payload) {
                    found = true;
                    break;
                }
            }
            if (found) {
                ret_payloads.push_back(payload);
                continue;
            }
        }
        media_item.payloads = ret_payloads;
    }
    return;
}

std::string rtc_media_info::dump() {
    std::stringstream ss;

    ss << "version: " << version << "\r\n";
    ss << "extmapAllowMixed: " << this->extmap_allow_mixed << "\r\n";
    ss << "groups:" << "\r\n";
    for (auto group_item : this->groups) {
        ss << "--type: " << group_item.type << "\r\n";
        ss << "--mids: ";
        for (auto mid : group_item.mids) {
            ss << mid << " ";
        }
        ss << "\r\n";
    }

    ss << "msidSemantic:" << "\r\n";
    ss << "--token: " << msid_semantic.token << "\r\n";
    ss << "name: " << name << "\r\n";

    ss << "origin:" << "\r\n";
    ss << "--address: " << origin.address << "\r\n";
    ss << "--ipVer: " << origin.ip_ver << "\r\n";
    ss << "--netType: " << origin.net_type << "\r\n";
    ss << "--sessionId: " << origin.session_id << "\r\n";
    ss << "--sessionVersion: " << origin.session_version << "\r\n";
    ss << "--username: " << origin.user_name << "\r\n";

    ss << "timing:" << "\r\n";
    ss << "--start: " << timing.start << "\r\n";
    ss << "--stop: " << timing.stop << "\r\n";
    
    ss << "direction: " << direction_type << "\r\n";
    ss << "finge print:" << "\r\n";
    ss << "--hash: " << finger_print.hash << "\r\n";
    ss << "--type: " << finger_print.type << "\r\n";
    ss << "ice information:" << "\r\n";
    ss << "--options: " << ice.ice_options << "\r\n";
    ss << "--pwd: " << ice.ice_pwd << "\r\n";
    ss << "--ufrag: " << ice.ice_ufrag << "\r\n";

    int candidate_index = 0;
    for (auto candidate_item : this->candidates) {
        ss << "--candidate[" << candidate_index++ << "]\r\n";
        ss << "----foundation:" << candidate_item.foundation << "\r\n";
        ss << "----component:" << candidate_item.component << "\r\n";
        ss << "----transport:" << candidate_item.transport << "\r\n";
        ss << "----priority:" << candidate_item.priority << "\r\n";
        ss << "----ip:" << candidate_item.ip << "\r\n";
        ss << "----port:" << candidate_item.port << "\r\n";
        ss << "----type:" << candidate_item.type << "\r\n";
    }

    for (auto media_item : medias) {
        ss << "\r\n";
        ss << "--media type: " << media_item.media_type << "\r\n";
        ss << "--mid: " << media_item.mid << "\r\n";
        ss << "--msid: " << media_item.msid << "\r\n";
        ss << "--port: " << media_item.port << "\r\n";
        ss << "--rtcpMux: " << media_item.rtcp_mux << "\r\n";
        ss << "--rtcpRsize: " << media_item.rtcp_rsize << "\r\n";
        ss << "--setup: " << media_item.setup << "\r\n";
        ss << "--protocol: " << media_item.protocol << "\r\n";

        ss << "--rtcp:\r\n";
        ss << "----address: " << media_item.rtcp.address << "\r\n";
        ss << "----ipVer: " << media_item.rtcp.ip_ver << "\r\n";
        ss << "----netType: " << media_item.rtcp.net_type << "\r\n";
        ss << "----port: " << media_item.rtcp.port << "\r\n";

        ss << "--header extensions:" << "\r\n";
        for (auto ext : media_item.header_extentions) {
            ss << "----uri: " << ext.uri << "\r\n";
            ss << "----value: " << ext.value << "\r\n";
            ss << "-----------" << "\r\n";
        }

        ss << "--rtcp fb:" << "\r\n";
        for (auto rtcp_fb : media_item.rtcp_fbs) {
            ss << "----payload: " << rtcp_fb.payload << "\r\n";
            ss << "----type: " << rtcp_fb.type << "\r\n";
            ss << "----subtype: " << rtcp_fb.subtype << "\r\n";
        }

        ss << "--rtp encodings:" << "\r\n";
        for (auto rtp_enc : media_item.rtp_encodings) {
            ss << "----codec: " << rtp_enc.codec << "\r\n";
            ss << "----payload: " << rtp_enc.payload << "\r\n";
            ss << "----clock_rate: " << rtp_enc.clock_rate << "\r\n";
            ss << "-----------" << "\r\n";
        }

        ss << "--ssrc information:" << "\r\n";
        for (auto ssrc_info : media_item.ssrc_infos) {
            ss << "----attribute: " << ssrc_info.attribute << "\r\n";
            ss << "----value: " << ssrc_info.value << "\r\n";
            ss << "----ssrc: " << ssrc_info.ssrc << "\r\n";
            ss << "-----------" << "\r\n";
        }

        ss << "--fmtp information:" << "\r\n";
        for (auto fmtp : media_item.fmtps) {
            ss << "---config: " << fmtp.config << "\r\n";
            ss << "---payload: " << fmtp.payload << "\r\n";
            ss << "-----------" << "\r\n";
        }

        ss << "--payloads:\r\n";
        int i = 0;
        for (auto payload :media_item.payloads) {
            if (i++ == 0) {
                ss << "----";
            }
            ss << " " << payload;
        }
        ss << "\r\n";
    }
    return ss.str();
}