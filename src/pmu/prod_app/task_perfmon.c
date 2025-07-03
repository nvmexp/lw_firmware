/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2009-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   task_perfmon.c
 * @brief  Performance Monitoring Task
 *
 * Task that performs Perfmon idle counter sampling and generates events to the
 * RM based on the sampling results.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "main_init_api.h"
#include "cmdmgmt/cmdmgmt.h"
#include "dbgprintf.h"
#include "lib/lib_fxp.h"
#include "logger.h"
#include "pmu_perfmon.h"
#include "pmu_objpmu.h"
#include "pmu_objgcx.h"
#include "pmu_objfifo.h"
#include "pmu_objsmbpbi.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Macros and Defines ----------------------------- */

/*! Given a Perfmon context, are increase events lwrrently enabled? */
#define IS_INCREASE_ENABLED(c,g)                                              \
    (!!((c)->flags[g] & RM_PMU_PERFMON_FLAG_ENABLE_INCREASE))

/*! Given a Perfmon context, are decrease events lwrrently enabled? */
#define IS_DECREASE_ENABLED(c,g)                                              \
    (!!((c)->flags[g] & RM_PMU_PERFMON_FLAG_ENABLE_DECREASE))

/*! Given a Perfmon context, does the specified counter us a moving average? */
#define USE_MOVING_AVERAGE(c,i)                                               \
    (!!((c)->pCounters[i].flags &                                             \
     RM_PMU_PERFMON_COUNTER_FLAG_USE_MOVING_AVERAGE))

/*!
 * Given a Perfmon context, does the specified counter measure GR
 * utilization?
 */
#define IS_GR_UTIL_COUNTER(c,i)                                              \
    (!!((c)->pCounters[i].flags &                                            \
     RM_PMU_PERFMON_COUNTER_FLAG_TYPE_GR_UTIL_COUNTER))

/*!
 * Given a Perfmon context, does the specified counter measure FB
 * utilization?
 */
#define IS_FB_UTIL_COUNTER(c,i)                                              \
    (!!((c)->pCounters[i].flags &                                            \
     RM_PMU_PERFMON_COUNTER_FLAG_TYPE_FB_UTIL_COUNTER))

/*!
 * Given a Perfmon context, does the specified counter measure LWENC0
 * utilization?
 */
#define IS_LWENC0_UTIL_COUNTER(c,i)                                           \
    (!!((c)->pCounters[i].flags &                                            \
     RM_PMU_PERFMON_COUNTER_FLAG_TYPE_LWENC0_UTIL_COUNTER))

/*!
 * Given a Perfmon context, does the specified counter measure LWENC1
 * utilization?
 */
#define IS_LWENC1_UTIL_COUNTER(c,i)                                          \
    (!!((c)->pCounters[i].flags &                                            \
     RM_PMU_PERFMON_COUNTER_FLAG_TYPE_LWENC1_UTIL_COUNTER))

/*!
 * Given a Perfmon context, does the specified counter measure LWENC2
 * utilization?
 */
#define IS_LWENC2_UTIL_COUNTER(c,i)                                          \
    (!!((c)->pCounters[i].flags &                                            \
     RM_PMU_PERFMON_COUNTER_FLAG_TYPE_LWENC2_UTIL_COUNTER))

/*!
 * Given a Perfmon context, does the specified counter measure LWDEC
 * utilization?
 */
#define IS_LWDEC_UTIL_COUNTER(c,i)                                           \
    (!!((c)->pCounters[i].flags &                                            \
     RM_PMU_PERFMON_COUNTER_FLAG_TYPE_LWDEC_UTIL_COUNTER))

/*!
 * Given a Perfmon context, does the specified counter measure PCI-E
 * Tx utilization?
 */
#define IS_PEX_TX_UTIL_COUNTER(c,i)                                           \
    (((c)->pCounters[i].flags &                                            \
     RM_PMU_PERFMON_COUNTER_FLAG_USE_PEX_TX_UTIL_COUNTER)!=0)

/*!
 * Given a Perfmon context, does the specified counter measure PCI-E
 * Rx utilization?
 */
#define IS_PEX_RX_UTIL_COUNTER(c,i)                                           \
    (((c)->pCounters[i].flags &                                            \
     RM_PMU_PERFMON_COUNTER_FLAG_USE_PEX_RX_UTIL_COUNTER)!=0)

