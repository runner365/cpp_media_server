# 1. 配置webrtc
## 1.1 webrtc配置
实例配置:
```markup
{
    "log_dir":"server.log",
    "log_level": "info",
    "webrtc":{
        "enable": "yes",
        "listen": 8000,
        "tls_key": "certs/server.key",
        "tls_cert": "certs/server.crt",
        "udp_port": 7000,
        "candidate_ip": "192.168.1.98"
    }
}
```

如何运行：./objs/cpp_media_server -c ./conf/webrtc.cfg

## 1.2 配置详解
webrtc模块，通过配置“webrtc”的json模块。

如果不配置webrtc的json模块，webrtc服务为“去使能”状态。
### 1.2.1 使能webrtc
```markup

"webrtc":{
    "enable": "yes"
    ......
}
```

### 1.2.2 websocket端口号
```markup
"webrtc":{
    "enable": "yes"
    "listen": 8000
    ......
}
```
该端口号用于websocket服务，接收webrtc的信令。

## 1.2.3 tls证书配置
```markup
"webrtc":{
    ....
    "tls_key": "certs/server.key",
    "tls_cert": "certs/server.crt",
    ....
}
```

## 1.2.4 udp端口号与ip
```markup
"webrtc":{
    ....
    "udp_port": 7000,
    "candidate_ip": "192.168.1.98"
    ....
}
```
ip与udp端口号为webrtc媒体通信服务接口。


划重点：


IP地址必须是，实际对外物理IP，必须可达(不能是0.0.0.0，不能是127.0.0.1)

## 1.3 webrtc client sdk
webrtc会议client sdk: [webrtc client sdk](https://github.com/runner365/webrtc-client-sdk)