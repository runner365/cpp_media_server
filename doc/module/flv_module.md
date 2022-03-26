# 1. flv module
几个cpp和hpp就能完成mpegts的mux/demux

## 1.1 flv demux
只需要编译如下文件，就能完成一个flv demux的操作，举例:
```markup
add_executable(flv_demux_demo
            ./src/format/flv/flv_demux.cpp
            ./src/format/flv/flv_demux_demo.cpp
            ./src/utils/logger.cpp
            ./src/utils/data_buffer.cpp
            ./src/utils/byte_stream.cpp)
```
上面这个demo，实现把一个flv做demux，解析出音视频具体的数据。

也就是，只需要包含文件:

./src/format/flv/flv_demux.cpp

./src/utils/logger.cpp

./src/utils/data_buffer.cpp

./src/utils/byte_stream.cpp

并包含对应的.h文件，就能实现flv demux的操作。

flv_demux_demo.cpp为demo, 具体见CMakeLists.txt中的例子

## 1.2 flv mux
只需要编译如下文件，就能完成一个flv mux的操作，举例:
```markup
add_executable(flv_mux_demo
            src/format/flv/flv_mux.cpp
            src/format/flv/flv_demux.cpp
            src/format/flv/flv_mux_demo.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
```
上面这个demo，实现把一个flv文件先demux后，再次mux成一个新的flv。

也就是，只需要包含文件:

src/format/flv/flv_mux.cpp

src/utils/logger.cpp

src/utils/data_buffer.cpp

src/utils/byte_stream.cpp

并包含对应的.h文件，就能实现flv mux的操作。