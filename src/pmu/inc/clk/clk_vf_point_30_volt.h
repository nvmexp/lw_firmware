/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_30_VOLT_H
#define CLK_VF_POINT_30_VOLT_H

/*!
 * @file clk_vf_point_30_volt.h
 * @brief @copydoc clk_vf_point_30_volt.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_30.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          CLK_VF_POINT_30_VOLT class type.
 *
 * @return  Pointer to the location of the CLK_VF_POINT_30_VOLT class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_30)
#define CLK_VF_POINT_30_VOLT_VTABLE() &ClkVfPoint30VoltVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE ClkVfPoint30VoltVirtualTable;
#else
#define CLK_VF_POINT_30_VOLT_VTABLE() NULL
#endif

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 30 Voltage structure.  Defines a point on a PRIMARY CLK_DOMAIN's
 * VF lwrve for which frequency is the independent variable (i.e. fixed for this
 * VF point) and voltage is the dependent variable (i.e. varies with process
 * and temperature).
 */
typedef struct
{
    /*!
     * CLK_VF_POINT_30 super class.  Must always be the first element in the structure.
     */
    CLK_VF_POINT_30 super;

    /*!
     * Source voltage (uV) which was used to specify this CLK_VF_POINT_30_VOLT.
     * These are the voltage values supported by the ADC/NAFLL.  This value will
     * be rounded to the regulator size supported by the VOLTAGE_RAIL and stored
     * in @ref super.voltageuV.  However, this source voltage value should be
     * used when looking up data corresponding to the original ADC/NAFLL values.
     */
    LwU32   sourceVoltageuV;
    /*!
     * This will give the deviation of given freq from it's nominal value.
     */
    LW2080_CTRL_CLK_FREQ_DELTA freqDelta;
} CLK_VF_POINT_30_VOLT;


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkVfPointGrpIfaceModel10ObjSet_30_VOLT)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_30_VOLT");

// CLK_VF_POINT_30 interfaces
FLCN_STATUS clkVfPoint30Cache_VOLT(CLK_VF_POINT_30 *pVfPoint30, CLK_DOMAIN_30_PRIMARY *pDomain30Primary, CLK_PROG_30_PRIMARY *pProg30Primary, LwU8 voltRailIdx, LW2080_CTRL_CLK_VF_PAIR *pVfPairLast, LW2080_CTRL_CLK_VF_PAIR *pBaseVFPairLast, LW2080_CTRL_CLK_VF_PAIR *pBaseVFPairLwrr)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint30Cache_VOLT");
ClkVfPointVoltageuVGet  (clkVfPointVoltageuVGet_30_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointVoltageuVGet_30_VOLT");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_VF_POINT_30_VOLT_H
