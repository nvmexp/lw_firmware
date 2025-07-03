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
#include "therm/thrmpolicy_dtc_volt.h"
#include "therm/thrmpolicy_dtc.h"

/* ------------------------ Static Function Prototypes --------------------- */
static LwU32                      s_thermPolicyDtcVoltRatedTdpLevelGet(THERM_POLICY_DTC_VOLT *pDtcVolt)
    GCC_ATTRIB_SECTION("imem_thermLibPolicy", "s_thermPolicyDtcVoltRatedTdpLevelGet");

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions  ------------------------------ */
/*!
 * THERM_POLICY_DTC_VF implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
thermPolicyGrpIfaceModel10ObjSetImpl_DTC_VOLT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_THERM_THERM_POLICY_DTC_VOLT_BOARDOBJ_SET *pSet =
        (RM_PMU_THERM_THERM_POLICY_DTC_VOLT_BOARDOBJ_SET *)pBoardObjSet;
    LwBool                  bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS             status          = FLCN_OK;
    BOARDOBJ               *pTmpBoardObj;
    THERM_POLICY_DTC_VOLT  *pDtcVolt;

    // Call the DOMGRP constructor.
    status = thermPolicyGrpIfaceModel10ObjSetImpl_DOMGRP(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDtcVolt     = (THERM_POLICY_DTC_VOLT *)*ppBoardObj;
    pTmpBoardObj = &(pDtcVolt->dtc.super);

    // Construct the THERM_POLICY_DTC interface.
    status = thermPolicyGrpIfaceModel10ObjSetImpl_DTC(pModel10, &pTmpBoardObj, size,
            (RM_PMU_BOARDOBJ *)&pSet->dtc);
    if (status != FLCN_OK)
    {
        return status;
    }

    // Set class-specific data.
    pDtcVolt->voltageMaxuV       = pSet->voltageMaxuV;
    pDtcVolt->voltageMinuV       = pSet->voltageMinuV;
    pDtcVolt->voltageStepuV      = pSet->voltageStepuV;
    pDtcVolt->limitMax           = pSet->limitMax;
    pDtcVolt->limitPivot         = pSet->limitPivot;
    pDtcVolt->ratedTdpVfEntryIdx = pSet->ratedTdpVfEntryIdx;

    if (bFirstConstruct)
    {
        //
        // We only want to set the current limit on the first initialization.
        // Subsequent calls (e.g. change thermal limits) need to leave the
        // current limit unchanged.
        //
        pDtcVolt->limitLwrr = pDtcVolt->limitMax;
    }

    return status;
}

/*!
 * DTC_VOLT-class implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
thermPolicyIfaceModel10GetStatus_DTC_VOLT
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_THERM_THERM_POLICY_DTC_VOLT_BOARDOBJ_GET_STATUS *pGetStatus;
    THERM_POLICY_DTC_VOLT                                  *pDtcVolt;
    THERM_POLICY_DTC                                       *pDtc;
    FLCN_STATUS                                             status;

    // Get super class information.
    status = thermPolicyIfaceModel10GetStatus_DOMGRP(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        return status;
    }
    pDtcVolt   = (THERM_POLICY_DTC_VOLT *)pBoardObj;
    pDtc       = &pDtcVolt->dtc;
    pGetStatus = (RM_PMU_THERM_THERM_POLICY_DTC_VOLT_BOARDOBJ_GET_STATUS *)pPayload;

    return thermPolicyDtcQuery(pDtc, &pGetStatus->dtc);
}

/*!
 * @copydoc ThermPolicyLoad
 */
FLCN_STATUS
thermPolicyLoad_DTC_VOLT
(
    THERM_POLICY *pPolicy
)
{
    THERM_POLICY_DTC_VOLT *pDtcVolt = (THERM_POLICY_DTC_VOLT *)pPolicy;
    FLCN_STATUS            status   = FLCN_OK;

    status = thermPolicyLoad_DOMGRP(pPolicy);
    if (status != FLCN_OK)
    {
        return status;
    }

    pDtcVolt->pRatedTdpVfEntry = thermPolicyDomGrpPerfVfEntryGet(
                                    pDtcVolt->ratedTdpVfEntryIdx);

    return FLCN_OK;
}

/*!
 * @copydoc ThermPolicyCallbackExelwte
 */
