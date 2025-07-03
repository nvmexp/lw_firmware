/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy.c
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
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy.h"
#include "dbgprintf.h"
#include "pmu_oslayer.h"
#include "task_therm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL)
static LwU32 s_pwrPolicyIntegralControlEvaluate(PWR_POLICY *pPolicy, LwU32 lwrrLimit)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyIntegralControlEvaluate")
    GCC_ATTRIB_NOINLINE();
#endif

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Public function to Evaluate each power policy.
 *
 * @param[in]     pPolicy   PWR_POLICY pointer
 *
 * @return FLCN_OK
 *     Policy Evaluation Successful
 */
FLCN_STATUS
pwrPolicyEvaluate
(
    PWR_POLICY *pPolicy
)
{
    if (pPolicy->status == FLCN_OK)
    {
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL)
        // Cache the current limit from Integral Control evaluation
        pPolicy->limitLwrr = s_pwrPolicyIntegralControlEvaluate(pPolicy,
                                pwrPolicyLimitArbLwrrGet(pPolicy));
#else
        // Cache the current limit from ARBout
        pPolicy->limitLwrr = pwrPolicyLimitArbLwrrGet(pPolicy);
#endif
    }

    return pPolicy->status;
}

/*!
 * Relays the 'construct' request to the appropriate PWR_POLICY implementation.
 * @Copydoc BoardObjGrpIfaceModel10SetEntry
 */
FLCN_STATUS
pmgrPwrPolicyIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BANG_BANG_VF:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_BANG_BANG_VF);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_BANG_BANG_VF(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_BANG_BANG_VF), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH_VF:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MARCH_VF);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_MARCH_VF(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_MARCH_VF), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_PROP_LIMIT);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_PROP_LIMIT(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_PROP_LIMIT), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_TOTAL_GPU);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_TOTAL_GPU(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_TOTAL_GPU), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_WORKLOAD);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_WORKLOAD), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_MULTIRAIL(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_WORKLOAD_MULTIRAIL), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_SINGLE_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_SINGLE_WORKLOAD_1X);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_SINGLE_1X(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_WORKLOAD_SINGLE_1X), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_WORKLOAD_COMBINED_1X(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_WORKLOAD_COMBINED_1X), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_HW_THRESHOLD:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_HW_THRESHOLD);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_HW_THRESHOLD(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_HW_THRESHOLD), pBuf);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BALANCE:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_BALANCE);
            return pwrPolicyGrpIfaceModel10ObjSetImpl_BALANCE(pModel10, ppBoardObj,
                sizeof(PWR_POLICY_BALANCE), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc PwrPolicyLoad
 */
FLCN_STATUS
pwrPolicyLoad
(
    PWR_POLICY *pPolicy,
    LwU32       ticksNow
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_HW_THRESHOLD:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_HW_THRESHOLD);
            return pwrPolicyLoad_HW_THRESHOLD(pPolicy, ticksNow);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD);
            return pwrPolicyLoad_WORKLOAD_MULTIRAIL(pPolicy, ticksNow);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_SINGLE_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_SINGLE_WORKLOAD_1X);
            return pwrPolicyLoad_WORKLOAD_SINGLE_1X(pPolicy, ticksNow);

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X);
            return pwrPolicyLoad_WORKLOAD_COMBINED_1X(pPolicy, ticksNow);
    }

    return pwrPolicyLoad_SUPER(pPolicy, ticksNow);
}

