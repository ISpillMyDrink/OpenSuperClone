/****************************************************************************
**
** This file is part of the MiniNVMe project.
** Copyright (C) 2024 Alexander E. <aekhv@vk.com>
** License: GNU GPL v2, see file LICENSE.
**
****************************************************************************/

#include "driver.h"
#include <linux/delay.h>

int device_open(struct inode *pInode, struct file *pFile)
{
    nvme_driver_data_t *pDrvData = container_of(pInode->i_cdev, nvme_driver_data_t, charDevice);
    pFile->private_data = pDrvData;

    if ((pFile->f_flags & O_ACCMODE) != O_RDWR)
        return -EACCES;

    return 0;
}

int device_release(struct inode *pInode, struct file *pFile)
{
    (void)(pInode);
    (void)(pFile);
    return 0;
}

static int ioctl_get_driver_version(minipci_driver_version_t *pVersion)
{
    minipci_driver_version_t version;

    if (!pVersion)
        return -EINVAL;

    version.major = NVME_DRIVER_VERSION_MAJOR;
    version.minor = NVME_DRIVER_VERSION_MINOR;
    version.patch = NVME_DRIVER_VERSION_PATCH;

    if (copy_to_user(pVersion, &version, sizeof(version)))
        return -EFAULT;

    return 0;
}

static int ioctl_get_pci_device_info(nvme_driver_data_t *pDrvData, minipci_device_info_t *pDevInfo)
{
    struct pci_dev *pPciDev = pDrvData->pPciDev;
    struct pci_bus *pBus = pPciDev->bus;
    struct pci_host_bridge *host = pci_find_host_bridge(pBus);
    minipci_device_info_t info;
    uint16_t linkStatus = 0;

    if (!pDevInfo)
        return -EINVAL;

    info.location.domain = (host->domain_nr == -1) ? 0 : host->domain_nr; // -1 means domain is not defined
    info.location.bus = pPciDev->bus->number;
    info.location.slot = PCI_SLOT(pPciDev->devfn);
    info.location.func = PCI_FUNC(pPciDev->devfn);

    info.id.vendorId = pPciDev->vendor;
    info.id.deviceId = pPciDev->device;
    info.id.classId = pPciDev->class >> 8;
    info.id.revision = pPciDev->revision;

    pcie_capability_read_word(pPciDev, PCI_EXP_LNKSTA, &linkStatus);
    info.link.speed = linkStatus & PCI_EXP_LNKSTA_CLS;
    info.link.width = (linkStatus & PCI_EXP_LNKSTA_NLW) >> PCI_EXP_LNKSTA_NLW_SHIFT;

    if (copy_to_user(pDevInfo, &info, sizeof(info)))
        return -EFAULT;

    return 0;
}

static int ioctl_get_controller_version(nvme_driver_data_t *pDrvData, nvme_controller_version_t *pVersion)
{
    NVME_MEMORY *pNvmeMem = pDrvData->pNvmeMem;
    nvme_controller_version_t version;

    if (!pVersion)
        return -EINVAL;

    version.major = pNvmeMem->vs.mjr;
    version.minor = pNvmeMem->vs.mnr;
    version.tertiary = pNvmeMem->vs.ter;

    if (copy_to_user(pVersion, &version, sizeof(version)))
        return -EFAULT;

    return 0;
}

static int ioctl_get_controller_state(nvme_driver_data_t *pDrvData, nvme_controller_state_t *pState)
{
    NVME_MEMORY *pNvmeMem = pDrvData->pNvmeMem;
    nvme_controller_state_t state;

    if (!pState)
        return -EINVAL;

    state.enabled = pNvmeMem->cc.en;
    state.ready = pNvmeMem->csts.rdy;
    state.fatal = pNvmeMem->csts.cfs;

    if (copy_to_user(pState, &state, sizeof(state)))
        return -EFAULT;

    return 0;
}

static int ioctl_run_command(nvme_driver_data_t *pDrvData, nvme_command_packet_t *pCmdPacket, int cmdQueueId)
{
    nvme_command_packet_t cmdPacket;
    if (copy_from_user(&cmdPacket, pCmdPacket, sizeof (cmdPacket)))
        return -EFAULT;

    if (cmdPacket.buffer.length > NVME_BUFFER_SIZE_MAX)
        return -EINVAL;

    nvme_execute_command(pDrvData, &cmdPacket, cmdQueueId);

    if (copy_to_user(pCmdPacket, &cmdPacket, sizeof (cmdPacket)))
        return -EFAULT;

    return 0;
}

static int ioctl_read_sectors(nvme_driver_data_t *pDrvData, nvme_lba_packet_t *pLbaPacket)
{
    nvme_lba_packet_t lbaPacket;
    if (copy_from_user(&lbaPacket, pLbaPacket, sizeof (lbaPacket)))
        return -EFAULT;

    if (lbaPacket.buffer.length > NVME_BUFFER_SIZE_MAX)
        return -EINVAL;

    if (!pDrvData->ioCompQuRdy) {
        nvme_create_io_completion_queue(pDrvData, &lbaPacket.status);
        if (lbaPacket.status.timeout )
            goto FAIL;
        if (lbaPacket.status.sct || lbaPacket.status.sc)
            goto FAIL;
        pDrvData->ioCompQuRdy = true;
    }

    if (!pDrvData->ioSubmQuRdy) {
        nvme_create_io_submission_queue(pDrvData, &lbaPacket.status);
        if (lbaPacket.status.timeout )
            goto FAIL;
        if (lbaPacket.status.sct || lbaPacket.status.sc)
            goto FAIL;
        pDrvData->ioSubmQuRdy = true;
    }

    nvme_command_packet_t cmdPacket;
    memset(&cmdPacket, 0, sizeof (cmdPacket));

    cmdPacket.cmd.opc = NVME_READ_SECTORS;
    cmdPacket.cmd.nsid = lbaPacket.nsid;
    cmdPacket.cmd.cdw10 = lbaPacket.lba.offset >> 0;
    cmdPacket.cmd.cdw11 = lbaPacket.lba.offset >> 32;
    cmdPacket.cmd.cdw12 = lbaPacket.lba.count - 1;
    cmdPacket.buffer = lbaPacket.buffer;

    nvme_execute_command(pDrvData, &cmdPacket, 1);

FAIL:
    lbaPacket.status = cmdPacket.status;

    if (copy_to_user(pLbaPacket, &lbaPacket, sizeof (lbaPacket)))
        return -EFAULT;

    return 0;
}

