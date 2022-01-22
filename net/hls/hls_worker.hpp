#ifndef HLS_WORKER_HPP
#define HLS_WORKER_HPP
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>

#include "media_packet.hpp"
#include "mpegts_handle.hpp"
#include "http_server.hpp"
#include "timer.hpp"

class hls_worker : public timer_interface
{
public:
    hls_worker(boost::asio::io_context& io_context, uint16_t port);
    virtual ~hls_worker();

public:
    void run();
    void stop();
    void set_path(const std::string& path) { path_ = path; }
    void set_rec_enable(bool enable) { rec_enable_ = enable; }
    void insert_packet(MEDIA_PACKET_PTR pkt_ptr);
    std::shared_ptr<mpegts_handle> get_mpegts_handle(const std::string& key);

protected:
    virtual void on_timer() override;

private:
    void on_work();
    void on_handle_packet(MEDIA_PACKET_PTR pkt_ptr);
    void check_timeout();
    std::shared_ptr<mpegts_handle> get_mpegts_handle(MEDIA_PACKET_PTR pkt_ptr);

private:
    bool run_flag_ = false;
    std::shared_ptr<std::thread> run_thread_ptr_;

private:
    std::string path_;
    bool rec_enable_ = false;
    std::map<std::string, std::shared_ptr<mpegts_handle>> mpegts_handles_;

private:
    boost::asio::io_context& io_context_;
};

#endif