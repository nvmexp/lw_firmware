/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2012-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pmu_clkcntr.c
 * @brief  Clock Counter feature related code
 *
 * The clock counter feature implements an interface to various clock counters
 * on both the GPU HW and in PMU SW which can count the number of clock edges on
 * a given clock domain.
 *
 * All counters are lwrrently continuous, 64-bit counters.  However,
 * "experiments" can be run by reading a starting and ending clock counter value
 * and taking the difference of their values.
 *
 * Caller-visible clock counter interfaces are all available within the
 * .clkLibCntr overlay which can be attached to any task which needs the clock
 * counter features.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dbgprintf.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objclk.h"
#include "pmu_objpcm.h"
#include "pmu_objperf.h"
#include "config/g_clk_private.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/*!
 * Period (in us) at which OS_TIMER should run SW counter callback @ref
 * clkCntrSchedPcmCallback().
 *
 * Value is lwrrently set to 500 us based on callwlation of how long the fastest
 * HW counter will saturate.  GK10X+ GPCCLK HW counter
 * (LW_PTRIM_GPC_BCAST_CLK_CNTR_NCGPCCLK_CNT) is 20 bits wide.  Assuming a
 * reasonable maximum GPCCLK of 2GHz (which is lwrrently well above our highest
 * POR clocks), this counter will saturate in 0xFFFFF / 2e9 =~ 524.3us => 500us.
 */
#define CLK_CNTR_PCM_CALLBACK_PERIOD_US                                      500

/*!
 * Period (in us) at which OS_TIMER should run SW counter callback @ref
 * clkCntrSchedPerfCallback().
 *
 * Value is lwrrently set to 13 Sec based on callwlation of how long the fastest
 * HW counter will saturate. Pascal+ HW counters are 36 bit free-running counters.
 *.Assuming a reasonable maximum clock of 5GHz, this counter will saturate in
 * 0xFFFFFFFFFF / 5000000 = ~13743ms.
 */
#define CLK_CNTR_PERF_CALLBACK_PERIOD_US                                13000000
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
OS_TMR_CALLBACK OsCBClkCntrSwCntr;
#endif
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Private Function Prototypes -------------------- */
static OsTimerCallback (s_clkCntrCallback)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "s_clkCntrCallback")
    GCC_ATTRIB_USED();
static OsTmrCallbackFunc (s_clkCntrOsCallback)
    GCC_ATTRIB_SECTION("imem_clkLibCntr", "s_clkCntrOsCallback")
    GCC_ATTRIB_USED();
static LwU64 s_clkCntrReadNonSync(CLK_CNTR *pCntr)
    GCC_ATTRIB_SECTION("imem_clkLibCntrSwSync", "s_clkCntrReadNonSync");

/* ------------------------- Public Functions ------------------------------- */

/*!
 * Initialize the CLK_CNTR feature, including loading any necessary HW state.
 */
void
clkCntrPreInit(void)
{
    //
    // Construct SW state.
    // Note: It reads registers and that's why it's called here.
    //
    clkCntrInitSw_HAL(&Clk);

    // Initialize HW state of all the supported CLK_CNTRs.
    clkCntrInitHw_HAL(&Clk);
}

/*!
 * Prepares PCMEVT task to run SW counter callback by scheduling relwrring
 * callback via PCMEVT task's OS_TIMER.
 * This function must be called from PCMEVT task initialization code.
 */
void
clkCntrSchedPcmCallback(void)
{
    // Register the OS_TIMER callback for the SW clock counter going forward.
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    PMU_HALT();
#else
    osTimerScheduleCallback(
        &PcmOsTimer,                                                // pOsTimer
        PMU_OS_TIMER_ENTRY_SW_CNTR,                                 // index
        lwrtosTaskGetTickCount32(),                                 // ticksPrev
        CLK_CNTR_PCM_CALLBACK_PERIOD_US,                            // usDelay
        DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC),   // flags
        s_clkCntrCallback,                                          // pCallback
        NULL,                                                       // pParam
        OVL_INDEX_IMEM(clkLibCntr));                                // ovlIdx
#endif
}

/*!
 * Prepares PERF task to run SW counter callback by scheduling relwrring
 * callback via PERF task's OS_TIMER.
 * This function must be called from PERF task initialization code.
 */
