#ifndef MPEGTS_DEMUX_H
#define MPEGTS_DEMUX_H
#include "mpegts_pub.hpp"
#include "av_format_interface.hpp"
#include "data_buffer.hpp"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

class mpegts_demux {
public:
    mpegts_demux();
    ~mpegts_demux();

    int decode(DATA_BUFFER_PTR data_ptr, av_format_callback* callback);

private:
    int decode_unit(unsigned char* data_p, av_format_callback* callback);
    bool is_pmt(unsigned short pmt_id);
    int pes_parse(unsigned char* p, size_t npos, unsigned char** ret_pp, size_t& ret_size,
            uint64_t& dts, uint64_t& pts);
    void insert_into_databuf(unsigned char* data_p, size_t data_size, unsigned short pid);
    void on_callback(av_format_callback* callback, unsigned short pid, uint64_t dts, uint64_t pts);

private:
    pat_info _pat;
    pmt_info _pmt;
    std::vector<DATA_BUFFER_PTR> _data_buffer_vec;
    size_t _data_total;
    unsigned short _last_pid;
    uint64_t _last_dts;
    uint64_t _last_pts;
};

typedef std::shared_ptr<mpegts_demux> TS_DEMUX_PTR;

#endif
