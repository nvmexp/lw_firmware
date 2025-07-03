/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    client_perf_cf_policy.c
 * @copydoc client_perf_cf_policy.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/cf/perf_cf.h"
#include "perf/cf/client_perf_cf_policy.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjGrpIfaceModel10SetHeader   (s_perfCfClientPerfCfPolicyIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfClientPerfCfPolicyIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry    (s_perfCfClientPerfCfPolicyIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfClientPerfCfPolicyIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10GetStatusHeader   (s_perfCfClientPerfCfPolicyIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfClientPerfCfPolicyIfaceModel10GetStatusHeader");
static BoardObjIfaceModel10GetStatus                (s_perfCfClientPerfCfPolicyIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "s_perfCfClientPerfCfPolicyIfaceModel10GetStatus");

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status;

    // Construct using header and entries.
    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,                 // _grpType
        CLIENT_PERF_CF_POLICY,                          // _class
        pBuffer,                                        // _pBuffer
        s_perfCfClientPerfCfPolicyIfaceModel10SetHeader,      // _hdrFunc
        s_perfCfClientPerfCfPolicyIfaceModel10SetEntry,       // _entryFunc
        turingAndLater.perfCf.client.policyGrpSet,      // _ssElement
        OVL_INDEX_DMEM(perfCfController),               // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);        // _ppObjectVtables
    if (status != FLCN_OK)
    {
        goto perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10Set_exit;
    }

perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10Set_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS status = LW_OK;

    status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,                        // _grpType
        CLIENT_PERF_CF_POLICY,                                  // _class
        pBuffer,                                                // _pBuffer
        s_perfCfClientPerfCfPolicyIfaceModel10GetStatusHeader,              // _hdrFunc
        s_perfCfClientPerfCfPolicyIfaceModel10GetStatus,                    // _entryFunc
        turingAndLater.perfCf.client.policyGrpGetStatus);       // _ssElement
    if (status != FLCN_OK)
    {
        goto perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10GetStatus_exit;
    }

perfCfClientPerfCfPolicyBoardObjGrpIfaceModel10GetStatus_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfCfClientPerfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_CF_CLIENT_PERF_CF_POLICY *pDescPolicy =
        (RM_PMU_PERF_CF_CLIENT_PERF_CF_POLICY *)pBoardObjDesc;
    CLIENT_PERF_CF_POLICY                *pPolicy;
    FLCN_STATUS                           status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        goto perfCfClientPerfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pPolicy = (CLIENT_PERF_CF_POLICY *)*ppBoardObj;

    // Set member variables.
    pPolicy->label     = pDescPolicy->label;
    pPolicy->policyIdx = pDescPolicy->policyIdx;

perfCfClientPerfCfPolicyGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpQuery
 */
FLCN_STATUS
perfCfClientPerfCfPolicyIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    FLCN_STATUS status;

    // Call super-class implementation
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfCfClientPerfCfPolicyIfaceModel10GetStatus_SUPER_exit;
    }

perfCfClientPerfCfPolicyIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_perfCfClientPerfCfPolicyIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10 *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
            boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc),
            s_perfCfClientPerfCfPolicyIfaceModel10SetHeader_exit);

s_perfCfClientPerfCfPolicyIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfCfClientPerfCfPolicyIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_CLIENT_PERF_CF_POLICY_TYPE_CTRL_MASK:
            return perfCfClientPerfCfPolicyGrpIfaceModel10ObjSetImpl_CTRL_MASK(pModel10, ppBoardObj,
                        LW_SIZEOF32(CLIENT_PERF_CF_POLICY_CTRL_MASK), pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_perfCfClientPerfCfPolicyIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    RM_PMU_PERF_CF_CLIENT_PERF_CF_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *pHdr =
        (RM_PMU_PERF_CF_CLIENT_PERF_CF_POLICY_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    BOARDOBJGRPMASK_E32     *pActiveMaskArbitrated = perfCfPolicysActiveMaskArbitratedGet();
    CLIENT_PERF_CF_POLICY   *pPolicy;
    LwBoardObjIdx            i;

    // Init mask.
    LW2080_CTRL_BOARDOBJGRP_MASK_E32_INIT(&(pHdr->activeMask.super));

    // Pull the active policy mask from internal policy.
    BOARDOBJGRP_ITERATOR_BEGIN(CLIENT_PERF_CF_POLICY, pPolicy, i, NULL)
    {
        if (boardObjGrpMaskBitGet(pActiveMaskArbitrated, pPolicy->policyIdx))
        {
            LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&(pHdr->activeMask.super), i);
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return FLCN_OK;
}

/*!
 * @Copydoc BoardObjIfaceModel10GetStatus
 */
static FLCN_STATUS
s_perfCfClientPerfCfPolicyIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10               *pModel10,
    RM_PMU_BOARDOBJ        *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_CLIENT_PERF_CF_POLICY_TYPE_CTRL_MASK:
            return perfCfClientPerfCfPolicyIfaceModel10GetStatus_CTRL_MASK(pModel10, pBuf);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}