/*!
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_GET_STATUS *pGetStatus;
    PWR_POLICY *pPolicy;
    FLCN_STATUS status = FLCN_OK;

    switch (pBoardObj->type)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BANG_BANG_VF:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_BANG_BANG_VF);
            status = pwrPolicyIfaceModel10GetStatus_BANG_BANG_VF(pModel10, pPayload);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH_VF:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MARCH_VF);
            status = pwrPolicyIfaceModel10GetStatus_MARCH_VF(pModel10, pPayload);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_WORKLOAD);
            status = pwrPolicyIfaceModel10GetStatus_WORKLOAD(pModel10, pPayload);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD);
            status = pwrPolicyIfaceModel10GetStatus_WORKLOAD_MULTIRAIL(pModel10, pPayload);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_SINGLE_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_SINGLE_WORKLOAD_1X);
            status = pwrPolicyIfaceModel10GetStatus_WORKLOAD_SINGLE_1X(pModel10, pPayload);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X);
            status = pwrPolicyIfaceModel10GetStatus_WORKLOAD_COMBINED_1X(pModel10, pPayload);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BALANCE:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_BALANCE);
            status = pwrPolicyIfaceModel10GetStatus_BALANCE(pModel10, pPayload);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_PROP_LIMIT);
            status = pwrPolicyIfaceModel10GetStatus_PROP_LIMIT(pModel10, pPayload);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_TOTAL_GPU);
            status = pwrPolicyIfaceModel10GetStatus_TOTAL_GPU(pModel10, pPayload);
            break;
    }
    if (status != FLCN_OK)
    {
        goto pwrPolicyIfaceModel10GetStatus_done;
    }

    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto pwrPolicyIfaceModel10GetStatus_done;
    }
    pPolicy    = (PWR_POLICY *)pBoardObj;
    pGetStatus = (RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_GET_STATUS *)pPayload;

    pGetStatus->valueLwrr = pPolicy->valueLwrr;
    pGetStatus->limitLwrr = pPolicy->limitLwrr;

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL)
    // Copy out Integral Control status
    pGetStatus->integral.lwrrRunningDiff   = pPolicy->integral.lwrrRunningDiff;
    pGetStatus->integral.lwrrIntegralLimit = pPolicy->integral.lwrrIntegralLimit;
#endif

    //
    // Copy out the limitArbLwrr LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_ARBITRATION
    // structure to the RM.
    //
    pGetStatus->limitArbLwrr.bArbMax   = pPolicy->limitArbLwrr.bArbMax;
    pGetStatus->limitArbLwrr.numInputs = pPolicy->limitArbLwrr.numInputs;
    pGetStatus->limitArbLwrr.output    = pPolicy->limitArbLwrr.output;

    LwU8 idx;

    for (idx = 0; idx < pPolicy->numLimitInputs; idx++)
    {
        pGetStatus->limitArbLwrr.inputs[idx] =
            pPolicy->limitArbLwrr.pInputs[idx];
    }

pwrPolicyIfaceModel10GetStatus_done:
    return status;
}

/*!
 * @copydoc PwrPolicyFilter
 */
FLCN_STATUS
pwrPolicyFilter_SUPER
(
    PWR_POLICY  *pPolicy,
    PWR_MONITOR *pMon
)
{
    FLCN_STATUS  status = FLCN_OK;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_WORKLOAD);
            status = pwrPolicyFilter_WORKLOAD(pPolicy, pMon);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD);
            status = pwrPolicyFilter_WORKLOAD_MULTIRAIL(pPolicy, pMon);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_PROP_LIMIT);
            status = pwrPolicyFilter_PROP_LIMIT(pPolicy, pMon);
            break;

        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_TOTAL_GPU);
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_3X);
            status = pwrPolicyFilter_TOTAL_GPU(pPolicy, pMon);
            break;
    }
    return status;
}

/*!
 * @copydoc PwrPolicyGetPffCachedFrequencyFloor
 */
FLCN_STATUS
pwrPolicyGetPffCachedFrequencyFloor
(
    PWR_POLICY *pPolicy,
    LwU32      *pFreqkHz
)
{
    FLCN_STATUS  status = FLCN_OK;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
            status = pwrPolicyGetPffCachedFrequencyFloor_TOTAL_GPU(pPolicy, pFreqkHz);
            break;
        default:
            status = FLCN_ERR_NOT_SUPPORTED;
            PMU_BREAKPOINT();
            break;
    }

    return status;
}

/*!
 * @copydoc PwrPolicyVfIlwalidate
 */
FLCN_STATUS
pwrPolicyVfIlwalidate
(
    PWR_POLICY *pPolicy
)
{
    FLCN_STATUS  status = FLCN_OK;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
            status = pwrPolicyVfIlwalidate_TOTAL_GPU(pPolicy);
            break;
    }

    return status;
}

