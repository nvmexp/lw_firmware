/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    pmu_pmugm20x.c
 *          PMU HAL Functions for GM20X and later GPUs
 *
 * PMU HAL functions implement chip-specific initialization, helper functions
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_pri.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpmu.h"

#include "config/g_pmu_private.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
//
// These variables are meaningless on RISCV, since SCTL is not used,
// and priv level is different for u mode and s mode so there's no
// need to reset to a "default".
//
#ifndef UPROC_RISCV
/*!
 * Define variables to cache PMU default privilege level state
 */
LwU8      PmuPrivLevelExtDefault;
LwU8      PmuPrivLevelCsbDefault;
LwU32     PmuPrivLevelCtrlRegAddr;
#endif // UPROC_RISCV

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief Set the falcon privilege level
 *
 * @param[in]  privLevelExt  falcon EXT privilege level
 * @param[in]  privLevelCsb  falcon CSB privilege level
 */
void
pmuFlcnPrivLevelSet_GM20X
(
    LwU8 privLevelExt,
    LwU8 privLevelCsb
)
{
    LwU32 flcnSctl1 = 0;

    flcnSctl1 = FLD_SET_DRF_NUM(_CPWR, _FALCON, _SCTL1_CSBLVL_MASK, privLevelCsb, flcnSctl1);
    flcnSctl1 = FLD_SET_DRF_NUM(_CPWR, _FALCON, _SCTL1_EXTLVL_MASK, privLevelExt, flcnSctl1);

    REG_WR32_STALL(CSB, LW_CPWR_FALCON_SCTL1, flcnSctl1);
}

/*!
 * @brief   Cache if in LS mode and default privilege level of falcon,
 *          and configure reset PLM.
 */
void
pmuPreInitFlcnPrivLevelCacheAndConfigure_GM20X(void)
{
    // Cache if in LS mode
    LwU32 flcnSctl = REG_RD32(CSB, LW_CPWR_FALCON_SCTL);
    Pmu.bLSMode = FLD_TEST_DRF(_CPWR, _FALCON_SCTL, _LSMODE, _TRUE, flcnSctl);

    // MMINTS-TODO: remove UPROC_RISCV check once GA100 RISCV PMU goes away
#ifndef UPROC_RISCV
    // Cache default privilege level
    PmuPrivLevelExtDefault  = LW_CPWR_FALCON_SCTL1_EXTLVL_MASK_INIT;
    PmuPrivLevelCsbDefault  = LW_CPWR_FALCON_SCTL1_CSBLVL_MASK_INIT;

    // BAR0 address is cached here, as sequencer task only does BAR0 writes.
    PmuPrivLevelCtrlRegAddr = LW_PPWR_FALCON_SCTL1;
#endif // UPROC_RISCV

    //
    // Raise priv level masks so that RM doesn't mess around with our operation
    // while we're running
    //
    pmuPreInitRaisePrivLevelMasks_HAL();
}

#if PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SELWRITY_BYPASS)
/*!
 * @brief   Cache the Vbios Security Bypass Register (VSBR).
 */
void
pmuPreInitVbiosSelwrityBypassRegisterCache_GM20X(void)
{
    PmuVSBCache = REG_RD32(CSB, LW_CPWR_THERM_SCRATCH(0));
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_VBIOS_SELWRITY_BYPASS)

/*
 * @brief Raise priv level masks so that RM doesn't interfere with PMU
 * operation until they are lowered again.
 */
void
pmuPreInitRaisePrivLevelMasks_GM20X(void)
{
    if (Pmu.bLSMode)
    {
        //
        // Only raise priv level masks when we're in booted in LS mode,
        // else we'll take away our own ability to reset the masks when we
        // unload.
        //
        REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FALCON_IRQTMR_PRIV_LEVEL_MASK,
                   _WRITE_PROTECTION_LEVEL0, _DISABLE);
    }
}

/*!
 * @brief Lower priv level masks when we unload
 */
void
pmuLowerPrivLevelMasks_GM20X(void)
{
    REG_FLD_WR_DRF_DEF(CSB, _CPWR, _FALCON_IRQTMR_PRIV_LEVEL_MASK,
               _WRITE_PROTECTION_LEVEL0, _ENABLE);
}

/*!
 * @brief More generic interface to lower priv level masks when we unload
 */
void
pmuDetachUnloadSequence_GM20X(LwBool bIsGc6Detach)
{
    (void)bIsGc6Detach;

    pmuLowerPrivLevelMasks_HAL();
}
