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
 * @file    vfe_var.h
 * @brief   VFE_VAR class interface
 */

#ifndef VFE_VAR_H
#define VFE_VAR_H

#include "g_vfe_var.h"

#ifndef G_VFE_VAR_H
#define G_VFE_VAR_H

/*!
 * @interface VFE_VAR
 *
 * The VFE_VAR represents the independent variables provided as input into
 * VFE_EQU. These variables may be observable characteristics of the GPU or
 * board, or they may be values provided by the evaluator to be used as input
 * for an equation.
 *
 * SSG shall specify the POR for the variables by populating the Independent
 * Variables Tables found in the VBIOS. SSG shall also specify which equations
 * these variable will be used in.
 *
 * When referenced by an equation, each VFE_VAR shall evaluate to a IEEE-754
 * 32-bit floating point (binary32) value.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "pmu/pmuifperf.h"
#include "perf/3x/vfe_equ_type.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct VFE_VAR      VFE_VAR, VFE_VAR_BASE;

typedef struct VFE_CONTEXT  VFE_CONTEXT;

typedef struct VFE_VARS     VFE_VARS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @memberof VFE_VARS
 *
 * @public
 *
 * @brief Accessor to obtain the primary VFE_VARS.
 *
 * @param[in]  _pPerf  Pointer to PERF object containing the variables
 *
 * @return A pointer to the variables
 * @return NULL if the @em _pPerf does not have the variables
 */
#define PERF_GET_VFE_VARS(_pPerf)                                              \
    (PERF_GET_VFE(_pPerf)->pVars)

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 * @memberof VFE_VARS
 * @public

 * The type colwersion code expects the class ID to match the boardObjGrp
 * data location.
 */
#define BOARDOBJGRP_DATA_LOCATION_VFE_VAR                                      \
    (&(PERF_GET_VFE_VARS(&Perf)->super.super))

/*!
 * @memberof VFE_VARS
 *
 * @public
 *
 * @copydoc BOARDOBJGRP_IS_VALID()
 */
#define VFE_VAR_IS_VALID(_objIdx)                                              \
    (BOARDOBJGRP_IS_VALID(VFE_VAR, (_objIdx)))

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Evaluates a VFE_VAR.
 *
 * @param[in]   pContext    VFE_CONTEXT pointer
 * @param[in]   pVfeVar     VFE_VAR object pointer
 * @param[out]  pResult     Buffer to hold evaluation result
 *
 * @return  FLCN_OK upon successful evaluation
 * @return  Error propagated from inner functions
 */
#define VfeVarEval(fname) FLCN_STATUS (fname)(VFE_CONTEXT *pContext, VFE_VAR *pVfeVar, LwF32 *pResult)

/*!
 * @brief Evaluates a VFE_VAR associated with the specified index.
 *
 * @param[in]   pContext    VFE_CONTEXT pointer
 * @param[in]   vfeVarIdx   VFE_VAR object index
 * @param[out]  pResult     Buffer to hold evaluation result
 *
 * @return  FLCN_OK upon successful evaluation
 * @return  Error propagated from inner functions
 */
#define VfeVarEvalByIdx(fname) FLCN_STATUS (fname)(VFE_CONTEXT *pContext, LwU8 vfeVarIdx, LwF32 *pResult)

/*!
 * @brief Samples a VFE_VAR and notify the caller if its value has changed.
 *
 * @param[in]   vfeVarIdx   VFE_VAR object index
 * @param[out]  pBChanged   Buffer to hold information if Variable has changed
 *
 * @return  FLCN_OK upon successful sampling
 * @return  Error propagated from inner functions
 */
#define VfeVarSample(fname) FLCN_STATUS (fname)(VFE_VAR *pVfeVar, LwBool *pBChanged)

/*!
 * @brief Sample all sensed VFE Variables and return the mask of affected ones.
 *
 * @param[in]   pVfeVars    Pointer to the VFE_VARS group to sample
 * @param[out]  pBChanged   Buffer to hold information if any VAR has changed
 *
 * @return  FLCN_OK upon successful sampling
 * @return  Error propagated from inner functions
 */
#define VfeVarSampleAll(fname) FLCN_STATUS (fname)(VFE_VARS *pVfeVars, LwBool *pBChanged)

/*!
 * @brief Ilwalidates cache of all VARs and EQUs depending on this variable.
 *
 * @param[in]   pContext    VFE_CONTEXT pointer
 * @param[in]   pVfeVar     VFE_VAR object pointer
 *
 * @return  FLCN_OK upon successful ilwalidation
 * @return  Error propagated from inner functions
 */
#define VfeVarDependentsCacheIlw(fname) FLCN_STATUS (fname)(VFE_CONTEXT *pContext, VFE_VAR *pVfeVar)

/*!
 * @brief Caches all VFE VARs that can be cached.
 *
 * In order to evaluate (and cache) SINGLE non-SENSED VFE Variable clinet must
 * provide a respective value or this Variable will be skipped as well as all
 * Variables depending on it.
 *
 * Once cached VFE Variable can be used until its been ilwalidated.
 *
 * Caching assures that VFE EQUs can directly use cached values reducing stack
 * usage since VARs do not need to get evaluated on the top of EQU stack usage.
 *
 * @param[in]   pContext    VFE_CONTEXT pointer
 *
 * @return  FLCN_OK Upon successful caching
 * @return  Error propagated from inner functions
 */
