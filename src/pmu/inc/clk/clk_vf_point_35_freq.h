/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_35_FREQ_H
#define CLK_VF_POINT_35_FREQ_H

/*!
 * @file clk_vf_point_35_freq.h
 * @brief @copydoc clk_vf_point_35_freq.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_35.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          CLK_VF_POINT_35_FREQ class type.
 *
 * @return  Pointer to the location of the CLK_VF_POINT_35_FREQ class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35)
#define CLK_VF_POINT_35_FREQ_VTABLE() &ClkVfPoint35FreqVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE ClkVfPoint35FreqVirtualTable;
#else
#define CLK_VF_POINT_35_FREQ_VTABLE() NULL
#endif

/*!
 * Restrict the max allowed clock domain entries to some safe value on secondary
 * VF lwrve.
 */
#define CLK_CLK_VF_POINT_35_FREQ_FREQ_TUPLE_MAX_SIZE                       0x02U

/*!
 * Wrapper of 35_FREQ to 3X_SUPER.
 */
#define clkVfPointVoltageuVGet_35_FREQ(pVfPoint, voltageType, pVoltageuV)     \
    clkVfPointVoltageuVGet_35(pVfPoint, voltageType, pVoltageuV)

/*!
 * @copydoc ClkVfPoint35BaseVFTupleGet
 */
#define clkVfPoint35BaseVFTupleGet_FREQ(pVfPoint35)                           \
    (&((CLK_VF_POINT_35_FREQ *)(pVfPoint35))->baseVFTuple)

/*!
 * @copydoc ClkVfPoint35OffsetedVFTupleGet
 */
#define clkVfPoint35OffsetedVFTupleGet_FREQ(pVfPoint35)                       \
    (((CLK_VF_POINT_35_FREQ *)(pVfPoint35))->offsetedVFTuple)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock VF Point 35 frequency structure. Defines a point on a PRIMARY CLK_DOMAIN's
 * VF lwrve for which frequency is the independent variable (i.e. fixed for this
 * VF point) and voltage is the dependent variable (i.e. varies with process
 * and temperature). The structure also contains the secondary's VF point as well,
 * based on its relationship with the primary domain.
 */
typedef struct
{
    /*!
     * CLK_VF_POINT_35 super class.  Must always be the first element in the structure.
     */
    CLK_VF_POINT_35 super;

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
        offsetedVFTuple[CLK_CLK_VF_POINT_35_FREQ_FREQ_TUPLE_MAX_SIZE];

    /*!
     * This will give the deviation of given voltage from it's nominal value.
     */
    LwS32           voltDeltauV;
} CLK_VF_POINT_35_FREQ;

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet   (clkVfPointGrpIfaceModel10ObjSet_35_FREQ)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_35_FREQ");
BoardObjIfaceModel10GetStatus       (clkVfPointIfaceModel10GetStatus_35_FREQ)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatus_35_FREQ");

// CLK_VF_POINT interfaces
ClkVfPointLoad          (clkVfPointLoad_35_FREQ)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfPointLoad_35_FREQ");

// CLK_VF_POINT_35 interfaces
ClkVfPoint35Cache   (clkVfPoint35Cache_FREQ)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPointCache_35_FREQ");
ClkVfPoint35Smoothing       (clkVfPoint35Smoothing_FREQ)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint35Smoothing_FREQ");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_VF_POINT_35_FREQ_H
