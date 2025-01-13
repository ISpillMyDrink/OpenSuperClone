/****************************************************************************
**
** This file is part of the MiniNVMe project.
** Copyright (C) 2024 Alexander E. <aekhv@vk.com>
** License: GNU GPL v2, see file LICENSE.
**
****************************************************************************/

#ifndef NVME_H
#define NVME_H

#include <linux/kernel.h>

#pragma once

// Some useful NVME admin opcodes...
#define NVME_CREATE_IO_SUBMISSION_QUEUE 0x01
#define NVME_CREATE_IO_COMPLETION_QUEUE 0x05

// Some useful NVME I/O opcodes...
#define NVME_WRITE_SECTORS  0x01
#define NVME_READ_SECTORS   0x02

// Structures...
typedef struct _NVME_REG_CAP
{
    uint64_t mqes : 16;     // Maximum Queue Entries Supported
    uint64_t cqr : 1;       // Contiguous Queues Required
    uint64_t ams : 2;       // Arbitration Mechanism Supported
    uint64_t rsvd0 : 5;     // Reserved
    uint64_t to : 8;        // Timeout
    uint64_t dstrd : 4;     // Doorbell Stride
    uint64_t nssrs : 1;     // NVM Subsystem Reset Supported
    uint64_t css : 8;       // Command Sets Supported
    uint64_t bps : 1;       // Boot Partition Support
    uint64_t rsvd1 : 2;     // Reserved
    uint64_t mpsmin : 4;    // Memory Page Size Minimum (2^(12+n))
    uint64_t mpsmax : 4;    // Memory Page Size Maximum  (2^(12+n))
    uint64_t pmrs : 1;      // Persistent Memory Region Supported
    uint64_t cmbs : 1;      // Controller Memory Buffer Supported
    uint64_t rsvd2 : 6;     // Reserved
} NVME_REG_CAP;

typedef struct _NVME_REG_VS
{
    uint32_t ter : 8;       // Tertiary Version Number
    uint32_t mnr : 8;       // Minor Version Number
    uint32_t mjr : 16;      // Major Version Number
} NVME_REG_VS;

typedef struct _NVME_REG_CC
{
    uint32_t en : 1;        // Enable
    uint32_t rsvd0 : 3;     // Reserved
    uint32_t css : 3;       // I/O Command Set Selected
    uint32_t mps : 4;       // Memory Page Size (2^(12+n))
    uint32_t ams : 3;       // Arbitration Mechanism Selected
    uint32_t shn : 2;       // Shutdown Notification
    uint32_t iosqes : 4;    // I/O Submission Queue Entry Size (2^n)
    uint32_t iocqes : 4;    // I/O Completion Queue Entry Size (2^n)
    uint32_t rsvd1 : 8;     // Reserved
} NVME_REG_CC;

typedef struct _NVME_REG_CSTS
{
    uint32_t rdy : 1;       // Ready
    uint32_t cfs : 1;       // Controller Fatal Status
    uint32_t shst : 2;      // Shutdown Status
    uint32_t nssro : 1;     // NVM Subsystem Reset Occurred
    uint32_t pp : 1;        // Processing Paused
    uint32_t rsvd : 26;     // Reserved
} NVME_REG_CSTS;

typedef struct _NVME_REG_AQA
{
    uint32_t asqs : 12;     // Admin Submission Queue Size (2...4096) (0 based!!!)
    uint32_t rsvd0 : 4;     // Reserved
    uint32_t acqs : 12;     // Admin Completion Queue Size (2...4096) (0 based!!!)
    uint32_t rsvd1 : 4;     // Reserved
} NVME_REG_AQA;

typedef volatile struct _NVME_MEMORY
{
    NVME_REG_CAP cap;       // Controller Capabilities
    NVME_REG_VS vs;         // Version
    uint32_t intms;         // Interrupt Mask Set
    uint32_t intmc;         // Interrupt Mask Clear
    NVME_REG_CC cc;         // Controller Configuration
    uint32_t rsvd0;         // Reserved;
    NVME_REG_CSTS csts;     // Controller Status
    uint32_t nssr;          // NVM Subsystem Reset
    NVME_REG_AQA aqa;       // Admin Queue Attributes
    uint64_t asq;           // Admin Submission Queue Base Address
    uint64_t acq;           // Admin Completion Queue Base Address
} NVME_MEMORY;

typedef struct _NVME_CMD_DWORD_0
{
    uint32_t opc : 8;       // Opcode
    uint32_t fuse : 2;      // Fused Operation
    uint32_t rsvd : 4;      // Reserved
    uint32_t psdt : 2;      // PRP or SGL for Data Transfer
    uint32_t cid : 16;      // Command Identifier
} NVME_CMD_DWORD_0;

typedef struct _NVME_SUBMISSION_QUEUE_ENTRY
{
    NVME_CMD_DWORD_0 cdw0;
    uint32_t nsid;          // Namespace Identifier
    uint32_t rsvd[2];       // Reserved
    uint64_t mptr;          // Metadata Pointer
    uint64_t prp[2];        // Physical Region Pages
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} NVME_SUBMISSION_QUEUE_ENTRY;

typedef struct _NVME_COMPLETION_QUEUE_ENTRY
{
    uint32_t cs;            // Command Specific
    uint32_t rsvd0;         // Reserved
    uint32_t sqhd : 16;     // SQ Head Pointer
    uint32_t sqid : 16;     // SQ Identifier
    uint32_t cid : 16;      // Command Identifier
    uint32_t p : 1;         // Phase Tag
    uint32_t sc : 8;        // Status Code
    uint32_t sct : 3;       // Status Code Type
    uint32_t rsvd1 : 2;     // Reserved
    uint32_t m : 1;         // More
    uint32_t dnr : 1;       // Do Not Retry
} NVME_COMPLETION_QUEUE_ENTRY;

// Create Subm/Compl queue
typedef struct _NVME_CMD_DWORD_10_CREATE_QUEUE
{
    uint32_t qid : 16;      // Queue Identifier
    uint32_t qsize : 16;    // Queue Size (0 based!!!)
} NVME_CMD_DWORD_10_CREATE_QUEUE;

typedef struct _NVME_CMD_DWORD_11_CREATE_COMP_QUEUE
{
    uint32_t pc : 1;        // Physically Contiguous
    uint32_t ien : 1;       // Interrupts Enabled
    uint32_t rsvd : 14;     // Reserved
    uint32_t iv : 16;       // Interrupt Vector
} NVME_CMD_DWORD_11_CREATE_COMP_QUEUE;

typedef struct _NVME_CMD_DWORD_11_CREATE_SUBM_QUEUE
{
    uint32_t pc : 1;        // Physically Contiguous
    uint32_t qprio : 2;     // Queue Priority
    uint32_t rsvd : 13;     // Reserved
    uint32_t cqid : 16;     // Completion Queue Identifier
} NVME_CMD_DWORD_11_CREATE_SUBM_QUEUE;

#endif // NVME_H
