# 2. 配置rtmp
## 2.1 rtmp配置
实例配置:
```markup
{
    "log_dir":"server.log",
    "log_level": "info",
    "rtmp":{
        "enable": "yes",
        "listen":1935,
        "gop_cache":"enable"
    }
}
```

如何运行：./objs/cpp_media_server -c ./conf/rtmp.cfg

## 2.2 配置详解
rtmp模块，通过配置“rtmp”的json模块。

如果不配置rtmp的json模块，rtmp服务为“去使能”状态。
### 2.2.1 使能rtmp
```markup
"rtmp":{
    "enable": "yes"
    ......
}
```
### 2.2.2 rtmp端口号
```markup
"rtmp":{
    "enable": "yes",
    "listen":1935
    ......
}
```
该端口号用于rtmp服务，如果不配置，默认1935。
### 2.2.3 gop cache配置
```markup
"rtmp":{
    "enable": "yes",
    "listen":1935,
    "gop_cache":"enable"
}
```
"gop_cache":"enable"配置后，使能gop cache，帮助客户端实现rtmp/httpflv play的秒开。

