/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task4_vrr.c
 * @brief  Task that is used to service all kinds of VRR (Variable Regresh Rate)
 *         requests/events.
 */

/* ------------------------ System includes -------------------------------- */
#include "dpusw.h"
#include "lwoslayer.h"

/* ------------------------ Application includes --------------------------- */
#include "dpu/dpuifvrr.h"
#include "dpu_vrr.h"
#include "dpu_objdpu.h"
#include "dpu_objvrr.h"
#include "dpu_task.h"

/* ------------------------ Defines ---------------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
LwrtosQueueHandle VrrQueue;
VRR_CONTEXT       VrrContext;

/* ------------------------ Static Variables ------------------------------- */
static OsTimerCallback (_vrrForceFrameReleaseCallback)
    GCC_ATTRIB_USED();
static void _vrrProcessCmd(RM_DPU_COMMAND *pCmd, LwU8 cmdQueueId);
static void _vrrProcessPmuRgLine(void);
static void _vrrEnableForceFrameRelease(RM_DPU_COMMAND *pCmd, LwBool bEnable);

/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task4_vrr, pvParameters)
{
    DISPATCH_VRR disp2Vrr;

    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&VrrContext.osTimer,
                                                  VrrQueue,
                                                  &disp2Vrr,
                                                  lwrtosMAX_DELAY);
        {
            switch (disp2Vrr.eventType)
            {
                case DISP2UNIT_EVT_SIGNAL:
                {
                    _vrrProcessPmuRgLine();
                    break;
                }

                case DISP2UNIT_EVT_COMMAND:
                {
                    _vrrProcessCmd(disp2Vrr.command.pCmd, disp2Vrr.command.cmdQueueId);
                    break;
                }
            }
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&VrrContext.osTimer, lwrtosMAX_DELAY);
    }
}

/* ------------------------- Static Functions ------------------------------ */

/*!
 * Top-level handler function to process all commands sent from the RM.
 *
 * @param[in]  pCmd  Pointer to the command to process
 */
static void
_vrrProcessCmd
(
    RM_DPU_COMMAND *pCmd,
    LwU8            cmdQueueId
)
{
    RM_FLCN_QUEUE_HDR hdr;

    switch (pCmd->cmd.vrr.cmdType)
    {
        case RM_DPU_CMD_TYPE(VRR, ENABLE):
            if (pCmd->cmd.vrr.cmdEnable.bEnableVrrForceFrameRelease)
            {
                _vrrEnableForceFrameRelease(pCmd, LW_TRUE);
            }
            else
            {
                _vrrEnableForceFrameRelease(pCmd, LW_FALSE);
            }
            break;

        default:
            break;
    }

    hdr.size      = RM_FLCN_QUEUE_HDR_SIZE;
    hdr.unitId    = RM_DPU_UNIT_VRR;
    hdr.ctrlFlags = 0;
    hdr.seqNumId  = pCmd->hdr.seqNumId;

    osCmdmgmtRmQueuePostBlocking(&hdr, NULL);
    osCmdmgmtCmdQSweep(&pCmd->hdr, cmdQueueId);
    return;
}

/*!
 * This function enables/disables the VRR Force frame release
 *
 * @param[in]  pCmd     Pointer to the command to process
 * @param[in]  bEnable  Indicates whether we are enabling or disabling the VRR
 */
static void
_vrrEnableForceFrameRelease
(
    RM_DPU_COMMAND *pCmd,
    LwBool          bEnable
)
{
    RM_DPU_VRR_CMD_ENABLE *pVrrCmd = &pCmd->cmd.vrr.cmdEnable;

    if (bEnable)
    {
        VrrContext.forceReleaseThresholdUs      = pVrrCmd->forceReleaseThresholdUs;
        VrrContext.head                         = pVrrCmd->headIdx;
        VrrContext.relCnt                       = 0;
        VrrContext.bEnableVrrForceFrameRelease  = LW_TRUE;
    }
    else
    {
        VrrContext.bEnableVrrForceFrameRelease  = LW_FALSE;
    }

    return;
}

/*!
 * This function handles PMU_RG_LINE interrupt signal
 */
static void
_vrrProcessPmuRgLine(void)
{
    // Cancel earlier callback
    osTimerDeScheduleCallback(&VrrContext.osTimer, 0);

    osTimerScheduleCallback(
        &VrrContext.osTimer,                            // pOsTimer
        0,                                              // index
        lwrtosTaskGetTickCount32(),                     // ticksPrev
        VrrContext.forceReleaseThresholdUs,             // usDelay
        DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _NO ),   // flags
        _vrrForceFrameReleaseCallback,                  // pCallback
        NULL,                                           // pParam
        OVL_INDEX_ILWALID);                             // ovlIdx
}

/*!
 * @brief OsTimerCallback which handles forced frame release callback
 *
 * @param[in]   pParam          Parameter provided at callback scheduling time.
 * @param[in]   ticksExpired    OS ticks time when callback expired.
 * @param[in]   skipped         Number of skipped callbacks
 */
static void
_vrrForceFrameReleaseCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    LwBool bStalled;

    // if RG isn't stalled then frame is already released from RM.
    vrrIsRgStalled_HAL(&VrrHal, VrrContext.head, &bStalled);
    if (!bStalled)
    {
        return;
    }

    // Release elv
    vrrAllowOneElv_HAL(&VrrHal, VrrContext.head);

    // Unstall RG
    vrrTriggerRgForceUnstall_HAL(&VrrHal, VrrContext.head);

    VrrContext.relCnt++;
}

/*!
 * @brief vrrInit
 *
 * Initialize VRR
 */
void vrrInit(void)
{
    // Initialize OS Timer
    osTimerInitTracked(OSTASK_TCB(VRR), &VrrContext.osTimer,
       VRR_OS_TIMER_ENTRY_NUM_ENTRIES);

    VrrContext.bEnableVrrForceFrameRelease  = LW_FALSE;
}
