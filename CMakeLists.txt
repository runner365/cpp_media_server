cmake_minimum_required(VERSION 3.7.1)
project(cpp_media_server)

set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++11 -g -Wno-deprecated -Wno-deprecated-declarations -Wno-reorder -Wall -fexceptions -frtti -D__STDC_FORMAT_MACROS -fPIC")
set(CMAKE_CXX_STANDARD 11)

set(CMAKE_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(BUILD_OUTPUT_BASE ${CMAKE_BINARY_DIR}/output)
set(PREFIX_DIR "${BUILD_OUTPUT_BASE}")

# set output static libary
SET(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/output/lib)

# set pkgconfig path
set(CMAKE_PREFIX_PATH "${CMAKE_BINARY_DIR}/output/lib/pkgconfig")

# set include path
include_directories(./
                    ./net
                    ./net/http
                    ./net/rtmp
                    ./net/websocket
                    ./net/websocket/wsimple
                    ./net/tcp
                    ./net/stun
                    ./net/httpflv
                    ./net/rtprtcp
                    ./format
                    ./format/flv
                    ./utils
                    ./utils/av
                    ./3rdparty/libsdptransform/include
                    ${CMAKE_BINARY_DIR}/output/include
                    /usr/local/include)

# set lib path
#link_directories(${CMAKE_BINARY_DIR}/output/lib /usr/local/lib64 /usr/local/lib)
link_directories(/usr/local/lib64 /usr/local/lib)
add_subdirectory(3rdparty)

add_executable(echo_demo net/tcp/echo_demo.cpp
            utils/logger.cpp
            utils/data_buffer.cpp)
IF (APPLE)
target_link_libraries(echo_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(echo_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()

add_executable(flv_demux_demo
            format/flv/flv_demux.cpp
            format/flv/flv_demux_demo.cpp
            utils/logger.cpp
            utils/data_buffer.cpp
            utils/byte_stream.cpp)
IF (APPLE)
target_link_libraries(flv_demux_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(flv_demux_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()

add_executable(flv_mux_demo
            format/flv/flv_mux.cpp
            format/flv/flv_demux.cpp
            format/flv/flv_mux_demo.cpp
            utils/logger.cpp
            utils/data_buffer.cpp
            utils/byte_stream.cpp)
IF (APPLE)
target_link_libraries(flv_mux_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(flv_mux_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()

add_executable(http_server_demo
            net/http/http_server_demo.cpp
            net/http/http_server.cpp
            net/http/http_session.cpp
            utils/data_buffer.cpp
            utils/logger.cpp)
IF (APPLE)
target_link_libraries(http_server_demo pthread dl z m boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(http_server_demo pthread rt dl z m boost_thread boost_system)
ENDIF ()

add_executable(rtmp_play_demo
            net/rtmp/client_demo/rtmp_play_demo.cpp
            net/rtmp/rtmp_client_session.cpp
            net/rtmp/rtmp_session_base.cpp
            net/rtmp/rtmp_control_handler.cpp
            net/rtmp/rtmp_handshake.cpp
            net/rtmp/chunk_stream.cpp
            format/flv/flv_mux.cpp
            utils/logger.cpp
            utils/data_buffer.cpp
            utils/byte_stream.cpp)
IF (APPLE)
target_link_libraries(rtmp_play_demo pthread dl z m ssl crypto boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(rtmp_play_demo pthread rt dl z m ssl crypto boost_thread boost_system)
ENDIF ()

add_executable(rtmp_publish_demo
            net/rtmp/client_demo/rtmp_publish_demo.cpp
            net/rtmp/rtmp_client_session.cpp
            net/rtmp/rtmp_session_base.cpp
            net/rtmp/rtmp_control_handler.cpp
            net/rtmp/rtmp_handshake.cpp
            net/rtmp/chunk_stream.cpp
            format/flv/flv_demux.cpp
            format/flv/flv_mux.cpp
            utils/logger.cpp
            utils/data_buffer.cpp
            utils/byte_stream.cpp)
IF (APPLE)
target_link_libraries(rtmp_publish_demo pthread dl z m ssl crypto boost_thread boost_system)
ELSEIF (UNIX)
target_link_libraries(rtmp_publish_demo pthread rt dl z m ssl crypto boost_thread boost_system)
ENDIF ()

add_executable(media_server
            media_server.cpp
            format/flv/flv_demux.cpp
            format/flv/flv_mux.cpp
            net/http/http_session.cpp
            net/http/http_server.cpp
            net/rtmp/rtmp_server.cpp
            net/rtmp/rtmp_server_session.cpp
            net/rtmp/rtmp_client_session.cpp
            net/rtmp/rtmp_session_base.cpp
            net/rtmp/rtmp_control_handler.cpp
            net/rtmp/rtmp_handshake.cpp
            net/rtmp/chunk_stream.cpp
            net/rtmp/rtmp_writer.cpp
            net/websocket/ws_server.cpp
            net/websocket/ws_session.cpp
            net/websocket/ws_service_imp.cpp
            net/websocket/wsimple/flv_websocket.cpp
            net/websocket/wsimple/protoo_server.cpp
            net/stun/stun_packet.cpp
            net/webrtc/room_service.cpp
            net/webrtc/sdp_analyze.cpp
            net/webrtc/user_info.cpp
            net/webrtc/rtc_media_info.cpp
            net/webrtc/rtc_base_session.cpp
            net/webrtc/webrtc_session.cpp
            net/webrtc/support_rtc_info.cpp
            net/httpflv/httpflv_server.cpp
            net/httpflv/httpflv_writer.cpp
            utils/logger.cpp
            utils/data_buffer.cpp
            utils/byte_stream.cpp
            utils/byte_crypto.cpp
            utils/av/media_stream_manager.cpp
            utils/av/gop_cache.cpp)

add_dependencies(media_server sdptransform)

IF (APPLE)
target_link_libraries(media_server pthread dl z m ssl crypto boost_thread boost_system sdptransform)
ELSEIF (UNIX)
target_link_libraries(media_server pthread rt dl z m ssl crypto boost_thread boost_system sdptransform)
ENDIF ()