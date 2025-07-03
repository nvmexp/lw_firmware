/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_pglogicwar.c
 * @brief  PMU Logic - PG WAR logics.
 *
 * This PG logic is used to run WARs in PMU PG task. PG logic receives
 * events and acts upon supported powergatables.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- System Includes -------------------------------- */
/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "pmu_objclk.h"
#include "pmu_objhal.h"
#include "pmu_objpg.h"
#include "pmu_objgr.h"
#include "pmu_objpmu.h"
#include "dbgprintf.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
LwU32 MaxGpc2ClkCyclesToWait  = 0;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
static void s_pgLogicGrWar200124305PpuThreshold(PG_LOGIC_STATE *);

/* ------------------------- Public Functions ------------------------------- */

/*!
 * PG WAR Logic.
 *
 * Handle all PG Logic for WARs.
 *
 * @param[in] @param[in]   pPgLogicState   Pointer to PG Logic state
 *
 * @returns FLCN_OK
 */
FLCN_STATUS
pgLogicWar
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PG_GR_WAR200124305_PPU_THRESHOLD))
    {
        s_pgLogicGrWar200124305PpuThreshold(pPgLogicState);
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief PPU Threshold WAR for PG_ENG(GR) coupled RPPG.
 *
 * PG_ENG(GR) manages two features -
 * 1) GR-ELPG
 * 2) GFX RPPG
 *
 * PG_ENG(GR) HW SM idle monitor counter has two different thresholds. Only one
 * threshold gets applied at given time based on current state of HW SM.
 * 1) PPU threshold  : Gets applied if SM getting IDLE notifications even after
 *                     completion of HW exit sequence. PPU threshold remains
 *                     active until HW SM gets first BUSY signal after exit. HW
 *                     SM will not start entry sequence if idle counter counting
 *                     in PPU threshold domain. This threshold avoids immediate
 *                     engagement of PG_ENG(GR) after wake-up.
 * 2) Idle threshold : When PPU threshold is not active. HW SM starts entry
 *                     sequence if we hit this threshold.
 *
 * Traditionally, GR-ELPG uses PPU threshold whereas RPPG don't need it. HW
 * didn't wanted to remove PPU threshold for RPPG to make their code simple and
 * maintainable. Thus we decided to introduce this SW WAR to ignore PPU
 * threshold for RPPG. WAR introduces BUSY spike on RPPG exit which forces
 * counter to idle threshold mode without waiting for the actual workload.
 *
 * @param[in]   pPgLogicState   Pointer to PG Logic state
 */
static void
s_pgLogicGrWar200124305PpuThreshold
(
    PG_LOGIC_STATE *pPgLogicState
)
{
    if ((pPgLogicState->eventId == PMU_PG_EVENT_POWERED_UP))
    {
        if ((pPgLogicState->ctrlId == RM_PMU_LPWR_CTRL_ID_GR_PG) &&
            (!pgCtrlIsPpuThresholdRequired(RM_PMU_LPWR_CTRL_ID_GR_PG)))
        {
            pgCtrlConfigCntrMode_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_GR_PG,
                                            PG_SUPP_IDLE_COUNTER_MODE_FORCE_BUSY);

            pgCtrlConfigCntrMode_HAL(&Pg, RM_PMU_LPWR_CTRL_ID_GR_PG,
                                            PG_SUPP_IDLE_COUNTER_MODE_AUTO_IDLE);
        }
    }
}
