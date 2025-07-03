/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_hwthreshold.c
 * @brief  Interface specification for a PWR_POLICY_HW_THRESHOLD.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_hwthreshold.h"
#include "pmgr/pwrmonitor.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _HW_THRESHOLD PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_HW_THRESHOLD
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PMGR_PWR_POLICY_HW_THRESHOLD_BOARDOBJ_SET *pHwThresholdSet =
        (RM_PMU_PMGR_PWR_POLICY_HW_THRESHOLD_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY_HW_THRESHOLD                          *pHwThreshold;
    FLCN_STATUS                                       status;
    LwU8                                              limitUnit;

    // Construct and initialize parent-class object.
    status = pwrPolicyGrpIfaceModel10ObjSetImpl_LIMIT(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        return status;
    }
    pHwThreshold = (PWR_POLICY_HW_THRESHOLD *)*ppBoardObj;

    // Copy in the _HW_THRESHOLD type-specific data.
    pHwThreshold->thresholdIdx      = pHwThresholdSet->thresholdIdx;
    pHwThreshold->lowThresholdIdx   = pHwThresholdSet->lowThresholdIdx;
    pHwThreshold->bUseLowThreshold  = pHwThresholdSet->bUseLowThreshold;
    pHwThreshold->lowThresholdValue = pHwThresholdSet->lowThresholdValue;

    // Copy in Power to Current Colwersion data.
    pwrPolicyHWThresPccSet(pHwThreshold, pHwThresholdSet);

    // The Power to Current colwersion should only be used when enabled
    // and if the limit units are correctly in mW
    limitUnit = pwrPolicyLimitUnitGet(pHwThreshold);
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_LWRRENT_COLWERSION) &&
        pHwThresholdSet->pcc.bUseLwrrentColwersion &&
        (limitUnit != LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW))
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * Load a _HW_THRESHOLD PWR_POLICY object.
 *
 * @copydoc PwrPolicyLoad
 */
FLCN_STATUS
pwrPolicyLoad_HW_THRESHOLD
(
    PWR_POLICY *pPolicy,
    LwU32       ticksNow
)
{
    PWR_POLICY_HW_THRESHOLD *pHwThreshold = (PWR_POLICY_HW_THRESHOLD *)pPolicy;
    PWR_CHANNEL             *pChannel     = NULL;
    LwU8                     limitUnit    = pwrPolicyLimitUnitGet(pHwThreshold);
    LwU32                    limitValue   = pwrPolicyLimitLwrrGet(&pHwThreshold->super.super);
    FLCN_STATUS              status;

    status = pwrPolicyLoad_SUPER(pPolicy, ticksNow);
    if (status != FLCN_OK)
    {
        goto pwrPolicyLoad_HW_THRESHOLD_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_LWRRENT_COLWERSION))
    {

        LW2080_CTRL_PMGR_PWR_POLICY_INFO_DATA_HW_THRESHOLD_POWER_TO_LWRR_COLW
           *pPcc = pwrPolicyHWThresPccGet(pHwThreshold);

        if (pPcc->bUseLwrrentColwersion)
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibQuery)
            };

            LW2080_CTRL_PMGR_PWR_TUPLE tuple = { 0 };
            LWU64_TYPE limitVal64;
            LwU64 milliToKiloConst;
            LwU64 pwrKW;
            LwU64 voltuV;

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        
            status = pwrMonitorChannelTupleQuery(Pmgr.pPwrMonitor,
                        pPcc->lwrrentChannelIdx, &tuple);
            if (status != FLCN_OK)
            {
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList); // NJ??
                goto pwrPolicyLoad_HW_THRESHOLD_exit;
            }

            // Colwert limitValue from mW to mA
            milliToKiloConst = 1000000;
            voltuV           = (LwU64)tuple.voltuV;
            limitVal64.val   = (LwU64)limitValue;

            lwU64Mul(&pwrKW, &limitVal64.val, &milliToKiloConst);
            lwU64Div(&limitVal64.val, &pwrKW, &voltuV);

            if (LWU64_HI(limitVal64))
            {
                limitValue = LW_U32_MAX;
            }
            else
            {
                limitValue = LWU64_LO(limitVal64);
            }

            // Change limitUnit from mW to mA
            limitUnit = LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA;

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        }
    }

    // Get the channel for this policy.
    pChannel = PWR_CHANNEL_GET(pPolicy->chIdx);
    if (pChannel == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrPolicyLoad_HW_THRESHOLD_exit;
    }

    // Apply channel limit(s).
    status = pwrChannelLimitSet(pChannel, pHwThreshold->thresholdIdx,
                                limitUnit, limitValue);
    if ((status == FLCN_OK) && pHwThreshold->bUseLowThreshold)
    {
        //
        // Numerical Analysis:-
        // We need to callwlate:
        // limitValue = limitValue * pHwThreshold->lowThresholdValue
        //
        // limitValue: 32.0
        // pHwThreshold->lowThresholdValue: FXP4.12
        //
        // For AD10X_AND_LATER chips:-
        //
        // The multiplication is handled by a PMGR wrapper over
        // multUFXP20_12FailSafe() which does intermediate math safely in
        // 64-bit as follows:-
        //
        // 4.12 is typecast to 20.12
        // 32.0 * 20.12 => 52.12
        // 52.12 >> 12 => 52.0
        //
        // If the upper 32 bits are non-zero (=> 32-bit overflow), it
        // returns LW_U32_MAX, otherwise
        // LWU64_LO(52.0) => 32.0 (the least significant 32 bits are
        //                         returned)
        //
        // Note: For PRE_AD10X chips, there is a possible FXP20.12 overflow
        //       at 1048576 mA/mW.
        //
        limitValue = pmgrMultUFXP20_12OverflowFixes(limitValue,
                         pHwThreshold->lowThresholdValue);

        status = pwrChannelLimitSet(pChannel, pHwThreshold->lowThresholdIdx,
                                    limitUnit, limitValue);
    }

pwrPolicyLoad_HW_THRESHOLD_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
