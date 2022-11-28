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

# Compile language files
file(GLOB PO_LANGS LIST_DIRECTORIES true ${CMAKE_CURRENT_SOURCE_DIR}/locale/*)
foreach(PO_LANG IN ITEMS ${PO_LANGS})
    if(IS_DIRECTORY ${PO_LANG})
        get_filename_component(PO_LANG_NAME ${PO_LANG} NAME)
        add_custom_command(
            OUTPUT locale/${PO_LANG_NAME}/LC_MESSAGES/oscviewer.mo
            COMMAND ${CMAKE_COMMAND} -E make_directory locale/${PO_LANG_NAME}/LC_MESSAGES
            COMMAND msgfmt -o locale/${PO_LANG_NAME}/LC_MESSAGES/oscviewer.mo ${PO_LANG}/oscviewer.po
        )
        LIST(APPEND MO_FILES locale/${PO_LANG_NAME}/LC_MESSAGES/oscviewer.mo)
    endif()
endforeach()

# Generate config file for OSCViewer
configure_file(config.h.in config.h)

# Add build target OSCViewer
add_executable(oscviewer ${SOURCES} ${RESOURCES} ${MO_FILES})

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
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/locale
        DESTINATION share
)

# Install desktop file
install(FILES ${CMAKE_SOURCE_DIR}/res/oscviewer.desktop
        DESTINATION share/applications
)