static int ioctl_write_sectors(nvme_driver_data_t *pDrvData, nvme_lba_packet_t *pLbaPacket)
{
    nvme_lba_packet_t lbaPacket;
    if (copy_from_user(&lbaPacket, pLbaPacket, sizeof (lbaPacket)))
        return -EFAULT;

    if (lbaPacket.buffer.length > NVME_BUFFER_SIZE_MAX)
        return -EINVAL;

    if (!pDrvData->ioCompQuRdy) {
        nvme_create_io_completion_queue(pDrvData, &lbaPacket.status);
        if (lbaPacket.status.timeout )
            goto FAIL;
        if (lbaPacket.status.sct || lbaPacket.status.sc)
            goto FAIL;
        pDrvData->ioCompQuRdy = true;
    }

    if (!pDrvData->ioSubmQuRdy) {
        nvme_create_io_submission_queue(pDrvData, &lbaPacket.status);
        if (lbaPacket.status.timeout )
            goto FAIL;
        if (lbaPacket.status.sct || lbaPacket.status.sc)
            goto FAIL;
        pDrvData->ioSubmQuRdy = true;
    }

    nvme_command_packet_t cmdPacket;
    memset(&cmdPacket, 0, sizeof (cmdPacket));

    cmdPacket.cmd.opc = NVME_WRITE_SECTORS;
    cmdPacket.cmd.nsid = lbaPacket.nsid;
    cmdPacket.cmd.cdw10 = lbaPacket.lba.offset >> 0;
    cmdPacket.cmd.cdw11 = lbaPacket.lba.offset >> 32;
    cmdPacket.cmd.cdw12 = lbaPacket.lba.count - 1;
    cmdPacket.buffer = lbaPacket.buffer;

    nvme_execute_command(pDrvData, &cmdPacket, 1);

FAIL:
    lbaPacket.status = cmdPacket.status;

    if (copy_to_user(pLbaPacket, &lbaPacket, sizeof (lbaPacket)))
        return -EFAULT;

    return 0;
}

static int nvme_set_timeout(nvme_driver_data_t *pDrvData, nvme_timeout_t *pTimeout)
{
    nvme_timeout_t timeout;

    if (copy_from_user(&timeout, pTimeout, sizeof (timeout)))
        return -EFAULT;

    pDrvData->timeout = timeout.value;

    return 0;
}

static int nvme_get_timeout(nvme_driver_data_t *pDrvData, nvme_timeout_t *pTimeout)
{
    nvme_timeout_t timeout;
    timeout.value = pDrvData->timeout;

    if (copy_to_user(pTimeout, &timeout, sizeof (timeout)))
        return -EFAULT;

    return 0;
}

long device_ioctl(struct file *pFile, unsigned int cmd, unsigned long arg)
{
    nvme_driver_data_t *pDrvData = pFile->private_data;

    if (_IOC_TYPE(cmd) != MINIPCI_IOCTL_BASE)
        return -EINVAL;

    switch (cmd) {
    case MINIPCI_IOCTL_GET_DRIVER_VERSION:
        return ioctl_get_driver_version((minipci_driver_version_t *)arg);

    case MINIPCI_IOCTL_GET_DEVICE_INFO:
        return ioctl_get_pci_device_info(pDrvData, (minipci_device_info_t *)arg);

    case NVME_IOCTL_GET_CONTROLLER_VERSION:
        return ioctl_get_controller_version(pDrvData, (nvme_controller_version_t *)arg);

    case NVME_IOCTL_GET_CONTROLLER_STATE:
        return ioctl_get_controller_state(pDrvData, (nvme_controller_state_t *)arg);

    case NVME_IOCTL_RUN_ADMIN_COMMAND:
        return ioctl_run_command(pDrvData, (nvme_command_packet_t *)arg, 0);

    case NVME_IOCTL_RUN_IO_COMMAND:
        return ioctl_run_command(pDrvData, (nvme_command_packet_t *)arg, 1);

    case NVME_IOCTL_READ_SECTORS:
        return ioctl_read_sectors(pDrvData, (nvme_lba_packet_t *)arg);

    case NVME_IOCTL_WRITE_SECTORS:
        return ioctl_write_sectors(pDrvData, (nvme_lba_packet_t *)arg);

    case NVME_IOCTL_CONTROLLER_RESET:
        nvme_controller_disable(pDrvData);
        mdelay(500);
        nvme_controller_enable(pDrvData);
        break;

    case NVME_IOCTL_SET_TIMOUT:
        return nvme_set_timeout(pDrvData, (nvme_timeout_t *)arg);

    case NVME_IOCTL_GET_TIMOUT:
        return nvme_get_timeout(pDrvData, (nvme_timeout_t *)arg);

    default:
        return -EINVAL;
    }

    return 0;
}
