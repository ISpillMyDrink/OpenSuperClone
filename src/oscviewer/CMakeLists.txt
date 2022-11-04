set(SOURCES
    oscviewer.c
    oscviewer.h
)

# Generate header files from resources
add_custom_command(
    OUTPUT oscviewer_glade.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/oscviewer.glade oscviewer.glade
    COMMAND xxd -i oscviewer.glade oscviewer_glade.h
)
LIST(APPEND RESOURCES oscviewer_glade.h)

# Generate config file for OSCViewer
configure_file(config.h.in config.h)

# Add build target OSCViewer
add_executable(oscviewer ${SOURCES} ${RESOURCES})

# Include resources
target_include_directories(oscviewer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Add GTK dependency
target_include_directories(oscviewer PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_directories(oscviewer PRIVATE ${GTK3_LIBRARY_DIRS})
target_compile_options(oscviewer PRIVATE ${GTK3_CFLAGS_OTHER})
target_link_libraries(oscviewer ${GTK3_LIBRARIES})

target_compile_options(oscviewer PRIVATE ${CC_OPTIONS})

# Install executable
install(TARGETS oscviewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install language files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/oscviewer
        DESTINATION share/doc
)

# Install desktop file
install(FILES ${CMAKE_SOURCE_DIR}/res/oscviewer.desktop
        DESTINATION share/applications
)