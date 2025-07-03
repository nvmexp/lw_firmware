/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 - 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   lpwr_perfseq.c
 * @brief  PMU LowPower Perf Sequencer
 *
 * Manages all LowPower features
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"
#include "lwrtosHooks.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objdisp.h"
#include "pmu_objhal.h"
#include "pmu_objlpwr.h"
#include "pmu_objpmu.h"
#include "pmu_objgr.h"
#include "pmu_objms.h"
#include "pmu_objdi.h"
#include "pmu_objei.h"
#include "pmu_objap.h"
#include "pmu_objdfpr.h"
#include "task_perf.h"
#include "cmdmgmt/cmdmgmt.h"
#include "pmu_oslayer.h"
#include "lwostimer.h"
#include "main.h"
#include "main_init_api.h"
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE))
#include "perf/3x/vfe.h"
#endif

#include "dbgprintf.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

#include "config/g_pg_private.h"
#if(PG_MOCK_FUNCTIONS_GENERATED)
#include "config/g_pg_mock.c"
#endif

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */

#if PMUCFG_FEATURE_ENABLED(PMU_LPWR_DEBUG)
LwU32 LpwrPerfCntrPreStart;
LwU32 LpwrPerfCntrPreEnd;
LwU32 LpwrPerfCntrPostStart;
LwU32 LpwrPerfCntrPostEnd;
LwU32 LpwrPerfLastIdx;
LwU32 LpwrPerfLwrrIdx;
#endif // PMUCFG_FEATURE_ENABLED(PMU_LPWR_DEBUG)

/* ------------------------- Prototypes ------------------------------------- */
static FLCN_STATUS s_lpwrPerfChangeSeqRegister(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION perfEvt, PERF_CHANGE_SEQ_NOTIFICATION *pNotification)
                   GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_lpwrPerfChangeSeqRegister");
static FLCN_STATUS s_lpwrLpPstateChangeSeqRegister(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION pstateEvt, PERF_CHANGE_SEQ_NOTIFICATION *pNotification)
                   GCC_ATTRIB_SECTION("imem_lpwrLoadin", "s_lpwrLpPstateChangeSeqRegister");
static void        s_lpwrPerfChangePre(void);
static void        s_lpwrPerfChangePost(void);
static LwU32       s_lpwrPerfChangePopulateDisableMask(void);
static LwU32       s_lpwrPerfChangePopulateEnableMask(void);
static LwBool      s_lpwrPerfIsCtrlEnabled(LwU8 ctrlId, LwU8 perfIdx);
static void        s_lpwrPerfChangeSubFeatureUpdate(void);
static void        s_lpwrPerfChangeMsGrpPre(void);
static LwBool      s_lpwrPerfChangeMsGrpPost(void);
static void        s_lpwrPerfChangeApPost(void);
static void        s_lpwrPerfChangeAckSend(void);
static void        s_lpwrLpPstateChangePre(LwU8 lwrPerfIdx)
                   GCC_ATTRIB_SECTION("imem_lpwrLp ", "s_lpwrLpPstateChangePre");
static void        s_lpwrLpPstateChangePost(LwU8 lwrPerfIdx)
                   GCC_ATTRIB_SECTION("imem_lpwrLp ", "s_lpwrLpPstateChangePre");
/* ------------------------- Public Functions ------------------------------- */

/*!
 * @brief Helper to initialize LPWR-Perf on post init
 */
FLCN_STATUS
lpwrPerfPostInit(void)
{
    static PERF_CHANGE_SEQ_NOTIFICATION LpwrPerfChangeNotificationPre
        GCC_ATTRIB_SECTION("dmem_perfDaemon", "LpwrPerfChangeNotificationPre");
    static PERF_CHANGE_SEQ_NOTIFICATION LpwrPerfChangeNotificationPost
        GCC_ATTRIB_SECTION("dmem_perfDaemon", "LpwrPerfChangeNotificationPost");
    FLCN_STATUS status = FLCN_OK;

    // Register for Pre perf change
    status = s_lpwrPerfChangeSeqRegister(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_PRE_CHANGE,
                                       &LpwrPerfChangeNotificationPre);
    if (status == FLCN_OK)
    {
        // Register for Post perf change
        status = s_lpwrPerfChangeSeqRegister(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_POST_CHANGE,
                                           &LpwrPerfChangeNotificationPost);
    }

    return status;
}

