mkdir build

# Build for Release and create Debian package with CPack
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr

cd build
make
make package
