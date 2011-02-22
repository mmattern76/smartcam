#/bin/bash

echo "Config Overo ..."

sed "s:OVEROTOP_MARK:$OVEROTOP:g" config/CMakeLists.txt > src/CMakeLists.txt

echo "Clean previous build files"
rm -rf bin
rm -rf lib
rm -rf build

mkdir bin
mkdir lib
mkdir build

cd build
cmake -G "Eclipse CDT4 - Unix Makefiles"  ../src/



echo "Config Overo ..."

cd ../pc-client

echo "Clean previous build files"
rm -rf bin
rm -rf lib
rm -rf build

mkdir bin
mkdir lib
mkdir build

cd build
cmake -G "Eclipse CDT4 - Unix Makefiles" -D CMAKE_BUILD_TYPE=Debug ../src/
