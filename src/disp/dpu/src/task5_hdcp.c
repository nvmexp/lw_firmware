/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task5_hdcp.c
 * @brief  Task that performs validation services related to HDCP
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "lwoslayer.h"
#include "dev_disp.h"
#include "unit_dispatch.h"
#include "lwos_dma.h"

/* ------------------------- OpenRTOS Includes ------------------------------ */
/* ------------------------- Application Includes --------------------------- */
#include "rmdpucmdif.h"
#include "dpu_task.h"
#include "dpu_mgmt.h"
#include "dpu_objdpu.h"
#include "lwdpu.h"
#include "unit_dispatch.h"
#include "lib_intfchdcp.h"

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/*!
 * This is the queue that may be used for dispatching work-items to the task.
 * It is assumed that this queue is setup/created before this task is scheduled
 * to run. Work-items may be HDCP commands.
 */
LwrtosQueueHandle HdcpQueue;

/* ------------------------- Static Variables ------------------------------- */
static HDCP_CONTEXT     HdcpContext;
/* ------------------------- Private Functions ------------------------------ */
#ifdef GSPLITE_RTOS
static void _hdcpLoadOverlays(void)
    GCC_ATTRIB_SECTION("imem_hdcp", "_hdcpLoadOverlays");
#endif
/*!
 * @brief Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task5_hdcp, pvParameters)
{
    DISPATCH_HDCP       disp2hdcp;
#if !((DPU_PROFILE_v0201) || (DPU_PROFILE_v0205) || (DPU_PROFILE_v0207) || (DPU_PROFILE_gv100))
    // This buffer is used for mirroring Hdcp1.x command for security purpose
    RM_DPU_COMMAND      dpuHdcp1xCmd;
#endif
    RM_DPU_COMMAND      *pCmd;

#ifndef GSPLITE_RTOS
    OSTASK_OVL_DESC     ovlHdcpDescList[] = {
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64)
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, hdcpCmn)
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libHdcp)
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, auth)
                        };
#else
    // Load hdcp1.x required overlays here
    _hdcpLoadOverlays();
#endif
    //
    // Event processing loop. Process commands sent from the RM to the Hdcp
    // task. Also, handle Hdcp specific signals such as the start of frame
    // signal.
    //
    for (;;)
    {
        if (OS_QUEUE_WAIT_FOREVER(HdcpQueue, &disp2hdcp))
        {
#ifndef GSPLITE_RTOS
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlHdcpDescList);
#endif
            switch (disp2hdcp.eventType)
            {
                //
                // Commands sent from the RM to the Hdcp task.
                //
                case DISP2UNIT_EVT_COMMAND:
                {
                    //
                    // Lwrrently only valid value the QueueID can take is QUEUE_RM.
                    // Else case should not be happening, so putting HALT.
                    //
                    if (disp2hdcp.command.cmdQueueId == DPU_MGMT_CMD_QUEUE_RM)
                    {
                        //
                        // This needs to be implemented for all profiles.
                        // As there are some perf tests failing in DVS,
                        // i am only enabling command mirroring for turing
                        //
#if !((DPU_PROFILE_v0201) || (DPU_PROFILE_v0205) || (DPU_PROFILE_v0207) || (DPU_PROFILE_gv100))
                        //
                        // Hdcp1x command copy is done here instead of command queue mirroring
                        // After returning from hdcpProcessCmd gDpuHdcp1xCmd will not be used again.
                        // So, it will not be overwritten by another command
                        //
                        memcpy((LwU8 *)&dpuHdcp1xCmd, (LwU8 *)disp2hdcp.command.pCmd, sizeof(RM_DPU_COMMAND));
                        pCmd = &dpuHdcp1xCmd;
#else
                        pCmd = disp2hdcp.command.pCmd;
#endif
                        HdcpContext.pOriginalCmd = (RM_FLCN_CMD *)disp2hdcp.command.pCmd;
                        HdcpContext.cmdQueueId = disp2hdcp.command.cmdQueueId;
                        HdcpContext.seqNumId = pCmd->hdr.seqNumId;
                        hdcpProcessCmd(&HdcpContext, (RM_FLCN_CMD *)pCmd);
                    }
                    else
                    {
                        // No support for non-RM clients for this task.
                        DPU_HALT();
                    }
                    break;
                }

                default:
                    break;
            }
#ifndef GSPLITE_RTOS
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlHdcpDescList);
#endif
        }
    }
}

#ifdef GSPLITE_RTOS
/*!
 * @brief Loads Overlays required for HDCP
 */
static void
_hdcpLoadOverlays(void)
{
    OSTASK_OVL_DESC ovlHdcpDescList[] = {
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libHdcp)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, auth)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSha1)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pvtbus)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBigInt)
                 };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlHdcpDescList);

    //
    // When HS ucode encryption is enabled,
    // for any task which gets scheduled in any timer interrupt,
    // OS will always reload the HS overlays attached to that task irrespective
    // of whether context switch or not which is effecting performance.
    // Hence, we are not attaching by default and dynamically attaching
    // and detaching HS overlays only when needed.
    //
#ifndef HS_UCODE_ENCRYPTION
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libSecBusHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libCommonHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libMemHs));
#ifndef HDCP22_KMEM_ENABLED
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libScpCryptHs));
#endif // !HDCP22_KMEM_ENABLED
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(selwreActionHs));
#endif // HS_UCODE_ENCRYPTION
}
#endif
