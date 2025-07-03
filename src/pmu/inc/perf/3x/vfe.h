/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    vfe.h
 * @brief   VFE class interface.
 */

#ifndef VFE_H
#define VFE_H

#include "g_vfe.h"

#ifndef G_VFE_H
#define G_VFE_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/3x/vfe_equ.h"
#include "perf/3x/vfe_var.h"
#include "perf/3x/vfe_equ_monitor.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @brief Accessor to look-up VFE structure.
 * @memberof VFE
 * @public
 *
 * @param[in]  _pPerf  PERF object pointer
 *
 * @return pointer to the VFE object; NULL if the VFE object does not exist
 */
#define PERF_GET_VFE(_pPerf)        ((_pPerf)->pVfe)

/*!
 * @brief Accessor to look-up requested VFE Context structure.
 *
 * @memberof VFE
 * @public
 *
 * @param[in]  _pPerf  PERF object pointer
 * @param[in]  _ctxId  VFE context ID
 *
 * @return pointer to the VFE_CONTEXT object; NULL if the VFE_CONTEXT with the
 * given @a _ctxId does not exist
 */
#define VFE_CONTEXT_GET(_pPerf, _ctxId) \
    (&(PERF_GET_VFE(_pPerf)->context[_ctxId]))

/*!
 * @brief Basic list of VFE overlays for attachment.
 *
 * @memberof VFE
 * @public
 *
 * @param[in]  _pPerf  PERF object pointer
 * @param[in]  _ctxId  VFE context ID
 *
 * @note Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 * updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_VFE_BASIC_COMMON                                \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVfe)                             \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfVfe)                             \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libBoardObj)

/*!
 * @brief Full list of VFE overlays for attachment.
 *
 * @memberof VFE
 * @public
 *
 * @note Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 * updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_VFE_FULL_COMMON                                 \
    OSTASK_OVL_DESC_DEFINE_VFE_BASIC_COMMON                                    \
    OSTASK_OVL_DESC_DEFINE_LIB_LW_F32

/*!
 * @brief List of VFE overlays needed for BOARDOBJ interactions.
 *
 * @memberof VFE
 * @public
 *
 * @note Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM_LS after
 * updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_VFE_BOARD_OBJ                                   \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVfeBoardObj)                     \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfVfe)                             \
    OSTASK_OVL_DESC_DEFINE_LIB_LW_F32

/*!
 * @brief Selects the list of IMEM and DMEM overlays for VFE.
 *
 * @memberof VFE
 * @public
 *
 * @param[in]  _common  BASIC or FULL
 *
 * @return List of IMEM and DMEM overlays for VFE
 */
#define OSTASK_OVL_DESC_DEFINE_VFE(_common)                                    \
    OSTASK_OVL_DESC_DEFINE_VFE_##_common##_COMMON                              \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVfeEquMonitor)

/*!
 * @brief VFE_CONTEXT IDs.
 */
typedef enum
{
    VFE_CONTEXT_ID_TASK_PERF = 0,
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35))
    VFE_CONTEXT_ID_TASK_PERF_DAEMON,
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35)
    VFE_CONTEXT_ID__COUNT,
} VFE_CONTEXT_ID;

/*!
 * @brief Interface to ilwalidate VFE and all its dependent state.
 *
 * @note This is a helper macro wrapping @ref perfVfeLoad() for now.  Deploying
 * the macro so that can replace with a function in the future if necessary.
 *
 * @return FLCN_STATUS returned by @ref perfVfeLoad().
 */
#define vfeIlwalidate()                                                        \
    perfVfeLoad(LW_TRUE)

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * @brief Evaluates a VFE_VAR given the VFE index.
 *
 * @param[in]   vfeVarIdx   Index to VFE_VAR.
 * @param[out]  pResult     Buffer to hold evaluation result.
 *
 * @return FLCN_OK upon successful evaluation
 * @return error propagated from inner functions otherwise
 */
#define VfeVarEvaluate(fname) FLCN_STATUS (fname)(LwU8 vfeVarIdx, LwF32 *pResult)

/*!
 * @brief Evaluates a VFE_EQU using an array of VFE_VAR values.
 *
 * @note If caller does not need to provide values for VFE_VAR this interface
 * should be called as vfeEquEvaluate(idx, NULL, 0, &result).
 *
 * @param[in]   vfeEquIdx   Equation list head node index.
 * @param[in]   pValues     Array of input VFE_VAR values.
 * @param[in]   valCount    Size of @ref pValues array.
 * @param[in]   outputType  Expected type of the result as
 *                          LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_<xyz>.
 * @param[out]  pResult     Buffer to hold evaluation result.
 *
 * @return FLCN_OK upon successful evaluation
 * @return error propagated from inner functions otherwise
 */
#define VfeEquEvaluate(fname) FLCN_STATUS (fname)(LwVfeEquIdx vfeEquIdx, RM_PMU_PERF_VFE_VAR_VALUE *pValues, LwU8 valCount, LwU8 outputType, RM_PMU_PERF_VFE_EQU_RESULT *pResult)

/*!
 * Structure holding all VFE related data.
 */
typedef struct
{
    /*!
     * @brief Board Object Group of primary VFE_VAR objects.
     */
    VFE_VARS   *pVars;

    /*!
     * @brief Board Object Group of primary VFE_EQU objects.
     */
    VFE_EQUS   *pEqus;

    /*!
     * @copydoc VFE_CONTEXT
     */
    VFE_CONTEXT context[VFE_CONTEXT_ID__COUNT];

    /*!
     * @brief Polling period [ms] at which the VFE logic will evaluate for
     * dynamic observed changes in the independent variables (e.g. temperature).
     */
    LwU8        pollingPeriodms;

    /*!
     * @brief Flag indicating the VFE_VAR or VFE_EQU have been updated.
     *
     * Set after processing GRP_SET command for either VFE_VAR or VFE_EQU.
     * VFE timer callback will force reevaluation and clear this flag.
     */
    LwBool      bObjectsUpdated;

    /*!
     * @brief Structure holding RM requests for periodic VFE_EQU evaluation.
     */
    VFE_EQU_MONITOR *pRmEquMonitor;
} VFE;

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
void            perfVfePostInit(void)
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfVfePostInit");
VFE_CONTEXT *   vfeContextGet(void);
FLCN_STATUS     perfVfeLoad(LwBool bLoad)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "perfVfeLoad");
FLCN_STATUS     vfeBoardObjIlwalidate(void)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "vfeBoardObjIlwalidate");

// VFE interfaces
VfeVarEvaluate          (vfeVarEvaluate);
mockable VfeEquEvaluate          (vfeEquEvaluate);

#endif // G_VFE_H
#endif // VFE_H
