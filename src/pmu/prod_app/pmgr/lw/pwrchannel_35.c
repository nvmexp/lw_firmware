/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchannel_35.c
 * @brief PMGR Power Channel Specific to PWR_CHANNEL_35.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "pmgr/pwrchannel_35.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
OS_TMR_CALLBACK OsCBPwrChannels;

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTmrCallbackFunc (s_pwrChannels35Callback)
    GCC_ATTRIB_SECTION("imem_pmgrPwrChannelsCallback", "s_pwrChannels35Callback")
    GCC_ATTRIB_USED();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * _35 implementation.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
pwrChannelIfaceModel10GetStatus_35
(
    BOARDOBJ_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJ *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PMGR_PWR_CHANNEL_STATUS *pGetStatus =
        (RM_PMU_PMGR_PWR_CHANNEL_STATUS *)pPayload;
    PWR_MONITOR_35 *pPwrMonitor35 = PWR_MONITOR_35_GET();

    // Copy out the polled tuple from the PWR_MONITOR_35::pPollingStatus
    pGetStatus->tuplePolled =
        pPwrMonitor35->pPollingStatus->channels[BOARDOBJ_GET_GRP_IDX(pBoardObj)]
            .channel.tuple;

    return FLCN_OK;
}

/*!
 * @brief   Public function to load all power channels. This includes scheduling
 *      callbacks to internally sample power tuples specific to PWR_CHANNEL_35.
 */
FLCN_STATUS
pwrChannelsLoad_35(void)
{
    FLCN_STATUS     status        = FLCN_OK;
    PWR_MONITOR_35 *pPwrMonitor35 = PWR_MONITOR_35_GET();
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PWR_CHANNELS_STATUS_GET
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Nothing to do when there are no CHANNEL objects.
        if (boardObjGrpMaskIsZero(&(Pmgr.pwr.channels.objMask)))
        {
            goto pwrChannelsLoad_35_exit;
        }

        // Init the polling PWR_CHANNELS MASK with the set of all PWR_CHANNELs.
        PWR_CHANNELS_STATUS_MASK_INIT(&pPwrMonitor35->pPollingStatus->mask);
        status = boardObjGrpMaskCopy(&(pPwrMonitor35->pPollingStatus->mask),
                                     &(Pmgr.pwr.channels.objMask));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrChannelsLoad_35_exit;
        }

        //
        // Call first evaluation of pwrChannelsStatusGet() so that will have last
        // values cached by first evaluation.
        //
        status = pwrChannelsStatusGet(pPwrMonitor35->pPollingStatus);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrChannelsLoad_35_exit;
        }

        if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPwrChannels)))
        {
            osTmrCallbackCreate(&(OsCBPwrChannels),                     // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                OVL_INDEX_IMEM(pmgrPwrChannelsCallback),                // ovlImem
                s_pwrChannels35Callback,                                // pTmrCallbackFunc
                LWOS_QUEUE(PMU, PMGR),                                  // queueHandle
                pPwrMonitor35->pollingPeriodus,                         // periodNormalus
                pPwrMonitor35->pollingPeriodus,                         // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                RM_PMU_TASK_ID_PMGR);                                   // taskId
        }
        osTmrCallbackSchedule(&(OsCBPwrChannels));
    }
pwrChannelsLoad_35_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @brief   Public function to load all power channels. This includes scheduling
 *      callbacks to internally sample power tuples specific to PWR_CHANNEL_35.
 */
void
pwrChannelsUnload_35(void)
{
    osTmrCallbackCancel(&OsCBPwrChannels);
}


/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_pwrChannels35Callback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_PWR_CHANNELS_STATUS_GET
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        PWR_MONITOR_35 *pPwrMonitor35 = PWR_MONITOR_35_GET();

        PMU_ASSERT_OK_OR_GOTO(status,
            pwrChannelsStatusGet(pPwrMonitor35->pPollingStatus),
            s_pwrChannels35Callback_exit);
    }
s_pwrChannels35Callback_exit:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}
