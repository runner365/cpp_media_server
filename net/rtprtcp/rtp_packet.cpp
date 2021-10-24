#include "rtp_packet.hpp"
#include "rtprtcp_pub.hpp"
#include "logger.hpp"
#include "timeex.hpp"
#include <arpa/inet.h>
#include <sstream>
#include <cstring>

rtp_packet* rtp_packet::parse(uint8_t* data, size_t len) {
    rtp_common_header* header = (rtp_common_header*)data;
    header_extension* ext = nullptr;
    uint8_t* p = (uint8_t*)(header + 1);

    if (len > RTP_PACKET_MAX_SIZE) {
        MS_THROW_ERROR("rtp len(%lu) is to large", len);
    }

    if (header->csrc_count > 0) {
        p += 4 * header->csrc_count;
    }

    if (header->extension) {
        if (len < (size_t)(p - data + 4)) {
            MS_THROW_ERROR("rtp len(%lu) is to small", len);
        }
        ext = (header_extension*)p;
        size_t extension_byte = (size_t)(ntohs(ext->length) * 4);
        if (len < (size_t)(p - data + 4 + extension_byte)) {
            MS_THROW_ERROR("rtp len(%lu) is to small", len);
        }
        p += 4 + extension_byte;//4bytes(externsion header) + extersion bytes
    }

    if (len <= (size_t)(p - data)) {
        MS_THROW_ERROR("rtp len(%lu) is to small, has no payload", len);
    }
    uint8_t* payload = p;
    size_t payload_len = len - (size_t)(p - data);
    uint8_t pad_len = 0;

    
    if (header->padding) {
        pad_len = data[len - 1];
        if (pad_len > 0) {
            if (payload_len <= pad_len) {
                MS_THROW_ERROR("rtp payload length(%lu), pad length(%d) error",
                        payload_len, pad_len);
            }
            payload_len -= pad_len;
        }
    }

    rtp_packet* pkt = new rtp_packet(header, ext, payload, payload_len, pad_len, len);

    return pkt;
}

rtp_packet::rtp_packet(rtp_common_header* header, header_extension* ext,
                uint8_t* payload, size_t payload_len,
                uint8_t pad_len, size_t data_len) {
    if (data_len > RTP_PACKET_MAX_SIZE) {
        MS_THROW_ERROR("rtp len(%lu) is to large", data_len);
    }
    this->header      = header;
    this->ext         = ext;
    this->payload     = payload;
    this->payload_len = payload_len;
    this->pad_len     = pad_len;

    this->data_len    = data_len;

    this->local_ms    = (int64_t)now_millisec();

    this->parse_ext();
}

rtp_packet::~rtp_packet() {

}

std::string rtp_packet::dump() {
    std::stringstream ss;
    char desc[128];

    snprintf(desc, sizeof(desc), "%p", this->get_data());

    ss << "rtp packet data:" << desc << ", data length:" << this->data_len << "\r\n";
    ss << "  version:" << (int)this->version() << ", padding:" << this->has_padding();
    ss << ", extension:" << this->has_extension() << ", csrc count:" << (int)this->csrc_count() << "\r\n";
    ss << "  marker:" << (int)this->get_marker() << ", payload type:" << (int)this->get_payload_type() << "\r\n";
    ss << "  sequence:" << (int)this->get_seq() << ", timestamp:" << this->get_timestamp();
    ss << ", ssrc:" << this->get_ssrc() << "\r\n";

    snprintf(desc, sizeof(desc), "%p", this->get_payload());
    ss << "  payload:" << desc << ", payload length:" << this->get_payload_length() << "\r\n";

    if (this->has_padding()) {
        uint8_t* media_data = this->get_data();
        ss << "  padding len:" << media_data[this->data_len - 1] << "\r\n";
    }

    if (this->has_extension()) {
        if (!this->onebyte_ext_map_.empty()) {
            ss <<  "  rtp onebyte extension:" << "\r\n";
            for (auto item : this->onebyte_ext_map_) {
                onebyte_extension* item_ext = item.second;
                ss << "    id:" << (int)item.first << ", length:" << (int)item_ext->len << "\r\n";
            }
        }

        if (!this->twobytes_ext_map_.empty()) {
            ss << "  rtp twobytes extension:" << "\r\n";
            for ( auto item : this->twobytes_ext_map_) {
                twobytes_extension* item_ext = item.second;
                ss << "    id:" << (int)item.first << ", length:" << (int)item_ext->len << "\r\n";
            }
        }
    }
    return ss.str();
}