/*!
 * @copydoc PwrPolicyValueLwrrGet
 */
LwU32
pwrPolicyValueLwrrGet
(
    PWR_POLICY *pPolicy
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_WORKLOAD);
            return pwrPolicyValueLwrrGet_WORKLOAD(pPolicy);
    }

    return pPolicy->valueLwrr;
}

/*!
 * @copydoc PwrPolicyLimitArbInputGet
 */
LwU32
pwrPolicyLimitArbInputGet
(
    PWR_POLICY *pPolicy,
    LwU8        pwrPolicyIdx
)
{
    LwU32 i;

    //
    // Search through current _LIMIT_INPUTs to get the last limit set by given
    // pwrPolicyIdx.
    //
    for (i = 0; i < pPolicy->limitArbLwrr.numInputs; i++)
    {
        // If found matching index, just update the value.
        if (pPolicy->limitArbLwrr.pInputs[i].pwrPolicyIdx == pwrPolicyIdx)
        {
            return pPolicy->limitArbLwrr.pInputs[i].limitValue;
        }
    }

    return LW_U8_MIN;
}

/*!
 * @copydoc PwrPolicyLimitArbInputSet
 */
FLCN_STATUS
pwrPolicyLimitArbInputSet
(
    PWR_POLICY *pPolicy,
    LwU8        pwrPolicyIdx,
    LwU32       limitValue
)
{
    FLCN_STATUS status   = FLCN_OK;
    LwU32      i;
    LwU32      output;
    LwBool     bFound   = LW_FALSE;

    // Bound limit value to range [limitMin, limitMax]
    limitValue = LW_MIN(limitValue, pPolicy->limitMax);
    limitValue = LW_MAX(limitValue, pPolicy->limitMin);

    // Initialize the minimum value to the current limit input value.
    output = limitValue;

    //
    // Search through current _LIMIT_INPUTs to see if pwrPolicyIdx has already
    // specified a limit.
    //
    for (i = 0; i < pPolicy->limitArbLwrr.numInputs; i++)
    {
        // If found matching index, just update the value.
        if (pPolicy->limitArbLwrr.pInputs[i].pwrPolicyIdx == pwrPolicyIdx)
        {
            pPolicy->limitArbLwrr.pInputs[i].limitValue = limitValue;
            bFound = LW_TRUE;
        }
        // Othwerwise, update the arb output.
        else if (pPolicy->limitArbLwrr.bArbMax)
        {
            output = LW_MAX(output,
                            pPolicy->limitArbLwrr.pInputs[i].limitValue);
        }
        else
        {
            output = LW_MIN(output,
                            pPolicy->limitArbLwrr.pInputs[i].limitValue);
        }
    }

    // If no existing _LIMIT_INPUT for this pwrPolicyIdx, add it.
    if (!bFound)
    {
        // Make sure there is room left in the array.
        if (pPolicy->limitArbLwrr.numInputs <
                pPolicy->numLimitInputs)
        {
            pPolicy->limitArbLwrr.pInputs[
                pPolicy->limitArbLwrr.numInputs].pwrPolicyIdx = pwrPolicyIdx;
            pPolicy->limitArbLwrr.pInputs[
                pPolicy->limitArbLwrr.numInputs].limitValue = limitValue;
            pPolicy->limitArbLwrr.numInputs++;
        }
        // No room, return error.
        else
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
        }
    }

    //
    // If _LIMIT_INPUT successfully changed, apply the arbitration output value
    // (output) to @ref LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_ARBITRATION::output.
    //
    if (status == FLCN_OK)
    {
        pPolicy->limitArbLwrr.output = output;
    }

    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL)
/*!
 * Construct the Integral Control in a Power Policy
 *
 * @param[in]     pPolicy      PWR_POLICY pointer
 * @param[in/out] pDesc
 *     Pointer to RM buffer specifying the power policy object
 * @param[in]     ovlIdxDmem   Overlay in which to allocate Integral Control
 *     samples
 *
 * @return FLCN_ERR_NO_FREE_MEM
 *     Not enough memory for integral control cirlwlar buffer
 * @return FLCN_OK
 *     State succesfully constructed.
 */
