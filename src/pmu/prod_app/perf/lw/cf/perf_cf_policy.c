/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_policy.c
 * @copydoc perf_cf_policy.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/cf/perf_cf.h"
#include "perf/cf/perf_cf_policy.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader   (s_perfCfPolicyIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPolicyIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry    (s_perfCfPolicyIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPolicyIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10GetStatusHeader   (s_perfCfPolicyIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPolicyIfaceModel10GetStatusHeader");
static BoardObjIfaceModel10GetStatus                (s_perfCfPolicyIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPolicyIfaceModel10GetStatus");
static PerfCfPolicyActivate         (s_perfCfPolicyActivate);
static FLCN_STATUS                  s_perfCfPolicysUpdate(PERF_CF_POLICYS *pPolicys, LwBool bForce);
static FLCN_STATUS                  s_perfCfPolicysActivateRequestUpdate(PERF_CF_POLICYS *pPolicys, PERF_CF_POLICY *pPolicy)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfPolicysActivateRequestUpdate");
static FLCN_STATUS                  s_perfCfPolicysActivateRequestUpdateAndArbitrate(PERF_CF_POLICYS *pPolicys, PERF_CF_POLICY *pPolicy)
    GCC_ATTRIB_SECTION("imem_perfCf", "s_perfCfPolicysActivateRequestUpdateAndArbitrate");
static FLCN_STATUS                  s_perfCfPolicyPostConstruct(PERF_CF_POLICYS *pPolicys)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfPolicyPostConstruct")
    GCC_ATTRIB_NOINLINE();

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfPolicyBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status     = FLCN_OK;

    // Construct using header and entries.
    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,             // _grpType
        PERF_CF_POLICY,                             // _class
        pBuffer,                                    // _pBuffer
        s_perfCfPolicyIfaceModel10SetHeader,              // _hdrFunc
        s_perfCfPolicyIfaceModel10SetEntry,               // _entryFunc
        turingAndLater.perfCf.policyGrpSet,         // _ssElement
        OVL_INDEX_DMEM(perfCfController),           // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        goto perfCfPolicyBoardObjGrpIfaceModel10Set_exit;
    }

    // Trigger Perf CF Policy patches.
    status = perfCfPolicysPatch_HAL(&PerfCf);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfPolicyBoardObjGrpIfaceModel10Set_exit;
    }

    // Trigger Perf CF Policy sanity check.
    status = perfCfPolicysSanityCheck_HAL(&PerfCf);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfCfPolicyBoardObjGrpIfaceModel10Set_exit;
    }

    status = s_perfCfPolicyPostConstruct(PERF_CF_GET_POLICYS());
    if (status != FLCN_OK)
    {
        goto perfCfPolicyBoardObjGrpIfaceModel10Set_exit;
    }

perfCfPolicyBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfPolicyBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = LW_OK;

    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,            // _grpType
        PERF_CF_POLICY,                             // _class
        pBuffer,                                    // _pBuffer
        s_perfCfPolicyIfaceModel10GetStatusHeader,              // _hdrFunc
        s_perfCfPolicyIfaceModel10GetStatus,                    // _entryFunc
        turingAndLater.perfCf.policyGrpGetStatus);  // _ssElement

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_POLICY *pDescPolicy =
        (RM_PMU_PERF_CF_POLICY *)pBoardObjDesc;
    PERF_CF_POLICYS *pPolicys = PERF_CF_GET_POLICYS();
    PERF_CF_POLICY     *pPolicy;
    BOARDOBJGRPMASK_E32 mask;
    LwBool              bFirstConstruct = (*ppBoardObj == NULL);
    FLCN_STATUS         status          = FLCN_OK;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pPolicy = (PERF_CF_POLICY *)*ppBoardObj;

    // Process the conflict mask input.
    boardObjGrpMaskInit_E32(&mask);
    status = boardObjGrpMaskImport_E32(&mask, &pDescPolicy->conflictMask);
    if (status != FLCN_OK)
    {
        goto perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    status = boardObjGrpMaskIlw(&mask);
    if (status != FLCN_OK)
    {
        goto perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    if (!bFirstConstruct)
    {
        // We cannot allow subsequent SET calls to change these parameters.
        if (!boardObjGrpMaskIsEqual(&pPolicy->compatibleMask, &mask) ||
            (pPolicy->priority != pDescPolicy->priority))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }

        if (pPolicy->label != pDescPolicy->label)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }
    else
    {
        perfCfPolicyDeactivateRequestIdMaskInit(&pPolicy->deactivateMask);
    }

    //
    // Set member variables.
    //

    boardObjGrpMaskInit_E32(&(pPolicy->compatibleMask));
    status = boardObjGrpMaskCopy(&(pPolicy->compatibleMask), &mask);
    if (status != FLCN_OK)
    {
        goto perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

    pPolicy->priority  = pDescPolicy->priority;
    pPolicy->label     = pDescPolicy->label;
    pPolicy->bActivate = pDescPolicy->bActivate;

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPolicyDeactivateRequestIdMaskRmImport(
            &pPolicy->deactivateMask, &pDescPolicy->deactivateMask),
        perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPolicysActivateRequestUpdate(pPolicys, pPolicy),
        perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit);

    // Policy has to be compatible with self.
    if (!boardObjGrpMaskBitGet(&(pPolicy->compatibleMask),
                               BOARDOBJ_GET_GRP_IDX(*ppBoardObj)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }

perfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfPolicyIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_CF_POLICY_GET_STATUS *pStatus =
        (RM_PMU_PERF_CF_POLICY_GET_STATUS *)pPayload;
    PERF_CF_POLICYS *pPolicys = PERF_CF_GET_POLICYS();
    PERF_CF_POLICY *pPolicy =
        (PERF_CF_POLICY *)pBoardObj;
    FLCN_STATUS status = FLCN_OK;

    // Call super-class impementation;
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfPolicyIfaceModel10GetStatus_SUPER_exit;
    }

    // SUPER class specific implementation.
    pStatus->bActiveLwrr = boardObjGrpMaskBitGet(&(pPolicys->activeMaskArbitrated),
                                                 BOARDOBJ_GET_GRP_IDX(pBoardObj));

    PMU_ASSERT_OK_OR_GOTO(status,
        perfCfPolicyDeactivateRequestIdMaskExport(
            &pPolicy->deactivateMask, &pStatus->deactivateMask),
        perfCfPolicyIfaceModel10GetStatus_SUPER_exit);

perfCfPolicyIfaceModel10GetStatus_SUPER_exit:
    return status;
}


/*!
 * @copydoc PerfCfPolicysLoad
 */
FLCN_STATUS
perfCfPolicysLoad
(
    PERF_CF_POLICYS    *pPolicys,
    LwBool              bLoad
)
{
    FLCN_STATUS     status          = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[]   = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfController)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Nothing to do when there are no POLICY objects.
        if (boardObjGrpMaskIsZero(&(pPolicys->super.objMask)))
        {
            goto perfCfPolicysLoad_exit;
        }

        if (bLoad)
        {
            // Apply active policys.
            status = s_perfCfPolicysUpdate(pPolicys, LW_TRUE);
            if (status != FLCN_OK)
            {
                goto perfCfPolicysLoad_exit;
            }
        }
        else
        {
            // Add unload() actions here.
        }
    }
perfCfPolicysLoad_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * PERF_CF_POLICYS post-init function.
 */
FLCN_STATUS
perfCfPolicysPostInit
(
    PERF_CF_POLICYS *pPolicys
)
{
    boardObjGrpMaskInit_E32(&(pPolicys->activeMaskRequested));

    boardObjGrpMaskInit_E32(&(pPolicys->activeMaskArbitrated));

    pPolicys->prioritySortedIdxN = 0;

    return FLCN_OK;
}

/*!
 * @copydoc PerfCfPolicysActivate
 */
