
if [ ! -d "__cmake" ]; then
  mkdir __cmake
fi

cd __cmake
cmake ..
make -j 4