void
clkCntrSchedPerfCallback(void)
{
    // Register the OS_TIMER callback for the SW clock counter going forward.
#if (PMUCFG_FEATURE_ENABLED(PMU_OS_CALLBACK_CENTRALISED))
    osTmrCallbackCreate(&OsCBClkCntrSwCntr,                      // pCallback
        OS_TMR_CALLBACK_TYPE_RELWRRENT_TO_EXPECTED_SKIP_MISSED,  // type
        OVL_INDEX_IMEM(clkLibCntr),                              // ovlImem
        s_clkCntrOsCallback,                                     // pTmrCallbackFunc
        LWOS_QUEUE(PMU, PERF),                                   // queueHandle
        CLK_CNTR_PERF_CALLBACK_PERIOD_US,                        // periodNormalus
        CLK_CNTR_PERF_CALLBACK_PERIOD_US,                        // periodSleepus
        OS_TIMER_RELAXED_MODE_USE_NORMAL,                        // bRelaxedUseSleep        
        RM_PMU_TASK_ID_PERF);                                    // taskId
    osTmrCallbackSchedule(&OsCBClkCntrSwCntr);
#else
    osTimerScheduleCallback(
        &PerfOsTimer,                                               // pOsTimer
        PERF_OS_TIMER_ENTRY_SW_CLK_CNTR_CALLBACK,                   // index
        lwrtosTaskGetTickCount32(),                                 // ticksPrev
        CLK_CNTR_PERF_CALLBACK_PERIOD_US,                           // usDelay
        DRF_DEF(_OS_TIMER, _FLAGS, _RELWRRING, _YES_MISSED_EXEC),   // flags
        s_clkCntrCallback,                                          // pCallback
        NULL,                                                       // pParam
        OVL_INDEX_IMEM(clkLibCntr));                                // ovlIdx
#endif
}

/*!
 * Helper function which maps a requested clock domain (@ref
 * LW2080_CTRL_CLK_DOMAIN_<xyz> enum) to the internal CLK_CNTR information
 * required to sample the counter.
 *
 * @param[in]   clkDomain
 *      LW2080_CTRL_CLK_DOMAIN_<xyz> enum for the requested clock domain.
 * @param[in]    partIdx
 *      Partition index for the applicable clock domains.
 *      @ref LW2080_CTRL_CLK_DOMAIN_PART_IDX_<xyz>.
 * @return Pointer to CLK_CNTR.
 */
CLK_CNTR *
clkCntrGet
(
    LwU32 clkDomain,
    LwU8  partIdx
)
{
    CLK_CNTR *pCntr = NULL;
    LwU8      i     = 0;

    while (i < Clk.cntrs.numCntrs)
    {
        pCntr = &Clk.cntrs.pCntr[i++];
        if ((clkDomain == pCntr->clkDomain) &&
            (partIdx == pCntr->partIdx))
        {
             return pCntr;
        }
    }

    return NULL;
}

/*!
 * Reads the latest counter value from the clock counter monitoring the
 * specified domain.  The counter value is an unsigned 64-bit value which is
 * continuously counting/incrementing.
 *
 * @param[in]   clkDomain
 *     LW2080_CTRL_CLK_DOMAIN_<xyz> enum for the requested clock domain.
 * @param[in]   partIdx
 *     Partition index for the applicable clock domains.
 *     @ref LW2080_CTRL_CLK_DOMAIN_PART_IDX_<xyz>.
 *
 * @return Latest 64-bit clock counter value.
 */
