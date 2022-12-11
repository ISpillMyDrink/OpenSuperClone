cmake_minimum_required(VERSION 3.10)

# Disallow in-source building
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR “In-source builds not allowed! Create a build directory and run CMake from there.”)
endif()

# Force a default build type if none was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to Release as none was specified.")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

# OpenSuperClone version 2.4
project(OpenSuperClone
        VERSION 2.4.1
        DESCRIPTION "Advanced disk cloning utility"
        HOMEPAGE_URL "https://github.com/ISpillMyDrink/OpenSuperClone"
        LANGUAGES C
)

# OSCDriver version 2.6.1
set(OSC_DRIVER_VERSION 2.6.1 CACHE STRING "OSCDriver Version")

set(OSC_DRIVER_IOCTL_NAME "oscdriverc" CACHE STRING "OSCDriver IOCTL Name")
set(OSC_DRIVER_MMAP_NAME "oscdrivermap_m" CACHE STRING "OSCDriver MMAP Name")
set(OSC_DRIVER_MMAPTB_NAME "oscdrivermap_tb" CACHE STRING "OSCDriver MMAPTB Name")
set(OSC_DRIVER_MMAPMDB_NAME "oscdrivermap_mdb" CACHE STRING "OSCDriver MMAPMDB Name")

# If the DEBUG option is enabled, set the build type to Debug and add the define DEBUG
option(DEBUG "Enable Debugging" OFF)

if(DEBUG)
    set(CMAKE_BUILD_TYPE Debug)
    add_definitions(-DDEBUG)
endif()

# Copyright year 2022
set(COPYRIGHT_YEAR 2022 CACHE STRING "Copyright Year")

# Path variables for language and script files
set(OSC_INSTALL_PATH "${CMAKE_INSTALL_PREFIX}/bin/" CACHE STRING "OpenSuperClone Installation Directory")
set(OSC_SCRIPT_PATH "${CMAKE_INSTALL_PREFIX}/bin/oscscripts/" CACHE STRING "OpenSuperClone Script Directory")
set(OSC_SCRIPT_MENU "${CMAKE_INSTALL_PREFIX}/bin/oscscripts/hddmenu" CACHE STRING "OpenSuperClone Script Menu File")
set(OSC_LANG_PATH "${CMAKE_INSTALL_PREFIX}/share/locale/" CACHE STRING "OpenSuperClone Language Directory")

# If Git is installed, save the current revision number
find_package(Git)

if(Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
                    OUTPUT_VARIABLE GIT_REVISION
                    OUTPUT_STRIP_TRAILING_WHITESPACE
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

# Package compiled binaries with CPack
set(CPACK_BUNDLE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_CONTACT "ISpillMyDrink")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/package")

set(CPACK_GENERATOR DEB ZIP CACHE STRING "CPack Generator")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgtk-3-0, libusb-0.1-4, dkms")
set(CPACK_DEBIAN_PACKAGE_RELEASE 1)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")

# Add Readme and License files to ZIP package
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_ARCHIVE_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE ON)

install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        DESTINATION .
        COMPONENT Metadata
        EXCLUDE_FROM_ALL
)

# Add installation scripts to ZIP package
install(DIRECTORY
        "${CMAKE_CURRENT_SOURCE_DIR}/res/install/"
        DESTINATION .
        COMPONENT Scripts
        EXCLUDE_FROM_ALL
)

Include(CPack)