/* ------------------------- Prototypes ------------------------------------- */
static void         s_perfmonGetGpumonSample(PERFMON_CONTEXT *pCtx, RM_PMU_GPUMON_PERFMON_SAMPLE *pSample)
    GCC_ATTRIB_SECTION("imem_libGpumon", "s_perfmonGetGpumonSample")
    GCC_ATTRIB_NOINLINE();
static void         s_perfmonLogGpumonSample(PERFMON_CONTEXT *pCtx, LwU32 delayUs)
    GCC_ATTRIB_SECTION("imem_libGpumon", "s_perfmonLogGpumonSample")
    GCC_ATTRIB_NOINLINE();
static void         s_perfmonLogGpumonSampleHelper(PERFMON_CONTEXT *pCtx, LwU32 delayUs);
static FLCN_STATUS  s_perfmonEventHandle(DISP2UNIT_RM_RPC *pRequest)
    GCC_ATTRIB_NOINLINE();
static void         s_perfmonEventsCheck(PERFMON_CONTEXT *pCtx);

lwrtosTASK_FUNCTION(task_perfmon, pvParameters);

/* ------------------------- Global Variables ------------------------------- */
PERFMON_CONTEXT   PerfmonContext;

#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TMR_CALLBACK OsCBPerfMon;
#else
OS_TIMER        PerfmonOsTimer;
#endif

//
// These time-stamps are used in perfmonDoSample to callwlate elapsed time since
// last time this routinue was exelwted.
//
FLCN_TIMESTAMP   PerfmonPreTime;

/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Static function Prototypes --------------------- */
static OsTimerCallback (perfmonDoSample)
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc (perfmonOsDoSample)
    GCC_ATTRIB_USED();

/* ------------------------- Private Functions ------------------------------ */

/*!
 * Enable or disable all events to the RM.
 *
 * @param[in]  pCtx     Current Perfmon context.
 * @param[in]  bEnable  LW_TRUE to enable event, LW_FALSE to disable events.
 */
static void
perfmonEnableEvents
(
    PERFMON_CONTEXT *pCtx,
    LwBool           bEnable,
    LwU8             groupId
)
{
    if (bEnable)
    {
        pCtx->flags[groupId] = RM_PMU_PERFMON_FLAG_ENABLE_INCREASE |
                               RM_PMU_PERFMON_FLAG_ENABLE_DECREASE;
    }
    else
    {
        pCtx->flags[groupId] = 0;
    }
}

/*!
 * Send a Perfmon event to the RM.
 *
 * @param[in]  pCtx       Current Perfmon context.
 * @param[in]  groupId    groupId of event.
 * @param[in]  data       data for the event.
 * @param[in]  bIncrease  True if a INCREASE event, false means DECREASE event.
 *
 * @return     status returned from pmuRmRpcExelwte().
 */
static FLCN_STATUS
perfmonSendEvent
(
    PERFMON_CONTEXT  *pCtx,
    LwU8              groupId,
    LwU8              data,
    LwBool            bIncrease
)
{
    PMU_RM_RPC_STRUCT_PERFMON_CHANGE_EVENT  rpc;
    FLCN_STATUS                             status;

    rpc.stateId   = pCtx->stateId[groupId];
    rpc.groupId   = groupId;
    rpc.data      = data;
    rpc.bIncrease = bIncrease;

    PMU_RM_RPC_EXELWTE_BLOCKING(status, PERFMON, CHANGE_EVENT, &rpc);

    return status;
}

/*!
 * Process initialization command that provides state setup information.
 */
FLCN_STATUS
pmuRpcPerfmonInit
(
    RM_PMU_RPC_STRUCT_PERFMON_INIT *pParams
)
{
    PMU_RM_RPC_STRUCT_PERFMON_INIT_EVENT rpc;
    PERFMON_CONTEXT                     *pCtx               = &PerfmonContext;
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_PEX_UTIL_COUNTERS))
    LwBool                               bPexCntInitialized = LW_FALSE;
#endif
    FLCN_STATUS                          status;
    LwU32                                i;

    pCtx->toDecreaseCount    = pParams->toDecreaseCount;
    pCtx->baseCounterId      = pParams->baseCounterId;
    pCtx->numCounters        = pParams->numCounters;
    pCtx->bInitialized       = LW_TRUE;
    pCtx->samplesInMovingAvg = pParams->samplesInMovingAvg;

#if PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING)
    // counters run at pwrclk freq
    pCtx->countersFreqMHz    = PmuInitArgs.cpuFreqHz / 1000000;
