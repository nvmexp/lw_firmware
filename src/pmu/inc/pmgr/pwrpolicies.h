/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2011-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicies.h
 * @brief Structure specification for the Power Policy functionality container -
 * all as specified by the Power Policy Table.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Policy_Table_2.X
 *
 * All of the actual functionality is found in @ref pwrpolicy.c.
 */

#ifndef PWRPOLICIES_H
#define PWRPOLICIES_H

/* ------------------------- System Includes ------------------------------- */
#include "pmgr/pwrpolicy.h"
#include "pmgr/pwrpolicyrel.h"
#include "pmgr/pwrviolation.h"
#include "pmgr/pwrmonitor.h"
#include "pmgr/pwrpolicies_status.h"

/* ------------------------- Application Includes -------------------------- */
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * @copydoc LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK
 */
typedef BOARDOBJGRPMASK_E32 PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK;

/*!
 * @copydoc LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE
 */
typedef struct
{
    /*!
     * @copydoc LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE::requestIdMask
     */
    PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK requestIdMask;

    /*!
     * @copydoc LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE::requests
     */
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST requests[
        LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_NUM];
} PWR_POLICIES_INFLECTION_POINTS_DISABLE;

/*!
 * Main container for all PWR_POLICY functionality.
 */
typedef struct PWR_POLICIES
{
    /*!
     * Version of Power Policy Table.
     */
    LwU8         version;
    /*!
     * Boolean indicating whether PWR_POLICY evaluation is enabled or not.  This
     * can disable of PWR_POLICY evaluation, while keeping the objects
     * constructed in the PMU such that they can be used for things like
     * PWR_CHANNEL evalutation.
     *
     * This feature is intended to be used for MODS and debug configurations
     * which need to disable power capping.
     */
    LwBool       bEnabled;
    /*!
     * Boolean indicating whether the power policies should send their limits
     * to the RM or PMU arbiter. A value of LW_FALSE will direct the power
     * policies to send its limits to the RM; LW_TRUE will send the limits
     * to the PMU arbiter.
     *
     * TODO-JBH: Remove once all the P-states 3.5 features have been enabled.
     */
    LwBool      bPmuPerfLimitsClient;
    /*!
     * Mask of PWR_POLICY_DOMGRP entries which take policy actions by
     * setting Domain Group PERF_LIMITs.
     */
    LwU32        domGrpPolicyMask;
    /*!
     * Mask of PWR_POLICY_DOMGRP entries whose inputs are strictly power-based.
     */
    LwU32        domGrpPolicyPwrMask;
    /*!
     * Mask of PWR_POLICY_DOMGRP entries whose inputs are come from thermal
     * policies.
     */
    LwU32        domGrpPolicyThermMask;
    /*!
     * Mask of PWR_POLICY_LIMIT entries which take policy actions by requesting
     * new limit values on other PWR_POLICY entries.
     */
    LwU32        limitPolicyMask;
    /*!
     * Mask of PWR_POLICY_BALANCE entries which balance the power drawn by
     * channels of PWR_POLICY_LIMIT controllers.
     */
    LwU32        balancePolicyMask;
    /*!
     * Global Domain Group Ceiling for all PWR_POLICY_DOMGRP objects..
     */
    PWR_POLICY_DOMGRP_GLOBAL_CEILING   domGrpGlobalCeiling;
    /*!
     * Global domgrp ceiling.
     */
    PERF_DOMAIN_GROUP_LIMITS    globalCeiling;
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SEMANTIC_POLICY_TABLE))
    /*!
     * An array that translates the semantic policy indexes/names (indexed
     * using LW2080_CTRL_PMGR_PWR_POLICY_IDX_<xyz> values) into an index within
     * the boardObjGrp containing all the power policies.
     *
     * Clients external to PMGR shall access power policies using
     * LW2080_CTRL_PMGR_PWR_POLICY_IDX_<xyz> values. An array value of
     * LW2080_CTRL_PMGR_PWR_POLICY_INDEX_ILWALID indicates that a semantic
     * policy is not present/specified on this GPU.
     */
    LwU8         semanticPolicyTbl[LW2080_CTRL_PMGR_PWR_POLICY_IDX_NUM_INDEXES];
