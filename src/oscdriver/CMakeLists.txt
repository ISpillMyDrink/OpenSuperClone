set(SOURCES
    oscdriver.c
)

# Generate config file for OSCDriver
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Configure the DKMS config file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dkms.conf.in ${CMAKE_CURRENT_BINARY_DIR}/dkms.conf)

# Configure the postinst and prerm scripts for the DEB package and put them in the root of the build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/postinst.in ${CMAKE_BINARY_DIR}/postinst)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/prerm.in ${CMAKE_BINARY_DIR}/prerm)

# Install the driver to the source directory
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/config.h DESTINATION ${CMAKE_INSTALL_PREFIX}/src/oscdriver-${OSC_DRIVER_VERSION})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dkms.conf DESTINATION ${CMAKE_INSTALL_PREFIX}/src/oscdriver-${OSC_DRIVER_VERSION})
install(FILES ${SOURCES} DESTINATION ${CMAKE_INSTALL_PREFIX}/src/oscdriver-${OSC_DRIVER_VERSION})
install(FILES Makefile DESTINATION ${CMAKE_INSTALL_PREFIX}/src/oscdriver-${OSC_DRIVER_VERSION})