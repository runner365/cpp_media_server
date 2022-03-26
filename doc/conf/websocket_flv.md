# 7. 配置rtmp与websocket flv推流
基于chrome浏览器的webcodec编码，websocket flv推流。

## 7.1 rtmp与websocket flv推流配置
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
    "websocket":{
        "enable": "yes",
        "listen": 9000
    }
}
```
如何运行：./objs/cpp_media_server -c ./conf/rtmp_websocketflv.cfg

## 7.2 配置详解
websocket flv模块，通过配置“websocket”的json模块。

如果不配置websocket的json模块，websocket服务为“去使能”状态。

### 7.2.1 使能websocket flv
```markup
"websocket":{
    "enable": "yes"
    ......
}
```
### 7.2.2 配置websocket port
```markup
"websocket":{
    "enable": "yes",
    "listen":9000
}
```
websocket端口号: 9000.

## 7.3 websocket推流sdk
基于webcodec的websocket flv推理: [websocket推流sdk](https://github.com/runner365/webcodecpush)