LwU64
clkCntrRead
(
    LwU32  clkDomain,
    LwU8   partIdx
)
{
    LwU64       cntrVal;
    CLK_CNTR   *pCntr;

    //
    // Get CLK_CNTR object corresponding to the requested
    // clock domain and partition index.
    //
    pCntr = clkCntrGet(clkDomain, partIdx);
    if (pCntr == NULL)
    {
#ifdef UPROC_RISCV
        dbgPrintf(LEVEL_ALWAYS, "pCntr(%u, %u)\n", clkDomain, (unsigned) partIdx);
#endif // UPROC_RISCV
        PMU_BREAKPOINT();
        return 0;
    }

    //
    // Elevate to critical to synchronize protected SW counter state.
    //
    appTaskCriticalEnter();
    {
        // If access to the HW cntrs is disabled, use the cached value.
        if (!CLK_CNTR_IS_HW_ACCESS_ENABLED(pCntr))
        {
            cntrVal = pCntr->swCntr;
        }
        else
        {
            cntrVal = s_clkCntrReadNonSync(pCntr);
        }
    }
    appTaskCriticalExit();

    // Scale the counter value by scaling factor.
    if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_64BIT_LIBS))
    {
        LwU64 factor = pCntr->scalingFactor;
        lwU64Mul(&cntrVal, &cntrVal, &factor);
    }
    else
    {
        //
        // For pre-Pascal GPUs, scalingFactor value is either 1 or 2
        // so colwerting it into a simple 64 bit addition for better size
        // and exelwtion time.
        //
        PMU_HALT_COND((1 == pCntr->scalingFactor) || (2 == pCntr->scalingFactor));
        if (pCntr->scalingFactor == 2)
        {
            lw64Add(&cntrVal, &cntrVal, &cntrVal);
        }
    }

    return cntrVal;
}

/*!
 * Helper interface to sample the latest clock counter (ticks) and timestamp
 * (ns) values for a clock domain.
 *
 * @param[in/out] pSample
 *      RM_PMU_CLK_CNTR_SAMPLE_DOMAIN in which the latest values will be stored
 *      for all the partitions of a given clock domain.
 * @return FLCN_OK
 *      Successfully took the samples.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *      Invalid parameters passed.
 */
