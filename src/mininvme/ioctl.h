/****************************************************************************
**
** This file is part of the MiniNVMe project.
** Copyright (C) 2024 Alexander E. <aekhv@vk.com>
** License: GNU GPL v2, see file LICENSE.
**
****************************************************************************/

#ifndef IOCTL_H
#define IOCTL_H

#include <linux/kernel.h>
#include <linux/ioctl.h>
#include "minipci.h"

typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t tertiary;
} nvme_controller_version_t;

typedef struct {
    bool enabled;
    bool ready;
    bool fatal;
} nvme_controller_state_t;

typedef struct {
    uint8_t opc;        // Opcode
    uint32_t nsid;      // Namespace Identifier
    uint32_t cdw10;     // Command dwords 10...15
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} nvme_command_t;

typedef struct {
    uint8_t *pointer;   // Buffer pointer
    uint32_t length;    // Buffer length
} nvme_buffer_t;

typedef struct {
    uint8_t sct;        // Status code type
    uint8_t sc;         // Status code
    bool more;          // More bit
    bool dnr;           // Do not retry bit
    bool timeout;       // Timeout bit
} nvme_status_t;

typedef struct {
    nvme_command_t cmd;
    nvme_buffer_t buffer;
    nvme_status_t status;
} nvme_command_packet_t;

typedef struct {
    uint64_t offset;    // LBA offset
    uint32_t count;     // LBA count
} nvme_lba_t;

typedef struct {
    uint32_t nsid;      // Namespace Identifier
    nvme_lba_t lba;
    nvme_buffer_t buffer;
    nvme_status_t status;
} nvme_lba_packet_t;

typedef struct {
    uint32_t value;     // Timeout in milliseconds
} nvme_timeout_t;

enum _NVME_IOCTL {
    _NVME_IOCTL_GET_CONTROLLER_VERSION = 0x20,
    _NVME_IOCTL_GET_CONTROLLER_STATE,
    _NVME_IOCTL_RUN_ADMIN_COMMAND,
    _NVME_IOCTL_RUN_IO_COMMAND,
    _NVME_IOCTL_READ_SECTORS,
    _NVME_IOCTL_WRITE_SECTORS,
    _NVME_IOCTL_CONTROLLER_RESET,
    _NVME_IOCTL_SET_TIMOUT,
    _NVME_IOCTL_GET_TIMOUT
};

#define NVME_IOCTL_GET_CONTROLLER_VERSION   _IOR(MINIPCI_IOCTL_BASE, _NVME_IOCTL_GET_CONTROLLER_VERSION, nvme_controller_version_t)
#define NVME_IOCTL_GET_CONTROLLER_STATE     _IOR(MINIPCI_IOCTL_BASE, _NVME_IOCTL_GET_CONTROLLER_STATE, nvme_controller_state_t)
#define NVME_IOCTL_RUN_ADMIN_COMMAND        _IOWR(MINIPCI_IOCTL_BASE, _NVME_IOCTL_RUN_ADMIN_COMMAND, nvme_command_packet_t)
#define NVME_IOCTL_RUN_IO_COMMAND           _IOWR(MINIPCI_IOCTL_BASE, _NVME_IOCTL_RUN_IO_COMMAND, nvme_command_packet_t)
#define NVME_IOCTL_READ_SECTORS             _IOWR(MINIPCI_IOCTL_BASE, _NVME_IOCTL_READ_SECTORS, nvme_lba_packet_t)
#define NVME_IOCTL_WRITE_SECTORS            _IOWR(MINIPCI_IOCTL_BASE, _NVME_IOCTL_WRITE_SECTORS, nvme_lba_packet_t)
#define NVME_IOCTL_CONTROLLER_RESET         _IO(MINIPCI_IOCTL_BASE, _NVME_IOCTL_CONTROLLER_RESET)
#define NVME_IOCTL_SET_TIMOUT               _IOW(MINIPCI_IOCTL_BASE, _NVME_IOCTL_SET_TIMOUT, nvme_timeout_t)
#define NVME_IOCTL_GET_TIMOUT               _IOR(MINIPCI_IOCTL_BASE, _NVME_IOCTL_GET_TIMOUT, nvme_timeout_t)

#endif // IOCTL_H
