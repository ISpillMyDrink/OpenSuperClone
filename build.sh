mkdir build

# Build for Debug and install locally
# cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=./debug

# Build for Release and install locally
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=./release

# Build for Release and install to /usr/local/
# cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local/

cd build
make
make install