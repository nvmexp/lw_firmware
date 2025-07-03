/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseq.c
 * @copydoc changeseq.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"
#include "pmu/ssurface.h"
#include "dmemovl.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "task_perf_daemon.h"
#include "task_perf_cf.h"
#include "pmu_objclk.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"
#include "dev_pwr_csb.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Export a PERF_CHANGE_SEQ_CHANGE_CLIENT_QUEUE item to an
 * LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_CLIENT_QUEUE item.
 *
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[out]  _pDst   Pointer to LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_CLIENT_QUEUE
 *                      item.
 * @param[in]   _pSrc   Pointer to PERF_CHANGE_SEQ_CHANGE_CLIENT_QUEUE item to
 *                      export.
 */
#define PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE_EXPORT(_pDst, _pSrc)                       \
do {                                                                                        \
    LwU8 _i;                                                                                \
    (_pDst)->state        = (_pSrc)->state;                                                 \
    (_pDst)->profiling    = (_pSrc)->profiling;                                             \
    (_pDst)->bLockWaiting = (_pSrc)->bLockWaiting;                                          \
    (_pDst)->seqIdCounter = (_pSrc)->seqIdCounter;                                          \
    for (_i = 0; _i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; ++_i)             \
    {                                                                                       \
        (_pDst)->syncChangeQueue[_i].seqId    = (_pSrc)->syncChangeQueue[_i].seqId;         \
        (_pDst)->syncChangeQueue[_i].clientId = (_pSrc)->syncChangeQueue[_i].clientId;      \
    }                                                                                       \
} while (LW_FALSE)

/*!
 * @brief Queries if the change sequencer daemon is ready to accept a change
 * request to be processed.
 *
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param  pChangeSeq  The change sequencer to query.
 *
 * @return LW_TRUE if the change sequencer daemon is ready to accept a request.
 * @return LW_FALSE if the change sequencer daemon is not ready to accept a
 *         request.
 */
#define perfChangeSeqIsDaemonReady(pChangeSeq) \
    (((pChangeSeq)->pmu.state == LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE) || \
     ((pChangeSeq)->pmu.state == LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_WAITING))


/* ------------------------ Function Prototypes ----------------------------- */
static FLCN_STATUS s_perfChangeSeqColwertToChangeSeqChange(CHANGE_SEQ *pChangeSeq, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeNext, LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT *pChangeInput)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "s_perfChangeSeqColwertToChangeSeqChange");
static void        s_perfChangeSeqTrimVoltOffsetValues(CHANGE_SEQ *pChangeSeq, VOLT_RAIL_OFFSET_RANGE_LIST *pVoltRangeList, LwU8 railIdx, LwU8 ctrlId)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "s_perfChangeSeqTrimVoltOffsetValues");
static FLCN_STATUS s_perfChangeSeqSendChangeCompletionToClients(CHANGE_SEQ *pChangeSeq)
    GCC_ATTRIB_SECTION("imem_libChangeSeq", "s_perfChangeSeqSendChangeCompletionToClients");

/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global variables Defines   ---------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief PERF RPC interface for change sequence INFO_GET.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[out]  pParams  RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_INFO_GET pointer
 *
 * @return FLCN_OK if the the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
pmuRpcPerfChangeSeqInfoGet
(
    RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_INFO_GET *pParams
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList[] = {
        CHANGE_SEQ_OVERLAYS_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Execute perf change sequence object interface.
        status = perfChangeSeqInfoGet(&pParams->infoGet);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief PERF RPC interface for change sequence INFO_SET.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]  pParams  RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_INFO_SET pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
pmuRpcPerfChangeSeqInfoSet
(
    RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_INFO_SET *pParams
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList[] = {
        CHANGE_SEQ_OVERLAYS_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Execute perf change sequence object interface.
        status = perfChangeSeqInfoSet(&pParams->infoSet);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief PERF RPC interface for change sequence SET_CONTROL.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]  pParams  RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_SET_CONTROL pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
pmuRpcPerfChangeSeqSetControl
(
    RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_SET_CONTROL *pParams
)
{
    RM_PMU_PERF_CHANGE_SEQ_SET_CONTROL *pChangeSeqControl = &pParams->control;
    CHANGE_SEQ     *pChangeSeq = PERF_CHANGE_SEQ_GET();
    FLCN_STATUS     status     = FLCN_ERR_ILWALID_STATE;
    OSTASK_OVL_DESC ovlDescList[] = {
        CHANGE_SEQ_OVERLAYS_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Client must lock before calling the SET CONTROL RPC.
        if (pChangeSeq->pmu.state != LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pmuRpcPerfChangeSeqSetControl_exit;
        }

        pChangeSeq->stepIdExclusionMask = pChangeSeqControl->stepIdExclusionMask;

        boardObjGrpMaskInit_E32(&(pChangeSeq->clkDomainsExclusionMask));
        status = boardObjGrpMaskImport_E32(&(pChangeSeq->clkDomainsExclusionMask),
                    &(pChangeSeqControl->clkDomainsExclusionMask));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pmuRpcPerfChangeSeqSetControl_exit;
        }

        boardObjGrpMaskInit_E32(&(pChangeSeq->clkDomainsInclusionMask));
        status = boardObjGrpMaskImport_E32(&(pChangeSeq->clkDomainsInclusionMask),
                    &(pChangeSeqControl->clkDomainsInclusionMask));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pmuRpcPerfChangeSeqSetControl_exit;
        }

pmuRpcPerfChangeSeqSetControl_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief PERF RPC interface to queue new perf change request
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]  pParams  RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_INFO_SET pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
pmuRpcPerfChangeSeqQueueChange
(
    RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_QUEUE_CHANGE *pParams
)
{
    FLCN_STATUS status;
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE changeRequest = { 0 };
    OSTASK_OVL_DESC ovlDescList[] = {
        CHANGE_SEQ_OVERLAYS_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Execute perf change sequence object interface.
        changeRequest.clientId = LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_RM;
        changeRequest.seqId    = pParams->seqId;
        status = perfChangeSeqQueueChange(&pParams->change, &changeRequest);
        if (status == FLCN_OK)
        {
            pParams->seqId = changeRequest.seqId;
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief PERF RPC interface to lock/unlock perf change sequence
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]  pParams  RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_LOCK pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
pmuRpcPerfChangeSeqLock
(
    RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_LOCK *pParams
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList[] = {
        CHANGE_SEQ_OVERLAYS_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Execute perf change sequence object interface.
        status = perfChangeSeqLock(&pParams->lock);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief PERF RPC interface to lock/unlock perf change sequence
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]   pParams  RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_QUERY pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
pmuRpcPerfChangeSeqQuery
(
    RM_PMU_RPC_STRUCT_PERF_CHANGE_SEQ_QUERY *pParams,
    PMU_DMEM_BUFFER                         *pBuffer
)
{
    FLCN_STATUS     status;
    LwU32           maxDmemSize;
    OSTASK_OVL_DESC ovlDescList[] = {
        CHANGE_SEQ_OVERLAYS_BASE
    };

    // Keep in sync with the RM!!!
    maxDmemSize =
        sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED);
    maxDmemSize =
        LW_MAX(maxDmemSize,
               sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ALIGNED));
    maxDmemSize =
        LW_MAX(maxDmemSize,
               sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));

#if PMUCFG_FEATURE_ENABLED(PMU_FBQ) && PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
    if (pBuffer->size < maxDmemSize)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto pmuRpcPerfChangeSeqQuery_exit;
    }
#endif

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Execute perf change sequence object interface.
        status = perfChangeSeqQuery(&(pParams->query), pBuffer->pBuf);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

pmuRpcPerfChangeSeqQuery_exit:
    return status;
}

/*!
 * @brief Construct the perf change sequence object.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqConstruct(void)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_35))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfDaemon)
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqClient)
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqChangeNext)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Construct the perf change sequence object
            status = perfChangeSeqConstruct_35(&Perf.changeSeqs.pChangeSeq,
                        sizeof(CHANGE_SEQ_35),
                        OVL_INDEX_DMEM(perfDaemon));
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqConstruct_exit;
        }
    }

perfChangeSeqConstruct_exit:
    return status;
}

/*!
 * @brief   Copy out perf change sequence object information to RM buffer.
 *
 * RM will use this information to validate the RM/PMU version and update the RM
 * perf change sequence object based on information received from PMU.
 *
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[out]  pInfo       RM_PMU_PERF_CHANGE_SEQ_INFO_GET pointer
 *
 * @return FLCN_OK if the change sequencer object was read
 * @return Detailed error code on error
 */
FLCN_STATUS
perfChangeSeqInfoGet
(
    RM_PMU_PERF_CHANGE_SEQ_INFO_GET *pInfo
)
{
    CHANGE_SEQ *pChangeSeq = PERF_CHANGE_SEQ_GET();
    FLCN_STATUS status     = FLCN_ERR_NOT_SUPPORTED;

    // Sanity check
    if (pChangeSeq == NULL)
    {
        goto perfChangeSeqInfoGet_exit;
    }

    switch (pChangeSeq->version)
    {
        case LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_35))
            {
                status = perfChangeSeqInfoGet_35(pChangeSeq, pInfo);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

perfChangeSeqInfoGet_exit:

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief   Copy in perf change sequence object information send from RM/CPU.
 *
 * PMU will use this information to update the assumption made about RM/CPU
 * change sequence model. PMU will also initialize itself with the new
 * information received from RM about the FB surface.
 *
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]   pInfo       RM_PMU_PERF_CHANGE_SEQ_INFO_SET pointer
 *
 * @return FLCN_OK if the change sequencer object info was set
 * @return Detailed error code on error
 */
