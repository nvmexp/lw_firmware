/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pstate.c
 * @brief  Function definitions for the PSTATES SW module.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "dbgprintf.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * @brief   Array of all vtables pertaining to different PSTATE object types.
 */
BOARDOBJ_VIRTUAL_TABLE *PerfPstateVirtualTables[LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, MAX)] =
{
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, BASE)] = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, 2X)]   = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, 3X)]   = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, 30)]   = NULL,
    [LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, 35)]   = PERF_PSTATE_35_VTABLE(),
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief       PSTATES handler for the RM_PMU_BOARDOBJ_CMD_GRP_SET interface.
 *
 * @copydoc     BoardObjGrpIfaceModel10CmdHandler()
 *
 * @memberof    PSTATES
 *
 * @protected
 */
FLCN_STATUS
pstateBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPstateBoardObj)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_35)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfIlwalidation)
#endif
    };
    LwBool bFirstTime = (Perf.pstates.super.super.objSlots == 0U);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_SET(E32,         // _grpType
            PSTATE,                                 // _class
            pBuffer,                                // _pBuffer
            perfPstateIfaceModel10SetHeader,              // _hdrFunc
            perfPstateIfaceModel10SetEntry,               // _entryFunc
            pascalAndLater.perf.pstateGrpSet,       // _ssElement
            (LwU8)OVL_INDEX_DMEM(perf),             // _ovlIdxDmem
            PerfPstateVirtualTables);               // _ppObjectVtables
        if (status != FLCN_OK)
        {
            goto pstateBoardObjGrpIfaceModel10Set_exit;
        }

        // Ilwalidate the PSTATE frequency tuple.
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_INFRA))
        {
            if (!bFirstTime)
            {
                status = perfPstatesCache();
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pstateBoardObjGrpIfaceModel10Set_exit;
                }

                status = perfPstatesIlwalidate(LW_FALSE);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pstateBoardObjGrpIfaceModel10Set_exit;
                }
            }
        }

pstateBoardObjGrpIfaceModel10Set_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief       PSTATES handler for the RM_PMU_BOARDOBJ_CMD_GRP_GET_STATUS interface.
 *
 * @copydoc     BoardObjGrpIfaceModel10CmdHandler()
 *
 * @memberof    PSTATES
 *
 * @protected
 */
FLCN_STATUS
pstateBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPstateBoardObj)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E32,            // _grpType
            PSTATE,                                     // _class
            pBuffer,                                    // _pBuffer
            NULL,                                       // _hdrFunc
            pstateIfaceModel10GetStatus,                            // _entryFunc
            pascalAndLater.perf.pstateGrpGetStatus);    // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief       PSTATES implementation of @ref BoardObjGrpIfaceModel10SetHeader()
 *
 * @copydoc     BoardObjGrpIfaceModel10SetHeader()
 *
 * @memberof    PSTATES
 *
 * @protected
 */
FLCN_STATUS
perfPstateIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_PSTATE_BOARDOBJGRP_SET_HEADER   *pHdr;
    PSTATES *pPstates = (PSTATES *)pBObjGrp;
    FLCN_STATUS status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        goto perfPstateIfaceModel10SetHeader_exit;
    }
    pHdr = (RM_PMU_PERF_PSTATE_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    // Sanity check the index of script step
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (pHdr->numClkDomains > LW2080_CTRL_PERF_PSTATE_MAX_CLK_DOMAINS)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto perfPstateIfaceModel10SetHeader_exit;
        }
    }

    pPstates->numClkDomains = pHdr->numClkDomains;
    pPstates->bootPstateIdx = pHdr->bootPstateIdx;
    perfPstateSetNumVoltDomains(pPstates, pHdr->numVoltDomains);

perfPstateIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @brief       PSTATES implementation of @ref BoardObjGrpIfaceModel10SetEntry().
 *
 * @copydoc     BoardObjGrpIfaceModel10SetEntry()
 *
 * @memberof    PSTATES
 *
 * @protected
 */
FLCN_STATUS
perfPstateIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_PSTATE_TYPE_30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_30))
            {
                status = perfPstateGrpIfaceModel10ObjSet_30(pModel10, ppBoardObj,
                    sizeof(PSTATE_30), pBuf);
            }
            break;
        }
        case LW2080_CTRL_PERF_PSTATE_TYPE_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35))
            {
                status = perfPstateGrpIfaceModel10ObjSet_35(pModel10, ppBoardObj,
                    sizeof(PSTATE_35), pBuf);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief       PSTATES implementation of @ref BoardObjIfaceModel10GetStatus().
 *
 * @copydoc     BoardObjIfaceModel10GetStatus()
 *
 * @memberof    PSTATES
 *
 * @protected
 */
FLCN_STATUS
pstateIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pBuf
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_PSTATE_TYPE_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35))
            {
                status = pstateIfaceModel10GetStatus_35(pModel10, pBuf);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief       PSTATE implementation of @ref BoardObjGrpIfaceModel10ObjSet().
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 *
 * @memberof    PSTATE
 *
 * @protected
 */
FLCN_STATUS
perfPstateGrpIfaceModel10ObjSet_SUPER_IMPL
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    RM_PMU_PERF_PSTATE *pSet = (RM_PMU_PERF_PSTATE *)pBoardObjDesc;
    PSTATE             *pPstate;
    FLCN_STATUS         status;

    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPstateGrpIfaceModel10ObjSet_SUPER_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPstate, *ppBoardObj, PERF, PSTATE, BASE,
                                  &status, perfPstateGrpIfaceModel10ObjSet_SUPER_exit);

    // Copy-in PSTATE params
    pPstate->lpwrEntryIdx   = pSet->lpwrEntryIdx;
    pPstate->flags          = pSet->flags;
    pPstate->pstateNum      = pSet->pstateNum;

perfPstateGrpIfaceModel10ObjSet_SUPER_exit:
    return status;
}

/*!
 * @copydoc     PerfPstateClkFreqGet()
 *
 * @memberof    PSTATE
 *
 * @public
 */
FLCN_STATUS
perfPstateClkFreqGet_IMPL
(
    PSTATE                                 *pPstate,
    LwU8                                    clkDomainIdx,
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY    *pPstateClkEntry
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Validate used parameters
    if (pPstate == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfPstateClkFreqGet_exit;
    }

    // Call into type-specific implementations
    switch (BOARDOBJ_GET_TYPE(&pPstate->super))
    {
        case LW2080_CTRL_PERF_PSTATE_TYPE_30:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_30))
            {
                status = perfPstateClkFreqGet_30(pPstate,
                    clkDomainIdx, pPstateClkEntry);
            }
            break;
        }
        case LW2080_CTRL_PERF_PSTATE_TYPE_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35))
            {
                status = perfPstateClkFreqGet_35(pPstate,
                    clkDomainIdx, pPstateClkEntry);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            PMU_BREAKPOINT();
            break;
        }
    }

perfPstateClkFreqGet_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- End of File ------------------------------------ */
