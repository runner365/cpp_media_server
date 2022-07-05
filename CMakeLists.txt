cmake_minimum_required(VERSION 3.7.1)
project(cpp_media_server)
project(cpp_media_server VERSION 1.1)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(BUILD_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(PREFIX_DIR "${BUILD_OUTPUT_BASE}")
set(INSTALL_RPATH "${PREFIX_DIR}/lib")
option(ENABLE_FFMPEG "Enable FFmpeg" true)
# set output static libary
SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/output/lib)

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
                    src/transcode
                    src/format
                    src/format/flv
                    src/utils
                    src/utils/av
                    3rdparty/libsdptransform/include
                    3rdparty/libsrtp/include
                    3rdparty/libsrtp/crypto/include
                    3rdparty/libuv/include
                    ${OPENSSL_INCLUDE_DIR}
                    ${CMAKE_BINARY_DIR}/output/include
                    ${CMAKE_BINARY_DIR}/output/include/srtp2
                    ${CMAKE_BINARY_DIR}/output/include/openssl
                    /usr/local/include)

# set lib path
IF (WIN32)
    find_package(OpenSSL)
    if(OpenSSL_FOUND)
        message(STATUS "found openssl:${OPENSSL_INCLUDE_DIR}")
        set(LIBS OpenSSL::SSL OpenSSL::Crypto)
    else()
        set(LIBS ssl crypto)
    endif()
    if (ENABLE_FFMPEG)
		# 根据实际情况，修改ffmpeg的安装路径，注意大于4.4的版本不行
        SET(FFMPEG_ROOT_DIR D:/Work/ffmpeg/v4.3)
        message(STATUS "found ffmpeg:${FFMPEG_ROOT_DIR}")
        add_definitions(-DENABLE_FFMPEG)
        include_directories(${FFMPEG_ROOT_DIR}/include)
        link_directories(${FFMPEG_ROOT_DIR}/lib)
        list(APPEND LIBS avfilter avformat avcodec avutil swscale swresample)
    endif()
else()
    # set pkgconfig path
    set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/output/lib/pkgconfig")
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -Wno-deprecated -Wno-deprecated-declarations -Wno-reorder -Wall -fexceptions -frtti -D__STDC_FORMAT_MACROS -fPIC")
    #link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib64 /usr/local/lib)
    link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib64 /usr/local/lib)
ENDIF()
add_subdirectory(3rdparty)