FLCN_STATUS
perfChangeSeqInfoSet
(
    RM_PMU_PERF_CHANGE_SEQ_INFO_SET *pInfo
)
{
    CHANGE_SEQ *pChangeSeq = PERF_CHANGE_SEQ_GET();
    FLCN_STATUS status     = FLCN_ERR_NOT_SUPPORTED;

    // Sanity check
    if (pChangeSeq == NULL)
    {
        goto perfChangeSeqInfoSet_exit;
    }

    switch (pChangeSeq->version)
    {
        case LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_35))
            {
                status = perfChangeSeqInfoSet_35(pChangeSeq, pInfo);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

perfChangeSeqInfoSet_exit:

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief   Queue up a potential perf change. If perf change sequence is idle,
 *          start exelwtion of pending perf change requests in the queue.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @note    This interface will continue accepting the input change request
 *          and updating the queue while change sequencer is not idle.
 *
 * @param[in]   pChangeInput   Pointer to buffer containing the perf change
 *                             that the client wants to queue.
 * @param[out]  pChangeRequest Pointer to structure containing additional
 *                             change request information.
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqQueueChange
(
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT   *pChangeInput,
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE   *pChangeRequest
)
{
    CHANGE_SEQ                         *pChangeSeq  = PERF_CHANGE_SEQ_GET();
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeNext = NULL;
    LwBoardObjIdx                       i;
    FLCN_STATUS                         status      = FLCN_ERR_ILWALID_STATE;

    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libPerfLimit)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, perfIlwalidation)
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqChangeNext)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Sanity check
        if ((pChangeSeq == NULL) ||
            (pChangeInput == NULL) ||
            (pChangeRequest == NULL))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto perfChangeSeqQueueChange_exit;
        }

        if (pChangeInput->pstateIndex == LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfChangeSeqQueueChange_exit;
        }

        //
        // Increment the sequence id counter and update the seqId with latest
        // counter value.
        //
        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_INCREMENT(
            &pChangeSeq->pmu.seqIdCounter);

        // Update the SYNC request queue with seq ID and callback handler.
        if ((pChangeInput->flags & LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC) == 0U)
        {
            status = perfChangeSeqSyncChangeQueueEnqueue(pChangeSeq, pChangeRequest);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfChangeSeqQueueChange_exit;
            }
        }

        // Update the time stamp if provided and queue the request.
        if (!LwU64_ALIGN32_IS_ZERO(&pChangeRequest->timeStart.parts))
        {
            for (i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_PMU_CHANGE_PROFILING_QUEUE_SIZE; i++)
            {
                if (pChangeSeq->pmu.profiling.queue[i].seqId ==
                        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_ILWALID)
                {
                     pChangeSeq->pmu.profiling.queue[i].seqId    =
                        pChangeSeq->pmu.seqIdCounter;
                    pChangeSeq->pmu.profiling.queue[i].timeStart =
                        pChangeRequest->timeStart.parts;
                     break;
                }
            }

            //
            // If we hit this scenario, SW will skip measureing the timing for new input
            // changes untill there is more space to enqueue. We do not want to increase
            // the queue size due to DMEM impact.
            //

            // if (i == LW2080_CTRL_PERF_CHANGE_SEQ_PMU_CHANGE_PROFILING_QUEUE_SIZE)
            // {
            //     // Increase the queue size.
            //     status = FLCN_ERR_ILWALID_STATE;
            //     PMU_BREAKPOINT();
            //     goto perfChangeSeqQueueChange_exit;
            // }
        }

        pChangeNext = pChangeSeq->pChangeNext;

        // Update the perf change sequence queue
        if ((pChangeInput->flags & LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE) != 0U)
        {

            //
            // PP-TODO
            // bug # 200568242 - Temporarily disable the PMU assert until we fix this.
            //
            // Ensure that we have space to inject the force change request.
            // if (PERF_CHANGE_SEQ_IS_CHANGE_VALID(pChangeSeq->pChangeForce))
            // {
            //     status = FLCN_ERR_ILLEGAL_OPERATION;
            //     PMU_BREAKPOINT();
            //     goto perfChangeSeqQueueChange_exit;
            // }

            pChangeNext = pChangeSeq->pChangeForce;
            PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, pChangeSeq->pChangeNext);
        }

        // Update the sequence id info.
        pChangeRequest->seqId       = pChangeSeq->pmu.seqIdCounter;
        pChangeNext->data.pmu.seqId = pChangeRequest->seqId;

        status = s_perfChangeSeqColwertToChangeSeqChange(pChangeSeq, pChangeNext, pChangeInput);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqQueueChange_exit;
        }

        // Start processing pending perf change request
        if (perfChangeSeqIsDaemonReady(pChangeSeq))
        {
            status = perfChangeSeqProcessPendingChange(pChangeSeq);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfChangeSeqQueueChange_exit;
            }
        }

perfChangeSeqQueueChange_exit:

        if ((status != FLCN_OK) &&
            (pChangeNext != NULL))
        {
            PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, pChangeNext);
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief   Queue up a potential volt offset request coming from controllers
 *          like CLFC and CLVC.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @note    This interface will continue accepting the input volt offset request
 *          and updating the queue even if change sequencer is not idle.
 *
 * @param[in]   pInputVoltOffset  Pointer to buffer containing the volt offsets
 *                                that the controller wants to queue.
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqQueueVoltOffset
(
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT_VOLT_OFFSET *pInputVoltOffset
)
{
    FLCN_STATUS status      = FLCN_ERR_ILWALID_STATE;

    // PP-TODO : Move this code to new changeseq_volt_offset.c file
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_VOLT_OFFSET_CHANGE))
    {
        CHANGE_SEQ                         *pChangeSeq      = PERF_CHANGE_SEQ_GET();
        BOARDOBJGRP_E32                    *pBoardObjGrpE32 = &(VOLT_RAILS_GET()->super);
        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeNext     = NULL;
        VOLT_RAIL                          *pVoltRail;
        BOARDOBJGRPMASK_E32                 tmpVoltRailsMask;
        LwBoardObjIdx                       i;

        // Attach overlays
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libClkVoltController)
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqChangeNext)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Sanity check
            if ((pChangeSeq       == NULL) ||
                (pInputVoltOffset == NULL))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_NOT_SUPPORTED;
                goto perfChangeSeqQueueVoltOffset_exit;
            }

            // Update the cached copy of adjusted input volt offset.

            // 1. Import the volt rail mask.
            boardObjGrpMaskInit_E32(&tmpVoltRailsMask);
            status = boardObjGrpMaskImport_E32(&tmpVoltRailsMask,
                        &(pInputVoltOffset->voltRailsMask));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfChangeSeqQueueVoltOffset_exit;
            }

            // 2. Sanity check that the input volt rail mask is subset of VOLT_RAIL mask.
            if (!boardObjGrpMaskIsSubset(&tmpVoltRailsMask, &(pBoardObjGrpE32->objMask)))
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto perfChangeSeqQueueVoltOffset_exit;
            }
            pChangeSeq->voltOffsetCached.voltRailsMask =
                pInputVoltOffset->voltRailsMask;

            // 3. Update the rail specific voltage offset params.
            BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pVoltRail, i, &tmpVoltRailsMask.super)
            {
                LwU8 idx;

                FOR_EACH_INDEX_IN_MASK(32, idx, pInputVoltOffset->rails[i].offsetMask)
                {
                    // Sanity check
                    PMU_ASSERT_TRUE_OR_GOTO(status,
                        (idx < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX),
                        FLCN_ERR_ILWALID_ARGUMENT,
                        perfChangeSeqQueueVoltOffset_exit);

                    if (pInputVoltOffset->bOverrideVoltOffset)
                    {
                        pChangeSeq->voltOffsetCached.rails[i].voltOffsetuV[idx] =
                            pInputVoltOffset->rails[i].voltOffsetuV[idx];
                    }
                    else
                    {
                        //
                        // Controllers are expected to send per sample voltage offset values.
                        // Perf Change Sequencer will accumulate these values and program them
                        // as part of perf change request.
                        //
                        pChangeSeq->voltOffsetCached.rails[i].voltOffsetuV[idx] +=
                            pInputVoltOffset->rails[i].voltOffsetuV[idx];
                    }
                }
                FOR_EACH_INDEX_IN_MASK_END;
            }
            BOARDOBJGRP_ITERATOR_END;

            // Short-circuit if force perf change is not requested.
            if (!pInputVoltOffset->bForceChange)
            {
                goto perfChangeSeqQueueVoltOffset_exit;
            }

            // If there is NO pending change in queue, create one.
            if ((!PERF_CHANGE_SEQ_IS_CHANGE_VALID(pChangeSeq->pChangeNext)) &&
                (!PERF_CHANGE_SEQ_IS_CHANGE_VALID(pChangeSeq->pChangeForce)))
            {
                // If in-progress, copy in the current change.
                if (pChangeSeq->pmu.state == LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS)
                {
                    *(pChangeSeq->pChangeNext) = *(pChangeSeq->pChangeLwrr);
                }
                // Copy in the last change.
                else
                {
                    *(pChangeSeq->pChangeNext) = *(pChangeSeq->pChangeLast);
                }
                pChangeNext = pChangeSeq->pChangeNext;

                // Set async
                pChangeNext->flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC;

                //
                // Increment the sequence id counter and update the seqId with latest
                // counter value.
                //
                LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_INCREMENT(
                    &pChangeSeq->pmu.seqIdCounter);

                // Update the sequence id info.
                pChangeNext->data.pmu.seqId = pChangeSeq->pmu.seqIdCounter;
            }

            // Start processing pending perf change request
            if (perfChangeSeqIsDaemonReady(pChangeSeq))
            {
                status = perfChangeSeqProcessPendingChange(pChangeSeq);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto perfChangeSeqQueueVoltOffset_exit;
                }
            }

