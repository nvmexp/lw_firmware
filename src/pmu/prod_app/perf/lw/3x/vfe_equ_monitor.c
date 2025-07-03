/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_equ_monitor.c
 * @brief   VFE_EQU_MONITOR class implementation.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "perf/3x/vfe.h"

#include "g_pmurpc.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Separate VFE_EQU_MONITOR allcoated in "dmem_perf" to be usable within PMU
 * PERF code without requiring to attach the entire "dmem_perfVfe" and
 * "imem_perfVfe" overlays at run-time.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_PMU_EQU_MONITOR)
VFE_EQU_MONITOR PmuEquMonitor
    GCC_ATTRIB_SECTION("dmem_perf", "PmuEquMonitor");
#endif

/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc VfeEquMonitorInit()
 * @memberof VFE_EQU_MONITOR
 * @public
 */
void
vfeEquMonitorInit
(
    VFE_EQU_MONITOR *pMonitor
)
{
    boardObjGrpMaskInit_E32(&pMonitor->equMonitorMask);
    boardObjGrpMaskInit_E32(&pMonitor->equMonitorDirtyMask);
}

/*!
 * @copydoc VfeEquMonitorImport()
 * @memberof VFE_EQU_MONITOR
 * @public
 */
FLCN_STATUS
vfeEquMonitorImport
(
    VFE_EQU_MONITOR                 *pMonitor,
    RM_PMU_PERF_VFE_EQU_MONITOR_SET *pRmMonitor
)
{
    FLCN_STATUS   status = FLCN_OK;
    LwBoardObjIdx i;

    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        //
        // Check that the mask being imported contains at most 16 elements
        // The array it is being copied to is of this size, prevent overflow
        // in the BOARDOBJGRPMASK_FOR_EACH_BEGIN below
        //
        pRmMonitor->equMonitorMask &=
            (LWBIT32(RM_PMU_PERF_VFE_EQU_MONITOR_COUNT_MAX) - 1);
    }

    //
    // Import the VFE_EQU mask.
    //
    // CRPTODO - Colwert the RM_PMU structure to using the formal
    // BOARDOBJGRPMASK type.
    //
    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskImport_E32(&pMonitor->equMonitorMask,
            (LW2080_CTRL_BOARDOBJGRP_MASK_E32 *)&pRmMonitor->equMonitorMask),
        vfeEquMonitorImport_exit);

    // On import, just assume that all entries are now dirty.
    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskCopy(&pMonitor->equMonitorDirtyMask, &pMonitor->equMonitorMask),
        vfeEquMonitorImport_exit);

    // Import the individual VFE_EQU_MONITOR entries which are set in the mask.
    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pMonitor->equMonitorMask, i)
    {
        pMonitor->monitor[i] = pRmMonitor->monitor[i];
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

vfeEquMonitorImport_exit:
    return status;
}

/*!
 * @copydoc VfeEquMonitorUpdate()
 * @memberof VFE_EQU_MONITOR
 * @public
 */
FLCN_STATUS
vfeEquMonitorUpdate
(
    VFE_EQU_MONITOR *pMonitor,
    LwBool           bIlwalidate
)
{
    FLCN_STATUS   status = FLCN_OK;
    LwBoardObjIdx i;

    // If ilwalidate requested, dirty all entries so that all will be re-cached.
    if (bIlwalidate)
    {
        PMU_HALT_OK_OR_GOTO(status,
            boardObjGrpMaskCopy(&pMonitor->equMonitorDirtyMask,
                                &pMonitor->equMonitorMask),
            vfeEquMonitorUpdate_exit);
    }

    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pMonitor->equMonitorDirtyMask, i)
    {
        LwVfeEquIdx monitorVfeEquIdxToUse;
        VFE_EQU_IDX_COPY_RM_TO_PMU(monitorVfeEquIdxToUse, pMonitor->monitor[i].equIdx);
        PMU_HALT_OK_OR_GOTO(status,
            vfeEquEvaluate(monitorVfeEquIdxToUse,
                pMonitor->monitor[i].varValues,
                pMonitor->monitor[i].varCount,
                pMonitor->monitor[i].outputType,
              &(pMonitor->monitor[i].result)),
            vfeEquMonitorUpdate_exit);

        // Mark the entry as now cached, i.e. not dirty.
        boardObjGrpMaskBitClr(&pMonitor->equMonitorDirtyMask, i);
    }
    BOARDOBJGRPMASK_FOR_EACH_END;

vfeEquMonitorUpdate_exit:
    return status;
}