FLCN_STATUS
pwrPolicyConstructIntegralControl
(
    PWR_POLICY                            *pPolicy,
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET   *pDesc,
    LwU8                                   ovlIdxDmem
)
{
    FLCN_STATUS status = FLCN_OK;

    // Check if the controller is enabled
    if (pDesc->integral.pastSampleCount == 0)
    {
        goto _pwrPolicyConstructIntegralControl_done;
    }

    //
    // Allocate PWR_POLICY_INTEGRAL if not already allocated
    // or there is a change in the integral control set info.
    //
    if (pPolicy->integral.pData == NULL)
    {
        pPolicy->integral.pData =
            lwosCallocType(ovlIdxDmem, pDesc->integral.pastSampleCount, LwS32);
        if (pPolicy->integral.pData == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            goto _pwrPolicyConstructIntegralControl_done;
        }
        pPolicy->integral.pastSampleCount   = pDesc->integral.pastSampleCount;
    }
    else
    {
        // Dynamically changing the past sample count is not allowed.
        PMU_HALT_COND(pPolicy->integral.pastSampleCount == pDesc->integral.pastSampleCount);
    }
    pPolicy->integral.nextSampleCount = pDesc->integral.nextSampleCount;
    pPolicy->integral.ratioLimitMin   = pDesc->integral.ratioLimitMin;
    pPolicy->integral.ratioLimitMax   = pDesc->integral.ratioLimitMax;

_pwrPolicyConstructIntegralControl_done:
    return status;
}
#endif

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pSet =
        (RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *)pBoardObjDesc;
    PWR_POLICY                          *pPolicy         = NULL;
    FLCN_STATUS                          status          = FLCN_OK;
    LwBool                               bFirstConstruct = (*ppBoardObj == NULL);

    // Sanity check pointer PWR_CHANNEL index.
    if ((Pmgr.pwr.channels.objMask.super.pData[0] & BIT(pSet->chIdx)) == 0)
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    // Call into BOARDOBJ_VTABLE super constructor.
    status = boardObjVtableGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        return status;
    }
    pPolicy = (PWR_POLICY *)*ppBoardObj;

    pPolicy->chIdx          = pSet->chIdx;
    pPolicy->limitUnit      = pSet->limitUnit;
    pPolicy->numLimitInputs = pSet->numLimitInputs;
    pPolicy->limitMin       = pSet->limitMin;
    pPolicy->limitMax       = pSet->limitMax;

    if (bFirstConstruct)
    {
        //
        // Initialize limitArbLwrr structure.
        //
        // Set the _RM _LIMIT_INPUT value to initialize the arbiter with the RM
        // value.  More inputs may be specified at run-time by other PWR_POLICYs.
        //
        pPolicy->limitArbLwrr.numInputs = 0;
        pPolicy->limitArbLwrr.bArbMax   = LW_FALSE;

        // Allocate the LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT array
        pPolicy->limitArbLwrr.pInputs =
            lwosCallocType(pBObjGrp->ovlIdxDmem, pSet->numLimitInputs,
                           LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT);
        if (pPolicy->limitArbLwrr.pInputs == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto pwrPolicyGrpIfaceModel10ObjSetImpl_done;
        }
    }
    status = pwrPolicyLimitArbInputSet(pPolicy,
        LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_RM,
        pSet->limitLwrr);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_done;
    }
    //
    // For bug 1483933, need to cache the initial ARBout limit.
    //
    pPolicy->limitLwrr = pwrPolicyLimitArbLwrrGet(pPolicy);

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL)
    // Construct integral control
    status = pwrPolicyConstructIntegralControl(pPolicy, pSet,
        PWR_POLICY_ALLOCATIONS_DMEM_OVL(pBObjGrp->ovlIdxDmem));
#endif

