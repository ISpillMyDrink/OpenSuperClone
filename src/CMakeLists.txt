# Add GTK dependency
if(GTKVER EQUAL 2)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK2 REQUIRED gtk+-2.0)

    include_directories(${GTK2_INCLUDE_DIRS})
    link_directories(${GTK2_LIBRARY_DIRS})

    add_definitions(${GTK2_CFLAGS_OTHER})

    set(LIBRARIES ${LIBRARIES} ${GTK2_LIBRARIES})
elseif(GTKVER EQUAL 3)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)

    include_directories(${GTK3_INCLUDE_DIRS})
    link_directories(${GTK3_LIBRARY_DIRS})

    add_definitions(${GTK3_CFLAGS_OTHER})

    set(LIBRARIES ${LIBRARIES} ${GTK3_LIBRARIES})
endif()

# Add LibUSB dependency
find_library(LIBUSB_LIBRARY
    NAMES usb
    PATH_SUFFIXES "lib" "lib32" "lib64"
)

add_subdirectory(opensuperclone)
add_subdirectory(oscviewer)
#add_subdirectory(create_script_help)