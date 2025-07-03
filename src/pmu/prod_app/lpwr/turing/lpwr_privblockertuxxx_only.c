/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwrprivblockertu10x.c
 * @brief  LPWR Priv Blocker Control File
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_lw_xve.h"
#include "dev_ioctrl.h"
#include "dev_disp.h"
#include "dev_pwr_pri.h"
#include "dev_sec_pri.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objlpwr.h"
#include "pmu_objms.h"
#include "pmu_objpmu.h"
#include "pmu_objic.h"
#include "dbgprintf.h"

#include "config/g_lpwr_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------  Public Functions  ------------------------------ */

/*!
 * @brief Program and enable Allow Ranges for XVE Blocker MS Profile
 *
 * Allowlist Range allows us to access a register or a range which has been
 * listed in disallow list in the XVE blocker by HW. This is essentially a CYA.
 */
void
lpwrPrivBlockerXveMsProfileAllowRangesInit_TU10X(void)
{
#if (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
    OBJPGSTATE *pMsState = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG);
    LwU32       val;

    val = REG_RD32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA);

    // Part 1: Program the WL Ranges and enable them

    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2)            &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, SEC2))
    {
        //
        // When SEC2 RTOS is supported in the MSCG sequence, add LW_PSEC
        // registers into XVE BAR blocker Allowlist range. SEC2 RTOS will
        // make us of dedicated FB and priv blockers in order to wake us
        // up from MSCG. By adding them in disallow list, we would end up waking MSCG
        // unnecessarily when, for example, a command is posted to SEC2,
        // that doesn't result in accesses to memory or other clock gated domains.
        //

        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_START_ADDR(0),
                           DEVICE_BASE(LW_PSEC));
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_END_ADDR(0),
                           DEVICE_EXTENT(LW_PSEC));

        // Enable ALLOW_RANGE0
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _ALLOW_RANGE0, _ENABLED, val);
    }

    if (
      //!IsGPU(_TU101)                                 &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG) &&
        LPWR_CTRL_IS_SF_SUPPORTED(pMsState, MS, HSHUB))
    {
        //
        // TODO: TU101 never became a chip - remove TU101 parts
        //
        // All the LWLink registers are by default added in disallow list in XVE BAR
        // blocker due the bug: 200302166 and 1861083. Reason is in TU101,
        // the LWLink configuration registers are routed through the main
        // priv ring. So every time we need to update LWLink state, MSCG
        // has to be waken up first as we will disable the priv ring as part
        // of MSCG entry.
        //
        // Therefore after the complete dislwssion it was agreed upon that
        // for TU101, we will keep the default MSCG sequecne and XVE BAR
        // blocker will wake the MSCG for LwLink register access for TU101.
        // i.e all the registers of LWLink will be disallowed access by default.
        //
        // For the other TU10x chips i.e TU102 - TU117, we will include
        // LwLink registers  in Allowlist in XVE BAR blocker.
        //

        // LwLink Register Ranges
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_START_ADDR(1),
                           DEVICE_BASE(LW_PIOCTRL));
        REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_END_ADDR(1),
                           DEVICE_EXTENT(LW_PIOCTRL));

        // Enable ALLOW_RANGE1
        val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _ALLOW_RANGE1, _ENABLED, val);
    }

    //
    // Bug: 200435216 requires below PDISP and UDISP ranges to be in Allowlist
    //      for Win7 aero off + OGL app which causes residency drop to 0.8%.
    //
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_START_ADDR(2),
                       LW_PDISP_FE_RM_INTR_EN_HEAD_TIMING(0));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_END_ADDR(2),
                       LW_PDISP_FE_RM_INTR_EN_OR);

    // Enable ALLOW_RANGE2
    val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _ALLOW_RANGE2, _ENABLED, val);

    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_START_ADDR(3),
                       DEVICE_BASE(LW_UDISP));
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA_ALLOW_RANGE_END_ADDR(3),
                       DEVICE_EXTENT(LW_UDISP));

    // Enable ALLOW_RANGE3
    val = FLD_SET_DRF(_XVE, _BAR_BLOCKER_CYA, _ALLOW_RANGE3, _ENABLED, val);

    // Program the Allow Range settings into HW
    REG_WR32(BAR0_CFG, LW_XVE_BAR_BLOCKER_CYA, val);
#endif // (!PMUCFG_FEATURE_ENABLED(PMU_LPWR_COMPILATION_FIX_GHXXX_PLUS))
}