FLCN_STATUS
clkCntrSample
(
    RM_PMU_CLK_CNTR_SAMPLE_DOMAIN *pSample
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8       i;

    if ((pSample == NULL) ||
        (pSample->numParts > LW2080_CTRL_CLK_DOMAIN_PART_IDX_MAX))
    {
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    // Get samples for each partition.
    for (i = 0; i < pSample->numParts; i++)
    {
        clkCntrSamplePart(pSample->clkDomain,
            pSample->parts[i].partIdx, &pSample->parts[i].sample);
    }

    return status;
}

/*!
 * Helper interface to sample the latest clock counter (ticks) and timestamp
 * (ns) values for a partition within a clock domain.
 *
 * @param[in]  clkDomain
 *      LW2080_CTRL_CLK_DOMAIN_<xyz> enum for the requested clock domain.
 * @param[in]  partIdx
 *      Partition index for the applicable clock domains.
 *      @ref LW2080_CTRL_CLK_DOMAIN_PART_IDX_<xyz>.
 * @param[out] pSample
 *      LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED in which the latest values will be stored.
 */
void
clkCntrSamplePart
(
    LwU32                                clkDomain,
    LwU8                                 partIdx,
    LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED *pSample
)
{
    //
    // To get most accurate/matching samples, tick count and timestamp values
    // must be as close as possible.  Use a critical section to ensure that the
    // PMU will not be context switched during this operation.
    //
    appTaskCriticalEnter();
    {
        LwU64 tickCnt = clkCntrRead(clkDomain, partIdx);

        // Read the latest counter value and time.
        LwU64_ALIGN32_PACK(&pSample->tickCnt, &tickCnt);
        osPTimerTimeNsLwrrentGetUnaligned(&pSample->timens);
    }
    appTaskCriticalExit();
}

/*!
 * Computes the average frequency (KHz) of a given clock domain over a requested
 * sampling period - as derived by the number of clock cycles (from a counter)
 * divided by the sampling time.
 *
 * This interface takes the beginning counter value and timestamp as input and
 * overrides with the latest counter value and timestamp, so that this interface
 * can be called continuously to compute the average frequency of adjacent samples.
 *
 * @param[in]    clkDomain
 *      LW2080_CTRL_CLK_DOMAIN_<xyz> enum for the requested clock domain.
 * @param[in]    partIdx
 *      Partition index for the applicable clock domains.
 *      @ref LW2080_CTRL_CLK_DOMAIN_PART_IDX_<xyz>.
 * @param[inout] pSample
 *      Pointer to structure containing the starting counter and timestamp values
 *      corresponding to the beginning of the requested sampling period.  After
 *      computing frequency, these values will be overridden by the latest
 *      counter value and timestamp values.
 *
 * @return Computed average frequency (KHz).
 */
LwU32
clkCntrAvgFreqKHz32Bit
(
    LwU32                                clkDomain,
    LwU8                                 partIdx,
    LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED *pSample
)
{
    LwU32           freqMHz;
    FLCN_TIMESTAMP  timeDiffns;
    FLCN_TIMESTAMP  timeDiffTempns;
    FLCN_TIMESTAMP  timeRoundns;
    LwU32           timeDiffus;
    FLCN_TIMESTAMP  cntrDiff;
    LwU32           shift      = 0;
    LwU32           cntrDiff32 = 0;
    LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED sampleNow;

    // Read the latest counter value and time.
    clkCntrSamplePart(clkDomain, partIdx, &sampleNow);

    //
    // This code is implemented in 32-bit math - both the counter and timer inputs
    // are 32-bit values.  Need to handle overflow in the following cases:
    //
    // 1. Timer overflow: The 32-bit ns timer will overflow in ~4.29s.  In cases
    // where this time period is exceeded, shift down the timer to get in the
    // appropriate range.  The result value will need to be similarly shifted
    // down to get back to appropriate units.
    //
    // 2. Counter overflow: Assuming a 1.2GHz clock (roughly our current
    // shipping max clock), the counter will overflow 32-bits in ~3.58s.
    // However, this interface is never expected to be called with a period that
    // large when the clocks are that high.  So, this condition will be
    // hand-waved away for now.  If seeing improper behavior from this code,
    // please confirm that this is not the case.
    //
    LwU64_ALIGN32_SUB(&timeDiffns.parts, &sampleNow.timens, &pSample->timens);
    if (timeDiffns.parts.hi != 0)
    {
        //
        // Native 64-bit shifting is inefficiently implemented as a GCC function
        // (in the resident section).  Instead, as the result is guaranteed to
        // fit within 32-bits, use this hard-coded implementation.
        //
        // 1. Compute shift factor: Find highest set bit in upper 32-bits.
        //
        shift = timeDiffns.parts.hi;
        HIGHESTBITIDX_32(shift);
        shift++;

        //
        // 2. Rounding: Add a bit at MSB which will be shifted out.  Note, this
        // may change the highest set bit in the upper 32-bits, so recompute the
        // shift factor.
        //
        timeDiffTempns = timeDiffns;
        timeRoundns.parts.lo = BIT(shift - 1);
        timeRoundns.parts.hi = 0;
        lw64Add(&timeDiffns.data, &timeDiffTempns.data, &timeRoundns.data);
        shift = timeDiffns.parts.hi;
        HIGHESTBITIDX_32(shift);
        shift++;

        // 3. Do actual bit shifting from 64-bit to 32-bit number.
        timeDiffns.parts.lo =
            (timeDiffns.parts.lo >> shift) |
            (timeDiffns.parts.hi << (32 - shift));
        timeDiffns.parts.hi = 0;
    }

    //
    // Want average freq in MHz = 1 / 1e6 / s => 1 / us.  Input is provided in 1
    // / ns => GHz.  Pre-divide time to us.
    //
    timeDiffus = LW_UNSIGNED_ROUNDED_DIV(timeDiffns.parts.lo, 1000);
    PMU_HALT_COND(timeDiffus != 0);

    // MHz = 1 / us.
    LwU64_ALIGN32_SUB(&cntrDiff.parts, &sampleNow.tickCnt, &pSample->tickCnt);
    PMU_HALT_COND(LWU64_HI(cntrDiff) == 0);
    cntrDiff32 = LWU64_LO(cntrDiff);

    freqMHz = LW_UNSIGNED_ROUNDED_DIV(cntrDiff32, timeDiffus);
    //
    // If shifted time period above, shift the freqMHz result to get back to
    // correct units.
    //
    if (shift != 0)
    {
        freqMHz = LW_RIGHT_SHIFT_ROUNDED(freqMHz, shift);
    }

    // Update start values to the current values.
    *pSample = sampleNow;

    // Return frequency in KHz
    return (freqMHz * 1000);
}

/*!
 * Computes the average frequency (KHz) of a given clock domain over a requested
 * sampling period - as derived by the number of clock cycles (from a counter)
 * divided by the sampling time.
 *
 * This interface takes the beginning counter value and timestamp as input and
 * overrides with the latest counter value and timestamp, so that this interface
 * can be called continuously to compute the average frequency of adjacent samples.
 *
 * Unlike clkCntrAvgFreqKHz32Bit, this interface uses 64 bit math library for
 * frequency computation.
 * @pre Required .libLw64
 *
 * @param[in]    clkDomain
 *      LW2080_CTRL_CLK_DOMAIN_<xyz> enum for the requested clock domain.
 * @param[in]    partIdx
 *      Partition index for the applicable clock domains.
 *      @ref LW2080_CTRL_CLK_DOMAIN_PART_IDX_<xyz>.
 * @param[inout] pSample
 *      Pointer to structure containing the starting counter and timestamp values
 *      corresponding to the beginning of the requested sampling period.  After
 *      computing frequency, these values will be overridden by the latest
 *      counter value and timestamp values.
 *
 * @return Computed average frequency (KHz).
 */
LwU32
clkCntrAvgFreqKHz64Bit
(
    LwU32                                clkDomain,
    LwU8                                 partIdx,
    LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED *pSample
)
{
    LwU64 freqKHz = 0;
    FLCN_TIMESTAMP cntrDiff;
    FLCN_TIMESTAMP timeDiffns;
    LwU64 mulVal;
    LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED sampleNow;

    // Read the latest counter value and time.
    clkCntrSamplePart(clkDomain, partIdx, &sampleNow);

    // Diff samples to compute frequency.
    LwU64_ALIGN32_SUB(&timeDiffns.parts, &sampleNow.timens, &pSample->timens);
    LwU64_ALIGN32_SUB(&cntrDiff.parts, &sampleNow.tickCnt, &pSample->tickCnt);

    //
    // Input is provided in 1/ns => GHz. Pre-Multiply counter value to get
    // Frequency in KHz.
    //
    mulVal = 1000000;
    lwU64Mul(&cntrDiff.data, &cntrDiff.data, &mulVal);

    // Callwlate the frequency
    if (timeDiffns.data != 0)
    {
        lwU64Div(&freqKHz, &cntrDiff.data, &timeDiffns.data);
    }
    else
    {
        PMU_HALT();
    }

    // Update start values to the current values.
    *pSample = sampleNow;

    return (LwU32)freqKHz;
}

/*!
 * @brief Helper interface which synchronizes access to HW counter registers.
 *
 * This interface is lwrrently used by low power features like MSCG (on earlier
 * chips as well) GPC-RG (GA10x & later) to disable/enable the access to HW clock
 * counters before and after the gating of concerned clocks.
 *
 * To-do @akshatam: Have this function return a status code.
 * 
 * @param[in] clkDomMask
 *      Mask of clock domains for @ref LW2080_CTRL_CLK_DOMAIN_<xyz> which the
 *      client wishes to disable HW counter access to
 * @param[in] clientId
 *      ID of the client who wishes to change the clock counter access state
 *      @ref LW2080_CTRL_CLK_CNTR_CLIENT_ID_<xyz>
 * @param[in] clkCntrAccessState
 *     Clock counter access state indicating whether access has been enabled
 *     or disabled, and if force refresh of counters is required.
 */
void
clkCntrAccessSync
(
    LwU32   clkDomMask,
    LwU8    clientId,
    LwU8    clkCntrAccessState
)
{
    FLCN_STATUS status;

    // Sanity check if client ID is valid (is one of the known client IDs)
    if ((LWBIT_TYPE(clientId, LwU32) &
         LW2080_CTRL_CLK_CLIENT_ID_VALID_MASK) == 0U)
    {
        PMU_BREAKPOINT();
        return;
    }

    //
    // Elevate to critical to synchronize protected SW counter state.
    //
    appTaskCriticalEnter();
    {
        CLK_CNTR *pCntr;
        LwBool    bCntrPreviouslyEnabled;
        LwU8      i = 0;

        // Update SW state for all clock counters.
        while (i < Clk.cntrs.numCntrs)
        {
            pCntr = &Clk.cntrs.pCntr[i++];

            // Skip for the domain which is not gated/ungated.
            if ((pCntr->clkDomain & clkDomMask) == 0)
            {
                continue;
            }

            bCntrPreviouslyEnabled = CLK_CNTR_IS_HW_ACCESS_ENABLED(pCntr);

            if (clkCntrAccessState == CLK_CNTR_ACCESS_DISABLED)
            {
                pCntr->accessDisableClientsMask |= LWBIT_TYPE(clientId, LwU32);
            }
            else if ((clkCntrAccessState == CLK_CNTR_ACCESS_ENABLED_TRIGGER_CALLBACK) ||
                     (clkCntrAccessState == CLK_CNTR_ACCESS_ENABLED_SKIP_CALLBACK))
            {
                pCntr->accessDisableClientsMask &= ~LWBIT_TYPE(clientId, LwU32);
            }
            else
            {
                PMU_BREAKPOINT();
            }

            //
            // Read and update the cached clock counter value if
            // Case 1: Register access was enabled earlier and disabling now.
            // Case 2: Register access was disabled earlier and enabling now &
            //         force read is requested.
            // Also enable/disable the counter logic if the concerned feature
            // is enabled.
            //
            if (bCntrPreviouslyEnabled && !CLK_CNTR_IS_HW_ACCESS_ENABLED(pCntr))
            {
                //
                // Case 1 -> Read the latest clock counter value first and then
                // disable counter logic.
                //
                (void)s_clkCntrReadNonSync(pCntr);
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_TOGGLE_GATED_LOGIC))
                {
                    status = clkCntrEnableContinuousUpdate_HAL(&Clk, pCntr, LW_FALSE);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                    }
                }
            }
            else if (!bCntrPreviouslyEnabled && CLK_CNTR_IS_HW_ACCESS_ENABLED(pCntr))
            {
                //
                // Case 2 -> Enable the counter logic first and then read the
                // latest clock counter value.
                //
                if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CNTR_TOGGLE_GATED_LOGIC))
                {
                    status = clkCntrEnableContinuousUpdate_HAL(&Clk, pCntr, LW_TRUE);
                    if (status != FLCN_OK)
                    {
                        PMU_BREAKPOINT();
                    }
                }

                if (clkCntrAccessState == CLK_CNTR_ACCESS_ENABLED_TRIGGER_CALLBACK)
                {
                    (void)s_clkCntrReadNonSync(pCntr);
                }
            }
            else
            {
                /* Do nothing */
            }
        }
    }
    appTaskCriticalExit();
}

