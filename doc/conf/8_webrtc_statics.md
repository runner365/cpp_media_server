# 8. webrtc statics
## 8.1 webrtc statics配置
实例配置:
```markup
{
    "log_dir":"server.log",
    "log_level":"info",
    "webrtc":{
        "enable":true,
        "listen":8000,
        "tls_key":"certs/server.key",
        "tls_cert":"certs/server.crt",
        "udp_port":7000,
        "candidate_ip":"192.168.1.98"
    },
    "httpapi":{
        "enable":true,
        "listen":8090
    }
}
```

如何运行：./objs/cpp_media_server -c ./conf/webrtc.cfg

## 8.2 配置详解
statics模块，通过配置“httpapi”的json模块。

如果不配置httpapi的json模块，httpapi服务为“去使能”状态。

### 8.2.1 使能httpapi
```markup

"httpapi":{
    "enable": true
    ......
}
```

### 8.2.2 httpapi端口号
```markup
"httpapi":{
    "enable": true
    "listen": 8090
}
```
该端口号8090用于httpapi服务，提供httpapi等查询服务。

## 8.3 查询接口
### 8.3.1 房间查询
url: http://x.x.x.x:8090/api/webrtc/room

http方法: get

uri: /api/webrtc/room

返回数据示例:
```markup
{
    "code":0,
    "data":{
        "live_list":[
            {
                "uid":"8877"
            }
        ],
        "rtc_list":[
            {
                "publishers":1,
                "subscribers":2,
                "uid":"95235"
            },
            {
                "publishers":1,
                "subscribers":2,
                "uid":"98376"
            }
        ]
    },
    "desc":"ok"
}
```

### 8.3.2 webrtc推流查询
url: http://x.x.x.x:8090/api/webrtc/publisher?roomId=2001&uid=98376

http方法: get

uri: /api/webrtc/publisher

参数列表: roomId=2001&uid=98376

数据返回:
```markup
{
    "code":0,
    "data":{
        "count":2,
        "list":[
            {
                "bps":443128,
                "bytes":86466686,
                "clockrate":90000,
                "codec":"h264",
                "count":81895,
                "fps":52,
                "media":"video",
                "payload":108,
                "rtt":8.302937507629395,
                "rtx_payload":109,
                "rtx_ssrc":4154718217,
                "ssrc":1372123417,
                "stream":"camera"
            },
            {
                "bps":35048,
                "bytes":5611191,
                "channel":2,
                "clockrate":48000,
                "codec":"opus",
                "count":60375,
                "fps":50,
                "media":"audio",
                "payload":111,
                "rtt":8.614715576171875,
                "rtx_payload":0,
                "rtx_ssrc":0,
                "ssrc":3547349347,
                "stream":"camera"
            }
        ]
    },
    "desc":"ok"
}
```

### 8.3.3 webrtc拉流查询
url: http://x.x.x.x:8090/api/webrtc/subscriber?roomId=2001&uid=98376

http方法: get

uri: /api/webrtc/subscriber

参数列表: roomId=2001&uid=98376

数据返回:
```markup
{
    "code":0,
    "data":{
        "count":2,
        "list":[
            {
                "bps":42136,
                "bytes":5856531,
                "clockrate":48000,
                "count":65782,
                "fps":49,
                "jitter":53,
                "lostrate":0,
                "losttotal":22,
                "media":"audio",
                "mid":1,
                "payload":111,
                "remote_uid":"95235",
                "rtt":8.115927696228027,
                "rtx_payload":0,
                "rtx_ssrc":0,
                "src_mid":1,
                "ssrc":3771392512,
                "stream":"rtc",
                "uid":"98376"
            },
            {
                "bps":457200,
                "bytes":81152736,
                "clockrate":90000,
                "count":77021,
                "fps":58,
                "jitter":1819,
                "lostrate":0,
                "losttotal":0,
                "media":"video",
                "mid":0,
                "payload":108,
                "remote_uid":"95235",
                "rtt":17.771154403686523,
                "rtx_payload":109,
                "rtx_ssrc":3139563927,
                "src_mid":0,
                "ssrc":1607552468,
                "stream":"rtc",
                "uid":"98376"
            }
        ]
    },
    "desc":"ok"
}
```
