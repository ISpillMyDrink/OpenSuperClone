mkdir build

cd ./build

cmake -DCMAKE_BUILD_TYPE=Release ../

make

mv ./src/opensuperclone/opensuperclone ../bin/opensuperclone
mv ./src/oscviewer/oscviewer ../bin/oscviewer