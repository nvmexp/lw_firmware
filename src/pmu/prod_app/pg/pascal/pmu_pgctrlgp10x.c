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
 * @file   pmu_pgctrlgp10x.c
 * @brief  PMU PG Controller Operation (Pascal)
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
 * @brief Initialize FSM interrupts
 *
 * Enable idle-snap interrupt on init.
 *
 * @param[in]  ctrlId   PG controller ID
 */
void
pgCtrlInterruptsInit_GP10X
(
    LwU32 ctrlId
)
{
    LwU32 hwEngIdx = PG_GET_ENG_IDX(ctrlId);
    LwU32 data;

    // Enable idle-snap interrupt on init
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_IDLE_SNAP))
    {
        data = REG_RD32(CSB, LW_CPWR_PMU_PG_INTREN(hwEngIdx));
        data = FLD_SET_DRF(_CPWR, _PMU_PG_INTREN, _IDLE_SNAP, _ENABLE, data);
        REG_WR32(CSB, LW_CPWR_PMU_PG_INTREN(hwEngIdx), data);
    }
}

/*!
 * @brief Process Idle snap interrupt
 *
 * This API checks if idle-snap interrupt pending. If its pending then it
 * finds the reason for idle snap and disable PgCtrl if idle snap is generated
 * because of error.
 *
 * @param[in]  ctrlId   PG controller ID
 */
void
pgCtrlIdleSnapProcess_GP10X
(
    LwU32 ctrlId
)
{
    OBJPGSTATE *pPgState = GET_OBJPGSTATE(ctrlId);
    LwU32       hwEngIdx = PG_GET_ENG_IDX(ctrlId);
    LwU32       intrStat;

    intrStat = REG_RD32(CSB, LW_CPWR_PMU_PG_INTRSTAT(hwEngIdx));
    if (FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _IDLE_SNAP, _SET, intrStat))
    {
        // Get the reason for IDLE_SNAP
        pPgState->idleSnapReasonMask =
            pgCtrlIdleSnapReasonMaskGet_HAL(&Pg, ctrlId);
        PMU_HALT_COND(pPgState->idleSnapReasonMask != PG_IDLE_SNAP_REASON__INIT);

        // Disallow PgCtrl on error
        if (PG_CTRL_IS_IDLE_SNAP_ERR(pPgState->idleSnapReasonMask))
        {
            pgCtrlEventSend(ctrlId, PMU_PG_EVENT_DISALLOW,
                            PG_CTRL_DISALLOW_REASON_MASK(IDLE_SNAP));
        }

        // Idle-snap processing completed. Clear the interrupt.
        intrStat = FLD_SET_DRF(_CPWR, _PMU_PG_INTRSTAT, _IDLE_SNAP, _CLEAR, intrStat);
        REG_WR32(CSB, LW_CPWR_PMU_PG_INTRSTAT(hwEngIdx), intrStat);
    }
}
