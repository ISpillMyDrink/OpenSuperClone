/****************************************************************************
**
** This file is part of the MiniNVMe project.
** Copyright (C) 2024 Alexander E. <aekhv@vk.com>
** License: GNU GPL v2, see file LICENSE.
**
****************************************************************************/

#include "driver.h"

static void nvme_allocate_buffers(nvme_driver_data_t *pDrvData)
{
    int i;

    // Memory allocation and page memory mapping mode - full 64-bit
    dma_set_mask(&(pDrvData->pPciDev->dev), ~0ULL); // ~0ULL = DMA_BIT_MASK(64)
    dma_set_coherent_mask(&(pDrvData->pPciDev->dev), ~0ULL); // ~0ULL = DMA_BIT_MASK(64)

    for (i = 0; i < 2; i++) {
        pDrvData->cmdQueue[i].pCompEntries = dma_alloc_coherent(&(pDrvData->pPciDev->dev),
                                                                NVME_QUEUE_ENTRY_COUNT * sizeof(NVME_COMPLETION_QUEUE_ENTRY),
                                                                &(pDrvData->cmdQueue[i].pCompEntriesDma), GFP_KERNEL);
        memset(pDrvData->cmdQueue[i].pCompEntries, 0, NVME_QUEUE_ENTRY_COUNT * sizeof(NVME_COMPLETION_QUEUE_ENTRY));
        if (pDrvData->debug)
            printk(KERN_INFO "%s: cmdQueue[%d].pCompEntries: allocated at 0x%016llx\n",
                   KBUILD_MODNAME, i, (uint64_t)pDrvData->cmdQueue[i].pCompEntriesDma);

        pDrvData->cmdQueue[i].pSubmEntries = dma_alloc_coherent(&(pDrvData->pPciDev->dev),
                                                                NVME_QUEUE_ENTRY_COUNT * sizeof(NVME_SUBMISSION_QUEUE_ENTRY),
                                                                &(pDrvData->cmdQueue[i].pSubmEntriesDma), GFP_KERNEL);
        memset(pDrvData->cmdQueue[i].pSubmEntries, 0, NVME_QUEUE_ENTRY_COUNT * sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
        if (pDrvData->debug)
            printk(KERN_INFO "%s: cmdQueue[%d].pSubmEntries: allocated at 0x%016llx\n",
                   KBUILD_MODNAME, i, (uint64_t)pDrvData->cmdQueue[i].pSubmEntriesDma);
    }

    pDrvData->pSgList = dma_alloc_coherent(&(pDrvData->pPciDev->dev),
                                           (NVME_BUFFER_SIZE_MAX / PAGE_SIZE + 1) * sizeof(dma_addr_t),
                                           &(pDrvData->pSgListDma), GFP_KERNEL);
    if (pDrvData->debug)
        printk(KERN_INFO "%s: pSgList: allocated at 0x%016llx\n",
               KBUILD_MODNAME, (uint64_t)pDrvData->pSgListDma);
}

static void nvme_free_buffers(nvme_driver_data_t *pDrvData)
{
    int i;

    for (i = 0; i < 2; i++) {
        if (pDrvData->cmdQueue[i].pCompEntries) {
            dma_free_coherent(&(pDrvData->pPciDev->dev),
                              NVME_QUEUE_ENTRY_COUNT * sizeof(NVME_COMPLETION_QUEUE_ENTRY),
                              pDrvData->cmdQueue[i].pCompEntries, pDrvData->cmdQueue[i].pCompEntriesDma);
            if (pDrvData->debug)
                printk(KERN_INFO "%s: cmdQueue[%d].pCompEntries: memory deallocated\n", KBUILD_MODNAME, i);
        }

        if (pDrvData->cmdQueue[i].pSubmEntries) {
            dma_free_coherent(&(pDrvData->pPciDev->dev),
                              NVME_QUEUE_ENTRY_COUNT * sizeof(NVME_COMPLETION_QUEUE_ENTRY),
                              pDrvData->cmdQueue[i].pSubmEntries, pDrvData->cmdQueue[i].pSubmEntriesDma);
            if (pDrvData->debug)
                printk(KERN_INFO "%s: cmdQueue[%d].pSubmEntries: memory deallocated\n", KBUILD_MODNAME, i);
        }
    }

    if (pDrvData->pSgList) {
        dma_free_coherent(&(pDrvData->pPciDev->dev),
                          (NVME_BUFFER_SIZE_MAX / PAGE_SIZE + 1) * sizeof(dma_addr_t),
                          pDrvData->pSgList, pDrvData->pSgListDma);
        if (pDrvData->debug)
            printk(KERN_INFO "%s: pSgList: memory deallocated\n", KBUILD_MODNAME);
    }
}

static irqreturn_t irq_handler(int irq, void *data)
{
    nvme_driver_data_t *pDrvData = data;
    (void)irq;

    pDrvData->intCounter++;

//    if (pDrvData->debug)
//        printk(KERN_INFO "%s: INTERRUPT! (irq=%d, counter=%d)\n", KBUILD_MODNAME, irq, pDrvData->intCounter);

    return IRQ_HANDLED;
}

static int nvme_register_interrupt(nvme_driver_data_t *pDrvData)
{
    int err;

    err = pci_alloc_irq_vectors(pDrvData->pPciDev, 1, 1, PCI_IRQ_MSI | PCI_IRQ_MSIX);
    if (err < 0) {
        printk(KERN_ERR "%s: Error at pci_alloc_irq_vectors()! (%d)\n", KBUILD_MODNAME, err);
        return -EIO;
    }

    err = request_irq(pci_irq_vector(pDrvData->pPciDev, 0), irq_handler, IRQF_TRIGGER_NONE, KBUILD_MODNAME, pDrvData);
    if (err < 0) {
        pci_free_irq_vectors(pDrvData->pPciDev);
        printk(KERN_ERR "%s: Error at pci_request_irq()! (%d)\n", KBUILD_MODNAME, err);
        return -EIO;
    }

    return 0;
}

static void nvme_unregister_interrupt(nvme_driver_data_t *pDrvData)
{
    free_irq(pci_irq_vector(pDrvData->pPciDev, 0), pDrvData);
    pci_free_irq_vectors(pDrvData->pPciDev);
}

static void nvme_map_user_pages(nvme_driver_data_t *pDrvData, nvme_buffer_t *pBuffer)
{
    uint32_t i, n;
    uint32_t offs, len;

    const uint64_t first_page = (uint64_t)pBuffer->pointer >> PAGE_SHIFT;
    const uint64_t last_page = ((uint64_t)pBuffer->pointer + pBuffer->length - 1) >> PAGE_SHIFT;

    pDrvData->userPagesCount = get_user_pages((uint64_t)pBuffer->pointer & PAGE_MASK,
                                            last_page - first_page + 1,
                                            FOLL_FORCE,
                                            pDrvData->pUserPages);

    n = 0;
    for (i = 0; i < pDrvData->userPagesCount; i++) {

        offs = 0;
        len = PAGE_SIZE;

        if (i == 0) {
            offs = (uint64_t)pBuffer->pointer & (PAGE_SIZE - 1);
            len -= offs;
            if (len > pBuffer->length)
                len = pBuffer->length;
        }
        else
            if (i == pDrvData->userPagesCount - 1)
                len = pBuffer->length - n;

        pDrvData->pSgList[i] = dma_map_page(&(pDrvData->pPciDev->dev),
                                                  pDrvData->pUserPages[i],
                                                  offs,
                                                  len,
                                                  DMA_BIDIRECTIONAL);
        n += len;

        if (pDrvData->debug)
            printk(KERN_INFO "%s: [+] page %d mapped (0x%016llx, %d)\n", KBUILD_MODNAME, i,
                   (uint64_t)pDrvData->pSgList[i], len);
    }
}

static void nvme_unmap_user_pages(nvme_driver_data_t *pDrvData, nvme_buffer_t *pBuffer)
{
    uint32_t i, n, len;

    n = 0;
    for (i = 0; i < pDrvData->userPagesCount; i++) {

        len = PAGE_SIZE;

        if (i == 0) {
            len -= pDrvData->pSgList[i] & (PAGE_SIZE - 1);
            if (len > pBuffer->length)
                len = pBuffer->length;
        }
        else
            if (i == pDrvData->userPagesCount - 1)
                len = pBuffer->length - n;

        dma_unmap_page(&(pDrvData->pPciDev->dev),
                       pDrvData->pSgList[i],
                       len,
                       DMA_BIDIRECTIONAL);
        n += len;

        if (pDrvData->debug)
            printk(KERN_INFO "%s: [-] page %d unmapped (0x%016llx, %d)\n", KBUILD_MODNAME, i,
                   (uint64_t)pDrvData->pSgList[i], len);

    }

    pDrvData->userPagesCount = 0;
}

int nvme_controller_enable(nvme_driver_data_t *pDrvData)
{
    int err, mpsmax, mpsmin;
    unsigned long future;
    NVME_MEMORY *pNvmeMem = pDrvData->pNvmeMem;

    // Disable NVMe controller if it's already enabled by BIOS/OS
    future = jiffies + msecs_to_jiffies(NVME_DEFAULT_TIMEOUT);
    pNvmeMem->cc.en = 0;
    while (true) {
        if (!pNvmeMem->csts.rdy)
            break;
        if (time_after(jiffies, future)) {
            printk(KERN_ERR "%s: Controller disable error!\n", KBUILD_MODNAME);
            break;
        }
        cpu_relax();
    }

    // Supported memory page size (min & max)
    mpsmin = 1 << (12 + pNvmeMem->cap.mpsmin);
    mpsmax = 1 << (12 + pNvmeMem->cap.mpsmax);
    if (pDrvData->debug)
        printk(KERN_INFO "%s: Memory page size (min/max): %d/%d bytes\n", KBUILD_MODNAME, mpsmin, mpsmax);

    if (((int)PAGE_SIZE < mpsmin) || ((int)PAGE_SIZE > mpsmax)) {
        printk(KERN_ERR "%s: Unsupported memory page size!\n", KBUILD_MODNAME);
        return -EIO;
    }

    // Setting actual memory page size
    pNvmeMem->cc.mps = 0; // (1 << (12 + 0)) = 4096 bytes

    // Register interrupt
    err = nvme_register_interrupt(pDrvData);
    if (err < 0)
        return err;

    // Allocate memory buffers
    nvme_allocate_buffers(pDrvData);

    if (pDrvData->debug)
        printk(KERN_INFO "%s: Doorbell stride: %d bytes\n", KBUILD_MODNAME, 4 * (1 << pNvmeMem->cap.dstrd));

    pDrvData->cmdQueue[0].psqtd = (uint32_t *)pNvmeMem + 0x1000 / sizeof(uint32_t); // All offsets in uint32_t units
    pDrvData->cmdQueue[0].pcqhd = (uint32_t *)pNvmeMem + 0x1000 / sizeof(uint32_t) + 1 * (1 << pNvmeMem->cap.dstrd);
    pDrvData->cmdQueue[1].psqtd = (uint32_t *)pNvmeMem + 0x1000 / sizeof(uint32_t) + 2 * (1 << pNvmeMem->cap.dstrd);
    pDrvData->cmdQueue[1].pcqhd = (uint32_t *)pNvmeMem + 0x1000 / sizeof(uint32_t) + 3 * (1 << pNvmeMem->cap.dstrd);

    pNvmeMem->aqa.asqs = NVME_QUEUE_ENTRY_COUNT - 1;
    pNvmeMem->aqa.acqs = NVME_QUEUE_ENTRY_COUNT - 1;
    pNvmeMem->acq = pDrvData->cmdQueue[0].pCompEntriesDma;
    pNvmeMem->asq = pDrvData->cmdQueue[0].pSubmEntriesDma;

    pDrvData->cmdQueue[0].cqhidx = 0;
    pDrvData->cmdQueue[0].sqtidx = 0;
    pDrvData->cmdQueue[1].cqhidx = 0;
    pDrvData->cmdQueue[1].sqtidx = 0;
    pDrvData->ioCompQuRdy = false;
    pDrvData->ioSubmQuRdy = false;

    pDrvData->timeout = NVME_DEFAULT_TIMEOUT;

    // Enable NVMe controller
    future = jiffies + msecs_to_jiffies(NVME_DEFAULT_TIMEOUT);
    pNvmeMem->cc.en = 1;
    while (true) {
        if (pNvmeMem->csts.rdy)
            break;
        if (time_after(jiffies, future)) {
            printk(KERN_ERR "%s: Controller enable error!\n", KBUILD_MODNAME);
            break;
        }
        cpu_relax();
    }

    return 0;
}

void nvme_controller_disable(nvme_driver_data_t *pDrvData)
{
    unsigned long future;
    NVME_MEMORY *pNvmeMem = pDrvData->pNvmeMem;

    // Disable NVMe controller
    future = jiffies + msecs_to_jiffies(NVME_DEFAULT_TIMEOUT);
    pNvmeMem->cc.en = 0;
    while (true) {
        if (!pNvmeMem->csts.rdy)
            break;
        if (time_after(jiffies, future)) {
            printk(KERN_ERR "%s: Controller disable error!\n", KBUILD_MODNAME);
            break;
        }
        cpu_relax();
    }

    // Free memory buffers
    nvme_free_buffers(pDrvData);

    // Unregister interrupt
    nvme_unregister_interrupt(pDrvData);
}

void nvme_execute_command(nvme_driver_data_t *pDrvData, nvme_command_packet_t *pCmdPacket, int cmdQueueId)
{
    unsigned long future;
    nvme_command_queue_t *pCmdQueue = &(pDrvData->cmdQueue[cmdQueueId]);
    NVME_SUBMISSION_QUEUE_ENTRY *pEntry = &(pCmdQueue->pSubmEntries[pCmdQueue->sqtidx]);

    memset(pCmdQueue->pSubmEntries, 0, NVME_QUEUE_ENTRY_COUNT * sizeof(NVME_SUBMISSION_QUEUE_ENTRY));
    memset(pCmdQueue->pCompEntries, 0, NVME_QUEUE_ENTRY_COUNT * sizeof(NVME_COMPLETION_QUEUE_ENTRY));

    pCmdQueue->cid++;
    if (pCmdQueue->cid == 0)
        pCmdQueue->cid++;

    pEntry->cdw0.cid = pCmdQueue->cid;
    pEntry->cdw0.opc = pCmdPacket->cmd.opc;
    pEntry->nsid = pCmdPacket->cmd.nsid;
    pEntry->cdw10 = pCmdPacket->cmd.cdw10;
    pEntry->cdw11 = pCmdPacket->cmd.cdw11;
    pEntry->cdw12 = pCmdPacket->cmd.cdw12;
    pEntry->cdw13 = pCmdPacket->cmd.cdw13;
    pEntry->cdw14 = pCmdPacket->cmd.cdw14;
    pEntry->cdw15 = pCmdPacket->cmd.cdw15;

    if (pCmdPacket->buffer.length != 0) {
        nvme_map_user_pages(pDrvData, &(pCmdPacket->buffer));
        switch (pDrvData->userPagesCount) {
        case 1:
            pEntry->prp[0] = pDrvData->pSgList[0];
            break;
        case 2:
            pEntry->prp[0] = pDrvData->pSgList[0];
            pEntry->prp[1] = pDrvData->pSgList[1];
            break;
        default:
            pEntry->prp[0] = pDrvData->pSgList[0];
            pEntry->prp[1] = pDrvData->pSgListDma + sizeof(dma_addr_t);
        }
    } else
        pEntry->prp[0] = (uint64_t)pCmdPacket->buffer.pointer;

    pDrvData->intCounter = 0;

    pCmdQueue->sqtidx++;
    if (pCmdQueue->sqtidx == NVME_QUEUE_ENTRY_COUNT)
        pCmdQueue->sqtidx = 0;

    *pCmdQueue->psqtd = pCmdQueue->sqtidx;

    future = jiffies + msecs_to_jiffies(3000);

    while (true) {
        // Interrupt detected
        if (pDrvData->intCounter)
            break;
        // Timeout
        if (time_after(jiffies, future)) {
            pCmdPacket->status.timeout = true;
            break;
        }
        cpu_relax();
    }

    // What will we do in this case?...
    if (pCmdQueue->cid != pCmdQueue->pCompEntries[pCmdQueue->cqhidx].cid)
        printk(KERN_ERR "%s: Critical error! Wrong CID received!\n", KBUILD_MODNAME);

    pCmdPacket->status.sct = pCmdQueue->pCompEntries[pCmdQueue->cqhidx].sct;
    pCmdPacket->status.sc = pCmdQueue->pCompEntries[pCmdQueue->cqhidx].sc;
    pCmdPacket->status.more = pCmdQueue->pCompEntries[pCmdQueue->cqhidx].m;
    pCmdPacket->status.dnr = pCmdQueue->pCompEntries[pCmdQueue->cqhidx].dnr;

    pCmdQueue->cqhidx++;
    if (pCmdQueue->cqhidx == NVME_QUEUE_ENTRY_COUNT)
        pCmdQueue->cqhidx = 0;

    *pCmdQueue->pcqhd = pCmdQueue->cqhidx;

    if (pCmdPacket->buffer.length != 0)
        nvme_unmap_user_pages(pDrvData, &(pCmdPacket->buffer));
}

void nvme_create_io_completion_queue(nvme_driver_data_t *pDrvData, nvme_status_t *status)
{
    nvme_command_packet_t cmdPacket;
    NVME_CMD_DWORD_10_CREATE_QUEUE *pcdw10 = (NVME_CMD_DWORD_10_CREATE_QUEUE *)&(cmdPacket.cmd.cdw10);
    NVME_CMD_DWORD_11_CREATE_COMP_QUEUE *pcdw11 = (NVME_CMD_DWORD_11_CREATE_COMP_QUEUE *)&(cmdPacket.cmd.cdw11);
    memset(&cmdPacket, 0, sizeof (cmdPacket));

    if (pDrvData->debug)
        printk(KERN_INFO "%s: Create IO completion queue requested...\n", KBUILD_MODNAME);

    cmdPacket.cmd.opc = NVME_CREATE_IO_COMPLETION_QUEUE;
    cmdPacket.buffer.pointer = (uint8_t *)pDrvData->cmdQueue[1].pCompEntriesDma;
    pcdw10->qid = 1;
    pcdw10->qsize = NVME_QUEUE_ENTRY_COUNT - 1;
    pcdw11->pc = 1;
    pcdw11->ien = 1;

    nvme_execute_command(pDrvData, &cmdPacket, 0);

    *status = cmdPacket.status;
}

void nvme_create_io_submission_queue(nvme_driver_data_t *pDrvData, nvme_status_t *status)
{
    nvme_command_packet_t cmdPacket;
    NVME_CMD_DWORD_10_CREATE_QUEUE *pcdw10 = (NVME_CMD_DWORD_10_CREATE_QUEUE *)&(cmdPacket.cmd.cdw10);
    NVME_CMD_DWORD_11_CREATE_SUBM_QUEUE *pcdw11 = (NVME_CMD_DWORD_11_CREATE_SUBM_QUEUE *)&(cmdPacket.cmd.cdw11);
    memset(&cmdPacket, 0, sizeof (cmdPacket));

    if (pDrvData->debug)
        printk(KERN_INFO "%s: Create IO submission queue requested...\n", KBUILD_MODNAME);

    cmdPacket.cmd.opc = NVME_CREATE_IO_SUBMISSION_QUEUE;
    cmdPacket.buffer.pointer = (uint8_t *)pDrvData->cmdQueue[1].pSubmEntriesDma;
    pcdw10->qid = 1;
    pcdw10->qsize = NVME_QUEUE_ENTRY_COUNT - 1;
    pcdw11->pc = 1;
    pcdw11->cqid = 1;

    nvme_execute_command(pDrvData, &cmdPacket, 0);

    *status = cmdPacket.status;
}
