/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_35_VOLT_PRI_H
#define CLK_VF_POINT_35_VOLT_PRI_H

/*!
 * @file clk_vf_point_35_volt_pri.h
 * @brief @copydoc clk_vf_point_35_volt_pri.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_35.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          CLK_VF_POINT_35_VOLT_PRI class type.
 *
 * @return  Pointer to the location of the CLK_VF_POINT_35_VOLT_PRI class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35)
#define CLK_VF_POINT_35_VOLT_PRI_VTABLE() &ClkVfPoint35VoltPriVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE ClkVfPoint35VoltPriVirtualTable;
#else
#define CLK_VF_POINT_35_VOLT_PRI_VTABLE() NULL
#endif

/*!
 * Restrict the max allowed clock domain entries to some safe value on secondary
 * VF lwrve.
 */
#define CLK_CLK_VF_POINT_35_VOLT_PRI_FREQ_TUPLE_MAX_SIZE                   0x05

/*!
 * @copydoc ClkVfPoint35BaseVFTupleGet
 */
#define clkVfPoint35BaseVFTupleGet_VOLT_PRI(pVfPoint35)                       \
    (&((CLK_VF_POINT_35_VOLT_PRI *)(pVfPoint35))->baseVFTuple)

/*!
 * @copydoc ClkVfPoint35OffsetedVFTupleGet
 */
#define clkVfPoint35OffsetedVFTupleGet_VOLT_PRI(pVfPoint35)                   \
    (((CLK_VF_POINT_35_VOLT_PRI *)(pVfPoint35))->offsetedVFTuple)

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
     * CLK_VF_POINT_35_VOLT super class.  Must always be the first element in the structure.
     */
    CLK_VF_POINT_35_VOLT                        super;

    /*!
     * @ref LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE
     * Base VF Tuple represent the values that are input / output of VFE.
     */
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE  baseVFTuple;

    /*!
     * @ref LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE
     * Offseted VF Tuple represent the VF tuple adjusted with the
     * applied offsets.
     */
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE
        offsetedVFTuple[CLK_CLK_VF_POINT_35_VOLT_PRI_FREQ_TUPLE_MAX_SIZE];
} CLK_VF_POINT_35_VOLT_PRI;


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjIfaceModel10GetStatus     (clkVfPointIfaceModel10GetStatus_35_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatus_35_VOLT_PRI");

// CLK_VF_POINT_35 interfaces
ClkVfPoint35Cache               (clkVfPoint35Cache_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint35Cache_VOLT_PRI");
ClkVfPoint35Smoothing           (clkVfPoint35Smoothing_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint35Smoothing_VOLT_PRI");
ClkVfPointLoad                  (clkVfPointLoad_35_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfPointLoad_35_VOLT_PRI");
ClkVfPoint35FreqTupleAccessor   (clkVfPoint35FreqTupleAccessor_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPoint35FreqTupleAccessor_VOLT_PRI");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_VF_POINT_35_VOLT_PRI_H
