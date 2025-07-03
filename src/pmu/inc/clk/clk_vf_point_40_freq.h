/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_40_FREQ_H
#define CLK_VF_POINT_40_FREQ_H

/*!
 * @file clk_vf_point_40_freq.h
 * @brief @copydoc clk_vf_point_40_freq.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_40.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          CLK_VF_POINT_40_FREQ class type.
 *
 * @return  Pointer to the location of the CLK_VF_POINT_40_FREQ class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_40)
#define CLK_VF_POINT_40_FREQ_VTABLE() &ClkVfPoint40FreqVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE ClkVfPoint40FreqVirtualTable;
#else
#define CLK_VF_POINT_40_FREQ_VTABLE() NULL
#endif

/*!
 * Restrict the max allowed clock domain entries to some safe value on secondary
 * VF lwrve.
 */
#define CLK_CLK_VF_POINT_40_FREQ_FREQ_TUPLE_MAX_SIZE                      0x02U

/*!
 * Wrapper of 40_FREQ to 3X_SUPER.
 */
#define clkVfPointVoltageuVGet_40_FREQ(pVfPoint, voltageType, pVoltageuV)     \
    clkVfPointVoltageuVGet_40(pVfPoint, voltageType, pVoltageuV)

/*!
 * @copydoc ClkVfPoint40BaseVFTupleGet
 */
#define clkVfPoint40BaseVFTupleGet_FREQ(pVfPoint40)                           \
    (&((CLK_VF_POINT_40_FREQ *)(pVfPoint40))->baseVFTuple)

/*!
 * @copydoc ClkVfPoint40OffsetedVFTupleGet
 */
#define clkVfPoint40OffsetedVFTupleGet_FREQ(pVfPoint40)                       \
    (((CLK_VF_POINT_40_FREQ *)(pVfPoint40))->offsetedVFTuple)

/*!
 * @copydoc ClkVfPoint40OffsetVFTupleGet
 */
#define clkVfPoint40OffsetVFTupleGet_FREQ(pVfPoint40)                       \
    (((CLK_VF_POINT_40_FREQ *)(pVfPoint40))->offsetVFTuple)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 40 frequency structure. Defines a point on a PRIMARY CLK_DOMAIN's
 * VF lwrve for which frequency is the independent variable (i.e. fixed for this
 * VF point) and voltage is the dependent variable (i.e. varies with process
 * and temperature).
 */
typedef struct
{
    /*!
     * CLK_VF_POINT_40 super class. Must always be the first element in the structure.
     */
    CLK_VF_POINT_40 super;

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
        offsetedVFTuple[CLK_CLK_VF_POINT_40_FREQ_FREQ_TUPLE_MAX_SIZE];

    /*!
     * @ref LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE
     * Offset VF Tuple represent the VF tuple adjusted with the
     * applied offsets.
     */
    LW2080_CTRL_CLK_OFFSET_VF_TUPLE
        offsetVFTuple[CLK_CLK_VF_POINT_40_FREQ_FREQ_TUPLE_MAX_SIZE];

    /*!
     * This will give the deviation of given voltage from it's nominal value.
     */
    LwS32           voltDeltauV;

    /*!
     * This will give the deviation of given voltage from it's nominal value
     * applied from client
     */
    LwS32           clientVoltDeltauV;
} CLK_VF_POINT_40_FREQ;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet   (clkVfPointGrpIfaceModel10ObjSet_40_FREQ)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_40_FREQ");
BoardObjIfaceModel10GetStatus       (clkVfPointIfaceModel10GetStatus_40_FREQ)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatus_40_FREQ");

// CLK_VF_POINT interfaces
ClkVfPointClientVoltDeltaSet    (clkVfPointClientVoltDeltaSet_40_FREQ)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPointClientVoltDeltaSet_40_FREQ");

// CLK_VF_POINT_40 interfaces
ClkVfPoint40Load            (clkVfPoint40Load_FREQ)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfPoint40Load_FREQ");
ClkVfPoint40Cache           (clkVfPoint40Cache_FREQ)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPointCache_40_FREQ");
ClkVfPoint40OffsetVFCache   (clkVfPoint40OffsetVFCache_FREQ)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPointOffsetVFCache_40_FREQ");
ClkVfPoint40BaseVFCache     (clkVfPoint40BaseVFCache_FREQ)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPointBaseVFCache_40_FREQ");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_VF_POINT_40_FREQ_H