#define VfeVarsCacheAll(fname) FLCN_STATUS (fname)(VFE_CONTEXT *pContext)

/*!
 * @brief Base class for all variable objects.
 *
 * Independent Variables are values, either observable characteristics of the
 * GPU/board or values provided by the evaluator, which may be used as input for
 * an equation. When setting the POR, the chip solutions team will specify in
 * the Independent Variables Table which independent variables they wish to use
 * in their equations and any configuration parameters associated with them.
 *
 * When referenced by an equation, each independent variable evaluates to a
 * IEEE-754 32-bit floating point (binary32) value.
 *
 * The are several classes of Independent Variables, which represent different
 * types of values.
 *
 * The output (output) of each variable is bound by the specified Output Range
 * Minimum (outputmin) and Output Range Maximum (outputmax), both of which are
 * IEEE-754 32-bit floating point (binary32) values.
 *
 * @code{.c}
 * output = max(min(output, outputmax), outputmin);
 * @endcode
 *
 * @extends BOARDOBJ
 */
struct VFE_VAR
{
    /*!
     * @brief Base class.
     *
     * Must be first member in the structure.
     *
     * @protected
     */
    BOARDOBJ                super;

    /*!
     * @brief Lower limit for the variable value (as IEEE-754 32-bit floating
     * point).
     *
     * @protected
     */
    LwF32                   outRangeMin;

    /*!
     * @brief Upper limit for the variable value (as IEEE-754 32-bit floating
     * point).
     *
     * @protected
     */
    LwF32                   outRangeMax;

    /*!
     * @brief Mask of all VFE_VAR not dependent on this VFE_VAR.
     *
     * @protected
     */
    BOARDOBJGRPMASK_E255    maskNotDependentVars;

    /*!
     * @brief Mask of all VFE_EQU not dependent on this VFE_VAR.
     *
     * @protected
     */
    VFE_EQU_BOARDOBJGRPMASK maskNotDependentEqus;
};

/*!
 * @brief Collection of all @ref VFE_VAR and related information.
 *
 * @extends BOARDOBJGRP_E255
 */
struct VFE_VARS
{
    /*!
     * @brief Base class.
     *
     * Must be first member in the structure.
     *
     * @protected
     */
    BOARDOBJGRP_E255 super;
};

/*!
 * @brief   VFE Context
 */
struct VFE_CONTEXT
{
    /*!
     * @brief Indicates if VFE VAR caching is in progress (@see
     * vfeVarsCacheAll()).
     *
     * Used to supress selected error (OBJECT_NOT_FOUND) breakpoints allowing
     * us to skip the evaluation of a SINGLE non-SENSED Variables and their
     * dependants that would normaly fail due to the lack of client's input.
     */
    LwBool                      bCachingInProgress;

    /*!
     * @brief Relwrsion control state for variable evaluation.
     */
    LwU8                        rcVfeVarEvalByIdx;

    /*!
     * @brief Relwrsion control state for equation evaluation.
     */
    LwVfeEquIdx                 rcVfeEquEvalList;

    /*!
     * @brief Number of client provided VFE variables.
     */
    LwU8                        clientVarValuesCnt;

    /*!
     * @brief Client provided VFE Variable values to be used as an overrides
     * when evaluating VFE Equations.
     */
    RM_PMU_PERF_VFE_VAR_VALUE  *pClientVarValues;

    /*!
     * @brief VFE Variable cache state.
     */
    BOARDOBJGRPMASK_E255        maskCacheValidVars;

    /*!
     * @brief VFE Variables cached values.
     */
    LwF32                      *pOutCacheVars;

    /*!
     * @brief VFE Equation cache state.
     */
    VFE_EQU_BOARDOBJGRPMASK     maskCacheValidEqus;

    /*!
     * @brief VFE Equation cached values.
     */
    LwF32                      *pOutCacheEqus;
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler       (vfeVarBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "vfeVarBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler       (vfeVarBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "vfeVarBoardObjGrpIfaceModel10GetStatus");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet           (vfeVarGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjIfaceModel10GetStatus               (vfeVarIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_perfVfeBoardObj", "vfeVarIfaceModel10GetStatus_SUPER");

// VFE_VAR interfaces.
            VfeVarEval                  (vfeVarEval_SUPER);
            VfeVarEvalByIdx             (vfeVarEvalByIdx);
mockable    VfeVarEvalByIdx             (vfeVarEvalByIdxFromCache);
            VfeVarSampleAll             (vfeVarSampleAll);
            VfeVarDependentsCacheIlw    (vfeVarDependentsCacheIlw);

// VFE_VARs interfaces.
VfeVarsCacheAll             (vfeVarsCacheAll);

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/3x/vfe_var_derived.h"
#include "perf/3x/vfe_var_single.h"

#endif // G_VFE_VAR_H
#endif // VFE_VAR_H