#endif

    //
    // Copy over only max supported counters for now.
    // In future this will be changed to return error to RM.
    //
    pParams->numCounters =
        LW_MIN(pParams->numCounters, RM_PMU_PERFMON_MAX_COUNTERS);

    // Allocate space for the utilization samples and set it to 0.
    pCtx->pSampleBuffer = lwosCallocResidentType(pCtx->numCounters, LwU16);
    if (pCtx->pSampleBuffer == NULL)
    {
        PMU_HALT();
        return FLCN_ERR_NO_FREE_MEM;
    }

    for (i = 0; i < RM_PMU_DOMAIN_GROUP_NUM; i++)
    {
        perfmonEnableEvents(pCtx, LW_FALSE, i);
    }

    // Allocate space for the counters.
    pCtx->pCounters =
        lwosMallocResidentType(pCtx->numCounters, RM_PMU_PERFMON_COUNTER);
    if (pCtx->pCounters == NULL)
    {
        PMU_HALT();
        return FLCN_ERR_NO_FREE_MEM;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
    // Allocate space for the counters to store previously sampled count.
    pCtx->pCountersHist =
        lwosMallocResidentType(pCtx->numCounters, RM_PMU_PERFMON_COUNTER_HIST);
    if (pCtx->pCountersHist == NULL)
    {
        PMU_HALT();
        return FLCN_ERR_NO_FREE_MEM;
    }
#endif // PMU_PERFMON_NO_RESET_COUNTERS

#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
    appTaskCriticalEnter();
#endif // PMU_PERFMON_NO_RESET_COUNTERS
    {
        // Copy over the counter initialization data: flags and index.
        for (i = 0; i < pCtx->numCounters; i++)
        {
            RM_PMU_PERFMON_COUNTER *pCounter = &(pParams->counter[i]);

            pCtx->pCounters[i].flags   = pCounter->flags;
            pCtx->pCounters[i].index   = pCounter->index;
            pCtx->pCounters[i].groupId = pCounter->groupId;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_PEX_UTIL_COUNTERS))
            // If PEX counters are already initialized, no need to re-initialize them
            if (!bPexCntInitialized)
            {
                //
                // If we are interested in PEX Tx or Rx utilization metrics,
                // initialize and enable the utilization counters
                //
                if ((IS_PEX_TX_UTIL_COUNTER(pCtx, i)) ||
                    (IS_PEX_RX_UTIL_COUNTER(pCtx, i)))
                {
                    pmuPEXCntInit_HAL(&Pmu);
                    bPexCntInitialized = LW_TRUE;
                }
            }
#endif

#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_PEX_UTIL_COUNTERS))
            if (IS_PEX_RX_UTIL_COUNTER(pCtx, i))
            {
                pCtx->pCountersHist[i].count = pmuPEXCntGetRx_HAL(&Pmu);
            }
            else if (IS_PEX_TX_UTIL_COUNTER(pCtx, i))
            {
                pCtx->pCountersHist[i].count = pmuPEXCntGetTx_HAL(&Pmu);
            }
            else
#endif
            {
                pCtx->pCountersHist[i].count =
                    pmuIdleCounterGet_HAL(&Pmu, pCtx->pCounters[i].index);
            }
#endif // PMU_PERFMON_NO_RESET_COUNTERS
        }

        //
        // Base counter is no more configured when PMU_PERFMON_TIMER_BASED_SAMPLING
        // is enabled.
        //
#if (!PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING))
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
        // Sample base counter
        pCtx->baseCount = pmuIdleCounterGet_HAL(&Pmu, pCtx->baseCounterId);
#endif // PMU_PERFMON_NO_RESET_COUNTERS
#endif // PMU_PERFMON_TIMER_BASED_SAMPLING

        if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING) ||
            PMUCFG_FEATURE_ENABLED(PMU_PERFMON_PEX_UTIL_COUNTERS))
        {
            // Take the first time stamp for perfmon sampling.
            osPTimerTimeNsLwrrentGet(&PerfmonPreTime);
        }
    }
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
    appTaskCriticalExit();
#endif // PMU_PERFMON_NO_RESET_COUNTERS

    //
    // Schedule relwrring perfmon sampling callback.  We can just use the
    // current tick count here because we really don't care relative from when
    // we start the scheduling the callbacks - only that they're separated by
    // the sampling period.
    //
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackCreate(&OsCBPerfMon,                            // pCallback
        OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,  // type
        OVL_INDEX_ILWALID,                                       // ovlImem
        perfmonOsDoSample,                                       // pTmrCallbackFunc
        LWOS_QUEUE(PMU, PERFMON),                                // queueHandle
        pParams->samplePeriodus,                                 // periodNormalus
        pParams->samplePeriodus,                                 // periodSleepus
        OS_TIMER_RELAXED_MODE_USE_NORMAL,                        // bRelaxedUseSleep
        RM_PMU_TASK_ID_PERFMON);                                 // taskId
    osTmrCallbackSchedule(&OsCBPerfMon);