/*!
 * @brief Enable the clock counter(s) for the given clock domain such that it
 * starts counting its configured source (pCntr->cntrSource).
 *
 * @param[in]   clkDomain
 *      LW2080_CTRL_CLK_DOMAIN_<xyz> enum for the requested clock domain.
 *
 * @return FLCN_OK                   If counter(s) is(are) enabled successfully
 * @return FLCN_ERR_ILWALID_ARGUMENT If pCntr is not supported for the given domain
 */
FLCN_STATUS
clkCntrDomainEnable
(
    LwU32 clkDomain
)
{
    CLK_CNTR   *pCntr;
    FLCN_STATUS status     = FLCN_OK;
    LwBool      bCntrFound = LW_FALSE;
    LwU8        i          = 0;

    while (i < Clk.cntrs.numCntrs)
    {
        pCntr = &Clk.cntrs.pCntr[i++];
        if (pCntr->clkDomain == clkDomain)
        {
            bCntrFound = LW_TRUE;
            status = clkCntrEnableRuntime_HAL(&Clk, pCntr);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                return status;
            }
        }
    }

    // Check if the counter(s) was(were) found for the given clock domain
    if (!bCntrFound)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }

    return status;
}

/*!
 * @brief Reset cached last HW counter value for the given domain.
 *
 * This could be needed for low power features where both the clock is gated
 * and the counter registers are reset. Resetting the cached value would ensure
 * we add the right difference to the SW counter in clkCntrRead once the clock
 * is ungated and the HW counter access is restored.
 *
 * @param[in]   clkDomain
 *      LW2080_CTRL_CLK_DOMAIN_<xyz> enum for the requested clock domain.
 *
 * @return FLCN_OK                   If the reset of cached value was successful
 * @return FLCN_ERR_ILWALID_ARGUMENT If pCntr is not supported for the given domain
 */
