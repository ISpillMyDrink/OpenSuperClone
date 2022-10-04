# Find GTK dependency
if(GTKVER EQUAL 2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
elseif(GTKVER EQUAL 3)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
endif()

# Find LibUSB dependency
find_library(LIBUSB_LIBRARY
    NAMES usb
    PATH_SUFFIXES "lib" "lib32" "lib64"
)

add_subdirectory(opensuperclone)
add_subdirectory(oscviewer)
#add_subdirectory(create_script_help)