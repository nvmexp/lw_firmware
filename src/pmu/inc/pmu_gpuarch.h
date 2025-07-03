/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_GPUARCH_H
#define PMU_GPUARCH_H

/*!
 * @file pmu_gpuarch.h
 */

#include "gpuarch.h"

//
// RM shifts a nibble to the left to define GPU ARCHITECTURE.
// PMU only takes architecture number from RM's GPU_ARCHITECTURE.
//
#define PMU_XLATE_RM_GPU_ARCH(arch)  \
                                 (DRF_VAL(GPU, _ARCHITECTURE, _ARCH, arch) >> 4)

// Wrapper macro to define PMU ARCHITECTURE
#define PMU_ARCHITECTURE(arch)       \
                                 PMU_XLATE_RM_GPU_ARCH(GPU_ARCHITECTURE##arch)

/*!
 * PMU CHIP ID - A unique chip id which is built by concatenating arch and impl
 *
 * NOTE: each ID must match the value of concatenating its sub-fields, arch and
 * impl, in the assumed endian (LE). It means that id == impl << 8 | arch when
 * the struct is defined as
 *     struct
 *     {
 *         LwU8 arch;
 *         LwU8 impl;
 *     } sId;
 */
typedef struct
{
    union
    {
        struct
        {
            LwU8  arch;
            LwU8  impl;
        } sId;
        LwU16 id;
    } id;
    // Additional chip info goes here (e.g. revision, etc..)
    LwU8  majorRev;
    LwU8  minorRev;
    LwU32 netList;
} PMU_CHIP_INFO;

#define PMU_CHIP_ID_DEFINE(arch, impl)  (GPU_IMPLEMENTATION##impl << 8 | \
                                         PMU_ARCHITECTURE(arch))

#define PMU_CHIP_ID_GM107                PMU_CHIP_ID_DEFINE(_MAXWELL,  _GM107)
#define PMU_CHIP_ID_GM108                PMU_CHIP_ID_DEFINE(_MAXWELL,  _GM108)
#define PMU_CHIP_ID_GM200                PMU_CHIP_ID_DEFINE(_MAXWELL2, _GM200)
#define PMU_CHIP_ID_GM204                PMU_CHIP_ID_DEFINE(_MAXWELL2, _GM204)
#define PMU_CHIP_ID_GM206                PMU_CHIP_ID_DEFINE(_MAXWELL2, _GM206)

#define PMU_CHIP_ID_GP100                PMU_CHIP_ID_DEFINE(_PASCAL,   _GP100)
#define PMU_CHIP_ID_GP102                PMU_CHIP_ID_DEFINE(_PASCAL,   _GP102)
#define PMU_CHIP_ID_GP104                PMU_CHIP_ID_DEFINE(_PASCAL,   _GP104)
#define PMU_CHIP_ID_GP106                PMU_CHIP_ID_DEFINE(_PASCAL,   _GP106)
#define PMU_CHIP_ID_GP107                PMU_CHIP_ID_DEFINE(_PASCAL,   _GP107)
#define PMU_CHIP_ID_GP108                PMU_CHIP_ID_DEFINE(_PASCAL,   _GP108)

#define PMU_CHIP_ID_GV100                PMU_CHIP_ID_DEFINE(_VOLTA,    _GV100)

#define PMU_CHIP_ID_TU101                PMU_CHIP_ID_DEFINE(_TURING,   _TU101)
#define PMU_CHIP_ID_TU102                PMU_CHIP_ID_DEFINE(_TURING,   _TU102)
#define PMU_CHIP_ID_TU104                PMU_CHIP_ID_DEFINE(_TURING,   _TU104)
#define PMU_CHIP_ID_TU106                PMU_CHIP_ID_DEFINE(_TURING,   _TU106)
#define PMU_CHIP_ID_TU107                PMU_CHIP_ID_DEFINE(_TURING,   _TU107)
#define PMU_CHIP_ID_TU116                PMU_CHIP_ID_DEFINE(_TURING,   _TU116)
#define PMU_CHIP_ID_TU117                PMU_CHIP_ID_DEFINE(_TURING,   _TU117)

#define PMU_CHIP_ID_GA100                PMU_CHIP_ID_DEFINE(_AMPERE,   _GA100)
#define PMU_CHIP_ID_GA102                PMU_CHIP_ID_DEFINE(_AMPERE,   _GA102)
#define PMU_CHIP_ID_GA102F               PMU_CHIP_ID_DEFINE(_AMPERE,   _GA102F)
#define PMU_CHIP_ID_GA103                PMU_CHIP_ID_DEFINE(_AMPERE,   _GA103)
#define PMU_CHIP_ID_GA10B                PMU_CHIP_ID_DEFINE(_AMPERE,   _GA10B)
#define PMU_CHIP_ID_GA104                PMU_CHIP_ID_DEFINE(_AMPERE,   _GA104)
#define PMU_CHIP_ID_GA106                PMU_CHIP_ID_DEFINE(_AMPERE,   _GA106)
#define PMU_CHIP_ID_GA107                PMU_CHIP_ID_DEFINE(_AMPERE,   _GA107)
#define PMU_CHIP_ID_GA10F                PMU_CHIP_ID_DEFINE(_AMPERE,   _GA10F)
#define PMU_CHIP_ID_GA11B                PMU_CHIP_ID_DEFINE(_AMPERE,   _GA11B)

#define PMU_CHIP_ID_AD102                PMU_CHIP_ID_DEFINE(_ADA,      _AD102)
#define PMU_CHIP_ID_AD102F               PMU_CHIP_ID_DEFINE(_ADA,      _AD102F)

#define PMU_CHIP_ID_GH100                PMU_CHIP_ID_DEFINE(_HOPPER,   _GH100)

#define PMU_CHIP_ID_G000                 PMU_CHIP_ID_DEFINE(_G00X,     _G000)

/*!
 * Colwenient GPU macros.
 */

#define IsARCH(a)               (Pmu.chipInfo.id.sId.arch == PMU_ARCHITECTURE(a))

#define IsGPU(g)                (Pmu.chipInfo.id.id == PMU_CHIP_ID##g)

#define IsNetList(n)            (Pmu.chipInfo.netList == (n))

#define IsNetListOrLater(n)     (Pmu.chipInfo.netList >= (n))

#define IsGpuNetList(g,n)       ((IsGPU(g)) && IsNetList(n))

#define IsEmulation()           (!IsNetList(0))

#define IsSimulation()          (Pmu.chipInfo.id.sId.arch == PMU_ARCHITECTURE(_SIMS))

// Ideally we should make use of PTOP_PLATFORM_TYPE register to find this out
#define IsSilicon()             (!IsEmulation() && !IsSimulation())

#endif //PMU_GPUARCH_H
