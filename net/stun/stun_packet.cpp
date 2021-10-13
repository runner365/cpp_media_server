#include "stun_packet.hpp"
#include "logger.hpp"
#include "byte_stream.hpp"
#include "byte_crypto.hpp"
#include "ipaddress.hpp"
#include <stdint.h>
#include <sstream>
#include <cstring>

//The magic cookie field MUST contain the fixed value 0x2112A442
const uint8_t stun_packet::magic_cookie[] = { 0x21, 0x12, 0xA4, 0x42 };

/*
   rfc: https://datatracker.ietf.org/doc/html/rfc5389

   All STUN messages MUST start with a 20-byte header followed by zero
   or more Attributes.  The STUN header contains a STUN message type,
   magic cookie, transaction ID, and message length.

       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |0 0|     STUN Message Type     |         Message Length        |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                         Magic Cookie                          |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                                                               |
      |                     Transaction ID (96 bits)                  |
      |                                                               |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

bool stun_packet::is_stun(const uint8_t* data, size_t len) {
    if ((len >= STUN_HEADER_SIZE) && (data[0] < 3) &&
        (data[4] == stun_packet::magic_cookie[0]) && (data[5] == stun_packet::magic_cookie[1]) &&
        (data[6] == stun_packet::magic_cookie[2]) && (data[7] == stun_packet::magic_cookie[3])) {
        return true;
    } 

    return false;
}

stun_packet* stun_packet::parse(const uint8_t* data, size_t len) {
    if (!stun_packet::is_stun(data, len)) {
        MS_THROW_ERROR("it's not a stun packet");
    }

/*
   The message type field is decomposed further into the following
   structure:
                 0                 1
                 2  3  4 5 6 7 8 9 0 1 2 3 4 5

                +--+--+-+-+-+-+-+-+-+-+-+-+-+-+
                |M |M |M|M|M|C|M|M|M|C|M|M|M|M|
                |11|10|9|8|7|1|6|5|4|0|3|2|1|0|
                +--+--+-+-+-+-+-+-+-+-+-+-+-+-+

                Figure 3: Format of STUN Message Type Field
*/
    const uint8_t* p = data;
    uint16_t msg_type = read_2bytes(p);
    p += 2;
    uint16_t msg_len  = read_2bytes(p);
    p += 2;

    if (((size_t)msg_len != (len - 20)) || ((msg_len & 0x03) != 0)) {
        MS_THROW_ERROR("stun packet message len(%d) error", msg_len);
    }

    stun_packet* ret_packet = new stun_packet(data, len);

    uint16_t msg_method = (msg_type & 0x000f) | ((msg_type & 0x00e0) >> 1) | ((msg_type & 0x3E00) >> 2);
    ret_packet->stun_method = static_cast<STUN_METHOD_ENUM>(msg_method);

    uint16_t msg_class = ((data[0] & 0x01) << 1) | ((data[1] & 0x10) >> 4);
    ret_packet->stun_class  = static_cast<STUN_CLASS_ENUM>(msg_class);

    p += 4;//skip Magic Cookie(4bytes)
    memcpy(ret_packet->transaction_id, p, sizeof(ret_packet->transaction_id));
    p += 12;//skip Transaction ID (12bytes)
    
    bool has_fingerprint = false;
    bool has_message_integrity = false;
    size_t fingerprint_pos   = 0;

    while ((p + 4) < (data + len)) {
        STUN_ATTRIBUTE_ENUM attr_type = static_cast<STUN_ATTRIBUTE_ENUM>(read_2bytes(p));
        p += 2;
        uint16_t attr_len = read_2bytes(p);
        p += 2;

        if ((p + attr_len) > (data + len)) {
            delete ret_packet;
            MS_THROW_ERROR("stun packet attribute length(%d) is too long", attr_len);
        }

        if (has_fingerprint) {
            delete ret_packet;
            MS_THROW_ERROR("stun packet attribute fingerprint must be the last one");
        }

        if (has_message_integrity && (attr_type != STUN_FINGERPRINT))
        {
            delete ret_packet;
            MS_THROW_ERROR("fingerprint is only allowed after message integrity attribute.");
        }

        const uint8_t* attr_data = p;
        size_t skip_len = (size_t)pad_to_4bytes((uint16_t)(attr_len));
        p += skip_len;

        switch (attr_type)
        {
            case STUN_USERNAME:
            {
                ret_packet->user_name.assign((char*)attr_data, attr_len);
                break;
            }
            case STUN_PRIORITY:
            {
                if (attr_len != 4) {
                    delete ret_packet;
                    MS_THROW_ERROR("stun attribute priority len(%d) is not 4", attr_len);
                }
                ret_packet->priority = read_4bytes(attr_data);
                break;
            }
            case STUN_ICE_CONTROLLING:
            {
                if (attr_len != 8) {
                    delete ret_packet;
                    MS_THROW_ERROR("stun attribute icecontrolling len(%d) is not 8", attr_len);
                }
                ret_packet->ice_controlling = read_8bytes(attr_data);
                break;
            }
            case STUN_ICE_CONTROLLED:
            {
                if (attr_len != 8) {
                    delete ret_packet;
                    MS_THROW_ERROR("stun attribute icecontrolled len(%d) is not 8", attr_len);
                }
                ret_packet->ice_controlled = read_8bytes(attr_data);
                break;
            }
            case STUN_USE_CANDIDATE:
            {
                if (attr_len != 0) {
                    delete ret_packet;
                    MS_THROW_ERROR("stun attribute use candidate len(%d) is not 0", attr_len);
                }
                ret_packet->has_use_candidate = true;
                break;
            }
            case STUN_MESSAGE_INTEGRITY:
            {
                if (attr_len != 20) {
                    delete ret_packet;
                    MS_THROW_ERROR("stun attribute message integrity len(%d) is not 20", attr_len);
                }
                has_message_integrity = true;
                ret_packet->message_integrity = attr_data;
                break;
            }
            case STUN_FINGERPRINT:
            {
                if (attr_len != 4) {
                    delete ret_packet;
                    MS_THROW_ERROR("stun attribute fingerprint len(%d) is not 4", attr_len);
                }
                has_fingerprint = true;
                ret_packet->fingerprint = read_4bytes(attr_data);
                fingerprint_pos = (attr_data - 4) - data;
                break;
            }
            case STUN_ERROR_CODE:
            {
                if (attr_len != 4) {
                    delete ret_packet;
                    MS_THROW_ERROR("stun attribute error code len(%d) is not 4", attr_len);
                }
                attr_data += 2;
                uint8_t error_class = *attr_data;
                attr_data++;
                uint8_t error_number = *attr_data;
                ret_packet->error_code = (uint16_t)(error_class * 100 + error_number);
                break;
            }
            default:
            {
            }
        }
    }
    
    if (p != (data + len)) {
        delete ret_packet;
        MS_THROW_ERROR("data offset(%p) is not data end(%p)", p, data + len);
    }