/*!
 * @copydoc VfeEquMonitorResultGet()
 * @memberof VFE_EQU_MONITOR
 * @public
 */
FLCN_STATUS
vfeEquMonitorResultGet
(
    VFE_EQU_MONITOR            *pMonitor,
    LwU8                        idx,
    RM_PMU_PERF_VFE_EQU_RESULT *pResult
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check the requested index.
    if (!boardObjGrpMaskBitGet(&pMonitor->equMonitorMask, idx) ||
        (idx >= RM_PMU_PERF_VFE_EQU_MONITOR_COUNT_MAX))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto vfeEquMonitorResultGet_exit;
    }
    //
    // Ensure that the monitor entry is actually cached, such that the result is
    // safe to return.
    //
    if (boardObjGrpMaskBitGet(&pMonitor->equMonitorDirtyMask, idx))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto vfeEquMonitorResultGet_exit;
    }

    *pResult = pMonitor->monitor[idx].result;

vfeEquMonitorResultGet_exit:
    return status;
}

/*!
 * @copydoc VfeEquMonitorAcquire()
 * @memberof VFE_EQU_MONITOR
 * @public
 */
FLCN_STATUS
vfeEquMonitorAcquire
(
    VFE_EQU_MONITOR    *pMonitor,
    LwU8               *pIdx
)
{
    BOARDOBJGRPMASK_E32       equMonitorMaskFree;
    FLCN_STATUS status = FLCN_OK;
    LwU8        i;

    // Find the first free bit in the equMonitorMask.
    boardObjGrpMaskInit_E32(&equMonitorMaskFree);

    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskCopy(&equMonitorMaskFree, &pMonitor->equMonitorMask),
        vfeEquMonitorAcquire_exit);

    PMU_HALT_OK_OR_GOTO(status,
        boardObjGrpMaskIlw(&equMonitorMaskFree),
        vfeEquMonitorAcquire_exit);

    i = boardObjGrpMaskBitIdxLowest(&equMonitorMaskFree);

    // Check if space remains in @ref VFE_EQU_MONITOR::monitor[].
    if (i >= RM_PMU_PERF_VFE_EQU_MONITOR_COUNT_MAX)
    {
        status = FLCN_ERR_OUT_OF_RANGE;
        PMU_BREAKPOINT();
        goto vfeEquMonitorAcquire_exit;
    }

    //
    // "Acquire" the entry in @ref VFE_EQU_MONITOR::monitor[] by setting the
    // bit in the mask.  Also, mark it dirty so it will be cached in @ref
    // VfeEquMonitorUpdate().
    //
    *pIdx = i;
    boardObjGrpMaskBitSet(&pMonitor->equMonitorMask, i);
    boardObjGrpMaskBitSet(&pMonitor->equMonitorDirtyMask, i);

vfeEquMonitorAcquire_exit:
    return status;
}

/*!
 * @copydoc VfeEquMonitorSet()
 * @memberof VFE_EQU_MONITOR
 * @public
 */
FLCN_STATUS
vfeEquMonitorSet
(
    VFE_EQU_MONITOR           *pMonitor,
    LwU8                       idx,
    LwVfeEquIdx                equIdx,
    LwU8                       outputType,
    LwU8                       varCount,
    RM_PMU_PERF_VFE_VAR_VALUE *pVarValues
)
{
    RM_PMU_PERF_VFE_EQU_EVAL *pEval;
    FLCN_STATUS status = FLCN_OK;
    LwU8        i;

    // Sanity check the input arguments
    if (!boardObjGrpMaskBitGet(&pMonitor->equMonitorMask, idx))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto vfeEquMonitorSet_exit;
    }
    if (varCount > RM_PMU_PERF_VFE_EQU_EVAL_VAR_COUNT_MAX)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto vfeEquMonitorSet_exit;
    }

    // Input is changing, so mark the index as dirty.
    boardObjGrpMaskBitSet(&pMonitor->equMonitorDirtyMask, idx);

    //
    // Copy in the specified caller parameters to the @ref
    // VFE_EQU_MONITOR::monitor[] structure.
    //
    pEval = &pMonitor->monitor[idx];
    VFE_EQU_IDX_COPY_PMU_TO_RM(pEval->equIdx, equIdx);
    pEval->outputType = outputType;
    pEval->varCount = varCount;
    for (i = 0; i < varCount; i++)
    {
        pEval->varValues[i] = pVarValues[i];
    }

vfeEquMonitorSet_exit:
    return status;
}