FLCN_STATUS
perfCfPolicysActivate
(
    PERF_CF_POLICYS    *pPolicys,
    LwU8                label,
    LwBool              bActivate
)
{
    PERF_CF_POLICY *pPolicy = NULL;
    LwBoardObjIdx   i;
    FLCN_STATUS     status  = FLCN_ERR_NOT_SUPPORTED;

    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_POLICY, pPolicy, i, NULL)
    {
        if (pPolicy->label == label)
        {
            status = FLCN_OK;

            pPolicy->bActivate = bActivate;

            PMU_ASSERT_OK_OR_GOTO(status,
                s_perfCfPolicysActivateRequestUpdateAndArbitrate(
                    pPolicys, pPolicy),
                perfCfPolicysActivate_exit);

            // Always break out
            break;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

perfCfPolicysActivate_exit:
    return status;
}

/*!
 * @copydoc PerfCfPolicysDeactivateMaskSet
 */
FLCN_STATUS
perfCfPolicysDeactivateMaskSet
(
    PERF_CF_POLICYS *pPolicys,
    LwU8 label,
    LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID requestId,
    LwBool bSet
)
{
    FLCN_STATUS status;
    PERF_CF_POLICY *pPolicy = NULL;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPolicys != NULL) &&
        (label < LW2080_CTRL_PERF_PERF_CF_POLICY_LABEL_NUM) &&
        (requestId >= LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_PMU__START) &&
        (requestId <= LW2080_CTRL_PERF_PERF_CF_POLICY_DEACTIVATE_REQUEST_ID_PMU__END) &&
        ((pPolicy = PERF_CF_POLICYS_LABEL_GET(pPolicys, label)) != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfPolicysDeactivateMaskSet_exit);

    // Bail early if the mask is already set to the provided state.
    if (perfCfPolicyDeactivateRequestIdMaskGet(
            &pPolicy->deactivateMask, requestId) == bSet)
    {
        goto perfCfPolicysDeactivateMaskSet_exit;
    }

    if (bSet)
    {
        perfCfPolicyDeactivateRequestIdMaskSet(
            &pPolicy->deactivateMask, requestId);
    }
    else
    {
        perfCfPolicyDeactivateRequestIdMaskClr(
            &pPolicy->deactivateMask, requestId);
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPolicysActivateRequestUpdateAndArbitrate(pPolicys, pPolicy),
        perfCfPolicysDeactivateMaskSet_exit);

perfCfPolicysDeactivateMaskSet_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader  
 */
static FLCN_STATUS
s_perfCfPolicyIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10 *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    FLCN_STATUS status;
    PERF_CF_POLICYS *const pPolicys =
        (PERF_CF_POLICYS *)pBObjGrp;
    const RM_PMU_PERF_CF_POLICY_BOARDOBJGRP_SET_HEADER *const pHdr =
        (RM_PMU_PERF_CF_POLICY_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    LwU32 label;

    PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc),
            s_perfCfPolicyIfaceModel10SetHeader_exit);

    for (label = 0U; label < LW2080_CTRL_PERF_PERF_CF_POLICY_LABEL_NUM; label++)
    {
        pPolicys->labelToIdxMap[label] = pHdr->labelToIdxMap[label];
    }

