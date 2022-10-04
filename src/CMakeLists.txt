find_package(PkgConfig REQUIRED)

# Find GTK dependency
if(GTKVER EQUAL 2)
    pkg_check_modules(GTK2 REQUIRED gtk+-2.0)
elseif(GTKVER EQUAL 3)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
endif()

# Find LibUSB dependency
pkg_check_modules(LIBUSB REQUIRED libusb)

add_subdirectory(opensuperclone)
add_subdirectory(oscviewer)
#add_subdirectory(create_script_help)