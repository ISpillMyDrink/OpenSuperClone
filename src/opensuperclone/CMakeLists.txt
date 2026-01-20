set(SOURCES
    opensuperclone.c 
    commands.c
    io.c
    tool.c
    usbrelay.c
    common.c
    clone_gui.c
    asclepius.c
)

# Generate header files from resources
add_custom_command(
    OUTPUT opensuperclone_glade.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/opensuperclone.glade opensuperclone.glade
    COMMAND xxd -i opensuperclone.glade opensuperclone_glade.h
    DEPENDS opensuperclone.glade
)
LIST(APPEND RESOURCES opensuperclone_glade.h)

# Compile language files
file(GLOB PO_LANGS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/locale/*)
foreach(PO_LANG IN ITEMS ${PO_LANGS})
    if(IS_DIRECTORY ${PO_LANG})
        get_filename_component(PO_LANG_NAME ${PO_LANG} NAME)
        add_custom_command(
            OUTPUT locale/${PO_LANG_NAME}/LC_MESSAGES/opensuperclone.mo
            COMMAND ${CMAKE_COMMAND} -E make_directory locale/${PO_LANG_NAME}/LC_MESSAGES
            COMMAND msgfmt -o locale/${PO_LANG_NAME}/LC_MESSAGES/opensuperclone.mo ${PO_LANG}/opensuperclone.po
        )
        LIST(APPEND MO_FILES locale/${PO_LANG_NAME}/LC_MESSAGES/opensuperclone.mo)
    endif()
endforeach()

# Generate config file for OpenSuperClone
configure_file(config.h.in config.h)

# Add build target OpenSuperClone
add_executable(opensuperclone ${SOURCES} ${RESOURCES} ${MO_FILES})

# Include resources
target_include_directories(opensuperclone PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Add GTK dependency
target_include_directories(opensuperclone PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_directories(opensuperclone PRIVATE ${GTK3_LIBRARY_DIRS})
target_compile_options(opensuperclone PRIVATE ${GTK3_CFLAGS_OTHER})
target_link_libraries(opensuperclone ${GTK3_LIBRARIES})

# Add LibUSB dependency
target_link_libraries(opensuperclone ${LIBUSB_LIBRARIES})

# Add libconfig dependency
target_include_directories(opensuperclone PRIVATE ${LIBCONFIG_INCLUDE_DIRS})
target_link_directories(opensuperclone PRIVATE ${LIBCONFIG_LIBRARY_DIRS})
target_compile_options(opensuperclone PRIVATE ${LIBCONFIG_CFLAGS_OTHER})
target_link_libraries(opensuperclone ${LIBCONFIG_LIBRARIES})

target_compile_options(opensuperclone PRIVATE ${CC_OPTIONS})

# Install executable
install(TARGETS opensuperclone RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install script files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/oscscripts
        DESTINATION bin
)

# Install language files
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/locale
        DESTINATION share
)

# Install desktop files
install(FILES ${CMAKE_SOURCE_DIR}/res/opensuperclone.desktop
        DESTINATION share/applications
)

install(FILES ${CMAKE_SOURCE_DIR}/res/opensupertool.desktop
        DESTINATION share/applications
)

# Install resources
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/img
        DESTINATION share/opensuperclone
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/templates
        DESTINATION share/opensuperclone
)