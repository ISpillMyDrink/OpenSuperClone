if [ -d build ]; then
    rm -rf build
fi

if [ -d Debug ]; then
    rm -rf Debug
fi

if [ -d Release ]; then
    rm -rf Release
fi

if [ -d package ]; then
    rm -rf package
fi