
/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef FBFLCN_GPUARCH_H
#define FBFLCN_GPUARCH_H

/*!
 * @file fbflcn_gpuarch.h
 */

#include "gpuarch.h"

//
// RM shifts a nibble to the left to define GPU ARCHITECTURE.
// FBFalcon only takes architecture number from RM's GPU_ARCHITECTURE.
//
#define FBFLCN_XLATE_RM_GPU_ARCH(arch)  \
                                  (DRF_VAL(GPU, _ARCHITECTURE, _ARCH, arch) >> 4)

// Wrapper macro to define FB Falcon ARCHITECTURE
#define FBFLCN_ARCHITECTURE(arch)       \
                                  FBFLCN_XLATE_RM_GPU_ARCH(GPU_ARCHITECTURE_##arch)

/*!
 * FBFLCN CHIP ID - A unique chip id which is built by concatenating arch and impl
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
} FBFLCN_CHIP_INFO;

#define FBFLCN_CHIP_ID_DEFINE(arch, impl)  (GPU_IMPLEMENTATION_##impl << 8 | \
                                           FBFLCN_ARCHITECTURE(arch))

#define FBFLCN_CHIP_ID_GM200                FBFLCN_CHIP_ID_DEFINE(MAXWELL2, GM200)
#define FBFLCN_CHIP_ID_GM204                FBFLCN_CHIP_ID_DEFINE(MAXWELL2, GM204)
#define FBFLCN_CHIP_ID_GM206                FBFLCN_CHIP_ID_DEFINE(MAXWELL2, GM206)

#define FBFLCN_CHIP_ID_GP100                FBFLCN_CHIP_ID_DEFINE(PASCAL,   GP100)
#define FBFLCN_CHIP_ID_GP102                FBFLCN_CHIP_ID_DEFINE(PASCAL,   GP102)
#define FBFLCN_CHIP_ID_GP104                FBFLCN_CHIP_ID_DEFINE(PASCAL,   GP104)
#define FBFLCN_CHIP_ID_GP106                FBFLCN_CHIP_ID_DEFINE(PASCAL,   GP106)
#define FBFLCN_CHIP_ID_GP107                FBFLCN_CHIP_ID_DEFINE(PASCAL,   GP107)
#define FBFLCN_CHIP_ID_GP108                FBFLCN_CHIP_ID_DEFINE(PASCAL,   GP108)

#define FBFLCN_CHIP_ID_GV100                FBFLCN_CHIP_ID_DEFINE(VOLTA,    GV100)

/*!
 * Colwenient GPU macros.
 */

#define IsArch(a)               (Sec2.chipInfo.arch == FBFLCN_ARCHITECTURE(a))

#define IsArchOrLater(a)        (Sec2.chipInfo.arch >= FBFLCN_ARCHITECTURE(a))

#define IsGpu(g)                (Sec2.chipInfo.id == FBFLCN_CHIP_ID_##g)

#define IsNetList(n)            (Sec2.chipInfo.netList == (n))

#define IsGpuNetList(g,n)       ((IsGpu(g)) && IsNetList(n))

#define IsEmulation()           (!IsNetList(0))

#endif //FBFLCN_GPUARCH_H

