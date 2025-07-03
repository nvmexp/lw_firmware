/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_40_VOLT_H
#define CLK_VF_POINT_40_VOLT_H

/*!
 * @file clk_vf_point_40_volt.h
 * @brief @copydoc clk_vf_point_40_volt.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_40.h"

/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 40 Voltage structure.  Defines a point on a PRIMARY CLK_DOMAIN's
 * VF lwrve for which frequency is the independent variable (i.e. fixed for this
 * VF point) and voltage is the dependent variable (i.e. varies with process
 * and temperature).
 */
struct CLK_VF_POINT_40_VOLT
{
    /*!
     * CLK_VF_POINT_40 super class.  Must always be the first element in the structure.
     */
    CLK_VF_POINT_40             super;

    /*!
     * Source voltage (uV) which was used to specify this CLK_VF_POINT_40_VOLT.
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
    /*!
     * CPM max Freq Offset override value
     */
    LwU16 clientCpmMaxFreqOffsetOverrideMHz;

};

/*!
 * @interface CLK_VF_POINT_40_VOLT
 *
 * Smoothen the dynamically evaluated frequency value for this
 * CLK_VF_POINT_40_VOLT per the semantics of this _VOLT object's class.
 *
 * @param[in]       pVfPoint40Volt      CLK_VF_POINT_40_VOLT pointer
 * @param[in/out]   pVfPoint40VoltLast  CLK_VF_POINT_40_VOLT pointer pointing to the last
 *                                      evaluated clk vf point.
 * @param[in]       pDomain40Prog       CLK_DOMAIN_40_PROG pointer
 * @param[in]       pVfRelRatioVolt     CLK_VF_REL_RATIO_VOLT Pointer
 * @param[in]       lwrveIdx            Vf lwrve index.
 * @param[in]       cacheIdx
 *      VF lwrve cache buffer index (temperature / step size). If the cacheIdx
 *      is marked "CLK_CLK_VF_POINT_VF_CACHE_IDX_ILWALID", it represents to use
 *      the main active buffers instead of cache buffers.
 *
 * @return FLCN_OK
 *     CLK_VF_POINT_40_VOLT successfully smoothened.
 * @return Other errors
 *     Unexpected errors oclwrred during VF lwrve smoothing.
 */
#define ClkVfPoint40VoltSmoothing(fname) FLCN_STATUS (fname)(CLK_VF_POINT_40_VOLT *pVfPoint40Volt, CLK_VF_POINT_40_VOLT *pVfPoint40VoltLast, CLK_DOMAIN_40_PROG *pDomain40Prog, CLK_VF_REL_RATIO_VOLT *pVfRelRatioVolt, LwU8 lwrveIdx, LwU8 cacheIdx)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet           (clkVfPointGrpIfaceModel10ObjSet_40_VOLT)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_40_VOLT");

// CLK_VF_POINT interfaces
ClkVfPointVoltageuVGet                     (clkVfPointVoltageuVGet_40_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointVoltageuVGet_40_VOLT");
ClkVfPointClientCpmMaxOffsetOverrideSet    (clkVfPointClientCpmMaxOffsetOverrideSet_40_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointClientCpmMaxOffsetOverrideSet_40_VOLT");

// CLK_VF_POINT_40 interfaces
ClkVfPoint40Load                (clkVfPoint40Load_VOLT)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfPoint40Load_VOLT");
ClkVfPoint40Cache               (clkVfPoint40Cache_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40Cache_VOLT");
ClkVfPoint40SecondaryCache          (clkVfPoint40SecondaryCache_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40SecondaryCache_VOLT");
ClkVfPoint40OffsetVFCache       (clkVfPoint40OffsetVFCache_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40OffsetVFCache_VOLT");
ClkVfPoint40BaseVFCache         (clkVfPoint40BaseVFCache_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40BaseVFCache_VOLT");
ClkVfPoint40SecondaryBaseVFCache    (clkVfPoint40SecondaryBaseVFCache_VOLT)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40SecondaryBaseVFCache_VOLT");
ClkVfPoint40VoltSmoothing       (clkVfPoint40VoltSmoothing)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40VoltSmoothing");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_vf_point_40_volt_pri.h"
#include "clk/clk_vf_point_40_volt_sec.h"

#endif // CLK_VF_POINT_40_VOLT_H
