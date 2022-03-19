# cpp media server

cpp media server是基于c++11开发的webrtc会议服务sfu，网络部分基于Boost.Asio。

## 1. 支持特性
### 1.1 webrtc相关特性
* 房间管理服务
* websocket长连接接入
* 加入/离开房间
* 推流/停止推流
* 拉流/停止拉流
* 高性能webrtc转rtmp: 无转码

   高性能支持webrtc的旁路rtmp直播
* 高性能rtmp转webrtc: 无转码

   高性能支持低延时直播，支持rtmp转为webrtc

<font size=3>划重点：为什么能支持高性能webrtc与rtmp的互转，因为支持opus/vp8 in flv，并提供自定义ffmpeg，见4.2.</font>

### 1.2 直播相关特性
* rtmp推拉流服务(支持h264/vp8+aac/opus in rtmp/flv)
* httpflv拉流服务(支持h264/vp8+aac/opus in rtmp/flv)
* hls录像服务(支持h264/vp8+aac/opus in mpegts)
* websocket推送flv直播服务(webcodec编码，websocket flv推流封装)


## 2. 如何编译
[如何编译](doc/how_to_build.md)

## 3. Wiki文档

* 如何配置webrtc: [webrtc配置指南](doc/webrtc.md)
* 如何配置rtmp server: [rtmp配置指南](doc/rtmp.md)
* 如何配置rtmp和httpflv服务: [rtmp和httpflv配置指南](doc/rtmp_httpflv.md)
* 如何配置webrtc2rtmp: [webrtc转rtmp配置指南](doc/webrtc2rtmp.md)
* 如何配置rtmp2webrtc: [rtmp转webrtc配置指南](doc/rtmp2webrtc.md)
* 如何配置rtmp和hls服务: [rtmp和hls配置指南](doc/rtmp_hls.md)
* 如何配置websocket推送flv服务: [websocket flv配置指南](doc/websocket_flv.md)

## 4. 支持相关
### 4.1 webrtc client sdk
webrtc会议client sdk: [webrtc client sdk](https://github.com/runner365/webrtc-client-sdk)

### 4.2 自定义ffmpeg

服务支持丰富的编码格式：
* h264/vp8, aac/opus in flv
* h264/vp8, aac/opus in hls

提供自定义的ffmpeg源码: [my_ffmpeg](https://github.com/runner365/my_ffmpeg)

### 4.3 webcodec client sdk
基于webcodec的websocket flv推理: [websocket推流sdk](https://github.com/runner365/webcodecpush)

