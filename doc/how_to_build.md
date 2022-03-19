# 0. 安装boost asio
## 0.1 下载boost库
下载地址boost v1.76: [boost v1.76](https://boostorg.jfrog.io/ui/native/main/release/1.76.0/source/)

## 0.2 How to build
如何在Mac/linux内编译
```markup
./bootstrap.sh
./b2 -j 4 --with-system --with-thread --with-date_time --with-regex --with-serialization stage
./b2 install
```