/****************************************************************************
**
** This file is part of the MiniNVMe project.
** Copyright (C) 2024 Alexander E. <aekhv@vk.com>
** License: GNU GPL v2, see file LICENSE.
**
****************************************************************************/

#ifndef DRIVER_H
#define DRIVER_H

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/cdev.h>
#include "nvme.h"
#include "ioctl.h"

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Alexander E. <aekhv@vk.com>");
MODULE_DESCRIPTION("MiniNVMe kernel module");
MODULE_VERSION("1.0");

// Use "insmod mininvme.ko debug=1" to turn debug on
static bool debug = 0;
module_param(debug, bool, 0);

// PCI base address register
#define NVME_PCI_BAR                0

// Driver version
#define NVME_DRIVER_VERSION_MAJOR   1
#define NVME_DRIVER_VERSION_MINOR   0
#define NVME_DRIVER_VERSION_PATCH   0

// Driver limits
#define NVME_QUEUE_ENTRY_COUNT      2
#define NVME_BUFFER_SIZE_MAX        1048576

// Default timeout in milliseconds
#define NVME_DEFAULT_TIMEOUT        5000

typedef struct {
    NVME_COMPLETION_QUEUE_ENTRY *pCompEntries; // Virtual addresses
    NVME_SUBMISSION_QUEUE_ENTRY *pSubmEntries;
    dma_addr_t pCompEntriesDma; // Physical addresses
    dma_addr_t pSubmEntriesDma;
    uint16_t cid;       // Command ID
    uint32_t *psqtd;    // Points to Subm. Queue Tail Doorbell, depends from pNvmeMem->cap.dstrd
    uint32_t *pcqhd;    // Points to Comp. Queue Head Doorbell, depends from pNvmeMem->cap.dstrd
    uint32_t sqtidx;    // Current Submissin Queue Tail index
    uint32_t cqhidx;    // Current Completion Queue Head index
} nvme_command_queue_t;

typedef struct {
    struct pci_dev *pPciDev;
    struct cdev charDevice;
    struct device *pDevice;
    NVME_MEMORY __iomem *pNvmeMem;
    nvme_command_queue_t cmdQueue[2];   // 0 - Admin Queue, 1 - Regular I/O Queue

    volatile uint32_t intCounter;       // Interrupt counter

    uint32_t userPagesCount;
    struct page *pUserPages[NVME_BUFFER_SIZE_MAX / PAGE_SIZE + 1]; // User buffer mapped pages

    dma_addr_t *pSgList; // Scatter Gather List (list of physical memory pages) - virtual address
    dma_addr_t pSgListDma; // Physical address

    bool ioCompQuRdy; // IO completion queue ready flag
    bool ioSubmQuRdy; // IO submission queue ready flag

    uint32_t timeout;

    bool debug;
} nvme_driver_data_t;

// Base part
int nvme_controller_enable(nvme_driver_data_t *pDrvData);
void nvme_controller_disable(nvme_driver_data_t *pDrvData);
void nvme_execute_command(nvme_driver_data_t *pDrvData, nvme_command_packet_t *pCmdPacket, int cmdQueueId);
void nvme_create_io_completion_queue(nvme_driver_data_t *pDrvData, nvme_status_t *status);
void nvme_create_io_submission_queue(nvme_driver_data_t *pDrvData, nvme_status_t *status);

// IOCTL part
int device_open(struct inode *pInode, struct file *pFile);
int device_release(struct inode *pInode, struct file *pFile);
long device_ioctl(struct file *pFile, unsigned int cmd, unsigned long arg);

#endif // DRIVER_H
