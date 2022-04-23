# 0. 安装boost asio
## 0.1 下载boost库
下载地址boost v1.76: [boost v1.76](https://boostorg.jfrog.io/ui/native/main/release/1.76.0/source/)

推荐gcc 8.4以上

## 0.2 安装c++11
### 0.2.1 centos安装c++11
```markup
yum install centos-release-scl -y
yum install devtoolset-8-gcc* -y
source /opt/rh/devtoolset-8/enable
```

### 0.2.2 ubuntu安装c++11
```markup
apt-get install -y gcc-8 g++-8
```

## 0.3 How to build
如何在Mac/linux内编译
```markup
./bootstrap.sh
./b2 -j 4 --with-system --with-thread --with-date_time --with-regex --with-serialization stage
./b2 install
```
