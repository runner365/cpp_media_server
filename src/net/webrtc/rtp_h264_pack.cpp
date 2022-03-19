#include "rtp_h264_pack.hpp"
#include "net/rtprtcp/rtprtcp_pub.hpp"
#include "net/rtprtcp/rtp_packet.hpp"

#include <cstring>

static rtp_packet* make_rtp_packet(header_extension* ext, size_t payload_len) {
    uint8_t* data = new uint8_t[RTP_PACKET_MAX_SIZE];
    size_t ext_len = 0;
    rtp_common_header* header = (rtp_common_header*)data;

    memset(header, 0, sizeof(rtp_common_header));
    header->version = RTP_VERSION;

    if (ext) {
        header->extension = 1;
        uint8_t* p = (uint8_t*)(header + 1);
        memcpy(p, ext, 4 + 4 * ntohs(ext->length));
        ext_len = 4 + 4 * ntohs(ext->length);
    } else {
        header->extension = 0;
        ext_len = 0;
    }

    size_t data_len = payload_len + sizeof(rtp_common_header) + ext_len;
    if (data_len > RTP_PACKET_MAX_SIZE) {
        return nullptr;
    }
    rtp_packet* packet = rtp_packet::parse(data, data_len);
    packet->set_need_delete(true);

    return packet;
}

rtp_packet* generate_stapA_packets(std::vector<std::pair<unsigned char*, int>> NaluVec, header_extension* ext) {
    size_t data_len = 0;

    data_len += kNalHeaderSize;
    for (auto& nalu : NaluVec) {
        data_len += kLengthFieldSize;
        data_len += nalu.second;
    }
    rtp_packet* packet = make_rtp_packet(ext, data_len);

    uint8_t* payload = packet->get_payload();
    auto nalu        = NaluVec[0];
    auto nalu_header = (nalu.first)[0];
    payload[0]       = (nalu_header & (kFBit | kNriMask)) | NaluType::kStapA;
    size_t index     = kNalHeaderSize;

    for (auto& nalu : NaluVec)
    {
        payload[index]     = nalu.second >> 8;
        payload[index + 1] = nalu.second;
        index += kLengthFieldSize;
        memcpy(&payload[index], nalu.first, nalu.second);
        index += nalu.second;
    }
    packet->set_payload_length(index);

    return packet;
}


std::vector<rtp_packet*> generate_fuA_packets(uint8_t* nalu, size_t nalu_size, header_extension* ext) {
    std::vector<rtp_packet*> packets;
    size_t payload_left             = nalu_size - kNalHeaderSize;
    std::vector<int> fragment_sizes = split_nalu(payload_left);
    uint8_t naul_header             = nalu[0];
    auto fragment                   = nalu + kNalHeaderSize;

    for (size_t i = 0; i < fragment_sizes.size(); ++i) {
        size_t payload_len = kFuAHeaderSize + fragment_sizes[i];
        rtp_packet* packet = make_rtp_packet(ext, payload_len);
        uint8_t* payload   = packet->get_payload();

        uint8_t fu_indicator = (naul_header & (kFBit | kNriMask)) | NaluType::kFuA;

        uint8_t fu_header = 0;
        // S | E | R | 5 bit type.
        fu_header |= (i == 0 ? kSBit : 0);
        fu_header |= (i == (fragment_sizes.size() - 1) ? kEBit : 0);

        uint8_t type = naul_header & kTypeMask;
        fu_header |= type;

        payload[0] = fu_indicator;
        payload[1] = fu_header;
        memcpy(payload + kFuAHeaderSize, fragment, fragment_sizes[i]);

        fragment += fragment_sizes[i];

        packet->set_payload_length(kFuAHeaderSize + fragment_sizes[i]);
        packet->set_marker(i == (fragment_sizes.size() - 1));
        packets.push_back(packet);
    }
    return packets;
}

rtp_packet* generate_singlenalu_packets(uint8_t* data, size_t len, header_extension* ext) {
    rtp_packet* packet = make_rtp_packet(ext, len);

    uint8_t* payload = packet->get_payload();

    memcpy(payload, data, len);
    packet->set_payload_length(len);
    return packet;
}

std::vector<int> split_nalu(int payload_len) {
    std::vector<int> payload_sizes;
    size_t payload_left = payload_len;

    while (payload_left > 0) {
        if (payload_left > kPayloadMaxSize) {
            payload_sizes.push_back(kPayloadMaxSize);
            payload_left -= kPayloadMaxSize;
        }
        else {
            payload_sizes.push_back(payload_left);
            payload_left = 0;
        }
    }
    return payload_sizes;
}


