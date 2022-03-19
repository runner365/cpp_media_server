cmake_minimum_required(VERSION 3.7.1)
project(cpp_media_server)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wno-deprecated -Wno-deprecated-declarations -Wno-reorder -Wall -fexceptions -frtti -D__STDC_FORMAT_MACROS -fPIC")
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(BUILD_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(PREFIX_DIR "${BUILD_OUTPUT_BASE}")
set(INSTALL_RPATH "${PREFIX_DIR}/lib")

# set output static libary
SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/output/lib)

# set pkgconfig path
set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/output/lib/pkgconfig")

# set include path
include_directories(src
                    src/net
                    src/net/http
                    src/net/rtmp
                    src/net/websocket
                    src/net/websocket/wsimple
                    src/net/tcp
                    src/net/stun
                    src/net/httpflv
                    src/net/rtprtcp
                    src/net/hls
                    src/net/webrtc
                    src/net/webrtc/libwebrtc
                    src/format
                    src/format/flv
                    src/utils
                    src/utils/av
                    3rdparty/libsdptransform/include
                    ${CMAKE_BINARY_DIR}/output/include
                    ${CMAKE_BINARY_DIR}/output/include/srtp2
                    ${CMAKE_BINARY_DIR}/output/include/openssl
                    /usr/local/include)

# set lib path
#link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib64 /usr/local/lib)
link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib64 /usr/local/lib)
add_subdirectory(3rdparty)

add_executable(echo_demo
            src/net/tcp/echo_demo.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp)
IF (APPLE)
target_link_libraries(echo_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(echo_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()

add_executable(flv_demux_demo
            ./src/format/flv/flv_demux.cpp
            ./src/format/flv/flv_demux_demo.cpp
            ./src/utils/logger.cpp
            ./src/utils/data_buffer.cpp
            ./src/utils/byte_stream.cpp)
IF (APPLE)
target_link_libraries(flv_demux_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(flv_demux_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()

add_executable(flv_mux_demo
            src/format/flv/flv_mux.cpp
            src/format/flv/flv_demux.cpp
            src/format/flv/flv_mux_demo.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
IF (APPLE)
target_link_libraries(flv_mux_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(flv_mux_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()

add_executable(mpegts_demux_demo
            src/format/mpegts/mpegts_demux_demo.cpp
            src/format/mpegts/mpegts_demux.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
IF (APPLE)
target_link_libraries(mpegts_demux_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(mpegts_demux_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()

add_executable(mpegts_mux_demo
            src/format/mpegts/mpegts_mux_demo.cpp
            src/format/mpegts/mpegts_mux.cpp
            src/format/flv/flv_demux.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
IF (APPLE)
target_link_libraries(mpegts_mux_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(mpegts_mux_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()


add_executable(http_server_demo
            src/net/http/http_server_demo.cpp
            src/net/http/http_server.cpp
            src/net/http/http_session.cpp
            src/utils/data_buffer.cpp
            src/utils/logger.cpp)
IF (APPLE)
target_link_libraries(http_server_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(http_server_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()

add_executable(rtmp_play_demo
            src/net/rtmp/client_demo/rtmp_play_demo.cpp
            src/net/rtmp/rtmp_client_session.cpp
            src/net/rtmp/rtmp_session_base.cpp
            src/net/rtmp/rtmp_control_handler.cpp
            src/net/rtmp/rtmp_handshake.cpp
            src/net/rtmp/chunk_stream.cpp
            src/format/flv/flv_mux.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
add_dependencies(rtmp_play_demo openssl)

IF (APPLE)
target_link_libraries(rtmp_play_demo pthread dl z m ssl crypto pthread boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(rtmp_play_demo pthread rt dl z m ssl crypto pthread boost_thread boost_system)
ENDIF ()

add_executable(rtmp_publish_demo
            src/net/rtmp/client_demo/rtmp_publish_demo.cpp
            src/net/rtmp/rtmp_client_session.cpp
            src/net/rtmp/rtmp_session_base.cpp
            src/net/rtmp/rtmp_control_handler.cpp
            src/net/rtmp/rtmp_handshake.cpp
            src/net/rtmp/chunk_stream.cpp
            src/format/flv/flv_demux.cpp
            src/format/flv/flv_mux.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
add_dependencies(rtmp_publish_demo openssl)

IF (APPLE)
target_link_libraries(rtmp_publish_demo pthread dl z m ssl crypto pthread boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(rtmp_publish_demo pthread rt dl z m ssl crypto pthread boost_thread boost_system)
ENDIF ()

add_executable(cpp_media_server
            src/media_server.cpp
            src/format/flv/flv_demux.cpp
            src/format/flv/flv_mux.cpp
            src/format/mpegts/mpegts_mux.cpp
            src/net/http/http_session.cpp
            src/net/http/http_server.cpp
            src/net/hls/hls_worker.cpp
            src/net/hls/hls_writer.cpp
            src/net/hls/mpegts_handle.cpp
            src/net/rtmp/rtmp_server.cpp
            src/net/rtmp/rtmp_server_session.cpp
            src/net/rtmp/rtmp_client_session.cpp
            src/net/rtmp/rtmp_session_base.cpp
            src/net/rtmp/rtmp_control_handler.cpp
            src/net/rtmp/rtmp_handshake.cpp
            src/net/rtmp/chunk_stream.cpp
            src/net/rtmp/rtmp_writer.cpp
            src/net/rtprtcp/rtp_packet.cpp
            src/net/websocket/ws_server.cpp
            src/net/websocket/ws_session.cpp
            src/net/websocket/ws_service_imp.cpp
            src/net/websocket/wsimple/flv_websocket.cpp
            src/net/websocket/wsimple/protoo_server.cpp
            src/net/stun/stun_packet.cpp
            src/net/webrtc/room_service.cpp
            src/net/webrtc/sdp_analyze.cpp
            src/net/webrtc/user_info.cpp
            src/net/webrtc/rtc_media_info.cpp
            src/net/webrtc/rtc_base_session.cpp
            src/net/webrtc/webrtc_session.cpp
            src/net/webrtc/rtc_subscriber.cpp
            src/net/webrtc/rtc_dtls.cpp
            src/net/webrtc/srtp_session.cpp
            src/net/webrtc/support_rtc_info.cpp
            src/net/webrtc/rtc_publisher.cpp
            src/net/webrtc/rtp_recv_stream.cpp
            src/net/webrtc/rtp_send_stream.cpp
            src/net/webrtc/nack_generator.cpp
            src/net/webrtc/jitterbuffer.cpp
            src/net/webrtc/pack_handle_h264.cpp
            src/net/webrtc/pack_handle_vp8.cpp
            src/net/webrtc/rtp_h264_pack.cpp
            src/net/webrtc/rtmp2rtc.cpp
            src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/bwe_defines.cc
            src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/inter_arrival.cc
            src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/overuse_detector.cc
            src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/overuse_estimator.cc
            src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.cc
            src/net/httpflv/httpflv_server.cpp
            src/net/httpflv/httpflv_writer.cpp
            src/utils/logger.cpp
            src/utils/config.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp
            src/utils/byte_crypto.cpp
            src/utils/av/media_stream_manager.cpp
            src/utils/av/gop_cache.cpp)

add_dependencies(cpp_media_server openssl sdptransform libsrtp)

IF (APPLE)
target_link_libraries(cpp_media_server pthread dl z m srtp2 ssl crypto pthread boost_thread boost_system sdptransform)
ELSEIF (UNIX)
target_link_libraries(cpp_media_server pthread rt dl z m srtp2 ssl crypto pthread boost_thread boost_system sdptransform)
ENDIF ()
