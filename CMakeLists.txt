cmake_minimum_required(VERSION 3.10)

# OpenSuperClone version 2.4
project(OpenSuperClone
        VERSION 2.4
        DESCRIPTION "Advanced disk utility"
        HOMEPAGE_URL "https://github.com/ISpillMyDrink/OpenSuperClone"
        LANGUAGES C
)

# OSCViewer version 1.0
set(OSCVIEWER_VERSION 1.0 CACHE STRING "OSCViewer Version")

# Path variables for language and script files
# TODO: Remove the need for most of these
set(OSC_INSTALL_PATH "/usr/local/bin/" CACHE STRING "OpenSuperClone Installation Directory")
set(OSC_SCRIPT_PATH "/usr/local/bin/oscscripts/" CACHE STRING "OpenSuperClone Script Directory")
set(OSC_SCRIPT_MENU "/usr/local/bin/oscscripts/hddmenu" CACHE STRING "OpenSuperClone Script Menu File")
set(OSC_LANG_PATH "/usr/local/share/doc/opensuperclone/Language" CACHE STRING "OpenSuperClone Language Directory")
set(OSCVIEWER_LANG_PATH "/usr/local/share/doc/oscviewer/Language" CACHE STRING "OSCViewer Language Directory")

# Copyright year 2022
set(COPYRIGHT_YEAR 2022 CACHE STRING "Copyright Year")

# Set the GTK version to 3.x
set(GTKVER 3 CACHE STRING "GTK Version")

# If Git is installed, save the current revision number
find_package(Git)

if(Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                    OUTPUT_VARIABLE GIT_REVISION
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    COMMAND_ERROR_IS_FATAL ANY
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

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