# 3. rtmp module
几个文件就能实现rtmp的publish或play

划重点，因为设计网络io通信，使用了boost asio，使用必须编译和连接boost asio。

## 3.1 rtmp play
几个文件就可以完成rtmp的拉流操作，举例如下
``` markup
add_executable(rtmp_play_demo
            src/net/rtmp/client_demo/rtmp_play_demo.cpp
            src/net/rtmp/rtmp_client_session.cpp
            src/net/rtmp/rtmp_session_base.cpp
            src/net/rtmp/rtmp_control_handler.cpp
            src/net/rtmp/rtmp_handshake.cpp
            src/net/rtmp/chunk_stream.cpp
            src/format/flv/flv_mux.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
```
上面的例子，是rtmp拉去一个rtmp地址的流，然后写成一个flv文件。

只需要如下的cpp和相关hpp文件，就可以实现rtmp play:

src/net/rtmp/rtmp_client_session.cpp

src/net/rtmp/rtmp_session_base.cpp

src/net/rtmp/rtmp_control_handler.cpp

src/net/rtmp/rtmp_handshake.cpp

src/net/rtmp/chunk_stream.cpp

src/utils/logger.cpp

src/utils/data_buffer.cpp

src/utils/byte_stream.cpp

其中demo的程序入口是rtmp_play_demo.cpp，具体见CMakeList.txt

## 3.2 rtmp publish
几个文件就可以完成rtmp的推流操作，举例如下
``` markup
add_executable(rtmp_publish_demo
            src/net/rtmp/client_demo/rtmp_publish_demo.cpp
            src/net/rtmp/rtmp_client_session.cpp
            src/net/rtmp/rtmp_session_base.cpp
            src/net/rtmp/rtmp_control_handler.cpp
            src/net/rtmp/rtmp_handshake.cpp
            src/net/rtmp/chunk_stream.cpp
            src/format/flv/flv_demux.cpp
            src/format/flv/flv_mux.cpp
            src/utils/logger.cpp
            src/utils/data_buffer.cpp
            src/utils/byte_stream.cpp)
```
上面的例子，是demux一个flv文件，得出音视频数据，rtmp 推流啊到rtmp服务器。

只需要如下的cpp和相关hpp文件，就可以实现rtmp publish:

src/net/rtmp/rtmp_client_session.cpp

src/net/rtmp/rtmp_session_base.cpp

src/net/rtmp/rtmp_control_handler.cpp

src/net/rtmp/rtmp_handshake.cpp

src/net/rtmp/chunk_stream.cpp

src/format/flv/flv_demux.cpp

src/format/flv/flv_mux.cpp

src/utils/logger.cpp

src/utils/data_buffer.cpp

src/utils/byte_stream.cpp

其中demo的程序入口是rtmp_publish_demo.cpp，具体见CMakeList.txt