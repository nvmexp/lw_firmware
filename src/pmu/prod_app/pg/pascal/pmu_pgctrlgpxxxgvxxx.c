/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pgctrlgpxxxgvxxx.c
 * @brief  PMU PG Controller Operation (Pascal and Volta)
 */

/* ------------------------ System includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"
#include "dev_pwr_falcon_csb.h"

/* ------------------------ Application includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "dbgprintf.h"

#include "config/g_pg_private.h"

/* ------------------------ Types definitions ------------------------------ */
/* ------------------------ External definitions --------------------------- */
/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Static variables ------------------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Prototypes  ------------------------------------ */
/* ------------------------ Private Prototypes ----------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief Processes Idle Snap interrupt
 *
 * This function reads the idle status registers corresponding to idle snap
 * and checks reason for idle snap.
 *
 * @param[in]   ctrlId          PG controller id
 *
 * @return  PG_IDLE_SNAP_REASON_WAKE            For valid wake-up
 *          PG_IDLE_SNAP_REASON_ERR_DEFAULT     Otherwise
 */
LwU32
pgCtrlIdleSnapReasonMaskGet_GP10X
(
    LwU32 ctrlId
)
{
    LwU32       val;
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    
    // Set IDX to current engine and enable SNAP_READ
    val = REG_RD32(CSB, LW_CPWR_PMU_PG_CTRL);
    val = FLD_SET_DRF_NUM(_CPWR, _PMU_PG_CTRL, _IDLE_SNAP_IDX, pPgState->hwEngIdx, val);
    val = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _IDLE_SNAP_READ, _ENABLE, val);
    REG_WR32(CSB, LW_CPWR_PMU_PG_CTRL, val);

    // Cache IDLE_STATUS and IDLE_STATUS1
    pPgState->idleStatusCache[0] = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS);
    pPgState->idleStatusCache[1] = REG_RD32(CSB, LW_CPWR_PMU_IDLE_STATUS_1);

    // Disable SNAP_READ
    val = FLD_SET_DRF(_CPWR, _PMU_PG_CTRL, _IDLE_SNAP_READ, _DISABLE, val);
    REG_WR32(CSB, LW_CPWR_PMU_PG_CTRL, val);

    //
    // Exception: GFX RPPG with SEC2 RTOS
    // Wake GR if SW0 signal raised IDLE_SNAP interrupt
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2_RTOS_WAR_200089154) &&
        PMUCFG_FEATURE_ENABLED(PMU_PG_GR)                      &&
        FLD_TEST_DRF(_CPWR, _PMU_IDLE_MASK_1, _SW0,
                     _ENABLED, pPgState->idleMask[1])          &&
        FLD_TEST_DRF(_CPWR, _PMU_IDLE_STATUS_1,
                     _SW0, _BUSY, pPgState->idleStatusCache[1]))
    {
        // Its valid wake-up. Return reason as _WAKE
        return PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF;
    }
    //Wake GR if GR_FE signal raised IDLE_SNAP interrupt
    else if (PMUCFG_FEATURE_ENABLED(PMU_PG_WAR_BUG_2432734) &&
             PMUCFG_FEATURE_ENABLED(PMU_PG_GR)              &&
             FLD_TEST_DRF(_CPWR, _PMU_IDLE_MASK, _GR,
                          _ENABLED, pPgState->idleMask[0])  &&
             FLD_TEST_DRF(_CPWR, _PMU_IDLE_STATUS,
                          _GR_FE, _BUSY, pPgState->idleStatusCache[0]))
    {
        // Its valid wake-up. Return reason as _WAKE
        return PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF;
    }
    //
    // Exception: MSCG with SEC2 RTOS
    // Wake MSCG if SW1 signal raised IDLE_SNAP interrupt
    //
    else if (PMUCFG_FEATURE_ENABLED(PMU_PG_SEC2_RTOS_WAR_200089154) &&
             PMUCFG_FEATURE_ENABLED(PMU_PG_MS)                      &&
             FLD_TEST_DRF(_CPWR, _PMU_IDLE_MASK_1, _SW1,
                          _ENABLED, pPgState->idleMask[1])          &&
             FLD_TEST_DRF(_CPWR, _PMU_IDLE_STATUS_1,
                          _SW1, _BUSY, pPgState->idleStatusCache[1]))
    {
        // Its valid wake-up. Return reason as _WAKE
        return PG_IDLE_SNAP_REASON_WAKEUP_IDLE_FLIP_PWR_OFF;
    }
    else
    {
        // Default idle snap should be treated as an error.
        return PG_IDLE_SNAP_REASON_ERR_IDLE_FLIP_PWR_OFF;
    }
}