#endif

    /*!
     * Timestamp indicating time when PWR CHANNEL tampering has been detected
     */
    FLCN_TIMESTAMP                              tamperPenaltyStart;
    /*!
     * Bool indicating channel has been tampered with
     */
    LwBool                                      bChannelTampered;
    /*!
     * Pointer to PWR_MONITOR object which manages power channels.
     */
    PWR_MONITOR *pMon;
    /*!
     * Maintains the number of conselwtive cycles of pwr policy evaluation
     * that a cap wasn't issued
     */
    LwU8         unCapCount;

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    /*!
     * @copydoc LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS::inflectionPointsDisable
     */
    PWR_POLICIES_INFLECTION_POINTS_DISABLE inflectionPointsDisable;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)
} PWR_POLICIES;

/* ------------------------- External definitions --------------------------- */
extern LwrtosSemaphoreHandle PmgrPwrPoliciesSemaphore;

/* ------------------------- Macros ----------------------------------------- */
/*!
 * Helper macro to retrieve pointer to the PWR_POLICIES structure.
 *
 * @return pointer ot the PWR_POLICIES structure.
 */
#define PWR_POLICIES_GET()                                                     \
    (Pmgr.pPwrPolicies)

/*!
 * @copydoc BOARDOBJGRP_GRP_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PWR_POLICY \
    (&(Pmgr.pwr.policies.super))

/*!
 * Macros for taking and giving semaphores using RTOS APIs.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SEMAPHORE))
    #define PWR_POLICIES_SEMAPHORE_TAKE                                        \
                    lwrtosSemaphoreTakeWaitForever(PmgrPwrPoliciesSemaphore)
    #define PWR_POLICIES_SEMAPHORE_GIVE                                        \
                    lwrtosSemaphoreGive(PmgrPwrPoliciesSemaphore)
#else
    #define PWR_POLICIES_SEMAPHORE_TAKE lwosNOP()
    #define PWR_POLICIES_SEMAPHORE_GIVE lwosNOP()
#endif

/*!
 * Helper macros to take/give PWR_POLICY semaphore whenever attaching/detaching
 * pmgrLibPwrPolicyClient overlay to keep it threadsafe
 */
#define ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED                       \
    do {                                                                       \
        OSTASK_ATTACH_AND_LOAD_IMEM_OVERLAY(                                   \
            OVL_INDEX_IMEM(pmgrLibPwrPolicyClient));                           \
        PWR_POLICIES_SEMAPHORE_TAKE;                                           \
    } while (LW_FALSE)

#define DETACH_OVERLAY_PWR_POLICY_CLIENT_LOCKED                                \
    do {                                                                       \
        PWR_POLICIES_SEMAPHORE_GIVE;                                           \
        OSTASK_DETACH_IMEM_OVERLAY(OVL_INDEX_IMEM(pmgrLibPwrPolicyClient));    \
    } while (LW_FALSE)

/*!
 * @copydoc BOARDOBJGRP_GET
 */
#define PWR_POLICY_GET(_objIdx) \
    (BOARDOBJGRP_OBJ_GET(PWR_POLICY, (_objIdx)))

/*!
 * @copydoc BOARDOBJGRP_IS_VALID
 */
#define PWR_POLICY_IS_VALID(_objIdx) \
    (BOARDOBJGRP_IS_VALID(PWR_POLICY, (_objIdx)))

