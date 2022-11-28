find_package(PkgConfig REQUIRED)
find_package(Gettext REQUIRED)

# Find GTK dependency
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Find LibUSB dependency
pkg_check_modules(LIBUSB REQUIRED libusb)

add_subdirectory(opensuperclone)
add_subdirectory(oscviewer)
#add_subdirectory(create_script_help)