/*!
 * @brief Hepler to register for perf change sequencer events
 *
 * @param[in]   perfEvt          Perf event for which the client wants to register
 * @param[in]   pNotification    Pointer to Perf change seq notification element
 *
 * Steps for registration -
 *      1) Load perfDaemon Overlay
 *      2) Allocate memory for notification element (in perfDaemon Overlay)
 *      3) Update the notification element with client info
 *      4) Update the notification request event structure
 *      5) Send completion signal to PERF task
 *      6) Unload perfDaemon Overlay
 */
static FLCN_STATUS
s_lpwrPerfChangeSeqRegister
(
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION perfEvt,
    PERF_CHANGE_SEQ_NOTIFICATION           *pNotification
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
    {
        DISPATCH_PERF disp2Perf = {{ 0 }};
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfDaemon)
        };

        if (pNotification == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto s_lpwrPerfChangeSeqRegister_exit;
        }

        // Load perfDaemon overlay
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Update the notification element with client info
            pNotification->clientTaskInfo.taskEventId = LPWR_EVENT_ID_PERF_CHANGE_NOTIFICATION;
            pNotification->clientTaskInfo.taskQueue   = LWOS_QUEUE(PMU, LPWR);

            // Update the notification request event structure
            disp2Perf.hdr.eventType =
                PERF_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_REQUEST_REGISTER;
            disp2Perf.notificationRequest.notification  = perfEvt;
            disp2Perf.notificationRequest.pNotification = pNotification;

            // Send completion signal to PERF task
            lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF), &disp2Perf,
                                      sizeof(disp2Perf));
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

s_lpwrPerfChangeSeqRegister_exit:
    return status;
}

/*!
 * @brief Helper to initialize LPWR_LP-Pstate on post init
 */
FLCN_STATUS
lpwrLpPstatePostInit(void)
{
    static PERF_CHANGE_SEQ_NOTIFICATION LpwrLpPstateChangeNotificationPre
        GCC_ATTRIB_SECTION("dmem_perfDaemon", "LpwrLpPstateChangeNotificationPre");
    static PERF_CHANGE_SEQ_NOTIFICATION LpwrLpPstateChangeNotificationPost
        GCC_ATTRIB_SECTION("dmem_perfDaemon", "LpwrLpPstateChangeNotificationPost");
    FLCN_STATUS status = FLCN_OK;

    // Register for Pre pstate change
    status = s_lpwrLpPstateChangeSeqRegister(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_PRE_PSTATE ,
                                        &LpwrLpPstateChangeNotificationPre);
    if (status == FLCN_OK)
    {
        // Register for Post pstate change
        status = s_lpwrLpPstateChangeSeqRegister(RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_POST_PSTATE ,
                                        &LpwrLpPstateChangeNotificationPost);
    }
    return status;
}

/*!
 * @brief Hepler to register for pstate change sequencer events
 *
 * @param[in]   pstateEvt        Pstate event for which the client wants to register
 * @param[in]   pNotification    Pointer to Perf change seq notification element
 *
 * Steps for registration -
 *      1) Load perfDaemon Overlay
 *      2) Allocate memory for notification element (in perfDaemon Overlay)
 *      3) Update the notification element with client info
 *      4) Update the notification request event structure
 *      5) Send completion signal to PERF task
 *      6) Unload perfDaemon Overlay
 */
static FLCN_STATUS
s_lpwrLpPstateChangeSeqRegister
(
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION pstateEvt,
    PERF_CHANGE_SEQ_NOTIFICATION           *pNotification
)
{
    FLCN_STATUS status = FLCN_OK;

    //
    // we need this config feature check because, 
    // we only want to register for PSTATE notification if PSI_CTRL_PSTATE config feature is supported.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35)&&
        PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_PSTATE))
    {
        DISPATCH_PERF disp2Perf = {{ 0 }};
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfDaemon)
        };

        if (pNotification == NULL)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto s_lpwrLpPstateChangeSeqRegister_exit;
        }

        // Load perfDaemon overlay
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Update the notification element with client info
            pNotification->clientTaskInfo.taskEventId = LPWR_LP_PSTATE_CHANGE_NOTIFICATION;
            pNotification->clientTaskInfo.taskQueue   = LWOS_QUEUE(PMU, LPWR_LP);

            // Update the notification request event structure
            disp2Perf.hdr.eventType =
                PERF_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_REQUEST_REGISTER;
            disp2Perf.notificationRequest.notification  = pstateEvt;
            disp2Perf.notificationRequest.pNotification = pNotification;

            // Send completion signal to PERF task
            lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, PERF), &disp2Perf,
                                      sizeof(disp2Perf));
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }

