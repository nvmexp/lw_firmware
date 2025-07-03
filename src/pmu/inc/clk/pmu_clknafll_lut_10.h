/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_LUT_10_H
#define PMU_CLKNAFLL_LUT_10_H

/*!
 * @file pmu_clknafll_lut_10.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Forward Definitions ---------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clknafll_lut.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro for INVALID VF gain switch hysteresis(ADC code threshold)
 */
#define CLK_NAFLL_LUT_ILWALID_HYSTERESIS_THRESHOLD                        (0xF)

/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// Routines that belong to the .perfClkAvfsInit overlay
ClkNafllLutConstruct        (clkNafllLutConstruct_10)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkNafllLutConstruct_10");

// Routines that belong to the .perfClkAvfs overlay
ClkNafllLutOverride         (clkNafllLutOverride_10)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutOverride_10");
ClkNafllLutEntriesProgram   (clkNafllLutEntriesProgram_10)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutEntriesProgram_10");
FLCN_STATUS clkNafllProgramLutBeforeEnablingAdcWar(CLK_NAFLL_DEVICE *pNafllDev, LwBool bSetWar)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllProgramLutBeforeEnablingAdcWar");
ClkNafllLutEntryCallwlate   (clkNafllLutEntryCallwlate_10)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutEntryCallwlate_10");
ClkNafllLutVfLwrveGet       (clkNafllLutVfLwrveGet_10)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutVfLwrveGet_10");

/* ------------------------ Include Derived Types -------------------------- */

#endif // PMU_CLKNAFLL_LUT_10_H
