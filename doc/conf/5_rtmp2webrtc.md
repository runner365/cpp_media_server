# 5. 配置rtmp转webrtc
## 5.1 rtmp转webrtc配置
实例配置:
```markup
{
    "log_dir":"server.log",
    "log_level": "info",
    "rtmp":{
        "enable": true,
        "listen":1935,
        "gop_cache":"enable"
    },
    "webrtc":{
        "enable": true,
        "listen": 8000,
        "tls_key": "certs/server.key",
        "tls_cert": "certs/server.crt",
        "udp_port": 7000,
        "candidate_ip": "192.168.1.98",
        "rtmp2rtc": true
    }
}
```

## 5.2 配置详解
使能rtmp转webrtc服务，必须webrtc与rtmp同时使能

### 5.2.1 使能rtmp转webrtc
```markup
"webrtc":{
    ......
    "rtmp2rtc": true
}
```

### 5.2.2 举例
使用webrtc web sdk加入房间后，如：

webrtc join的roomid: 2001, userid: 20000。

rtmp推流流url地址: rtmp://x.x.x.x/2001/10000。

webrtc用户自动会在房间roomid=2001内，看见rtmp流。

### 5.2.3 ffmpeg推流举例
自定义的ffmpeg源码(支持opus in flv): [my_ffmpeg](https://github.com/runner365/my_ffmpeg)

如果你编译了我自定义的ffmpeg(支持opus in flv)，示例的命令行：
```markup
ffmpeg -re -i source.flv -c:v libx264 -s 640x360 -profile:v baseline -r 15 -x264-params 'keyint=30:vbv-maxrate=1000:vbv-bufsize=2000' -c:a libopus -ar 48000 -ac 2 -ab 32k -f flv rtmp://127.0.0.1/2001/30001
```
如果你没有编译我自定义的ffmpeg(不支持opus in flv)，webrtc观看只能看见视频，没有声音，示例命令行:
```markup
ffmpeg -re -i source.flv -c:v libx264 -s 640x360 -profile:v baseline -r 15 -x264-params 'keyint=30:vbv-maxrate=1000:vbv-bufsize=2000' -c:a aac -ar 44100 -ac 2 -ab 32k -f flv rtmp://127.0.0.1/2001/30001
```

## 5.3 webrtc client sdk
webrtc会议client sdk: [webrtc client sdk](https://github.com/runner365/webrtc-client-sdk)