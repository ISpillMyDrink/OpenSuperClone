set(SOURCES
    driver.h
    ioctl.c
    ioctl.h
    main.c
    minipci.h
    nvme.c
    nvme.h
)

# Generate config file for Mininvme
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Configure the DKMS config file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/dkms.conf.in ${CMAKE_CURRENT_BINARY_DIR}/dkms.conf)

# Install the driver to the source directory
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/dkms.conf DESTINATION ${CMAKE_INSTALL_PREFIX}/src/mininvme-${MININVME_VERSION})
install(FILES ${SOURCES} DESTINATION ${CMAKE_INSTALL_PREFIX}/src/mininvme-${MININVME_VERSION})
install(FILES Makefile DESTINATION ${CMAKE_INSTALL_PREFIX}/src/mininvme-${MININVME_VERSION})