#else
    osTimerScheduleCallback(
        &PerfmonOsTimer,                                            // pOsTimer
        PERFMON_OS_TIMER_ENTRY_SAMPLE,                              // index
        lwrtosTaskGetTickCount32(),                                 // ticksPrev
        pParams->samplePeriodus,                                    // usDelay
        DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC),   // flags
        perfmonDoSample,                                            // pCallback
        NULL,                                                       // pParam
        OVL_INDEX_ILWALID);                                         // ovlIdx
#endif

    // Send an INIT event to the RM, we are good to go!
    PMU_RM_RPC_EXELWTE_BLOCKING(status, PERFMON, INIT_EVENT, &rpc);

    return status;
}

/*!
 * Stops further sampling of utilization.
 */
FLCN_STATUS
pmuRpcPerfmonDeinit
(
    RM_PMU_RPC_STRUCT_PERFMON_DEINIT *pParams
)
{
    PERFMON_CONTEXT *pCtx = &PerfmonContext;

    // Cancel Perfmon sample callback.
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackCancel(&OsCBPerfMon);
#else
    osTimerDeScheduleCallback(&PerfmonOsTimer, PERFMON_OS_TIMER_ENTRY_SAMPLE);
#endif

    // It is illegal to use counters after this.
    pCtx->numCounters   = 0;

    return FLCN_OK;
}

/*!
 * Returns utilization values computed by the task.
 */
FLCN_STATUS
pmuRpcPerfmonQuery
(
    RM_PMU_RPC_STRUCT_PERFMON_QUERY *pParams
)
{
    PERFMON_CONTEXT *pCtx = &PerfmonContext;

    memcpy(pParams->sampleBuffer, pCtx->pSampleBuffer,
        sizeof(LwU16) * pCtx->numCounters);
    return FLCN_OK;
}

/*!
 * Process command to stop current events.
 */
FLCN_STATUS
pmuRpcPerfmonStop
(
    RM_PMU_RPC_STRUCT_PERFMON_STOP *pParams
)
{
    LwU32 i;

    for (i = 0; i < RM_PMU_DOMAIN_GROUP_NUM; i++)
    {
        perfmonEnableEvents(&PerfmonContext, LW_FALSE, i);
    }

    return FLCN_OK;
}

/*!
 * Starting Perfmon sampling resets all counters and updates all thresholds
 * based on information sent by the RM.
 */
FLCN_STATUS
pmuRpcPerfmonStart
(
    RM_PMU_RPC_STRUCT_PERFMON_START *pParams
)
{
    PERFMON_CONTEXT *pCtx = &PerfmonContext;
    LwU32            i;

    pCtx->stateId[pParams->groupId] = pParams->stateId;
    pCtx->flags[pParams->groupId]   = pParams->flags;

    // Clear previous state(s).
    if (pParams->flags & RM_PMU_PERFMON_FLAG_CLEAR_PREV)
    {
        pCtx->decreaseCount[pParams->groupId] = 0;
        pCtx->decreasePct[pParams->groupId]   = 0;
    }

    // Reset counter-specific state.
    for (i = 0; i < pCtx->numCounters; i++)
    {
        RM_PMU_PERFMON_COUNTER *pCounter = &(pParams->counter[i]);

        if ((pCounter->flags & RM_PMU_PERFMON_COUNTER_FLAG_VALID_COUNTER) == 0)
        {
            continue;
        }

        pCtx->pCounters[i].upperThresholdPct = pCounter->upperThresholdPct;
        pCtx->pCounters[i].lowerThresholdPct = pCounter->lowerThresholdPct;

        if (PMUCFG_FEATURE_ENABLED(PMU_LOGGER))
        {
            pCtx->pCounters[i].scale = pCounter->scale;
        }
    }

    return FLCN_OK;
}

/*!
 * Helper function to get a gpu monitoring perfmon utilization sample.
 *
 * @param[in]  pCtx  Current Perfmon context.
 * @param[out] pSample @ref LW2080_CTRL_PERF_GPUMON_PERFMON_UTIL_SAMPLE
 */
