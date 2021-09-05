#ifndef RTMP_PUB_HPP
#define RTMP_PUB_HPP
#include "media_packet.hpp"
#include "stringex.hpp"
#include "logger.hpp"
#include <string>
#include <stdint.h>

#define RTMP_OK             0
#define RTMP_NEED_READ_MORE 1

#define CHUNK_DEF_SIZE      128

typedef enum {
    RTMP_CONTROL_SET_CHUNK_SIZE = 1,//idSetChunkSize = 1,
    RTMP_CONTROL_ABORT_MESSAGE,//idAbortMessage,
    RTMP_CONTROL_ACK,//idAck,
    RTMP_CONTROL_USER_CTRL_MESSAGES,//idUserControlMessages,
    RTMP_CONTROL_WINDOW_ACK_SIZE,//idWindowAckSize,
    RTMP_CONTROL_SET_PEER_BANDWIDTH,//idSetPeerBandwidth,
    RTMP_MEDIA_PACKET_AUDIO = 8,
    RTMP_MEDIA_PACKET_VIDEO = 9,
    RTMP_COMMAND_MESSAGES_META_DATA3 = 15,
    RTMP_COMMAND_MESSAGES_AMF3 = 17,
    RTMP_COMMAND_MESSAGES_META_DATA0 = 18,
    RTMP_COMMAND_MESSAGES_AMF0 = 20
} RTMP_CONTROL_TYPE;

typedef enum {
    Event_streamBegin = 0,
    Event_streamEOF,
    Event_streamDry,
    Event_setBufferLen,
    Event_streamIsRecorded,
    Event_pingRequest,
    Event_pingResponse,
 } RTMP_CTRL_EVENT_TYPE;

#define	CMD_Connect       "connect"
#define	CMD_Fcpublish     "FCPublish"
#define	CMD_ReleaseStream "releaseStream"
#define	CMD_CreateStream  "createStream"
#define	CMD_Publish       "publish"
#define	CMD_FCUnpublish   "FCUnpublish"
#define	CMD_DeleteStream  "deleteStream"
#define	CMD_Play          "play"

inline int get_rtmp_url_info(const std::string& url, std::string& host, uint16_t& port,
            std::string& tc_url, std::string& app, std::string& streamname) {
    std::size_t pos = url.find("rtmp://");

    if (pos != 0) {
        return -1;
    }

    if (url.size() < 8) {
        return -2;
    }

    std::string info = url.substr(7);
    std::vector<std::string> info_vec;

    int cnt = string_split(info, "/", info_vec);
    if (cnt < 2) {
        return -3;
    }

    host = info_vec[0];
    int iport = -1;
    pos = host.find(":");
    if (pos == host.npos) {
        iport = 1935;
    } else {
        std::string port_str = host.substr(pos+1);
        iport = atoi(port_str.c_str());
        host = host.substr(0, pos);
    }

    if (iport < 0) {
        return -4;
    }
    port = (uint16_t)iport;

    char app_sz[512];
    int app_len = 0;
    char tcurl_sz[512];
    int tcurl_len = 0;

    tcurl_len += snprintf(tcurl_sz + tcurl_len, sizeof(tcurl_sz) - tcurl_len, "rtmp://");
    for (size_t index = 0; index < info_vec.size() - 1; index++) {
        if (index == 0) {
            tcurl_len += snprintf(tcurl_sz + tcurl_len, sizeof(tcurl_sz) - tcurl_len, "%s", info_vec[index].c_str());
        } else {
            tcurl_len += snprintf(tcurl_sz + tcurl_len, sizeof(tcurl_sz) - tcurl_len, "/%s", info_vec[index].c_str());
            app_len += snprintf(app_sz + app_len, sizeof(app_sz) - app_len, "%s", info_vec[index].c_str());
            if ((index + 1) < (info_vec.size() - 1)) {
                app_len += snprintf(app_sz + app_len, sizeof(app_sz) - app_len, "/");
            }
        }
    }
    app = app_sz;

    tc_url = tcurl_sz;
    
    streamname = info_vec[cnt - 1];

    return 0;
}

#endif //RTMP_PUB_HPP