/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All information
 * contained herein is proprietary and confidential to LWPU Corporation.  Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef GPUIO_H
#define GPUIO_H
#include <stdint.h>
#include <lwtypes.h>
#ifdef IS_TARGET_X86_64
#include <pci/pci.h>
#endif

typedef struct
{
#ifdef IS_TARGET_X86_64
    struct pci_access *pci_bus;
#endif
    int devmem_fd;
    LwU64 bar0_address;
    LwU64 bar0_size;
    void *bar0_mapped;
} Gpu;

Gpu *gpuOpen(unsigned device_id);
void gpuClose(Gpu *gpu);

uint32_t gpuRegRead(Gpu *gpu, unsigned offset);
void gpuRegWrite(Gpu *gpu, unsigned offset, uint32_t val);

const char *gpuPlatformType(Gpu *pGpu);
LwBool gpuIsAlive(Gpu *pGpu);
#ifdef IS_TARGET_AARCH64
static inline LwBool gpuIsDevinitCompleted(Gpu *pGpu) { return LW_TRUE; }
#else
LwBool gpuIsDevinitCompleted(Gpu *pGpu);
#endif

#endif
