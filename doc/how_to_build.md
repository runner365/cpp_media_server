## preinstall
How to install boost.asio
### Download boost
We suggenst boost v1.76: [boost v1.76](https://boostorg.jfrog.io/ui/native/main/release/1.76.0/source/)

### How to build
Build boost asio in Mac/linux
<pre>
./bootstrap.sh
./b2 -j 4 --with-system --with-thread --with-date_time --with-regex --with-serialization stage
./b2 install
</pre>