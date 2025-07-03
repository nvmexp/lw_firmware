/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_FREQ_EFFECTIVE_AVG_H
#define CLK_FREQ_EFFECTIVE_AVG_H

/*!
 * @file clk_freq_effective_avg.h
 * @brief @copydoc clk_freq_effective_avg.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/pmu_clkcntr.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */

/*!
 * Structure to hold the effective frequency and the clock counter sample 
 * which includes the timestamp of the sample and the sampled clock counter 
 * value
 */
typedef struct
{
    /*!
     * Sampled effective frequency in kHz.
     */
    LwU32   freqkHz;

    /*!
     * State of clock counter when it was sampled.
     */
    LW2080_CTRL_CLK_CNTR_SAMPLE_ALIGNED sample;
} CLK_FREQ_EFFECTIVE_AVG;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */

// CLK_FREQ_EFFECTIVE_AVG interfaces
FLCN_STATUS clkFreqEffAvgGet(LwU32 clkDomainMask, LwU32 freqkHz[LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS])
    GCC_ATTRIB_SECTION("imem_libClkEffAvg", "clkFreqEffAvgGet");
FLCN_STATUS clkFreqEffAvgLoad(LwU32   actionMask)
    GCC_ATTRIB_SECTION("imem_libClkEffAvg", "clkFreqEffAvgGet");
/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_FREQ_EFFECTIVE_AVG_H
