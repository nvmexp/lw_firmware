/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_cf_controller.h
 * @brief   Common PERF_CF Controller related defines.
 *
 * @copydoc perf_cf.h
 */

#ifndef PERF_CF_CONTROLLER_H
#define PERF_CF_CONTROLLER_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "boardobj/boardobjgrp.h"
#include "perf/cf/perf_cf_pm_sensor.h"
#include "perf/cf/perf_cf_topology.h"
#include "perf/perf_limit_client.h"
#include "pmgr/pwrchannel_status.h"
#include "pmgr/pwrpolicies_status.h"
#include "perf/cf/perf_cf_controller_status.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_CF_CONTROLLER PERF_CF_CONTROLLER, PERF_CF_CONTROLLER_BASE;

typedef struct PERF_CF_CONTROLLERS PERF_CF_CONTROLLERS;

typedef struct PERF_CF_CONTROLLER_MULTIPLIER_DATA PERF_CF_CONTROLLER_MULTIPLIER_DATA;


/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Colwenience macro to look-up PERF_CF_CONTROLLERS.
 */
#define PERF_CF_GET_CONTROLLERS()                                             \
    (PERF_CF_GET_OBJ()->pControllers)

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PERF_CF_CONTROLLER                          \
    (&(PERF_CF_GET_CONTROLLERS()->super.super))

/*!
 * @brief       Accessor for a PERF_CF_CONTROLLER BOARDOBJ by BOARDOBJGRP index.
 *
 * @param[in]   _idx BOARDOBJGRP index for a PERF_CF_CONTROLLER BOARDOBJ
 *
 * @return      Pointer to a PERF_CF_CONTROLLER object at the provided BOARDOBJGRP index.
 *
 * @memberof    PERF_CF_CONTROLLER
 *
 * @public
 */
#define PERF_CF_CONTROLLER_GET(_idx)                                          \
    (BOARDOBJGRP_OBJ_GET(PERF_CF_CONTROLLER, (_idx)))

/*!
 * @brief Accessor macro for PERF_CF_CONTROLLERS::maskActive
 */
#define perfCfControllersMaskActiveGet()                                      \
    (&(PERF_CF_GET_CONTROLLERS())->maskActive)

/*!
 * Invalid PERF_CF_CONTROLLER limit index.
 */
#define PERF_CF_CONTROLLERS_LIMIT_IDX_ILWALID 0xff

/*!
 * Colwenience macro to iterate over all multiplier data structures.
 *
 * @param[in]   _pC     PERF_CF_CONTROLLERS object pointer.
 * @param[out]  _pMD    Iterating pointer (must be L-value).
 *
 * @note    Do not alter list (add/remove elements)  while iterating.
 */
#define FOR_EACH_MULTIPLIER(_pC,_pMD) \
    for (_pMD = (_pC)->pMultDataHead; _pMD != NULL; _pMD = _pMD->pNext)

/*!
 * @brief   List of an overlay descriptors required by a controllers code.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
// Perf limit related overlays
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_PERF_LIMIT                 \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfLimitClient)             \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfLimitClient)
#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_PERF_LIMIT                 \
        OSTASK_OVL_DESC_ILWALID
#endif // PMU_PERF_PERF_LIMIT

/*!
 * @brief List of overlay descriptors required for
 * PMU_PERF_CF_CONTROLLER_XBAR_FMAX_AT_VOLT feature within @ref
 * s_perfCfControllersEvaluate().
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_XBAR_FMAX_AT_VOLT))
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_EVALUATE_XBAR_FMAX_AT_VOLT  \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfLimitVf)
#else
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_EVALUATE_XBAR_FMAX_AT_VOLT  \
        OSTASK_OVL_DESC_ILWALID
#endif

// Overlays attached throughout callback
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_COMMON                     \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_BASE                                   \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfCfController)               \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)                 \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)

// Overlays attached only during evaluate
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_EVALUATE                   \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_MODEL                                  \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libSemaphoreRW)                 \
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVf)                         \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)                           \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLER_DLPPC_1X_EVALUATE           \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_EVALUATE_XBAR_FMAX_AT_VOLT

// Overlays attached only during filter (sample - common to perf and pwr)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_COMMON              \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_BASE

// Overlays attached only during filter (sample Perf)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_PERF                \
        OSTASK_OVL_DESC_DEFINE_PERF_CF_TOPOLOGY

// Overlays attached only during filter (while sampling PWR_POLICIES_STATUS)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_PWR_POLICY          \
        OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_STATUS_GET
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_PWR_POLICY           \
        OVL_INDEX_ILWALID
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)

// Overlays attached only during filter (sample Pwr)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_PWR                 \
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, i2cDevice)                      \
        OSTASK_OVL_DESC_DEFINE_PWR_CHANNELS_STATUS_GET
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
#define OSTASK_OVL_DESC_DEFINE_PERF_CF_CONTROLLERS_FILTER_PWR                 \
        OVL_INDEX_ILWALID
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)

/*!
 * Structure holding all data related to a single sampling multiplier.
 */