FLCN_STATUS
thermPolicyCallbackExelwte_DTC_VOLT
(
    THERM_POLICY *pPolicy
)
{
    THERM_POLICY_DTC_VOLT      *pDtcVolt = (THERM_POLICY_DTC_VOLT *)pPolicy;
    THERM_POLICY_DOMGRP        *pDomGrp  = &pDtcVolt->super;
    THERM_POLICY_DTC           *pDtc     = &pDtcVolt->dtc;
    FLCN_STATUS                 status   = FLCN_OK;

    LW2080_CTRL_THERMAL_POLICY_PERF_LIMITS limits;
    LwS32 limitChange;
    LwU32 voltageMultiplier;
    LwU32 newRatedTdpLevel = 0;

    newRatedTdpLevel = s_thermPolicyDtcVoltRatedTdpLevelGet(pDtcVolt);
    // Determine limit changes
    limitChange = thermPolicyDtcGetLevelChange(pDtc,
                                       pDtcVolt->super.super.channelReading);
    if (limitChange != 0)
    {
        if ((limitChange > 0) &&
            ((pDtcVolt->limitLwrr >= (pDtcVolt->limitMax - limitChange)) ||
             (limitChange == THERM_POLICY_DTC_LIMIT_CHANGE_IF_DISENGAGED)))
        {
            // Increasing performance & will exceed the max. perf. level.
            pDtcVolt->limitLwrr = pDtcVolt->limitMax;
        }
        else if ((limitChange < 0) &&
                 (pDtcVolt->limitLwrr <= (LwU32)(-limitChange)))
        {
            // Decreasing performance & will drop below min. perf. level.
            pDtcVolt->limitLwrr = 0;
        }
        else
        {
            // Otherwise, adjust accordingly.
            pDtcVolt->limitLwrr += limitChange;
        }
        if (thermPolicyDomGrpIsRatedTdpLimited(pDomGrp))
        {
            if (pDtcVolt->limitLwrr < newRatedTdpLevel)
            {
                pDtcVolt->limitLwrr = newRatedTdpLevel;
            }
        }
    }

    // Update limits.
    thermPolicyPerfLimitsClear(&limits);

    // If the controller is not at its max level, limits need to be imposed.
    if (pDtcVolt->limitLwrr != pDtcVolt->limitMax)
    {
        // P-state limit
        if (pDtcVolt->limitLwrr < pDtcVolt->limitPivot)
        {
            limits.limit[LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_IDX_PSTATE] =
                pDtcVolt->limitLwrr;
        }

        // Voltage limit
        if (pDtcVolt->limitLwrr <= pDtcVolt->limitPivot)
        {
            voltageMultiplier = pDtcVolt->limitMax - pDtcVolt->limitPivot;
        }
        else
        {
            voltageMultiplier = pDtcVolt->limitMax - pDtcVolt->limitLwrr;
        }
        limits.limit[LW2080_CTRL_THERMAL_POLICY_PERF_LIMIT_IDX_LWVDD] =
            pDtcVolt->voltageMaxuV -
            (voltageMultiplier * pDtcVolt->voltageStepuV);
    }

    // Apply new limits
    status = thermPolicyDomGrpApplyLimits(pDomGrp, &limits);
    return status;
}

/* ---------------------- Private Static Functions -------------------------- */
/*!
 * Function to find the first level in the limit ladder that has a voltage that is
 * equal to or greater than the Rated TDP Voltage. We need this function because the
 * voltage associated with the rated TDP entry can change depending on the ambient
 * temperature etc. We want to make sure that we handle the case where the rated TDP
 * voltage entry has moved "up" (less restrictive level) in the ladder
 *
 * @param[in/out] pDtcVolt
 *     Pointer to THERM_POLICY_DTC_VOLT structure
 *
 * @return i
 *     returns the level corresponding to the first votlage that is equal to or
 *     greater than the ratedTDP voltage
 */
static LwU32
s_thermPolicyDtcVoltRatedTdpLevelGet(THERM_POLICY_DTC_VOLT *pDtcVolt)
{
    LwU32 stepVoltage = 0;
    LwU32 volt        = pDtcVolt->pRatedTdpVfEntry->voltageuV;
    LwU32 i;
    for (i = pDtcVolt->limitPivot; i < pDtcVolt->limitMax; i++)
    {
        stepVoltage = pDtcVolt->voltageMinuV +
                      ((i-pDtcVolt->limitPivot)*pDtcVolt->voltageStepuV);

        if (stepVoltage >= volt)
        {
            break;
        }
    }
    return i;
}

