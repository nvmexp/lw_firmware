/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrchannel_pmumon.c
 * @brief PMGR Power Channel PMUMON functionality.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "lib/lib_pmumon.h"
#include "pmu_objpmgr.h"
#include "pmgr/pwrchannel_pmumon.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"

/* ------------------------- Macros and Defines ----------------------------- */
#define ONE_SECOND                                                    1000000000
#define AVERAGE_POWER_BUFFER_SIZE                                            200

/* ------------------------- Type Definitions ------------------------------- */
typedef struct _PWR_CHANNELS_INTERNAL_QUEUE_DESC
{
    /*!
     * Head index indicates the latest item in the queue.
     */
    LwU32 headIndex;

    /*!
     * The size of the queue.
     */
    LwU32 size;
} PWR_CHANNELS_INTERNAL_QUEUE_DESC;

typedef struct _PWR_CHANNELS_PMUMON_SAMPLE
{
    /*!
     * Ptimer timestamp of when this data was collected.
     */
    LW2080_CTRL_PMUMON_SAMPLE super;

    /*!
     * Total GPU power in milliwatts.
     *
     * LW2080_CTRL_PMGR_PMUMON_PWR_CHANNELS_SAMPLE_ILWALID if not supported.
     */
    LwU32 tgpmW;

    /*!
     * Core power in milliwatts.
     *
     * LW2080_CTRL_PMGR_PMUMON_PWR_CHANNELS_SAMPLE_ILWALID if not supported.
     */
    LwU32 coremW;

} PWR_CHANNELS_PMUMON_SAMPLE;

/*!
 * @todo Document.
 */
typedef struct _PWR_CHANNELS_PMUMON
{
    /*!
     * @todo Document.
     */
    LwBoardObjIdx tgpChannelIndex;

    /*!
     * @todo Document.
     */
    LwBoardObjIdx coreChannelIndex;

    /*!
     * @todo Document.
     */
    PWR_CHANNELS_STATUS status;

    /*!
     * Internal queue descriptor.
     */
    PWR_CHANNELS_INTERNAL_QUEUE_DESC internalQueueDesc;

    /*!
     * Internal queue that cache raw power data from sensors. With sample period
     * being potentially as fast every 5ms, this gives us 1 seconds worth of
     * data.
     */
    PWR_CHANNELS_PMUMON_SAMPLE internalQueue[AVERAGE_POWER_BUFFER_SIZE];

    /*!
     * The latest sample ready to be published to PMUMON queue.
     */
    LW2080_CTRL_PMGR_PMUMON_PWR_CHANNELS_SAMPLE latestSample;
} PWR_CHANNELS_PMUMON;

/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/*!
 * @todo Move to PWR_CHANNELS_PMUMON
 */
static PMUMON_QUEUE_DESCRIPTOR s_pwrChannelsPmumonQueueDesc
    GCC_ATTRIB_SECTION("dmem_pmgr", "s_pwrChannelsPmumonQueueDesc");

/*!
 * @todo Move to PWR_MONITOR
 */
static PWR_CHANNELS_PMUMON s_pwrChannelsPmumon
    GCC_ATTRIB_SECTION("dmem_pmgr", "s_pwrChannelsPmumon");

static OS_TMR_CALLBACK OsCBPwrChannelsPmuMon;

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Private Functions ------------------------------ */
static OsTmrCallbackFunc (s_pwrChannelsPmumonCallback)
    GCC_ATTRIB_SECTION("imem_pmgrPwrChannelsCallback", "s_pwrChannelsPmumonCallback")
    GCC_ATTRIB_USED();
static FLCN_STATUS s_pwrChannelsPmumonPublishAndAverage(PWR_CHANNELS_PMUMON_SAMPLE *pSample);
static LwU32 s_pwrChannelsPmumonIndexIncrement(LwU32 index);
static LwU32 s_pwrChannelsPmumonIndexDecrement(LwU32 index);
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc pmgrPwrChannelsPmumonConstruct
 */
