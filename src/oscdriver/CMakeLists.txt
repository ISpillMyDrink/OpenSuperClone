set(SOURCES
    oscdriver.c
)

# Generate config file for OSCDriver
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Install the config file, the sources, the DKMS config and the Makefile to ./src/oscdriver-${OSC_DRIVER_VERSION}
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/config.h DESTINATION ${CMAKE_INSTALL_PREFIX}/src/oscdriver-${OSC_DRIVER_VERSION})
install(FILES ${SOURCES} DESTINATION ${CMAKE_INSTALL_PREFIX}/src/oscdriver-${OSC_DRIVER_VERSION})
install(FILES dkms.conf DESTINATION ${CMAKE_INSTALL_PREFIX}/src/oscdriver-${OSC_DRIVER_VERSION})
install(FILES Makefile DESTINATION ${CMAKE_INSTALL_PREFIX}/src/oscdriver-${OSC_DRIVER_VERSION})