#add_executable(echo_demo
#            src/net/tcp/echo_demo.cpp
#            src/utils/logger.cpp
#            src/utils/data_buffer.cpp)
#
#IF (APPLE)
#target_link_libraries(echo_demo dl z m ssl crypto pthread uv)
#ELSEIF (UNIX)
#target_link_libraries(echo_demo rt dl z m ssl crypto pthread uv)
#ENDIF ()
#
#add_executable(flv_demux_demo
#            ./src/format/flv/flv_demux.cpp
#            ./src/format/flv/flv_demux_demo.cpp
#            ./src/utils/logger.cpp
#            ./src/utils/data_buffer.cpp
#            ./src/utils/byte_stream.cpp)
#IF (APPLE)
#target_link_libraries(flv_demux_demo pthread dl z m)
#ELSEIF (UNIX)
#target_link_libraries(flv_demux_demo pthread rt dl z m)
#ENDIF ()
#
#add_executable(flv_mux_demo
#            src/format/flv/flv_mux.cpp
#            src/format/flv/flv_demux.cpp
#            src/format/flv/flv_mux_demo.cpp
#            src/utils/logger.cpp
#            src/utils/data_buffer.cpp
#            src/utils/byte_stream.cpp)
#IF (APPLE)
#target_link_libraries(flv_mux_demo pthread dl z m)
#ELSEIF (UNIX)
#target_link_libraries(flv_mux_demo pthread rt dl z m)
#ENDIF ()
#
#add_executable(mpegts_demux_demo
#            src/format/mpegts/mpegts_demux_demo.cpp
#            src/format/mpegts/mpegts_demux.cpp
#            src/utils/logger.cpp
#            src/utils/data_buffer.cpp
#            src/utils/byte_stream.cpp)
#IF (APPLE)
#target_link_libraries(mpegts_demux_demo pthread dl z m)
#ELSEIF (UNIX)
#target_link_libraries(mpegts_demux_demo pthread rt dl z m)
#ENDIF ()
#
#add_executable(mpegts_mux_demo
#            src/format/mpegts/mpegts_mux_demo.cpp
#            src/format/mpegts/mpegts_mux.cpp
#            src/format/flv/flv_demux.cpp
#            src/utils/logger.cpp
#            src/utils/data_buffer.cpp
#            src/utils/byte_stream.cpp)
#IF (APPLE)
#target_link_libraries(mpegts_mux_demo pthread dl z m)
#ELSEIF (UNIX)
#target_link_libraries(mpegts_mux_demo pthread rt dl z m)
#ENDIF ()
#
#add_executable(http_server_demo
#            src/net/http/http_server_demo.cpp
#            src/net/http/http_server.cpp
#            src/net/http/http_session.cpp
#            src/utils/data_buffer.cpp
#            src/utils/logger.cpp)
#IF (APPLE)
#target_link_libraries(http_server_demo  dl z m ssl crypto pthread uv)
#ELSEIF (UNIX)
#target_link_libraries(http_server_demo rt dl z m ssl crypto pthread uv)
#ENDIF ()
#
#add_executable(https_server_demo
#            src/net/http/https_server_demo.cpp
#            src/net/http/http_server.cpp
#            src/net/http/http_session.cpp
#            src/utils/data_buffer.cpp
#            src/utils/logger.cpp)
#IF (APPLE)
#target_link_libraries(https_server_demo dl z m ssl crypto pthread uv)
#ELSEIF (UNIX)
#target_link_libraries(https_server_demo rt dl z m ssl crypto pthread uv)
#ENDIF ()
#
#add_executable(rtmp_play_demo
#            src/net/rtmp/client_demo/rtmp_play_demo.cpp
#            src/net/rtmp/rtmp_client_session.cpp
#            src/net/rtmp/rtmp_session_base.cpp
#            src/net/rtmp/rtmp_control_handler.cpp
#            src/net/rtmp/rtmp_handshake.cpp
#            src/net/rtmp/chunk_stream.cpp
#            src/format/flv/flv_mux.cpp
#            src/utils/logger.cpp
#            src/utils/data_buffer.cpp
#            src/utils/byte_stream.cpp)
#add_dependencies(rtmp_play_demo openssl)
#
#IF (APPLE)
#target_link_libraries(rtmp_play_demo pthread dl z m ssl crypto pthread uv)
#ELSEIF (UNIX)
#target_link_libraries(rtmp_play_demo pthread rt dl z m ssl crypto pthread uv)
#ENDIF ()
#
#add_executable(rtmp_publish_demo
#            src/net/rtmp/client_demo/rtmp_publish_demo.cpp
#            src/net/rtmp/rtmp_client_session.cpp
#            src/net/rtmp/rtmp_session_base.cpp
#            src/net/rtmp/rtmp_control_handler.cpp
#            src/net/rtmp/rtmp_handshake.cpp
#            src/net/rtmp/chunk_stream.cpp
#            src/format/flv/flv_demux.cpp
#            src/format/flv/flv_mux.cpp
#            src/utils/logger.cpp
#            src/utils/data_buffer.cpp
#            src/utils/byte_stream.cpp)
#add_dependencies(rtmp_publish_demo openssl)
#
#IF (APPLE)
#target_link_libraries(rtmp_publish_demo pthread dl z m ssl crypto pthread uv)
#ELSEIF (UNIX)
#target_link_libraries(rtmp_publish_demo pthread rt dl z m ssl crypto pthread uv)
#ENDIF ()