FLCN_STATUS
pmgrPwrChannelsPmumonConstruct(void)
{
    FLCN_STATUS status;

    status = pmumonQueueConstruct(
        &s_pwrChannelsPmumonQueueDesc,
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.pmgr.pwrChannelsPmumonQueue.header),
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.pmgr.pwrChannelsPmumonQueue.buffer),
        sizeof(LW2080_CTRL_PMGR_PMUMON_PWR_CHANNELS_SAMPLE),
        LW2080_CTRL_PMGR_PMUMON_PWR_CHANNELS_SAMPLE_COUNT);
    if (status != FLCN_OK)
    {
        goto pmgrPwrChannelsPmumonConstruct_exit;
    }

pmgrPwrChannelsPmumonConstruct_exit:
    return FLCN_OK;
}

/*!
 * @copydoc pmgrPwrChannelsPmumonLoad
 */
FLCN_STATUS
pmgrPwrChannelsPmumonLoad(void)
{
    FLCN_STATUS     status = FLCN_OK;
    PWR_POLICY     *pPolicy;

    memset(&s_pwrChannelsPmumon, 0x0, sizeof(s_pwrChannelsPmumon));
    PWR_CHANNELS_STATUS_MASK_INIT(&s_pwrChannelsPmumon.status.mask);
    s_pwrChannelsPmumon.tgpChannelIndex  = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;
    s_pwrChannelsPmumon.coreChannelIndex = LW2080_CTRL_BOARDOBJ_IDX_ILWALID;
    s_pwrChannelsPmumon.internalQueueDesc.size = AVERAGE_POWER_BUFFER_SIZE;
    s_pwrChannelsPmumon.internalQueueDesc.headIndex = 0;

    // Nothing to do when there are no CHANNEL objects.
    if (boardObjGrpMaskIsZero(&(Pmgr.pwr.channels.objMask)))
    {
        goto pmgrPwrChannelsPmumonLoad_exit;
    }

    // Kernel and TGP index are mutually exclusive, so if
    // one of them isn't valid, try the other.
    pPolicy = PMGR_GET_PWR_POLICY_IDX(LW2080_CTRL_PMGR_PWR_POLICY_IDX_TGP);
    if (pPolicy == NULL)
    {
        pPolicy = PMGR_GET_PWR_POLICY_IDX(LW2080_CTRL_PMGR_PWR_POLICY_IDX_KERNEL);
    }
    if (pPolicy != NULL)
    {
        s_pwrChannelsPmumon.tgpChannelIndex = pPolicy->chIdx;
    }

    pPolicy = PMGR_GET_PWR_POLICY_IDX(LW2080_CTRL_PMGR_PWR_POLICY_IDX_CORE);
    if (pPolicy != NULL)
    {
        s_pwrChannelsPmumon.coreChannelIndex = pPolicy->chIdx;
    }

    if (s_pwrChannelsPmumon.tgpChannelIndex != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        boardObjGrpMaskBitSet(&s_pwrChannelsPmumon.status.mask, s_pwrChannelsPmumon.tgpChannelIndex);
    }

    if (s_pwrChannelsPmumon.coreChannelIndex != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        boardObjGrpMaskBitSet(&s_pwrChannelsPmumon.status.mask, s_pwrChannelsPmumon.coreChannelIndex);
    }

    // Nothing to do when there are no channels to query.
    if (boardObjGrpMaskIsZero(&s_pwrChannelsPmumon.status.mask))
    {
        goto pmgrPwrChannelsPmumonLoad_exit;
    }

    // Burn first read
    status = pwrChannelsStatusGet(&s_pwrChannelsPmumon.status);
    if (status != FLCN_OK)
    {
        goto pmgrPwrChannelsPmumonLoad_exit;
    }

    if (!OS_TMR_CALLBACK_WAS_CREATED(&(OsCBPwrChannelsPmuMon)))
    {
        status = osTmrCallbackCreate(&(OsCBPwrChannelsPmuMon),              // pCallback
                    OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED, // type
                    OVL_INDEX_IMEM(pmgrPwrChannelsCallback),                // ovlImem
                    s_pwrChannelsPmumonCallback,                            // pTmrCallbackFunc
                    LWOS_QUEUE(PMU, PMGR),                                  // queueHandle
                    PWR_CHANNEL_PMUMON_ACTIVE_POWER_CALLBACK_PERIOD_US(),   // periodNormalus
                    PWR_CHANNEL_PMUMON_LOW_POWER_CALLBACK_PERIOD_US(),      // periodSleepus
                    OS_TIMER_RELAXED_MODE_USE_NORMAL,                       // bRelaxedUseSleep
                    RM_PMU_TASK_ID_PMGR);                                   // taskId
        if (status != FLCN_OK)
        {
            goto pmgrPwrChannelsPmumonLoad_exit;
        }
    }

    status = osTmrCallbackSchedule(&(OsCBPwrChannelsPmuMon));
    if (status != FLCN_OK)
    {
        goto pmgrPwrChannelsPmumonLoad_exit;
    }

pmgrPwrChannelsPmumonLoad_exit:
    return status;
}

