/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_30_H
#define PMU_CLKNAFLL_30_H

/*!
 * @file pmu_clknafll_30.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_NAFLL_DEVICE_V30 CLK_NAFLL_DEVICE_V30;

/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clknafll.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure describing an individual V30 NAFLL Device
 */
struct CLK_NAFLL_DEVICE_V30
{
    CLK_NAFLL_DEVICE   super;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// Routines that belong to the .perfClkAvfsInit overlay
BoardObjGrpIfaceModel10ObjSet          (clkNafllGrpIfaceModel10ObjSet_V30)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkNafllGrpIfaceModel10ObjSet_V30");

#endif // PMU_CLKNAFLL_30_H
