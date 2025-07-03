/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchrel_weighted.c
 * @brief Interface specification for a weighted PWR_CHRELATIONSHIP class.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrchrel_weighted.h"
#include "lib/lib_fxp.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc PwrChRelationshipTupleEvaluate
 */
FLCN_STATUS
pwrChRelationshipTupleEvaluate_WEIGHTED
(
    PWR_CHRELATIONSHIP              *pChRel,
    LwU8                             chIdxEval,
    PWR_CHRELATIONSHIP_TUPLE_STATUS *pStatus
)
{
    LW2080_CTRL_PMGR_PWR_TUPLE tuple = { 0 };
    LwSFXP20_12 weight;
    FLCN_STATUS status;

    // Query the PWR_CHANNEL tuple for this relationship.
    status = pwrMonitorChannelTupleStatusGet(Pmgr.pPwrMonitor, pChRel->chIdx,
                                             &tuple);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Obtain weight associated with this relationship.
    weight = pwrChRelationshipWeightGet(pChRel, chIdxEval);

    //
    // Multiply the power/current by the weight value and scale back to integer.
    //
    // From FXP20.12 point of view, this is safe since the multSFXP32 API
    // is designed to handle intermediate overflow. However, 32-bit overflow
    // is still possible (at ~4.29MW/MA).
    //
    pStatus->pwrmW  = multSFXP32(12, (LwS32)tuple.pwrmW, weight);
    pStatus->lwrrmA = multSFXP32(12, (LwS32)tuple.lwrrmA, weight);
    pStatus->voltuV = (LwS32)tuple.voltuV;

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