struct PERF_CF_CONTROLLER_MULTIPLIER_DATA
{
    /*!
     * Multiplier data structures are linked into a singly linked list so that
     * they can be iterated over using @ref FOR_EACH_MULTIPLIER() macro.
     */
    PERF_CF_CONTROLLER_MULTIPLIER_DATA    *pNext;
    /*!
     * Topologys status structure holding Perf input data to the controller(s).
     */
    PERF_CF_TOPOLOGYS_STATUS               topologysStatus;
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)
    /*!
     * Policy status structure holding @ref PWR_POLICY input data to the
     * controller(s).
     */
    PWR_POLICIES_STATUS                    *pPwrPoliciesStatus;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
    /*!
     * Channel status structure holding Pwr input data to the controller(s).
     */
    PWR_CHANNELS_STATUS                   *pPwrChannelsStatus;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
    /*!
     * PM sensors status structure holding PM sensor input data to the controller(s).
     */
    PERF_CF_PM_SENSORS_STATUS             *pPmSensorsStatus;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
    /*!
     * Mask of the Controllers that use same @ref samplingMultiplier.
     */
    BOARDOBJGRPMASK_E32                    maskControllers;
    /*!
     * Time (is OS ticks) when this multiplier is due to expire.
     */
    LwU32                                  expirationTimeTicks;
    /*!
     * Sampling period (is OS ticks) of this multiplier.
     */
    LwU32                                  samplingPeriodTicks;
    /*!
     * Sampling multiplier identifying this structure.
     */
    LwU8                                   samplingMultiplier;
};

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @brief   Run a PERF_CF_CONTROLLER.
 *
 * @param[in]   pController PERF_CF_CONTROLLER object pointer.
 *
 * @return  FLCN_OK                     PERF_CF_CONTROLLER run() successfully exelwted.
 * @return  FLCN_ERR_STATE_RESET_NEEDED Need to reset controller states and skip current cycle.
 * @return  other                       Propagates return values from various calls.
 */
#define PerfCfControllerExelwte(fname) FLCN_STATUS (fname)(PERF_CF_CONTROLLER *pController)

/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @brief   Loads a PERF_CF_CONTROLLER.
 *
 * @param[in]   pController PERF_CF_CONTROLLER object pointer
 *
 * @return  FLCN_OK     PERF_CF_CONTROLLER loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfControllerLoad(fname) FLCN_STATUS (fname)(PERF_CF_CONTROLLER *pController)

/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @brief   Do post-init for PERF_CF_CONTROLLERS group.
 *
 * @param[in]   ppControllers   Pointer to PERF_CF_CONTROLLERS object pointer.
 *
 * @return  FLCN_OK                 PERF_CF_CONTROLLERS post-init successful
 * @return  FLCN_ERR_NOT_SUPPORTED  No PERF_CF_CONTROLLERS version is enabled
 * @return  Other                   Propagates return values from various calls
 */
#define PerfCfControllersPostInit(fname) FLCN_STATUS (fname)(PERF_CF_CONTROLLERS **ppControllers)

/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @note    Common construction code for @ref PERF_CF_CONTROLLERS class.
 */
#define PerfCfControllersConstruct(fname) FLCN_STATUS (fname)(PERF_CF_CONTROLLERS *pControllers, LwBool bFirstConstruct)

/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @brief   Load all PERF_CF_CONTROLLERs and start timer callback.
 *
 * @param[in]   pControllers    PERF_CF_CONTROLLERS object pointer.
 * @param[in]   bLoad           LW_TRUE on load(), LW_FALSE on unload().
 *
 * @return  FLCN_OK     All PERF_CF_CONTROLLERs loaded successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfControllersLoad(fname) FLCN_STATUS (fname)(PERF_CF_CONTROLLERS *pControllers, LwBool bLoad)

/*!
 * @interface   PERF_CF
 *
 * @brief   Retrieve status for the requested controller objects.
 *
 * @param[in/out]   pStatus     Pointer to the buffer to hold the status.
 *
 * @return  FLCN_OK     On successfull retrieval
 * @return  other       Propagates return values from various calls
 */
