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
 * @file   pwrpolicy_totalgpu.c
 * @brief  Interface specification for a PWR_POLICY_TOTAL_GPU.
 *
 * The TOTAL_GPU Power Policy distributes a given GPU/board power budget between
 * FB, Core, and static rails.  Total GPU power can be defined as the sum of the
 * FB, Core, and various static rails (e.g. fan, analog rails, etc.):
 *
 *     totalGPU = fb + core + static
 *
 * The static rails cannot be controlled (further, we're only estimating them,
 * not measuring them directly).  FB cannot be controlled in any graceful manner
 * (significant changes to FB power significantly punish user-experience).  So,
 * the TOTAL_GPU Power Policy gives the static and FB rails all the power they
 * need, and assign the rest of the budget to Core.
 *
 *     core = totalGPU - fb - static
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrpolicy_totalgpu.h"
#include "pmgr/pwrpolicyrel.h"
#include "pmgr/pff.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static BoardObjVirtualTableGetInterfaceFromBoardObj (s_pwrPolicyGetInterfaceFromBoardObj_TGP)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "s_pwrPolicyGetInterfaceFromBoardObj_TGP");

/* ------------------------- Global Variables ------------------------------- */
/*!
 * Main structure for all PWR_POLICY_TOTAL_GPU interface data.
 */
BOARDOBJ_VIRTUAL_TABLE pwrPolicyTotalGpulVirtualTable_TGP =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_GET_INTERFACE_FROM_BOARDOBJ(
        s_pwrPolicyGetInterfaceFromBoardObj_TGP)
};

/*!
 * Main structure for all PWR_POLICY_TOTAL_GPU VIRTUAL_TABLE data.
 */
INTERFACE_VIRTUAL_TABLE pwrPolicyTotalGpuVirtualTable =
{
    LW_OFFSETOF(PWR_POLICY_TOTAL_GPU, tgpIface)   // offset
};

/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _TOTAL_GPU PWR_POLICY object.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
pwrPolicyGrpIfaceModel10ObjSetImpl_TOTAL_GPU
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_BOARDOBJ_SET *pTotalGpuSet =
        (RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_BOARDOBJ_SET *)pBoardObjDesc;
    BOARDOBJ_VTABLE                               *pBoardObjVtable;
    PWR_POLICY_TOTAL_GPU                          *pTotalGpu;
    FLCN_STATUS                                    status;

    // Construct and initialize parent-class object.
    status = pwrPolicyGrpIfaceModel10ObjSetImpl_LIMIT(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_TOTAL_GPU_exit;
    }
    pTotalGpu       = (PWR_POLICY_TOTAL_GPU *)*ppBoardObj;
    pBoardObjVtable = (BOARDOBJ_VTABLE *)*ppBoardObj;

    // Copy in the _TOTAL_GPU type-specific data.
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_TGP_MULTI_INFLECTIONS))
    pTotalGpu->limitInflection2 = pTotalGpuSet->limitInflection2;
#endif
    pwrPolicyTotalGpuPffSet(pTotalGpu, pTotalGpuSet->pff);

    // Construct TGP interface class
    status = pwrPolicyConstructImpl_TOTAL_GPU_INTERFACE(pBObjGrp,
                &pTotalGpu->tgpIface.super,
                &pTotalGpuSet->tgpIface.super,
                &pwrPolicyTotalGpuVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyGrpIfaceModel10ObjSetImpl_TOTAL_GPU_exit;
    }

    // Override the Vtable pointer.
   pBoardObjVtable->pVirtualTable = &pwrPolicyTotalGpulVirtualTable_TGP;

pwrPolicyGrpIfaceModel10ObjSetImpl_TOTAL_GPU_exit:
    return status;
}

/*!
 * TOTAL_GPU implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrPolicyIfaceModel10GetStatus_TOTAL_GPU
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_BOARDOBJ_GET_STATUS *pGetStatus  =
        (RM_PMU_PMGR_PWR_POLICY_TOTAL_GPU_BOARDOBJ_GET_STATUS *)pPayload;
    PWR_POLICY_TOTAL_GPU       *pTotalGpu;
    PIECEWISE_FREQUENCY_FLOOR  *pPff;
    FLCN_STATUS                 status;

    pTotalGpu = (PWR_POLICY_TOTAL_GPU *)pBoardObj;
    pPff      = pwrPolicyTotalGpuPffGet(pTotalGpu);

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PIECEWISE_FREQUENCY_FLOOR) &&
        (pPff != NULL))
    {
        pGetStatus->pff = pPff->status;
    }

    // Query TOTAL_GPU_INTERFACE interface
    status = pwrPolicyGetStatus_TOTAL_GPU_INTERFACE(&pTotalGpu->tgpIface,
                &pGetStatus->tgpIface);
    if (status != FLCN_OK)
    {
        return status;
    }

    return FLCN_OK;
}

/*!
 * @copydoc PwrPolicyLimitEvaluate
 */