FLCN_STATUS
clkCntrDomainResetCachedHwLast
(
    LwU32 clkDomain
)
{
    CLK_CNTR   *pCntr;
    FLCN_STATUS status     = FLCN_OK;
    LwBool      bCntrFound = LW_FALSE;
    LwU8        i          = 0;

    while (i < Clk.cntrs.numCntrs)
    {
        pCntr = &Clk.cntrs.pCntr[i++];
        if (pCntr->clkDomain == clkDomain)
        {
            bCntrFound = LW_TRUE;
            pCntr->hwCntrLast = 0;
        }
    }

    // Check if the counter(s) was found for the given clock domain
    if (!bCntrFound)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
    }

    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @ref     OsTimerOsCallback
 *
 * OS_TMR callback which implements SW counter feature to HW limitations
 * (non-continuous, < 64-bit counter).
 *
 * Scheduled from @ref clkCntrSwCntrInit().
 */
static FLCN_STATUS
s_clkCntrOsCallback
(
    OS_TMR_CALLBACK *pCallback
)
{
    CLK_CNTR *pCntr;
    LwU8      i = 0;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, clkLibCntrSwSync)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        //
        // Elevate to critical to synchronize protected SW counter state.
        //
        appTaskCriticalEnter();
        {
            // Update all supported clock counters.
            while (i < Clk.cntrs.numCntrs)
            {
                pCntr = &Clk.cntrs.pCntr[i++];

                //
                // Read the clock counter if register access is enabled,
                // which has side effect of updating SW counter
                // tracking (preventing overflow) and restarting legacy counter, if any.
                //
                if (CLK_CNTR_IS_HW_ACCESS_ENABLED(pCntr))
                {
                    (void)s_clkCntrReadNonSync(pCntr);
                }
            }
        }
        appTaskCriticalExit();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return FLCN_OK;
}

