# cpp_media_server

A media server is writen by C++11, and the network io is writen by Boost.Asio.

It support rtmp/httpflv/websocket(flv)/webrtc.

## How to build
[How to build](doc/how_to_build.md)

## What does It support
network protocol feature:

### webrtc
* roommanager
* websocket
* join
* publish/unpublish
* subscribe/unsubscribe

The webrtc client sdk demo(only support chrome browser): [webrtc sdk demo](https://github.com/runner365/webrtc-client-sdk)

### rtmp
* rtmp publish
* rtmp play

### httpflv
* httpflv play
* flv(h264/vp8/vp9, aac/opus)

### websocket
* flv in websocket push

webcodec encode media in flv(h264+opus) over websocket, we suggest webclient demo: 
[webcodecpush](https://github.com/runner365/webcodecpush)




