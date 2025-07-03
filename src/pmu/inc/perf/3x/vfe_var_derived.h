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
 * @file    vfe_var_derived.h
 * @brief   VFE_VAR_DERIVED class interface.
 */

#ifndef VFE_VAR_DERIVED_H
#define VFE_VAR_DERIVED_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_DERIVED VFE_VAR_DERIVED;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Constructor.
 * @memberof VFE_VAR_DERIVED
 * @public
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 */
#define vfeVarGrpIfaceModel10ObjSetImpl_DERIVED(pModel10, ppBoardObj, size, pDesc)         \
    vfeVarGrpIfaceModel10ObjSetImpl_SUPER((pModel10), (ppBoardObj), (size), (pDesc))

/*!
 * @copydoc VfeVarEval()
 * @memberof VFE_VAR_DERIVED
 * @public
 */
#define vfeVarEval_DERIVED(pContext, pVfeVar, pResult)                         \
    vfeVarEval_SUPER((pContext), (pVfeVar), (pResult))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief A variable type that derives its value from two other variables.
 *
 * @extends VFE_VAR
 */
struct VFE_VAR_DERIVED
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure.
     *
     * @protected
     */
    VFE_VAR super;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/* ------------------------ Include Derived Types --------------------------- */
#include "perf/3x/vfe_var_derived_product.h"
#include "perf/3x/vfe_var_derived_sum.h"

#endif // VFE_VAR_DERIVED_H
