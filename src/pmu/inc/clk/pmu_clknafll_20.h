/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef PMU_CLKNAFLL_20_H
#define PMU_CLKNAFLL_20_H

/*!
 * @file pmu_clknafll_20.h
 */

/* ------------------------ System Includes -------------------------------- */
/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_NAFLL_DEVICE_V20 CLK_NAFLL_DEVICE_V20, NAFLL_DEVICE_V20;

/* ------------------------ Application Includes --------------------------- */
#include "clk/pmu_clknafll.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the NAFLL_DEVICE_V20
 *          class type.
 *
 * @return  Pointer to the location of the NAFLL_DEVICE_V20 class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_DEVICE_V20)
#define CLK_NAFLL_DEVICE_V20_VTABLE() &ClkNafllDeviceV20VirtualTable
extern BOARDOBJ_VIRTUAL_TABLE ClkNafllDeviceV20VirtualTable;
#else
#define CLK_NAFLL_DEVICE_V20_VTABLE() NULL
#endif

/* ------------------------ Types Definitions ------------------------------ */
/*!
 * Structure describing an individual V20 NAFLL Device
 */
struct CLK_NAFLL_DEVICE_V20
{
    CLK_NAFLL_DEVICE   super;
};

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Static Variables ------------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// Routines that belong to the .perfClkAvfsInit overlay
BoardObjGrpIfaceModel10ObjSet          (clkNafllGrpIfaceModel10ObjSet_V20)
    GCC_ATTRIB_SECTION("imem_perfClkAvfsInit", "clkNafllGrpIfaceModel10ObjSet_V20");

#endif // PMU_CLKNAFLL_20_H
