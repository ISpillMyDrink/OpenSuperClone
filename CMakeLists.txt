cmake_minimum_required(VERSION 3.10)

# OpenSuperClone version 2.3.3
project(OpenSuperClone VERSION 2.3.3)

# Set the GTK version to 2.x
set(GTKVER 2)

set(CMAKE_C_COMPILER gcc)

set(GCC_OPTIONS
    -Wall
    -W
    -Wno-deprecated-declarations
    -fcommon
    -lcurl
)

set(CMAKE_EXE_LINKER_FLAGS "-export-dynamic")

add_subdirectory(src)