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
 * @file    vfe_equ.h
 * @brief   VFE_EQU and VFE_EQUS class interfaces.
 *
 * @note    Do not use these functions directly for VFE_EQU code
 *          or derived classes, instead use the VFE specific wrappers
 *          around these functions, or create VFE wrappers if need be.
 *          This is to facilitate the varying VFE index size
 *          (8-bit versus 16-bit) issue in PMU code.
 */

#ifndef VFE_EQU_H
#define VFE_EQU_H

#include "g_vfe_equ.h"

#ifndef G_VFE_EQU_H
#define G_VFE_EQU_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrpmask_e1024.h"
#include "boardobj/boardobjgrp_e1024.h"
#include "perf/3x/vfe_var.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_EQU VFE_EQU, VFE_EQU_BASE;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Accessor to obtain a pointer to primary VFE_EQU.
 *
 * @memberof VFE_EQUS
 * @public
 *
 * @param[in]  _pPerf  Pointer to PERF object containing the primary VFE_EQU.
 *
 * @return A pointer to the primary VFE_EQU if present; NULL if not.
 */
#define PERF_GET_VFE_EQUS(_pPerf)                                              \
    (PERF_GET_VFE(_pPerf)->pEqus)

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 * @memberof VFE_EQUS
 * @public

 * The type colwersion code expects the class ID to match the boardObjGrp
 * data location.
 */
#define BOARDOBJGRP_DATA_LOCATION_VFE_EQU                                      \
    (&(PERF_GET_VFE_EQUS(&Perf)->super.super))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Evaluates list of VFE Equations.
 *
 * @param[in]   pContext    VFE Context pointer.
 * @param[in]   vfeEquIdx   Equation list head node index.
 * @param[out]  pResult     Buffer to hold evaluation result.
 *
 * @return  FLCN_OK Upon successful evaluation.
 * @return  Error propagated from inner functions.
 */
#define VfeEquEvalList(fname) FLCN_STATUS (fname)(VFE_CONTEXT *pContext, LwVfeEquIdx vfeEquIdx, LwF32 *pResult)

/*!
 * @brief Evaluates single node of VFE Equation list.
 *
 * @param[in]   pContext    VFE Context pointer.
 * @param[in]   pVfeEqu     VFE Equation node pointer.
 * @param[out]  pResult     Buffer to hold evaluation result.
 *
 * @return  FLCN_OK Upon successful evaluation.
 * @return  Error propagated from inner functions.
 */
#define VfeEquEvalNode(fname) FLCN_STATUS (fname)(VFE_CONTEXT *pContext, VFE_EQU *pVfeEqu, LwF32 *pResult)

/*!
 * @brief Base class for all equations.
 *
 * Equations evaluate to an output as a function of an Independent Variable. The
 * form of the function is specified per the Equation class (below). Each
 * equation/function evaluates to a IEEE-754 32-bit floating point (binary32) of
 * specified Output Type.
 *
 * **Input Field Restrictions:**
 *
 * In the IEEE-754 32-bit floating point (binary32) fields, exceptional values
 * (i.e. NaNs, +/-INF, -0) and subnormal values (which have a loss of precision)
 * are prohibited. Specifically, this means:
 *
 * @li The exponent (bits 23-30) may not be 0xff.
 * @li The exponent (bits 23-30) may not be 0x00 unless the entire field
 * (bits 0-31) are 0x00000000.
 *
 * Equivalently, this rule can be specified using the constants returned by
 * clib's standard fpclassify function as:
 *
 * @li Only FP_NORMAL and FP_ZERO are permitted.
 * @li If the value is FP_ZERO, then it must be colwerted to positive zero
 * (0x00000000).
 *
 * **Output Range Limits**
 *
 * The output (output) of each equation is bound by the specified Output Range
 * Minimum (outputmin) and Output Range Maximum (outputmax), both of which are
 * IEEE-754 32-bit floating point (binary32) values.
 *
 * @code{.c}
 * output = max(min(output, outputmax), outputmin);
 * @endcode
 *
 * **Equation Chaining**
 *
 * Each Equation points to a possible Next Equation - i.e. forming a linked list
 * of Equations. The output of any Equation is the sum of its function and the
 * functions of all Equations pointed to by the successive Next Equation
 * Pointers. Thus, any evaluator which refers to an Equation Entry is really
 * referring to the linked list of Equations which starts at that entry.
 *
 * The sum of all Equations in the linked list is callwlated as a IEEE-754
 * 32-bit floating point (binary32) value.
 *
 * @extends BOARDOBJ
 */
struct VFE_EQU
{
    /*!
     * @brief Base class.
     *
     * @protected
     *
     * Must be first element of the structure to allow casting to parent class.
     */
    BOARDOBJ        super;

    /*!
     * @brief Index of an Independent Variable (VFE_VAR) used by child object
     * (where required).
     *
     * @protected
     *
     * If the equation does not require a VFE_VAR, this data member shall be
     * set @ref LW2080_CTRL_PERF_VFE_VAR_INDEX_ILWALID.
     */
    LwU8            varIdx;

    /*!
     * @brief Index of the next VFE Equation in the Equation Chain (linked list).
     *
     * @protected
     *
     * List is terminated by @ref
     * PMU_PERF_VFE_EQU_INDEX_ILWALID value.
     */
    LwVfeEquIdx     equIdxNext;

    /*!
     * @brief Equation's result type as @ref
     * LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_ENUM.
     *
     * @protected
     */
    LwU8            outputType;

    /*!
     * @brief Lower limit for the equation value (as IEEE-754 32-bit floating
     * point).
     *
     * @protected
     */
    LwF32           outRangeMin;

    /*!
     * @brief Upper limit for the equation value (as IEEE-754 32-bit floating
     * point).
     *
     * @protected
     */
    LwF32           outRangeMax;
};

/*!
 * @brief Container of all VFE Equations and related information.
 *
 * @extends BOARDOBJGRP_E1024
 */
typedef struct
{
    /*!
     * @brief Base class.
     *
     * @protected
     *
     * Must be first element of the structure to allow casting to parent class.
     */
    VFE_EQU_BOARDOBJGRP super;
} VFE_EQUS;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler   (vfeEquBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "vfeEquBoardObjGrpIfaceModel10Set");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet       (vfeEquGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjGrpIfaceModel10ObjSet       (vfeEquGrpIfaceModel10ObjSetValidate_SUPER)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetValidate_SUPER");

// VFE_EQU interfaces.
mockable    VfeEquEvalList  (vfeEquEvalList);
mockable    VfeEquEvalNode  (vfeEquEvalNode_SUPER);

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/3x/vfe_equ_compare.h"
#include "perf/3x/vfe_equ_minmax.h"
#include "perf/3x/vfe_equ_quadratic.h"
#include "perf/3x/vfe_equ_equation_scalar.h"

#endif // G_VFE_EQU_H
#endif // VFE_EQU_H