s_perfCfPolicyIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfCfPolicyIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_PERF_CF_POLICY_TYPE_CTRL_MASK:
            return perfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK(pModel10, ppBoardObj,
                        sizeof(PERF_CF_POLICY_CTRL_MASK), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_perfCfPolicyIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_CF_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PERF_CF_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    PERF_CF_POLICYS *pPolicys =
        (PERF_CF_POLICYS *)pBoardObjGrp;
    FLCN_STATUS status = FLCN_OK;

    status = boardObjGrpMaskExport_E32(&(pPolicys->activeMaskRequested),
        &(pHdr->activeMaskRequested));
    if (status != FLCN_OK)
    {
        goto s_perfCfPolicyIfaceModel10GetStatusHeader_exit;
    }

    status = boardObjGrpMaskExport_E32(&(pPolicys->activeMaskArbitrated),
        &(pHdr->activeMaskArbitrated));
    if (status != FLCN_OK)
    {
        goto s_perfCfPolicyIfaceModel10GetStatusHeader_exit;
    }

s_perfCfPolicyIfaceModel10GetStatusHeader_exit:
    return FLCN_OK;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfCfPolicyIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_PERF_CF_POLICY_TYPE_CTRL_MASK:
            return perfCfPolicyIfaceModel10GetStatus_CTRL_MASK(pModel10, pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc PerfCfPolicyActivate
 */
static FLCN_STATUS
s_perfCfPolicyActivate
(
    PERF_CF_POLICY         *pPolicy,
    LwBool                  bActivate,
    BOARDOBJGRPMASK_E32    *pMaskControllers
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (BOARDOBJ_GET_TYPE(pPolicy))
    {
        case LW2080_CTRL_PERF_PERF_CF_POLICY_TYPE_CTRL_MASK:
            status = perfCfPolicyActivate_CTRL_MASK(pPolicy, bActivate, pMaskControllers);
            break;
        default:
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto s_perfCfPolicyActivate_exit;
    }

s_perfCfPolicyActivate_exit:
    return status;
}

/*!
 * @interface   PERF_CF_POLICYS
 *
 * @brief       To be called after control (e.g. activate/deactivate) changes,
 *              to resolve conflicts and apply policies.
 *
 * @return  FLCN_OK     Policys successfully activated
 * @return  other       Propagates return values from various calls
 */
static FLCN_STATUS
s_perfCfPolicysUpdate
(
    PERF_CF_POLICYS    *pPolicys,
    LwBool              bForce
)
{
    PERF_CF_CONTROLLERS    *pControllers = PERF_CF_GET_CONTROLLERS();
    BOARDOBJGRPMASK_E32     maskControllers;
    PERF_CF_POLICY         *pPolicy;
    LwBoardObjIdx           i;
    FLCN_STATUS             status          = FLCN_OK;


    // Start off with requested active mask.
    status = boardObjGrpMaskCopy(&(pPolicys->activeMaskArbitrated),
                                 &(pPolicys->activeMaskRequested));
    if (status != FLCN_OK)
    {
        goto s_perfCfPolicysUpdate_exit;
    }

    //
    // Go through the policys by priority. For each active policy, bit-wise-AND
    // the active mask with its compatible mask to clear conflicts.
    //

    for (i = 0; i < pPolicys->prioritySortedIdxN; i++)
    {
        if (boardObjGrpMaskBitGet(&(pPolicys->activeMaskArbitrated),
                                  pPolicys->prioritySortedIdx[i]))
        {
            pPolicy = BOARDOBJGRP_OBJ_GET(PERF_CF_POLICY, pPolicys->prioritySortedIdx[i]);
            if (pPolicy == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto s_perfCfPolicysUpdate_exit;
            }

            status = boardObjGrpMaskAnd(&(pPolicys->activeMaskArbitrated),
                                        &(pPolicys->activeMaskArbitrated),
                                        &(pPolicy->compatibleMask));
            if (status != FLCN_OK)
            {
                goto s_perfCfPolicysUpdate_exit;
            }
        }
    }

    // Activate/deactivate all policys and generate controller mask.
    boardObjGrpMaskInit_E32(&maskControllers);
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_POLICY, pPolicy, i, NULL)
    {
        status = s_perfCfPolicyActivate(pPolicy,
            boardObjGrpMaskBitGet(&(pPolicys->activeMaskArbitrated), i),
            &maskControllers);
        if (status != FLCN_OK)
        {
            goto s_perfCfPolicysUpdate_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    // Set active controller mask.
    status = perfCfControllersMaskActiveSet(pControllers, &maskControllers);
    if (status != FLCN_OK)
    {
        goto s_perfCfPolicysUpdate_exit;
    }

s_perfCfPolicysUpdate_exit:
    return status;
}

/*!
 * @interface   PERF_CF_POLICYS
 *
 * @brief   Updates @ref PERF_CF_POLICYS::activeMaskRequested after a change in
 *          activation state of a @ref PERF_CF_POLICY
 *
 * @param[in,out]   pPolicys    @ref PERF_CF_POLICYS group pointer in which to
 *                              update
 * @param[in]       pPolicy     @ref PERF_CF_POLICY object pointer for which to
 *                              update activate request state.
 *
 *
 * @return  @ref FLCN_OK    Success
 */
static FLCN_STATUS
s_perfCfPolicysActivateRequestUpdate
(
    PERF_CF_POLICYS *pPolicys,
    PERF_CF_POLICY *pPolicy
)
{
    if (pPolicy->bActivate &&
        !perfCfPolicyDeactivateRequestIdMaskIsDeactivated(
                        &pPolicy->deactivateMask))
    {
        boardObjGrpMaskBitSet(&pPolicys->activeMaskRequested,
                              BOARDOBJ_GET_GRP_IDX(&pPolicy->super));
    }
    else
    {
        boardObjGrpMaskBitClr(&pPolicys->activeMaskRequested,
                              BOARDOBJ_GET_GRP_IDX(&pPolicy->super));
    }

    return FLCN_OK;
}

/*!
 * @interface   PERF_CF_POLICYS
 *
 * @brief   Updates @ref PERF_CF_POLICYS::activeMaskRequested after a change in
 *          activation state of a @ref PERF_CF_POLICY and does the arbitration
 *          based on that update.
 *
 * @param[in,out]   pPolicys    @ref PERF_CF_POLICYS group pointer in which to
 *                              update
 * @param[in]       pPolicy     @ref PERF_CF_POLICY object pointer for which to
 *                              update activate request state.
 *
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors propagated from callees.
 */
static FLCN_STATUS
s_perfCfPolicysActivateRequestUpdateAndArbitrate
(
    PERF_CF_POLICYS *pPolicys,
    PERF_CF_POLICY *pPolicy
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        s_perfCfPolicysActivateRequestUpdate(pPolicys, pPolicy),
        s_perfCfPolicysActivateRequestUpdateAndArbitrate_exit);

    // Need to update policys after activate/deactivate during runtime.
    if (PERF_CF_GET_OBJ()->bLoaded)
    {
        PMU_ASSERT_OK_OR_GOTO(status,
            s_perfCfPolicysUpdate(pPolicys, LW_FALSE),
            s_perfCfPolicysActivateRequestUpdateAndArbitrate_exit);
    }

s_perfCfPolicysActivateRequestUpdateAndArbitrate_exit:
    return status;
}

/*!
 * @interface   PERF_CF_POLICYS
 *
 * @brief       To be called after construct, to finish construct and validate.
 *
 * @return  FLCN_OK     Policys successfully post-constructed and validated.
 * @return  other       Propagates return values from various calls.
 */
static FLCN_STATUS
s_perfCfPolicyPostConstruct
(
    PERF_CF_POLICYS *pPolicys
)
{
    PERF_CF_POLICY     *pPolicy;
    LwU8                priorityVal[LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS];
    LwBoardObjIdx       i;
    LwU8                j;
    LwU8                n;
    LwU8                highest;
    FLCN_STATUS         status  = FLCN_OK;

    // Initialize the (yet) unsorted index and priority arrays.
    n = 0;
    BOARDOBJGRP_ITERATOR_BEGIN(PERF_CF_POLICY, pPolicy, i, NULL)
    {
        pPolicys->prioritySortedIdx[n] = BOARDOBJ_GRP_IDX_TO_8BIT(i);
        priorityVal[n] = pPolicy->priority;
        n++;
    }
    BOARDOBJGRP_ITERATOR_END;
    pPolicys->prioritySortedIdxN = n;

    // Selection sort policy indexes from highest priority to lowest.
    for (i = 0; i < n; i++)
    {
        highest = i;

        // Find the highest priority from the remaining. Lower index wins tie-breaker.
        for (j = i+1; j < n; j++)
        {
            if (priorityVal[j] > priorityVal[highest])
            {
                highest = j;
            }
            else if ((priorityVal[j] == priorityVal[highest]) &&
                        (pPolicys->prioritySortedIdx[j] < pPolicys->prioritySortedIdx[highest]))
            {
                highest = j;
            }
        }

        // Swap highest with i, using j as temporary.
        j = pPolicys->prioritySortedIdx[i];
        pPolicys->prioritySortedIdx[i] = pPolicys->prioritySortedIdx[highest];
        pPolicys->prioritySortedIdx[highest] = j;

        // Move priority value too.
        priorityVal[highest] = priorityVal[i];
    }

    // Need to update policys after activate/deactivate in runtime.
    if (PERF_CF_GET_OBJ()->bLoaded)
    {
        status = s_perfCfPolicysUpdate(pPolicys, LW_FALSE);
        if (status != FLCN_OK)
        {
            goto s_perfCfPolicyPostConstruct_exit;
        }
    }

s_perfCfPolicyPostConstruct_exit:
    return status;
}