perfChangeSeqQueueVoltOffset_exit:
            lwosNOP();
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

    return status;
}

/*!
 * @brief   Queue up a potential memory tuning request coming from controllers
 *          like PERF_CF_CONTROLLER_MEM_TUNE.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @note    This interface will continue accepting the input memory tuning request
 *          and updating the queue even if change sequencer is not idle.
 *
 * @param[in]   tFAW            Current tFAW value to be programmed in HW
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqQueueMemTuneChange
(
    LwU8    tFAW
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // PP-TODO : Move this code to new changeseq_mem_tune.c file
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE))
    {
        CHANGE_SEQ                         *pChangeSeq  = PERF_CHANGE_SEQ_GET();
        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeNext = NULL;

        // Init the status
        status = FLCN_OK;

        // Sanity check
        if (pChangeSeq == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto perfChangeSeqQueueMemTuneChange_exit;
        }

        // Update the cached copy of adjusted input volt offset.
        pChangeSeq->tFAW = tFAW;

        // If there is NO pending change in queue, create one.
        if ((!PERF_CHANGE_SEQ_IS_CHANGE_VALID(pChangeSeq->pChangeNext)) &&
            (!PERF_CHANGE_SEQ_IS_CHANGE_VALID(pChangeSeq->pChangeForce)))
        {
            // If in-progress, copy in the current change.
            if (pChangeSeq->pmu.state == LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS)
            {
                *(pChangeSeq->pChangeNext) = *(pChangeSeq->pChangeLwrr);
            }
            // Copy in the last change.
            else
            {
                *(pChangeSeq->pChangeNext) = *(pChangeSeq->pChangeLast);
            }
            pChangeNext = pChangeSeq->pChangeNext;

            // Set async
            pChangeNext->flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC;

            //
            // Increment the sequence id counter and update the seqId with latest
            // counter value.
            //
            LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_INCREMENT(
                &pChangeSeq->pmu.seqIdCounter);

            // Update the sequence id info.
            pChangeNext->data.pmu.seqId = pChangeSeq->pmu.seqIdCounter;
        }

        // Start processing pending perf change request
        if (perfChangeSeqIsDaemonReady(pChangeSeq))
        {
            status = perfChangeSeqProcessPendingChange(pChangeSeq);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfChangeSeqQueueMemTuneChange_exit;
            }
        }

    perfChangeSeqQueueMemTuneChange_exit:
        lwosNOP();
    }

    return status;
}

/*!
 * @brief   Helper interface to send completion signal for cases where arbiter
 *          determines that the actual perf change is not required.
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]     bSync           Specifies if change request is synchronous
 * @param[inout]  pChangeRequest  Pointer to structure containing additional
 *                                change request information.
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqSendCompletionNotification
(
    LwBool                                      bSync,
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE   *pChangeRequest
)
{
    CHANGE_SEQ *pChangeSeq  = PERF_CHANGE_SEQ_GET();
    FLCN_STATUS status      = FLCN_OK;

    // Sanity check
    if ((pChangeSeq == NULL) ||
        (pChangeRequest == NULL))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }

    //
    // If the request is ASYNC, just update the seqId and return.
    // If the request is SYNC, check if the required change status.
    //      If change is already completed, send completion signal.
    //      If change is in-progress, update the sync queue without
    //          incrementing the sequence id.
    //
    if (bSync)
    {
        if (LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_IS_COMPLETED(
                pChangeSeq->pChangeLast->data.pmu.seqId,
                pChangeSeq->pmu.seqIdCounter))
        {
            if (pChangeRequest->clientId ==
                    LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU)
            {
                PERF_CHANGE_SEQ_PMU_CLIENT_COMPLETE_NOTIFICATION notification;

                if (pChangeRequest->data.pmu.queueHandle == NULL)
                {
                    status = FLCN_ERR_ILWALID_ARGUMENT;
                    PMU_BREAKPOINT();
                    goto perfChangeSeqSendCompletionNotification_exit;
                }

                notification.seqId = pChangeSeq->pChangeLwrr->data.pmu.seqId;
                status = lwrtosQueueSendBlocking(pChangeRequest->data.pmu.queueHandle,
                    &notification, sizeof(notification));
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto perfChangeSeqSendCompletionNotification_exit;
                }
            }
        }
        else
        {
            status = perfChangeSeqSyncChangeQueueEnqueue(pChangeSeq, pChangeRequest);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfChangeSeqSendCompletionNotification_exit;
            }
        }
    }

    pChangeRequest->seqId = pChangeSeq->pmu.seqIdCounter;

perfChangeSeqSendCompletionNotification_exit:
    return status;
}

/*!
 * @copydoc PerfChangeSeqLock()
 * @memberof CHANGE_SEQ
 * @private
 */
FLCN_STATUS
perfChangeSeqLock
(
    RM_PMU_PERF_CHANGE_SEQ_LOCK *pLock
)
{
    CHANGE_SEQ *pChangeSeq = PERF_CHANGE_SEQ_GET();
    FLCN_STATUS status     = FLCN_OK;

    // Sanity check
    if (pChangeSeq == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
        goto perfChangeSeqLock_exit;
    }

    // Check the input request to lock/unlock
    if (pLock->pmu.bLock)
    {
        // Sanity check for redundant lock request
        if (pChangeSeq->pmu.state ==
                LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto perfChangeSeqLock_exit;
        }

        // If change sequence state is in progress, set the lock waiting flag.
        if ((pChangeSeq->pmu.state ==
                LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS) ||
            (pChangeSeq->pmu.state ==
                LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_ERROR))
        {
            //
            // When lock becomes available, all the clients in clientLockMask
            // will get the async message about the availability of lock.
            // @note This is single write but multiple simultaneous read lock.
            //
            pChangeSeq->pmu.bLockWaiting = LW_TRUE;
            status = FLCN_ERR_LOCK_NOT_AVAILABLE;
        }
        else if (pChangeSeq->pmu.state ==
                LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_WAITING)
        {
            LwU8    i;
            LwBool  bPendingSyncChange = LW_FALSE;

            // Check whether there is a pending sync change.
            for (i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; i++)
            {
                if (pChangeSeq->pmu.syncChangeQueue[i].clientId !=
                    LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID)
                {
                    bPendingSyncChange = LW_TRUE;
                    break;
                }
            }

            // If there is a pending sync change, do not provide lock.
            if (bPendingSyncChange)
            {
                pChangeSeq->pmu.bLockWaiting = LW_TRUE;
                status = FLCN_ERR_LOCK_NOT_AVAILABLE;
            }
            else
            {
                pChangeSeq->pmu.state =
                    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED;
            }
        }
        // If change sequence is idle, update the state to LOCKED.
        else if (pChangeSeq->pmu.state ==
                LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE)
        {
            pChangeSeq->pmu.state =
                LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED;
        }
        // ALL cases must be covered above.
        else
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto perfChangeSeqLock_exit;
        }
    }
    else
    {
        // Attach overlays
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqChangeNext)
        };

        // Sanity check for redundant unlock request
        if (pChangeSeq->pmu.state !=
                LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto perfChangeSeqLock_exit;
        }

        // Unlock the perf change sequence by updating the state to NONE.
        pChangeSeq->pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;

        // Attach overlay.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Request to process any pending change, if available
            status = perfChangeSeqProcessPendingChange(pChangeSeq);
        }
        // Detach overlays
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqLock_exit;
        }

    }

perfChangeSeqLock_exit:
    return status;
}

/*!
 * @copydoc PerfChangeSeqLoad()
 * @memberof CHANGE_SEQ
 * @public
 */
