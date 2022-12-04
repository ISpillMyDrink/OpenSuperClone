[ ! -d build ] && mkdir build

echo "Configuring..."
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./Release

if [ $? -ne 0 ]; then
    echo "CMake failed. Aborting."
    exit 1
else
    echo "CMake succeeded."
fi

echo "Building..."
cd build
make

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