uint16_t rtp_packet::get_ext_id(header_extension* rtp_ext) {
    if (rtp_ext == nullptr) {
        return 0;
    }
    
    return ntohs(rtp_ext->id);
}

uint16_t rtp_packet::get_ext_length(header_extension* rtp_ext) {
    if (rtp_ext == nullptr) {
        return 0;
    }
    
    return ntohs(rtp_ext->length) * 4;
}

uint8_t* rtp_packet::get_ext_value(header_extension* rtp_ext) {
    if (rtp_ext == nullptr) {
        return 0;
    }
    
    return rtp_ext->value;
}

void rtp_packet::parse_ext() {
    if ((this->header->extension == 0) || (this->ext == nullptr)) {
        return;
    }

    //base on rfc5285
    if (has_onebyte_ext(this->ext)) {
        parse_onebyte_ext();
    } else if (has_twebytes_ext(this->ext)) {
        parse_twobytes_ext();
    } else {
        MS_THROW_ERROR("the rtp extension id(%02x) error", this->ext->id);
    }
}

void rtp_packet::parse_onebyte_ext() {
    onebyte_ext_map_.clear();

    uint8_t* ext_start = (uint8_t*)(this->ext) + 4;//skip id(16bits) + length(16bits)
    uint8_t* ext_end   = ext_start + get_ext_length(this->ext);
    uint8_t* p = ext_start;

/*
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |       0xBE    |    0xDE       |           length=3            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |  ID   | L=0   |     data      |  ID   |  L=1  |   data...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            ...data   |    0 (pad)    |    0 (pad)    |  ID   | L=3   |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          data                                 |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
    while (p < ext_end) {
        uint8_t id = (*p & 0xF0) >> 4;
        size_t len = (size_t)(*p & 0x0F) + 1;

        if (id == 0x0f)
            break;

        if (id != 0) {
            if (p + 1 + len > ext_end) {
                MS_THROW_ERROR("rtp extension length(%d) is not enough in one byte extension mode.",
                    get_ext_length(this->ext));
                break;
            }

            onebyte_ext_map_[id] = (onebyte_extension*)p;
            p += (1 + len);
        }
        else {
            p++;
        }

        while ((p < ext_end) && (*p == 0))
        {
            p++;
        }
    }
}

void rtp_packet::parse_twobytes_ext() {
    twobytes_ext_map_.clear();

    uint8_t* ext_start = (uint8_t*)(this->ext) + 4;//skip id(16bits) + length(16bits)
    uint8_t* ext_end   = ext_start + get_ext_length(this->ext);
    uint8_t* p         = ext_start;

/*
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |       0x10    |    0x00       |           length=3            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |      ID       |     L=0       |     ID        |     L=1       |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |       data    |    0 (pad)    |       ID      |      L=4      |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                          data                                 |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    The 8-bit length field is the length of extension data in bytes not
    including the ID and length fields.  The value zero indicates there
    is no data following.
*/
    while (p + 1 < ext_end) {
        uint8_t id  = *p;
        uint8_t len = *(p + 1);

        if (id != 0) {
            if (p + 2 + len > ext_end) {
                MS_THROW_ERROR("rtp extension length(%d) is not enough in two bytes extension mode.",
                    get_ext_length(this->ext));
                break;
            }

            // Store the Two-Bytes extension element in the map.
            twobytes_ext_map_[id] = (twobytes_extension*)p;

            p += (2 + len);
        } else {
            ++p;
        }

        while ((p < ext_end) && (*p == 0)) {
            ++p;
        }
    }
}

bool rtp_packet::has_onebyte_ext(header_extension* rtp_ext) {
    return get_ext_id(rtp_ext) == 0xBEDE;
}

bool rtp_packet::has_twebytes_ext(header_extension* rtp_ext) {
/*
       0                   1
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |         0x100         |appbits|
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/
    return (get_ext_id(rtp_ext) & 0xfff0) == 0x1000;
}

void rtp_packet::rtx_demux(uint32_t ssrc, uint8_t payloadtype) {
    if (this->payload_len < 2) {
        MS_THROW_ERROR("rtx payload len(%lu) is less than 2", this->payload_len);
    }

    uint16_t replace_seq = ntohs(*(uint16_t*)(this->payload));
    set_payload_type(payloadtype);
    set_seq(replace_seq);
    set_ssrc(ssrc);

    std::memmove(this->payload, this->payload + 2, this->payload_len - 2);
    this->payload_len -= 2;
    this->data_len    -= 2;

    if (this->has_padding()) {
        set_padding(false);
        this->data_len -= this->pad_len;
        this->pad_len   = 0;
    }
}