/*! _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2014-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task6_hdcp22.c
 * @brief  This task implements HDCP 2.2 state mechine
 *         Here is SW IAS describes flow //sw/docs/resman/chips/Maxwell/gm20x/Display/HDCP-2-2_SW_IAS.doc
 */

/* ------------------------- System Includes -------------------------------- */
#include "dpusw.h"
#include "rmflcncmdif.h"
/* ------------------------- OpenRTOS Includes ------------------------------ */
#include "lwostimer.h"
#if (!DPUCFG_FEATURE_ENABLED(DPU_OS_CALLBACK_CENTRALISED))
#include "lwostmrcallback.h"
#endif
/* ------------------------ Application includes --------------------------- */
#include "dpu_objdpu.h"
#include "lib_intfchdcp22wired.h"
#include "dpu_mgmt.h"

/* ------------------------ Function Prototypes ---------------------------- */
/* ------------------------ Global Variables ------------------------------- */
LwrtosQueueHandle     Hdcp22WiredQueue;
#if (!DPUCFG_FEATURE_ENABLED(DPU_OS_CALLBACK_CENTRALISED))
extern OS_TIMER       Hdcp22WiredOsTimer;
#endif
extern HDCP22_SESSION lwrrentSession;
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
#ifdef GSPLITE_RTOS
static void _hdcp22LoadOverlays(void)
    GCC_ATTRIB_SECTION("imem_hdcp22wired", "_hdcp22LoadOverlays");
#endif

static void _hdcp22EventHandle(DISPATCH_HDCP22WIRED *pRequest)
    GCC_ATTRIB_SECTION("imem_hdcp22wired", "_hdcp22EventHandle");

static void
_hdcp22EventHandle
(
    DISPATCH_HDCP22WIRED *pRequest
)
{
#if !((DPU_PROFILE_v0201) || (DPU_PROFILE_v0205) || (DPU_PROFILE_v0207) || (DPU_PROFILE_gv100))
    // This buffer is used for mirroring Hdcp22wired command for security purpose
    RM_DPU_COMMAND        dpuHdcp22WiredCmd;
#endif
    RM_DPU_COMMAND* pCmd;

#ifndef GSPLITE_RTOS
    OSTASK_OVL_DESC ovlHdcp22DescList[] = {
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, hdcpCmn)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libHdcp22wired)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
                    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlHdcp22DescList);
#endif

    switch (pRequest->eventType)
    {
        case DISP2UNIT_EVT_SIGNAL:
        {
            if (pRequest->hdcp22Evt.subType == HDCP22_TIMER0_INTR)
            {
                // Handles the Timer0 Event
                hdcp22wiredHandleTimerEvent(&lwrrentSession,
                                            pRequest->hdcp22Evt.eventInfo.timerEvtType);
            }
            else if (pRequest->hdcp22Evt.subType == HDCP22_SOR_INTR)
            {
                // Disables HDCP22 if SOR is power down.
                hdcp22wiredHandleSorInterrupt(pRequest->hdcp22Evt.eventInfo.sorNum);
            }
#ifndef GSPLITE_RTOS
            else if (pRequest->hdcp22Evt.subType == HDCP22_SEC2_INTR)
            {
                hdcp22wiredProcessSec2Req ();
            }
#endif
            break;
        }
        case DISP2UNIT_EVT_COMMAND:
        {
            //
            // Handles the New Command from RM
            // Lwrrently only valid value the QueueID can take is QUEUE_RM.
            // Else case should not be happening, so putting HALT.
            //
            if (pRequest->command.cmdQueueId == DPU_MGMT_CMD_QUEUE_RM)
            {
                //
                // This needs to be implemented for all profiles.
                // As there are some perf tests failing in DVS,
                // i am only enabling command mirroring for turing
                //
#if !((DPU_PROFILE_v0201) || (DPU_PROFILE_v0205) || (DPU_PROFILE_v0207) || (DPU_PROFILE_gv100))
                //
                // Hdcp22wired command copy is done here, instead of command queue mirroring
                // After returning from hdcp22wired ProcessCmd gDpuHdcp22WiredCmd will not be used again.
                // So, it will not be overwritten by another command
                //
                memcpy((LwU8 *)&dpuHdcp22WiredCmd, (LwU8 *)pRequest->command.pCmd, sizeof(RM_DPU_COMMAND));
                pCmd = &dpuHdcp22WiredCmd;
#else
                pCmd = pRequest->command.pCmd;
#endif
                hdcp22wiredProcessCmd(&lwrrentSession,
                                      (RM_FLCN_CMD*)pCmd,
                                      pCmd->hdr.seqNumId,
                                      pRequest->command.cmdQueueId);
                osCmdmgmtCmdQSweep(&(((RM_FLCN_CMD *)(pRequest->command.pCmd))->cmdGen.hdr),
                                   pRequest->command.cmdQueueId);
            }
            else
            {
                // No support for non-RM clients for this task.
                DPU_HALT();
            }
            break;
        }
        default:
        {
            break;
        }
    }

