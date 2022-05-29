# 0. 编译
# 0.1 全编译
直接执行:

./build.sh

编译后，执行程序在objs路径下

# 0.2 增量编译
在已经编译过的情况下，已经存在objs目录

cd objs

cmake ..

make -j 2



