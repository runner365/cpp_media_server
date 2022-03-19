# 4. 配置webrtc转rtmp
## 4.1 webrtc转rtmp配置
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
        
    }
}
```

## 4.2 配置详解
使能webrtc转rtmp服务，必须webrtc与rtmp同时使能
### 4.2.1 使能webrtc转rtmp
```markup
"webrtc":{
    ......
    "rtc2rtmp": "yes"
}
```

### 4.2.2 举例
使用webrtc web sdk推流后，如：

webrtc推流者的roomid: 2001, userid: 10000。

rtmp拉流url地址: rtmp://x.x.x.x/2001/10000。

也就是rtmp的app=2001, streamname=10000。

webrtc  --> rtmp映射关系:

 roomid --> app

 userid --> streamname

## 4.3 webrtc client sdk
webrtc会议client sdk: [webrtc client sdk](https://github.com/runner365/webrtc-client-sdk)
