set(SOURCES
    oscviewer${GTKVER}.c
    oscviewer${GTKVER}.h
)

# Generate header file from glade file
add_custom_command(
    OUTPUT oscviewer${GTKVER}_glade.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/oscviewer${GTKVER}.glade oscviewer${GTKVER}.glade
    COMMAND xxd -i oscviewer${GTKVER}.glade oscviewer${GTKVER}_glade.h
)

# Include generated header file
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(oscviewer ${SOURCES} oscviewer${GTKVER}_glade.h)

target_link_libraries(oscviewer ${LIBRARIES})

target_compile_options(oscviewer PRIVATE ${GCC_OPTIONS})