/*!
 * @copydoc pmgrPwrChannelsPmumonUnload
 */
void
pmgrPwrChannelsPmumonUnload(void)
{
    osTmrCallbackCancel(&OsCBPwrChannelsPmuMon);
}

/*!
 * @copydoc pmgrPwrChannelsPmumonGetAverageTotalGpuPowerUsage
 */
LwU32
pmgrPwrChannelsPmumonGetAverageTotalGpuPowerUsage
(
)
{
    return s_pwrChannelsPmumon.latestSample.averageTgpmW;
}
/* ------------------------- Private Functions ------------------------------ */
/*!
 * @copydoc OsTmrCallbackFunc
 */
static FLCN_STATUS
s_pwrChannelsPmumonCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    FLCN_STATUS                                 status;
    PWR_CHANNELS_PMUMON_SAMPLE                  sample;

    status = pwrChannelsStatusGet(&s_pwrChannelsPmumon.status);
    if (status != FLCN_OK)
    {
        goto s_pwrChannels35Callback_exit;
    }

    osPTimerTimeNsLwrrentGetAsLwU64(&sample.super.timestamp);

    // Tgp power sample
    if (s_pwrChannelsPmumon.tgpChannelIndex != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        sample.tgpmW = s_pwrChannelsPmumon.status.channels[s_pwrChannelsPmumon.tgpChannelIndex].channel.tuple.pwrmW;
    }
    else
    {
        sample.tgpmW = LW2080_CTRL_PMGR_PMUMON_PWR_CHANNELS_SAMPLE_ILWALID;
    }

    // Core power sample
    if (s_pwrChannelsPmumon.coreChannelIndex != LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
    {
        sample.coremW = s_pwrChannelsPmumon.status.channels[s_pwrChannelsPmumon.coreChannelIndex].channel.tuple.pwrmW;
    }
    else
    {
        sample.coremW = LW2080_CTRL_PMGR_PMUMON_PWR_CHANNELS_SAMPLE_ILWALID;
    }

    status = s_pwrChannelsPmumonPublishAndAverage(&sample);
    if (status != FLCN_OK)
    {
        goto s_pwrChannels35Callback_exit;
    }

#if PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = lwosUstreamerLog
    (
        pmumonUstreamerQueueId,
        LW2080_CTRL_PMUMON_USTREAMER_EVENT_PWR_CHANNELS,
        (LwU8*)&s_pwrChannelsPmumon.latestSample,
        sizeof(LW2080_CTRL_PMGR_PMUMON_PWR_CHANNELS_SAMPLE)
    );
    lwosUstreamerFlush(pmumonUstreamerQueueId);
#else // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    status = pmumonQueueWrite(&s_pwrChannelsPmumonQueueDesc,
        &s_pwrChannelsPmumon.latestSample);
