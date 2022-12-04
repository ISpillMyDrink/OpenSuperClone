[ ! -d build ] && mkdir build

echo "Configuring..."
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

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

echo "Packaging..."
make package

if [ $? -ne 0 ]; then
    echo "Packaging failed. Aborting."
    exit 1
else
    echo "Packaging succeeded."
fi