# cpp_media_server

A media server is writen by C++11, and the network io is write by Boost.Asio.

## preinstall
How to install boost.asio
* Download boost

We suggenst boost v1.76: [boost v1.76](https://boostorg.jfrog.io/ui/native/main/release/1.76.0/source/)
* How to build boost asio in Mac/linux

step 1st: ./bootstrap.sh

step 2nd: ./b2 install -j 4 --with-system --with-thread --with-date_time --with-regex --with-serialization stage && ./b2 install

## What does It support
network protocol feature:
* rtmp
* httpflv
* websocket(webcodec encode media in flv(h264+opus) over websocket)

media format support:
* flv(h264/vp8/vp9, aac/opus)