#endif // PMUCFG_FEATURE_ENABLED(PMU_LIB_PMUMON_OVER_USTREAMER)
    if (status != FLCN_OK)
    {
        goto s_pwrChannels35Callback_exit;
    }

s_pwrChannels35Callback_exit:
    return FLCN_OK; // NJ-TODO-CB
}

/*!
 * @brief Publish a sample to the internal queue and compute the moving average.
 *
 * @param[in] pSample      Pointer to the sample ready to be published
 *
 * @return FLCN_OK on success
 */
static FLCN_STATUS
s_pwrChannelsPmumonPublishAndAverage
(
    PWR_CHANNELS_PMUMON_SAMPLE *pSample
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32       index = 0;
    LwU32       count = 0;
    LwU64       prev  = 0;
    LwU64       lwrr  = 0;
    LwU32       totalTimeWnd = 0;
    LwF32       tgpmW  = 0.0;
    LwF32       coremW = 0.0;
    LwF32       weight = 0.0;
    PWR_CHANNELS_PMUMON_SAMPLE *pQueue = s_pwrChannelsPmumon.internalQueue;

    if (pQueue == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto s_pwrChannelsPmumonPublishAndAverage_exit;
    }

    index = s_pwrChannelsPmumon.internalQueueDesc.headIndex;

    // Publish to the internal queue
    index = s_pwrChannelsPmumonIndexIncrement(index);
    pQueue[index].super.timestamp = pSample->super.timestamp;
    pQueue[index].tgpmW = pSample->tgpmW;
    pQueue[index].coremW = pSample->coremW;
    s_pwrChannelsPmumon.internalQueueDesc.headIndex = index;

    // Compute average in last second
    lwrr = pQueue[index].super.timestamp;
    while (totalTimeWnd < ONE_SECOND)
    {
        prev = pQueue[s_pwrChannelsPmumonIndexDecrement(index)].super.timestamp;
        if (prev == 0)
        {
            // current sample is the first sample
            break;
        }

        totalTimeWnd += LwU64_LO32(lwrr - prev);

        index = s_pwrChannelsPmumonIndexDecrement(index);
        lwrr = pQueue[index].super.timestamp;
        ++count;
    }

    index = s_pwrChannelsPmumon.internalQueueDesc.headIndex;
    lwrr = pQueue[index].super.timestamp;
    while (count != 0)
    {
        prev = pQueue[s_pwrChannelsPmumonIndexDecrement(index)].super.timestamp;
        weight = lwF32ColwertFromU32(LwU64_LO32(lwrr - prev))/totalTimeWnd;

        tgpmW += pQueue[index].tgpmW*weight;
        coremW += pQueue[index].coremW*weight;

        index = s_pwrChannelsPmumonIndexDecrement(index);
        lwrr = pQueue[index].super.timestamp;
        --count;
    }

    s_pwrChannelsPmumon.latestSample.super.timestamp = pSample->super.timestamp;
    s_pwrChannelsPmumon.latestSample.tgpmW = pSample->tgpmW;
    s_pwrChannelsPmumon.latestSample.coremW = pSample->coremW;
    s_pwrChannelsPmumon.latestSample.averageTgpmW = tgpmW;
    s_pwrChannelsPmumon.latestSample.averageCoremW = coremW;

s_pwrChannelsPmumonPublishAndAverage_exit:
    return status;
}

static LwU32
s_pwrChannelsPmumonIndexIncrement
(
    LwU32 index
)
{
    return (index + 1) % s_pwrChannelsPmumon.internalQueueDesc.size;
}

static LwU32
s_pwrChannelsPmumonIndexDecrement
(
    LwU32 index
)
{
    return (index == 0) ?
        (s_pwrChannelsPmumon.internalQueueDesc.size - 1): (index - 1);
}