/*!
 * Check if power policy at specified semantic index is enabled.
 *
 * @param[in] policyIdx     LW2080_CTRL_PMGR_PWR_POLICY_IDX_<xyz> index.
 *
 * @return A boolean indicating if power policy at this index is enabled.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SEMANTIC_POLICY_TABLE))
#define PMGR_PWR_POLICY_IDX_IS_ENABLED(policyIdx)                           \
    ((Pmgr.pPwrPolicies != NULL) &&                                         \
     ((policyIdx) <  LW2080_CTRL_PMGR_PWR_POLICY_IDX_NUM_INDEXES) &&        \
     PWR_POLICY_IS_VALID(Pmgr.pPwrPolicies->semanticPolicyTbl[(policyIdx)]))
#else
#define PMGR_PWR_POLICY_IDX_IS_ENABLED(policyIdx)                    LW_FALSE
#endif

/*!
 * Returns a mask of the power policy specified by semantic index.
 *
 * @param[in] policyIdx     LW2080_CTRL_PMGR_PWR_POLICY_IDX_<xyz> index.
 *
 * @return The mask of specified power policy if semantic policy feature is
 *         enabled; otherwise, LW2080_CTRL_PMGR_PWR_POLICY_INDEX_ILWALID is
 *         returned.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SEMANTIC_POLICY_TABLE))
#define PMGR_GET_PWR_POLICY_IDX_MASK(policyIdx)                             \
    BIT(Pmgr.pPwrPolicies->semanticPolicyTbl[(policyIdx)])
#else
#define PMGR_GET_PWR_POLICY_IDX_MASK(policyIdx)                             \
    LW2080_CTRL_PMGR_PWR_POLICY_INDEX_ILWALID
#endif

/*!
 * Returns a pointer to the power policy at specified semantic index.
 *
 * @param[in] policyIdx     LW2080_CTRL_PMGR_PWR_POLICY_IDX_<xyz> index.
 *
 * @return A pointer to the power policy if the policy at this index is
 *         enabled; otherwise, NULL is returned.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_SEMANTIC_POLICY_TABLE))
#define PMGR_GET_PWR_POLICY_IDX(policyIdx)                                  \
    (PMGR_PWR_POLICY_IDX_IS_ENABLED(policyIdx) ?                            \
     PWR_POLICY_GET(Pmgr.pPwrPolicies->semanticPolicyTbl[policyIdx]) :      \
     NULL)
#else
#define PMGR_GET_PWR_POLICY_IDX(policyIdx)                               NULL
#endif

/*!
 * @brief   List of an overlay descriptors required by a pwr policies construct code.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_CONSTRUCT                          \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)                                \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicyConstruct)           \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICY_35_CONSTRUCT

/*!
 * @brief   List of an overlay descriptors required by a pwr policies evaluate code.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE                           \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perf)                                \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE_PWR_EQUATION_DYNAMIC          \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE_PWR_POLICY_INPUT_FILTERING    \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicy)                    \
    PWR_POLICIES_35_OVERLAYS_EVALUATE

/*!
 * @brief   List of an overlay descriptors required by a pwr policies load code.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 */
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_LOAD                               \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_I2C_DEVICE_LOAD                        \
    PWR_POLICY_WORKLOAD_MULTIRAIL_LOAD_OVERLAYS                                \
    PWR_POLICY_WORKLOAD_COMBINED_1X_LOAD_OVERLAYS                              \
    PWR_POLICY_PERF_CF_PWR_MODEL_OVERLAYS

/*!
 * Helper macro to wrap the client-specific calls of pwrPoliciesEvaluate_XX().
 * Includes all common functionality across implementations.  This allows us to
 * remove a common wrapper helper function (formerly pwrPoliciesEvaluate()) and
 * switch statement ((formerly _pwrPoliciesEvaluate()), allowing more flexible
 * implementations of PWR_POLICIES and saving on IMEM overhead.
 *
 * @note Expected to be called in conjunction with @ref
 * PWR_POLICIES_EVALUATE_EPILOGUE().
 *
 * @param[in] pPolicies   PWR_POLICIES pointer
 */
