/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgapgm10x.c
 * @brief  Maxwell DI specific Adaptive Power Control object file
 */

/* ------------------------ System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "pmu/pmumailboxid.h"

/* ------------------------ Application Includes --------------------------- */
#include "main.h"
#include "pmu_objhal.h"
#include "pmu_objap.h"
#include "pmu_objgcx.h"
#include "dbgprintf.h"

#include "config/g_pg_private.h"

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
LwU32 apDiIdleThresholdUsCache;
/* ------------------------ Prototypes ------------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
ct_assert(RM_PMU_MAILBOX_ID_LPWR_AP_DI < LW_CPWR_PMU_MAILBOX__SIZE_1);
/* ------------------------ Local Functions  ------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */

/*!
 * @brief Initialize Idle Mask for Adaptive GC5
 *
 * Idle Mask of GC5 = Idle Mask of MSCG + Idle Mask of DeepL1
 *                  = Idle Mask of MSCG + XP is in L1
 */
void
pgApDiIdleMaskInit_GM10X(void)
{
    AP_CTRL  *pApCtrl = AP_GET_CTRL(RM_PMU_AP_CTRL_ID_DI_DEEP_L1);
    LwU32    *pIdleMask;

    PMU_HALT_COND(pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG));

    pIdleMask = GET_OBJPGSTATE(RM_PMU_LPWR_CTRL_ID_MS_MSCG)->idleMask;

    // Copy Idle Mask of MSCG
    pApCtrl->idleMask[0] = pIdleMask[0];
    pApCtrl->idleMask[1] = pIdleMask[1];

    //
    // XP_L1 connected to xp3g2pm_tx_l1_state. It indicates that PCIE link is
    // in L1.
    //
    pApCtrl->idleMask[0] = FLD_SET_DRF(_CPWR_PMU, _IDLE_MASK, _XP_L1, _ENABLED,
                                       pApCtrl->idleMask[0]);
}

/*!
 * @brief Get DeepL1 Idle Thresholds in micro seconds for Adaptive GC5
 *
 * Adaptive GC5 uses LW_CPWR_PMU_MAILBOX(4) as scratch register to store
 * value of Idle Threshold. RM will collect value from LW_CPWR_PMU_MAILBOX(4)
 * and will update idle threshold.
 *
 * This function reads last value store in scratch register.
 */
LwU32
pgApDiIdleThresholdUsGet_GM10X(void)
{
    return apDiIdleThresholdUsCache;
}

/*!
 * @brief Set DeepL1 Idle Thresholds in micro seconds for Adaptive GC5
 *
 * @param[in]   thresholdUs     DeepL1 Idle Thresholds in micro second
 */
void
pgApDiIdleThresholdUsSet_GM10X
(
    LwU32 thresholdUs
)
{
    // Update Cache
    apDiIdleThresholdUsCache = thresholdUs;

    // Send new threshold setting to RM
    REG_WR32(CSB, LW_CPWR_PMU_MAILBOX(RM_PMU_MAILBOX_ID_LPWR_AP_DI),
                  thresholdUs);
}
