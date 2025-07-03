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
 * @file    pstate_3x_infra.c
 * @brief   Function definitions for the PSTATE_3X_INFRA SW module.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "perf/3x/pstate_3x.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc PerfPstateGetByLevel()
 *
 * @memberof PSTATES
 *
 * @public
 */
PSTATE *
perfPstateGetByLevel
(
    LwU8 level
)
{
    PSTATE         *pPstate;
    LwBoardObjIdx   pstateIdx   = 0;
    LwU8            tmpLevel    = 0;
    FLCN_STATUS     status      = FLCN_OK;

    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_BEGIN(PERF, PSTATE, BASE, pPstate,
                                            pstateIdx, NULL, &status,
                                            perfPstateGetByLevel_exit)
    {
        // TODO: add a level parameter to the PSTATE object in the PMU and make this check nicer
        if (tmpLevel == level)
        {
            return pPstate;
        }

        ++tmpLevel;
    }
    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_END;

perfPstateGetByLevel_exit:
    return NULL;
}

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- End of File ------------------------------------ */