pwrPolicyGrpIfaceModel10ObjSetImpl_done:
    return status;
}

/*!
 * @copydoc PwrPolicyLoad
 */
FLCN_STATUS
pwrPolicyLoad_SUPER
(
    PWR_POLICY *pPolicy,
    LwU32       ticksNow
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_30))
    {
        return pwrPolicyLoad_SUPER_30(pPolicy, ticksNow);
    }

    return FLCN_OK;
}

/* ------------------------- Private Functions ------------------------------ */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INTEGRAL_CONTROL)
/*!
 * The integral control algorithm in a power policy
 *
 * @param[in]     pPolicy   PWR_POLICY pointer
 *
 */
static LwU32
s_pwrPolicyIntegralControlEvaluate
(
    PWR_POLICY  *pPolicy,
    LwU32        lwrrLimit
)
{
    LwU32 integralLimit;
    LwU8  i;

    // Check if the controller is enabled
    if (pPolicy->integral.pastSampleCount == 0)
    {
        integralLimit  = lwrrLimit;
        goto s_pwrPolicyIntegralControlEvaluate_done;
    }

    // Replace the current sample with the current error
    pPolicy->integral.pData[pPolicy->integral.lwrrSampleIndex] =
                (LwS32)((LwS32)pPolicy->valueLwrr - (LwS32)lwrrLimit);

    // Callwlate the running sum of errors
    pPolicy->integral.lwrrRunningDiff = 0;
    for (i = 0; i<pPolicy->integral.pastSampleCount; i++)
    {
        pPolicy->integral.lwrrRunningDiff += pPolicy->integral.pData[i];
    }

    //
    // The new callwlated integral difference is an adjusted limit
    // from the current policy limit and the average differences
    // over past sample length.
    //
    pPolicy->integral.lwrrIntegralLimit = (LwS32)((LwS32)lwrrLimit -
        ((LwS32)pPolicy->integral.lwrrRunningDiff /
        ((LwS32)pPolicy->integral.nextSampleCount)));

    // Floor the callwlated limit before bounding
    integralLimit =
        (LwU32)LW_MAX(pPolicy->integral.lwrrIntegralLimit, 0);

    //
    // Bound the callwlated limit within ratio max and min.
    //
    // Numerical Analysis:-
    // We need to callwlate:
    // 1. integralLimit = lwrrLimit * pPolicy->integral.ratioLimitMin
    // 2. integralLimit = lwrrLimit * pPolicy->integral.ratioLimitMax
    //
    // lwrrLimit: 32.0
    // pPolicy->integral.ratioLimitMin: FXP4.12
    // pPolicy->integral.ratioLimitMax: FXP4.12
    //
    // For AD10X_AND_LATER chips:-
    //
    // The multiplication is handled by a PMGR wrapper over
    // multUFXP20_12FailSafe() which does intermediate math safely in 64-bit as
    // follows:-
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
    integralLimit = LW_MAX(integralLimit,
                        pmgrMultUFXP20_12OverflowFixes(lwrrLimit,
                            pPolicy->integral.ratioLimitMin));

    integralLimit = LW_MIN(integralLimit,
                        pmgrMultUFXP20_12OverflowFixes(lwrrLimit,
                            pPolicy->integral.ratioLimitMax));

    // Bound further the integral limit between policy max and min limits
    if (PMUCFG_FEATURE_ENABLED(PMU_PWR_POLICY_INTEGRAL_LIMIT_BOUNDS_REQUIRED))
    {
        integralLimit = LW_MAX(integralLimit, pPolicy->limitMin);
        integralLimit = LW_MIN(integralLimit, pPolicy->limitMax);
    }

    // Increment the cirlwlar buffer count
    pPolicy->integral.lwrrSampleIndex++;

    // Reset the cirlwlar buffer count on reaching past sample count
    if (pPolicy->integral.lwrrSampleIndex ==
            pPolicy->integral.pastSampleCount)
    {
        pPolicy->integral.lwrrSampleIndex = 0;
    }

s_pwrPolicyIntegralControlEvaluate_done:
    return integralLimit;
}
#endif
