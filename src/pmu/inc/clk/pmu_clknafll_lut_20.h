/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_LUT_20_H
#define PMU_CLKNAFLL_LUT_20_H

/*!
 * @file pmu_clknafll_lut_20.h
 *
 */

/* ------------------------ System Includes -------------------------------- */
#include "main.h"

/* ------------------------ Forward Definitions ---------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clknafll_lut.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// Routines that belong to the .perfClkAvfsInit overlay
ClkNafllLutConstruct        (clkNafllLutConstruct_20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkNafllLutConstruct_20");

// Routines that belong to the .perfClkAvfs overlay
ClkNafllLutOverride         (clkNafllLutOverride_20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutOverride_20");
ClkNafllLutEntriesProgram   (clkNafllLutEntriesProgram_20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutEntriesProgram_20");
ClkNafllLutEntryCallwlate   (clkNafllLutEntryCallwlate_20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutEntryCallwlate_20");
ClkNafllLutVfLwrveGet       (clkNafllLutVfLwrveGet_20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutVfLwrveGet_20");

/* ------------------------ Include Derived Types -------------------------- */

#endif // PMU_CLKNAFLL_LUT_20_H
