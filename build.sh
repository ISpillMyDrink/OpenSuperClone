mkdir build

# Build for Release
cmake -S . -B ./build -DCMAKE_BUILD_TYPE=Release

cd build
make
mv ./src/opensuperclone/opensuperclone ../bin/opensuperclone
mv ./src/oscviewer/oscviewer ../bin/oscviewer