FLCN_STATUS
perfChangeSeqLoad
(
    LwBool  bLoad
)
{
    CHANGE_SEQ         *pChangeSeq = PERF_CHANGE_SEQ_GET();
    CHANGE_SEQ_SCRIPT  *pScript    = NULL;
    CLK_DOMAIN         *pDomain;
    VOLT_RAIL          *pRail;
    PSTATE             *pPstate;
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY
                        pstateClkEntry;
    LwBoardObjIdx       idx;
    FLCN_STATUS         status        = FLCN_OK;
    OSTASK_OVL_DESC     ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfClkAvfs)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Sanity check
        if (pChangeSeq == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NOT_SUPPORTED;
            goto perfChangeSeqLoad_exit;
        }
        pScript = &pChangeSeq->script;

        // Sanity check.
        if ((pChangeSeq->pmu.state != LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED) &&
            (pChangeSeq->pmu.state != LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto perfChangeSeqLoad_exit;
        }

        // Load perf change seq specific param.
        if (bLoad)
        {
            // Init the sequence id to invalid
            pChangeSeq->pChangeLast->data.pmu.seqId =
                    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_ILWALID;

            // Init the flgs to ZERO
            pChangeSeq->pChangeLast->flags = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_NONE;

            // Update the version.
            pChangeSeq->pChangeLast->version = pChangeSeq->version;

            // Get the boot pstate.
            pPstate = PSTATE_GET(perfPstatesBootPstateIdxGet());
            if (pPstate == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto perfChangeSeqLoad_exit;
            }
            pChangeSeq->pChangeLast->pstateIndex = perfPstatesBootPstateIdxGet();

            // Build the default clock list from boot pstate.
            BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, idx, NULL)
            {
                CLK_DOMAIN_3X *pDomain3x = (CLK_DOMAIN_3X *)pDomain;

                // Skip fixed clock domains.
                if (clkDomainIsFixed(pDomain))
                {
                    //
                    // SW is still using the VBIOS position and BOARDOBJ masks,
                    // so the count is equal to one plus the last bit set in mask.
                    //
                    pChangeSeq->pChangeLast->clkList.numDomains++;
                    continue;
                }

                // Get the PSTATE Frequency tuple.
                status = perfPstateClkFreqGet(pPstate,
                            BOARDOBJ_GRP_IDX_TO_8BIT(idx), &pstateClkEntry);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto perfChangeSeqLoad_exit;
                }

                pChangeSeq->pChangeLast->clkList.clkDomains[idx].clkDomain  =
                    clkDomainApiDomainGet(pDomain);
                pChangeSeq->pChangeLast->clkList.clkDomains[idx].clkFreqKHz =
                    pstateClkEntry.nom.freqkHz;

                if (pDomain3x->bNoiseAwareCapable)
                {
                    // VBIOS always boots in FFR.
                    pChangeSeq->pChangeLast->clkList.clkDomains[idx].regimeId =
                        LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
                }

                pChangeSeq->pChangeLast->clkList.numDomains++;
            }
            BOARDOBJGRP_ITERATOR_END;

            // Build the default volt list.
            BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, idx, NULL)
            {
                LwU32 voltageuV;

                status = voltRailGetVbiosBootVoltage(pRail, &voltageuV);
                if ((status != FLCN_OK) ||
                    (voltageuV == RM_PMU_VOLT_VALUE_0V_IN_UV))
                {
                    PMU_BREAKPOINT();
                    goto perfChangeSeqLoad_exit;
                }
                pChangeSeq->pChangeLast->voltList.rails[idx].railIdx    = idx;
                pChangeSeq->pChangeLast->voltList.rails[idx].railAction =
                    LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH;
                pChangeSeq->pChangeLast->voltList.rails[idx].voltageuV  = voltageuV;
                pChangeSeq->pChangeLast->voltList.rails[idx].voltageMinNoiseUnawareuV =
                    voltageuV;
                pChangeSeq->pChangeLast->voltList.numRails++;
            }
            BOARDOBJGRP_ITERATOR_END;

            // Update the change struct
            status = ssurfaceWr(pChangeSeq->pChangeLast,                                                // source
                CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLast, change),    // offset
                pChangeSeq->changeSize);                                                                // size
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfChangeSeqLoad_exit;
            }

            // Build perf change sequence LPWR script buffer.
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR))
            {
                OSTASK_OVL_DESC lpwrOvlDescList[] = {
                    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqLpwr)
                };

                OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(lpwrOvlDescList);
                status = perfChangeSeqLpwrScriptBuild(pChangeSeq);
                OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(lpwrOvlDescList);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto perfChangeSeqLoad_exit;
                }
            }

            // PP-TODO : Move this code to correct file and codebase.
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE))
            {
                 //
                // GC6 cycles will reset this value. Init the value to
                // default programmed value in FBFLCN.
                //
                clkFbflcnTrrdValResetSet(clkFbflcnTrrdGet_HAL(&Clk));
            }
        }

perfChangeSeqLoad_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc PerfChangeSeqQuery()
 * @memberof CHANGE_SEQ
 * @private
 */
FLCN_STATUS
perfChangeSeqQuery
(
    RM_PMU_PERF_CHANGE_SEQ_QUERY   *pQuery,
    void                           *pBuf
)
{
    RM_PMU_PERF_CHANGE_SEQ_QUERY_SUPER *pQuerySuper = &pQuery->super;
    CHANGE_SEQ         *pChangeSeq = PERF_CHANGE_SEQ_GET();
    CHANGE_SEQ_SCRIPT  *pScript    = NULL;
    FLCN_STATUS         status     = FLCN_OK;
    LwU8                numSteps;
    LwU8                i;

    // Sanity check
    if ((pChangeSeq == NULL) ||
        (pQuerySuper == NULL)||
        (pBuf == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_NOT_SUPPORTED;
        goto perfChangeSeqQuery_exit;
    }
    pScript = &pChangeSeq->script;

    // Copy-out the header struct of the last completed script.
    status = ssurfaceRd(pBuf,                                                               // destination
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLast, hdr),   // offset
        sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED));                     // size
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqQuery_exit;
    }

    numSteps = ((LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER *)pBuf)->numSteps;

    // Sanity check the index of script step
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (numSteps > LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto perfChangeSeqQuery_exit;
        }
    }

    status = ssurfaceWr(pBuf,                                                               // source
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetQuery, hdr),  // offset
        sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED));                     // size
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqQuery_exit;
    }

    // Copy-out the dynamic status params.
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE_EXPORT(&pQuerySuper->pmu.data, &pChangeSeq->pmu);

    // Copy-out the last completed script.

    // Copy-out the change struct
    status = ssurfaceRd(pBuf,                                                                   // destination
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLast, change),    // offset
        pChangeSeq->changeSize);                                                                // size
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqQuery_exit;
    }

    status = ssurfaceWr(pBuf,                                                                   // source
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetQuery, change),   // offset
        pChangeSeq->changeSize);                                                                // size
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqQuery_exit;
    }

    // Update all the valid steps struct
    for (i = 0; i < numSteps; i++)
    {
        // Read the step from latest completed script
        status = ssurfaceRd(pBuf,                                                                   // destination
            CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLast, steps[i]),  // offset
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));                      // size
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqQuery_exit;
        }

        // Update the last script with latest step values.
        status = ssurfaceWr(pBuf,                                                                   // source
            CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetQuery, steps[i]), // offset
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));                      // size
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqQuery_exit;
        }
    }

perfChangeSeqQuery_exit:
    return status;
}

/*!
 * @brief   Perf change sequence entry point interface.
 *
 * Perf Daemon task will call this interface upon completion of the perf change
 * seq script.
 *
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]   status    Status of the last perf change sequence script
 */
void
perfChangeSeqScriptCompletion
(
    FLCN_STATUS completionStatus
)
{
    CHANGE_SEQ         *pChangeSeq = PERF_CHANGE_SEQ_GET();
    CHANGE_SEQ_SCRIPT  *pScript    = &pChangeSeq->script;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER
                       *pHdr       = &pScript->hdr.data;
    CLK_DOMAIN         *pDomain    = NULL;
    LwBoardObjIdx       idx;
    FLCN_STATUS         status     = completionStatus;
    LwBool              bWriteSemaOwned = LW_FALSE;

    // Sanity check.
    if (pChangeSeq->pmu.state != LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqScriptCompletion_exit;
    }

    // Sanity check the status of perf change sequence scriptExelwte
    if (completionStatus != FLCN_OK)
    {
        // Check if change was SKIPPED. This is NOT an error.
        if (completionStatus == FLCN_WARN_NOTHING_TO_DO)
        {
            pChangeSeq->pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_WAITING;
            goto perfChangeSeqScriptCompletion_next;
        }
        else
        {
            // From this point, change sequencer is effectively hanged.
            pChangeSeq->pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_ERROR;
            PMU_BREAKPOINT();
            goto perfChangeSeqScriptCompletion_exit;
        }
    }

    //
    // Acquire PERF Write semaphore for updating the synchronized
    // "last change" structure.
    //
    perfWriteSemaphoreTake();
    bWriteSemaOwned = LW_TRUE;
    {
        // Load CLFC at the end of every perf change.
        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
        {
            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = clkFreqControllersLoad_WRAPPER(LW_TRUE);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfChangeSeqScriptCompletion_exit;
            }
        }

        // Sanity check the clock list.
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, idx, &pScript->clkDomainsActiveMask.super)
        {
            CLK_DOMAIN_3X *pDomain3x = (CLK_DOMAIN_3X *)pDomain;

            if (pDomain3x->bNoiseAwareCapable)
            {
                // VBIOS always boots in FFR.
                if (pChangeSeq->pChangeLwrr->clkList.clkDomains[idx].regimeId ==
                        LW2080_CTRL_CLK_NAFLL_REGIME_ID_ILWALID)
                {
                    PMU_BREAKPOINT();
                    goto perfChangeSeqScriptCompletion_exit;
                }
            }
        }
        BOARDOBJGRP_ITERATOR_END;

        // Stop counting build time.
        PERF_CHANGE_SEQ_PROFILE_END(&pHdr->profiling.totalTimens);

        // Update the last change with the latest values.
        *(pChangeSeq->pChangeLast) = *(pChangeSeq->pChangeLwrr);

        // Update the last script with latest completed script.
        status = perfChangeSeqCopyScriptLwrrToScriptLast(pChangeSeq,
                                                         pScript,
                                                         pChangeSeq->pChangeLwrr);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqScriptCompletion_exit;
        }

        perfChangeSeqUpdateProfilingHistogram(pChangeSeq);
    }
    //
    // Critical section of updating synchronized last change is
    // completed.  Now, can continue to client handling without
    // holding write semaphore.
    //
    perfWriteSemaphoreGive();
    bWriteSemaOwned = LW_FALSE;

    // Trigger depedencies on first perf change.
    if (!pChangeSeq->bFirstChangeCompleted)
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE))
        {
            DISPATCH_PERF_CF    disp2perfcf = {{0}};

            // Queue the perf cf request to load monitor.
            disp2perfcf.hdr.eventType =
                PERF_CF_EVENT_ID_PERF_CF_CONTROLLER_MEM_TUNE_MONITOR_LOAD;

            (void)lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF_CF),
                                            &disp2perfcf, sizeof(disp2perfcf));
        }

        pChangeSeq->bFirstChangeCompleted = LW_TRUE;
    }

    status = s_perfChangeSeqSendChangeCompletionToClients(pChangeSeq);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqScriptCompletion_exit;
    }