#ifndef GSPLITE_RTOS
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlHdcp22DescList);
#endif
}

/* ------------------------- Public Functions ------------------------------- */
/* ------------------------- Function Definitions ----------------------------*/
/*!
 * Defines the entry-point for this task.  This function will execute forever
 * processing each command that arrives in the message queues and exelwting
 * each command.
 */
lwrtosTASK_FUNCTION(task6_hdcp22, pvParameters)
{
    DISPATCH_HDCP22WIRED disp2Hdcp22;
#ifndef GSPLITE_RTOS
    OSTASK_OVL_DESC      ovlHdcp22DescList[] = {
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, hdcpCmn)
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libHdcp22wired)
                            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
                          };
#else
    // Load hdcp22 required overlays here
    _hdcp22LoadOverlays();
#endif

#ifndef GSPLITE_RTOS
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlHdcp22DescList);
#endif

    hdcp22wiredInitialize();

#ifndef GSPLITE_RTOS
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlHdcp22DescList);
#endif

#if (DPUCFG_FEATURE_ENABLED(DPU_OS_CALLBACK_CENTRALISED))
    LWOS_TASK_LOOP(Hdcp22WiredQueue, &disp2Hdcp22, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        _hdcp22EventHandle(&disp2Hdcp22);
    }
    LWOS_TASK_LOOP_END(status);
#else
    // Dispatch event handling.
    while (LW_TRUE)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(&Hdcp22WiredOsTimer,
                                                  Hdcp22WiredQueue,
                                                  &disp2Hdcp22, lwrtosMAX_DELAY);
        {
            _hdcp22EventHandle(&disp2Hdcp22);
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&Hdcp22WiredOsTimer,
                                                 lwrtosMAX_DELAY);
    }
#endif
}

#ifdef GSPLITE_RTOS
/*!
 * @brief Loads Overlays required for HDCP22
 */
static void
_hdcp22LoadOverlays(void)
{
    OSTASK_OVL_DESC ovlHdcp22DescList[] = {
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libHdcp22wired)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libLw64Umul)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSE)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, hdcp22Repeater)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, hdcp22CertrxSrm)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, hdcp22Ake)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, hdcp22Akeh)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, hdcp22Lc)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, hdcp22Ske)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSha)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSha1)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, auth)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBigInt)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libI2c)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libI2cHw)
                        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libDpaux)
                   };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlHdcp22DescList);

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
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libShaHs));
#ifndef HDCP22_USE_HW_RSA
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libBigIntHs));
#else
#ifdef LIB_CCC_PRESENT
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libCccHs));
#endif
#endif// HDCP22_USE_HW_RSA
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22StartSessionHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22EndSessionHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22GenerateKmkdHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22HprimeValidationHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22LprimeValidationHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22GenerateSessionkeyHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22ControlEncryptionHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22VprimeValidationHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22MprimeValidationHs));
    OSTASK_ATTACH_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(libHdcp22ValidateCertrxSrmHs));
    OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY_HS(OVL_INDEX_IMEM(selwreActionHs));   
#endif // HS_UCODE_ENCRYPTION
 }
#endif // GSPLITE_RTOS
