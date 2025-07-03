/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_3x.c
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
#include "pmgr/3x/pwrpolicy_3x.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
static FLCN_STATUS  s_pwrPolicy3XConstructInputFilter_BLOCK(PWR_POLICY *pPolicy, PWR_POLICY_3X_FILTER *pFilter, RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pBoardObjSet, LwU8 ovlIdxDmem)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "s_pwrPolicy3XConstructInputFilter_BLOCK");
static FLCN_STATUS  s_pwrPolicy3XConstructInputFilter_MOVING(PWR_POLICY *pPolicy, PWR_POLICY_3X_FILTER *pFilter, RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pBoardObjSet, LwU8 ovlIdxDmem)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "s_pwrPolicy3XConstructInputFilter_MOVING");
static FLCN_STATUS  s_pwrPolicy3XConstructInputFilter_IIR(PWR_POLICY *pPolicy, PWR_POLICY_3X_FILTER *pFilter, RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pBoardObjSet, LwU8 ovlIdxDmem)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "s_pwrPolicy3XConstructInputFilter_IIR");
static FLCN_STATUS  s_pwrPolicy3XInputFilter_BLOCK(PWR_POLICY_3X_FILTER *pFilter, LwU32 input, LwU32 *pOutput)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicy3XInputFilter_BLOCK");
static FLCN_STATUS  s_pwrPolicy3XInputFilter_MOVING(PWR_POLICY_3X_FILTER *pFilter, LwU32 input, LwU32 *pOutput)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicy3XInputFilter_MOVING");
static FLCN_STATUS  s_pwrPolicy3XInputFilter_IIR(PWR_POLICY_3X_FILTER *pFilter, LwU32 input, LwU32 *pOutput)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicy3XInputFilter_IIR");

/*!
 * Super-class implementation.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_3X
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    PWR_POLICY         *pPolicy;
    LwBool              bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS         status;

    RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pSet =
        (RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *)pBoardObjDesc;

    //
    // TODO-aherring: To avoid issues with conditional compilation that makes
    // this variable unused. Can be removed if conditional compilation changed
    //
    (void)pBObjGrp;

    status = pwrPolicyGrpIfaceModel10ObjSetImpl(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_3X_done;
    }

    // Set 3X specific fields.
    if (Pmgr.pPwrPolicies->version ==
        RM_PMU_PMGR_PWR_POLICY_DESC_TABLE_VERSION_3X)
    {
        pPolicy = (PWR_POLICY *)*ppBoardObj;

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INPUT_FILTERING))
        {
            PWR_POLICY_3X_FILTER *pFilter = &pPolicy->filter;

            status = pwrPolicy3XConstructInputFilter(pPolicy, pFilter, pSet,
                PWR_POLICY_ALLOCATIONS_DMEM_OVL(pBObjGrp->ovlIdxDmem));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_3X_done;
            }
        }

        if (!bFirstConstruct)
        {
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
            // We cannot allow subsequent SET calls to change these parameters.
            if ((pPolicy->sampleMult != pSet->sampleMult))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_3X_done;
            }
        }
        pPolicy->sampleMult = pSet->sampleMult;

        // Validate member variables.
        if (pPolicy->sampleMult == 0)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_3X_done;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
        }

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_30)
        //
        // Derive _30 specific fields.
        //
        // It is important to divide base period [ns] with the time of OS tick
        // before we apply sampleMult to assure that resulting period in ticks
        // is proportional to the sampleMult without rounding error.
        //
        pPolicy->ticksPeriod =
            (((PWR_POLICIES_3X *)Pmgr.pPwrPolicies)->baseSamplePeriodms *
             1000 / LWRTOS_TICK_PERIOD_US) *
            pSet->sampleMult;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_30)
    }

pwrPolicyGrpIfaceModel10ObjSetImpl_SUPER_3X_done:
    return status;
}

/*!
 * @copydoc PwrPolicy3XChannelMaskGet
 */
