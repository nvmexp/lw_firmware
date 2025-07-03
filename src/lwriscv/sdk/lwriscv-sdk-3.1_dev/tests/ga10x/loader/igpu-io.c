
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

Gpu *gpuOpen(unsigned device_id)
{
    Gpu *gpu = calloc(1, sizeof(Gpu));

    if (!gpu)
        return gpu;

    gpu->devmem_fd = open("/dev/mem", O_SYNC | O_RDWR);
    if (gpu->devmem_fd < 0) {
        ERR_PRINT("Failed to open /dev/mem. Must run as root (or have access to /dev/mem).\n");
        goto err;
    }
    gpu->bar0_address = 0x17000000;
    gpu->bar0_size = 0xfffffff;

    // Memory map resources TODO: mmap64?
    gpu->bar0_mapped = mmap(0, gpu->bar0_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                            gpu->devmem_fd, gpu->bar0_address);
    if (gpu->bar0_mapped == (void*)-1) {
        ERR_PRINT("Failed to mmap /dev/mem. Must have sudo permissions.\n");
        goto err_close;
    }

    DBG_PRINT("Bar0 mmapped at %p.\n", gpu->bar0_mapped);

    return gpu;

err_close:
    close(gpu->devmem_fd);
err:
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

    free(gpu);
}