s_lpwrLpPstateChangeSeqRegister_exit:
    return status;
}

/*!
 * @brief Hepler to handle perf change sequencer events
 *
 * After the pre/post handling is done, we send back the Ack to PERF_DAEMON
 * task from the respective handlers itself, as some of the steps might need
 * to handle blocking calls.
 *
 * @param[in]   perfData    Info recieved from change sequencer
 *                          on perf change event
 */
FLCN_STATUS
lpwrPerfChangeEvtHandler
(
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA perfNotificationData
)
{
    FLCN_STATUS status = FLCN_OK;

    // Update the new Pstate
    Lpwr.lastPerfIdx   = perfNotificationData.pstateLevelLast;
    Lpwr.targetPerfIdx = perfNotificationData.pstateLevelLwrr;

    // Update debug last and current perf idx variables
    LPWR_PERF_DEBUG_LAST_IDX_UPDATE(Lpwr.lastPerfIdx);
    LPWR_PERF_DEBUG_LWRR_IDX_UPDATE(Lpwr.targetPerfIdx);

    switch (perfNotificationData.notification)
    {
        case RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_PRE_CHANGE:
        {
            // Perf Pre change handler
            s_lpwrPerfChangePre();
            break;
        }
        case RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_POST_CHANGE:
        {
            // Perf Post change handler
            s_lpwrPerfChangePost();
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }
    }

    return status;
}

/*!
 * @brief: LowPower Perf Change Pre Sequencer
 *
 * This function exelwtes all the steps for lowpower
 * before a perf change.
 *
 */
void
lpwrPerfChangePreSeq(void)
{
    LwU32 lpwrCtrlId;

    // Disable all LPWR_CTRLs in lpwrCtrlPerfMask
    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, Lpwr.lpwrCtrlPerfMask)
    {
        if (!pgCtrlDisallow(lpwrCtrlId, PG_CTRL_DISALLOW_REASON_MASK(PERF)))
        {
            //
            // Immediate disablement is not possible. We need to wait until
            // disablement completes. Thus return from this function. We
            // re-trigger the sequencer after receiving the DISALLOW_ACK.
            //
            return;
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    //
    // Check if EI is enabled on target Pstate or not.
    // If its not enabled then disable the EI as well
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI)  &&
        pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_EI) &&
        !s_lpwrPerfIsCtrlEnabled(RM_PMU_LPWR_CTRL_ID_EI, Lpwr.targetPerfIdx))
    {
        if (!pgCtrlDisallow(RM_PMU_LPWR_CTRL_ID_EI, PG_CTRL_DISALLOW_REASON_MASK(PERF)))
        {
            return;
        }
    }

    // We are done with disablement, reset the Mask.
    Lpwr.lpwrCtrlPerfMask = 0;

    // Do the MSCG/DIFR Specific settings for the Pre Perf change
    if ((PMUCFG_FEATURE_ENABLED(PMU_PG_MS)                    &&
         pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG))      ||
        (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR) &&
         pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR)))
    {
        s_lpwrPerfChangeMsGrpPre();
    }

    // Send the ack to Perf Task
    s_lpwrPerfChangeAckSend();

    // Update debug counter variable
    LPWR_PERF_DEBUG_CNTR_UPDATE(PreEnd);
}

/*!
 * @brief: LowPower Perf Change Post Sequencer
 *
 * This function exelwtes all the steps for lowpower
 * after a perf change.
 *
 */
void
lpwrPerfChangePostSeq(void)
{
    LwU32  lpwrCtrlId;
    LwBool bMsAllowed;

    // Call the SubFeature Update
    s_lpwrPerfChangeSubFeatureUpdate();

    // Call MSCG/DIFR specific Pstate settings
    if ((PMUCFG_FEATURE_ENABLED(PMU_PG_MS)                    &&
         pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG))      ||
        (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR) &&
         pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR)))
    {
        bMsAllowed = s_lpwrPerfChangeMsGrpPost();
        //
        // If We are not able to enable MSCG, then we need to remove it
        // from the Perf enabledMask
        //
        if (!bMsAllowed)
        {
            // Remove MSCG from the Perf enabled mask
            Lpwr.lpwrCtrlPerfMask &= ~LWBIT(RM_PMU_LPWR_CTRL_ID_MS_MSCG |
                                            RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR);

            if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_GR_PASSIVE))
            {
                //
                // GR-Passive uses pstate setting from MSCG. We enable
                // GR-Passive only if MSCG/DIFR is enabled on current pstate.
                //
                Lpwr.lpwrCtrlPerfMask &= ~LWBIT(RM_PMU_LPWR_CTRL_ID_GR_PASSIVE);
            }
        }
    }

    // Call the Adaptive Power Specific settings
    s_lpwrPerfChangeApPost();

    // Enable all LPWR_CTRLs in lpwrCtrlPerfMask
    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, Lpwr.lpwrCtrlPerfMask)
    {
        pgCtrlAllow(lpwrCtrlId, PG_CTRL_ALLOW_REASON_MASK(PERF));
    }
    FOR_EACH_INDEX_IN_MASK_END;

    // We are done with enablement, reset the Mask.
    Lpwr.lpwrCtrlPerfMask = 0;

    // Send the ack to Perf Task
    s_lpwrPerfChangeAckSend();

    // Update debug counter variable
    LPWR_PERF_DEBUG_CNTR_UPDATE(PostEnd);
}

