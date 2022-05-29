# 1. 编译
# 1.1 安装C++17
## 1.1.1 centos安装c++11
```markup
yum install centos-release-scl -y
yum install devtoolset-8-gcc* -y
source /opt/rh/devtoolset-8/enable
```
## 1.1.2 ubuntu安装c++11
```markup
apt-get install -y gcc-8 g++-8
```
### 1.1.3 bzip2
```markup
yum install -y bzip2
yum install -y bzip2-devel
```


# 1.2 全编译
直接执行:

./build.sh

编译后，执行程序在objs路径下

# 1.2 增量编译
在已经编译过的情况下，已经存在objs目录

cd objs

cmake ..

make -j 2



