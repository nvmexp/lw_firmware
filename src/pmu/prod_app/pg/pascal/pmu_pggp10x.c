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
 * @file   pmu_pggp10x.c
 * @brief  PMU Power Gating Object
 *
 * PG - PASCAL PG
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dev_pwr_csb.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objpg.h"
#include "pmu_objpmu.h"
#include "dbgprintf.h"

#include "config/g_pg_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Public Functions  ------------------------------ */

/*!
 * @brief Colwert HW FSM interrupts to SW Events
 *
 * This function colwerts FSM interrupts to SW events. This is interrupt
 * colwersion/service order -
 * 1) CFG_ERR
 * 2) PG_ON
 * 3) PG_ON_DONE
 * 4) ENG_RST
 * 5) CTX_RESTORE
 * 6) IDLE_SNAP
 *
 * @param[in/out]  pPgLogicState  Pointer to PG Logic state
 * @                              Translates the dispatch pg (RM Command or
 *                                HW interrupts) into PG event identifier
 * @param[in]   intrStat          Translates the dispatch pg (RM Command or
 *                                HW interrupts) into PG event identifier
 */
void
pgColwertPgInterrupts_GP10X
(
    PG_LOGIC_STATE *pPgLogicState,
    LwU32           intrStat
)
{
    //
    // Call kepler handler to colwert/process CFG_ERR, PG_ON, PG_ON_DONE,
    // ENG_RST, CTX_RESTORE interrupts
    //
    pgColwertPgInterrupts_GMXXX(pPgLogicState, intrStat);

    //
    // Idle-snap interrupt can come along with other FSM interrupts. Use API
    // pgCtrlEventColwertOrSend() to make sure that idle-snap colwersion is
    // not overriding the earlier state of PgLogic.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_IDLE_SNAP) &&
        FLD_TEST_DRF(_CPWR, _PMU_PG_INTRSTAT, _IDLE_SNAP, _SET, intrStat))
    {
        pgCtrlEventColwertOrSend(pPgLogicState, pPgLogicState->ctrlId,
                                 PMU_PG_EVENT_WAKEUP, PG_WAKEUP_EVENT_IDLE_SNAP);
    }
}
