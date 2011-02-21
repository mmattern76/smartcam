#/bin/bash

sed "s:OVEROTOP_MARK:$OVEROTOP:g" config/CMakeLists.txt > src/CMakeLists.txt

mkdir bin
mkdir lib
mkdir build

cd build
cmake -G "Eclipse CDT4 - Unix Makefiles"  ../src/
