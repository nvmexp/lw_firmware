/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
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

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static LwBool              s_pwrMonitor(PWR_MONITOR *pMon)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "s_pwrMonitor")
    GCC_ATTRIB_NOINLINE();
static void                s_pwrMonitorSample(PWR_MONITOR *pMon)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "s_pwrMonitorSample")
    GCC_ATTRIB_NOINLINE();
static void                s_pwrMonitorIterationComplete(PWR_MONITOR *pMon)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "s_pwrMonitorIterationComplete")
    GCC_ATTRIB_NOINLINE();
static FLCN_STATUS s_pwrMonitorChannelsQuery (PWR_MONITOR *pMon, LwU32 channelMask, RM_PMU_PMGR_PWR_CHANNELS_QUERY_PAYLOAD *pPayload)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "s_pwrMonitorChannelsQuery")
    GCC_ATTRIB_NOINLINE();

/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc PwrMonitorDelay
 */
LwU32
pwrMonitorDelay
(
    PWR_MONITOR *pMon
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_MONITOR_LOW_POWER_POLLING)
    PERF_DOMAIN_GROUP_LIMITS domGrpLimits;

    //
    // If this GPU supports a separate "low power" polling period, check if the
    // GPU is in the "low power" state - i.e. lowest pstate, and that PMU power
    // policies are not holding it there.
    //
    if ((pMon->samplingPeriodLowPowerTicks != 0) &&
        (perfVoltageuVGet() != 0) &&
        (perfDomGrpGetValue(RM_PMU_PERF_DOMAIN_GROUP_PSTATE) == 0))
    {
        //
        // Check that PMU power policies aren't limiting to the lowest pstate -
        // if so, still need normal sampling rate so can uncap the clocks ASAP.
        //
        pwrPolicyDomGrpLimitsGetMin(&domGrpLimits);
        if (domGrpLimits.values[RM_PMU_PERF_DOMAIN_GROUP_PSTATE] != 0)
        {
            return pMon->samplingPeriodLowPowerTicks;
        }
    }
#endif
    return pMon->samplingPeriodTicks;
}

/*!
 * @copydoc PwrMonitor
 * The function is called inside task6 entry function
 * calling a static helper function to hide overlay info.
 */
LwBool
pwrMonitor
(
    PWR_MONITOR *pMon
)
{
    LwBool          bRet          = LW_FALSE;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_LIB_PWR_MONITOR
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        bRet = s_pwrMonitor(pMon);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return bRet;
}

/*!
 * @copydoc PwrMonitor
 */
static LwBool
s_pwrMonitor
(
    PWR_MONITOR *pMon
)
{
    LwBool bIterationComplete = LW_FALSE;

    // Make sure that we have valid PWR_MONITOR settings
    if (pMon->sampleCount == 0)
    {
        return bIterationComplete;
    }

    // Do a sample for this iteration
    s_pwrMonitorSample(pMon);
    pMon->lwrrentSample++;

    // If we've done all samples for the iteration, call the iteration code
    if (pMon->lwrrentSample == pMon->sampleCount)
    {
        s_pwrMonitorIterationComplete(pMon);

        bIterationComplete = LW_TRUE;
        pMon->lwrrentSample = 0;
    }

    return bIterationComplete;
}

/*!
 * @copydoc PwrMonitorChannelStatusGet
 */
FLCN_STATUS
pwrMonitorChannelStatusGet
(
    PWR_MONITOR  *pMon,
    LwU8          channelIdx,
    LwU32        *pPowermW
)
{
    PWR_CHANNEL *pChannel;
    FLCN_STATUS status = FLCN_OK;

    // Make sure that requested channels are supported!
    if ((Pmgr.pwr.channels.objMask.super.pData[0] & BIT(channelIdx)) == 0)
    {
        status = FLCN_ERR_NOT_SUPPORTED;
        goto pwrMonitorChannelStatusGet_exit;
    }

    pChannel = PWR_CHANNEL_GET(channelIdx);
    if (pChannel == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto pwrMonitorChannelStatusGet_exit;
    }

    // Copy the channel power
    *pPowermW = pChannel->pwrAvgmW;

pwrMonitorChannelStatusGet_exit:
    return status;
}

