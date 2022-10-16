mkdir build

# Build for Release and install to /usr/local/
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local/

cd build
make
sudo make install
