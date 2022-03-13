#ifndef RTP_PACKET_HPP
#define RTP_PACKET_HPP
#include "rtprtcp_pub.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <arpa/inet.h>
#include <map>

#define RTP_SEQ_MOD (1<<16)

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

/**
    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                           timestamp                           |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |           synchronization source (SSRC) identifier            |
   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
   |            contributing source (CSRC) identifiers             |
   |                             ....                              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |      defined by profile       |           length              |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |                        header extension                       |
   |                             ....                              |
 */

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
    void set_padding(bool flag) {this->header->padding = flag ? 1 : 0;}
    bool has_extension() {return (this->header->extension == 1) ? true : false;}
    uint8_t csrc_count() {return this->header->csrc_count;}
    uint8_t get_payload_type() {return this->header->payload_type;}
    void set_payload_type(uint8_t type) {this->header->payload_type = type;}
    uint8_t get_mpayload_type() {
        uint8_t marker = this->header->marker;
        return (marker << 7) | this->header->payload_type;
    }
    uint8_t get_marker() {return this->header->marker;}
    void set_marker(uint8_t marker) { this->header->marker = marker; }
    uint16_t get_seq() {return ntohs(this->header->sequence);}
    void set_seq(uint16_t seq) {this->header->sequence = htons(seq);}
    uint32_t get_timestamp() {return ntohl(this->header->timestamp);}
    void set_timestamp(uint32_t ts) { this->header->timestamp = (uint32_t)htonl(ts); }
    uint32_t get_ssrc() {return ntohl(this->header->ssrc);}
    void set_ssrc(uint32_t ssrc) {this->header->ssrc = (uint32_t)htonl(ssrc);}

    uint8_t* get_data() {return (uint8_t*)this->header;}
    size_t get_data_length() {return data_len;}

    uint8_t* get_payload() {return this->payload;}
    size_t get_payload_length() {return this->payload_len;}
    void set_payload_length(size_t len) { this->payload_len = len; }

    void set_mid_extension_id(uint8_t id) { mid_extension_id_ = id; }
    uint8_t get_mid_extension_id() { return mid_extension_id_; }

    void set_abs_time_extension_id(uint8_t id) { abs_time_extension_id_ = id; }
    uint8_t get_abs_time_extension_id() { return abs_time_extension_id_; }

    bool update_mid(uint8_t mid);
    bool read_mid(uint8_t& mid);

    bool read_abs_time(uint32_t& abs_time_24bits);
    bool update_abs_time(uint32_t abs_time_24bits);

    void set_need_delete(bool flag) { this->need_delete = false; }

    int64_t get_local_ms() {return this->local_ms;}

    void rtx_demux(uint32_t ssrc, uint8_t payloadtype);

    std::string dump();

public:
    static rtp_packet* parse(uint8_t* data, size_t len);
    rtp_packet* clone();

private:
    void parse_ext();
    void parse_onebyte_ext();
    void parse_twobytes_ext();
    uint16_t get_ext_id(header_extension* rtp_ext);
    uint16_t get_ext_length(header_extension* rtp_ext);
    uint8_t* get_ext_value(header_extension* rtp_ext);
    bool has_onebyte_ext(header_extension* rtp_ext);
    bool has_twebytes_ext(header_extension* rtp_ext);

    uint8_t* get_extension(uint8_t id, uint8_t& len);

    bool update_extension_length(uint8_t id, uint8_t len);
    
private:
    rtp_common_header* header = nullptr;
    header_extension* ext     = nullptr;
    uint8_t* payload          = nullptr;
    size_t payload_len        = 0;
    uint8_t pad_len           = 0;
    size_t data_len           = 0;
    int64_t local_ms          = 0;
    bool need_delete          = false;

private:
    uint8_t mid_extension_id_      = 0;
    uint8_t abs_time_extension_id_ = 0;

private:
    std::map<uint8_t, onebyte_extension*>  onebyte_ext_map_;
    std::map<uint8_t, twobytes_extension*> twobytes_ext_map_;
};
#endif