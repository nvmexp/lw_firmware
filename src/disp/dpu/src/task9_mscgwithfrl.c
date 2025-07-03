/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task9_mscgwithfrl.c
 * @brief  Task that is used for MSCG with FRL
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "lwoslayer.h"

/* ------------------------ Application includes --------------------------- */
#include "dpu/dpuifmscgwithfrl.h"
#include "dpu_mscgwithfrl.h"
#include "dpu_objdpu.h"
#include "dpu_objisohub.h"
#include "dpu_task.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
LwrtosQueueHandle   MscgWithFrlQueue;
MSCGWITHFRL_CONTEXT mscgWithFrlContext;

static OsTimerCallback (_restoreCwmCallback)
    GCC_ATTRIB_USED();

/* ------------------------ Static Variables ------------------------------- */
static void _mscgWithFrlCmdDispatch(RM_DPU_COMMAND *pCmd, LwU8 cmdQueueId);
static void _mscgWithFrlEngage(RM_DPU_COMMAND *pCmd);
static void _mscgWithFrlDisable(RM_DPU_COMMAND *pCmd);

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task9_mscgWithFrl, pvParameters)
{
    DISPATCH_MSCGWITHFRL mscgWithFrl;

    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&mscgWithFrlContext.osTimer,
                                                  MscgWithFrlQueue,
                                                  &mscgWithFrl,
                                                  lwrtosMAX_DELAY);
        {
            switch (mscgWithFrl.eventType)
            {
                //command sent from RM to configure MSCG with FRL
                case DISP2UNIT_EVT_COMMAND:
                {
                    _mscgWithFrlCmdDispatch(mscgWithFrl.command.pCmd,
                                            mscgWithFrl.command.cmdQueueId);
                    break;
                }
            }
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&mscgWithFrlContext.osTimer,
                                                 lwrtosMAX_DELAY);
    }
}

/* ------------------------- Static Functions ------------------------------ */
/*!
 * @brief Process a command.
 *
 * @param[in]  pCmd        Command from the RM.
.* @param[in]  cmdQueueId  Queue Id to acknowledge command is processed
 */
static void
_mscgWithFrlCmdDispatch(RM_DPU_COMMAND *pCmd, LwU8 cmdQueueId)
{
    RM_FLCN_QUEUE_HDR    hdr;
    RM_DPU_MSCGWITHFRL_CMD_ENQUEUE *pMscgWithFrlCmd = &pCmd->cmd.mscgWithFrl.cmdEnqueue;

    switch (pCmd->cmd.mscgWithFrl.cmdType)
    {
        case RM_DPU_CMD_TYPE(MSCGWITHFRL, ENQUEUE):
        {
            switch (pMscgWithFrlCmd->flag)
            {
                case MSCGWITHFRL_CMD_ENQUEUE_FLAG_ENGAGE:
                    _mscgWithFrlEngage(pCmd);
                    break;

                case MSCGWITHFRL_CMD_ENQUEUE_FLAG_DISABLE:
                    _mscgWithFrlDisable(pCmd);

                default:
                    break;
            }
        }
            break;

        default:
            break;
    }

    // As no msg is sent from DPU to RM, size should only size of HDR
    hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;
    hdr.unitId    = RM_DPU_UNIT_MSCGWITHFRL;
    hdr.ctrlFlags = 0;
    hdr.seqNumId  = pCmd->hdr.seqNumId;
    osCmdmgmtRmQueuePostBlocking(&hdr, NULL);
    osCmdmgmtCmdQSweep(&pCmd->hdr, cmdQueueId);
}

/*!
 * @brief Engages MSCG with FRL
 *
 * @param[in]  pCmd        Command from the RM.
 */
static void
_mscgWithFrlEngage(RM_DPU_COMMAND *pCmd)
{
    RM_DPU_MSCGWITHFRL_CMD_ENQUEUE *pMscgWithFrlCmd = &pCmd->cmd.mscgWithFrl.cmdEnqueue;
    FLCN_TIMESTAMP                  time;
    LwU64                           lwrrTimeStampNs;
    LwU64                           startTimeNs;
    LwU64                           frlDelayNs;
    LwU64                           deductionFactor;

    // time stamp based on DPU ptimer
    osPTimerTimeNsLwrrentGet(&time);
    lwrrTimeStampNs = ((LwU64)time.parts.hi << 32) | time.parts.lo;

    startTimeNs = ((LwU64)pMscgWithFrlCmd->startTimeNsHi << 32) | pMscgWithFrlCmd->startTimeNsLo;

    frlDelayNs = ((LwU64)pMscgWithFrlCmd->frlDelayNsHi << 32) | pMscgWithFrlCmd->frlDelayNsLo;

    mscgWithFrlContext.head = pMscgWithFrlCmd->head;

    //
    // Find out time it took command to reach DPU from RM. We need to consider this
    // time to decide time after which we have to restore CWM and Isohub Compression settings
    //
    deductionFactor = lwrrTimeStampNs - startTimeNs;
    if (deductionFactor < frlDelayNs)
    {
        frlDelayNs = frlDelayNs - deductionFactor;
    }
    else
    {
        return;
    }

    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(isohub));
    // Ignore CWM
    isohubSetCwmState_HAL(&IsohubHal, mscgWithFrlContext.head, LW_TRUE /*bIgnore*/);

    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(isohub));

    osTimerScheduleCallback(
        &mscgWithFrlContext.osTimer,                    // pOsTimer
        MSCG_WITH_FRL_OS_TIMER_ENTRY,                   // index
        lwrtosTaskGetTickCount32(),                     // ticksPrev
        (frlDelayNs / 1000),                            // usDelay
        DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _NO),    // flags
        _restoreCwmCallback,                            // pCallback
        NULL,                                           // pParam
        OVL_INDEX_ILWALID);                             // ovlIdx
}

/*!
 * @brief Disable MSCG with FRL
 *
 * @param[in]  pCmd        Command from the RM.
 */
static void
_mscgWithFrlDisable(RM_DPU_COMMAND *pCmd)
{
    // De-Schedule previous callback
    osTimerDeScheduleCallback(&mscgWithFrlContext.osTimer,      // pOsTimer
                              MSCG_WITH_FRL_OS_TIMER_ENTRY);    // index
}

/*!
 * @brief OsTimerCallback
 *
 * Handles MSCG with FRL callback
 */
static void
_restoreCwmCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(OVL_INDEX_IMEM(isohub));
    // Enable CWM
    isohubSetCwmState_HAL(&IsohubHal, mscgWithFrlContext.head, LW_FALSE /*bIgnore*/);
    OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(isohub));
}

/*!
 * @brief mscgWithFrlInit
 *
 * Initialize MSCG with FRL
 */
void mscgWithFrlInit(void)
{
    // Initialize OS Timer
    osTimerInitTracked(OSTASK_TCB(MSCGWITHFRL), &mscgWithFrlContext.osTimer,
       MSCG_WITH_FRL_OS_TIMER_ENTRY_NUM_ENTRIES);
}