perfChangeSeqScriptCompletion_next:

    if (perfChangeSeqIsDaemonReady(pChangeSeq))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqChangeNext)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = perfChangeSeqProcessPendingChange(pChangeSeq);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }

perfChangeSeqScriptCompletion_exit:
    if (bWriteSemaOwned)
    {
        perfWriteSemaphoreGive();
        bWriteSemaOwned = LW_FALSE;
    }

    return;
}

/*!
 * @brief Helper interface to copy out current script to last completed script
 *        after successful script exelwtion.
 * @memberof CHANGE_SEQ
 * @protected
 *
 * @param[in]  pChangeSeq       Change Sequencer pointer.
 * @param[in]  pScript          Script buffer.
 * @param[in]  pChangeLwrr      Current change buffer
 *
 * @return FLCN_OK                  if the function completed successfully
 * @return FLCN_ERR_ILWALID_STATE   if there is a DMEM corrution.
 * @return Detailed status code on error
 *
 */
FLCN_STATUS
perfChangeSeqCopyScriptLwrrToScriptLast
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr
)
{
    LwU8        i;
    FLCN_STATUS status;

    // Sanity check the index of script step
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {

        if (pScript->hdr.data.numSteps >
            LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_VF_SWITCH_MAX_STEPS)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfChangeSeqCopyScriptLwrrToScriptLast_exit;
        }
    }

    // Update the change struct
    status = ssurfaceWr(pChangeLwrr,                                                            // source
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLast, change),    // offset
        pChangeSeq->changeSize);                                                                // size
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqCopyScriptLwrrToScriptLast_exit;
    }

    // Update all the valid steps struct
    for (i = 0; i < pScript->hdr.data.numSteps; i++)
    {

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_ODP))

        pScript->pStepLwrr = &pScript->steps[i];

#else
        // Clear the step buffer.
        (void)memset(pScript->pStepLwrr, 0,
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));

        // Read the step from latest completed script
        status = ssurfaceRd(pScript->pStepLwrr,                                                     // destination
            CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLwrr, steps[i]),  // offset
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));                      // size
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqCopyScriptLwrrToScriptLast_exit;
        }
#endif

        // Update the last script with latest step values.
        status = ssurfaceWr(pScript->pStepLwrr,                                                     // source
            CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLast, steps[i]),  // offset
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));                      // size
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqCopyScriptLwrrToScriptLast_exit;
        }
    }

    // Update the header struct
    status = ssurfaceWr(&pScript->hdr,                                                      // source
        CHANGE_SEQ_SUPER_SURFACE_OFFSET_BY_SCRIPT_OFFSET(pScript->scriptOffsetLast, hdr),   // offset
        sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED));                     // size
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqCopyScriptLwrrToScriptLast_exit;
    }

perfChangeSeqCopyScriptLwrrToScriptLast_exit:
    return status;
}

/*!
 * @copydoc PerfChangeSeqConstruct()
 * @memberof CHANGE_SEQ
 * @protected
 */
FLCN_STATUS
perfChangeSeqConstruct_SUPER_IMPL
(
    CHANGE_SEQ    **ppChangeSeq,
    LwU16           size,
    LwU8            ovlIdx
)
{
    CHANGE_SEQ *pChangeSeq = NULL;
    FLCN_STATUS status     = FLCN_OK;

    // Allocate memory for perf change sequence object
    if (*ppChangeSeq == NULL)
    {
        *ppChangeSeq = (CHANGE_SEQ *)lwosCalloc(ovlIdx, size);
        if (*ppChangeSeq == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto perfChangeSeqConstruct_SUPER_exit;
        }
    }
    pChangeSeq = (*ppChangeSeq);

    // Construct low power change sequencer interface.
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_LPWR))
    {
        status = perfChangeSeqConstruct_LPWR(pChangeSeq);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqConstruct_SUPER_exit;
        }
    }

    // Set change sequence specific parameters
    pChangeSeq->version   = LW2080_CTRL_PERF_CHANGE_SEQ_VERSION_UNKNOWN;
    pChangeSeq->pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
    boardObjGrpMaskInit_E32(&(pChangeSeq->clkDomainsExclusionMask));
    boardObjGrpMaskInit_E32(&(pChangeSeq->clkDomainsInclusionMask));
    boardObjGrpMaskInit_E32(&(pChangeSeq->script.clkDomainsActiveMask));

    // Update the expected CPU step ids mask
    pChangeSeq->cpuAdvertisedStepIdMask  =
    (
        // TODO : Remove VOLT and LPWR step once the support is enabled in PMU.
        LWBIT32(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_CHANGE_RM)      |
        LWBIT32(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_CHANGE_RM)     |
        LWBIT32(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_PRE_PSTATE_RM)      |
        LWBIT32(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_POST_PSTATE_RM)     |
#if !PMUCFG_FEATURE_ENABLED(PMU_VOLT_35)
        LWBIT32(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_VOLT)               |
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_35)
        LWBIT32(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_STEP_ID_LPWR)
    );

    // Set up VF switch script
    pChangeSeq->script.scriptOffsetLwrr  = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptLwrr);
    pChangeSeq->script.scriptOffsetLast  = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptLast);
    pChangeSeq->script.scriptOffsetQuery = RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.changeSeq.scriptQuery);

#if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_ODP))
    // Set up scratch step data pointer.
    pChangeSeq->script.pStepLwrr = &pChangeSeq->script.stepLwrr;
    memset(pChangeSeq->script.pStepLwrr, 0x00,
        sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));
#endif

    // Clear the pointers mapping.
    pChangeSeq->pChangeForce    = NULL;
    pChangeSeq->pChangeNext     = NULL;
    pChangeSeq->pChangeLast     = NULL;
    pChangeSeq->pChangeLwrr     = NULL;
    pChangeSeq->pChangeScratch  = NULL;

perfChangeSeqConstruct_SUPER_exit:
    return status;
}

/*!
 * @copydoc PerfChangeSeqInfoGet()
 * @memberof CHANGE_SEQ
 * @protected
 */
FLCN_STATUS
perfChangeSeqInfoGet_SUPER_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    RM_PMU_PERF_CHANGE_SEQ_INFO_GET    *pInfo
)
{
    RM_PMU_PERF_CHANGE_SEQ_SUPER_INFO_GET *pSuperInfo = &pInfo->super;

    pSuperInfo->version                = pChangeSeq->version;
    pSuperInfo->stepIdExclusionMask    = pChangeSeq->stepIdExclusionMask;
    pInfo->pmu.cpuAdvertisedStepIdMask =
        pChangeSeq->cpuAdvertisedStepIdMask;

    return FLCN_OK;
}

/*!
 * @copydoc PerfChangeSeqInfoSet()
 * @memberof CHANGE_SEQ
 * @protected
 */
