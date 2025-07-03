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
 * @file    vfe_equ_derived_sum.h
 * @brief   VFE_EQU_DERIVED_SUM class interface.
 */

#ifndef VFE_VAR_DERIVED_SUM_H
#define VFE_VAR_DERIVED_SUM_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_derived.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_DERIVED_SUM VFE_VAR_DERIVED_SUM;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_VAR_DERIVED_SUM class type.
 *
 * @return  Pointer to the location of the VFE_VAR_DERIVED_SUM class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR)
#define PERF_VFE_VAR_DERIVED_SUM_VTABLE() &VfeVarDerivedSumVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeVarDerivedSumVirtualTable;
#else
#define PERF_VFE_VAR_DERIVED_SUM_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief A variable type that derives its value by adding two other variables.
 *
 * The sum class of independent variables represents the addition of two other
 * independent variables. The two referenced independent variables can be of any
 * type - i.e. single value or derived. This means that the sum independent
 * variables can be chained together provide a sum (or more complex expression)
 * of more than two independent variables, if so desired.
 *
 * Parameters:
 * @li First Independent Variable Index
 * @li Second Independent Variable Index
 *
 * @extends VFE_VAR_DERIVED
 */
struct VFE_VAR_DERIVED_SUM
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    VFE_VAR_DERIVED super;

    /*!
     * @brief Index of the first variable used by sum().
     * @protected
     */
    LwU8            varIdx0;

    /*!
     * @brief Index of the second variable used by sum().
     * @protected
     */
    LwU8            varIdx1;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_SUM)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_SUM");

// VFE_VAR interfaces.
VfeVarEval          (vfeVarEval_DERIVED_SUM);

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_DERIVES_SUM_H
