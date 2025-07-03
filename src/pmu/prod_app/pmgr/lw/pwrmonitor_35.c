/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrmonitor_35.c
 * @brief PMGR Power Monitoring Specific to PWR_CHANNEL_35.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objpmgr.h"
#include "therm/objtherm.h"
#include "pmgr/pwrmonitor_35.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Constructs the PWR_MONITOR_35 object.
 *
 * @copydoc PwrMonitorConstruct
 */
FLCN_STATUS
pwrMonitorConstruct_35
(
    PWR_MONITOR **ppMonitor
)
{
    PWR_CHANNELS_STATUS *pStatus;
    FLCN_STATUS          status = FLCN_OK;
    OSTASK_OVL_DESC      ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrChannelsStatus)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Construct the PWR_MONITOR_35 object
        *ppMonitor = (PWR_MONITOR *)lwosCallocType(OVL_INDEX_DMEM(pmgr),
                            1, PWR_MONITOR_35);
        if (*ppMonitor == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto pwrMonitorConstruct_35_exit;
        }

        pStatus = lwosCallocType(OVL_INDEX_DMEM(pmgrPwrChannelsStatus), 1,
                                 PWR_CHANNELS_STATUS);
        if (pStatus == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto pwrMonitorConstruct_35_exit;
        }

        // Assign the pointer to pPollingStatus in PWR_MONITOR_35 object
        ((PWR_MONITOR_35 *)*ppMonitor)->pPollingStatus = pStatus;
        // TODO-Tariq: Hardcoding to 100ms for now, should come from VBIOS.
        ((PWR_MONITOR_35 *)*ppMonitor)->pollingPeriodus = 100000;

        // Construct PWR_MONITOR SUPER object state.
        status = pwrMonitorConstruct_SUPER(ppPolicies);

pwrMonitorConstruct_35_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * Return the latest polled PWR_CHANNEL tuple value of the channel requested
 *
 * @param[in]   channelIdx Index of the channel to query
 * @param[out]  pTuple     Pointer to LW2080_CTRL_PMGR_PWR_TUPLE in which to
 *                         return the tuple
 *
 * @return FLCN_OK
 *      Successfully retreived tuple 
 * @return FLCN_ERR_ILWALID_STATE
 *      Invalid channelIdx provided by caller. 
 */
FLCN_STATUS
pwrMonitorPolledTupleGet
(
    LwU8                        channelIdx,
    LW2080_CTRL_PMGR_PWR_TUPLE *pTuple
)
{
    PWR_MONITOR_35 *pPwrMonitor35 = PWR_MONITOR_35_GET();
    OSTASK_OVL_DESC ovlDescList[] = {
        PWR_CHANNEL_35_OVERLAYS_STATUS_GET
    };
    FLCN_STATUS     status = FLCN_OK;

    // Sanity check the channel index passed in
    if (!PWR_CHANNEL_IS_VALID(channelIdx))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto pwrMonitorPolledTupleGet_exit;
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        *pTuple = pPwrMonitor35->pPollingStatus->channels[channelIdx].channel.tuple;
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

pwrMonitorPolledTupleGet_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */

