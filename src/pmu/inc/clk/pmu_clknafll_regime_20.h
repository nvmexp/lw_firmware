/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_REGIME_10_H
#define PMU_CLKNAFLL_REGIME_10_H

/*!
 * @file pmu_clknafll_10.h
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
FLCN_STATUS clkVfChangeInject(RM_PMU_CLK_CLK_DOMAIN_LIST *pClkList, RM_PMU_VOLT_VOLT_RAIL_LIST *pVoltList)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkVfChangeInject");

#endif // PMU_CLKNAFLL_REGIME_10_H