FLCN_STATUS
perfChangeSeqInfoSet_SUPER_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    RM_PMU_PERF_CHANGE_SEQ_INFO_SET    *pInfo
)
{
    FLCN_STATUS                            status     = FLCN_OK;
    RM_PMU_PERF_CHANGE_SEQ_SUPER_INFO_SET *pSuperInfo = &pInfo->super;
    LwU8                                   i;


    if (pSuperInfo->version != pChangeSeq->version)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_VERSION;
    }

    pChangeSeq->stepIdExclusionMask = pSuperInfo->stepIdExclusionMask;

    boardObjGrpMaskInit_E32(&(pChangeSeq->clkDomainsExclusionMask));
    status = boardObjGrpMaskImport_E32(&(pChangeSeq->clkDomainsExclusionMask),
                                       &(pSuperInfo->clkDomainsExclusionMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqInfoSet_SUPER_exit;
    }

    boardObjGrpMaskInit_E32(&(pChangeSeq->clkDomainsInclusionMask));
    status = boardObjGrpMaskImport_E32(&(pChangeSeq->clkDomainsInclusionMask),
                                       &(pSuperInfo->clkDomainsInclusionMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqInfoSet_SUPER_exit;
    }

    pChangeSeq->bVfPointCheckIgnore = pInfo->pmu.bVfPointCheckIgnore;
    pChangeSeq->bSkipVoltRangeTrim  = pInfo->pmu.bSkipVoltRangeTrim;
    pChangeSeq->cpuStepIdMask       = pInfo->pmu.cpuStepIdMask;

    // Update the lock state information.
    if (pInfo->pmu.bLock)
    {
        pChangeSeq->pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED;
    }

    // Init the change profiling histogram data.
    pChangeSeq->pmu.profiling.timeMax     = LW_U32_MIN;
    pChangeSeq->pmu.profiling.timeMin     = LW_U32_MAX;
    (void)memset(&pChangeSeq->pmu.profiling.execTimeBins, 0,
        (sizeof(LwU32) * LW2080_CTRL_PERF_CHANGE_SEQ_PMU_CHANGE_PROFILING_BIN_COUNT));

    //
    // 20 bins of 2^19[ns] == ~524[us]  starting at  0.0[ms]   ending at ~10.5[ms]
    // 12 bins of 2^20[ns] == ~1048[us] starting at ~10.5[ms]  ending at ~22.6[ms]
    //
    pChangeSeq->pmu.profiling.smallBinCount     = 20;
    pChangeSeq->pmu.profiling.smallBinSizeLog2  = 19;
    pChangeSeq->pmu.profiling.bigBinSizeLog2    = 20;
    for (i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_PMU_CHANGE_PROFILING_QUEUE_SIZE; i++)
    {
        pChangeSeq->pmu.profiling.queue[i].seqId =
                LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_ILWALID;
    }

perfChangeSeqInfoSet_SUPER_exit:
    return status;
}

/*!
 * @brief Public interface to register for perf change sequencer notifications.
 *
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]  notification
 * @param[in]  pRegister
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqRegisterNotification
(
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION notification,
    PERF_CHANGE_SEQ_NOTIFICATION           *pRegister
)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_NOTIFICATION))
    CHANGE_SEQ *pChangeSeq  = PERF_CHANGE_SEQ_GET();
    FLCN_STATUS status      = FLCN_OK;
    PERF_CHANGE_SEQ_NOTIFICATIONS  *pNotifications        = NULL;
    PERF_CHANGE_SEQ_NOTIFICATION   *pNotificationInactive = NULL;

    // Sanity check
    if ((pChangeSeq == NULL) ||
        (pRegister == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfChangeSeqRegisterNotification_exit;
    }

    // Get the notifications tracking info
    pNotifications = &pChangeSeq->notifications[notification];
    if (pNotifications == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfChangeSeqRegisterNotification_exit;
    }
    pNotifications->bInSync = LW_FALSE; // Update the sync flag.
    pNotificationInactive   = pNotifications->pNotificationInactive;

    // First check to see if the callback was already registered
    while (pNotificationInactive != NULL)
    {
        if (pNotificationInactive->clientTaskInfo.taskEventId ==
            pRegister->clientTaskInfo.taskEventId)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfChangeSeqRegisterNotification_exit;
        }
        pNotificationInactive = pNotificationInactive->pNextInactive;
    }

    // Attach the input notification element to the notifications tracking list
    pRegister->pNextInactive = pNotifications->pNotificationInactive;
    pNotifications->pNotificationInactive = pRegister;

perfChangeSeqRegisterNotification_exit:
    return status;
#else
    return FLCN_ERR_NOT_SUPPORTED;
#endif
}

/*!
 * @brief Public interface to un-register for perf change sequencer notifications.
 *
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]  notification
 * @param[in]  pRegister
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqUnregisterNotification
(
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION notification,
    PERF_CHANGE_SEQ_NOTIFICATION           *pRegister
)
{
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_NOTIFICATION))
    CHANGE_SEQ *pChangeSeq  = PERF_CHANGE_SEQ_GET();
    FLCN_STATUS status      = FLCN_OK;
    PERF_CHANGE_SEQ_NOTIFICATIONS  *pNotifications            = NULL;
    PERF_CHANGE_SEQ_NOTIFICATION   *pNotificationInactive     = NULL;
    PERF_CHANGE_SEQ_NOTIFICATION   *pPrevNotificationInactive = NULL;

    // Sanity check
    if ((pChangeSeq == NULL) ||
        (pRegister == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfChangeSeqUnregisterNotification_exit;
    }

    pNotifications = &pChangeSeq->notifications[notification];
    if (pNotifications == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfChangeSeqUnregisterNotification_exit;
    }
    pNotifications->bInSync = LW_FALSE; // Update the sync flag.
    pNotificationInactive   = pNotifications->pNotificationInactive;

    // find the listener being removed
    while (pNotificationInactive != NULL)
    {
        if (pNotificationInactive->clientTaskInfo.taskEventId ==
            pRegister->clientTaskInfo.taskEventId)
        {
            break;
        }
        pPrevNotificationInactive = pNotificationInactive;
        pNotificationInactive     = pNotificationInactive->pNextInactive;
    }

    // Bail if not found or if the callback passed-in is NULL
    if (pNotificationInactive == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OBJECT_NOT_FOUND;
        goto perfChangeSeqUnregisterNotification_exit;
    }

    // Remove the requested notification element from list.
    if (pPrevNotificationInactive != NULL)
    {
        pPrevNotificationInactive->pNextInactive =
            pNotificationInactive->pNextInactive;
    }
    else
    {
        pNotifications->pNotificationInactive =
            pNotificationInactive->pNextInactive;
    }

perfChangeSeqUnregisterNotification_exit:
    return status;
#else
    return FLCN_ERR_NOT_SUPPORTED;
#endif
}

/*!
 * @brief Start processing pending perf change request.
 * @memberof CHANGE_SEQ
 * @private
 *
 * @note    This interface process only one pending change request at a time.
 *          When perf task receives a completion signal from the perf daemon
 *          task, it will check if there is any pending lock request. If there
 *          is none, it will call this interface again to start processing next
 *          pending change.
 *
 * @param[in] pChangeSeq  CHANGE_SEQ pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqProcessPendingChange_IMPL
(
    CHANGE_SEQ *pChangeSeq
)
{
    DISPATCH_PERF_DAEMON                disp2perfDaemon;
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr = pChangeSeq->pChangeLwrr;
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER
                                       *pHdr        = &pChangeSeq->script.hdr.data;
    LwU8        i;
    FLCN_STATUS status = FLCN_OK;

    // Sanity check.
    if (!perfChangeSeqIsDaemonReady(pChangeSeq))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfChangeSeqProcessPendingChange_exit;
    }

    // Give high priority to force change request to maintain the input ordering.
    if (PERF_CHANGE_SEQ_IS_CHANGE_VALID(pChangeSeq->pChangeForce))
    {
        (void)memcpy(pChangeLwrr, pChangeSeq->pChangeForce, pChangeSeq->changeSize);

        PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, pChangeSeq->pChangeForce);
    }
    else if (PERF_CHANGE_SEQ_IS_CHANGE_VALID(pChangeSeq->pChangeNext))
    {
        (void)memcpy(pChangeLwrr, pChangeSeq->pChangeNext, pChangeSeq->changeSize);

        PERF_CHANGE_SEQ_ILWALIDATE_CHANGE(pChangeSeq, pChangeSeq->pChangeNext);
    }
    // If there is no pending change, bail out.
    else
    {
        status = FLCN_OK;
        goto perfChangeSeqProcessPendingChange_exit;
    }

    // Set the perf change sequence state in progress.
    pChangeSeq->pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_IN_PROGRESS;

    // Init the header structure
    (void)memset(&pChangeSeq->script.hdr, 0,
           sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED));

    // Begin counting build time.
    PERF_CHANGE_SEQ_PROFILE_BEGIN(&pHdr->profiling.totalTimens);

    // Unload CLFC at the start of every perf change.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_CONTROLLER_V10))
    {
        OSTASK_OVL_DESC ovlDescListLocal[] = {
            OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListLocal);
        {
            status = clkFreqControllersLoad_WRAPPER(LW_FALSE);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListLocal);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqProcessPendingChange_exit;
        }
    }

    //
    // Sanity check the VF lwrve change counter to determine whether VF data
    // is stale. If so, discard the change as there will be new change in queue
    // with the updated VF lwrve.
    //
    if ((pChangeLwrr->vfPointsCacheCounter !=
            clkVfPointsVfPointsCacheCounterGet()) &&
        (pChangeLwrr->vfPointsCacheCounter !=
            LW2080_CTRL_CLK_CLK_VF_POINT_CACHE_COUNTER_TOOLS))
    {
        //
        // VF lwrve changed so there is nothing to do. Trigger the change
        // sequencer script completion. The completion interface will trigger
        // the processing of next change with updated VF lwrve.
        //
        (void)perfChangeSeqScriptCompletion(FLCN_WARN_NOTHING_TO_DO);

        // Explicitly colwert the status = OK.
        status = FLCN_OK;
        goto perfChangeSeqProcessPendingChange_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_VOLT_OFFSET_CHANGE))
    {
        // Trim voltage offsets
        status = perfChangeSeqAdjustVoltOffsetValues(pChangeSeq, pChangeLwrr);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfChangeSeqProcessPendingChange_exit;
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_MEM_TUNE_CHANGE))
    {
        // Update the latest memory tuning settings.
        pChangeLwrr->tFAW = pChangeSeq->tFAW;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_NOTIFICATION))
    {
        //
        // Update the active notification linked-list based on the latest
        // updated in-active notification linked-list.
        //
        for (i = 0; i < RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_MAX; i++)
        {
            PERF_CHANGE_SEQ_NOTIFICATIONS  *pNotifications        = NULL;
            PERF_CHANGE_SEQ_NOTIFICATION   *pNotificationInactive = NULL;

            // Get the notifications tracking info
            pNotifications = &pChangeSeq->notifications[i];
            if (pNotifications == NULL)
            {
                status = FLCN_ERR_ILWALID_ARGUMENT;
                PMU_BREAKPOINT();
                goto perfChangeSeqProcessPendingChange_exit;
            }

            if (!pNotifications->bInSync)
            {
                pNotifications->pNotification = pNotifications->pNotificationInactive;
                pNotificationInactive = pNotifications->pNotificationInactive;

                while (pNotificationInactive != NULL)
                {
                    pNotificationInactive->pNext = pNotificationInactive->pNextInactive;
                    pNotificationInactive = pNotificationInactive->pNextInactive;
                }
                pNotifications->bInSync = LW_TRUE;
            }
        }
    }

    // Queue the change request to PERF_DAEMON task
    disp2perfDaemon.hdr.eventType =
        PERF_DAEMON_EVENT_ID_PERF_CHANGE_SEQ_SCRIPT_EXELWTE;
    disp2perfDaemon.scriptExelwte.pChangeSeq = pChangeSeq;

    status = lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF_DAEMON),
                                &disp2perfDaemon, sizeof(disp2perfDaemon));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqProcessPendingChange_exit;
    }

perfChangeSeqProcessPendingChange_exit:
    return status;
}

/*!
 * @brief Helper interface to validate the input change request.
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]   pChangeSeq      CHANGE_SEQ pointer
 * @param[in]   pClkDomainsMask BOARDOBJGRPMASK_E32 pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqValidateClkDomainInputMask_IMPL
(
    CHANGE_SEQ             *pChangeSeq,
    BOARDOBJGRPMASK_E32    *pClkDomainsMask
)
{
    BOARDOBJGRPMASK_E32 tmpClkDomainsActiveMask;
    FLCN_STATUS         status  = FLCN_OK;

    // Init the active mask.
    boardObjGrpMaskInit_E32(&tmpClkDomainsActiveMask);

    // Set active mask equal to exclusion clock domain mask.
    status = boardObjGrpMaskCopy(&tmpClkDomainsActiveMask,
                                 &pChangeSeq->clkDomainsExclusionMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqValidateClkDomainInputMask_exit;
    }

    // Ilwert the active mask to clear the excluded clock domains.
    status = boardObjGrpMaskIlw(&tmpClkDomainsActiveMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqValidateClkDomainInputMask_exit;
    }

    // Clear the non-programmable clock domains.
    status = boardObjGrpMaskAnd(&tmpClkDomainsActiveMask,
                                &tmpClkDomainsActiveMask,
                                &Clk.domains.progDomainsMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqValidateClkDomainInputMask_exit;
    }

    // Include fixed clock domains on client's request.
    status = boardObjGrpMaskOr(&tmpClkDomainsActiveMask,
                               &tmpClkDomainsActiveMask,
                               &pChangeSeq->clkDomainsInclusionMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqValidateClkDomainInputMask_exit;
    }

    // Ensure that the active mask is sub-set of input clk domain mask
    if (!boardObjGrpMaskIsSubset(&tmpClkDomainsActiveMask, pClkDomainsMask))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfChangeSeqValidateClkDomainInputMask_exit;
    }

perfChangeSeqValidateClkDomainInputMask_exit:
    return status;
}

/*!
 * @brief Helper interface to insert sync change request into @ref syncChangeQueue.
 * @memberof CHANGE_SEQ
 * @private
 *
 * @param[in]   pChangeSeq      CHANGE_SEQ pointer
 * @param[in]   pChangeRequest  PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE pointer
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqSyncChangeQueueEnqueue_IMPL
(
    CHANGE_SEQ                                 *pChangeSeq,
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE   *pChangeRequest
)
{
    LwU8        i;
    FLCN_STATUS status = FLCN_OK;

    // If a PMU client, sanity check the queue/DMEM overlay
    if ((LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU == pChangeRequest->clientId) &&
        (pChangeRequest->data.pmu.queueHandle == NULL))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfChangeSeqSyncChangeQueueEnqueue_exit;
    }

    for (i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; i++)
    {
        if (pChangeSeq->pmu.syncChangeQueue[i].clientId ==
            LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID)
        {
             pChangeSeq->pmu.syncChangeQueue[i].seqId    =
                pChangeSeq->pmu.seqIdCounter;
             pChangeSeq->pmu.syncChangeQueue[i].clientId =
                pChangeRequest->clientId;

             if (LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU ==
                 pChangeRequest->clientId)
             {
                 pChangeSeq->pmu.syncChangeQueue[i].data.pmu =
                     pChangeRequest->data.pmu;
             }
             break;
        }
    }

    if (i == LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE)
    {
        // Increase the queue size. If the client is not RM, add support
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfChangeSeqSyncChangeQueueEnqueue_exit;
    }

perfChangeSeqSyncChangeQueueEnqueue_exit:
    return status;
}

/*!
 * @brief Helper interface to update the profiling histogram.
 *
 * @param[in,out]  pChangeSeq   the change sequencer
 *
 */
void
perfChangeSeqUpdateProfilingHistogram_IMPL
(
    CHANGE_SEQ *pChangeSeq
)
{
    LwU8 i;

    for (i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_PMU_CHANGE_PROFILING_QUEUE_SIZE; i++)
    {
        if ((pChangeSeq->pmu.profiling.queue[i].seqId !=
                LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_ILWALID) &&
            LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_IS_COMPLETED(
                pChangeSeq->pChangeLwrr->data.pmu.seqId,
                pChangeSeq->pmu.profiling.queue[i].seqId))
        {
            LwU32   elapsedns;
            LwU32   binID;

            // Callwlate the elapsed time.
            elapsedns = osPTimerTimeNsElapsedGetUnaligned(
                &pChangeSeq->pmu.profiling.queue[i].timeStart);

            // Update the min and max time stamp
            pChangeSeq->pmu.profiling.timeMax =
                LW_MAX(pChangeSeq->pmu.profiling.timeMax, elapsedns);
            pChangeSeq->pmu.profiling.timeMin =
                LW_MIN(pChangeSeq->pmu.profiling.timeMin, elapsedns);

            // Callwlate the bin id.
            binID = elapsedns >> pChangeSeq->pmu.profiling.smallBinSizeLog2;
            if (binID >= pChangeSeq->pmu.profiling.smallBinCount)
            {
                binID -= pChangeSeq->pmu.profiling.smallBinCount;

                binID >>= (pChangeSeq->pmu.profiling.bigBinSizeLog2 -
                            pChangeSeq->pmu.profiling.smallBinSizeLog2);

                binID += pChangeSeq->pmu.profiling.smallBinCount;

                binID = LW_MIN(binID,
                            (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_CHANGE_PROFILING_BIN_COUNT - 1U));
            }

            // Increment the bin's count.
            pChangeSeq->pmu.profiling.execTimeBins[binID] += 1U;

            // Clear the queue.
            pChangeSeq->pmu.profiling.queue[i].seqId =
                LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_ILWALID;
        }
    }
}

/*!
 * @brief Adjusts the voltage offset values.
 *
 * This adjustment must be done in Perf task as it is tracking the
 * single set of volt offset values which get adjusted on every
 * perf change inject to perf daemon task with Vmin and Vmax as well
 * as on every new incoming controller request with updated volt offset.
 *
 * @param[in,out]  pChangeSeq   the change sequencer
 * @param[in]      pChangeLwrr  the lwrrently requested values to change to
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqAdjustVoltOffsetValues
(
    CHANGE_SEQ                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr
)
{
    FLCN_STATUS                         status            = FLCN_OK;
    OSTASK_OVL_DESC                     ovlDescListVolt[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVolt)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libVoltApi)
    };
    LwU8                                idx;
    LwBoardObjIdx                       railIdx;
    VOLT_RAIL                          *pRail = NULL;
    VOLT_RAIL_OFFSET_RANGE_LIST         voltRangeList;

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListVolt);
    {
        status = voltVoltOffsetRangeGet(
            LW2080_CTRL_VOLT_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ,
            &pChangeLwrr->voltList, &voltRangeList, pChangeSeq->bSkipVoltRangeTrim);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListVolt);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqAdjustVoltOffsetValues_exit;
    }

    // Per rail voltage offset adjustments.
    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, railIdx, NULL)
    {
        //
        // CLVC ensures that the "Vsensed = Vtarget + Vclfcoff" therefore we
        // are first adjusting with the CLFC offset followed by CLVC offset.
        // Voltage Margin is adjusted at the end.
        //

        // Trim voltage offset from CLFC.
        (void)s_perfChangeSeqTrimVoltOffsetValues(pChangeSeq, &voltRangeList,
                railIdx, LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC);

        // Trim voltage offset from CLVC.
        (void)s_perfChangeSeqTrimVoltOffsetValues(pChangeSeq, &voltRangeList,
                railIdx, LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLVC);

        // Trim voltage offset from VOLT_MARGIN.
        (void)s_perfChangeSeqTrimVoltOffsetValues(pChangeSeq, &voltRangeList,
                railIdx, LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_VOLT_MARGIN);

        // Update final adjusted voltage offsets
        for (idx = 0; idx < LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_MAX; idx++)
        {
            pChangeLwrr->voltList.rails[railIdx].voltOffsetuV[idx] =
                pChangeSeq->voltOffsetCached.rails[railIdx].voltOffsetuV[idx];
        }
    }
    BOARDOBJGRP_ITERATOR_END;

perfChangeSeqAdjustVoltOffsetValues_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */

/*!
 * @brief Trim the voltage offset for a given voltage rail from given voltage
 *        controller to ensure the Vtarget with aall offset adjustments stays
 *        within [Vmin - Vmax] range bound.
 *
 * @param[in,out]  pChangeSeq      The change sequencer
 * @param[in]      pVoltRangeList  Voltage range bounds
 * @param[in]      railIdx         Voltage rail index
 * @param[in]      ctrlId          Voltage controller id.
 */
static void
s_perfChangeSeqTrimVoltOffsetValues
(
    CHANGE_SEQ                         *pChangeSeq,
    VOLT_RAIL_OFFSET_RANGE_LIST        *pVoltRangeList,
    LwU8                                railIdx,
    LwU8                                ctrlId
)
{
    // Trim controller's volt Offset to range [Vmin, Vmax]
    if (pChangeSeq->voltOffsetCached.rails[railIdx].voltOffsetuV[ctrlId] < 0)
    {
        pChangeSeq->voltOffsetCached.rails[railIdx].voltOffsetuV[ctrlId] =
            LW_MAX(pVoltRangeList->railOffsets[railIdx].voltOffsetNegativeMaxuV,
                pChangeSeq->voltOffsetCached.rails[railIdx].voltOffsetuV[ctrlId]);
        pVoltRangeList->railOffsets[railIdx].voltOffsetNegativeMaxuV -=
            pChangeSeq->voltOffsetCached.rails[railIdx].voltOffsetuV[ctrlId];
    }
    else if (pChangeSeq->voltOffsetCached.rails[railIdx].voltOffsetuV[ctrlId] > 0)
    {
        pChangeSeq->voltOffsetCached.rails[railIdx].voltOffsetuV[ctrlId] =
            LW_MIN(pVoltRangeList->railOffsets[railIdx].voltOffsetPositiveMaxuV,
                pChangeSeq->voltOffsetCached.rails[railIdx].voltOffsetuV[ctrlId]);
        pVoltRangeList->railOffsets[railIdx].voltOffsetPositiveMaxuV -=
            pChangeSeq->voltOffsetCached.rails[railIdx].voltOffsetuV[ctrlId];
    }
    else
    {
        // Do nothing
    }

    return;
}

static FLCN_STATUS
s_perfChangeSeqColwertToChangeSeqChange
(
    CHANGE_SEQ                                 *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE         *pChangeNext,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT   *pChangeInput
)
{
    BOARDOBJGRP_E32    *pBoardObjGrpE32 = &(VOLT_RAILS_GET()->super);
    BOARDOBJGRPMASK_E32 tmpClkDomainsMask;
    BOARDOBJGRPMASK_E32 tmpVoltRailsMask;
    CLK_DOMAIN         *pDomain;
    VOLT_RAIL          *pVoltRail;
    LwBoardObjIdx       i;
    FLCN_STATUS         status;

    // Build the internal change struct from input change struct.
    pChangeNext->pstateIndex           = pChangeInput->pstateIndex;
    pChangeNext->flags                 = pChangeInput->flags;
    pChangeNext->vfPointsCacheCounter  = pChangeInput->vfPointsCacheCounter;

    // Import the clock domain mask.
    boardObjGrpMaskInit_E32(&tmpClkDomainsMask);
    status = boardObjGrpMaskImport_E32(&tmpClkDomainsMask,
                                       &(pChangeInput->clkDomainsMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfChangeSeqColwertToChangeSeqChange_exit;
    }

    // Validate the input change clock domain mask.
    status = perfChangeSeqValidateClkDomainInputMask(pChangeSeq, &tmpClkDomainsMask);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfChangeSeqColwertToChangeSeqChange_exit;
    }

    pChangeNext->clkList.numDomains =
        boardObjGrpMaskBitIdxHighest(&(tmpClkDomainsMask)) + 1U;
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i, &tmpClkDomainsMask.super)
    {
        pChangeNext->clkList.clkDomains[i].clkDomain =
            clkDomainApiDomainGet(pDomain);
        pChangeNext->clkList.clkDomains[i].clkFreqKHz =
            pChangeInput->clk[i].clkFreqkHz;
    }
    BOARDOBJGRP_ITERATOR_END;

    // Import the volt rail mask.
    boardObjGrpMaskInit_E32(&tmpVoltRailsMask);
    status = boardObjGrpMaskImport_E32(&tmpVoltRailsMask,
                                       &(pChangeInput->voltRailsMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfChangeSeqColwertToChangeSeqChange_exit;
    }

    // Sanity check that the input volt rail and VOLT_RAIL (BOARDOBJGRP) mask are equal.
    if (!boardObjGrpMaskIsEqual(&tmpVoltRailsMask, &(pBoardObjGrpE32->objMask)))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_perfChangeSeqColwertToChangeSeqChange_exit;
    }

    // Populate volt rail information
    pChangeNext->voltList.numRails =
        boardObjGrpMaskBitIdxHighest(&(tmpVoltRailsMask)) + 1U;
    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pVoltRail, i, &tmpVoltRailsMask.super)
    {
        pChangeNext->voltList.rails[i].railIdx    = i;
        pChangeNext->voltList.rails[i].railAction = LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH;
        pChangeNext->voltList.rails[i].voltageuV  =
            pChangeInput->volt[i].voltageuV;
        pChangeNext->voltList.rails[i].voltageMinNoiseUnawareuV =
            pChangeInput->volt[i].voltageMinNoiseUnawareuV;
    }
    BOARDOBJGRP_ITERATOR_END;