LwU32
pwrPolicy3XChannelMaskGet
(
    PWR_POLICY *pPolicy
)
{
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD);
            {
                PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface =
                    PWR_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, WORKLOAD_MULTIRAIL_INTERFACE);
                if (pWorkIface == NULL)
                {
                    PMU_BREAKPOINT();
                    // TODO: propagate
                }
                else
                {
                    return pwrPolicy3XChannelMaskGet_WORKLOAD_MULTIRAIL_INTERFACE(pWorkIface);
                }
                break;
            }
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X);
            return pwrPolicy3XChannelMaskGet_WORKLOAD_COMBINED_1X(pPolicy);
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BALANCE:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_BALANCE);
            return pwrPolicy3XChannelMaskGet_BALANCE(pPolicy);
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_TOTAL_GPU);
            {
                PWR_POLICY_TOTAL_GPU_INTERFACE *pTgpIface =
                    PWR_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, TOTAL_GPU_INTERFACE);
                if (pTgpIface == NULL)
                {
                    PMU_BREAKPOINT();
                    // TODO: Propagate
                }
                else
                {
                    return pwrPolicy3XChannelMaskGet_TOTAL_GPU_INTERFACE(pTgpIface);
                }
                break;
            }
    }

    return BIT(pPolicy->chIdx);
}

/*!
 * @copydoc PwrPolicy3XFilter
 */
FLCN_STATUS
pwrPolicy3XFilter_SUPER
(
    PWR_POLICY     *pPolicy,
    PWR_MONITOR    *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload
)
{
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple;
    FLCN_STATUS status = FLCN_OK;
    LwU32       valueLwrr;

    // Fetch the reading depending on current or power based policy type.
    pTuple = pwrPolicy3xFilterPayloadTupleGet(pPayload, pPolicy->chIdx);
    switch (pPolicy->limitUnit)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW:
            valueLwrr = pTuple->pwrmW;
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA:
            valueLwrr = pTuple->lwrrmA;
            break;
        default:
            PMU_HALT();
            return FLCN_ERR_NOT_SUPPORTED;
    }

    // If input filtering is supported apply the filter and update valueLwrr.
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INPUT_FILTERING))
    {
        PWR_POLICY_3X_FILTER *pFilter      = &pPolicy->filter;
        LwU32                 filterOutput = valueLwrr;

        status = pwrPolicy3XInputFilter(pFilter, valueLwrr, &filterOutput);
        if (status == FLCN_OK)
        {
            pPolicy->valueLwrr = filterOutput;
        }
    }
    else
    {
        pPolicy->valueLwrr = valueLwrr;
    }

    return status;
}

/*
 * @copydoc PwrPolicy3XFilter
 */
FLCN_STATUS
pwrPolicy3XFilter
(
    PWR_POLICY     *pPolicy,
    PWR_MONITOR    *pMon,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload
)
{
    FLCN_STATUS status = FLCN_OK;

    // Call into any class-specific implementations.
    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_WORKLOAD);
            status = pwrPolicy3XFilter_WORKLOAD(pPolicy, pMon, pPayload);
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD);
            status = pwrPolicy3XFilter_WORKLOAD_MULTIRAIL(pPolicy,
                        pMon, pPayload);
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_SINGLE_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_SINGLE_WORKLOAD_1X);
            status = pwrPolicy3XFilter_WORKLOAD_SINGLE_1X(pPolicy,
                        pMon, pPayload);
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_COMBINED_WORKLOAD_1X);
            status = pwrPolicy3XFilter_WORKLOAD_COMBINED_1X(pPolicy,
                        pMon, pPayload);
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BALANCE:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_BALANCE);
            status = pwrPolicy3XFilter_BALANCE(pPolicy, pMon, pPayload);
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_PROP_LIMIT);
            status = pwrPolicy3XFilter_PROP_LIMIT(pPolicy, pMon, pPayload);
            break;
        case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_TOTAL_GPU:
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PMGR_PWR_POLICY_TOTAL_GPU);
            {
                PWR_POLICY_TOTAL_GPU_INTERFACE *pTgpIface =
                    PWR_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, TOTAL_GPU_INTERFACE);
                if (pTgpIface == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    goto pwrPolicy3XFilter_exit;
                }
                else
                {
                    status = pwrPolicy3XFilter_TOTAL_GPU_INTERFACE(pTgpIface, pMon, pPayload);
                }
                break;
            }
        default:
            status = pwrPolicy3XFilter_SUPER(pPolicy, pMon, pPayload);
            break;
    }