FLCN_STATUS
pwrPolicyLimitEvaluate_TOTAL_GPU
(
    PWR_POLICY_LIMIT *pLimit
)
{
    PWR_POLICY_TOTAL_GPU            *pTotalGpu  = (PWR_POLICY_TOTAL_GPU *)pLimit;
    PWR_POLICY_TOTAL_GPU_INTERFACE  *pTgpIface;
    PWR_POLICY_RELATIONSHIP         *pPolicyRel;
    PERF_DOMAIN_GROUP_LIMITS         domGrpCeiling;
    LwU32                            value;
    LwU32                            limit;
    FLCN_STATUS                      status     = FLCN_OK;

    pTgpIface = PWR_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pTotalGpu, TOTAL_GPU_INTERFACE);
    if (pTgpIface == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrPolicyLimitEvaluate_TOTAL_GPU_exit;
    }

    // The current limit value for the entire GPU.
    limit = pwrPolicyLimitLwrrGet(&pTotalGpu->super.super);

    // First, evaluate the policy's pff lwrve, if applicable
    if (pwrPolicyTotalGpuPffGet(pTotalGpu) != NULL)
    {
        status = pffEvaluate(pwrPolicyTotalGpuPffGet(pTotalGpu),
                             pwrPolicyValueLwrrGet(&pTotalGpu->super.super),
                             pwrPolicyLimitArbInputGet(&pTotalGpu->super.super,
                                LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_RM));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrPolicyLimitEvaluate_TOTAL_GPU_exit;
        }
    }

    //
    // Second, do evaluation of limitInflection to consider if a global Domain
    // Group ceiling should be applied to lock the GPU down to the inflection
    // vpstate.
    // The current limit can swing because of integral controller and the inflection
    // limits are set w.r.t the pre-integral limits. So use ArbLwrr instead.
    // Limit inflection 0 > limit inflection 1 > limit inflection 2
    //
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_TGP_MULTI_INFLECTIONS))
    if (pwrPolicyLimitArbLwrrGet(&pTotalGpu->super.super) < pTotalGpu->limitInflection2)
    {
        status = pwrPolicyGetDomGrpLimitsByVpstateIdx(RM_PMU_PERF_VPSTATE_IDX_INFLECTION2,
            &domGrpCeiling);
    }
    else if (pwrPolicyLimitArbLwrrGet(&pTotalGpu->super.super) < pTgpIface->limitInflection1)
    {
        status = pwrPolicyGetDomGrpLimitsByVpstateIdx(RM_PMU_PERF_VPSTATE_IDX_INFLECTION1,
            &domGrpCeiling);
    }
    else
#endif
    if (pwrPolicyLimitArbLwrrGet(&pTotalGpu->super.super) < pTgpIface->limitInflection0)
    {
        status = pwrPolicyGetDomGrpLimitsByVpstateIdx(RM_PMU_PERF_VPSTATE_IDX_INFLECTION0,
            &domGrpCeiling);
    }
    else
    {
        status = perfPstatePackPstateCeilingToDomGrpLimits(&domGrpCeiling);
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyLimitEvaluate_TOTAL_GPU_exit;
    }

    pwrPolicyDomGrpGlobalCeilingRequest(BOARDOBJ_GET_GRP_IDX_8BIT(&pTotalGpu->super.super.super.super),
        &domGrpCeiling);

    //
    // Then, do actual evaluation of the _TOTAL_GPU controller:
    // 1. Subtract the static value allotment out of the current limit,
    // flooring the limit to 0 if it is less than the static value
    //
    if (pTgpIface->bUseChannelIdxForStatic)
    {
        limit = ((limit > pTgpIface->lastStaticPower) ?
                    (limit - pTgpIface->lastStaticPower) : 0);
    }
    else
    {
        limit = ((limit > pTgpIface->staticVal.fixed) ?
                    (limit - pTgpIface->staticVal.fixed) : 0);
    }

    //
    // 2. Subtract out the FB value from the limit.  Will never punish FB to
    // enforce the GPU limit.
    //
    pPolicyRel = PWR_POLICY_REL_GET(pTgpIface->fbPolicyRelIdx);
    if (pPolicyRel == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrPolicyLimitEvaluate_TOTAL_GPU_exit;
    }
    value = pwrPolicyRelationshipValueLwrrGet(pPolicyRel);
    if (value > limit)
    {
        limit = 0;
    }
    else
    {
        limit -= value;
    }

    // 3. Apply the remaining limit to Core.
    pPolicyRel = PWR_POLICY_REL_GET(pTgpIface->corePolicyRelIdx);
    if (pPolicyRel == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrPolicyLimitEvaluate_TOTAL_GPU_exit;
    }
    status = pwrPolicyRelationshipLimitInputSet(pPolicyRel,
                BOARDOBJ_GET_GRP_IDX_8BIT(&pTotalGpu->super.super.super.super), LW_FALSE, &limit);