/*!
 * @ref     OsTimerCallback
 *
 * OS_TIMER callback which implements SW counter feature to HW limitations
 * (non-continuous, < 64-bit counter).
 *
 * Scheduled from @ref clkCntrSwCntrInit().
 */
static void
s_clkCntrCallback
(
    void   *pParam,
    LwU32   ticksExpired,
    LwU32   skipped
)
{
    s_clkCntrOsCallback(NULL);
}

/*!
 * Reads a clock counter (as specified by an CLK_CNTR structure) which is
 * implemeted via SW clock counter mechanism.
 *
 * Reads value from HW and adds difference from last read to running SW clock
 * counter value, which implements a continuous, 64-bit counter.
 *
 * @pre Callers to this function must have already elevated to critical via @ref
 * appTaskCriticalEnter();
 *
 * @pre This helper interface does not check the synchronized accessiblity value
 * and should only be called from synchronized callers and make sure that the
 * register access is enabled.
 *
 * @param[in/out] pCntr Pointer to CLK_CNTR object.
 *
 * @return Latest 64-bit clock counter value.
 */
static LwU64
s_clkCntrReadNonSync
(
    CLK_CNTR *pCntr
)
{
    LwU64      hwCntrLwrr;
    LwU64      hwCntrDiff;
    LwU8       hwCntrSize = Clk.cntrs.hwCntrSize;
    LWU64_TYPE uHwCntrLast;

    // Read/refresh the HW CNTR value.
    hwCntrLwrr = clkCntrRead_HAL(&Clk, pCntr);

    //
    // If counter has wrapped around, extend from HW counter size to SW
    // counter size.
    //
    if (lwU64CmpLT(&hwCntrLwrr, &pCntr->hwCntrLast))
    {
        uHwCntrLast.val = pCntr->hwCntrLast;
        if (hwCntrSize < 32)
        {
            LWU64_LO(uHwCntrLast) |= ~(BIT(hwCntrSize) - 1);
            LWU64_HI(uHwCntrLast) = LW_U32_MAX;
        }
        else
        {
            LWU64_HI(uHwCntrLast) |= ~(BIT(hwCntrSize - 32) - 1);
        }
        pCntr->hwCntrLast = uHwCntrLast.val;
    }

    // Callwlate the difference in the HW counter from the last read value.
    lw64Sub(&hwCntrDiff, &hwCntrLwrr, &pCntr->hwCntrLast);

    //
    // Update SW counter with difference and store last HW counter value for
    // next read.
    //
    lw64Add(&pCntr->swCntr, &pCntr->swCntr, &hwCntrDiff);
    pCntr->hwCntrLast = hwCntrLwrr;

    // Return the updated SW counter value.
    return pCntr->swCntr;
}
