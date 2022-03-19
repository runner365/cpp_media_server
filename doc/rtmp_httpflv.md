# 3. 配置rtmp和httpflv
## 3.1 rtmp和httpflv配置
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
    "httpflv":{
        "enable": "yes",
        "listen":8070
    }
}
```

如何运行：./objs/cpp_media_server -c ./conf/rtmp_httpflv.cfg

## 3.2 配置详解
httpflv模块，通过配置“httpflv”的json模块。

如果不配置httpflv的json模块，httpflv服务为“去使能”状态。
### 3.2.1 使能httpflv
```markup
"httpflv":{
    "enable": "yes"
    ......
}
```
### 3.2.2 配置httpflv端口号
```markup
"httpflv":{
    "enable": "yes"
    "listen":8070
}
```
配置httpflv端口号。

## 3.3 使用举例
推流举例：
```markup
ffmpeg -re -i xxx.flv -c copy -f flv rtmp://x.x.x.x/live/livestream
```
httpflv举例：
```markup
ffplay http://x.x.x.x:8070/live/livestream.flv
```