pwrPolicy3XFilter_exit:
    pPolicy->status = status;
    return status;
}

/*
 * @copydoc PwrPolicy3XConstructInputFilter
 */
FLCN_STATUS
pwrPolicy3XConstructInputFilter
(
    PWR_POLICY                          *pPolicy,
    PWR_POLICY_3X_FILTER                *pFilter,
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pBoardObjSet,
    LwU8                                 ovlIdxDmem
)
{
    switch (pBoardObjSet->filterInfo.type)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_BLOCK:
            pFilter->type =
                (LwU8)LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_BLOCK;
            return s_pwrPolicy3XConstructInputFilter_BLOCK(pPolicy, pFilter,
                        pBoardObjSet, ovlIdxDmem);
        case LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_MOVING:
            pFilter->type =
                (LwU8) LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_MOVING;
            return s_pwrPolicy3XConstructInputFilter_MOVING(pPolicy, pFilter,
                        pBoardObjSet, ovlIdxDmem);
        case LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_IIR:
            pFilter->type =
                (LwU8) LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_IIR;
            return s_pwrPolicy3XConstructInputFilter_IIR(pPolicy, pFilter,
                        pBoardObjSet, ovlIdxDmem);
        case LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_NONE:
            //
            // type _NONE
            // nothing to do return the allocation would zero out the data
            //
            return FLCN_OK;
        default:
            PMU_HALT();
            return FLCN_ERR_NOT_SUPPORTED;
    }
}

/*
 * @copydoc PwrPolicy3XInputFilter
 */
FLCN_STATUS
pwrPolicy3XInputFilter
(
    PWR_POLICY_3X_FILTER *pFilter,
    LwU32                 input,
    LwU32                *pOutput
)
{
    switch (pFilter->type)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_BLOCK:
            return s_pwrPolicy3XInputFilter_BLOCK(pFilter, input, pOutput);
        case LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_MOVING:
            return s_pwrPolicy3XInputFilter_MOVING(pFilter, input, pOutput);
        case LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_IIR:
            return s_pwrPolicy3XInputFilter_IIR(pFilter, input, pOutput);
        case LW2080_CTRL_PMGR_PWR_POLICY_3X_FILTER_TYPE_NONE:
            return FLCN_OK;
        default:
            PMU_HALT();
            return FLCN_ERR_NOT_SUPPORTED;
    }
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Constructs input filter of a block average type.
 *
 * @param[in/out]  pPolicy      PWR_POLICY pointer.
 * @param[in/out]  pFilter      PWR_POLICY_3X_FILTER pointer.
 * @param[in]      pBoardObjSet RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET pointer.
 * @param[in]      ovlIdxDmem   Overlay in which to allocate filter data.
 *
 * @return FLCN_OK
 *     On success.
 * @return FLCN_ERR_NO_FREE_MEM
 *     Insufficient memory.
 */
static FLCN_STATUS
s_pwrPolicy3XConstructInputFilter_BLOCK
(
    PWR_POLICY                          *pPolicy,
    PWR_POLICY_3X_FILTER                *pFilter,
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pBoardObjSet,
    LwU8                                 ovlIdxDmem
)
{
    PWR_POLICY_3X_FILTER_BLOCK *pBlock =
        (PWR_POLICY_3X_FILTER_BLOCK *)&pFilter->block;
    pBlock->count     = 0;
    pBlock->blockSize = pBoardObjSet->filterInfo.params.block.blockSize;

    // Check to see if we need to build a filter
    if ((pBlock->blockSize != 0) && (pBlock->pFilterData == NULL))
    {
        pBlock->pFilterData =
            lwosCallocType(ovlIdxDmem, pBlock->blockSize, LwU32);
        if (pBlock->pFilterData == NULL)
        {
            return FLCN_ERR_NO_FREE_MEM;
        }
    }

    return FLCN_OK;
}