#define PerfCfControllersStatusGet(fname) FLCN_STATUS (fname)(PERF_CF_CONTROLLERS *pControllers, PERF_CF_CONTROLLERS_STATUS *pStatus)

/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @brief   Set multiplier data for controller.
 *
 * @param[in]   pController PERF_CF_CONTROLLER object pointer.
 * @param[in]   pMultData   PERF_CF_CONTROLLER_MULTIPLIER_DATA object pointer.
 *
 * @return  FLCN_OK     Multiplier data set successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfControllerSetMultData(fname) FLCN_STATUS (fname)(PERF_CF_CONTROLLER *pController, PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData)

/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @brief   Apply new mask of active PERF_CF_CONTROLLERs.
 *
 * @param[in]   pControllers    PERF_CF_CONTROLLERS object pointer.
 * @param[in]   pMask           Mask of new active controllers.
 *
 * @return  FLCN_OK     Mask successfully applied
 * @return  other       Propagates return values from various calls
 */
#define PerfCfControllersMaskActiveSet(fname) FLCN_STATUS (fname)(PERF_CF_CONTROLLERS *pControllers, BOARDOBJGRPMASK_E32 *pMask)

/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @brief   Return the index of PERF_CF_CONTROLLER's min limit with matching clock domain index.
 *
 * @param[in]   pControllers    PERF_CF_CONTROLLERS object pointer.
 * @param[in]   clkDomIdx       Clock domain index to find.
 *
 * @return  PERF_CF_CONTROLLERS_LIMIT_IDX_ILWALID   PERF_CF_CONTROLLER limit not found.
 * @return  other                                   Index of PERF_CF_CONTROLLER limit found.
 */
#define PerfCfControllersGetMinLimitIdxFromClkDomIdx(fname) LwBoardObjIdx (fname)(PERF_CF_CONTROLLERS *pControllers, LwBoardObjIdx clkDomIdx)

/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @brief   Return the index of PERF_CF_CONTROLLER's max limit with matching clock domain index.
 *
 * @param[in]   pControllers    PERF_CF_CONTROLLERS object pointer.
 * @param[in]   clkDomIdx       Clock domain index to find.
 *
 * @return  PERF_CF_CONTROLLERS_LIMIT_IDX_ILWALID   PERF_CF_CONTROLLER limit not found.
 * @return  other                                   Index of PERF_CF_CONTROLLER limit found.
 */
#define PerfCfControllersGetMaxLimitIdxFromClkDomIdx(fname) LwBoardObjIdx (fname)(PERF_CF_CONTROLLERS *pControllers, LwBoardObjIdx clkDomIdx)

/*!
 * @interface   PERF_CF_CONTROLLER
 *
 * @brief   Update PERF_CF_CONTROLLER's OPTP performance ratio.
 *
 * @param[in]   pControllers    PERF_CF_CONTROLLERS object pointer.
 * @param[in]   perfRatio       New OPTP performance ratio.
 *
 * @return  FLCN_OK     OPTP performance ratio set successfully
 * @return  other       Propagates return values from various calls
 */
#define PerfCfControllersOptpUpdate(fname) FLCN_STATUS (fname)(PERF_CF_CONTROLLERS *pControllers, LwUFXP8_24 perfRatio)

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Virtual BOARDOBJ child providing attributes common to all PERF_CF Controllers.
 */
struct PERF_CF_CONTROLLER
{
    /*!
     * BOARDOBJ super-class.
     * Must be first element of the structure!
     */
    BOARDOBJ                            super;
    /*!
     * Freqeuncy limit requests in kHz. 0 == no request. Execute() stores the values here, to be arbitrated.
     */
    LwU32                               limitFreqkHz[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS];
    /*!
     * Multiplier data structure shared by this Controller.
     */
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData;
    /*!
     * Counting the number of execute iterations.
     */
    LwU32                               iteration;
    /*!
     * This controller's sampling period is multiplier * baseSamplingPeriodms.
     */
    LwU8                                samplingMultiplier;
    /*!
     * Index of topology input to the controller. Can be
     * LW2080_CTRL_PERF_PERF_CF_CONTROLLER_INFO_TOPOLOGY_IDX_NONE.
     */
    LwU8                                topologyIdx;
    /*!
     * Reset controller state on next cycle.
     */
    LwBool                              bReset;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    /*!
     * Inflection points disablement request from this controller.
     */
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST inflectionPointsDisableRequest;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE)
};