SET(SRC_LIST src/media_server.cpp
    src/media_server.hpp
    src/format/flv/flv_demux.cpp
    src/format/flv/flv_demux.hpp
    src/format/flv/flv_mux.cpp
    src/format/flv/flv_mux.hpp
    src/format/flv/flv_pub.hpp
    src/format/mpegts/mpegts_mux.cpp
    src/format/mpegts/mpegts_mux.hpp
    src/format/audio_pub.hpp
    src/format/av_format_interface.hpp
    src/format/h264_header.hpp
    src/net/tcp/tcp_server.hpp
    src/net/tcp/tcp_session.hpp
    src/net/udp/udp_server.hpp
    src/net/http/http_session.cpp
    src/net/http/http_server.cpp
    src/net/http/http_common.hpp
    src/net/http/http_server.hpp
    src/net/http/http_session.hpp
    src/net/hls/hls_worker.cpp
    src/net/hls/hls_worker.hpp
    src/net/hls/hls_writer.cpp
    src/net/hls/hls_writer.hpp
    src/net/hls/mpegts_handle.cpp
    src/net/hls/mpegts_handle.hpp
    src/net/rtmp/amf/amf0.hpp
    src/net/rtmp/rtmp_pub.hpp
    src/net/rtmp/rtmp_request.hpp
    src/net/rtmp/rtmp_server.cpp
    src/net/rtmp/rtmp_server.hpp
    src/net/rtmp/rtmp_server_session.cpp
    src/net/rtmp/rtmp_server_session.hpp
    src/net/rtmp/rtmp_client_session.cpp
    src/net/rtmp/rtmp_client_session.hpp
    src/net/rtmp/rtmp_session_base.cpp
    src/net/rtmp/rtmp_session_base.hpp
    src/net/rtmp/rtmp_control_handler.cpp
    src/net/rtmp/rtmp_control_handler.hpp
    src/net/rtmp/rtmp_handshake.cpp
    src/net/rtmp/rtmp_handshake.hpp
    src/net/rtmp/chunk_stream.cpp
    src/net/rtmp/chunk_stream.hpp
    src/net/rtmp/rtmp_writer.cpp
    src/net/rtmp/rtmp_writer.hpp
    src/net/rtmp/rtmp_relay.cpp
    src/net/rtmp/rtmp_relay.hpp
    src/net/rtmp/rtmp_relay_mgr.cpp
    src/net/rtmp/rtmp_relay_mgr.hpp
    src/net/rtprtcp/rtp_packet.cpp
    src/net/rtprtcp/rtp_packet.hpp
    src/net/rtprtcp/rtcp_fb_pub.hpp
    src/net/rtprtcp/rtcp_pspli.hpp
    src/net/rtprtcp/rtcp_rr.hpp
    src/net/rtprtcp/rtcp_sr.hpp
    src/net/rtprtcp/rtcp_xr_dlrr.hpp
    src/net/rtprtcp/rtcp_xr_rrt.hpp
    src/net/rtprtcp/rtcpfb_nack.hpp
    src/net/rtprtcp/rtcpfb_remb.hpp
    src/net/rtprtcp/rtprtcp_pub.hpp
    src/net/websocket/ws_server.cpp
    src/net/websocket/ws_server.hpp
    src/net/websocket/ws_session.cpp
    src/net/websocket/ws_session.hpp
    src/net/websocket/ws28/Server.cpp
    src/net/websocket/ws28/Server.h
    src/net/websocket/ws28/base64.cpp
    src/net/websocket/ws28/base64.h
    src/net/websocket/ws28/Client.cpp
    src/net/websocket/ws28/Client.h
    src/net/websocket/ws28/Headers.h
    src/net/websocket/wsimple/protoo_pub.hpp
    src/net/websocket/wsimple/flv_websocket.cpp
    src/net/websocket/wsimple/flv_websocket.hpp
    src/net/websocket/wsimple/protoo_server.cpp
    src/net/websocket/wsimple/protoo_server.hpp
    src/net/stun/stun_packet.cpp
    src/net/stun/stun_packet.hpp
    src/net/webrtc/rtc_session_pub.hpp
    src/net/webrtc/rtc_stream_pub.hpp
    src/net/webrtc/webrtc_pub.hpp
    src/net/webrtc/room_service.cpp
    src/net/webrtc/room_service.hpp
    src/net/webrtc/sdp_analyze.cpp
    src/net/webrtc/sdp_analyze.hpp
    src/net/webrtc/user_info.cpp
    src/net/webrtc/user_info.hpp
    src/net/webrtc/rtc_media_info.cpp
    src/net/webrtc/rtc_media_info.hpp
    src/net/webrtc/rtc_base_session.cpp
    src/net/webrtc/rtc_base_session.hpp
    src/net/webrtc/webrtc_session.cpp
    src/net/webrtc/webrtc_session.hpp
    src/net/webrtc/rtc_subscriber.cpp
    src/net/webrtc/rtc_subscriber.hpp
    src/net/webrtc/rtc_dtls.cpp
    src/net/webrtc/rtc_dtls.hpp
    src/net/webrtc/srtp_session.cpp
    src/net/webrtc/srtp_session.hpp
    src/net/webrtc/support_rtc_info.cpp
    src/net/webrtc/support_rtc_info.hpp
    src/net/webrtc/rtc_publisher.cpp
    src/net/webrtc/rtc_publisher.hpp
    src/net/webrtc/rtp_recv_stream.cpp
    src/net/webrtc/rtp_recv_stream.hpp
    src/net/webrtc/rtp_send_stream.cpp
    src/net/webrtc/rtp_send_stream.hpp
    src/net/webrtc/nack_generator.cpp
    src/net/webrtc/nack_generator.hpp
    src/net/webrtc/pack_handle_audio.hpp
    src/net/webrtc/pack_handle_pub.hpp
    src/net/webrtc/jitterbuffer.cpp
    src/net/webrtc/jitterbuffer.hpp
    src/net/webrtc/jitterbuffer_pub.hpp
    src/net/webrtc/pack_handle_h264.cpp
    src/net/webrtc/pack_handle_h264.hpp
    src/net/webrtc/pack_handle_vp8.cpp
    src/net/webrtc/pack_handle_vp8.hpp
    src/net/webrtc/rtp_h264_pack.cpp
    src/net/webrtc/rtp_h264_pack.hpp
    src/net/webrtc/rtmp2rtc.cpp
    src/net/webrtc/rtmp2rtc.hpp
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/bwe_defines.cc
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/include/bwe_defines.h
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/inter_arrival.cc
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/inter_arrival.h
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/overuse_detector.cc
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/overuse_detector.h
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/rate_control.cc
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/rate_control.h
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/overuse_estimator.h
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/overuse_estimator.cc
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/overuse_estimator.h
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.cc
    src/net/webrtc/libwebrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h
    src/net/httpflv/httpflv_server.cpp
    src/net/httpflv/httpflv_server.hpp
    src/net/httpflv/httpflv_writer.cpp
    src/net/httpflv/httpflv_writer.hpp
    src/net/httpapi/httpapi_server.cpp
    src/net/httpapi/httpapi_server.hpp
    src/utils/ipaddress.hpp
    src/utils/session_aliver.hpp
    src/utils/stream_statics.hpp
    src/utils/stringex.hpp
    src/utils/timeex.hpp
    src/utils/timer.hpp
    src/utils/uuid.hpp
    src/utils/logger.cpp
    src/utils/logger.hpp
    src/utils/config.cpp
    src/utils/config.hpp
    src/utils/data_buffer.cpp
    src/utils/data_buffer.hpp
    src/utils/byte_stream.cpp
    src/utils/byte_stream.hpp
    src/utils/byte_crypto.cpp
    src/utils/byte_crypto.hpp
    src/utils/av/media_stream_manager.cpp
    src/utils/av/media_stream_manager.hpp
    src/utils/av/gop_cache.cpp
    src/utils/av/gop_cache.hpp)

IF(ENABLE_FFMPEG)
    aux_source_directory(src/transcode SRC_LIST)
ENDIF()

add_executable(cpp_media_server ${SRC_LIST})

add_dependencies(cpp_media_server sdptransform srtp2) #openssl libsrtp ffmpeg  libx264 libopus)

IF (APPLE)
target_link_libraries(cpp_media_server pthread dl z m
    srtp2 ssl crypto pthread sdptransform
    avfilter avformat avcodec avutil postproc swscale swresample
    uv)
ELSEIF (UNIX)
target_link_libraries(cpp_media_server
    avfilter avformat avcodec avutil postproc swscale swresample
    x264 fdk-aac opus
    ssl crypto
    srtp2 pthread sdptransform
    pthread rt dl z m bz2
    uv)
ELSEIF (WIN32)
target_link_libraries(cpp_media_server ${LIBS} srtp2 uv_a sdptransform)
ENDIF ()

