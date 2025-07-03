/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    task_acr.h
 * @copydoc task_acr.c
 */

#ifndef TASK_ACR_H
#define TASK_ACR_H

/* ------------------------- System Includes ------------------------------- */
#include "pmusw.h"
#if LWRISCV_PARTITION_SWITCH
# include "drivers/drivers.h"
# include "syslib/syslib.h"
# include "shlib/partswitch.h"
#endif // LWRISCV_PARTITION_SWITCH

/* ------------------------- Application Includes -------------------------- */
#include "unit_api.h"

/* ------------------------- Types Definitions ----------------------------- */

/*!
 * A union of all available commands to ACR.
 */
typedef union
{
    DISP2UNIT_HDR       hdr;
    DISP2UNIT_RM_RPC    rpc;
} DISPATCH_ACR;

/*!
 * @brief: memory aperture settings of TRANSCFG
 */
struct FBIF_MEMORY_APERTURE
{
    /*!
      Context DMA id to indentify TRANSCFG
     */
    LwU8 ctxDma;

    /*!
     * Targetted Address space. it can be FB or SYS
     */
    LwU8 addrSpace;

    /*!
     * CPU Cache attrb for SYS MEM
     */
    LwU8 cpuCacheAttrib;
};

/* ------------------------- Defines --------------------------------------- */
#define ACR_BUFFER_SIZE_BYTES                           0x100

/*!
 * Target memory Location
 */
#define ADDR_FBMEM                                      0
#define ADDR_SYSMEM                                     1

/*!
 * CPU cache attributes
 */
#define LW_MEMORY_CACHED                                0
#define LW_MEMORY_UNCACHED                              1

/* ------------------------- Function Prototypes  -------------------------- */
#if PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
//! On RISCV cheetah profiles, this file is shared with the ACR partition
# define PARTITION_ID_TEGRA_ACR 1

# define TEGRA_ACR_FUNC_ACR_BOOTSTRAP_FALCON     0
# define TEGRA_ACR_FUNC_ACR_INIT_WPR_REGION      1
# define TEGRA_ACR_FUNC_ACR_BOOTSTRAP_GR_FALCONS 2
#endif // PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)

#if !defined(PMU_TEGRA_ACR_PARTITION) && PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)
# if !LWRISCV_PARTITION_SWITCH
#  error LWRISCV_PARTITION_SWITCH must be set if PMU_ACR_HS_PARTITION is enabled!
# endif // !LWRISCV_PARTITION_SWITCHvoid

# define acrBootstrapFalcon(falconId, bReset, bUseVA, ...) \
        partitionSwitch(PARTITION_ID_TEGRA_ACR, TEGRA_ACR_FUNC_ACR_BOOTSTRAP_FALCON, (falconId), (bReset), (bUseVA), 0)
# define acrInitWprRegion(wprRegionId, wprOffset) \
        partitionSwitch(PARTITION_ID_TEGRA_ACR, TEGRA_ACR_FUNC_ACR_INIT_WPR_REGION, (wprRegionId), (wprOffset), 0, 0)
# define acrBootstrapGrFalcons(falconIdMask, bReset, falcolwAMask, wprBaseVirtual) \
        partitionSwitch(PARTITION_ID_TEGRA_ACR, TEGRA_ACR_FUNC_ACR_BOOTSTRAP_GR_FALCONS, (falconIdMask), (bReset), (falcolwAMask),\
                        (LwU64_ALIGN32_VAL(&(wprBaseVirtual))))
#else // !defined(PMU_TEGRA_ACR_PARTITION) && PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)

FLCN_STATUS acrBootstrapFalcon(LwU32 falconId, LwBool bReset, LwBool bUseVA, RM_FLCN_U64 *pWprBaseVirtual)
        GCC_ATTRIB_SECTION("imem_acr", "acrBootstrapFalcon");
FLCN_STATUS acrInitWprRegion(LwU8 wprRegionId, LwU8 wprOffset)
        GCC_ATTRIB_SECTION("imem_acr", "acrInitWprRegion");
FLCN_STATUS acrBootstrapGrFalcons(LwU32 falconIdMask, LwBool bReset, LwU32 falcolwAMask, RM_FLCN_U64 wprBaseVirtual)
        GCC_ATTRIB_SECTION("imem_acr", "acrBootstrapGrFalcons");
#endif // !defined(PMU_TEGRA_ACR_PARTITION) && PMUCFG_FEATURE_ENABLED(PMU_ACR_HS_PARTITION)

/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */

#endif // TASK_ACR_H