static void
s_perfmonGetGpumonSample
(
    PERFMON_CONTEXT *pCtx,
    RM_PMU_GPUMON_PERFMON_SAMPLE *pSample
)
{
    LwU32 i;
    LwU32 util = 0;
    LwU32 lwenlwtil = 0;
    LwU32 lwencCount = 0;
    LwU8 engId = 0;

    memset(pSample, 0,
        sizeof(RM_PMU_GPUMON_PERFMON_SAMPLE));

    for (i = 0; i < pCtx->numCounters; i++)
    {
        util = multUFXP32(16, pCtx->pSampleBuffer[i], pCtx->pCounters[i].scale);

        // Clamp utilization to 100%
        if (util > 10000)
        {
            util = 10000;
        }

        if (IS_FB_UTIL_COUNTER(pCtx, i))
        {
            pSample->fb.util = util;
            // For FB, returns GR engine's context
            fifoGetEngineStatus_HAL(&Fifo, PMU_ENGINE_GR,
                (LwU32 *)&pSample->fb.context);
        }
        else if (IS_GR_UTIL_COUNTER(pCtx, i) && FIFO_IS_ENGINE_PRESENT(GR))
        {
            pSample->gr.util = util;
            fifoGetEngineStatus_HAL(&Fifo, PMU_ENGINE_GR,
                (LwU32 *)&pSample->gr.context);
        }
        //
        // As LWENCs are configured to seperate sensors (bug 1722303),
        // fill lwenc with average of utilized LWENCs and context ID of LWENC0.
        //
        else if (IS_LWENC0_UTIL_COUNTER(pCtx, i) && FIFO_IS_ENGINE_PRESENT(ENC0))
        {
            lwenlwtil += util;
            lwencCount++;
            fifoGetEngineStatus_HAL(&Fifo, PMU_ENGINE_ENC0,
                (LwU32 *)&pSample->lwenc.context);
        }
        else if (IS_LWENC1_UTIL_COUNTER(pCtx, i) && FIFO_IS_ENGINE_PRESENT(ENC1))
        {
            lwenlwtil += util;
            lwencCount++;
        }
        else if (IS_LWENC2_UTIL_COUNTER(pCtx, i) && FIFO_IS_ENGINE_PRESENT(ENC2))
        {
            lwenlwtil += util;
            lwencCount++;
        }
        else if (IS_LWDEC_UTIL_COUNTER(pCtx, i))
        {
            fifoGetLwdecPmuEngineId_HAL(&Fifo, &engId);
            if (GET_FIFO_ENG(engId) != PMU_ENGINE_MISSING)
            {
                pSample->lwdec.util = util;
                //
                // LWDEC is handled differently because PMU engine id for LWDEC is
                // repurposed on different chips.
                //
                fifoGetEngineStatus_HAL(&Fifo, engId,
                                        (LwU32 *)&pSample->lwdec.context);
            }
        }
    }
    // Callwlate avarage if LWENC engines are present.
    if (lwencCount != 0)
    {
         pSample->lwenc.util = lwenlwtil / lwencCount;
    }

    timerGetGpuLwrrentNs_HAL(&Timer, (FLCN_TIMESTAMP *)&pSample->base.timeStamp);
}

/*!
 * Helper function to log a gpu monitoring perfmon utilization sample.
 *
 * @param[in]  pCtx  Current Perfmon context.
 */
static void
s_perfmonLogGpumonSample
(
    PERFMON_CONTEXT *pCtx,
    LwU32 delayUs
)
{
    LwU8 sample[RM_PMU_LOGGER_SAMPLE_SIZE_PERFMON_UTIL*2];
    RM_PMU_GPUMON_PERFMON_SAMPLE *pSample;

    pSample = (RM_PMU_GPUMON_PERFMON_SAMPLE*)
        LW_ALIGN_UP((LwUPtr)&sample, RM_PMU_LOGGER_SAMPLE_SIZE_PERFMON_UTIL);

    ct_assert(sizeof(RM_PMU_GPUMON_PERFMON_SAMPLE) == RM_PMU_LOGGER_SAMPLE_SIZE_PERFMON_UTIL);

    s_perfmonGetGpumonSample(pCtx, pSample);

    if (PMUCFG_FEATURE_ENABLED(PMU_SMBPBI_ACLWMULATE_UTIL_COUNTERS))
    {
        FLCN_STATUS status;
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, smbpbi)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = smbpbiAclwtilCounterHelper(delayUs, pSample);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }

    loggerWrite(RM_PMU_LOGGER_SEGMENT_ID_PERFMON_UTIL,
        sizeof(RM_PMU_GPUMON_PERFMON_SAMPLE),
        pSample);
}

