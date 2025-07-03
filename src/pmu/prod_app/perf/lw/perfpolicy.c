/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perfpolicy.c
 * @copydoc perfpolicy.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "dmemovl.h"
#include "pmu_objperf.h"
#include "perf/perfpolicy.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

#include "g_pmurpc.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader (s_perfPolicyIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "s_perfPolicyIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry  (s_perfPolicyIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "s_perfPolicyIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10GetStatusHeader (s_perfPolicyIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "s_perfPolicyIfaceModel10GetStatusHeader");
static BoardObjIfaceModel10GetStatus              (s_perfPolicyIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libPerfPolicyBoardObj", "s_perfPolicyIfaceModel10GetStatus");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * Construct the perf. policies object.
 */
FLCN_STATUS
perfPoliciesConstruct(void)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfPolicy)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY_35))
        {
            status = perfPoliciesConstruct_35(&Perf.pPolicies);
        }

        if ((status != FLCN_OK) ||
            (Perf.pPolicies == NULL))
        {
            PMU_BREAKPOINT();
            goto perfPoliciesConstruct_exit;
        }

perfPoliciesConstruct_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * PERF_POLICIES super implementation.
 *
 * @copydoc PerfPoliciesConstruct
 */
FLCN_STATUS
perfPoliciesConstruct_SUPER
(
    PERF_POLICIES **ppPolicies
)
{
    PERF_POLICIES *pPolicies = *ppPolicies;

    LW2080_CTRL_PERF_POINT_VIOLATION_STATUS_INIT(&pPolicies->globalViolationStatus);

    return FLCN_OK;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfPolicyBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList[] = {
        PERF_POLICY_OVERLAYS_BOARDOBJ
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
            PERF_POLICY,                                // _class
            pBuffer,                                    // _pBuffer
            s_perfPolicyIfaceModel10SetHeader,                // _hdrFunc
            s_perfPolicyIfaceModel10SetEntry,                 // _entryFunc
            turingAndLater.perf.perfPolicyGrpSet,       // _ssElement
            OVL_INDEX_DMEM(perfPolicy),                 // _ovlIdxDmem
            BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfPolicyBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList[] = {
        PERF_POLICY_OVERLAYS_BOARDOBJ
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                // _grpType
            PERF_POLICY,                                    // _class
            pBuffer,                                        // _pBuffer
            s_perfPolicyIfaceModel10GetStatusHeader,                    // _hdrFunc
            s_perfPolicyIfaceModel10GetStatus,                          // _entryFunc
            turingAndLater.perf.perfPolicyGrpGetStatus);    // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfPolicyGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PERF_POLICY        *pPolicy;
    RM_PMU_PERF_POLICY *pPolicyDesc = (RM_PMU_PERF_POLICY *)pBoardObjDesc;
    FLCN_STATUS         status;

    // Construct super class.
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pPolicy = (PERF_POLICY *)*ppBoardObj;

    // Set PERF_POLICY parameters.
    pPolicy->flags = pPolicyDesc->flags;

perfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * PERF_POLICY super implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfPolicyIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_POLICY_GET_STATUS *pPolicyStatus =
        (RM_PMU_PERF_POLICY_GET_STATUS *)pPayload;
    PERF_POLICY   *pPolicy   = (PERF_POLICY *)pBoardObj;
    PERF_POLICIES *pPolicies = PERF_POLICIES_GET();
    FLCN_STATUS    status;
    LwU8           i;

    // Call super-class implementation.
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfPolicyIfaceModel10GetStatus_SUPER_exit;
    }

    pPolicyStatus->violationStatus = pPolicy->violationStatus;
    FOR_EACH_INDEX_IN_MASK(32, i, pPolicy->violationStatus.perfPointMask)
    {
        LwU64_ALIGN32_ADD(
            &pPolicyStatus->violationStatus.perfPointTimeNs[i],
            &pPolicyStatus->violationStatus.perfPointTimeNs[i],
            &pPolicies->referenceTimeNs.parts
        );
    }
    FOR_EACH_INDEX_IN_MASK_END;

perfPolicyIfaceModel10GetStatus_SUPER_exit:
    return status;
}

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY_PMUMON)
/*!
 * @brief Gets the current status of the perf policies.
 *
 * This approach is similar to the BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS approach. Instead of
 * waiting for the client to request the status, the status is pushed to a
 * cirlwlar buffer in the super surface.
 *
 * @param  pPolicies  the perf policies to query
 * @param  pSample    the buffer to store the status
 *
 * @return FLCN_OK upon success; detailed error code otherwise
 */