/*!
 * @copydoc PwrMonitorhannelsQuery
 */
FLCN_STATUS
pwrMonitorChannelsQuery
(
    PWR_MONITOR                            *pMon,
    LwU32                                   channelMask,
    RM_PMU_PMGR_PWR_CHANNELS_QUERY_PAYLOAD *pPayload
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibQuery)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = s_pwrMonitorChannelsQuery(pMon, channelMask, pPayload);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * Samples all the PWR_CHANNELs associated with this PWR_MONITOR.
 *
 * @param[in] pMon        PWR_MONITOR pointer
 */
static void
s_pwrMonitorSample
(
    PWR_MONITOR *pMon
)
{
    PWR_CHANNEL *pChannel;
    LwU8         c;

    FOR_EACH_INDEX_IN_MASK(32, c, Pmgr.pwr.channels.objMask.super.pData[0])
    {
        DBG_PRINTF_PMGR(("MS: c: %d\n", c, 0, 0, 0));
        pChannel = PWR_CHANNEL_GET(c);
        if (pChannel == NULL)
        {
            PMU_BREAKPOINT();
            return;
        }
        pwrChannelSample(pChannel);
    }
    FOR_EACH_INDEX_IN_MASK_END;
}

/*!
 * Iterates all the PWR_CHANNELs associated with this PWR_MONITOR, sending each
 * of them the pwrChannelRefresh() command which will complete the power
 * statistics collection for this iteration.
 *
 * @param[in] pMon        PWR_MONITOR pointer
 */
static void
s_pwrMonitorIterationComplete
(
    PWR_MONITOR *pMon
)
{
    PWR_CHANNEL    *pChannel;
    LwU8            c;
    DISP2THERM_CMD  disp2Therm = {{ 0 }};
    LwBool          bTgpConstituent;
    LwU32           tgpmW = 0;
    LwU32           powermW;

    FOR_EACH_INDEX_IN_MASK(32, c, Pmgr.pwr.channels.objMask.super.pData[0])
    {
        pChannel = PWR_CHANNEL_GET(c);
        if (pChannel == NULL)
        {
            PMU_BREAKPOINT();
            return;
        }
        pwrChannelRefresh(pChannel);

        //
        // If THERM/SMBPBI task enabled, send UPDATE of latest PWR_CHANNEL
        // values.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
        {
            powermW = 0;
            (void)pwrMonitorChannelStatusGet(pMon, c, &powermW);
            bTgpConstituent = (BIT(c) & pMon->totalGpuPowerChannelMask) != 0;
            if (bTgpConstituent)
            {
                tgpmW += powermW;
            }
        }
    }
    FOR_EACH_INDEX_IN_MASK_END;

    if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI))
    {
        disp2Therm.hdr.unitId = RM_PMU_UNIT_SMBPBI;
        disp2Therm.hdr.eventType = SMBPBI_EVT_PWR_MONITOR_TGP_UPDATE;
        disp2Therm.smbpbi.pwrMonTgpUpdate.totalGpuPowermW = tgpmW;
        lwrtosQueueIdSendBlocking(LWOS_QUEUE_ID(PMU, THERM),
                                  &disp2Therm, sizeof(disp2Therm));
    }
}

/*!
 * @copydoc PwrMonitorChannelsQuery
 */
static FLCN_STATUS
s_pwrMonitorChannelsQuery
(
    PWR_MONITOR                            *pMon,
    LwU32                                   channelMask,
    RM_PMU_PMGR_PWR_CHANNELS_QUERY_PAYLOAD *pPayload
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        channelIdx;
    LwU32       powermW;

    memset(pPayload, 0, sizeof(RM_PMU_PMGR_PWR_CHANNELS_QUERY_PAYLOAD));

    // Loop over requsted channels
    FOR_EACH_INDEX_IN_MASK(32, channelIdx, channelMask)
    {
        status = pwrMonitorChannelStatusGet(pMon, channelIdx, &powermW);
        if (status != FLCN_OK)
        {
            break;
        }
        pPayload->channels[channelIdx].pwrmW = powermW;
    }
    FOR_EACH_INDEX_IN_MASK_END;
    return status;
}
