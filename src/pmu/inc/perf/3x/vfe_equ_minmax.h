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
 * @file    vfe_equ_minmax.h
 * @brief   VFE_EQU_MINMAX class interface.
 */

#ifndef VFE_EQU_MINMAX_H
#define VFE_EQU_MINMAX_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_equ.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_EQU_MINMAX VFE_EQU_MINMAX;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the VFE_EQU_MINMAX
 *          class type.
 *
 * @return  Pointer to the location of the VFE_EQU_MINMAX class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU)
#define PERF_VFE_EQU_MINMAX_VTABLE() &VfeEquMinMaxVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeEquMinMaxVirtualTable;
#else
#define PERF_VFE_EQU_MINMAX_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief The VFE_EQU class providing attributes of VFE Min/Max Equation.
 *
 * The Min/Max Equation type returns the minimum or maximum of the result of
 * two equations.
 *
 * @code{.c}
 * result = Criteria(Evaluate(Index0), Evaluate(Index1));
 * @endcode
 *
 * @note This class relwrsively evaluates equations to determine the results of
 * the two equations.
 *
 * @extends VFE_EQU
 */
struct VFE_EQU_MINMAX
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
     * @brief When true, the class evaluates the maximum value; otherwise, it
     * evaluates the minimum value.
     * @protected
     */
    LwBool          bMax;

    /*!
     * @brief Index of the first equation used by min()/max().
     * @protected
     */
    LwVfeEquIdx     equIdx0;

    /*!
     * @brief Index of the second equation used by min()/max().
     * @protected
     */
    LwVfeEquIdx     equIdx1;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeEquGrpIfaceModel10ObjSetImpl_MINMAX)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetImpl_MINMAX");
BoardObjGrpIfaceModel10ObjSet   (vfeEquGrpIfaceModel10ObjSetValidate_MINMAX)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetValidate_MINMAX");

// VFE_EQU interfaces.
VfeEquEvalNode      (vfeEquEvalNode_MINMAX);

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_EQU_MINMAX_H
