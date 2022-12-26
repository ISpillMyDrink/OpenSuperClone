#!/bin/sh

[ ! -d build ] && mkdir build

echo "Configuring..."

cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

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

echo "Packaging..."
make package

if [ $? -ne 0 ]; then
    echo "Packaging failed. Aborting."
    exit 1
else
    echo "Packaging succeeded."
fi

echo "Done."