/*!
 * @ref     OsTmrCallback
 *
 * @brief   Sample the idle counters and generate events if needed.
 *
 * This functions will perform the following steps:
 *     * Sample the idle counters
 *     * Compute percentage idle (with moving average if required)
 *     * Check for threshold violation
 *     * Send RM event if time to change state
 *     * Stop events if event was sent
 *
 * The following two rules describe when events are sent to the RM:
 *     1. If ANY counter is above its 'upperThreshold' send increase event.
 *     2. If ALL counters are below their 'lowerThreshold' for
 *        'toDecreaseCount' conselwtive sample, send decrease event.
 *
 * @param[in]  pCtx  Current Perfmon context.
 */
static FLCN_STATUS
perfmonOsDoSample
(
    OS_TMR_CALLBACK *pCallback
)
{
    PERFMON_CONTEXT *pCtx = &PerfmonContext;
    LwU32  baseCount = 0;
    LwU32  count;
    LwU16  pct;
    LwU32  i;
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
    LwU32 prevCount;
#endif // PMU_PERFMON_NO_RESET_COUNTERS
    LwU32  delayUs = 0;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
    appTaskCriticalEnter();
#endif // PMU_PERFMON_NO_RESET_COUNTERS
    {
        //
        // Callwlate percentages (in units of 0.01% to minimize the effects of
        // truncation in integer math):
        //     (x / base) * 100 * 100
        //         = (10000 * x) / base
        //             = x / (base / 10000)
        //
        // This method avoids overflow. We add 1 to the 'baseCount' to avoid
        // dividing by 0. If the base counter has 0 ticks there is a likely another
        // problem.
        //
        // Base counter is no more configured when PMU_PERFMON_TIMER_BASED_SAMPLING
        // is enabled.
        //
#if (!PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING))
        baseCount = pmuIdleCounterGet_HAL(&Pmu, pCtx->baseCounterId);
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
        prevCount = pCtx->baseCount;
        pCtx->baseCount = baseCount;
        baseCount = PMU_GET_EFF_COUNTER_VALUE(baseCount, prevCount);
#endif // PMU_PERFMON_NO_RESET_COUNTERS
        baseCount = baseCount / 10000 + 1;
#endif // PMU_PERFMON_TIMER_BASED_SAMPLING

#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING))
        {
            LwU32  baseFromTimer;

            //
            // Callwlate the base from PTIMER to get the actual time between callbacks.
            // This is to adjust utilization if we enter a low power state (GC5) where
            // the clocks are stopped. In this scenario, we would need to account for
            // the missing idle time in callwlations.
            //
            delayUs = osPTimerTimeNsElapsedGet(&PerfmonPreTime) / 1000;
            baseFromTimer = (pCtx->countersFreqMHz * delayUs) / 10000 + 1;

            // if baseCount is lower, GC5 engaged, so use the adjusted base to account for the idle time.
            if (baseCount < baseFromTimer)
            {
                baseCount = baseFromTimer;
            }
        }
#endif

        for (i = 0; i < pCtx->numCounters; i++)
        {
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_PEX_UTIL_COUNTERS))
            if (IS_PEX_TX_UTIL_COUNTER(pCtx, i))
            {
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
                pct = pmuPEXCntSampleTxNoReset_HAL(&Pmu, &PerfmonPreTime, &pCtx->pCountersHist[i].count);
#else
                pct = pmuPEXCntSampleTx_HAL(&Pmu);
#endif // PMU_PERFMON_NO_RESET_COUNTERS
            }
            else if (IS_PEX_RX_UTIL_COUNTER(pCtx, i))
            {
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
                pct = pmuPEXCntSampleRxNoReset_HAL(&Pmu, &PerfmonPreTime, &pCtx->pCountersHist[i].count);
#else
                pct = pmuPEXCntSampleRx_HAL(&Pmu);
#endif // PMU_PERFMON_NO_RESET_COUNTERS
            }
            else
#endif
            {
                count = pmuIdleCounterGet_HAL(&Pmu, pCtx->pCounters[i].index);
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
                prevCount = pCtx->pCountersHist[i].count;
                pCtx->pCountersHist[i].count = count;
                count = PMU_GET_EFF_COUNTER_VALUE(count, prevCount);
#endif // PMU_PERFMON_NO_RESET_COUNTERS
                pct   = (LwU16)(count / baseCount);
            }
            // callwlate a moving average
            if (USE_MOVING_AVERAGE(pCtx, i))
            {
                pct = (LwU16)
                    (
                        (
                            (LwU32)pCtx->pSampleBuffer[i] *
                            (pCtx->samplesInMovingAvg - 1) + (LwU32)pct
                        ) / pCtx->samplesInMovingAvg
                    );
            }

            // store and reset
            pCtx->pSampleBuffer[i] = pct;

#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_PEX_UTIL_COUNTERS))
            // reset only if counter is associated with a Hardware register
            if (pCtx->pCounters[i].index != RM_PMU_PERFMON_SOFTWARE_COUNTER_INDEX)
#endif
            {
                if (!PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
                {
                    pmuIdleCounterReset_HAL(&Pmu, pCtx->pCounters[i].index);
                }
            }
        }

        //
        // Base counter is no more configured when PMU_PERFMON_TIMER_BASED_SAMPLING
        // is enabled.
        //
        if ((!PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING)) &&
            (!PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS)))
        {
            pmuIdleCounterReset_HAL(&Pmu, pCtx->baseCounterId);
        }

        // save the current time if needed
        if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_TIMER_BASED_SAMPLING) ||
            PMUCFG_FEATURE_ENABLED(PMU_PERFMON_PEX_UTIL_COUNTERS))
        {
            osPTimerTimeNsLwrrentGet(&PerfmonPreTime);
        }
    }
