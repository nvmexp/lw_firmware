/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrmonitor.c
 * @brief Interface Specification for Power-Monitoring/Capping Algorithms
 *
 * 'Power Monitor' is any algorithm capable of some form of
 * current/voltage/power monitoring on a specific set of input channels,
 * possibly with some set of corrective actions when a subset of the input
 * channels exceeds a certain limits.  These channels will be monitoring a
 * subset of the 'Power Devices' on the board.
 *
 * The interfaces defined here are generic and intended to abstract away any
 * algorithm-specific attributes (corrective actions, etc.).
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmu.h"
#include "pmu_objpmgr.h"
#include "task_therm.h"
#include "dbgprintf.h"
#include "pmu_oslayer.h"
#include "logger.h"
#include "pmgr/pwrchannel_pmumon.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_SEMAPHORE))
LwrtosSemaphoreHandle    PwrMonitorChannelQueryMutex;
#endif
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
pmgrPwrMonitorIfaceModel10SetHeader_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJGRP_SET_HEADER *pHdr;
    FLCN_STATUS                                     status;

    // Call to Board Object Group constructor must always be first!
    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        return status;
    }

    pHdr = (RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    switch (pHdr->type)
    {
        case RM_PMU_PMGR_PWR_MONITOR_TYPE_DEFAULT:
        case RM_PMU_PMGR_PWR_MONITOR_TYPE_NO_POLLING:
            return pmgrPwrMonitorIfaceModel10SetHeader_DEFAULT(pModel10, pHdrDesc);
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
pmgrPwrMonitorIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJGRP_SET_HEADER *pHdr   = NULL;
    PWR_MONITOR                                    *pMon   = Pmgr.pPwrMonitor;
    FLCN_STATUS                                     status = FLCN_OK;

    pHdr = (RM_PMU_PMGR_PWR_CHANNEL_BOARDOBJGRP_SET_HEADER *)pHdrDesc;

    pMon->type = pHdr->type;

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_1X)
    // Copy new sampling information
    if (pMon->type != RM_PMU_PMGR_PWR_MONITOR_TYPE_NO_POLLING)
    {
        pMon->samplingPeriodms    = pHdr->samplingPeriodms;
        pMon->samplingPeriodTicks = LW_UNSIGNED_ROUNDED_DIV(
                pMon->samplingPeriodms * (LwU32)1000, LWRTOS_TICK_PERIOD_US);
        if (pMon->samplingPeriodTicks == 0)
        {
            PMU_BREAKPOINT();
            pMon->samplingPeriodTicks = (LwU32)lwrtosMAX_DELAY;
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_LOW_POWER_POLLING))
        {
            pMon->samplingPeriodLowPowerms = pHdr->samplingPeriodLowPowerms;
            pMon->samplingPeriodLowPowerTicks = LW_UNSIGNED_ROUNDED_DIV(
                pMon->samplingPeriodLowPowerms * (LwU32)1000, LWRTOS_TICK_PERIOD_US);
        }

        pMon->sampleCount   = pHdr->sampleCount;
        pMon->lwrrentSample = 0;
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_1X)

    //
    // Hack: Due to high stack usage on GM10x, we apply a hack to reduce RM
    // supported maximum channel number from 32 to 16. This saves stack usage
    // but will cause problem if we have GM10x boards with more than 16
    // entries. Check here on channelMask and bail out early if we have more
    // than 16 entries.
    //
    if ((PMU_PROFILE_GM10X) &&
        (pHdr->super.objMask.super.pData[0] & 0xffff0000))
    {
        PMU_HALT();
        status = FLCN_ERROR;
        goto pmgrPwrMonitorIfaceModel10SetHeader_done;
    }

    // Copy the new channels
    pMon->totalGpuPowerChannelMask = pHdr->totalGpuPowerChannelMask;
    pMon->physicalChannelMask      = pHdr->physicalChannelMask;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_2X_READABLE_CHANNEL))
    {
        // Init and copy in the readable channel mask.
        boardObjGrpMaskInit_E32(&(pMon->readableChannelMask));
        status = boardObjGrpMaskImport_E32(&(pMon->readableChannelMask),
                                           &(pHdr->readableChannelMask));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pmgrPwrMonitorIfaceModel10SetHeader_done;
        }
    }

