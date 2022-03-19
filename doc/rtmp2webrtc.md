# 5. 配置rtmp转webrtc
## 5.1 rtmp转webrtc配置
实例配置:
```markup
{
    "log_dir":"server.log",
    "log_level": "info",
    "rtmp":{
        "enable": "yes",
        "listen":1935,
        "gop_cache":"enable"
    },
    "webrtc":{
        "enable": "yes",
        "listen": 8000,
        "tls_key": "certs/server.key",
        "tls_cert": "certs/server.crt",
        "udp_port": 7000,
        "candidate_ip": "192.168.1.98",
        "rtmp2rtc": "yes"
    }
}
```

## 5.2 配置详解
使能rtmp转webrtc服务，必须webrtc与rtmp同时使能

### 5.2.1 使能rtmp转webrtc
```markup
"webrtc":{
    ......
    "rtmp2rtc": "yes"
}
```

### 5.2.2 举例
使用webrtc web sdk加入房间后，如：

webrtc join的roomid: 2001, userid: 20000。

rtmp推流流url地址: rtmp://x.x.x.x/2001/10000。

webrtc用户自动会在房间roomid=2001内，看见rtmp流。

## 5.3 webrtc client sdk
webrtc会议client sdk: [webrtc client sdk](https://github.com/runner365/webrtc-client-sdk)