/*
15.5.  FINGERPRINT
   The FINGERPRINT attribute MAY be present in all STUN messages.  The
   value of the attribute is computed as the CRC-32 of the STUN message
   up to (but excluding) the FINGERPRINT attribute itself, XOR'ed with
   the 32-bit value 0x5354554e (the XOR helps in cases where an
   application packet is also using CRC-32 in it)
*/
    if (has_fingerprint) {
        uint32_t caculate_fingerprint = byte_crypto::get_crc32(data, fingerprint_pos);
        caculate_fingerprint ^= 0x5354554e;
        if (ret_packet->fingerprint != caculate_fingerprint) {
            delete ret_packet;
            MS_THROW_ERROR("fingerprint(%u) is not equal caculate fingerprint(%u)",
                ret_packet->fingerprint, caculate_fingerprint);
        }
        ret_packet->has_fingerprint = true;
    }
    return ret_packet;
}

stun_packet::stun_packet() {
    std::memset(this->data, 0, sizeof(this->data));
    this->origin_data = this->data;
}

stun_packet::stun_packet(const uint8_t* data, size_t len) {
    if (len > sizeof(this->data)) {
        MS_THROW_ERROR("the input packet len(%lu) is larger than stun packet max len(%lu)",
            len, sizeof(this->data));
    }
    memcpy(this->data, data, len);
    this->origin_data = const_cast<uint8_t*>(data);
    this->data_len = len;
}

stun_packet::~stun_packet() {

}

