rm -r build
mkdir build

# Build for Release and install locally
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./release

cd build
make
make install
