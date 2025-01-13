/****************************************************************************
**
** This file is part of the QMiniPCIDevice library.
** Copyright (C) 2024 Alexander E. <aekhv@vk.com>
** License: GNU GPL v2, see file LICENSE.
**
****************************************************************************/

#ifndef MINIPCI_H
#define MINIPCI_H

#include <linux/kernel.h>
#include <linux/ioctl.h>

typedef struct {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} minipci_driver_version_t;

typedef struct {
    uint8_t domain;     // Domain
    uint8_t bus;        // Bus
    uint8_t slot;       // Slot
    uint8_t func;       // Function
} minipci_device_location_t;

typedef struct {
    uint16_t vendorId;  // Vendor ID
    uint16_t deviceId;  // Device ID
    uint16_t classId;   // Class ID
    uint8_t revision;   // Revision
} minipci_device_id_t;

typedef struct {
    uint8_t speed;      // Bus speed (Gen.1, Gen.2, etc.)
    uint8_t width;      // Bus width (1x, 2x, 4x, etc.)
} minipci_device_link_t;

typedef struct {
    minipci_device_location_t location;
    minipci_device_id_t id;
    minipci_device_link_t link;
} minipci_device_info_t;

enum _MINIPCI_IOCTL {
    _MINIPCI_IOCTL_GET_DRIVER_VERSION = 0,
    _MINIPCI_IOCTL_GET_DEVICE_INFO
};

#define MINIPCI_IOCTL_BASE '#'

#define MINIPCI_IOCTL_GET_DRIVER_VERSION   _IOR(MINIPCI_IOCTL_BASE, _MINIPCI_IOCTL_GET_DRIVER_VERSION, minipci_driver_version_t)
#define MINIPCI_IOCTL_GET_DEVICE_INFO      _IOR(MINIPCI_IOCTL_BASE, _MINIPCI_IOCTL_GET_DEVICE_INFO, minipci_device_info_t)

#endif // MINIPCI_H
