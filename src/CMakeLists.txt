# Find PkgConfig dependency
find_package(PkgConfig REQUIRED)

# Find Gettext dependency
find_package(Gettext REQUIRED)

# Find GTK dependency
pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

# Find LibUSB dependency
pkg_check_modules(LIBUSB REQUIRED libusb)

# Find libconfig dependency
pkg_check_modules(LIBCONFIG REQUIRED libconfig)

add_subdirectory(opensuperclone)
add_subdirectory(oscdriver)
add_subdirectory(oscviewer)
add_subdirectory(mininvme)

# Configure the postinst and prerm scripts for the DEB package and put them in the root of the build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/postinst.in ${CMAKE_BINARY_DIR}/postinst)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/prerm.in ${CMAKE_BINARY_DIR}/prerm)