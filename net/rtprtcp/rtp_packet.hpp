#ifndef RTP_PACKET_HPP
#define RTP_PACKET_HPP
#include "rtprtcp_pub.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <arpa/inet.h>
#include <map>

typedef struct header_extension_s
{
    uint16_t id;
    uint16_t length;
    uint8_t  value[1];
} header_extension;

typedef struct onebyte_extension_s {
    uint8_t len : 4;
    uint8_t id  : 4;
    uint8_t value[1];
} onebyte_extension;

typedef struct twobytes_extension_s {
    uint8_t id  : 8;
    uint8_t len : 8;
    uint8_t value[1];
} twobytes_extension;

class rtp_packet
{
public:
    rtp_packet(rtp_common_header* header, header_extension* ext,
            uint8_t* payload, size_t payload_len,
            uint8_t pad_len, size_t data_len);
    ~rtp_packet();

public:
    uint8_t version() {return this->header->version;}
    bool has_padding() {return (this->header->padding == 1) ? true : false;}
    bool has_extension() {return (this->header->extension == 1) ? true : false;}
    uint8_t csrc_count() {return this->header->csrc_count;}
    uint8_t get_payload_type() {return this->header->payload_type;}
    uint8_t get_mpayload_type() {
        uint8_t marker = this->header->marker;
        return (marker << 7) | this->header->payload_type;
    }
    uint8_t get_marker() {return this->header->marker;}
    uint16_t get_seq() {return ntohs(this->header->sequence);}
    uint32_t get_timestamp() {return ntohl(this->header->timestamp);}
    uint32_t get_ssrc() {return ntohl(this->header->ssrc);}

    uint8_t* get_data() {return (uint8_t*)this->header;}
    size_t get_data_length() {return data_len;}

    uint8_t* get_payload() {return this->payload;}
    size_t get_payload_length() {return this->payload_len;}

    int64_t get_local_ms() {return this->local_ms;}

    std::string dump();

public:
    static rtp_packet* parse(uint8_t* data, size_t len);

private:
    void parse_ext();
    void parse_onebyte_ext();
    void parse_twobytes_ext();
    uint16_t get_ext_id(header_extension* rtp_ext);
    uint16_t get_ext_length(header_extension* rtp_ext);
    uint8_t* get_ext_value(header_extension* rtp_ext);
    bool has_onebyte_ext(header_extension* rtp_ext);
    bool has_twebytes_ext(header_extension* rtp_ext);

private:
    rtp_common_header* header = nullptr;
    header_extension* ext     = nullptr;
    uint8_t* payload          = nullptr;
    size_t payload_len        = 0;
    uint8_t pad_len           = 0;
    size_t data_len           = 0;
    int64_t local_ms          = 0;

private:
    std::map<uint8_t, onebyte_extension*>  onebyte_ext_map_;
    std::map<uint8_t, twobytes_extension*> twobytes_ext_map_;
};
#endif