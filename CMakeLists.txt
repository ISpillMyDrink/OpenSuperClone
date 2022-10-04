cmake_minimum_required(VERSION 3.10)

# OpenSuperClone version 2.4
project(OpenSuperClone VERSION 2.4 HOMEPAGE_URL https://github.com/ISpillMyDrink/OpenSuperClone)

# Set the GTK version to 2.x
set(GTKVER 2 CACHE STRING "GTK Version")

# Set compiler flags
set(CC_OPTIONS
    -Wall
    -Wextra
    -Wno-deprecated-declarations
    -fcommon
)

# Set linker flags
set(CMAKE_EXE_LINKER_FLAGS
    -export-dynamic
)

# Add source directory
add_subdirectory(src)