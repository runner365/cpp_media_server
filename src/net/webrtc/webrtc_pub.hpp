#ifndef WEBRTC_PUB_HPP
#define WEBRTC_PUB_HPP
#include <boost/asio.hpp>
#include <string>
#include <stdint.h>
#include <stddef.h>

void init_single_udp_server(boost::asio::io_context& io_context, const std::string& candidate_ip, uint16_t port);
//void dtls_init(const std::string& key_file, const std::string& cert_file);
void init_webrtc_stream_manager_callback();

#endif
