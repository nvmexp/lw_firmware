/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_REGIME_3X_H
#define PMU_CLKNAFLL_REGIME_3X_H

/*!
 * @file pmu_clknafll_regime_3x.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clknafll_regime.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// Routines that belong to the .perfClkAvfs overlay
FLCN_STATUS clkNafllConfig_30(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus, RM_PMU_CLK_FREQ_TARGET_SIGNAL *pTarget)
        GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllConfig_30");
FLCN_STATUS clkNafllProgram_30(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus)
        GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllProgram_30");
LwU8 clkNafllDevTargetRegimeGet_30(CLK_NAFLL_DEVICE *pNafllDev, LwU16 targetFreqMHz, LwU16 dvcoMinFreqMHz, LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM *pVoltList)
        GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllDevTargetRegimeGet_30");
FLCN_STATUS clkNafllPldivConfig_30(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllPldivConfig_30");
FLCN_STATUS clkNafllPldivProgram_30(CLK_NAFLL_DEVICE *pNafllDev, LW2080_CTRL_CLK_FREQ_SOURCE_STATUS_NAFLL *pStatus)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkNafllPldivProgram_30");

/* ------------------------ Include Derived Types -------------------------- */

#endif // PMU_CLKNAFLL_REGIME_3X_H
