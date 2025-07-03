/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_35_VOLT_H
#define CLK_VF_POINT_35_VOLT_H

#include "g_clk_vf_point_35_volt.h"

#ifndef G_CLK_VF_POINT_35_VOLT_H
#define G_CLK_VF_POINT_35_VOLT_H

/*!
 * @file clk_vf_point_35_volt.h
 * @brief @copydoc clk_vf_point_35_volt.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_35.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 35 Voltage structure.  Defines a point on a PRIMARY CLK_DOMAIN's
 * VF lwrve for which frequency is the independent variable (i.e. fixed for this
 * VF point) and voltage is the dependent variable (i.e. varies with process
 * and temperature).
 */
typedef struct
{
    /*!
     * CLK_VF_POINT_35 super class.  Must always be the first element in the structure.
     */
    CLK_VF_POINT_35             super;

    /*!
     * Source voltage (uV) which was used to specify this CLK_VF_POINT_35_VOLT.
     * These are the voltage values supported by the ADC/NAFLL.  This value will
     * be rounded to the regulator size supported by the VOLTAGE_RAIL and stored
     * in @ref super.voltageuV.  However, this source voltage value should be
     * used when looking up data corresponding to the original ADC/NAFLL values.
     */
    LwU32                       sourceVoltageuV;
    /*!
     * This will give the deviation of given freq from it's nominal value.
     */
    LW2080_CTRL_CLK_FREQ_DELTA  freqDelta;
} CLK_VF_POINT_35_VOLT;


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkVfPointGrpIfaceModel10ObjSet_35_VOLT)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_35_VOLT");

// CLK_VF_POINT interfaces
mockable ClkVfPointVoltageuVGet  (clkVfPointVoltageuVGet_35_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointVoltageuVGet_35_VOLT");
mockable ClkVfPointLoad          (clkVfPointLoad_35_VOLT)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfPointLoad_35_VOLT");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_vf_point_35_volt_pri.h"
#include "clk/clk_vf_point_35_volt_sec.h"

#endif // G_CLK_VF_POINT_35_VOLT_H
#endif // CLK_VF_POINT_35_VOLT_H
