# 6. 配置rtmp与hls
## 6.1 rtmp与hls配置
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
    "hls":{
        "enable": "yes",
        "ts_duration":5000,
        "hls_path":"./hls"
    }
}
```


如何运行：./objs/cpp_media_server -c ./conf/rtmp_hls.cfg

## 6.2 配置详解
hls模块，通过配置“hls”的json模块。

如果不配置hls的json模块，hls服务为“去使能”状态。
### 6.2.1 使能hls
```markup
"hls":{
    "enable": "yes"
    ......
}
```
### 6.2.2 配置mpegts切片市场
```markup
"hls":{
    "enable": "yes",
    "ts_duration":5000,
    ....
}
```
配置ts_duration，为mpegts切片的最小时长，单位毫秒。
### 6.2.3 配置hls的mpegts生成目录
```markup
"hls":{
    "enable": "yes",
    "ts_duration":5000,
    "hls_path":"./hls"
}
```
配置hls_path，路径为切片的路径，如果路径不存在，生成切片的时候会动态创建，请确定有创建路径的权限。

## 6.3 使用举例
推流举例：
```markup
ffmpeg -re -i xxx.flv -c copy -f flv rtmp://x.x.x.x/live/livestream
```

在hls_path路径下，生成hls相关文件。

