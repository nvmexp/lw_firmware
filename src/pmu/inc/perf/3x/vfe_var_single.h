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
 * @file    vfe_var_single.h
 * @brief   VFE_VAR_SINGLE class interface.
 */

#ifndef VFE_VAR_SINGLE_H
#define VFE_VAR_SINGLE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_var.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR_SINGLE VFE_VAR_SINGLE;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Link/unlink client provided VFE_VAR values with current context.
 *
 * @param[in]   pContext    VFE Context pointer.
 * @param[in]   pValues     Array of input VFE Variable values.
 * @param[in]   valCount    Size of @ref pValues array.
 * @param[in]   bLink       Operation to perform (link vs. unlink)
 *
 * @return  FLCN_OK Upon successful operation.
 * @return  other   Error propagated from inner functions.
 */
#define VfeVarClientValuesLink(fname) FLCN_STATUS (fname)(VFE_CONTEXT *pContext, RM_PMU_PERF_VFE_VAR_VALUE *pValues, LwU8 valCount, LwBool bLink)

/*!
 * @brief Look-up client provided VFE_VAR value (if provided).
 *
 * @param[in]   pVfeVar     VFE_VAR_SINGLE object pointer.
 * @param[in]   pContext    VFE Context pointer.
 * @param[in]   pValue      Buffer holding client provided VFE Variable value.
 *                          If NULL interface checks client input existence.
 *
 * @return  FLCN_OK Upon successful look-up.
 * @return  other   Error propagated from inner functions.
 */
#define VfeVarSingleClientValueGet(fname) FLCN_STATUS (fname)(VFE_VAR_SINGLE *pVfeVar, VFE_CONTEXT *pContext,  LwF32 *pValue)

/*!
 * @brief Client input match for VFE_VAR Single type.
 *
 * @param[in]   pVfeVar            VFE_VAR_SINGLE object pointer.
 * @param[in]   pClientVarValue    RM_PMU_PERF_VFE_VAR_VALUE object pointer.
 *
 * @return  FLCN_OK Upon successful match.
 * @return  FLCN_ERR_OBJECT_NOT_FOUND upon failure to match any of the client specific fields.
 */
#define VfeVarSingleClientInputMatch(fname) FLCN_STATUS (fname)(VFE_VAR_SINGLE *pVfeVar, RM_PMU_PERF_VFE_VAR_VALUE *pClientVarValue)

/*!
 * @brief Fetches the var value from input client value.
 *
 * @param[in]   pVfeVar            VFE_VAR_SINGLE object pointer.
 * @param[in]   pClientVarValue    RM_PMU_PERF_VFE_VAR_VALUE object pointer.
 * @param[out]  pValue             LwU32 var value pointer.
 *
 * @return  FLCN_OK Upon successful match.
 * @return  FLCN_ERR_OBJECT_NOT_FOUND upon failure to match any of the client specific fields.
 */
#define VfeVarInputClientValueGet(fname) FLCN_STATUS (fname)(VFE_VAR_SINGLE *pVfeVar, RM_PMU_PERF_VFE_VAR_VALUE *pClientVarValue, LwF32 *pValue)

/*!
 * @brief VFE_VAR class providing attributes of Single VFE Variable.
 *
 * @extends VFE_VAR
 */
struct VFE_VAR_SINGLE
{
    /*!
     * @brief Base class.
     *
     * Must be first element of the structure!
     *
     * @protected
     */
    VFE_VAR super;

    /*!
     * @brief @ref LW2080_CTRL_PERF_VFE_VAR_SINGLE_OVERRIDE_TYPE_ENUM ID of a
     * VFE variable override type (by default set to _NONE).
     * @protected
     */
    LwU8    overrideType;

    /*!
     * @brief Value of a VFE variable override (as IEEE-754 32-bit floating
     * point).
     * @protected
     */
    LwF32   overrideValue;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet   (vfeVarGrpIfaceModel10ObjSetImpl_SINGLE)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_SINGLE");

/*!
 * @memberof VFE_VAR_SINGLE
 *
 * @public
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
#define vfeVarIfaceModel10GetStatus_SINGLE(pModel10, pBuf)                                \
    vfeVarIfaceModel10GetStatus_SUPER((pModel10), (pBuf))

// VFE_VAR interfaces.
VfeVarEval  (vfeVarEval_SINGLE);
VfeVarEval  (vfeVarEval_SINGLE_CALLER);

// VFE_VAR_SINGLE interfaces.
VfeVarClientValuesLink        (vfeVarClientValuesLink);
VfeVarSingleClientValueGet    (vfeVarSingleClientValueGet_SINGLE);
VfeVarSingleClientInputMatch  (vfeVarSingleClientInputMatch_SINGLE);

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/3x/vfe_var_single_frequency.h"
#include "perf/3x/vfe_var_single_sensed.h"
#include "perf/3x/vfe_var_single_voltage.h"
#include "perf/3x/vfe_var_single_caller_specified.h"
#include "perf/3x/vfe_var_single_globally_specified.h"

#endif // VFE_VAR_SINGLE_H
