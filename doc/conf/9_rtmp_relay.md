# 9. 配置rtmp回源
配置rtmp回源

## 9.1 rtmp与websocket flv推流配置
实例配置:
```markup
{
    "log_dir":"server.log",
    "log_level": "info",
    "rtmp":{
        "enable": true,
        "listen":1935,
        "gop_cache":"enable",
        "rtmp_relay":{
            "enable": true,
            "host": "192.168.1.120:1935"
        }
    }
}
```

其中"host": "192.168.1.120:1935"，为上游的rtmp服务器地址和服务tcp端口号。

如何运行：./objs/cpp_media_server -c ./conf/rtmp_websocketflv.cfg

## 9.2 配置详解
rtmp基本配置模块，通过配置“rtmp”的json模块。

如果不配置rtmp的json模块，rtmp服务为“去使能”状态。

### 9.2.1 使能rtmp回源功能
```markup
    "rtmp_relay":{
        "enable": true,
        "host": "192.168.1.120:1935"
    }
```
上游的rtmp服务器的rtmp服务信息: 192.168.1.120:1935.

举例，拉流播放：rtmp://127.0.0.1/live/123456.

如果服务器没有live/123456这条直播流，立刻回源192.168.1.120:1935，会去拉流:rtmp://192.168.1.120/live/123456，最终把流给到用户。