#if (PMUCFG_FEATURE_ENABLED(PMU_PERFMON_NO_RESET_COUNTERS))
    appTaskCriticalExit();
#endif // PMU_PERFMON_NO_RESET_COUNTERS

    // Check for events to be fired.
    s_perfmonEventsCheck(pCtx);

    s_perfmonLogGpumonSampleHelper(pCtx, delayUs);

    //
    // GMxxx only: Auto wake DIOS to service Perfmon callback. After completion
    // of the callback re-engage DIOS provided all pre-conditions are satisfied.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_DIDLE_OS))
    {
        gcxDiosAttemptToEngage();
    }

    return FLCN_OK; // NJ-TODO-CB
}

static void
s_perfmonEventsCheck(PERFMON_CONTEXT *pCtx)
{
    LwBool  bIncrease[RM_PMU_DOMAIN_GROUP_NUM];
    LwU16   increasePct[RM_PMU_DOMAIN_GROUP_NUM];
    LwU32   i;

    for (i = 0; i < RM_PMU_DOMAIN_GROUP_NUM; i++)
    {
        bIncrease[i]            = LW_FALSE;
        increasePct[i]          = 0;
        pCtx->decreaseCount[i] += 1;
    }

    for (i = 0; i < pCtx->numCounters; i++)
    {
        if (pCtx->pSampleBuffer[i] > pCtx->pCounters[i].lowerThresholdPct)
        {
            pCtx->decreaseCount[pCtx->pCounters[i].groupId] = 0;
            pCtx->decreasePct[pCtx->pCounters[i].groupId] = 0;
        }
        else if (pCtx->decreasePct[pCtx->pCounters[i].groupId] < pCtx->pSampleBuffer[i])
        {
            pCtx->decreasePct[pCtx->pCounters[i].groupId] = pCtx->pSampleBuffer[i];
        }

        if (pCtx->pSampleBuffer[i] > pCtx->pCounters[i].upperThresholdPct)
        {
            bIncrease[pCtx->pCounters[i].groupId] = LW_TRUE;
            if (increasePct[pCtx->pCounters[i].groupId] < pCtx->pSampleBuffer[i])
            {
                increasePct[pCtx->pCounters[i].groupId] = pCtx->pSampleBuffer[i];
            }
        }
    }

    for (i = 0; i < RM_PMU_DOMAIN_GROUP_NUM; i++)
    {
        // if we get an event, stop any events after it
        if (bIncrease[i] && IS_INCREASE_ENABLED(pCtx,i))
        {
            // RC-TODO, this funciton need to properly handle status.
            (void)perfmonSendEvent(pCtx, i, increasePct[i]/100, LW_TRUE);  // INCREASE
            pCtx->decreaseCount[i] = 0;
            pCtx->decreasePct[i] = 0;
            perfmonEnableEvents(pCtx, LW_FALSE, i);
        }
        else if ((pCtx->decreaseCount[i] > pCtx->toDecreaseCount) && IS_DECREASE_ENABLED(pCtx,i))
        {
            // RC-TODO, this funciton need to properly handle status.
            (void)perfmonSendEvent(pCtx, i, pCtx->decreasePct[i]/100, LW_FALSE); // DECREASE
            pCtx->decreaseCount[i] = 0;
            pCtx->decreasePct[i] = 0;
            perfmonEnableEvents(pCtx, LW_FALSE, i);
        }
    }
}

