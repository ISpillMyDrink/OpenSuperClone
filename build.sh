mkdir -p build

cd ./build

cmake ../

make

mv ./src/opensuperclone/opensuperclone ../bin/opensuperclone
mv ./src/oscviewer/oscviewer ../bin/oscviewer