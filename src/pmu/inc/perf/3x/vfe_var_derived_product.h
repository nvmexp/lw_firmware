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
 * @file    vfe_equ_derived_product.h
 * @brief   VFE_EQU_DERIVED_PRODUCT class interface.
 */

#ifndef VFE_VAR_DERIVED_PRODUCT_H
#define VFE_VAR_DERIVED_PRODUCT_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var_derived.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_DERIVED_PRODUCT VFE_VAR_DERIVED_PRODUCT;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief   Colwenience macro for getting the vtable for the
 *          VFE_VAR_DERIVED_PRODUCT class type.
 *
 * @return  Pointer to the location of the VFE_VAR_DERIVED_PRODUCT class vtable.
 * @return  NULL if the vtable is not enabled in the build.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_VFE_VAR)
#define PERF_VFE_VAR_DERIVED_PRODUCT_VTABLE() &VfeVarDerivedProductVirtualTable
extern BOARDOBJ_VIRTUAL_TABLE VfeVarDerivedProductVirtualTable;
#else
#define PERF_VFE_VAR_DERIVED_PRODUCT_VTABLE() NULL
#endif

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief A variable type that derives its value by multiplying two other
 * variables.
 *
 * The Product class of Independent Variables represents the multiplicative
 * product of two other Independent Variables. The two referenced Independent
 * Variables can be of any type - e.g. Single Value or Product. This means that
 * Product Independent Variables can be chained together provide a product (or
 * more complex expression) of more than two Independent Variables, if so
 * desired.
 *
 * Parameters:
 * @li First Independent Variable Index
 * @li Second Independent Variable Index

 * @extends VFE_VAR_DERIVED
 */
struct VFE_VAR_DERIVED_PRODUCT
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
     * @brief Index of the first variable used by product().
     * @protected
     */
    LwU8            varIdx0;

    /*!
     * @brief Index of the second variable used by product().
     * @protected
     */
    LwU8            varIdx1;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_PRODUCT)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_DERIVED_PRODUCT");

// VFE_VAR interfaces.
VfeVarEval          (vfeVarEval_DERIVED_PRODUCT);

/* ------------------------ Include Derived Types --------------------------- */

#endif // VFE_VAR_DERIVED_PRODUCT_H
