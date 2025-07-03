/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SOE_GPUARCH_H
#define SOE_GPUARCH_H

/*!
 * @file soe_gpuarch.h
 */

#include "gpuarch.h"

//
// RM shifts a nibble to the left to define GPU ARCHITECTURE.
// SOE only takes architecture number from RM's GPU_ARCHITECTURE.
//
#define SOE_XLATE_RM_GPU_ARCH(arch)  \
                                  (DRF_VAL(GPU, _ARCHITECTURE, _ARCH, arch) >> 4)

// Wrapper macro to define SOE ARCHITECTURE
#define SOE_ARCHITECTURE(arch)       \
                                  SOE_XLATE_RM_GPU_ARCH(GPU_ARCHITECTURE_##arch)

/*!
 * SOE CHIP ID - A unique chip id which is built by concatenating arch and impl
 *
 * NOTE: each ID must match the value of concatenating its sub-fields, arch and
 * impl, in the assumed endian (LE). It means that id == impl << 8 | arch when
 * the struct is defined as
 *     struct
 *     {
 *         LwU8 arch;
 *         LwU8 impl;
 *     };
 */
typedef struct
{
    union
    {
        struct
        {
            LwU8  arch;
            LwU8  impl;
        };
        LwU16 id;
    };
    // additional chip info goes here (e.g. revision, etc..)
    LwU8  majorRev;
    LwU8  minorRev;
    LwU32 netList;
} SOE_CHIP_INFO;

#define SOE_CHIP_ID_DEFINE(arch, impl)  (GPU_IMPLEMENTATION_##impl << 8 | \
                                          SOE_ARCHITECTURE(arch))
/*!
 * Colwenient GPU macros.
 */

#define IsArch(a)               (Soe.chipInfo.arch == SOE_ARCHITECTURE(a))

#define IsArchOrLater(a)        (Soe.chipInfo.arch >= SOE_ARCHITECTURE(a))

#define IsGpu(g)                (Soe.chipInfo.id == SOE_CHIP_ID_##g)

#define IsNetList(n)            (Soe.chipInfo.netList == (n))

#define IsGpuNetList(g,n)       ((IsGpu(g)) && IsNetList(n))

#define IsEmulation()           (!IsNetList(0))

#endif //SOE_GPUARCH_H
