# 2 mpegts module
几个cpp和hpp就能完成mpegts的mux/demux

## 2.1 mpegts demux
只需要编译如下文件，就能完成一个mpegts demux的操作，举例:

```markup
add_executable(mpegts_demux_demo
            src/format/mpegts/mpegts_demux_demo.cpp
            src/format/mpegts/mpegts_demux.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
```
上面这个例子是demux解析一个mpegs文件，输出具体的音频、视频数据等信息。

也就是，只需要包含文件:

src/format/mpegts/mpegts_demux.cpp

src/utils/logger.cpp

src/utils/data_buffer.cpp

src/utils/byte_stream.cpp

mpegts_demux_demo.cpp是这个例子的demo入口。

## 2.2 mpegts mux
只需要编译如下文件，就能完成一个mpegts mux的操作，举例:
```markup
add_executable(mpegts_mux_demo
            src/format/mpegts/mpegts_mux_demo.cpp
            src/format/mpegts/mpegts_mux.cpp
            src/format/flv/flv_demux.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
```
上面这个例子是demux解析一个flv文件，然后在mux输出一个mpegts文件。

也就是，只需要包含文件:

src/format/mpegts/mpegts_mux.cpp

src/format/flv/flv_demux.cpp

src/utils/logger.cpp

src/utils/data_buffer.cpp

src/utils/byte_stream.cpp

mpegts_mux_demo.cpp是这个例子程序的入口。