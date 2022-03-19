
if [ ! -d "objs" ]; then
  mkdir objs
else
  rm -rf objs
  mkdir objs
fi

cd objs
cmake ..
make -j 4