/*!
 * @copydoc VfeEquMonitorRelease()
 * @memberof VFE_EQU_MONITOR
 * @public
 */
FLCN_STATUS
vfeEquMonitorRelease
(
    VFE_EQU_MONITOR *pMonitor,
    LwU8             idx
)
{
    FLCN_STATUS status = FLCN_OK;

    // Sanity check that the specified index is actually acquired.
    if (!boardObjGrpMaskBitGet(&pMonitor->equMonitorMask, idx))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto vfeEquMonitorRelease_exit;
    }

    // Clear the index in the  acquired and dirty masks.
    boardObjGrpMaskBitClr(&pMonitor->equMonitorMask, idx);
    boardObjGrpMaskBitClr(&pMonitor->equMonitorDirtyMask, idx);

vfeEquMonitorRelease_exit:
    return status;
}

/*!
 * @brief   Exelwtes generic VFE_EQU_MONITOR_SET RPC.
 *
 * @memberof VFE_EQU_MONITOR
 * @public
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PERF_VFE_EQU_MONITOR_SET
 *
 * @return FLCN_OK upon success.
 * @return Error codes returned by called functions.
 */
FLCN_STATUS
pmuRpcPerfVfeEquMonitorSet
(
    RM_PMU_RPC_STRUCT_PERF_VFE_EQU_MONITOR_SET *pParams
)
{
    FLCN_STATUS     status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_VFE(FULL)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PMU_HALT_OK_OR_GOTO(status,
            vfeEquMonitorImport(PERF_GET_VFE(&Perf)->pRmEquMonitor, &pParams->data),
            pmuRpcPerfVfeEquMonitorSet_exit);

        // Use the PERF DMEM read semaphore.
        perfReadSemaphoreTake();
        {
            // Evaluate in client's structure since client expects updated results.
            status = vfeEquMonitorUpdate(PERF_GET_VFE(&Perf)->pRmEquMonitor, LW_FALSE);
        }
        perfReadSemaphoreGive();

        PMU_ASSERT_OK_OR_GOTO(status, status, pmuRpcPerfVfeEquMonitorSet_exit);

pmuRpcPerfVfeEquMonitorSet_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief   Exelwtes generic VFE_EQU_MONITOR_GET RPC.
 *
 * @memberof VFE_EQU_MONITOR
 * @public
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PERF_VFE_EQU_MONITOR_GET
 *
 * @return FLCN_OK upon success.
 * @return FLCN_ERR_ILWALID_ARGUMENT if the equations specified are not present.
 * @return FLCN_ERR_ILWALID_INDEX if any invalid index is encountered.
 * @return Error codes returned by called functions.
 */
FLCN_STATUS
pmuRpcPerfVfeEquMonitorGet
(
    RM_PMU_RPC_STRUCT_PERF_VFE_EQU_MONITOR_GET *pParams
)
{
    FLCN_STATUS     status = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_VFE(BASIC)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        BOARDOBJGRPMASK_E32 mask;
        LwBoardObjIdx       i;

        // Request must be a subset of tracked monitors.
        boardObjGrpMaskInit_E32(&mask);

        PMU_HALT_OK_OR_GOTO(status,
            boardObjGrpMaskImport_E32(&mask, (LW2080_CTRL_BOARDOBJGRP_MASK_E32 *)&pParams->equMonitorMask),
            pmuRpcPerfVfeEquMonitorGet_exit);

        if (!boardObjGrpMaskIsSubset(&mask, &(PERF_GET_VFE(&Perf)->pRmEquMonitor->equMonitorMask)))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto pmuRpcPerfVfeEquMonitorGet_exit;
        }

        // Copy-out most recently cached data (within timer callback).
        BOARDOBJGRPMASK_FOR_EACH_BEGIN(&mask, i)
        {
            if (i >= RM_PMU_PERF_VFE_EQU_MONITOR_COUNT_MAX)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto pmuRpcPerfVfeEquMonitorGet_exit;
            }

            PMU_HALT_OK_OR_GOTO(status,
                vfeEquMonitorResultGet(
                    PERF_GET_VFE(&Perf)->pRmEquMonitor,
                    BOARDOBJ_GRP_IDX_TO_8BIT(i),
                    &pParams->result[i]),
                pmuRpcPerfVfeEquMonitorGet_exit);
        }
        BOARDOBJGRPMASK_FOR_EACH_END;
    }

pmuRpcPerfVfeEquMonitorGet_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/* ------------------------ Private Functions ------------------------------- */
