/****************************************************************************
**
** This file is part of the MiniNVMe project.
** Copyright (C) 2024 Alexander E. <aekhv@vk.com>
** License: GNU GPL v2, see file LICENSE.
**
****************************************************************************/

#include "driver.h"

const struct pci_device_id id_table[] = {
    { PCI_DEVICE_CLASS(PCI_CLASS_STORAGE_EXPRESS, PCI_ANY_ID) },
    {0,}
};

MODULE_DEVICE_TABLE(pci, id_table);

static uint32_t _imajor = 0;
static struct class *_device_class = NULL;

const struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = device_open,
    .release        = device_release,
    .unlocked_ioctl = device_ioctl,
};

static int device_probe(struct pci_dev *pPciDev, const struct pci_device_id *pId)
{
    nvme_driver_data_t *pDrvData = NULL;
    uint32_t start, len;
    (void)(pId);

    printk(KERN_INFO "%s: PCI device attached: vendor 0x%04x, device 0x%04x, class 0x%04x, revision 0x%02x\n",
           KBUILD_MODNAME, pPciDev->vendor, pPciDev->device, pPciDev->class >> 8, pPciDev->revision);

    pDrvData = kzalloc(sizeof(nvme_driver_data_t), GFP_KERNEL);

    if (!pDrvData) {
        printk(KERN_ERR "%s: Error at kzalloc()!\n", KBUILD_MODNAME);
        return -ENOMEM;
    }

    pci_set_drvdata(pPciDev, pDrvData);
    pDrvData->pPciDev = pPciDev;
    pDrvData->debug = debug;

    if (pci_enable_device(pPciDev) != 0) {
        printk(KERN_ERR "%s: Error at pci_enable_device()!\n", KBUILD_MODNAME);
        return -EIO;
    }

    pci_set_master(pPciDev);
    pci_set_power_state(pPciDev, PCI_D0);

    if (pci_request_region(pPciDev, NVME_PCI_BAR, KBUILD_MODNAME) != 0) {
        printk(KERN_ERR "%s: Error at pci_request_region()!\n", KBUILD_MODNAME);
        goto ERR1;
    }

    start = pci_resource_start(pPciDev, NVME_PCI_BAR);
    len = pci_resource_len(pPciDev, NVME_PCI_BAR);

    //if ((len == 0) || (((pci_resource_flags(pPciDev, NVME_PCI_BAR) & IORESOURCE_MEM) != IORESOURCE_MEM))) {
    //    printk(KERN_ERR "%s: Unsupported hardware!\n", KBUILD_MODNAME);
    //    return -EIO;
    //}

    if (pDrvData->debug)
        printk(KERN_INFO "%s: PCI BAR[%d]: offset 0x%08x, length 0x%08x\n", KBUILD_MODNAME, NVME_PCI_BAR, start, len);

    pDrvData->pNvmeMem = pci_iomap(pPciDev, NVME_PCI_BAR, len);

    if (nvme_controller_enable(pDrvData) != 0) {
        printk(KERN_ERR "%s: Error at nvme_controller_enable()!\n", KBUILD_MODNAME);
        goto ERR2;
    }

    uint32_t _iminor = pPciDev->bus->number;

    cdev_init(&pDrvData->charDevice, &fops);
    pDrvData->charDevice.owner = THIS_MODULE;
    cdev_add(&pDrvData->charDevice, MKDEV(_imajor, _iminor), 1);

    pDrvData->pDevice = device_create(_device_class, NULL, MKDEV(_imajor, _iminor), NULL, "%s%d", KBUILD_MODNAME, _iminor);
    printk(KERN_INFO "%s: Character device created: /dev/%s%d\n", KBUILD_MODNAME, KBUILD_MODNAME, _iminor);

    // SUCCESS!!!
    return 0;

    // ERROR!!!
ERR2:
    if (pDrvData->pNvmeMem)
        pci_iounmap(pPciDev, (void *)pDrvData->pNvmeMem);

ERR1:
    pci_set_power_state(pPciDev, PCI_D3hot);
    pci_clear_master(pPciDev);
    pci_disable_device(pPciDev);
    kfree(pDrvData);

    return -EIO;
}

static void device_remove(struct pci_dev *pPciDev)
{
    nvme_driver_data_t *pDrvData = NULL;

    pDrvData = pci_get_drvdata(pPciDev);

    if (!pDrvData)
        return;

    uint32_t _iminor = pPciDev->bus->number;

    device_destroy(_device_class, MKDEV(_imajor, _iminor));
    cdev_del(&pDrvData->charDevice);
    printk(KERN_INFO "%s: Character device destroyed: /dev/%s%d\n", KBUILD_MODNAME, KBUILD_MODNAME, _iminor);

    nvme_controller_disable(pDrvData);

    if (pDrvData->pNvmeMem)
        pci_iounmap(pPciDev, (void *)pDrvData->pNvmeMem);

    pci_release_region(pPciDev, NVME_PCI_BAR);

    pci_set_power_state(pPciDev, PCI_D3hot);
    pci_clear_master(pPciDev);
    pci_disable_device(pPciDev);

    kfree(pDrvData);

    printk(KERN_INFO "%s: PCI device removed: vendor 0x%04x, device 0x%04x, class 0x%04x, revision 0x%02x\n",
           KBUILD_MODNAME, pPciDev->vendor, pPciDev->device, pPciDev->class >> 8, pPciDev->revision);
}

static struct pci_driver _driver = {
    .name = KBUILD_MODNAME,
    .id_table = id_table,
    .probe = device_probe,
    .remove = device_remove
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
static int uevent(struct device *pDev, struct kobj_uevent_env *pEnv)
#else
static int uevent(const struct device *pDev, struct kobj_uevent_env *pEnv)
#endif
{
    (void)(pDev);
    add_uevent_var(pEnv, "DEVMODE=%#o", 0666);
    return 0;
}

static int __init kernel_object_init(void)
{
    dev_t dev;
    int err = 0;

    printk(KERN_INFO "%s: Kernel object loaded\n", KBUILD_MODNAME);

    err = alloc_chrdev_region(&dev,
                              0,    // unsigned int firstminor
                              16,   // unsigned int count
                              KBUILD_MODNAME);
    if (err < 0) {
        printk(KERN_ERR "%s: Error at alloc_chrdev_region()\n", KBUILD_MODNAME);
        return -ENOMEM;
    }

    _imajor = MAJOR(dev);

    #if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
        _device_class = class_create(THIS_MODULE, KBUILD_MODNAME);
    #else
        _device_class = class_create(KBUILD_MODNAME);
    #endif

    if (!_device_class) {
        printk(KERN_ERR "%s: Error at class_create()\n", KBUILD_MODNAME);
        return -ENOMEM;
    }

    _device_class->dev_uevent = uevent;

    return pci_register_driver(&_driver);
}

static void __exit kernel_object_exit(void)
{
    pci_unregister_driver(&_driver);

    if (_device_class)
        class_destroy(_device_class);

    if (_imajor)
        unregister_chrdev_region(MKDEV(_imajor, 0), // dev_t first
                                 16);               // unsigned int count

    printk(KERN_INFO "%s: Kernel object unloaded\n", KBUILD_MODNAME);
}

module_init(kernel_object_init)
module_exit(kernel_object_exit)
