/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_40_VOLT_PRI_H
#define CLK_VF_POINT_40_VOLT_PRI_H

/*!
 * @file clk_vf_point_40_volt_pri.h
 * @brief @copydoc clk_vf_point_40_volt_pri.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_40_volt.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          CLK_VF_POINT_40_VOLT_PRI class type.
 *
 * @return  Pointer to the location of the CLK_VF_POINT_40_VOLT_PRI class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40)
#define CLK_VF_POINT_40_VOLT_PRI_VTABLE() &ClkVfPoint40VoltPriVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE ClkVfPoint40VoltPriVirtualTable;
#else
#define CLK_VF_POINT_40_VOLT_PRI_VTABLE() NULL
#endif

/*!
 * Restrict the max allowed clock domain entries to some safe value on secondary
 * VF lwrve.
 */
#define CLK_CLK_VF_POINT_40_VOLT_PRI_FREQ_TUPLE_MAX_SIZE                  0x05U

/*!
 * @copydoc ClkVfPoint40BaseVFTupleGet
 */
#define clkVfPoint40BaseVFTupleGet_VOLT_PRI(pVfPoint40)                       \
    (&((CLK_VF_POINT_40_VOLT_PRI *)(pVfPoint40))->baseVFTuple)

/*!
 * @copydoc ClkVfPoint40OffsetedVFTupleGet
 */
#define clkVfPoint40OffsetedVFTupleGet_VOLT_PRI(pVfPoint40)                   \
    (((CLK_VF_POINT_40_VOLT_PRI *)(pVfPoint40))->offsetedVFTuple)

/*!
 * @copydoc ClkVfPoint40OffsetVFTupleGet
 */
#define clkVfPoint40OffsetVFTupleGet_VOLT_PRI(pVfPoint40)                   \
    (((CLK_VF_POINT_40_VOLT_PRI *)(pVfPoint40))->offsetVFTuple)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 40 Voltage structure.  Defines a point on a PRIMARY CLK_DOMAIN's
 * VF lwrve for which frequency is the independent variable (i.e. fixed for this
 * VF point) and voltage is the dependent variable (i.e. varies with process
 * and temperature).
 */
typedef struct
{
    /*!
     * CLK_VF_POINT_40_VOLT super class.  Must always be the first element in the structure.
     */
    CLK_VF_POINT_40_VOLT                        super;

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
        offsetedVFTuple[CLK_CLK_VF_POINT_40_VOLT_PRI_FREQ_TUPLE_MAX_SIZE];

    /*!
     * @ref LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE
     * Offset VF Tuple represent the VF tuple adjusted with the
     * applied offsets.
     */
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE
        offsetVFTuple[CLK_CLK_VF_POINT_40_VOLT_PRI_FREQ_TUPLE_MAX_SIZE];
} CLK_VF_POINT_40_VOLT_PRI;


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
BoardObjIfaceModel10GetStatus           (clkVfPointIfaceModel10GetStatus_40_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatus_40_VOLT_PRI");

// CLK_VF_POINT_40 interfaces
ClkVfPoint40Cache       (clkVfPoint40Cache_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40Cache_VOLT_PRI");
ClkVfPoint40OffsetVFCache (clkVfPoint40OffsetVFCache_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40OffsetVFCache_VOLT_PRI");
ClkVfPoint40BaseVFCache (clkVfPoint40BaseVFCache_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint40BaseVFCache_VOLT_PRI");
ClkVfPoint40Load        (clkVfPoint40Load_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfPoint40Load_VOLT_PRI");
ClkVfPoint40FreqTupleAccessor   (clkVfPoint40FreqTupleAccessor_VOLT_PRI)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkVfPoint40FreqTupleAccessor_VOLT_PRI");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_VF_POINT_40_VOLT_PRI_H
