/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_POINT_35_VOLT_SEC_H
#define CLK_VF_POINT_35_VOLT_SEC_H

/*!
 * @file clk_vf_point_35_volt_sec.h
 * @brief @copydoc clk_vf_point_35_volt_sec.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "clk/clk_vf_point_35.h"

/* ------------------------ Macros ----------------------------------------- */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          CLK_VF_POINT_35_VOLT_SEC class type.
 *
 * @return  Pointer to the location of the CLK_VF_POINT_35_VOLT_SEC class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_35) && PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_POINT_SEC)
#define CLK_VF_POINT_35_VOLT_SEC_VTABLE() &ClkVfPoint35VoltSecVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE ClkVfPoint35VoltSecVirtualTable;
#else
#define CLK_VF_POINT_35_VOLT_SEC_VTABLE() NULL
#endif

/*!
 * Restrict the max allowed clock domain entries to some safe value on secondary
 * VF lwrve.
 */
#define CLK_CLK_VF_POINT_35_VOLT_SEC_FREQ_TUPLE_MAX_SIZE                   0x01

/*!
 * @copydoc ClkVfPoint35BaseVFTupleGet
 */
#define clkVfPoint35BaseVFTupleGet_VOLT_SEC(pVfPoint35)                       \
    (&((CLK_VF_POINT_35_VOLT_SEC *)(pVfPoint35))->baseVFTuple.super)

/*!
 * @copydoc ClkVfPoint35OffsetedVFTupleGet
 */
#define clkVfPoint35OffsetedVFTupleGet_VOLT_SEC(pVfPoint35)                   \
    (((CLK_VF_POINT_35_VOLT_SEC *)(pVfPoint35))->offsetedVFTuple)

/*!
 * Helper macro to sanity check if secondary lwrve interface is supported.
 */
#define clkVfPoint35VoltSecSanityCheck(pVfPoint, lwrveIdx)                    \
    ((Clk.progs.vfSecEntryCount != 0) &&                                      \
     ((lwrveIdx) >= LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0) && \
     (((lwrveIdx) - LW2080_CTRL_CLK_CLK_PROG_35_PRIMARY_VF_LWRVE_IDX_SEC_0)    \
         < Clk.progs.vfSecEntryCount) &&                                      \
     (BOARDOBJ_GET_TYPE((pVfPoint)) == LW2080_CTRL_CLK_CLK_VF_POINT_TYPE_35_VOLT_SEC))

/*!
 * Helper macro to get active DVCO offset code.
 */
#define clkVfPoint35VoltSecActiveDvcoOffsetCodeGet(p35VoltSec)                \
    (((p35VoltSec)->dvcoOffsetCodeOverride !=                                 \
        LW2080_CTRL_CLK_CLK_VF_POINT_DVCO_OFFSET_CODE_ILWALID) ?              \
     (p35VoltSec)->dvcoOffsetCodeOverride :                                   \
     (p35VoltSec)->baseVFTuple.dvcoOffsetCode)

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
    CLK_VF_POINT_35_VOLT    super;

    /*!
     * DVCO offset override in terms of DVCO codes to trigger the fast
     * slowdown while HW switches from reference NDIV point to secondary NDIV.
     *
     * This value will be override by SET CONTROL and used in PMU when not
     * equal to @ref LW2080_CTRL_CLK_CLK_VF_POINT_DVCO_OFFSET_CODE_ILWALID
     */
    LwU8                    dvcoOffsetCodeOverride;

    /*!
     * @ref LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE_SEC
     * Base VF Tuple represent the values that are input / output of VFE.
     */
    LW2080_CTRL_CLK_CLK_VF_POINT_BASE_VF_TUPLE_SEC  baseVFTuple;

    /*!
     * @ref LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE
     * Offseted VF Tuple represent the VF tuple adjusted with the
     * applied offsets.
     */
    LW2080_CTRL_CLK_CLK_VF_POINT_VF_TUPLE
        offsetedVFTuple[CLK_CLK_VF_POINT_35_VOLT_SEC_FREQ_TUPLE_MAX_SIZE];
} CLK_VF_POINT_35_VOLT_SEC;


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkVfPointGrpIfaceModel10ObjSet_35_VOLT_SEC)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfPointGrpIfaceModel10ObjSet_35_VOLT_SEC");
BoardObjIfaceModel10GetStatus     (clkVfPointIfaceModel10GetStatus_35_VOLT_SEC)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfPointIfaceModel10GetStatus_35_VOLT_SEC");

// CLK_VF_POINT_35 interfaces
ClkVfPoint35Cache               (clkVfPoint35Cache_VOLT_SEC)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint35Cache_VOLT_SEC");
ClkVfPoint35Smoothing           (clkVfPoint35Smoothing_VOLT_SEC)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfPoint35Smoothing_VOLT_SEC");
ClkVfPoint35FreqTupleAccessor   (clkVfPoint35FreqTupleAccessor_VOLT_SEC)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfPoint35FreqTupleAccessor_VOLT_SEC");
ClkVfPointLoad                  (clkVfPointLoad_35_VOLT_SEC)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfPointLoad_35_VOLT_SEC");

/* ------------------------ Include Derived Types -------------------------- */

#endif // CLK_VF_POINT_35_VOLT_SEC_H
