cmake_minimum_required(VERSION 3.10)

# OpenSuperClone version 2.3.3
project(OpenSuperClone VERSION 2.3.3)

# Set the GTK version to 2.x
set(GTKVER 2 CACHE STRING "GTK Version")

set(GCC_OPTIONS
    -Wall
    -Wextra
    -Wno-deprecated-declarations
    -fcommon
)

set(CMAKE_EXE_LINKER_FLAGS "-export-dynamic")

add_subdirectory(src)