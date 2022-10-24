set(SOURCES
    oscviewer${GTKVER}.c
    oscviewer${GTKVER}.h
)

# Generate header files from resources
add_custom_command(
    OUTPUT oscviewer${GTKVER}_glade.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/oscviewer${GTKVER}.glade oscviewer${GTKVER}.glade
    COMMAND xxd -i oscviewer${GTKVER}.glade oscviewer${GTKVER}_glade.h
)
LIST(APPEND RESOURCES oscviewer${GTKVER}_glade.h)

# Generate config file for OSCViewer
configure_file(config.h.in config.h)

# Add build target OSCViewer
add_executable(oscviewer ${SOURCES} ${RESOURCES})

# Include resources
target_include_directories(oscviewer PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Add GTK dependency
if(GTKVER EQUAL 2)
    target_include_directories(oscviewer PRIVATE ${GTK2_INCLUDE_DIRS})
    target_link_directories(oscviewer PRIVATE ${GTK2_LIBRARY_DIRS})
    target_compile_options(oscviewer PRIVATE ${GTK2_CFLAGS_OTHER})
    target_link_libraries(oscviewer ${GTK2_LIBRARIES})
elseif(GTKVER EQUAL 3)
    target_include_directories(oscviewer PRIVATE ${GTK3_INCLUDE_DIRS})
    target_link_directories(oscviewer PRIVATE ${GTK3_LIBRARY_DIRS})
    target_compile_options(oscviewer PRIVATE ${GTK3_CFLAGS_OTHER})
    target_link_libraries(oscviewer ${GTK3_LIBRARIES})
endif()

target_compile_options(oscviewer PRIVATE ${CC_OPTIONS})

# Install executable
install(TARGETS oscviewer)

# Install language files
install(DIRECTORY ${CMAKE_SOURCE_DIR}/res/oscviewer
        DESTINATION share/doc
)

# Install desktop file
install(FILES ${CMAKE_SOURCE_DIR}/res/oscviewer.desktop
        DESTINATION share/applications
)