/*!
 * Constructs input filter of a moving average type.
 *
 * @param[in/out]  pPolicy      PWR_POLICY pointer.
 * @param[in/out]  pFilter      PWR_POLICY_3X_FILTER pointer.
 * @param[in]      pBoardObjSet RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET pointer.
 * @param[in]      ovlIdxDmem   Overlay in which to allocate filter data.
 *
 * @return FLCN_OK
 *     On success.
 * @return FLCN_ERR_NO_FREE_MEM
 *     Insufficient memory.
 */
static FLCN_STATUS
s_pwrPolicy3XConstructInputFilter_MOVING
(
    PWR_POLICY                          *pPolicy,
    PWR_POLICY_3X_FILTER                *pFilter,
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pBoardObjSet,
    LwU8                                 ovlIdxDmem
)
{
    PWR_POLICY_3X_FILTER_MOVING *pMoving =
        (PWR_POLICY_3X_FILTER_MOVING *)&pFilter->moving;
    pMoving->count       = 0;
    pMoving->bWindowFull = LW_FALSE;
    pMoving->windowSize  = pBoardObjSet->filterInfo.params.moving.windowSize;

    // Check to see if we need to build a filter
    if ((pMoving->windowSize != 0) && (pMoving->pFilterData == NULL))
    {
        pMoving->pFilterData =
            lwosCallocType(ovlIdxDmem, pMoving->windowSize, LwU32);
        if (pMoving->pFilterData == NULL)
        {
            return FLCN_ERR_NO_FREE_MEM;
        }
    }

    return FLCN_OK;
}

/*!
 * Constructs input filter of a IIR type.
 *
 * @param[in/out]  pPolicy      PWR_POLICY pointer.
 * @param[in/out]  pFilter      PWR_POLICY_3X_FILTER pointer.
 * @param[in]      pBoardObjSet RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET pointer.
 * @param[in]      ovlIdxDmem   Overlay in which to allocate filter data.
 *
 * @return FLCN_OK
 *     On success.
 */
static FLCN_STATUS
s_pwrPolicy3XConstructInputFilter_IIR
(
    PWR_POLICY                          *pPolicy,
    PWR_POLICY_3X_FILTER                *pFilter,
    RM_PMU_PMGR_PWR_POLICY_BOARDOBJ_SET *pBoardObjSet,
    LwU8                                 ovlIdxDmem
)
{
    PWR_POLICY_3X_FILTER_IIR *pIir =
        (PWR_POLICY_3X_FILTER_IIR *)&pFilter->iir;
    pIir->lastSample = 0;
    pIir->divisor    = pBoardObjSet->filterInfo.params.iir.divisor;

    return FLCN_OK;
}

/*!
 * Evaluate input filter of a Block Average type.
 *
 * @param[in]   pFilter     PWR_POLICY_3X_FILTER pointer
 * @param[in]   input       Input value to feed into filter
 * @param[out]  pOutput     Pointer to output buffer which should be written with filter
 *     output when filtering is complete
 *
 * @return FLCN_OK
 *     On success.
 */
static FLCN_STATUS
s_pwrPolicy3XInputFilter_BLOCK
(
    PWR_POLICY_3X_FILTER *pFilter,
    LwU32                 input,
    LwU32                *pOutput
)
{
    PWR_POLICY_3X_FILTER_BLOCK *pBlock =
        (PWR_POLICY_3X_FILTER_BLOCK *)&pFilter->block;
    LwU32  sumVal = 0;
    LwU32  i;


    if ((pBlock->pFilterData == NULL) ||
        (pBlock->blockSize == 0))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pBlock->pFilterData[pBlock->count] = input;
    pBlock->count++;

    // Reset the counter on reaching window size
    if (pBlock->count == pBlock->blockSize)
    {
        pBlock->count = 0;
        // Sum all the samples and average them against the window size
        for (i = 0; i < pBlock->blockSize; i++)
        {
            sumVal += pBlock->pFilterData[i];
        }
        *pOutput = LW_UNSIGNED_ROUNDED_DIV(sumVal, pBlock->blockSize);
        return FLCN_OK;
    }

    // Don't evaluate this sample
    return FLCN_ERR_MORE_PROCESSING_REQUIRED;
}

/*!
 * Evaluate input filter of a Moving average type.
 *
 * @param[in]   pFilter     PWR_POLICY_3X_FILTER pointer
 * @param[in]   input       Input value to feed into filter
 * @param[out]  pOutput     Pointer to output buffer which should be written with filter
 *     output when filtering is complete
 *
 * @return FLCN_OK
 *     On success.
 */
