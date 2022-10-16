mkdir build

# Build for Debug and install locally
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=./debug

cd build
make
make install
