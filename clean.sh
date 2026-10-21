#!/bin/sh
# Clean the OpenSuperClone project build artifacts.
# Usage:
#   ./clean.sh

echo "Cleaning up..."

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

echo "Done."
