/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_FREQ_COUNTED_AVG_H
#define CLK_FREQ_COUNTED_AVG_H

/*!
 * @file    clk_freq_counted_avg.h
 * @copydoc clk_freq_counted_avg.c
 */

/* ------------------------ Includes ---------------------------------------- */
#include "pmu_objclk.h"

/* ------------------------ Macros ------------------------------------------ */
/*!
 * @brief   List of an overlay descriptors required by the counted average
 *          frequency feature.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_LIB_FREQ_COUNTED_AVG                            \
    OSTASK_OVL_DESC_DEFINE_LIB_LW64                                            \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libClkFreqCountedAvg)

/* ------------------------ Datatypes --------------------------------------- */

/*!
 * Structure to hold the clock counter sample which includes the timestamp and
 * ticks count.
 */
typedef struct
{
    /*!
     * Timestamp in ns.
     */
    LwU64   timens;
    /*!
     * Tick count in microticks (ut). This unit was chosen purely for
     * simplicity. ut / ns = kHz.
     */
    LwU64   ticksut;
} CLK_FREQ_COUNTER_SAMPLE;


/*!
 * Structure to hold previously requested target frequency and previous
 * sample value computed based on that for a clock domain.
 */
typedef struct
{
    /*!
     * Clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>.
     */
    LwU32   clkDomain;
    /*!
     * Previous requested target frequency in kHz. This is used to update the
     * previous sample values.
     */
    LwU32   prevFreqkHz;
    /*!
     * Previous sample values (timestamp and ticks count). This is increamented
     * everytime there a new target frequency request or client's request to
     * compute the counted average frequency against it's sample.
     */
    CLK_FREQ_COUNTER_SAMPLE prevSample;
} CLK_FREQ_COUNTER;


/*!
 * Main structure to hold mask of all supported clock domains and their frequency
 * counter values to compute counted average frequency.
 */
typedef struct
{
    /*!
     * Mask of all the supported clock domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz>
     */
    LwU32   clkDomainMask;
    /*!
     * Pointer to the statically allocated array of frequency counter state.
     */
    CLK_FREQ_COUNTER *pFreqCntr;
} CLK_FREQ_COUNTED_AVG;


/* ------------------------ External Definitions ---------------------------- */
/* ------------------------ Function Prototypes ----------------------------- */

// CLK_FREQ_COUNTED_AVG interfaces
LwU32 clkFreqCountedAvgComputeFreqkHz(LwU32 clkDomain, CLK_FREQ_COUNTER_SAMPLE *pSample)
    GCC_ATTRIB_SECTION("imem_libClkFreqCountedAvg", "clkFreqCountedAvgComputeFreqkHz");
FLCN_STATUS clkFreqCountedAvgUpdate(LwU32 clkDomain, LwU32 freqkHz)
    GCC_ATTRIB_SECTION("imem_libClkFreqCountedAvg", "clkFreqCountedAvgUpdate");

/* ------------------------ Include Derived Types --------------------------- */

#endif // CLK_FREQ_COUNTED_AVG_H