s_perfChangeSeqColwertToChangeSeqChange_exit:
    return status;
}

/*!
 * @brief   Helper interface to send perf change completion signal to clients.
 *
 * @memberof CHANGE_SEQ
 * @public
 *
 * @param[in]   pChangeSeq    Change Sequencer pointer.
 */
static FLCN_STATUS
s_perfChangeSeqSendChangeCompletionToClients
(
    CHANGE_SEQ *pChangeSeq
)
{
    PMU_RM_RPC_STRUCT_PERF_SEQ_COMPLETION
                        rpc;
    LwU8                i;
    FLCN_STATUS         status;

    // Zero rpc as fields (such as mask) assume will be zero-ed when constructed.
    (void)memset(&rpc, 0, sizeof(rpc));

    //
    // Check if it is required to send notifications for any pending sync perf
    // change requests.
    //
    for (i = 0; i < LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_QUEUE_SIZE; i++)
    {
        if (pChangeSeq->pmu.syncChangeQueue[i].clientId ==
            LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID)
        {
            continue;
        }

        //
        // Logic: Assume the counter and number are 8 bits for simplicity.
        // Assume there are pending sync change with id =
        // 252, 253, 254, 255, 0, 2
        // If the current completed change seqId = 254,
        // we will notify = 252, 253, 254
        // 254 - 2 = 252 (LwS8 252 is negative number)
        //
        // If the current completed change seqId = 1,
        // we will notify = 252, 253, 255, 0
        // 1 - 252 = 1 - FC = 05 (LwS8 05 is positive number)
        //
        if (LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_SEQ_ID_IS_COMPLETED(
                pChangeSeq->pChangeLwrr->data.pmu.seqId,
                pChangeSeq->pmu.syncChangeQueue[i].seqId))
        {
            if (pChangeSeq->pmu.syncChangeQueue[i].clientId ==
                    LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU)
            {
                PERF_CHANGE_SEQ_PMU_CLIENT_COMPLETE_NOTIFICATION notification;

                if (pChangeSeq->pmu.syncChangeQueue[i].data.pmu.queueHandle == NULL)
                {
                    //
                    // Sanity check on the parameters. While this is done when
                    // queuing up the change, verify there have been no changes.
                    // If one oclwrs, we can't send a notification back.
                    // There is NO need to exit out here, It should have been captured
                    // at the queue.
                    //
                    PMU_BREAKPOINT();
                    continue;
                }

                notification.seqId = pChangeSeq->pChangeLwrr->data.pmu.seqId;
                status = lwrtosQueueSendBlocking(pChangeSeq->pmu.syncChangeQueue[i].data.pmu.queueHandle,
                    &notification, sizeof(notification));
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto s_perfChangeSeqSendChangeCompletionToClients_exit;
                }
            }

            pChangeSeq->pmu.syncChangeQueue[i].clientId =
            LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_ILWALID;
        }
    }

    // Send completion notifications for perf change request.
    rpc.seqId = pChangeSeq->pChangeLwrr->data.pmu.seqId;

    // Handle pending perf change sequence lock request
    if (pChangeSeq->pmu.bLockWaiting)
    {
        // Update the eventMask to notify the ownership of lock.
        rpc.eventMask = FLD_SET_DRF(2080_CTRL_PERF, _CHANGE_SEQ_COMPLETION,
                                    _LOCK_ACQUIRED, _TRUE, rpc.eventMask);

        pChangeSeq->pmu.state        =
            LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_LOCKED;
        pChangeSeq->pmu.bLockWaiting = LW_FALSE;
    }
    else
    {
        pChangeSeq->pmu.state = LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STATE_NONE;
    }

    PMU_RM_RPC_EXELWTE_BLOCKING(status, PERF, SEQ_COMPLETION, &rpc);

s_perfChangeSeqSendChangeCompletionToClients_exit:
    return status;
}

