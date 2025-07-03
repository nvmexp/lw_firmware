
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All information
 * contained herein is proprietary and confidential to LWPU Corporation.  Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <lwtypes.h>
#include <dev_master.h>
#include <dev_top.h>
#include <dev_gc6_island.h>
#include <dev_gc6_island_addendum.h>
#include <lwmisc.h>

#include "misc.h"
#include "gpu-io.h"
#include <pci/pci.h>

#define LW_SIZE (16*1024*1024)
#define PAGEMASK (getpagesize() - 1)

Gpu *gpuOpen(unsigned device_id)
{
    Gpu *gpu = calloc(1, sizeof(Gpu));
    struct pci_dev *dev;

    if (!gpu)
        return gpu;

    gpu->pci_bus = pci_alloc();
    pci_init(gpu->pci_bus);
    pci_scan_bus(gpu->pci_bus);

    dev = gpu->pci_bus->devices;
    while (dev)
    {
        uint32_t reg;

        reg = pci_read_word(dev, PCI_CLASS_DEVICE);
        if (reg != PCI_CLASS_DISPLAY_VGA &&
            reg != 0x0302) // this is PCI_CLASS_DISPLAY_3D, but LW_TOOLS libpci doesn't know it
            goto next;

        DBG_PRINT("Found a video device.\n");

        if (pci_read_word(dev, PCI_VENDOR_ID) != 0x10de)
            goto next;

        reg = pci_read_word(dev, PCI_DEVICE_ID);
        DBG_PRINT("Device is an LWPU device with PCI ID: %x\n", reg);

        if (device_id != reg) {
            ERR_PRINT("Warning: Target device %x does not match found device %x.\n",
                device_id, reg);
            goto next;
        }

        DBG_PRINT("PCI ID is supported by the Loader.\n");

        // No clue what's that
        pci_fill_info(dev, PCI_FILL_BASES);

        if ((dev->known_fields & PCI_FILL_BASES) == 0)
            goto next;

        gpu->bar0_address = 0;
        reg = pci_read_long(dev, PCI_BASE_ADDRESS_0);
        if ((reg & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY) {
            gpu->bar0_address = dev->base_addr[0] & ~PAGEMASK;
            gpu->bar0_size = dev->size[0];
            if ((reg & PCI_BASE_ADDRESS_MEM_TYPE_MASK) == PCI_BASE_ADDRESS_MEM_TYPE_64)
                gpu->bar0_address |= ((pciaddr_t)pci_read_long(dev, PCI_BASE_ADDRESS_1)) << 32;
        } else {
            DBG_PRINT("Unsupported BAR0.\n");
            goto next;
        }

        DBG_PRINT("Found BAR0 address at %llx-%llx (%lldMiB).\n", gpu->bar0_address,
               gpu->bar0_address + gpu->bar0_size, gpu->bar0_size >> 20);

        // TODO: do we need that?
        reg = pci_read_word(dev, PCI_COMMAND);
        pci_write_word(dev, PCI_COMMAND, (reg | PCI_COMMAND_MASTER |
                                          PCI_COMMAND_MEMORY));

        break; // Device found && setup
next:
        dev = dev->next;
    }

    if (!dev) {
        ERR_PRINT("No supported devices found.\n");
        goto err;
    }

    gpu->devmem_fd = open("/dev/mem", O_SYNC | O_RDWR);
    if (gpu->devmem_fd < 0) {
        ERR_PRINT("Failed to open /dev/mem. Must run as root (or have access to /dev/mem).\n");
        goto err;
    }

    // Memory map resources TODO: mmap64?
    gpu->bar0_mapped = mmap(0, gpu->bar0_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                            gpu->devmem_fd, gpu->bar0_address);
    if (gpu->bar0_mapped == (void*)-1) {
        ERR_PRINT("Failed to mmap /dev/mem. Must have sudo permissions.\n");
        goto err_close;
    }

    DBG_PRINT("Bar0 mmapped at %p\n", gpu->bar0_mapped);

    return gpu;

err_close:
    close(gpu->devmem_fd);
err:
    pci_cleanup(gpu->pci_bus);
    free(gpu);
    return NULL;
}

void gpuClose(Gpu *gpu)
{
    if (!gpu)
        return;

    if (gpu->bar0_mapped)
        munmap(gpu->bar0_mapped, gpu->bar0_size);

    if (gpu->devmem_fd > 0)
        close(gpu->devmem_fd);

    if (gpu->pci_bus)
        pci_cleanup(gpu->pci_bus);
    free(gpu);
}

LwBool gpuIsDevinitCompleted(Gpu *pGpu)
{
    LwU32     gfwBootStatusPlm;
    LwU32     gfwBootStatus;
    LwBool    bVbiosBootComplete = LW_FALSE;

    if (!gpuIsAlive(pGpu))
        return LW_FALSE;

    gfwBootStatusPlm = gpuRegRead(pGpu, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_PRIV_LEVEL_MASK);
    gfwBootStatus    = gpuRegRead(pGpu, LW_PGC6_AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT);

    bVbiosBootComplete = FLD_TEST_DRF(_PGC6,
                                      _AON_SELWRE_SCRATCH_GROUP_05_PRIV_LEVEL_MASK,
                                      _READ_PROTECTION_LEVEL0, _ENABLE,
                                    gfwBootStatusPlm) &&
                        FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT,
                                     _PROGRESS, _COMPLETED,
                                     gfwBootStatus) &&
                        FLD_TEST_DRF(_PGC6, _AON_SELWRE_SCRATCH_GROUP_05_0_GFW_BOOT,
                                     _VALIDATION_STATUS, _PASS_TRUSTED,
                                     gfwBootStatus);

    // try alternative scratch
    if (!bVbiosBootComplete)
    {
        gfwBootStatus = gpuRegRead(pGpu, LW_PTOP_SCRATCH1);
        bVbiosBootComplete = FLD_TEST_DRF(_PTOP, _SCRATCH1, _DEVINIT_COMPLETED, _TRUE, gfwBootStatus);
    }
    return bVbiosBootComplete;
}
