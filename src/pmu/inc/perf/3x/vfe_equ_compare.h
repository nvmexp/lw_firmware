/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_equ_compare.h
 * @brief   VFE_EQU_COMPARE class interface.
 */

#ifndef VFE_EQU_COMPARE_H
#define VFE_EQU_COMPARE_H

#include "g_vfe_equ_compare.h"

#ifndef G_VFE_EQU_COMPARE_H
#define G_VFE_EQU_COMPARE_H


/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_equ.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_EQU_COMPARE VFE_EQU_COMPARE;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the VFE_EQU_COMPARE
 *          class type.
 *
 * @return  Pointer to the location of the VFE_EQU_COMPARE class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU)
#define PERF_VFE_EQU_COMPARE_VTABLE() &VfeEquCompareVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeEquCompareVirtualTable;
#else
#define PERF_VFE_EQU_COMPARE_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief The VFE_EQU class providing attributes of VFE Compare Equation.
 *
 * The Compare Equation type compares an Independent Variable (Ind) against a
 * Criteria value (Criteria) with a given boolean Comparator function
 * (Comparator()). Depending on the result of the boolean Comparison Function,
 * the class will evaluate the equation specified by Comparator Result indexes
 * (IndexTrue, IndexFalse).
 *
 * @code{.c}
 * result = Comparator(Ind, Criteria) ? Evaluate(IndexTrue) : Evaluate(IndexFalse);
 * @endcode
 *
 * @note This class relwrsively evaluates equations based on the results of the
 * comparison.
 *
 * @extends VFE_EQU
 */
struct VFE_EQU_COMPARE
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    VFE_EQU         super;

    /*!
     * @brief Comparison function ID.
     *
     * The function ID is specifed as
     * @ref LW2080_CTRL_PERF_VFE_EQU_COMPARE_FUNCTION_ENUM.
     *
     * @protected
     */
    LwU8            funcId;

    /*!
     * @brief Index of an equation to evaluate when the comparison is true.
     * @protected
     */
    LwVfeEquIdx     equIdxTrue;

    /*!
     * @brief Index of an equation to evaluate when the comparison false.
     * @protected
     */
    LwVfeEquIdx     equIdxFalse;

    /*!
     * @brief Numeric value used when evaluating comparison function @ref funcId
     * (as IEEE-754 32-bit floating point).
     * @protected
     */
    LwF32           criteria;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeEquGrpIfaceModel10ObjSetImpl_COMPARE)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetImpl_COMPARE");
BoardObjGrpIfaceModel10ObjSet   (vfeEquGrpIfaceModel10ObjSetValidate_COMPARE)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetValidate_COMPARE");

// VFE_EQU interfaces.
mockable    VfeEquEvalNode  (vfeEquEvalNode_COMPARE);

/* ------------------------ Include Derived Types --------------------------- */

#endif // G_VFE_EQU_COMPARE_H
#endif // VFE_EQU_COMPARE_H
