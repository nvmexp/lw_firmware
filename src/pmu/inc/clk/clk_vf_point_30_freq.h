/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_30_FREQ_H
#define CLK_VF_POINT_30_FREQ_H

/*!
 * @file clk_vf_point_30_freq.h
 * @brief @copydoc clk_vf_point_30_freq.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_30.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          CLK_VF_POINT_30_FREQ class type.
 *
 * @return  Pointer to the location of the CLK_VF_POINT_30_FREQ class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30)
#define CLK_VF_POINT_30_FREQ_VTABLE() &ClkVfPoint30FreqVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE ClkVfPoint30FreqVirtualTable;
#else
#define CLK_VF_POINT_30_FREQ_VTABLE() NULL
#endif

/*!
 * Wrapper of 30_FREQ to 3X_SUPER.
 */
#define clkVfPointVoltageuVGet_30_FREQ(pVfPoint, voltageType, pVoltageuV)     \
    clkVfPointVoltageuVGet_30(pVfPoint, voltageType, pVoltageuV)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 30 frequency structure. Defines a point on a PRIMARY CLK_DOMAIN's
 * VF lwrve for which frequency is the independent variable (i.e. fixed for this
 * VF point) and voltage is the dependent variable (i.e. varies with process
 * and temperature).
 */
typedef struct
{
    /*!
     * CLK_VF_POINT_30 super class.  Must always be the first element in the structure.
     */
    CLK_VF_POINT_30    super;

    /*!
     * This will give the deviation of given voltage from it's nominal value.
     */
    LwS32           voltDeltauV;
} CLK_VF_POINT_30_FREQ;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkVfPointGrpIfaceModel10ObjSet_30_FREQ)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_30_FREQ");

// CLK_VF_POINT_30 interfaces
FLCN_STATUS clkVfPoint30Cache_FREQ(CLK_VF_POINT_30 *pVfPoint30, CLK_DOMAIN_30_PRIMARY *pDomain30Primary, CLK_PROG_30_PRIMARY *pProg30Primary, LwU8 voltRailIdx, LW2080_CTRL_CLK_VF_PAIR *pVfPairLast, LW2080_CTRL_CLK_VF_PAIR *pBaseVFPairLast, LW2080_CTRL_CLK_VF_PAIR *pBaseVFPairLwrr)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPointCache_30_FREQ");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_VF_POINT_30_FREQ_H
