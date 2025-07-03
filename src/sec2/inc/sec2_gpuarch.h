/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef SEC2_GPUARCH_H
#define SEC2_GPUARCH_H

/*!
 * @file sec2_gpuarch.h
 */

#include "gpuarch.h"

//
// RM shifts a nibble to the left to define GPU ARCHITECTURE.
// SEC2 only takes architecture number from RM's GPU_ARCHITECTURE.
//
#define SEC2_XLATE_RM_GPU_ARCH(arch)  \
                                  (DRF_VAL(GPU, _ARCHITECTURE, _ARCH, arch) >> 4)

// Wrapper macro to define SEC2 ARCHITECTURE
#define SEC2_ARCHITECTURE(arch)       \
                                  SEC2_XLATE_RM_GPU_ARCH(GPU_ARCHITECTURE_##arch)

/*!
 * SEC2 CHIP ID - A unique chip id which is built by concatenating arch and impl
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
} SEC2_CHIP_INFO;

#define SEC2_CHIP_ID_DEFINE(arch, impl)  (GPU_IMPLEMENTATION_##impl << 8 | \
                                          SEC2_ARCHITECTURE(arch))

#define SEC2_CHIP_ID_GM200                SEC2_CHIP_ID_DEFINE(MAXWELL2, GM200)
#define SEC2_CHIP_ID_GM204                SEC2_CHIP_ID_DEFINE(MAXWELL2, GM204)
#define SEC2_CHIP_ID_GM206                SEC2_CHIP_ID_DEFINE(MAXWELL2, GM206)

#define SEC2_CHIP_ID_GP100                SEC2_CHIP_ID_DEFINE(PASCAL,   GP100)
#define SEC2_CHIP_ID_GP102                SEC2_CHIP_ID_DEFINE(PASCAL,   GP102)
#define SEC2_CHIP_ID_GP104                SEC2_CHIP_ID_DEFINE(PASCAL,   GP104)
#define SEC2_CHIP_ID_GP106                SEC2_CHIP_ID_DEFINE(PASCAL,   GP106)
#define SEC2_CHIP_ID_GP107                SEC2_CHIP_ID_DEFINE(PASCAL,   GP107)
#define SEC2_CHIP_ID_GP108                SEC2_CHIP_ID_DEFINE(PASCAL,   GP108)

#define SEC2_CHIP_ID_GV100                SEC2_CHIP_ID_DEFINE(VOLTA,    GV100)

/*!
 * Colwenient GPU macros.
 */

#define IsArch(a)               (Sec2.chipInfo.arch == SEC2_ARCHITECTURE(a))

#define IsArchOrLater(a)        (Sec2.chipInfo.arch >= SEC2_ARCHITECTURE(a))

#define IsGpu(g)                (Sec2.chipInfo.id == SEC2_CHIP_ID_##g)

#define IsNetList(n)            (Sec2.chipInfo.netList == (n))

#define IsGpuNetList(g,n)       ((IsGpu(g)) && IsNetList(n))

#define IsEmulation()           (!IsNetList(0))

#endif //SEC2_GPUARCH_H
