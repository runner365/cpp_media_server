# cpp media server

cpp media server是基于c++17开发的webrtc会议服务sfu，网络部分基于Boost.Asio。

Boost.Asio为网络IO高性能并发提供好的基础，并且支持跨平台(linux/mac)，windows可以自行修改cmake支持。

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
* webobs: websocket推送flv直播服务(webcodec编码，websocket flv推流封装)

## 2. 如何编译
[如何编译](doc/conf/0_how_to_build.md)

## 3. Wiki文档
### 3.1 配置指南
* 如何配置webrtc: [webrtc配置指南](doc/conf/1_webrtc.md)
* 如何配置rtmp server: [rtmp配置指南](doc/conf/2_rtmp.md)
* 如何配置rtmp和httpflv服务: [rtmp和httpflv配置指南](doc/conf/3_rtmp_httpflv.md)
* 如何配置webrtc2rtmp: [webrtc转rtmp配置指南](doc/conf/4_webrtc2rtmp.md)
* 如何配置rtmp2webrtc: [rtmp转webrtc配置指南](doc/conf/5_rtmp2webrtc.md)
* 如何配置rtmp和hls服务: [rtmp和hls配置指南](doc/conf/6_rtmp_hls.md)
* 如何配置websocket推送flv服务: [websocket flv配置指南](doc/conf/7_websocket_flv.md)
* 如何配置webrtc数据统计http api接口: [webrtc statics维护接口配置](doc/conf/8_webrtc_statics.md)
* 如何配置rtmp回源: [rtmp回源配置指南](doc/conf/9_rtmp_relay.md)

### 3.2 C++媒体模块
如何引用几个cpp/hpp文件，就能实现流媒体的封装
* [flv mux/demux模块](doc/module/flv_module.md)
* [mpegts mux/demux模块](doc/module/mpegts_module.md)
* [rtmp play/publish模块](doc/module/rtmp_module.md)

### 3.3 webrtc信令接口
* [webrtc whip信令接口](doc/api/whip_api.md)
* [webrtc rtcdn信令接口](doc/api/rtcdn_api.md)
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

