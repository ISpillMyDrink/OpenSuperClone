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
    clone_gui${GTKVER}.c
    clone_gui${GTKVER}.h
    clone_gui_language.c
    clone_gui_language.h
)

# Generate header files from resources
add_custom_command(
    OUTPUT opensuperclone_EULA.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/opensuperclone_EULA.txt opensuperclone_EULA.txt
    COMMAND xxd -i opensuperclone_EULA.txt opensuperclone_EULA.h
)
LIST(APPEND RESOURCES opensuperclone_EULA.h)

add_custom_command(
    OUTPUT opensuperclone_help.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/opensuperclone_help.txt opensuperclone_help.txt
    COMMAND xxd -i opensuperclone_help.txt opensuperclone_help.h
)
LIST(APPEND RESOURCES opensuperclone_help.h)

add_custom_command(
    OUTPUT opensuperclone${GTKVER}_glade.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/opensuperclone${GTKVER}.glade opensuperclone${GTKVER}.glade
    COMMAND xxd -i opensuperclone${GTKVER}.glade opensuperclone${GTKVER}_glade.h
)
LIST(APPEND RESOURCES opensuperclone${GTKVER}_glade.h)

add_custom_command(
    OUTPUT opensupertool_help.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/opensupertool_help.txt opensupertool_help.txt
    COMMAND xxd -i opensupertool_help.txt opensupertool_help.h
)
LIST(APPEND RESOURCES opensupertool_help.h)

add_custom_command(
    OUTPUT opensuperclone_driver.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/driver/opensuperclone_driver.c opensuperclone_driver.c
    COMMAND xxd -i opensuperclone_driver.c opensuperclone_driver.h
)
LIST(APPEND RESOURCES opensuperclone_driver.h)

# Include generated header files
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(LIBRARIES ${LIBRARIES} ${LIBUSB_LIBRARY})

add_executable(opensuperclone ${SOURCES} ${RESOURCES})
target_link_libraries(opensuperclone ${LIBRARIES})
target_compile_options(opensuperclone PRIVATE ${GCC_OPTIONS})