/*!
 * Collection of all PERF_CF Controllers and related information.
 */
struct PERF_CF_CONTROLLERS
{
    /*!
     * BOARDOBJGRP_E32 super class. This should always be the first member!
     */
    BOARDOBJGRP_E32                                     super;
    /*!
     * Current arbitrated limits across all Controllers.
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT   limits[LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_LIMIT_MAX_LIMITS];
    /*!
     * Each PERF_CF controller run at a multiple of this base sampling period [us].
     */
    LwU32                                               baseSamplingPeriodus;
    /*!
     * Mask of the lwrrently active Controllers.
     */
    BOARDOBJGRPMASK_E32                                 maskActive;
    /*!
     * Head pointer to the singly linked list of multiplier data structures.
     * Used by @ref FOR_EACH_MULTIPLIER() iterating macro.
     */
    PERF_CF_CONTROLLER_MULTIPLIER_DATA                 *pMultDataHead;
    /*!
     * Pointer to the PERF_LIMITS_CLIENT structure. This is used to set and
     * clear PERF_LIMITS when using the PMU arbiter.
     */
    PERF_LIMITS_CLIENT                                 *pLimitsClient;
    /*!
     * Latest performance ratio request from OPTP. Can be
     * LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_STATUS_OPTP_PERF_RATIO_INACTIVE.
     */
    LwUFXP8_24                                          optpPerfRatio;
    /*!
     * Counting perfLimitsClientArbitrateAndApply() failures.
     */
    LwU32                                               limitsArbErrCount;
    /*!
     * Tracking the last perfLimitsClientArbitrateAndApply() FLCN_STATUS return code != OK.
     */
    LwU8                                                limitsArbErrLast;

