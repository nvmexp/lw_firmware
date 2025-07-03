/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    cmdmgmt_queue_fsp.c
 * @copydoc cmdmgmt_queue_fsp.h
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "pmu_objpmu.h"

/* ------------------------- Application Includes --------------------------- */
#include "cmdmgmt/cmdmgmt_queue_fsp.h"
#include "fsp_rpc.h"
#include "dev_fsp_addendum.h"
#include "dbgprintf.h"
#include "pmu_objic.h"
#include "pmu_objsmbpbi.h"
#include "task_therm.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Compile Time Checks ---------------------------- */
// Assure that this file compiles only when required.
ct_assert(PMUCFG_FEATURE_ENABLED(PMU_FSP_RPC));

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc queueFspQueuesInit
 */
FLCN_STATUS
queueFspQueuesInit
(
    void
)
{
    LW_STATUS status;
    FSP_RPC_CLIENT_INIT_DATA initData;

    initData.channelId = FSP_EMEM_CHANNEL_PMU_SOE;
    initData.channelOffsetDwords = FSP_EMEM_CHANNEL_PMU_SOE_OFFSET / FSP_RPC_BYTES_PER_DWORD;
    initData.channelSizeDwords = FSP_EMEM_CHANNEL_PMU_SOE_SIZE / FSP_RPC_BYTES_PER_DWORD;
    initData.bDuplexChannel = LW_TRUE;

    status = fspRpcInit(&initData);

    return ((status == LW_OK) ? FLCN_OK : FLCN_ERROR);
}

/*!
 * @copydoc queueFspProcessCmdQueue
 */
FLCN_STATUS
queueFspProcessCmdQueue
(
    LwU32 headId
)
{
    DISP2THERM_CMD disp2Therm = {{ 0 }};
    FLCN_STATUS status = FLCN_OK;

    if (headId != PMU_CMD_QUEUE_FSP_RPC_MESSAGE)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto queueFspProcessCmdQueue_exit;
    }

    if (SmbpbiResident.requestCnt++ == 0)
    {
        // Add message to THERM task queue.
        disp2Therm.hdr.unitId = RM_PMU_UNIT_SMBPBI;
        disp2Therm.hdr.eventType = SMBPBI_EVT_MSGBOX_REQUEST;
        lwrtosQueueIdSend(LWOS_QUEUE_ID(PMU, THERM),
                          &disp2Therm, sizeof(disp2Therm),
                          0U);
    }

queueFspProcessCmdQueue_exit:
    return status;
}