static FLCN_STATUS
s_pwrPolicy3XInputFilter_MOVING
(
    PWR_POLICY_3X_FILTER *pFilter,
    LwU32                 input,
    LwU32                *pOutput
)
{
    PWR_POLICY_3X_FILTER_MOVING *pMoving =
        (PWR_POLICY_3X_FILTER_MOVING *)&pFilter->moving;
    LwU8        sampleSize;
    LwU32       sumVal = 0;
    LwU32       i;
    FLCN_STATUS status = FLCN_OK;

    // Sanity check the filter parameters
    if ((pMoving->pFilterData == NULL) ||
        (pMoving->windowSize == 0))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto s_pwrPolicy3XInputFilter_MOVING_exit;
    }

    pMoving->pFilterData[pMoving->count] = input;
    pMoving->count++;

    // If window was not yet full, check if it is full now.
    if (!pMoving->bWindowFull &&
        (pMoving->count == pMoving->windowSize))
    {
        pMoving->bWindowFull = LW_TRUE;
    }

    // Reset count on reaching window size
    pMoving->count = pMoving->count % pMoving->windowSize;

    // sampleSize represents the number of filterData elements populated.
    sampleSize = pMoving->bWindowFull ? pMoving->windowSize : pMoving->count;

    // Sum all the samples and average them against the sampleSize
    for (i = 0; i < sampleSize; i++)
    {
        sumVal += pMoving->pFilterData[i];
    }
    *pOutput = LW_UNSIGNED_ROUNDED_DIV(sumVal, sampleSize);

s_pwrPolicy3XInputFilter_MOVING_exit:
    return status;
}

/*!
 * Evaluate input filter of a IIR type.
 *
 * @param[in]   pFilter     PWR_POLICY_3X_FILTER pointer
 * @param[in]   input       Input value to feed into filter
 * @param[out]  pOutput     Pointer to output buffer which should be written with filter
 *     output when filtering is complete
 *
 * @return FLCN_OK
 *     On success.
 */
static FLCN_STATUS
s_pwrPolicy3XInputFilter_IIR
(
    PWR_POLICY_3X_FILTER *pFilter,
    LwU32                 input,
    LwU32                *pOutput
)
{
    PWR_POLICY_3X_FILTER_IIR *pIir =
        (PWR_POLICY_3X_FILTER_IIR *)&pFilter->iir;
    LwUFXP52_12 prevSample = (LwUFXP52_12)pIir->lastSample;
    LwUFXP52_12 lwrrSample;
    LwUFXP52_12 tempSample;
    LwU64       tempDiv;

    PMU_HALT_COND(pIir->divisor != 0);

    //
    // Filter output = ((lastSample * (divisor - 1)) + (lwrrSample)) / divisor
    // tempDiv is 64.0
    //
    tempDiv = (LwU64)pIir->divisor;

    // lwrrSample is 52.12
    lwrrSample = (LwU64)input << 12;
    tempDiv = tempDiv - 1;

    // tempSample = prevSample * tempDiv = 52.12 * 64.0 = 52.12
    lwU64Mul(&tempSample, &prevSample, &tempDiv);
    prevSample = tempSample;

    // tempSample = prevSample + lwrrSample = 52.12 + 52.12 = 52.12
    lw64Add(&tempSample, &prevSample, &lwrrSample);
    tempDiv = (LwU64)pIir->divisor;
    if (tempSample != 0)
    {
        // prevSample = tempSample / tempDiv = 52.12 / 64.0 = 52.12
        lwU64Div(&prevSample, &tempSample, &tempDiv);
    }

    // 52.12 to 20.12 colwersion. In case of overflow return LW_U32_MAX
    pIir->lastSample =
        (LwU64_HI32(prevSample) != 0) ? LW_U32_MAX : LwU64_LO32(prevSample);

    // Colwert to LwU32 before copying out to pOutput
    *pOutput = LW_TYPES_UFXP_X_Y_TO_U32(20,12, pIir->lastSample);

    return FLCN_OK;
}