FLCN_STATUS
perfPolicyPmumonGetSample
(
    PERF_POLICIES *pPolicies,
    RM_PMU_PERF_POLICY_BOARDOBJ_GRP_GET_STATUS *pSample
)
{
    BOARDOBJGRP_IFACE_MODEL_10 *pModel10 =
        boardObjGrpIfaceModel10FromBoardObjGrpGet((BOARDOBJGRP *)pPolicies);
    PERF_POLICY  *pPolicy;
    FLCN_STATUS   status;
    LwBoardObjIdx i;

    status = s_perfPolicyIfaceModel10GetStatusHeader(pModel10,
                                         (RM_PMU_BOARDOBJGRP *)pSample,
                                         &pPolicies->super.objMask.super);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPolicyPmumonGetSample_exit;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_POLICY, pPolicy, i, NULL)
    {
        BOARDOBJ_IFACE_MODEL_10 *pObjModel10 =
            boardObjIfaceModel10FromBoardObjGet((BOARDOBJ *)pPolicy);

        status = s_perfPolicyIfaceModel10GetStatus(pObjModel10,
                                       (RM_PMU_BOARDOBJ *)(&pSample->objects[i]));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfPolicyPmumonGetSample_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

perfPolicyPmumonGetSample_exit:
    return status;
}
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_POLICY_PMUMON)

/* ------------------------ Private Functions ------------------------------- */
/*!
 * PERF_POLICY implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_perfPolicyIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_POLICY_BOARDOBJGRP_SET_HEADER *pPoliciesDesc =
        (RM_PMU_PERF_POLICY_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    PERF_POLICIES *pPolicies = (PERF_POLICIES *)pBObjGrp;
    FLCN_STATUS    status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfPolicyIfaceModel10SetHeader_exit;
    }

    // Ensure that the RM version matches the PMU version.
    if (pPoliciesDesc->version != pPolicies->version)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_perfPolicyIfaceModel10SetHeader_exit;
    }

    // Copy in PERF_POLICIES data.
    pPolicies->pointMask           = pPoliciesDesc->supportedPointMask;
    pPolicies->pointMaskGlobalOnly = pPoliciesDesc->globalOnlyPointMask;

s_perfPolicyIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfPolicyIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10      *pModel10,
    BOARDOBJ        **ppBoardObj,
    RM_PMU_BOARDOBJ  *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_PERF_POLICY_SW_TYPE_35:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_POLICY_35);
        {
            return perfPolicyGrpIfaceModel10ObjSetImpl_35(pModel10, ppBoardObj,
                                              sizeof(PERF_POLICY_35),
                                              pBuf);
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * PERF_POLICY implementation
 *
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_perfPolicyIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *pPoliciesStatus =
        (RM_PMU_PERF_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    PERF_POLICIES *pPolicies = (PERF_POLICIES *)pBoardObjGrp;
    FLCN_STATUS    status    = FLCN_ERR_NOT_SUPPORTED;
    LwU8           i;

    // Copy out class-specific data.
    pPoliciesStatus->version               = pPolicies->version;
    osPTimerTimeNsLwrrentGet(&pPolicies->referenceTimeNs);
    pPoliciesStatus->referenceTimens       = pPolicies->referenceTimeNs.parts;
    pPoliciesStatus->globalViolationStatus = pPolicies->globalViolationStatus;
    pPoliciesStatus->limitingPolicyMask    = pPolicies->limitingPolicyMask;

    FOR_EACH_INDEX_IN_MASK(32, i, pPolicies->globalViolationStatus.perfPointMask)
    {
        LwU64_ALIGN32_ADD(
            &pPoliciesStatus->globalViolationStatus.perfPointTimeNs[i],
            &pPoliciesStatus->globalViolationStatus.perfPointTimeNs[i],
            &pPolicies->referenceTimeNs.parts
        );
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // Call into PERF_POLICIES version-specific header parsing.
    switch (pPolicies->version)
    {
        case LW2080_CTRL_PERF_POLICIES_VERSION_35:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_POLICY_35);
            status = perfPoliciesIfaceModel10GetStatusHeader_35(pModel10, pBuf, pMask);
            break;
        }
    }
    // Catch any version-specific errors.
    if (status != FLCN_OK)
    {
        goto s_perfPolicyIfaceModel10GetStatusHeader_exit;
    }

s_perfPolicyIfaceModel10GetStatusHeader_exit:
    return FLCN_OK;
}

/*!
 * PERF_POLICY implementation
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfPolicyIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_PERF_POLICY_SW_TYPE_35:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_POLICY_35);
        {
            return perfPolicyIfaceModel10GetStatus_35(pModel10, pBuf);
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
