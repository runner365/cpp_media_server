#include "hls_writer.hpp"

hls_writer::hls_writer(boost::asio::io_context& io_context, uint16_t port,
                    const std::string& path, bool rec_enable):work_(io_context, port)
{
    work_.set_path(path);
    work_.set_rec_enable(rec_enable);
}

hls_writer::~hls_writer()
{

}

void hls_writer::run() {
    work_.run();
}

int hls_writer::write_packet(MEDIA_PACKET_PTR pkt_ptr) {
    work_.insert_packet(pkt_ptr);
    return 0;
}

std::string hls_writer::get_key() {
    //ignore
    return "";
}

std::string hls_writer::get_writerid() {
    //ignore
    return "";
}

void hls_writer::close_writer() {
    //ignore
    return;
}

bool hls_writer::is_inited() {
    //ignore
    return true;
}

void hls_writer::set_init_flag(bool flag) {
    //ignore
    return;
}
