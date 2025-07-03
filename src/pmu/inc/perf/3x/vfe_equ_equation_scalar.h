/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe_equ_equation_scalar.h
 * @brief   VFE_EQU_EQUATION_SCALAR class interface.
 */

#ifndef VFE_EQU_EQUATION_SCALAR_H
#define VFE_EQU_EQUATION_SCALAR_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_equ.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_EQU_EQUATION_SCALAR VFE_EQU_EQUATION_SCALAR;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_EQU_EQUATION_SCALAR class type.
 *
 * @return  Pointer to the location of the VFE_EQU_EQUATION_SCALAR class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU_35)
#define PERF_VFE_EQU_EQUATION_SCALAR_VTABLE() &VfeEquEquationScalarVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeEquEquationScalarVirtualTable;
#else
#define PERF_VFE_EQU_EQUATION_SCALAR_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief VFE_EQU class providing attributes of VFE Equation Scalar.
 *
 * This class evaluates by scaling an equation pointed by "The Index of The
 * Equation to Scale" (`Ind1`) by the VFE common Independent Variable after
 * evaluating them.
 *
 * @code{.c}
 * result = Ind1 * Evaluate(Index0)
 * @endcode
 *
 * @note This class relwrsively evaluates equations to determine the result to
 * scale.
 *
 * @extends VFE_EQU
 */
struct VFE_EQU_EQUATION_SCALAR
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
     * @brief The Index of VFE_EQU to scale.
     * @protected
     */
    LwVfeEquIdx     equIdxToScale;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeEquGrpIfaceModel10ObjSetImpl_EQUATION_SCALAR)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetImpl_EQUATION_SCALAR");
BoardObjGrpIfaceModel10ObjSet   (vfeEquGrpIfaceModel10ObjSetValidate_EQUATION_SCALAR)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetValidate_EQUATION_SCALAR");

// VFE_EQU interfaces.
VfeEquEvalNode      (vfeEquEvalNode_EQUATION_SCALAR);

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_EQU_EQUATION_SCALAR_H