/*!
 * @brief Handler for each Perf pre change
 */
void
s_lpwrPerfChangePre(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
    {
        // Update debug counter variable
        LPWR_PERF_DEBUG_CNTR_UPDATE(PreStart);

        // Get the Mask of LpwrCtrl which we need to disable across Perf change
        Lpwr.lpwrCtrlPerfMask = s_lpwrPerfChangePopulateDisableMask();

        // Call the Lpwr Pre Pref Sequence
        lpwrPerfChangePreSeq();
    }
}

/*!
 * @brief Handler for each Perf post change
 */
static void
s_lpwrPerfChangePost(void)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_WITH_PS35))
    {
        // Update debug counter variable
        LPWR_PERF_DEBUG_CNTR_UPDATE(PostStart);

        Lpwr.lpwrCtrlPerfMask = s_lpwrPerfChangePopulateEnableMask();

        // Call the Lpwr Post Perf Sequencer
        lpwrPerfChangePostSeq();
    }
}

/*!
 * @brief Hepler to handle pstate change sequencer events
 *
 * @param[in]   perfData    Info recieved from change sequencer
 *                          on perf change event
 */
FLCN_STATUS
lpwrLpPstateChangeEvtHandler
(
    RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_DATA pstateNotificationData
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (pstateNotificationData.notification)
    {
        case RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_PRE_PSTATE:
        {
            // Pstate Pre change handler
            s_lpwrLpPstateChangePre(pstateNotificationData.pstateLevelLast);
            break;
        }
        case RM_PMU_PERF_CHANGE_SEQ_PMU_NOTIFICATION_POST_PSTATE:
        {
            // Pstate Post change handler
            s_lpwrLpPstateChangePost(pstateNotificationData.pstateLevelLwrr);
            break;
        }
        default:
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */

/*!
 * @brief: Populate the LpwrCtrl Enable Mask for Perf Change
 *
 * @param[in]   lpwrCtrlId  LPWR_CTRL Id
 * @param[in]   perfIdx     Current Pstate Index
 *
 * return: LW_TRUE  -> If LPWR_CTRL is enabled on given pstate
 *         LW_FALSE -> If LPWR_CTRL is not enabled on given Pstate
 */
LwBool
s_lpwrPerfIsCtrlEnabled
(
    LwU8    lpwrCtrlId,
    LwU8    perfIdx
)
{
    LwU8   idx;
    LwBool bEnabled = LW_FALSE;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
    };

    // If Lpwr CTRL is not supported, return False
    if (!pgIsCtrlSupported(lpwrCtrlId))
    {
        return bEnabled;
    }
    // Attach Perf dmem overlay as we will be accessing its structures.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        switch (lpwrCtrlId)
        {
            case RM_PMU_LPWR_CTRL_ID_GR_PG:
            {
                idx      = lpwrVbiosGrEntryIdxGet(perfIdx);
                bEnabled = Lpwr.pVbiosData->gr.entry[idx].bGrEnabled;
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_GR_RG:
            {
                idx      = lpwrVbiosGrRgEntryIdxGet(perfIdx);
                bEnabled = Lpwr.pVbiosData->grRg.entry[idx].bGrRgEnabled;
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_GR_PASSIVE:
            {
                //
                // GR-Passive uses pstate setting from MSCG. We enable GR-Passive
                // only if MSCG is supported and enabled on current pstate.
                //
                if (pgIsCtrlSupported(RM_PMU_LPWR_CTRL_ID_MS_MSCG))
                {
                    idx      = lpwrVbiosMsEntryIdxGet(perfIdx);
                    bEnabled = Lpwr.pVbiosData->ms.entry[idx].bMsEnabled;
                }
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_MS_MSCG:
            {
                idx      = lpwrVbiosMsEntryIdxGet(perfIdx);
                bEnabled = Lpwr.pVbiosData->ms.entry[idx].bMsEnabled;
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_MS_LTC:
            {
                // MS_LTC is enabled on all Pstates
                bEnabled = LW_TRUE;
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_MS_PASSIVE:
            {
                // MS-Passive is enabled on all Pstates
                bEnabled = LW_TRUE;
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_EI:
            {
                idx  = lpwrVbiosEiEntryIdxGet(perfIdx);
                bEnabled = Lpwr.pVbiosData->ei.entry[idx].bEiEnabled;
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_EI_PASSIVE:
            {
                // EI-Passive is enabled on all Pstates
                bEnabled = LW_TRUE;
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_DFPR:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR))
                {
                    idx  = lpwrVbiosDifrEntryIdxGet(perfIdx);
                    bEnabled = Lpwr.pVbiosData->pDifr->entry[idx].bDifrPrfetchLayerEnabled;
                }
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR))
                {
                    idx  = lpwrVbiosDifrEntryIdxGet(perfIdx);
                    bEnabled = Lpwr.pVbiosData->pDifr->entry[idx].bDifrSwAsrLayerEnabled;
                }
                break;
            }

            case RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG:
            {
                if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_CG))
                {
                    idx  = lpwrVbiosDifrEntryIdxGet(perfIdx);
                    bEnabled = Lpwr.pVbiosData->pDifr->entry[idx].bDifrCgLayerEnabled;
                }
                break;
            }

            default:
            {
                PMU_BREAKPOINT();
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return bEnabled;
}

/*!
 * @brief: LowPower Generic Steps pre Perf change
 *
 */
void
s_lpwrPerfChangeMsGrpPre(void)
{
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, dispImp)
    };

    // Disable MSCG/DIFR from IMP Side
    if (PMUCFG_FEATURE_ENABLED(PMU_DISP_IMP_INFRA) &&
        (Lpwr.targetPerfIdx != Lpwr.lastPerfIdx))
    {
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            // Also disable MSCG from IHUB.
            isohubMscgEnableDisable_HAL(&Disp, LW_FALSE,
                                        RM_PMU_DISP_MSCG_DISABLE_REASON_PERF_SWITCH);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
}

/*!
 * @brief: LowPower MS Group setting update handler after
 * a perf change
 *
 * return LW_TRUE -> Enable MS Group
 *        LW_FALSE -> Keep MS Disable
 */
LwBool
s_lpwrPerfChangeMsGrpPost(void)
{
    FLCN_STATUS status         = FLCN_OK;
    LwU32       msMclkSfmMask  = RM_PMU_LPWR_CTRL_SFM_VAL_DEFAULT;
    LwU32       lpwrCtrlId     = 0;
    LwBool      bMsAllowed     = LW_FALSE;
    LwBool      bWrTrainingReq = LW_FALSE;
    LwU8        msIdx;
    LwU8        difrIdx;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, dispImp)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Get the MS Group Settings based on MCLK Value
        msMclkAction(&bMsAllowed, &bWrTrainingReq);

        // Update the WR Training SFM
        if (!bWrTrainingReq)
        {
            msMclkSfmMask = LPWR_SF_MASK_CLEAR(MS, FB_WR_TRAINING, msMclkSfmMask);
        }

        FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, lpwrGrpCtrlMaskGet(RM_PMU_LPWR_GRP_CTRL_ID_MS))
        {
            // Send request to update sub-feature mask
            pgCtrlSubfeatureMaskRequest(lpwrCtrlId, LPWR_CTRL_SFM_CLIENT_ID_MCLK,
                                        msMclkSfmMask);
        }
        FOR_EACH_INDEX_IN_MASK_END;

        //
        // If DIFR_SW_ASR is enabled on new Pstate, do the DIFR related pre requisite
        // steps.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR)                            &&
            s_lpwrPerfIsCtrlEnabled(RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR, Lpwr.targetPerfIdx) &&
            bMsAllowed)
        {
            // TODO-pankumar: Add the Watermark programming Code

            //
            // Sleep aware callback: Pstate clients
            //
            // As of today, sleep aware callback framework benefits
            // the MSCG. Thus we added sleep aware callback flags in
            // MS VBIOS table. Based on these flags, update per pstate
            // settings for EI and GR coupled sleep aware callback.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_TASK_MGMT))
            {
                // Mscg vbios table index
                difrIdx = lpwrVbiosDifrEntryIdxGet(Lpwr.targetPerfIdx);

                //
                // Update arbiter based on EI coupled sleep aware
                // callback control flag in DIFR table
                //
                lpwrCallbackSacArbiter(LPWR_SAC_CLIENT_ID_EI_PSTATE,
                        Lpwr.pVbiosData->pDifr->entry[difrIdx].bDifrSacCtrlEiEnabled);

                //
                // Update arbiter based on GR coupled sleep aware
                // callback control flag in MS table
                //
                lpwrCallbackSacArbiter(LPWR_SAC_CLIENT_ID_GR_PSTATE,
                        Lpwr.pVbiosData->pDifr->entry[difrIdx].bDifrSacCtrlGrEnabled);
            }
        }
        //
        // If MSCG is enabled on new Pstate, do the MSCG related pre requisite
        // steps.
        //
        else if (PMUCFG_FEATURE_ENABLED(PMU_PG_MS)                                        &&
                 s_lpwrPerfIsCtrlEnabled(RM_PMU_LPWR_CTRL_ID_MS_MSCG, Lpwr.targetPerfIdx) &&
                 bMsAllowed)
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_DISP_IMP_INFRA))
            {
                //
                // MSCG watermarks are callwlated per-Pstate, so if the
                // Pstate is changing, make sure we are using the
                // correct watermarks for the new Pstate before we
                // reenable MSCG.
                //
                if (Lpwr.targetPerfIdx != Lpwr.lastPerfIdx)
                {
                    status = isohubProgramMscgWatermarks_HAL(&Disp);
                    if (status == FLCN_OK)
                    {
                        // Reenable MSCG from IHUB.
                        isohubMscgEnableDisable_HAL(&Disp,
                                LW_TRUE,
                                RM_PMU_DISP_MSCG_DISABLE_REASON_PERF_SWITCH);
                    }
                    else
                    {
                        bMsAllowed = LW_FALSE;
                    }
                }
            }

            //
            // Sleep aware callback: Pstate clients
            //
            // As of today, sleep aware callback framework benefits
            // the MSCG. Thus we added sleep aware callback flags in
            // MS VBIOS table. Based on these flags, update per pstate
            // settings for EI and GR coupled sleep aware callback.
            //
            if (PMUCFG_FEATURE_ENABLED(PMU_PG_TASK_MGMT))
            {
                // Mscg vbios table index
                msIdx = lpwrVbiosMsEntryIdxGet(Lpwr.targetPerfIdx);

                //
                // Update arbiter based on EI coupled sleep aware
                // callback control flag in MS table
                //
                lpwrCallbackSacArbiter(LPWR_SAC_CLIENT_ID_EI_PSTATE,
                        Lpwr.pVbiosData->ms.entry[msIdx].bMsSacCtrlEiEnabled);

                //
                // Update arbiter based on GR coupled sleep aware
                // callback control flag in MS table
                //
                lpwrCallbackSacArbiter(LPWR_SAC_CLIENT_ID_GR_PSTATE,
                        Lpwr.pVbiosData->ms.entry[msIdx].bMsSacCtrlGrEnabled);
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return bMsAllowed;
}

