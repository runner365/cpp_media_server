#ifndef NACK_GENERATOR_HPP
#define NACK_GENERATOR_HPP
#include "net/rtprtcp/rtprtcp_pub.hpp"
#include "net/rtprtcp/rtp_packet.hpp"
#include "timer.hpp"
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <map>

#define NACK_LIST_MAX        2000
#define NACK_DEFAULT_TIMEOUT 10//ms
#define NACK_RETRY_MAX       20
#define NACK_DEFAULT_RTT     20//ms

class NACK_INFO
{
public:
    NACK_INFO(uint16_t seq, int64_t sent_ms, int retry)
    {
        this->seq     = seq;
        this->sent_ms = sent_ms;
        this->retry   = retry;
    }
    ~NACK_INFO()
    {
    }

public:
    uint16_t seq;
    int64_t sent_ms;
    int retry = 0;
};

class nack_generator_callback_interface
{
public:
    virtual void generate_nacklist(const std::vector<uint16_t>& seq_vec) = 0;
};

class nack_generator : public timer_interface
{
public:
    nack_generator(nack_generator_callback_interface* cb);
    ~nack_generator();

    void update_nacklist(rtp_packet* pkt);
    void update_rtt(int64_t rtt) {rtt_ = rtt;};

protected:
    virtual void on_timer() override;

private:
    nack_generator_callback_interface* cb_ = nullptr;
    bool init_flag_ = false;
    uint16_t last_seq_ = 0;
    std::map<uint16_t, NACK_INFO> nack_map_;
    int64_t rtt_ = NACK_DEFAULT_RTT;
};

#endif