# cpp_media_server

A media server is written by C++11, and the network io is written by Boost.Asio.

It support rtmp/hls/httpflv/websocket(flv)/webrtc.

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
* flv(h264/vp8, aac/opus)

### hls
media stream is from rtmp/webrtc/websocket flv.
support codec: h264/vp8 + aac/opus

* hls live: 
### websocket
* flv in websocket push

webcodec encode media in flv(h264+opus) over websocket, we suggest webclient demo: 
[webcodecpush](https://github.com/runner365/webcodecpush)

## codec help
I provide the customized ffmpeg to support
* h264/vp8, aac/opus in flv
* h264/vp8, aac/opus in hls

the customized ffmpeg: [my_ffmpeg](https://github.com/runner365/my_ffmpeg)