/*!
 * @brief: LowPower Adaptive setting update handler after
 * a perf change
 */
void
s_lpwrPerfChangeApPost(void)
{
    LwU8 grIdx;
    LwU8 msIdx;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
    };

    // Attach Perf dmem overlay as we will be accessing its structures.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_GR) &&
            AP_IS_CTRL_SUPPORTED(RM_PMU_AP_CTRL_ID_GR))
        {
            grIdx = lpwrVbiosGrEntryIdxGet(Lpwr.targetPerfIdx);

            if (Lpwr.pVbiosData->gr.entry[grIdx].bGrAdaptive)
            {
                apCtrlEnable(RM_PMU_AP_CTRL_ID_GR, AP_CTRL_ENABLE_MASK(PERF));
            }
            else
            {
                apCtrlDisable(RM_PMU_AP_CTRL_ID_GR, AP_CTRL_DISABLE_MASK(PERF));
            }
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_AP_CTRL_MSCG) &&
            AP_IS_CTRL_SUPPORTED(RM_PMU_AP_CTRL_ID_MSCG))
        {
            msIdx = lpwrVbiosMsEntryIdxGet(Lpwr.targetPerfIdx);

            if (Lpwr.pVbiosData->ms.entry[msIdx].bMsAdaptive)
            {
                apCtrlEnable(RM_PMU_AP_CTRL_ID_MSCG, AP_CTRL_ENABLE_MASK(PERF));
            }
            else
            {
                apCtrlDisable(RM_PMU_AP_CTRL_ID_MSCG, AP_CTRL_DISABLE_MASK(PERF));
            }
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * @brief: LowPower Sub feature update handler after
 * a pstate change
 */
void
s_lpwrPerfChangeSubFeatureUpdate(void)
{
    PSI_CTRL *pPsiCtrl;
    LwU32     featureMask = 0;
    LwU32     idx         = 0;
    LwU32     ctrlId;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)
    };

    // Attach Perf dmem overlay as we will be accessing its structures.
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Update the PSI setting for target Pstate
        if (PMUCFG_FEATURE_ENABLED(PMU_PSI_MULTIRAIL))
        {
            FOR_EACH_INDEX_IN_MASK(32, ctrlId, Psi.ctrlSupportMask)
            {
                pPsiCtrl = PSI_GET_CTRL(ctrlId);

                pPsiCtrl->railEnabledMask = psiCtrlPstateRailMaskGet(ctrlId,
                                            Lpwr.targetPerfIdx);
            }
            FOR_EACH_INDEX_IN_MASK_END;
        }

        // Update the Child Feature
        FOR_EACH_INDEX_IN_MASK(32, ctrlId, Pg.supportedMask)
        {
            // Get feature mask corresponding to current Pstate
            switch (ctrlId)
            {
                case RM_PMU_LPWR_CTRL_ID_GR_PG:
                {
                    idx         = lpwrVbiosGrEntryIdxGet(Lpwr.targetPerfIdx);
                    featureMask = Lpwr.pVbiosData->gr.entry[idx].featureMask;
                    break;
                }
                case RM_PMU_LPWR_CTRL_ID_GR_RG:
                {
                    idx         = lpwrVbiosGrRgEntryIdxGet(Lpwr.targetPerfIdx);
                    featureMask = Lpwr.pVbiosData->grRg.entry[idx].featureMask;
                    break;
                }
                case RM_PMU_LPWR_CTRL_ID_MS_MSCG:
                {
                    idx         = lpwrVbiosMsEntryIdxGet(Lpwr.targetPerfIdx);
                    featureMask = Lpwr.pVbiosData->ms.entry[idx].featureMask;

                    // Update the Current Aware Per Pstate Parameters as well
                    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_FLAVOUR_LWRRENT_AWARE))
                    {
                        // Set MS sepecific dynamic current coefficient
                        Ms.dynamicLwrrentCoeff[RM_PMU_PSI_RAIL_ID_LWVDD]      =
                            Lpwr.pVbiosData->ms.entry[idx].dynamicLwrrentLogic;
                        Ms.dynamicLwrrentCoeff[RM_PMU_PSI_RAIL_ID_LWVDD_SRAM] =
                            Lpwr.pVbiosData->ms.entry[idx].dynamicLwrrentSram;
                    }

                    break;
                }
                case RM_PMU_LPWR_CTRL_ID_GR_PASSIVE:
                case RM_PMU_LPWR_CTRL_ID_MS_PASSIVE:
                case RM_PMU_LPWR_CTRL_ID_EI_PASSIVE:
                case RM_PMU_LPWR_CTRL_ID_DI:
                case RM_PMU_LPWR_CTRL_ID_EI:
                {
                    //
                    // DI child features are not controlled through SFM infra
                    // GR-passive, MS-Passive and EI-Passive does not have any child feature.
                    // TODO-pankumar: Add EI Vbios specific data as applicable
                    //
                    featureMask = LW_U32_MAX;
                    break;
                }

                case RM_PMU_LPWR_CTRL_ID_DFPR:
                {
                    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_DFPR))
                    {
                        idx         = lpwrVbiosDifrEntryIdxGet(Lpwr.targetPerfIdx);
                        featureMask = Lpwr.pVbiosData->pDifr->entry[idx].dfprFeatureMask;
                    }
                    break;
                }

                case RM_PMU_LPWR_CTRL_ID_MS_DIFR_SW_ASR:
                {
                    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_SW_ASR))
                    {
                        idx         = lpwrVbiosDifrEntryIdxGet(Lpwr.targetPerfIdx);
                        featureMask = Lpwr.pVbiosData->pDifr->entry[idx].difrSwAsrFeatureMask;
                    }
                    break;
                }

                case RM_PMU_LPWR_CTRL_ID_MS_DIFR_CG:
                {
                    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_MS_DIFR_CG))
                    {
                        idx         = lpwrVbiosDifrEntryIdxGet(Lpwr.targetPerfIdx);
                        featureMask = Lpwr.pVbiosData->pDifr->entry[idx].difrCgFeatureMask;
                    }
                    break;
                }

                default:
                {
                    PMU_HALT();
                }
            }

            // Send request to update sub-feature mask
            pgCtrlSubfeatureMaskRequest(ctrlId,
                                        LPWR_CTRL_SFM_CLIENT_ID_PERF,
                                        featureMask);
        }
        FOR_EACH_INDEX_IN_MASK_END;
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * @brief: Populate the LpwrCtrl Disable Mask for Perf Change
 *
 * return: Mask of LpwrCtrls which needs to be disabled
 */