    /*!
     * Number of VM slots that are active in vGPU's scheduler.
     * In PVMRL fair share mode, this is the number of VMs lwrrently running.
     * In PVMRL fixed share mode, this is the maximum number of VMs that could be run.
     */
    LwU8                                                maxVGpuVMCount;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING)
    /*!
     * Profiling data for the last exelwtion fo the controllers.
     */
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING      profiling;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING)
};

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// BOARDOBJGRP interfaces.
BoardObjGrpIfaceModel10SetHeader                    (perfCfControllerIfaceModel10SetHeader_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10SetHeader_SUPER");
BoardObjGrpIfaceModel10GetStatusHeader                    (perfCfControllerIfaceModel10GetStatusHeader_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatusHeader_SUPER");

// Board Object interfaces.
BoardObjGrpIfaceModel10ObjSet                             (perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerGrpIfaceModel10ObjSetImpl_SUPER");

BoardObjIfaceModel10GetStatus   (perfCfControllerIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerIfaceModel10GetStatus_SUPER");

// PERF_CF_CONTROLLER interfaces.
PerfCfControllersPostInit                     (perfCfControllersPostInit_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfInit", "perfCfControllersPostInit_SUPER");
PerfCfControllersConstruct                    (perfCfControllerGrpConstruct_SUPER)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllerGrpConstruct_SUPER");
PerfCfControllersLoad                         (perfCfControllersLoad)
    GCC_ATTRIB_SECTION("imem_perfCfLoad", "perfCfControllersLoad");
PerfCfControllersStatusGet                    (perfCfControllersStatusGet)
    GCC_ATTRIB_SECTION("imem_perfCfBoardObj", "perfCfControllersStatusGet");

PerfCfControllerExelwte                         (perfCfControllerExelwte_SUPER);
PerfCfControllersMaskActiveSet                  (perfCfControllersMaskActiveSet);
PerfCfControllersGetMinLimitIdxFromClkDomIdx    (perfCfControllersGetMinLimitIdxFromClkDomIdx);
PerfCfControllersGetMaxLimitIdxFromClkDomIdx    (perfCfControllersGetMaxLimitIdxFromClkDomIdx);
PerfCfControllersOptpUpdate                     (perfCfControllersOptpUpdate);

FLCN_STATUS                               perfCfControllersPostInit(void)
    GCC_ATTRIB_SECTION("imem_perfCfInit", "perfCfControllersPostInit");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Retrieves @ref PERF_CF_CONTROLLERS::profiling, if enabled.
 *
 * @param[in]   pControllers    Controllers from which to retrieve
 * @param[out]  ppProfiling     Pointer into which to assign
 *                              profiling structure or
 *                              @ref NULL if not enabled.
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pControllers or ppProfiling was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllersProfilingGet
(
    PERF_CF_CONTROLLERS *pControllers,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING **ppProfiling
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pControllers != NULL) && (ppProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersProfilingGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING)
    *ppProfiling = &pControllers->profiling;
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING)
    *ppProfiling = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING)

perfCfControllersProfilingGet_exit:
    return status;
}

/*!
 * @brief   Retrieves the next "free" instance of an
 *          @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE
 *          in an @ref LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING structure
 *
 * @param[in]   pProfiling          Pointer to profiling structure from which to
 *                                  retrieve
 * @param[out]  ppSampleProfiling   Pointer into which to assign profiling
 *                                  structure or @ref NULL if not enabled or no
 *                                  free mult data structure available
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllersProfilingMultDataSampleGetNext
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE **ppSampleProfiling
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (ppSampleProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersProfilingMultDataSampleGetNext_exit);

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING))
    {
        *ppSampleProfiling = NULL;
        goto perfCfControllersProfilingMultDataSampleGetNext_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersProfilingMultDataSampleGetNext_exit);

    //
    // Increment the number of mult datas profiled. Do this regardless of
    // whether we're beyond the max number, so that we can know if we drop any.
    //
    pProfiling->numMultDataSamplesProfiled++;

    //
    // If we've run out of free profiling structures, then return a NULL
    // pointer. Otherwise, return the next unused structure.
    //
    if (pProfiling->numMultDataSamplesProfiled >
            LW_ARRAY_ELEMENTS(pProfiling->multDataSampleProfiling))
    {
        *ppSampleProfiling = NULL;
    }
    else
    {
        *ppSampleProfiling = &pProfiling->multDataSampleProfiling[
            pProfiling->numMultDataSamplesProfiled - 1U];
    }

perfCfControllersProfilingMultDataSampleGetNext_exit:
    return status;
}

/*!
 * @brief   Resets profiling structure
 *
 * @param[in]   pProfiling  Pointer to profiling structure to reset
 *
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void
perfCfControllersProfileReset
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling
)
{
    FLCN_STATUS status;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING))
    {
        goto perfCfControllersProfileReset_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersProfileReset_exit);

    (void)memset(pProfiling, 0, sizeof(*pProfiling));

perfCfControllersProfileReset_exit:
    lwosNOP();
}

/*!
 * @brief   Begins the profiling for a given region of code in the @ref
 *          PERF_CF_CONTROLLERS
 *
 * @details This works by subtracting the current time from zero, giving us
 *          NEGATIVE(lwrrentTime). The "end" function can then add the
 *          then-current time to the value to get the elapsed time, i.e.,
 *          endTime - starTime == (-startTime) + endTime.
 *
 * @param[out]  pProfiling  Pointer to profiling structure in
 *                          which to start profiling.
 * @param[in]   region      Region to start profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void 
perfCfControllersProfileRegionBegin
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING))
    {
        goto perfCfControllersProfileRegionBegin_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL) &&
        (region < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersProfileRegionBegin_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    lw64Sub_MOD(&timensResult, &(LwU64){0U}, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfControllersProfileRegionBegin_exit:
    lwosNOP();
}

/*!
 * @brief   Ends the profiling for a given region of code previously begun with
 *          @ref perfCfControllersProfileRegionBegin, storing the elapsed time.
 *
 * @param[out]  pProfiling  Pointer to profiling structure in
 *                          which to end profiling.
 * @param[in]   region      Region for which to end profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void 
perfCfControllersProfileRegionEnd
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING))
    {
        goto perfCfControllersProfileRegionEnd_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pProfiling != NULL) &&
        (region < LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersProfileRegionEnd_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    LwU64_ALIGN32_UNPACK(
        &timensResult, &pProfiling->profiledTimesns[region]);
    lw64Add_MOD(&timensResult, &timensResult, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfControllersProfileRegionEnd_exit:
    lwosNOP();
}

/*!
 * @brief   Sets the mask of controllers for a given mult data profiling
 *          structure.
 *
 * @param[out]  pProfiling  Pointer to profiling structure in
 *                          which to set mask.
 * @param[in]   pMask       Mask to set
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void 
perfCfControllersProfileMultDataSampleMaskSet
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE *pProfiling,
    BOARDOBJGRPMASK_E32 *pMask
)
{
    FLCN_STATUS status;

    //
    // Exit early either if the feature is not enabled or the pointer is NULL.
    // Allowing the pointer to be NULL lets us handle cases where we run out of
    // available mult data profiling structures.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING) ||
        (pProfiling == NULL))
    {
        goto perfCfControllersProfileMultDataSampleMaskSet_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pMask != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersProfileMultDataSampleMaskSet_exit);

    PMU_ASSERT_OK_OR_GOTO(status,
        boardObjGrpMaskExport_E32(pMask, &pProfiling->maskControllers),
        perfCfControllersProfileMultDataSampleMaskSet_exit);

perfCfControllersProfileMultDataSampleMaskSet_exit:
    lwosNOP();
}

/*!
 * @brief   Begins the profiling for a given region of code in the
 *          @ref PERF_CF_CONTROLLERS mult data sampling code.
 *
 * @details This works by subtracting the current time from zero, giving us
 *          NEGATIVE(lwrrentTime). The "end" function can then add the
 *          then-current time to the value to get the elapsed time, i.e.,
 *          endTime - starTime == (-startTime) + endTime.
 *
 * @param[out]  pProfiling  Pointer to profiling structure in
 *                          which to start profiling.
 * @param[in]   region      Region to start profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void 
perfCfControllersProfileMultDataRegionBegin
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    //
    // Exit early either if the feature is not enabled or the pointer is NULL.
    // Allowing the pointer to be NULL lets us handle cases where we run out of
    // available mult data profiling structures.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING) ||
        (pProfiling == NULL))
    {
        goto perfCfControllersProfileMultDataRegionBegin_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (region <
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersProfileMultDataRegionBegin_exit);

    osPTimerTimeNsLwrrentGet(&timensLwrr);

    lw64Sub_MOD(&timensResult, &(LwU64){0U}, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfControllersProfileMultDataRegionBegin_exit:
    lwosNOP();
}

/*!
 * @brief   Ends the profiling for a given region of code previously begun with
 *          @ref perfCfControllersProfileMultDataRegionBegin, storing the elapsed time.
 *
 * @param[out]  pProfiling  Pointer to profiling structure in
 *                          which to end profiling.
 * @param[in]   region      Region for which to end profiling.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline void 
perfCfControllersProfileMultDataRegionEnd
(
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE *pProfiling,
    LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION_ENUM region
)
{
    FLCN_STATUS status;
    FLCN_TIMESTAMP timensLwrr;
    LwU64 timensResult;

    //
    // Exit early either if the feature is not enabled or the pointer is NULL.
    // Allowing the pointer to be NULL lets us handle cases where we run out of
    // available mult data profiling structures.
    //
    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLERS_PROFILING) ||
        (pProfiling == NULL))
    {
        goto perfCfControllersProfileMultDataRegionEnd_exit;
    }

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (region <
            LW2080_CTRL_PERF_PERF_CF_CONTROLLERS_PROFILING_MULT_DATA_SAMPLE_REGION__COUNT),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllersProfileMultDataRegionEnd_exit);


    osPTimerTimeNsLwrrentGet(&timensLwrr);

    LwU64_ALIGN32_UNPACK(
        &timensResult, &pProfiling->profiledTimesns[region]);
    lw64Add_MOD(&timensResult, &timensResult, &timensLwrr.data);
    LwU64_ALIGN32_PACK(
        &pProfiling->profiledTimesns[region], &timensResult);

perfCfControllersProfileMultDataRegionEnd_exit:
    lwosNOP();
}

/*!
 * @brief   Retrieves the @ref PWR_CHANNELS_STATUS from the multiplier
 *          data.
 *
 * @param[in]   pMultData   Multiplier data from which to retrieve @ref
 *                          PWR_CHANNELS_STATUS
 *
 * @return  @ref PWR_CHANNELS_STATUS pointer if enabled
 * @return  NULL if @ref PWR_CHANNELS_STATUS not enabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline PWR_CHANNELS_STATUS *
perfCfControllerMultiplierDataPwrChannelsStatusGet
(
    const PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
    return pMultData->pPwrChannelsStatus;
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
    return NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
}

/*!
 * @brief   Initializes the @ref PWR_CHANNELS_STATUS pointer in the
 *          multiplier data.
 *
 * @param[out]  pMultData           Multiplier data in which to initialize
 *                                  @ref PWR_CHANNELS_STATUS
 *
 * @return  FLCN_OK                 @ref PWR_CHANNELS_STATUS initialized
 *                                  successfully
 * @return  FLCN_ERR_NO_FREE_MEM    Could not allocate memory for
 *                                  @ref PWR_CHANNELS_STATUS
 * @return  FLCN_ERR_ILWALID_STATE  @ref PWR_CHANNELS_STATUS status in mult data
 *                                  not enabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerMultiplierDataPwrChannelsStatusInit
(
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
    {
        FLCN_STATUS status = FLCN_OK;

        if (pMultData->pPwrChannelsStatus == NULL)
        {
            pMultData->pPwrChannelsStatus = lwosCallocType(
                OVL_INDEX_DMEM(perfCfController), 1U, PWR_CHANNELS_STATUS);

            if (pMultData->pPwrChannelsStatus == NULL)
            {
                status = FLCN_ERR_NO_FREE_MEM;
                PMU_BREAKPOINT();
                goto perfCfControllerMultiplierDataPwrChannelsStatusInit_exit;
            }
            PWR_CHANNELS_STATUS_MASK_INIT(&pMultData->pPwrChannelsStatus->mask);
        }
perfCfControllerMultiplierDataPwrChannelsStatusInit_exit:
        return status;
    }
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
    {
        lwosVarNOP(pMultData);
        return FLCN_ERR_ILWALID_STATE;
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_CHANNEL)
}

/*!
 * @brief   Retrieves the @ref PWR_POLICIES_STATUS from the multiplier
 *          data.
 *
 * @param[in]   pMultData           Multiplier data from which to retrieve @ref
 *                                  PWR_POLICIES_STATUS
 * @param[out]  ppPwrPoliciesStatus Pointer in which to store pointer to
 *                                  @ref PWR_POLICIES_STATUS in pMultData. Set
 *                                  to the pointer if enabled, set to NULL
 *                                  otherwise.
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pMultData or ppPwrPoliciesStatus is
 *                                          NULL
 * @return  @ref FLCN_ERR_ILWALID_STATE     If called before
 *                                          @ref perfCfControllerMultiplierDataPwrPoliciesStatusInit
 *                                          is called or
 *                                          pMultData->pPwrPoliciesStatus is
 *                                          otherwise NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerMultiplierDataPwrPoliciesStatusGet
(
    const PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData,
    PWR_POLICIES_STATUS **ppPwrPoliciesStatus
)
{
    FLCN_STATUS status;

    PMU_ASSERT_OK_OR_GOTO(status,
        (pMultData != NULL) && (ppPwrPoliciesStatus != NULL) ?
            FLCN_OK : FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerMultiplierDataPwrPoliciesStatusGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)
    *ppPwrPoliciesStatus = pMultData->pPwrPoliciesStatus;
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)
    *ppPwrPoliciesStatus = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)

perfCfControllerMultiplierDataPwrPoliciesStatusGet_exit:
    return status;
}

/*!
 * @brief   Initializes the @ref PWR_POLICIES_STATUS pointer in the
 *          multiplier data.
 *
 * @param[out]  pMultData           Multiplier data in which to initialize
 *                                  @ref PWR_POLICIES_STATUS
 *
 * @return  @ref FLCN_OK                @ref PWR_POLICIES_STATUS initialized
 *                                      successfully
 * @return  @ref FLCN_ERR_NO_FREE_MEM   Could not allocate memory for
 *                                      @ref PWR_POLICIES_STATUS
 * @return  @ref FLCN_ERR_ILWALID_STATE @ref PWR_POLICIES_STATUS status in mult data
 *                                      not enabled
 * @return Others                       Errors propagated from callees
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerMultiplierDataPwrPoliciesStatusInit
(
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)
    {
        FLCN_STATUS status = FLCN_OK;

        if (pMultData->pPwrPoliciesStatus == NULL)
        {
            pMultData->pPwrPoliciesStatus = lwosCallocType(
                OVL_INDEX_DMEM(perfCfController), 1U, PWR_POLICIES_STATUS);

            PMU_ASSERT_OK_OR_GOTO(status,
                pMultData->pPwrPoliciesStatus != NULL ?
                    FLCN_OK : FLCN_ERR_NO_FREE_MEM,
                perfCfControllerMultiplierDataPwrPoliciesStatusInit_exit);

            PMU_ASSERT_OK_OR_GOTO(status,
                pwrPoliciesStatusInit(pMultData->pPwrPoliciesStatus),
                perfCfControllerMultiplierDataPwrPoliciesStatusInit_exit);
        }
perfCfControllerMultiplierDataPwrPoliciesStatusInit_exit:
        return status;
    }
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)
    {
        lwosVarNOP(pMultData);
        return FLCN_ERR_ILWALID_STATE;
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_MULT_DATA_PWR_POLICY)
}

/*!
 * @brief   Retrieves the @ref PERF_CF_PM_SENSORS_STATUS from the multiplier
 *          data.
 *
 * @param[in]   pMultData   Multiplier data from which to retrieve @ref
 *                          PERF_CF_PM_SENSORS_STATUS
 *
 * @return  @ref PERF_CF_PM_SENSORS_STATUS pointer if enabled
 * @return  NULL if @ref PERF_CF_PM_SENSORS_STATUS not enabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline PERF_CF_PM_SENSORS_STATUS *
perfCfControllerMultiplierDataPmSensorsStatusGet
(
    const PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
    return pMultData->pPmSensorsStatus;
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
    return NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
}

/*!
 * @brief   Initializes the @ref PERF_CF_PM_SENSORS_STATUS pointer in the
 *          multiplier data.
 *
 * @param[out]  pMultData           Multiplier data in which to initialize
 *                                  @ref PERF_CF_PM_SENSORS_STATUS
 *
 *
 * @return  FLCN_OK                 @ref PERF_CF_PM_SENSORS_STATUS initialized
 *                                  successfully
 * @return  FLCN_ERR_NO_FREE_MEM    Could not allocate memory for
 *                                  @ref PERF_CF_PM_SENSORS_STATUS
 * @return  FLCN_ERR_ILWALID_STATE  PM sensors status not enabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerMultiplierDataPmSensorsStatusInit
(
    PERF_CF_CONTROLLER_MULTIPLIER_DATA *pMultData
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
    {
        FLCN_STATUS status = FLCN_OK;

        if (pMultData->pPmSensorsStatus == NULL)
        {
            pMultData->pPmSensorsStatus = lwosCalloc(
                OVL_INDEX_DMEM(perfCfController),
                sizeof(*pMultData->pPmSensorsStatus));
            if (pMultData->pPmSensorsStatus == NULL)
            {
                status = FLCN_ERR_NO_FREE_MEM;
                PMU_BREAKPOINT();
                goto perfCfControllerMultiplierDataPmSensorsStatusInit_exit;
            }

            PERF_CF_PM_SENSORS_STATUS_MASK_INIT(&pMultData->pPmSensorsStatus->mask);
        }
perfCfControllerMultiplierDataPmSensorsStatusInit_exit:
        return status;
    }
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
    {
        lwosVarNOP(pMultData);
        return FLCN_ERR_ILWALID_STATE;
    }
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_PM_SENSOR)
}

/*!
 * @brief   Retrieves @ref PERF_CF_CONTROLLER::inflectionPointsDisableRequest
 *
 * @param[in]   pController Controller from which to retrieve
 * @param[out]  ppRequest   Pointer into which to assign
 *                          pController->inflectionPointsDisableRequest, or
 *                          @ref NULL if not enabled.
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pController or pRequest was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerInflectionPointsDisableRequestGet
(
    PERF_CF_CONTROLLER *pController,
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST **ppRequest
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pController != NULL) && (ppRequest != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerInflectionPointsDisableRequestGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    *ppRequest = &pController->inflectionPointsDisableRequest;
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    *ppRequest = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE)

perfCfControllerInflectionPointsDisableRequestGet_exit:
    return status;
}

/*!
 * @copydoc perfCfControllerInflectionPointsDisableRequestGet
 *
 * @note    Retrieves the data from a pointer to const into a pointer to const.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfCfControllerInflectionPointsDisableRequestGetConst
(
    PERF_CF_CONTROLLER *pController,
    const LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST **ppRequest
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pController != NULL) && (ppRequest != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        perfCfControllerInflectionPointsDisableRequestGetConst_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    *ppRequest = &pController->inflectionPointsDisableRequest;
#else // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    *ppRequest = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CF_CONTROLLER_PWR_POLICY_INFLECTION_POINTS_DISABLE)

perfCfControllerInflectionPointsDisableRequestGetConst_exit:
    return status;
}

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/cf/perf_cf_controller_1x.h"
#include "perf/cf/perf_cf_controller_2x.h"
#include "perf/cf/perf_cf_controller_util.h"
#include "perf/cf/perf_cf_controller_optp_2x.h"
#include "perf/cf/perf_cf_controller_dlppc_1x.h"
#include "perf/cf/perf_cf_controller_mem_tune.h"
#include "perf/cf/perf_cf_controller_mem_tune_2x.h"

#endif // PERF_CF_CONTROLLER_H