void stun_packet::serialize() {
    bool add_msg_integrity = (this->stun_class != STUN_CLASS_ENUM::STUN_ERROR_RESPONSE && !this->password.empty());
    bool add_xor_address = (this->xor_address != nullptr) &&
                        (this->stun_method == STUN_METHOD_ENUM::BINDING) &&
                        (this->stun_class == STUN_CLASS_ENUM::STUN_SUCCESS_RESPONSE);
    bool add_error = ((this->error_code != 0) && this->stun_class == STUN_CLASS_ENUM::STUN_ERROR_RESPONSE);
    uint16_t user_name_pad_len = (uint16_t)pad_to_4bytes((uint16_t)(this->user_name.length()));
    /*the origin data come from incomming stun packet*/
    this->data_len = STUN_HEADER_SIZE;//init stun data size

    if (!this->user_name.empty()) {
        this->data_len += 4 + user_name_pad_len;
    }

    if (this->priority) {
        this->data_len += 4 + 4;
    }

    if (this->ice_controlling) {
        this->data_len += 4 + 8;
    }

    if (this->ice_controlled) {
        this->data_len += 4 + 8;
    }

    if (this->has_use_candidate) {
        this->data_len += 4;
    }

    if (add_xor_address) {
        if (this->xor_address->sa_family == AF_INET) {
            this->data_len += 4 + 8;
        }
    }

    if (add_error) {
        this->data_len += 4 + 4;
    }

    if (add_msg_integrity) {
        this->data_len += 4 + 20;
    }

    //add fingerprint always
    this->data_len += 4 + 4;

    uint8_t* p = this->data;

    uint16_t type_field = ((uint16_t)(this->stun_method) & 0x0f80) << 2;

    type_field |= ((uint16_t)(this->stun_method) & 0x0070) << 1;
    type_field |= ((uint16_t)(this->stun_method) & 0x000f);
    type_field |= ((uint16_t)(this->stun_class) & 0x02) << 7;
    type_field |= ((uint16_t)(this->stun_class) & 0x01) << 4;

    
    write_2bytes(p, type_field);
    p += 2;
    write_2bytes(p, (uint16_t)(this->data_len - STUN_HEADER_SIZE));
    p += 2;

    //wait to write data len
    memcpy(p, stun_packet::magic_cookie, 4);
    p += 4;
    memcpy(p, this->transaction_id, 12);
    p += 12;

    //start set attributes
    if (!this->user_name.empty()) {
        write_2bytes(p, (uint16_t)STUN_USERNAME);
        p += 2;
        write_2bytes(p, (uint16_t)(this->user_name.length()));
        p += 2;
        memcpy(p, this->user_name.c_str(), this->user_name.length());
        p += user_name_pad_len;
    }

    if (this->priority) {
        write_2bytes(p, (uint16_t)STUN_PRIORITY);
        p += 2;
        write_2bytes(p, 4);
        p += 2;
        write_4bytes(p, this->priority);
        p += 4;
    }

    if (this->ice_controlling) {
        write_2bytes(p, (uint16_t)STUN_ICE_CONTROLLING);
        p += 2;
        write_2bytes(p, 8);
        p += 2;
        write_8bytes(p, this->ice_controlling);
        p += 8;
    }

    if (this->ice_controlled) {
        write_2bytes(p, (uint16_t)STUN_ICE_CONTROLLED);
        p += 2;
        write_2bytes(p, 8);
        p += 2;
        write_8bytes(p, this->ice_controlled);
        p += 8;
    }

    if (this->has_use_candidate) {
        write_2bytes(p, (uint16_t)STUN_USE_CANDIDATE);
        p += 2;
        write_2bytes(p, 0);
        p += 2;
    }

    if (add_xor_address) {
        if (this->xor_address->sa_family == AF_INET) {
            write_2bytes(p, (uint16_t)STUN_XOR_MAPPED_ADDRESS);
            p += 2;
            write_2bytes(p, (uint16_t)8);//AF_INET pad size is 8
            p += 2;

            p[0] = 0;
            p[1] = 0x01;//inet family

            memcpy(p + 2,
                &((sockaddr_in*)(this->xor_address))->sin_port,
                2);
            p[2] ^= stun_packet::magic_cookie[0];
            p[3] ^= stun_packet::magic_cookie[1];

            memcpy(p + 4,
                &((sockaddr_in*)(this->xor_address))->sin_addr.s_addr,
                4);
            p[4] ^= stun_packet::magic_cookie[0];
            p[5] ^= stun_packet::magic_cookie[1];
            p[6] ^= stun_packet::magic_cookie[2];
            p[7] ^= stun_packet::magic_cookie[3];
            p += 8;
        }
    }

    if (add_error) {
        write_2bytes(p, STUN_ERROR_CODE);
        p += 2;
        write_2bytes(p, 4);
        p += 2;

        uint8_t code_class  = (uint8_t)(this->error_code / 100);
        uint8_t code_number = (uint8_t)(this->error_code) - (code_class * 100);
        write_2bytes(p, 0);
        p += 2;
        *p = code_class;
        p++;
        *p = code_number;
        p++;
    }

    if (add_msg_integrity) {
        size_t pos = p - this->data;

        //reset for ignore fingerprint
        //subtract message integrity and fingerprint part
        write_2bytes(this->data + 2, (uint16_t)(this->data_len - 20 - 8));

        uint8_t* caculate_msg_integrity = byte_crypto::get_hmac_sha1(this->password, this->data, pos);

        write_2bytes(p, STUN_MESSAGE_INTEGRITY);
        p += 2;
        write_2bytes(p, 20);
        p += 2;
        this->message_integrity = p;
        memcpy(p, caculate_msg_integrity, 20);
        p += 20;

        //recover the message length
        write_2bytes(this->data + 2, (uint16_t)(this->data_len - STUN_HEADER_SIZE));
    } else {
        this->message_integrity = nullptr;
    }

    do {//set fingerprint
        size_t pos = p - this->data;
        uint32_t caculate_fingerprint = byte_crypto::get_crc32(this->data, pos) ^ 0x5354554e;

        write_2bytes(p, STUN_FINGERPRINT);
        p += 2;
        write_2bytes(p, 4);
        p += 2;
        write_4bytes(p, caculate_fingerprint);
        p += 4;

        this->has_fingerprint = true;
    } while(0);

    if (p != (this->data + this->data_len)) {
        MS_THROW_ERROR("the data end pos(%p) isn't equal to the data(%p) + size(%lu)",
            p, this->data, this->data_len);
    }
    return;
}