#define PWR_POLICIES_EVALUATE_PROLOGUE(pPolicies)                              \
    do {                                                                       \
        OSTASK_OVL_DESC _ovlDescListPwrPoliciesEvaluate[] = {                  \
            OSTASK_OVL_DESC_DEFINE_PWR_POLICIES_EVALUATE                       \
        };                                                                     \
                                                                               \
        /*! If PWR_POLICY evaluation is disabled, can short-circuit return. */ \
        if (!((pPolicies)->bEnabled))                                          \
        {                                                                      \
            break;                                                             \
        }                                                                      \
                                                                               \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(                                \
            _ovlDescListPwrPoliciesEvaluate);                                  \
        /*
         * Note that when taking both the perf semaphore and the PWR_POLICIES
         * semaphore, the perf semaphore must be taken first.
         */                                                                    \
        perfReadSemaphoreTake();                                               \
        ATTACH_AND_LOAD_OVERLAY_PWR_POLICY_CLIENT_LOCKED;

/*!
 * Helper macro to wrap the client-specific calls of pwrPoliciesEvaluate_XX().
 * Includes all common functionality across implementations.  This allows us to
 * remove a common wrapper helper function (formerly pwrPoliciesEvaluate()) and
 * switch statement ((formerly _pwrPoliciesEvaluate()), allowing more flexible
 * implementations of PWR_POLICIES and saving on IMEM overhead.
 *
 * @note Expected to be called in conjunction with @ref
 * PWR_POLICIES_EVALUATE_PROLOGUE().
 *
 * @param[in] pPolicies   PWR_POLICIES pointer
 */
#define PWR_POLICIES_EVALUATE_EPILOGUE(pPolicies)                              \
        DETACH_OVERLAY_PWR_POLICY_CLIENT_LOCKED;                               \
        perfReadSemaphoreGive();                                               \
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(                                 \
            _ovlDescListPwrPoliciesEvaluate);                                  \
    } while (LW_FALSE)

/*!
 * @brief   Initializes a
 *          @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK structure
 *
 * @param[out]  _pRequestIdMask @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK
 *                              to initialize
 */
#define pwrPoliciesInflectionPointsDisableRequestIdMaskInit(_pRequestIdMask)                       \
    boardObjGrpMaskInit_E32((_pRequestIdMask))

/*!
 * @brief   Exports a @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK
 *          structure to an
 *          @ref LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK
 *
 * @param[in]   _pRequestIdMask @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK
 *                              from which to export
 * @param[out]  _pExportMask    @ref LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK
 *                              to which to export
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors returned by callees
 */
#define pwrPoliciesInflectionPointsDisableRequestIdMaskExport(_pRequestIdMask, _pExportMask)   \
    boardObjGrpMaskExport_E32((_pRequestIdMask), (_pExportMask))

/*!
 * @brief   Sets a given request ID in a request ID mask.
 *
 * @param[in,out]   _pRequestIdMask @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK in
 *                                  which to request disablement
 * @param[in]       _requestId      @ref LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID
 *                                  for which to disable inflection points.
 */
#define pwrPoliciesInflectionPointsDisableRequestIdMaskRequest(_pRequestIdMask, _requestId)          \
    boardObjGrpMaskBitSet((_pRequestIdMask), (_requestId))

/*!
 * @brief   Clears a given request ID in a request ID mask.
 *
 * @param[in,out]   _pRequestIdMask @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_MASK in
 *                                  which to request disablement
 * @param[out]      _requestId     @ref LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID
 *                                  for which to no longer disable inflection
 *                                  points.
 */
#define pwrPoliciesInflectionPointsDisableRequestIdMaskClear(_pRequestIdMask, _requestId)            \
    boardObjGrpMaskBitClr((_pRequestIdMask), (_requestId))

/* ------------------------- Function Prototypes  --------------------------- */
/*!
 * Constructs the PWR_POLICIES object, including the implementation-specific
 * manner.
 */
#define PwrPoliciesConstruct(fname) FLCN_STATUS (fname)(PWR_POLICIES **ppPwrPolicies)

/*!
 * Construct Header of Power Policy Table. This will initialize all data
 * that does not belong to any policy entry or policyRel entry.
 *
 * @param[in/out] ppPolicies
 *     Pointer to PWR_POLICIES pointer at which to populate PWR_POLICIES
 *     structure.
 * @param[in]     pMon
 *     Pointer to power monitor object.
 * @param[in]     pHdr
 *     Pointer to RM buffer for PWR_POLICIES header data.
 *
 * @return FLCN_OK
 *     Successful construction of the PWR_POLICIES header.
 */
