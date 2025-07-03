/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_version_ga10x.c
 * @brief  Code for interfacing with the ucode versioning in hardware.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmu.h"
#include "pmu_objfuse.h"

#include "dev_fuse.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Retrieves the fuse version from hardware.
 *
 * @return  The fuse version
 */
LwU32
pmuFuseVersionGet_GA10X(void)
{
    return REF_VAL(
        LW_FUSE_OPT_FUSE_UCODE_LSPMU_REV_DATA,
        fuseRead(LW_FUSE_OPT_FUSE_UCODE_LSPMU_REV));
}

/*!
 * @brief   Retrieves the FPF version from hardware.
 *
 * @return  The FPF version
 */
LwU32
pmuFpfVersionGet_GA10X(void)
{
    LwU32 fpfVal = REF_VAL(
        LW_FUSE_OPT_FPF_UCODE_LSPMU_REV_DATA,
        fuseRead(LW_FUSE_OPT_FPF_UCODE_LSPMU_REV));
    LwU32 version = 0;

    //
    // Note: the numeric FPF version is:
    //  0, if fpfVal == 0
    //  highest bit index + 1, otherwise
    //
    while (fpfVal != 0U)
    {
        version++;
        fpfVal >>= 1;
    }

    return version;
}
