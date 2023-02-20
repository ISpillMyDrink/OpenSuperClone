#!/bin/sh

[ ! -d build ] && mkdir build

CMAKE_OPTIONS=""
CMAKE_BUILD_TYPE=Release
CMAKE_INSTALL_PREFIX=/usr

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

echo "Installing OpenSuperClone to $CMAKE_INSTALL_PREFIX..."
sudo make install

if [ $? -ne 0 ]; then
    echo "Install failed. Aborting."
    exit 1
else
    echo "Install succeeded."
fi

if ! command -v dkms >/dev/null; then
    echo "WARNING: DKMS is not installed, skipping driver installation."
    echo "To use the advanced features of OpenSuperClone you will have to install the driver manually."
    exit 0
fi

VERSION=$(grep -oP '(?<=set\(OSC_DRIVER_VERSION ).*(?=\))' ../CMakeLists.txt)
VERSION=$(echo $VERSION | cut -d' ' -f1)
echo "Found OSCDriver version $VERSION."

echo "Installing OSCDriver..."
sudo dkms install -m oscdriver -v $VERSION

if [ $? -ne 0 ]; then
    echo "Failed to install OSCDriver."
    exit 1
else
    echo "Installed OSCDriver."
fi

echo "Done."