#define PwrPoliciesConstructHeader(fname) FLCN_STATUS (fname)(PWR_POLICIES **ppPolicies, PWR_MONITOR *pMon, RM_PMU_PMGR_PWR_POLICY_BOARDOBJGRP_SET_HEADER *pHdr)

/*!
 * Implement this interface to update baseSamplingPeriod when a new Perf update
 * is sent to PMU.
 *
 * @param[in]  pStatus     new RM_PMU_PERF_STATUS to be updated
 */
#define PwrPoliciesProcessPerfStatus(fname) void (fname)(RM_PMU_PERF_STATUS *pStatus)

/*!
 * @brief   Evaluation function prototype for all PWR_VIOLATIONS & PWR_POLICIES.
 *
 * Evaluates selected PWR_VIOLATION and PWR_POLICY objects for their output
 * decisions (usually clock and/or pstate caps). After all are evaluated, their
 * output decisions are coalesced/arbitrated and applied to the SW/HW state.
 *
 * @param[in]   pPolicies
 *      Pointer to PWR_POLICIES structure to be evaluated
 * @param[in]   expiredPolicyMask
 *      Mask of all expired policies to be evaluated
 * @param[in]   pExpiredViolationMask
 *      Mask of all expired violations to be evaluated
 */
#define PwrPoliciesEvaluate(fname) void (fname)(PWR_POLICIES *pPolicies, LwU32 expiredPolicyMask, BOARDOBJGRPMASK *pExpiredViolationMask)

/*!
 * @brief   Retrieve status for the requested @ref PWR_POLICY objects
 *
 * @param[in]   pPwrPolicies    @ref PWR_POLICIES group pointer
 * @param[out]  pStatus         Pointer to buffer into which to place status
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  The requuested mask in pStatus is
 *                                          not a valid set of @ref PWR_POLICY
 *                                          objects
 * @return  Others                          Errors propagated from callees.
 */
#define PwrPoliciesStatusGet(fname) FLCN_STATUS (fname)(PWR_POLICIES *pPwrPolicies, PWR_POLICIES_STATUS *pStatus)

/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Global PWR_POLICIES interfaces
 */
FLCN_STATUS constructPmgrPwrPolicies(void)
    GCC_ATTRIB_SECTION("imem_init", "constructPmgrPwrPolicies");
PwrPoliciesConstruct    (pwrPoliciesConstruct)
    GCC_ATTRIB_SECTION("imem_libPmgrInit", "pwrPoliciesConstruct");
PwrPoliciesConstructHeader (pwrPoliciesConstructHeader)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPoliciesConstructHeader");
FLCN_STATUS pwrPoliciesLoad(PWR_POLICIES *pPolicies)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPoliciesLoad");
FLCN_STATUS pwrPoliciesVfIlwalidate(PWR_POLICIES *pPolicies)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPoliciesVfIlwalidate");
PwrPoliciesEvaluate (pwrPoliciesEvaluate_SUPER)
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X_ONLY)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "pwrPoliciesEvaluate_SUPER");
#else
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicy", "pwrPoliciesEvaluate_SUPER");
#endif
PwrPoliciesStatusGet    (pwrPoliciesStatusGet)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPoliciesStatusGet");
void       pwrPoliciesPwrChannelTamperDetected(PWR_POLICIES *pPolicies)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrMonitor", "pwrPoliciesPwrChannelTamperDetected");

/*!
 * BoardObjGrp interfaces
 */
