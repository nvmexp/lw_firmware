/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_35.c
 * @brief  Interface specification for a PWR_POLICY.
 *
 * A "Power Policy" is a logical construct representing a behavior (a limit
 * value) to enforce on a power rail, as monitored by a Power Channel.  This
 * limit is enforced via a mechanism specified in the Power Policy
 * implementation.
 *
 * PWR_POLICY is a virtual class/interface.  It alone does not specify a full
 * power policy/mechanism.  Classes which implement PWR_POLICY specify a full
 * mechanism for control.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objpmgr.h"
#include "pmu_objperf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief   Set multiplier data for policy.
 *
 * @param[in]   pPolicy    PWR_POLICY object pointer.
 * @param[in]   pMultData  PWR_POLICY_MULTIPLIER_DATA object pointer.
 *
 * @return  FLCN_OK     Multiplier data set successfully
 * @return  other       Propagates return values from various calls
 */
FLCN_STATUS
pwrPolicy35SetMultData
(
    PWR_POLICY                 *pPolicy,
    PWR_POLICY_MULTIPLIER_DATA *pMultData
)
{
    LwU32 channelMask;
    LwU32 idx;
    BOARDOBJGRPMASK_E32 perfCfControllerMask;
    FLCN_STATUS         status = FLCN_OK;

    // Let policy know what Multiplier data structure to use.
    pPolicy->pMultData = pMultData;

    // Let Multiplier data know what object(s) belong to it.
    boardObjGrpMaskBitSet(&(pMultData->maskPolicies), BOARDOBJ_GET_GRP_IDX(&pPolicy->super.super));

    // Set the channel the policy needs.
    channelMask = pwrPolicy3XChannelMaskGet(pPolicy);
    FOR_EACH_INDEX_IN_MASK(32, idx, channelMask)
    {
        boardObjGrpMaskBitSet(&(pMultData->channelsStatus.mask), idx);
    }
    FOR_EACH_INDEX_IN_MASK_END

    // Initialise the PERF_CF controllers mask needed to be polled on
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35_FILTER_PAYLOAD_PERF_CF_CONTROLLERS_REDUCED_STATUS))
    {
        boardObjGrpMaskInit_E32(&perfCfControllerMask);

        PMU_ASSERT_OK_OR_GOTO(status,
            pwrPolicy35PerfCfControllerMaskGet(pPolicy, &perfCfControllerMask),
            pwrPolicy35SetMultData_exit);

        PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpMaskOr(
            &(pwrPolicy35FilterPayloadPerfCfControllersStatusGet(pMultData)->mask),
            &(pwrPolicy35FilterPayloadPerfCfControllersStatusGet(pMultData)->mask),
            &perfCfControllerMask),
            pwrPolicy35SetMultData_exit);
    }

    // Skip the first policy cycle because channel status readings will not be valid.
    pPolicy->bReset = LW_TRUE;
pwrPolicy35SetMultData_exit:
    return status;
}

/*!
 * @copydoc  PwrPolicy35PerfCfControllerMaskGet
 */
FLCN_STATUS
pwrPolicy35PerfCfControllerMaskGet
(
    PWR_POLICY          *pPolicy,
    BOARDOBJGRPMASK_E32 *pPerfCfControllerMask
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPerfCfControllerMask != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPolicy35PerfCfControllerMaskGet_exit);

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X);
            PMU_ASSERT_OK_OR_GOTO(status,
                pwrPolicy35PerfCfControllerMaskGet_WORKLOAD_COMBINED_1X(
                    pPolicy, pPerfCfControllerMask),
                pwrPolicy35PerfCfControllerMaskGet_exit);
            break;
        default:
            status = FLCN_OK;
            break;
    }

pwrPolicy35PerfCfControllerMaskGet_exit:
    return status;
}