pmgrPwrMonitorIfaceModel10SetHeader_done:
    return status;
}

/*!
 * Constructs the PWR_MONITOR object (as applicable).
 *
 * @copydoc PwrMonitorConstruct
 */
FLCN_STATUS
pwrMonitorConstruct
(
    PWR_MONITOR **ppMonitor
)
{
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNEL_35))
    {
        pwrMonitorConstruct_35(ppMonitor);
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_SEMAPHORE))
    {
        status = lwrtosSemaphoreCreateBinaryGiven(&PwrMonitorChannelQueryMutex, OVL_INDEX_OS_HEAP);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrMonitorConstruct_exit;
        }
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNELS_PMUMON))
    {
        status = pmgrPwrChannelsPmumonConstruct();
        if (status != FLCN_OK)
        {
            goto pwrMonitorConstruct_exit;
        }
    }

pwrMonitorConstruct_exit:
    return status;
}

/*!
 * Retrieves the latest PWR_CHANNEL status and sends it to THERM/FAN
 * task for implementing Fan Stop sub-policy.
 *
 * @param[in]   pPwrChannelQuery  Pointer to the dispatch structure
 *
 * @return FLCN_OK
 *      PWR_CHANNEL status successfully obtained.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      Interface isn't supported.
 * @return Other unexpected errors
 *      Unexpected errors propagated from other functions.
 */
FLCN_STATUS
pmgrPwrMonitorChannelQuery
(
    DISPATCH_PMGR_PWR_CHANNEL_QUERY *pPwrChannelQuery
)
{
    DISP2THERM_CMD             disp2Therm = {{ 0 }};
    FLCN_STATUS                status     = FLCN_ERR_ILWALID_STATE;
    LW2080_CTRL_PMGR_PWR_TUPLE tuple      = { 0 };

    // If PWR_MONITOR not allocated yet, we're obviously not supported.
    if (Pmgr.pPwrMonitor != NULL)
    {
        // Sanity check the channel index passed in
        if (!PWR_CHANNEL_IS_VALID(pPwrChannelQuery->pwrChIdx))
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto pmgrPwrMonitorChannelQuery_exit;
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_CHANNEL_35))
        {
            status = pwrMonitorPolledTupleGet(pPwrChannelQuery->pwrChIdx,
                &tuple);
        }
        else
        {
            status = pwrMonitorChannelTupleQuery(Pmgr.pPwrMonitor,
                pPwrChannelQuery->pwrChIdx, &tuple);
        }
    }

pmgrPwrMonitorChannelQuery_exit:
    disp2Therm.hdr.unitId = RM_PMU_UNIT_FAN;
    disp2Therm.hdr.eventType = FANCTRL_EVENT_ID_PMGR_PWR_CHANNEL_QUERY_RESPONSE;
    disp2Therm.fanctrl.queryResponse.pmuStatus    = status;
    disp2Therm.fanctrl.queryResponse.pwrmW        = tuple.pwrmW;
    disp2Therm.fanctrl.queryResponse.fanPolicyIdx = pPwrChannelQuery->fanPolicyIdx;

    lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, THERM),
                              &disp2Therm, sizeof(disp2Therm));

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
FLCN_STATUS
pmgrPwrMonitorIfaceModel10SetHeader_DEFAULT
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    FLCN_STATUS status = FLCN_OK;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_LAZY_CONSTRUCT) &&
        (Pmgr.pPwrMonitor == NULL))
    {
        Pmgr.pPwrMonitor = lwosCallocType(pBObjGrp->ovlIdxDmem, 1, PWR_MONITOR);
        if (Pmgr.pPwrMonitor == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            goto pmgrPwrMonitorIfaceModel10SetHeader_DEFAULT_exit;
        }
    }
    //
    // If LAZY_CONSTRUCT disabled and pPwrMonitor (is) not pre-constructed
    // (i.e. NULL) bail out with error.  Need to implement @ref
    // PwrMonitorConstruct (for) the given PWR_MONITOR version.
    //
    else if (Pmgr.pPwrMonitor == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto pmgrPwrMonitorIfaceModel10SetHeader_DEFAULT_exit;
    }
    pmgrPwrMonitorIfaceModel10SetHeader(pModel10, pHdrDesc);

pmgrPwrMonitorIfaceModel10SetHeader_DEFAULT_exit:
    return status;
}
