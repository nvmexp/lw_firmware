/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_freq_effective_avg.c
 *
 * @brief Module managing all state and routines
 * related to the CLK_FREQ_EFFECTIVE_AVG feature.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "pmu_objperf.h"

/* ------------------------ Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TMR_CALLBACK OsCBClkFreqEffectiveSample;
#endif

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static OsTimerCallback (s_clkFreqEffAvgCallback)
    GCC_ATTRIB_SECTION("imem_libClkEffAvg", "s_clkFreqEffAvgCallback")
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc (s_clkFreqEffAvgOsCallback)
    GCC_ATTRIB_SECTION("imem_libClkEffAvg", "s_clkFreqEffAvgOsCallback")
    GCC_ATTRIB_USED();
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * Schedules the callback to evaluate the frequency at sampling period.
 *
 * @param[in] actionMask   @ref LW_RM_PMU_CLK_LOAD_ACTION_MASK_<xyz>.
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkFreqEffAvgLoad
(
    LwU32   actionMask
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_FREQ_EFFECTIVE_AVG) &&
        (Clk.domains.cntrSamplingPeriodms != 0) &&
        FLD_TEST_DRF(_RM_PMU_CLK_LOAD, _ACTION_MASK,
                     _FREQ_EFFECTIVE_AVG_CALLBACK, _YES, actionMask))
    {
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
            osTmrCallbackCreate(&OsCBClkFreqEffectiveSample,             // pCallback
                OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,  // type
                OVL_INDEX_IMEM(libClkEffAvg),                            // ovlImem
                s_clkFreqEffAvgOsCallback,                               // pTmrCallbackFunc
                LWOS_QUEUE(PMU, PERF),                                   // queueHandle
                Clk.domains.cntrSamplingPeriodms * 1000,                 // periodNormalus
                Clk.domains.cntrSamplingPeriodms * 1000,                 // periodSleepus
                OS_TIMER_RELAXED_MODE_USE_NORMAL,                        // bRelaxedUseSleep
                RM_PMU_TASK_ID_PERF);                                    // taskId
            osTmrCallbackSchedule(&OsCBClkFreqEffectiveSample);
#else
            osTimerScheduleCallback(
                &PerfOsTimer,                                             // pOsTimer
                PERF_OS_TIMER_ENTRY_CLK_FREQ_EFF_AVG_CALLBACK,            // index
                lwrtosTaskGetTickCount32(),                               // ticksPrev
                Clk.domains.cntrSamplingPeriodms * 1000,                  // usDelay
                DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC), // flags
                s_clkFreqEffAvgCallback,                                  // pCallback
                NULL,                                                     // pParam
                OVL_INDEX_IMEM(libClkEffAvg));                            // ovlIdx
#endif
    }

    return FLCN_OK;
}

/*!
 * Get the average effetive frequency for clock domains
 *
 * @param[in]   clkDomainMask   Mask of clock domains
 * @param[out]  freqkHz[]       Array of frequencies for the given clock domains
 *
 * @return FLCN_OK
 */
FLCN_STATUS
clkFreqEffAvgGet
(
    LwU32   clkDomainMask,
    LwU32   freqkHz[LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS]
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8        idx;
    LwU8        domIdx;

    FOR_EACH_INDEX_IN_MASK(32, domIdx, clkDomainMask)
    {
        LwBool bDomFound = LW_FALSE;
        freqkHz[domIdx] = LW_U32_MAX;
        for (idx = 0; idx < Clk.cntrs.numCntrs; idx++)
        {
            if (Clk.cntrs.pCntr[idx].clkDomain == BIT(domIdx))
            {
                bDomFound = LW_TRUE;
                //
                // For domains with multiple paritions (aka GPC), report the
                // slowest frequency as the effective frequency.
                //
                freqkHz[domIdx] = LW_MIN(freqkHz[domIdx],
                                         Clk.cntrs.pCntr[idx].freqEffAvg.freqkHz);
            }
        }

        if (!bDomFound)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            break;
        }
    } FOR_EACH_INDEX_IN_MASK_END;

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @ref OsTimerOsCallback
 *
 * OS_TIMER callback to get the average frequency of supported clocks
 */
static FLCN_STATUS
s_clkFreqEffAvgOsCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    LwU8 idx;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_DEFINE_CLK_COUNTER
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        for (idx = 0; idx < Clk.cntrs.numCntrs; idx++)
        {
            CLK_CNTR *pCntr = &Clk.cntrs.pCntr[idx];
            pCntr->freqEffAvg.freqkHz = clkCntrAvgFreqKHz(pCntr->clkDomain,
                                                     pCntr->partIdx,
                                                     &pCntr->freqEffAvg.sample);
        }
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

/*!
 * @ref  OsTimerCallback
 *
 * OS_TIMER callback which implements effective frequency sampling
 *
 */
static void
s_clkFreqEffAvgCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_clkFreqEffAvgOsCallback(NULL);
}
