/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_ustreamer.c
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main.h"
#include "lwos_ustreamer.h"
#include "rmpmusupersurfif.h"
#include "cmdmgmt/cmdmgmt_ustreamer.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

#include "lib/lib_pmumon.h"


#include "g_pmurpc.h"

/* --------------------- Private Defines ------------------------------------ */
//! Queue size for the uStreamer default queue. In bytes.
#define PMU_USTREAMER_DEFAULT_QUEUE_SIZE                   (DMEM_BLOCK_SIZE * 8)

/* --------------------- Variables ------------------------------------------ */
#ifdef USTREAMER
LwU32 pmuUstreamerDefaultQueueBuffer[PMU_USTREAMER_DEFAULT_QUEUE_SIZE / sizeof(LwU32)]
    GCC_ATTRIB_SECTION("alignedData256", "pmuUstreamerDefaultQueueBuffer");
#endif // USTREAMER

/* ------------------------ Static Function Prototypes ---------------------- */
#ifdef USTREAMER
static void s_ustreamerInitEventMask(void);
#endif // USTREAMER

/* --------------------- uStreamer Public Functions ------------------------- */
/*!
 * @copydoc pmuUstreamerInitialize
 */
FLCN_STATUS
pmuUstreamerInitialize(void)
{
    FLCN_STATUS status = FLCN_OK;

#ifdef USTREAMER

    if (LWOS_USTREAMER_IS_ENABLED())
    {
        LwU32               bufferOffset;
        LwU32               bufferSize;

        // uStreamer initialization
        status = lwosUstreamerInitialize();

        // Default queue construction
        s_ustreamerInitEventMask();

        bufferOffset =
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.ustreamer.ustreamerBuffer.data);
        bufferSize =
            RM_PMU_SUPER_SURFACE_MEMBER_SIZE(ga10xAndLater.ustreamer.ustreamerBuffer.data);

        status = lwosUstreamerQueueConstruct
        (
            pmuUstreamerDefaultQueueBuffer,
            PMU_USTREAMER_DEFAULT_QUEUE_SIZE,
            &PmuInitArgs.superSurface,
            bufferOffset,
            bufferSize,
            &ustreamerDefaultQueueId,
            LW2080_CTRL_FLCN_USTREAMER_FEATURE_DEFAULT,
            LW_TRUE
        );
        if (status != FLCN_OK)
        {
            goto pmuUstreamerInitialize_fail;
        }

        pmumonUstreamerInitHook();
    }

pmuUstreamerInitialize_fail:
#endif // USTREAMER
    return status;
}

/*!
 * @brief   Get current uStreamer info
 *
 * @param[out]   pParams RM_PMU_RPC_STRUCT_CMDMGMT_USTREAMER_GET_INFO
 */
FLCN_STATUS
pmuRpcCmdmgmtUstreamerGetInfo
(
    RM_PMU_RPC_STRUCT_CMDMGMT_USTREAMER_GET_INFO *pParams
)
{
    // TODO: Add check to see if PMU has been initialized JIRA: GPUSWDTMRD-673
    FLCN_STATUS status = FLCN_OK;

    if (LWOS_USTREAMER_IS_ENABLED())
    {
        LwU32   queueIndex;

        pParams->queueCount = LWOS_USTREAMER_ACTIVE_QUEUE_COUNT_GET();

        if (pParams->queueCount > LW2080_CTRL_FLCN_USTREAMER_FEATURE__COUNT)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pmuRpcCmdmgmtUstreamerGetInfo_exit;
        }

        for (queueIndex = 0; queueIndex < pParams->queueCount; queueIndex++)
        {
            status = LWOS_USTREAMER_GET_QUEUE_INFO(queueIndex, &(pParams->queues[queueIndex]));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pmuRpcCmdmgmtUstreamerGetInfo_exit;
            }
        }
    }

pmuRpcCmdmgmtUstreamerGetInfo_exit:
    return status;
}

/*!
 * @brief   Get ustreamer default queue run-time configuration
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_USTREAMER_CONTROL_GET
 */
FLCN_STATUS
pmuRpcCmdmgmtUstreamerControlGet
(
    RM_PMU_RPC_STRUCT_CMDMGMT_USTREAMER_CONTROL_GET *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (LWOS_USTREAMER_IS_ENABLED())
    {
        status = lwosUstreamerGetEventFilter(pParams->queueId, &pParams->eventFilter);
    }

    return status;
}

/*!
 * @brief   Set instrumentation configuration
 *
 * @param[in/out]   pParams     Pointer to RM_PMU_RPC_STRUCT_CMDMGMT_USTREAMER_CONTROL_SET
 */
FLCN_STATUS
pmuRpcCmdmgmtUstreamerControlSet
(
    RM_PMU_RPC_STRUCT_CMDMGMT_USTREAMER_CONTROL_SET *pParams
)
{
    FLCN_STATUS status = FLCN_OK;

    if (LWOS_USTREAMER_IS_ENABLED())
    {
        status = lwosUstreamerSetEventFilter(pParams->queueId, &pParams->eventFilter);
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
#ifdef USTREAMER
static void
s_ustreamerInitEventMask(void)
{
    LW2080_CTRL_FLCN_USTREAMER_EVENT_FILTER eventFilter;
    memset(eventFilter.mask, 0, LW2080_CTRL_FLCN_USTREAMER_MASK_SIZE_BYTES);

    LWOS_BM_SET(eventFilter.mask, 
                LW2080_CTRL_FLCN_LWOS_INST_EVT_RECALIBRATE, 8);
    LWOS_BM_SET(eventFilter.mask, 
                LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_TICK, 8);
    LWOS_BM_SET(eventFilter.mask, 
                LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_YIELD, 8);
    LWOS_BM_SET(eventFilter.mask, 
                LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_INT0, 8);
    LWOS_BM_SET(eventFilter.mask, 
                LW2080_CTRL_FLCN_LWOS_INST_EVT_CTXSW_BLOCK, 8);
    LWOS_BM_SET(eventFilter.mask, 
                LW2080_CTRL_FLCN_LWOS_INST_EVT_SKIPPED, 8);

    // TODO check status?
    lwosUstreamerSetEventFilter(ustreamerDefaultQueueId, &eventFilter);
}
#endif // USTREAMER
