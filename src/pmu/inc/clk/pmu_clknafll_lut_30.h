/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_LUT_30_H
#define PMU_CLKNAFLL_LUT_30_H

/*!
 * @file pmu_clknafll_lut_30.h
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
ClkNafllLutConstruct        (clkNafllLutConstruct_30)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkNafllLutConstruct_30");

// Routines that belong to the .perfClkAvfs overlay
ClkNafllLutOverride         (clkNafllLutOverride_30)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutOverride_30");
ClkNafllLutEntriesProgram   (clkNafllLutEntriesProgram_30)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutEntriesProgram_30");
ClkNafllLutEntryCallwlate   (clkNafllLutEntryCallwlate_30)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutEntryCallwlate_30");
ClkNafllLutVfLwrveGet       (clkNafllLutVfLwrveGet_30)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllLutVfLwrveGet_30");

/* ------------------------ Include Derived Types -------------------------- */

#endif // PMU_CLKNAFLL_LUT_30_H
