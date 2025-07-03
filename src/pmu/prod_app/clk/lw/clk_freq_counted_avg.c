/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_freq_counted_avg.c
 *
 * @brief Module managing all state related to the CLK_FREQ_COUNTED_AVG structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
static CLK_FREQ_COUNTER *s_clkFreqCounterGet(LwU32 clkDomain)
    GCC_ATTRIB_SECTION("imem_libClkFreqCountedAvg", "s_clkFreqCounterGet");
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Public Functions ------------------------------- */

/*!
 * Client interface to compute the counted average frequency. Computes the
 * average frequency as tick difference / time difference, where the difference
 * is computed between the current sample and the sample provided by the client.
 *
 * It also updates the client's passed sample with the current sample after
 * frequency computation.
 *
 * @param[in]     clkDomain
 *      Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
 * @param[in/out] pSample
 *      Pointer to the client's sample against which the current sample will be
 *      differenced to compute average frequency. After frequency computation,
 *      this will be updated to the current sample.
 *
 * @return Computed average frequency in kHz.
 * @return 0
 *      Invalid clock domain or sample provided by the client. This is expected
 *      for the first reading of the sample.
 */
LwU32
clkFreqCountedAvgComputeFreqkHz
(
    LwU32                       clkDomain,
    CLK_FREQ_COUNTER_SAMPLE    *pSample
)
{
    LwU64 timeDiffns;
    LwU64 ticksDiffut;
    LwU64 freqkHz = 0;

    CLK_FREQ_COUNTER *pCntr = s_clkFreqCounterGet(clkDomain);
    if ( pCntr == NULL )
    {
        return 0;
    }

    // Update cached time and ticks using cached frequency.
    if (FLCN_OK != clkFreqCountedAvgUpdate(clkDomain, pCntr->prevFreqkHz))
    {
        return 0;
    }

    // Compute time difference
    lw64Sub(&timeDiffns, &pCntr->prevSample.timens, &pSample->timens);

    // Compute ticks difference
    if (pCntr->prevSample.ticksut < pSample->ticksut)
    {
        // Handle overflow case
        ticksDiffut = LW_U64_MAX;
        lw64Sub(&ticksDiffut, &ticksDiffut, &pSample->ticksut);
        lw64Add(&ticksDiffut, &ticksDiffut, &pCntr->prevSample.ticksut);
    }
    else
    {
        lw64Sub(&ticksDiffut, &pCntr->prevSample.ticksut, &pSample->ticksut);
    }

    // Compute average frequency in kHz
    if (timeDiffns != 0)
    {
        lwU64Div(&freqkHz, &ticksDiffut, &timeDiffns);
    }
    else
    {
        PMU_HALT();
    }

    // Update client's sample with the new one.
    pSample->ticksut = pCntr->prevSample.ticksut;
    pSample->timens = pCntr->prevSample.timens;

    return (LwU32)freqkHz;
}

/*!
 * Updates the frequency counter sample based on the previous requested
 * frequency and cache the new one.
 *
 * @param[in]   clkDomain
 *      Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>.
 * @param[in]   freqkHz
 *      New requested frequency in kHz.
 *
 * @return FLCN_OK
 *      Successfully updates the frequency counter sample and cached frequency.
 * @return FLCN_ERR_NOT_SUPPORTED
 *      Unsupported clock domain passed.
 */
FLCN_STATUS
clkFreqCountedAvgUpdate
(
    LwU32 clkDomain,
    LwU32 freqkHz
)
{
    FLCN_TIMESTAMP  timeNowns;
    LwU64           timeDiffns;
    LwU64           tempFreqkHz;
    LwU64           tempTicksut;

    CLK_FREQ_COUNTER *pCntr = s_clkFreqCounterGet(clkDomain);
    if ( pCntr == NULL )
    {
        return FLCN_ERR_NOT_SUPPORTED;
    }

    //
    // Elevate to critical to synchronize average frequency counter state.
    //
    appTaskCriticalEnter();
    {
        // Get current timestamp.
        osPTimerTimeNsLwrrentGet(&timeNowns);

        // Update the ticks only if previous frequency is stored.
        if (pCntr->prevFreqkHz != 0)
        {
            // Compute elapsed ticks since last frequency update.
            lw64Sub(&timeDiffns, &(timeNowns.data), &pCntr->prevSample.timens);
            tempFreqkHz = pCntr->prevFreqkHz;
            lwU64Mul(&tempTicksut, &timeDiffns, &tempFreqkHz);

            // Update ticks by adding to elapsed ticks.
            lw64Add(&pCntr->prevSample.ticksut, &pCntr->prevSample.ticksut, &tempTicksut);
        }

        // Update previous values to the new ones.
        pCntr->prevSample.timens = timeNowns.data;
        pCntr->prevFreqkHz = freqkHz;
    }
    appTaskCriticalExit();

    return FLCN_OK;
}


/* ------------------------- Private Functions ------------------------------ */

/*!
 * Helper function to get the pointer to frequency counter object based on the
 * clock domain passed.
 *
 * @param[in]   clkDomain
 *      Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>.
 *
 * @return Pointer to frequency counter object
 * @return NULL
 *      Unsupported clock domain passed.
 */
CLK_FREQ_COUNTER *
s_clkFreqCounterGet
(
    LwU32 clkDomain
)
{
    LwU32 i;
    LwU32 count = Clk.freqCountedAvg.clkDomainMask;

    // Get the number of clock domains
    NUMSETBITS_32(count);

    for (i = 0; i < count; i++)
    {
        if (Clk.freqCountedAvg.pFreqCntr[i].clkDomain == clkDomain)
        {
            return &Clk.freqCountedAvg.pFreqCntr[i];
        }
    }

    return NULL;
}