STUN_AUTHENTICATION stun_packet::check_auth(const std::string& local_username, const std::string& local_pwd) {
    switch(this->stun_class)
    {
        case STUN_REQUEST:
        case STUN_INDICATION:
        {
            if (this->user_name.empty()) {
                return BAD_REQUEST;
            }

            size_t local_usernameLen = local_username.length();
            if (this->user_name.length() <= local_usernameLen || this->user_name.at(local_usernameLen) != ':' ||
              (this->user_name.compare(0, local_usernameLen, local_username) != 0)) {
                return UNAUTHORIZED;
            }
            break;
        }
        case STUN_SUCCESS_RESPONSE:
        case STUN_ERROR_RESPONSE:
        {
            return BAD_REQUEST; 
        }
    }

    if (this->has_fingerprint) {
        write_2bytes(this->origin_data + 2, (uint16_t)this->data_len - 20 - 8);
    }

    uint8_t* caculate_msg_integrity = byte_crypto::get_hmac_sha1(local_pwd, this->origin_data,
                                            this->message_integrity - 4 - this->origin_data);
    STUN_AUTHENTICATION ret;
    if (memcmp(caculate_msg_integrity, this->message_integrity, 20) == 0) {
        ret = STUN_AUTHENTICATION::OK;
    } else {
        ret = STUN_AUTHENTICATION::UNAUTHORIZED;
        log_info_data(caculate_msg_integrity, 20, "caculate msg integrity");
        log_info_data(this->message_integrity, 20, "msg integrity");
    }
    
    //recover the message length
    write_2bytes(this->origin_data + 2, (uint16_t)this->data_len - 20);
    return ret;
}

stun_packet* stun_packet::create_success_response() {
    stun_packet* pkt = new stun_packet();

    pkt->stun_class = STUN_SUCCESS_RESPONSE;
    pkt->stun_method = this->stun_method;
    memcpy(pkt->transaction_id, this->transaction_id, sizeof(pkt->transaction_id));

    pkt->error_code = 0;

    return pkt;
}

stun_packet* stun_packet::create_error_response(uint16_t err_code) {
    stun_packet* pkt = new stun_packet();

    pkt->stun_class = STUN_ERROR_RESPONSE;
    pkt->stun_method = this->stun_method;
    pkt->password    = this->password;
    memcpy(pkt->transaction_id, this->transaction_id, sizeof(pkt->transaction_id));

    pkt->error_code = err_code;

    return pkt;
}

std::string stun_packet::dump() {
    std::stringstream ss;

    ss << "stun packet:\r\n";
    ss << "  class:" << this->stun_class << ", method:" << this->stun_method << "\r\n";
    ss << "  data length:" << this->data_len << "\r\n";

    char transactionid_sz[64];
    int str_len = 0;

    for (size_t i = 0; i < sizeof(this->transaction_id); i++) {
        str_len += snprintf(transactionid_sz + str_len, sizeof(transactionid_sz) - str_len,
                        " %.2x", this->transaction_id[i]);
    }
    ss << "  transaction id:" << transactionid_sz << "\r\n";
    
    ss << "  username:" << this->user_name << "\r\n";
    ss << "  priority:" << this->priority << "\r\n";
    ss << "  ice_controlling:" << this->ice_controlling << "\r\n";
    ss << "  ice_controlled:" << this->ice_controlled << "\r\n";
    ss << "  fingerprint:" << this->fingerprint << "\r\n";
    ss << "  error_code:" << this->error_code << "\r\n";

    if (this->message_integrity) {
        char message_integrity_sz[64];
        str_len = 0;
        for (int i = 0; i < 20; i++) {
            str_len += snprintf(message_integrity_sz + str_len, 20 - str_len,
                        "%.2x", this->message_integrity[i]);
        }
        ss << "  message_integrity:" << message_integrity_sz << "\r\n";
    }

    ss << "  has_use_candidate:" << this->has_use_candidate << "\r\n";

    if (this->xor_address) {
        uint16_t port = 0;
        std::string ip_str = get_ip_str(this->xor_address, port);
        ss << "  xor_address:" <<  (int)(this->xor_address->sa_family) << " "<< ip_str << ":" << port << "\r\n";
    }
    return ss.str();
}