pwrPolicyLimitEvaluate_TOTAL_GPU_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyFilter
 */
FLCN_STATUS
pwrPolicyFilter_TOTAL_GPU
(
    PWR_POLICY     *pPolicy,
    PWR_MONITOR    *pMon
)
{
    PWR_POLICY_TOTAL_GPU_INTERFACE *pTgpIface;
    FLCN_STATUS                     status = FLCN_OK;
    LW2080_CTRL_PMGR_PWR_TUPLE      tuple;

    pTgpIface = PWR_POLICY_BOARDOBJ_TO_INTERFACE_CAST(pPolicy, TOTAL_GPU_INTERFACE);
    if (pTgpIface == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto pwrPolicyFilter_TOTAL_GPU_exit;
    }

    // Return early if this field is FALSE. We don't need to filter.
    if (!pTgpIface->bUseChannelIdxForStatic)
    {
        status = FLCN_OK;
        goto pwrPolicyFilter_TOTAL_GPU_exit;
    }

    // use the power monitor function to retrieve latest static rail power.
    status = pwrMonitorChannelTupleQuery(Pmgr.pPwrMonitor,
            pTgpIface->staticVal.pwrChannelIdx, &tuple);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyFilter_TOTAL_GPU_exit;
    }

    pTgpIface->lastStaticPower = tuple.pwrmW;

pwrPolicyFilter_TOTAL_GPU_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyGetPffLwrveIntersect
 */
FLCN_STATUS
pwrPolicyGetPffCachedFrequencyFloor_TOTAL_GPU
(
    PWR_POLICY *pPolicy,
    LwU32      *pFreqkHz
)
{
    PIECEWISE_FREQUENCY_FLOOR  *pPff    =
        pwrPolicyTotalGpuPffGet((PWR_POLICY_TOTAL_GPU *)pPolicy);
    FLCN_STATUS                 status  = FLCN_OK;

    if (pPff == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto pwrPolicyGetPffCachedFrequencyFloor_TOTAL_GPU_exit;
    }

    *pFreqkHz = pffGetCachedFrequencyFloor(pPff);

pwrPolicyGetPffCachedFrequencyFloor_TOTAL_GPU_exit:
    return status;
}

/*!
 * @copydoc PwrPolicyVfIlwalidate
 */
FLCN_STATUS
pwrPolicyVfIlwalidate_TOTAL_GPU
(
    PWR_POLICY *pPolicy
)
{
    PWR_POLICY_TOTAL_GPU       *pTotalGpu   = (PWR_POLICY_TOTAL_GPU *)pPolicy;
    PIECEWISE_FREQUENCY_FLOOR  *pPff        =
        pwrPolicyTotalGpuPffGet((PWR_POLICY_TOTAL_GPU *)pPolicy);
    FLCN_STATUS                 status      = FLCN_OK;

    if (pPff == NULL)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto pwrPolicyVfIlwalidate_TOTAL_GPU_exit;
    }

    status = pffSanityCheck(pwrPolicyTotalGpuPffGet(pTotalGpu),
                            pwrPolicyLimitArbInputGet(&pTotalGpu->super.super,
                                LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_RM));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyVfIlwalidate_TOTAL_GPU_exit;
    }

    status = pffIlwalidate(pPff,
                           pwrPolicyValueLwrrGet(&pTotalGpu->super.super),
                           pwrPolicyLimitArbInputGet(&pTotalGpu->super.super,
                                LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_INPUT_CLIENT_IDX_RM));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyVfIlwalidate_TOTAL_GPU_exit;
    }

pwrPolicyVfIlwalidate_TOTAL_GPU_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc BoardObjVirtualTableGetInterfaceFromBoardObj
 */
static BOARDOBJ_INTERFACE *
s_pwrPolicyGetInterfaceFromBoardObj_TGP
(
    BOARDOBJ   *pBoardObj,
    LwU8        interfaceType
)
{
    PWR_POLICY_TOTAL_GPU *pTotalGpu = (PWR_POLICY_TOTAL_GPU *)pBoardObj;

    switch (interfaceType)
    {
        case LW2080_CTRL_PMGR_PWR_POLICY_INTERFACE_TYPE_TOTAL_GPU_INTERFACE:
        {
            return &pTotalGpu->tgpIface.super;
        }
    }

    PMU_BREAKPOINT();
    return NULL;
}
