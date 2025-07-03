/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "therm/thrmpolicy_dtc_vf.h"
#include "pmu_objperf.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * THERM_POLICY_DTC_VF implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermPolicyGrpIfaceModel10ObjSetImpl_DTC_VF
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_THERM_THERM_POLICY_DTC_VF_BOARDOBJ_SET *pSet =
        (RM_PMU_THERM_THERM_POLICY_DTC_VF_BOARDOBJ_SET *)pBoardObjSet;
    LwBool                  bFirstConstruct = (*ppBoardObj == NULL);
    BOARDOBJ               *pTmpBoardObj;
    THERM_POLICY_DTC_VF    *pDtcVf;
    FLCN_STATUS             status;

    // Call the DOMGRP constructor.
    status = thermPolicyGrpIfaceModel10ObjSetImpl_DOMGRP(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDtcVf       = (THERM_POLICY_DTC_VF *)*ppBoardObj;
    pTmpBoardObj = &(pDtcVf->dtc.super);

    // Construct the THERM_POLICY_DTC interface.
    status = thermPolicyGrpIfaceModel10ObjSetImpl_DTC(pModel10, &pTmpBoardObj, size,
            (RM_PMU_BOARDOBJ *)&pSet->dtc);
    if (status != FLCN_OK)
    {
        return status;
    }

    pDtcVf->vfIdxMin    = pSet->vfIdxMin;
    pDtcVf->vfIdxMax    = pSet->vfIdxMax;
    pDtcVf->limitMax    = pSet->limitMax;
    pDtcVf->limitTdp    = pSet->limitTdp;
    pDtcVf->limitPivot  = pSet->limitPivot;

    if (bFirstConstruct)
    {
        //
        // We only want to set the current limit on the first initialization.
        // Subsequent calls (e.g. change thermal limits) need to leave the
        // current limit unchanged.
        //
        pDtcVf->limitLwrr = pDtcVf->limitMax;
    }

    return status;
}

/*!
 * DTC_VF-class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
thermPolicyIfaceModel10GetStatus_DTC_VF
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_THERM_THERM_POLICY_DTC_VF_BOARDOBJ_GET_STATUS *pGetStatus;
    THERM_POLICY_DTC_VF                                  *pDtcVf;
    THERM_POLICY_DTC                                     *pDtc;
    FLCN_STATUS                                           status;

    // Get super class information.
    status = thermPolicyIfaceModel10GetStatus_DOMGRP(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDtcVf     = (THERM_POLICY_DTC_VF *)pBoardObj;
    pDtc       = &pDtcVf->dtc;
    pGetStatus = (RM_PMU_THERM_THERM_POLICY_DTC_VF_BOARDOBJ_GET_STATUS *)pPayload;

    return thermPolicyDtcQuery(pDtc, &pGetStatus->dtc);
}

/*!
 * @copydoc ThermPolicyCallbackExelwte
 */
FLCN_STATUS
thermPolicyCallbackExelwte_DTC_VF
(
    THERM_POLICY *pPolicy
)
{
    THERM_POLICY_DTC_VF *pDtcVf  = (THERM_POLICY_DTC_VF *)pPolicy;
    THERM_POLICY_DOMGRP *pDomGrp = &pDtcVf->super;
    THERM_POLICY_DTC    *pDtc    = &pDtcVf->dtc;
    FLCN_STATUS          status  = FLCN_OK;

    LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS limits;
    LwS32 levelChange;
    LwU32 newLimit = pDtcVf->limitLwrr;

    // Determine limit changes & new VF index limit.
    levelChange = thermPolicyDtcGetLevelChange(pDtc,
                                       pDtcVf->super.super.channelReading);
    if (levelChange != 0)
    {
        if ((levelChange > 0) &&
            ((pDtcVf->limitLwrr >= (pDtcVf->limitMax - levelChange)) ||
             (levelChange == THERM_POLICY_DTC_LIMIT_CHANGE_IF_DISENGAGED)))
        {
            // Increasing performance & will exceed the max. perf. level.
            newLimit = pDtcVf->limitMax;
        }
        else if ((levelChange < 0) &&
                 (pDtcVf->limitLwrr <= (LwU32)(-levelChange)))
        {
            // Decreasing performance & will drop below min. perf. level.
            newLimit = 0;
        }
        else
        {
            // Otherwise, adjust accordingly.
            newLimit = pDtcVf->limitLwrr + levelChange;
        }

        // Limit to rated TDP, if necessary.
        if ((thermPolicyDomGrpIsRatedTdpLimited(pDomGrp)) &&
            (newLimit < pDtcVf->limitTdp))
        {
            newLimit = pDtcVf->limitTdp;
        }
    }

    if (pDtcVf->limitLwrr != newLimit)
    {
        // Controller needs to update limits.
        pDtcVf->limitLwrr = newLimit;

        thermPolicyPerfLimitsClear(&limits);

        // Translate limitLwrr into valid limits.
        if (pDtcVf->limitLwrr != pDtcVf->limitMax)
        {
            // P-state limit
            if (pDtcVf->limitLwrr < pDtcVf->limitPivot)
            {
                limits.limit[LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_IDX_PSTATE] =
                    pDtcVf->limitLwrr;
            }

            // GPC2CLK limit
            if (pDtcVf->limitLwrr <= pDtcVf->limitPivot)
            {
                limits.limit[LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_IDX_GPC2CLK] =
                    Perf.tables.pVfEntries[pDtcVf->vfIdxMin].maxFreqKHz;
            }
            else
            {
                LwU32 idx = pDtcVf->vfIdxMin + (pDtcVf->limitLwrr - pDtcVf->limitPivot);
                limits.limit[LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_IDX_GPC2CLK] =
                    Perf.tables.pVfEntries[idx].maxFreqKHz;
            }
        }

        // Apply new limits
        status = thermPolicyDomGrpApplyLimits(pDomGrp, &limits);
    }

    return status;
}

/* ---------------------- Private Static Functions -------------------------- */

