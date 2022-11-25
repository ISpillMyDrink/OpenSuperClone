set(SOURCES
    opensuperclone.c 
    opensuperclone.h
    commands.c
    commands.h
    io.c
    io.h
    tool.c
    tool.h
    usbrelay.c
    usbrelay.h
    common.c
    common.h
    clone_gui.c
    clone_gui.h
    clone_gui_language.c
    clone_gui_language.h
)

# Generate header files from resources
add_custom_command(
    OUTPUT opensuperclone_help.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/opensuperclone_help.txt opensuperclone_help.txt
    COMMAND xxd -i opensuperclone_help.txt opensuperclone_help.h
)
LIST(APPEND RESOURCES opensuperclone_help.h)

add_custom_command(
    OUTPUT opensuperclone_glade.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/opensuperclone.glade opensuperclone.glade
    COMMAND xxd -i opensuperclone.glade opensuperclone_glade.h
)
LIST(APPEND RESOURCES opensuperclone_glade.h)

add_custom_command(
    OUTPUT opensuperclone_driver.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/driver/opensuperclone_driver.c opensuperclone_driver.c
    COMMAND xxd -i opensuperclone_driver.c opensuperclone_driver.h
)
LIST(APPEND RESOURCES opensuperclone_driver.h)

# Generate config file for OpenSuperClone
configure_file(config.h.in config.h)

# Add build target OpenSuperClone
add_executable(opensuperclone ${SOURCES} ${RESOURCES})

# Include resources
target_include_directories(opensuperclone PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Add GTK dependency
target_include_directories(opensuperclone PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_directories(opensuperclone PRIVATE ${GTK3_LIBRARY_DIRS})
target_compile_options(opensuperclone PRIVATE ${GTK3_CFLAGS_OTHER})
target_link_libraries(opensuperclone ${GTK3_LIBRARIES})

# Add LibUSB dependency
target_link_libraries(opensuperclone ${LIBUSB_LIBRARIES})

target_compile_options(opensuperclone PRIVATE ${CC_OPTIONS})

# Install executable
install(TARGETS opensuperclone RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install script files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/oscscripts
        DESTINATION bin
)

# Install language files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/opensuperclone
        DESTINATION share/doc
)

# Install desktop files
install(FILES ${CMAKE_SOURCE_DIR}/res/opensuperclone.desktop
        DESTINATION share/applications
)

install(FILES ${CMAKE_SOURCE_DIR}/res/opensupertool.desktop
        DESTINATION share/applications
)