/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   perf_daemon_fbfln.c
 * @brief  FBFLCN specific Perf Daemon code.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "task_perf_daemon.h"
#include "perf/perf_daemon_fbflcn.h"
#include "clk/pmu_clkfbflcn.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
/* ------------------------- Default Text Section --------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Top-level command-handler function for MCLK Switch commands from the
 * PERF task. Responsible for sending the acknowledgment for each
 * command back to the client.
 *
 * @param[in]  pRequest
 *     Dispatch structure containing the command to handle and information on
 *     the queue the command originated from.
 */
FLCN_STATUS
perfDaemonProcessMclkSwitchEvent
(
    DISPATCH_PERF_DAEMON *pRequest
)
{
    FLCN_STATUS                             status = FLCN_ERR_NOT_SUPPORTED;
    PMU_RM_RPC_STRUCT_CLK_MCLK_SWITCH_ACK   rpc;
    RM_PMU_STRUCT_CLK_MCLK_SWITCH          *pMclkSwitchRequest;

    (void)memset(&rpc, 0x00, sizeof(rpc));
    pMclkSwitchRequest = &pRequest->mclkSwitch.mclkSwitchRequest;

    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_FBFLCN_SUPPORTED))
    {
        // Fill out the header size and msg type to send back to RM
        status = clkTriggerMClkSwitchOnFbflcn(pMclkSwitchRequest,
                               &rpc.mClkSwitchAck.fbStopTimeUs,
                               &rpc.mClkSwitchAck.status);
        rpc.mClkSwitchAck.bAsync = pMclkSwitchRequest->bAsync;
    }

    if (status != FLCN_OK)
    {
        // Invalid command type or request found
        PMU_HALT();
        return status;
    }

    PMU_RM_RPC_EXELWTE_BLOCKING(status, CLK, MCLK_SWITCH_ACK, &rpc);
    return status;
}
