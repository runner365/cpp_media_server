#ifndef STREAM_STATICS_HPP
#define STREAM_STATICS_HPP
#include <stdint.h>
#include <stddef.h>

#define STREAM_STATICS_INTERVAL_MAX (10*1000)

#define STREAM_STATICS_INTErVAL_DEFAULT (2*1000)

class stream_statics
{
public:
    stream_statics() {

    }
    ~stream_statics() {

    }

    void update(size_t bytes, int64_t now_ms) {
        bytes_ += bytes;
        count_ ++;
        if ((timestamp_ == 0) || ((now_ms - timestamp_) > STREAM_STATICS_INTERVAL_MAX)) {
            timestamp_  = now_ms;
            last_bytes_ = bytes_;
            last_count_ = count_;
        }
    }

    size_t get_count() {return count_;}
    size_t get_bytes() {return bytes_;}

    size_t bytes_per_second(int64_t now_ms, size_t& count_per_second) {
        if ((timestamp_ == 0) || ((now_ms - timestamp_) > STREAM_STATICS_INTERVAL_MAX)) {
            timestamp_  = now_ms;
            last_bytes_ = bytes_;
            last_count_ = count_;
            return 0;
        }
        
        if (now_ms <= timestamp_) {
            last_bytes_ = bytes_;
            last_count_ = count_;
            timestamp_  = now_ms;
            return 0;
        }

        size_t speed = (bytes_ - last_bytes_) * 1000 / (now_ms - timestamp_);
        count_per_second = (count_ - last_count_) * 1000 / (now_ms - timestamp_);

        last_bytes_ = bytes_;
        last_count_ = count_;
        timestamp_  = now_ms;

        return speed;
    }

    void reset() {
        count_      = 0;
        bytes_      = 0;
        last_bytes_ = 0;
        last_count_ = 0;
        timestamp_  = 0;
    }

private:
    size_t count_      = 0;
    size_t last_count_ = 0;
    size_t bytes_      = 0;
    size_t last_bytes_ = 0;
    int64_t timestamp_ = 0;
};

#endif