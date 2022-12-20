#include "ws_format.hpp"

websocket_frame::websocket_frame()
{
}

websocket_frame::~websocket_frame()
{
}

int websocket_frame::parse(uint8_t* data, size_t len) {
    buffer_.append_data((char*)data, len);

    if (buffer_.data_len() < 2) {
        return 1;
    }
    
    if (!header_ready_) {
        header_ = (WS_PACKET_HEADER*)buffer_.data();
        payload_len_ = header_->payload_len;
        payload_start_ = 2;
    
        if (header_->payload_len == 126 || header_->payload_len == 127) {
            if (header_->payload_len == 127) {
                int64_t ext_len = 0;
    
                if (buffer_.data_len() < 10) {
                    return 1;
                }
    
                ext_len |= ((int64_t)(header_->ext_payload_len_high)) << 32;
                ext_len |= header_->ext_payload_len_low;
    
                payload_len_ = ext_len;
                payload_start_ += 8;
            } else {
                if (buffer_.data_len() < 4) {
                    return 1;
                }
                payload_len_ = *(uint16_t*)(buffer_.data() + payload_start_);
                payload_start_ += 2;
            }
        }

        if (header_->mask) {
            if (buffer_.data_len() < (payload_start_ + 4)) {
                return 1;
            }
            header_->masking_key = (char*)data + payload_start_;
            payload_start_ += 4;
        }
        header_ready_ = true;
    }
    
    if (buffer_.data_len() < (payload_start_ + payload_len_)) {
        return 1;
    }

    if (header_->mask) {
        size_t frameLength = header_->payload_len;
        char* p = buffer_.data() + payload_start_;

        size_t temp_len = frameLength & ~3;
		for(size_t i = 0; i < temp_len; i += 4){
			p[i + 0] ^= header_->masking_key[0];
			p[i + 1] ^= header_->masking_key[1];
			p[i + 2] ^= header_->masking_key[2];
			p[i + 3] ^= header_->masking_key[3];
		}
		
		for(size_t i = temp_len; i < frameLength; ++i){
			p[i] ^= header_->masking_key[i % 4];
		}
	}

    return 0;
}