# cpp_media_server

A media server is writen by C++11, and the network io is writen by Boost.Asio.

It support rtmp/httpflv/websocket(flv)/webrtc.

## preinstall
How to install boost.asio
### Download boost
We suggenst boost v1.76: [boost v1.76](https://boostorg.jfrog.io/ui/native/main/release/1.76.0/source/)

### How to build
Build boost asio in Mac/linux
1) ./bootstrap.sh

2) ./b2 -j 4 --with-system --with-thread --with-date_time --with-regex --with-serialization stage && ./b2 install

## What does It support
network protocol feature:

### webrtc
* roommanager
* websocket
* join
* publish/unpublish
* subscribe/unsubscribe

The webrtc client sdk demo: [webrtc sdk demo](https://github.com/runner365/webrtc-client-sdk)
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




