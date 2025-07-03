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
 * @file    vfe_equ_quadratic.h
 * @brief   VFE_EQU_QUADRATIC class interface.
 */

#ifndef VFE_EQU_QUADRATIC_H
#define VFE_EQU_QUADRATIC_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_equ.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_EQU_QUADRATIC VFE_EQU_QUADRATIC;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the VFE_EQU_QUADRATIC
 *          class type.
 *
 * @return  Pointer to the location of the VFE_EQU_QUADRATIC class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_EQU)
#define PERF_VFE_EQU_QUADRATIC_VTABLE() &VfeEquQuadraticVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeEquQuadraticVirtualTable;
#else
#define PERF_VFE_EQU_QUADRATIC_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief VFE_EQU class providing attributes of VFE Quadratic Equation.
 *
 * A quadratic equation based on an Independent Variable `Ind`, which is a
 * signed floating point value with fractional precision specified by the
 * Independent Variable. The format of the quadratic is, thus:
 *
 * @code{.c}
 * result = (Ind^2 * C2) + (Ind * C1) + C0
 * @endcode
 *
 * where
 * * `C0` is @ref coeffs[0]
 * * `C1` is @ref coeffs[1]
 * * `C2` is @ref coeffs[2]
 *
 * @extends VFE_EQU
 */
struct VFE_EQU_QUADRATIC
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    VFE_EQU super;

    /*!
     * @brief Quadratic Equation's coefficients (as IEEE-754 32-bit floating
     * point).
     *
     * The index into the array correponds to the coefficient (i.e. coeffs[0] =
     * C0, coeffs[1] = C1, etc.).
     *
     * @protected
     */
    LwF32 coeffs[LW2080_CTRL_PERF_VFE_EQU_QUADRATIC_COEFF_COUNT];
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeEquGrpIfaceModel10ObjSetImpl_QUADRATIC)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetImpl_QUADRATIC");
BoardObjGrpIfaceModel10ObjSet   (vfeEquGrpIfaceModel10ObjSetValidate_QUADRATIC)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeEquGrpIfaceModel10ObjSetValidate_QUADRATIC");

// VFE_EQU interfaces.
VfeEquEvalNode      (vfeEquEvalNode_QUADRATIC);

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_EQU_QUADRATIC_H