LwU32
s_lpwrPerfChangePopulateDisableMask(void)
{
    LwU32 lpwrCtrlMask = 0;

    // Initialize with all supported LPWR_CTRLs
    lpwrCtrlMask = Pg.supportedMask;

    //
    // Exception for EI if its supported, Remove it from the mask.
    // We will check during Lpwr Perf Sequence if we really
    // need to disable the EI or not
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_LPWR_CTRL_EI) &&
        ((lpwrCtrlMask & LWBIT(RM_PMU_LPWR_CTRL_ID_EI)) != 0))
    {
        lpwrCtrlMask &= ~LWBIT(RM_PMU_LPWR_CTRL_ID_EI);
    }
    return lpwrCtrlMask;
}

/*!
 * @brief: Populate the LpwrCtrl Enable Mask for Perf Change
 *
 * return: Mask of LpwrCtrls which needs to be enabled
 */
LwU32
s_lpwrPerfChangePopulateEnableMask(void)
{
    LwU32 lpwrCtrlMask = 0;
    LwU32 lpwrCtrlId;

    //
    // Loop over all the LpwrCtrls and create the enabledMask based on current
    // pstate.
    //
    FOR_EACH_INDEX_IN_MASK(32, lpwrCtrlId, Pg.supportedMask)
    {
        if (s_lpwrPerfIsCtrlEnabled(lpwrCtrlId, Lpwr.targetPerfIdx))
        {
            lpwrCtrlMask |= LWBIT(lpwrCtrlId);
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;
    return lpwrCtrlMask;
}

/*!
 * @brief: LowPower Send the ACK to Perf Task
 */
void
s_lpwrPerfChangeAckSend(void)
{
    DISPATCH_PERF_DAEMON_COMPLETION dispatchComplete;

    memset(&dispatchComplete, 0, sizeof(dispatchComplete));

    dispatchComplete.hdr.eventType =
        PERF_DAEMON_COMPLETION_EVENT_ID_PERF_CHANGE_SEQ_NOTIFICATION_COMPLETION;
    lwrtosQueueSendBlocking(LWOS_QUEUE(PMU, PERF_DAEMON_RESPONSE), &dispatchComplete,
                            sizeof(dispatchComplete));
}

/*!
 * @brief: Handle task to be done before Pstate change
 */
static void
s_lpwrLpPstateChangePre
(
    LwU8 lwrPerfIdx
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_PSTATE) &&
        PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_PSTATE))
    {
        // Get railEnabled Mask for current Pstate
        LwU32 railEnabledMask = psiCtrlPstateRailMaskGet(RM_PMU_PSI_CTRL_ID_PSTATE, lwrPerfIdx);

        status = psiDisengagePsi_HAL(&Psi, RM_PMU_PSI_CTRL_ID_PSTATE, railEnabledMask,
                                     PSI_I2C_ENTRY_EXIT_STEP_MASK_ALL);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }

    // Send the ack to Perf Task
    s_lpwrPerfChangeAckSend();
}

/*!
 * @brief: Handle task to be done after Pstate change
 */
static void
s_lpwrLpPstateChangePost
(
    LwU8 lwrPerfIdx
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PSI_CTRL_PSTATE)      &&
        PSI_IS_CTRL_SUPPORTED(RM_PMU_PSI_CTRL_ID_PSTATE) &&
        (!Psi.bPstateCoupledPsiDisallowed))
    {
        // Get railEnabled Mask for current Pstate
        LwU32 railEnabledMask = psiCtrlPstateRailMaskGet(RM_PMU_PSI_CTRL_ID_PSTATE, lwrPerfIdx);

        status = psiEngagePsi_HAL(&Psi, RM_PMU_PSI_CTRL_ID_PSTATE, railEnabledMask,
                                  PSI_I2C_ENTRY_EXIT_STEP_MASK_ALL);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }

    // Send the ack to Perf Task
    s_lpwrPerfChangeAckSend();
}
