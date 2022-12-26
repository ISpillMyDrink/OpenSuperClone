#!/bin/sh

[ ! -d build ] && mkdir build

CMAKE_OPTIONS=""
CMAKE_BUILD_TYPE=Release
CMAKE_INSTALL_PREFIX=./Release

if [ "$1" = "debug" ]; then
    CMAKE_OPTIONS="-DDEBUG=ON"
    CMAKE_BUILD_TYPE=Debug
    CMAKE_INSTALL_PREFIX=./Debug
fi

echo "Configuring for $CMAKE_BUILD_TYPE..."
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=$CMAKE_BUILD_TYPE -DCMAKE_INSTALL_PREFIX=$CMAKE_INSTALL_PREFIX $CMAKE_OPTIONS

if [ $? -ne 0 ]; then
    echo "CMake failed. Aborting."
    exit 1
else
    echo "CMake succeeded."
fi

if [ -z "$THREADS" ]; then
    THREADS=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 1)
fi

echo "Building with $THREADS threads..."
cd build
make -j$THREADS

if [ $? -ne 0 ]; then
    echo "Build failed. Aborting."
    exit 1
else
    echo "Build succeeded."
fi

echo "Installing..."
make install

if [ $? -ne 0 ]; then
    echo "Install failed. Aborting."
    exit 1
else
    echo "Install succeeded."
fi

echo "Done."