static void
s_perfmonLogGpumonSampleHelper(PERFMON_CONTEXT *pCtx, LwU32 delayUs)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_LOGGER))
    {
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libGpumon)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libFxpBasic)
            OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, loggerWrite)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            s_perfmonLogGpumonSample(pCtx, delayUs);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    }
}

static void
perfmonDoSample
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    perfmonOsDoSample(NULL);
}

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Pre-init the PERFMON task.
 */
void
perfmonPreInit(void)
{
    memset(&PerfmonContext, 0x0, sizeof(PERFMON_CONTEXT));

#if (!PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTimerInitTracked(OSTASK_TCB(PERFMON), &PerfmonOsTimer,
                       PERFMON_OS_TIMER_ENTRY_NUM_ENTRIES);
#endif

    // Permanently attach utilization library to the task.
    OSTASK_ATTACH_IMEM_OVERLAY_TO_TASK(OSTASK_TCB(PERFMON),
        OVL_INDEX_IMEM(libEngUtil));
}

/*!
 * @brief      Initialize the PERFMON Task.
 *
 * @return     FLCN_OK on success,
 * @return     descriptive error code otherwise.
 */
FLCN_STATUS
perfmonPreInitTask(void)
{
    FLCN_STATUS status = FLCN_OK;
    LwU32 queueSize = 4;

    OSTASK_CREATE(status, PMU, PERFMON);
    if (status != FLCN_OK)
    {
        goto perfmonPreInitTask_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    {
        // Assure that we have queue slot for each callback.
        queueSize++;    // OsCBPerfMon
    }

    status = lwrtosQueueCreateOvlRes(&LWOS_QUEUE(PMU, PERFMON), queueSize,
                                     sizeof(DISP2UNIT_RM_RPC));
    if (status != FLCN_OK)
    {
        goto perfmonPreInitTask_exit;
    }

perfmonPreInitTask_exit:
    return status;
}

/*!
 * Main task entry point and event processing loop.
 */
lwrtosTASK_FUNCTION(task_perfmon, pvParameters)
{
    DISP2UNIT_RM_RPC disp2perfMon;

    //
    // Event processing loop. Process commands sent from the RM to the Perfmon
    // task. Also handles queue timeouts and use them to know when to sample the
    // counters.
    //
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    LWOS_TASK_LOOP(LWOS_QUEUE(PMU, PERFMON), &disp2perfMon, status, LWOS_TASK_CMD_BUFFER_ILWALID)
    {
        status = s_perfmonEventHandle(&disp2perfMon);
    }
    LWOS_TASK_LOOP_END(status);
#else
    for (;;)
    {
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_START(
            &PerfmonOsTimer,              // pOsTimer
            LWOS_QUEUE(PMU, PERFMON),     // queue
            &disp2perfMon,                // pDispStruct
            lwrtosMAX_DELAY);             // maxDelay
        {
            if (FLCN_OK != s_perfmonEventHandle(&disp2perfMon))
            {
                PMU_HALT();
            }
        }
        OS_TIMER_WAIT_FOR_QUEUE_OR_CALLBACK_STOP(&PerfmonOsTimer, lwrtosMAX_DELAY);
    }
#endif
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   Helper call handling events sent to PERFMON task.
 */
static FLCN_STATUS
s_perfmonEventHandle
(
    DISP2UNIT_RM_RPC *pRequest
)
{
    FLCN_STATUS status = FLCN_ERR_QUEUE_MGMT_ILWALID_UNIT_ID;

    switch (pRequest->hdr.unitId)
    {
        case RM_PMU_UNIT_LOGGER:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_LOGGER);

            OSTASK_OVL_DESC ovlDescList[] = {
                OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libGpumon)
            };

            OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
            {
                status = pmuRpcProcessUnitLogger(pRequest);
            }
            OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

            break;
        }
        case RM_PMU_UNIT_PERFMON:
        {
            status = FLCN_ERR_QUEUE_TASK_ILWALID_EVENT_TYPE; // NJ??

            if (pRequest->hdr.eventType == DISP2UNIT_EVT_RM_RPC)
            {
                status = pmuRpcProcessUnitPerfmon(pRequest);
            }
            break;
        }
    }

    return status;
}