BoardObjGrpIfaceModel10CmdHandler       (pwrPolicyBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_pmgrLibBoardObj", "pwrPolicyBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler       (pwrPolicyBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrPolicyBoardObjGrpIfaceModel10GetStatus");
BoardObjGrpIfaceModel10SetHeader  (pmgrPwrPolicyIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pmgrPwrPolicyIfaceModel10SetHeader");
BoardObjGrpIfaceModel10SetEntry   (pmgrPwrPolicyIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pmgrPwrPolicyIfaceModel10SetEntry");
BoardObjGrpIfaceModel10SetEntry   (pmgrPwrPolicyRelIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pmgrPwrPolicyRelIfaceModel10SetEntry");
BoardObjGrpIfaceModel10SetEntry   (pmgrPwrViolationIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pmgrPwrViolationIfaceModel10SetEntry");

BoardObjGrpIfaceModel10GetStatusHeader  (pmgrPwrPolicyIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pmgrPwrPolicyIfaceModel10GetStatusHeader");
BoardObjIfaceModel10GetStatus               (pwrViolationIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_pmgrLibQuery", "pwrViolationIfaceModel10GetStatus");

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_3X)
#define pwrPoliciesProcessPerfStatus(pStatus)                                 \
    pwrPoliciesProcessPerfStatus_3X(pStatus)
#else
#define pwrPoliciesProcessPerfStatus(pstatus)                                 \
    pwrPoliciesProcessPerfStatus_2X(pStatus)
#endif

/* ------------------------- Inline Functions ------------------------------- */
/*!
 * @brief   Initializes a @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE structure
 *
 * @param[out]  pDisable    @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE to
 *                          initialize
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPoliciesInflectionPointsDisableInit
(
    PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable
)
{
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID requestId;

    pwrPoliciesInflectionPointsDisableRequestIdMaskInit(
        &pDisable->requestIdMask);

    for (requestId = 0U;
         requestId <
            LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_NUM;
         requestId++)
    {
        LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_INIT(
            &pDisable->requests[requestId]);
    }

    return FLCN_OK;
}

/*!
 * @brief   Exports a @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE structure to a
 *          @ref LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE
 *
 * @param[in]   pDisable        @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE from
 *                              which to export
 * @param[out]  pExportDisable  @ref LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE
 *                              to which to export
 *
 * @return  @ref FLCN_OK    Success
 * @return  Others          Errors returned by callees
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPoliciesInflectionPointsDisableExport
(
    PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable,
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE *pExportDisable
)
{
    FLCN_STATUS status;
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID requestId;

    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPoliciesInflectionPointsDisableRequestIdMaskExport(
            &pDisable->requestIdMask, &pExportDisable->requestIdMask),
        pwrPoliciesInflectionPointsDisableExport_exit);

    for (requestId = 0U;
         requestId <
            LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_NUM;
         requestId++)
    {
        pExportDisable->requests[requestId] = pDisable->requests[requestId];
    }

pwrPoliciesInflectionPointsDisableExport_exit:
    return status;
}

/*!
 * @brief   Indicates that inflection points should be disabled for a given
 *          requestId
 *
 * @param[in,out]   pDisable   @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE in
 *                              which to request disablement
 * @param[in]       requestId     @ref LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID
 *                              for which to disable inflection points.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPoliciesInflectionPointsDisableRequest
(
    PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable,
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID requestId,
    const LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST *pRequest
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (requestId <
            LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_NUM),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPoliciesInflectionPointsDisableRequest_exit);

    pwrPoliciesInflectionPointsDisableRequestIdMaskRequest(
        &pDisable->requestIdMask, requestId);

    pDisable->requests[requestId] = *pRequest;

pwrPoliciesInflectionPointsDisableRequest_exit:
    return status;
}

/*!
 * @brief   Indicates that inflection points should no longer disabled for a
 *          given requestId
 *
 * @param[in,out]   pDisable   @ref PWR_POLICIES_INFLECTION_POINTS_DISABLE in
 *                              which to request disablement
 * @param[out]      requestId     @ref LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID
 *                              for which to no longer disable inflection
 *                              points.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPoliciesInflectionPointsDisableClear
(
    PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable,
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID requestId
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (requestId < LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID_NUM),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPoliciesInflectionPointsDisableClear_exit);

    pwrPoliciesInflectionPointsDisableRequestIdMaskClear(
        &pDisable->requestIdMask, requestId);

    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_INIT(
        &pDisable->requests[requestId]);

pwrPoliciesInflectionPointsDisableClear_exit:
    return status;
}

/*!
 * @brief   Retrieves the lowest @ref PSTATE for which inflection limits are
 *          disabled
 *
 * @param[in]   pDisable        Pointer to disable requestIdMask structure
 * @param[in]   timestamp       Minimum timestamp from which to consider
 *                              requests
 * @param[out]  pPstateIdxMin   Minimum @ref PSTATE @ref LwBoardObjIdx at which
 *                              inflection should be disabled.
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  pDisable or pPstateIdxMin
 *                                          were NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPoliciesInflectionPointsDisableLowestDisabledPstateGet
(
    PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable,
    LwU64_ALIGN32 timestamp,
    LwBoardObjIdx *pPstateIdxLowest
)
{
    FLCN_STATUS status;
    LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_ID requestId;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pDisable != NULL) && (pPstateIdxLowest != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPoliciesInflectionPointsDisableLowestDisabledPstateGet_exit);

    *pPstateIdxLowest = LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;
    BOARDOBJGRPMASK_FOR_EACH_BEGIN(&pDisable->requestIdMask, requestId)
    {
        const LwU64 requestTimestamp =
            LwU64_ALIGN32_VAL(&pDisable->requests[requestId].timestamp);

        //
        // Update the lowest PSTATE index only if the timestamp is valid and
        // the old index is invalid or the new index is lower.
        //
        if (((requestTimestamp ==
                LW2080_CTRL_PMGR_PWR_POLICIES_INFLECTION_POINTS_DISABLE_REQUEST_TIMESTAMP_INFINITELY_RECENT) ||
             (requestTimestamp >= LwU64_ALIGN32_VAL(&timestamp))) &&
            ((*pPstateIdxLowest == LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID) ||
             (pDisable->requests[requestId].pstateIdxLowest < *pPstateIdxLowest)))
        {
            *pPstateIdxLowest = pDisable->requests[requestId].pstateIdxLowest;
        }

    }
    BOARDOBJGRPMASK_FOR_EACH_END;

pwrPoliciesInflectionPointsDisableLowestDisabledPstateGet_exit:
    return status;
}

/*!
 * @brief   Retrieves @ref PWR_POLICIES::inflectionPointsDisableGet
 *
 * @param[in]   pPwrPolicies    @ref PWR_POLICIES object pointer
 * @param[out]  ppDisable       Pointer to which to assign pPwrPolicies
 *                              inflectionPointsDisable, if enabled
 *                              (and NULL otherwise).
 *
 * @return  @ref FLCN_OK                    Success
 * @return  @ref FLCN_ERR_ILWALID_ARGUMENT  A pointer argument was NULL
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPoliciesInflectionPointsDisableGet
(
    PWR_POLICIES *pPwrPolicies,
    PWR_POLICIES_INFLECTION_POINTS_DISABLE **ppDisable
)
{
    FLCN_STATUS status;
    PMU_ASSERT_TRUE_OR_GOTO(status,
        (pPwrPolicies != NULL) && (ppDisable != NULL),
        FLCN_ERR_ILWALID_ARGUMENT,
        pwrPoliciesInflectionPointsDisableGet_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    *ppDisable = &pPwrPolicies->inflectionPointsDisable;
#else // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)
    *ppDisable = NULL;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_INFLECTION_POINTS_DISABLE)

pwrPoliciesInflectionPointsDisableGet_exit:
    return status;
}

/*!
 * @copydoc PwrPoliciesConstruct
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPoliciesConstruct_SUPER
(
    PWR_POLICIES **ppPolicies
)
{
    FLCN_STATUS status  = FLCN_OK;
    PWR_POLICIES_INFLECTION_POINTS_DISABLE *pDisable;

    PMU_ASSERT_OK_OR_GOTO(status,
        pwrPoliciesInflectionPointsDisableGet(
            *ppPolicies, &pDisable),
        pwrPoliciesConstruct_SUPER_exit);

    if (pDisable != NULL)
    {
        pwrPoliciesInflectionPointsDisableInit(pDisable);
    }

pwrPoliciesConstruct_SUPER_exit:
    return status;
}

/* ------------------------- Child Class Includes --------------------------- */
#include "pmgr/2x/pwrpolicies_2x.h"
#include "pmgr/3x/pwrpolicies_3x.h"

#endif // PWRPOLICIES_H
