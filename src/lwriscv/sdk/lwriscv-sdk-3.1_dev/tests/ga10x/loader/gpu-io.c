/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2019 by LWPU Corporation.  All rights reserved.  All information
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

#define LW_SIZE (16*1024*1024)
#define PAGEMASK (getpagesize() - 1)

uint32_t gpuRegRead(Gpu *gpu, unsigned offset)
{
    volatile uint32_t *addr;
    uint32_t val;

    if (!gpu)
        return 0x0BADADD3;

    addr = (uint32_t *)((uint8_t *)(gpu->bar0_mapped) + offset);

    val = *addr;
    if (val == 0xbadf5620)
    {
        printf("INVALID PRI LOCKDOWN READ @%x\n", offset);
    }

    return val;
}

void gpuRegWrite(Gpu *gpu, unsigned offset, uint32_t val)
{
    volatile uint32_t *addr;

    if (!gpu)
        return;

    addr = (uint32_t *)((uint8_t *)(gpu->bar0_mapped) + offset);
    *addr = val;
}

const char *gpuPlatformType(Gpu *pGpu)
{
    static const char *platforms[] = {
        "Silicon",
        "Fmodel",
        "RTL",
        "Emulation",
        "FPGA"
    };
    LwU32 reg = gpuRegRead(pGpu, LW_PTOP_PLATFORM);

    if (reg < 5)
        return platforms[reg];
    return "Unknown";
}

LwBool gpuIsAlive(Gpu *pGpu)
{
    return gpuRegRead(pGpu, LW_PMC_BOOT_0) != 0xFFFFFFFF;
}

