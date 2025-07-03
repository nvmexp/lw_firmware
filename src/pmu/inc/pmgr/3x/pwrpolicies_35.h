/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file  pwrpolicies_35.h
 * @brief Structure specification for the Power Policy functionality container -
 * all as specified by the Power Policy Table 3.X.
 *
 * https://wiki.lwpu.com/engwiki/index.php/Resman/PState/Data_Tables/Power_Tables/Power_Policy_Table_3.X
 *
 * All of the actual functionality is found in @ref pwrpolicy.c.
 */

#ifndef PWRPOLICIES_35_H
#define PWRPOLICIES_35_H

/* ------------------------- System Includes ------------------------------- */
/* ------------------------- Application Includes -------------------------- */
#include "pmgr/3x/pwrpolicies_3x.h"
#include "perf/cf/perf_cf_controller_status.h"

/* ------------------------- Types Definitions ----------------------------- */
/*!
 * Structure holding all data related to a single sampling multiplier.
 */
struct PWR_POLICY_MULTIPLIER_DATA
{
    /*!
     * Multiplier data structures are linked into a singly linked list so that
     * they can be iterated over using @ref FOR_EACH_MULTIPLIER() macro.
     */
    PWR_POLICY_MULTIPLIER_DATA    *pNext;
    /*!
     * Channel status structure holding input data to the policy(s).
     */
    PWR_CHANNELS_STATUS            channelsStatus;
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35_FILTER_PAYLOAD_PERF_CF_CONTROLLERS_REDUCED_STATUS)
    /*!
     * Perf Cf Controller Util Status structure holding input data to the policy(s).
     */
    PERF_CF_CONTROLLERS_REDUCED_STATUS
                                   perfCfControllersStatus;
#endif
    /*!
     * Mask of the policies that use same @ref samplingMultiplier.
     */
    BOARDOBJGRPMASK_E32            maskPolicies;
    /*!
     * Time (in OS ticks) when this multiplier is due to expire.
     */
    LwU32                          expirationTimeTicks;
    /*!
     * Sampling period (in OS ticks) of this multiplier.
     */
    LwU32                          samplingPeriodTicks;
    /*!
     * Sampling multiplier identifying this structure.
     */
    LwU8                           samplingMultiplier;
};

/*!
 * Structure extending the PWR_POLICIES_3X object for Power Policy 3.5.
 */
typedef struct PWR_POLICIES_35
{
    /*
     * All PWR_POLICIES_3X fields.
     */
    PWR_POLICIES_3X super;
    /*!
     * Head pointer to the singly linked list of multipler data structures.
     * Used by @ref FOR_EACH_MULTIPLIER() iterating macro.
     */
    PWR_POLICY_MULTIPLIER_DATA *pMultDataHead;
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL)
    /*!
     * Mask of the policies that implement the PERF_CF_PWR_MODEL interface.
     */
    BOARDOBJGRPMASK_E32 perfCfPwrModelPolicyMask;
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP_DATA_MAP))
    /*!
     * Clock domain index to @ref RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY
     * map.
     */
    RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY  domGrpDataMap[RM_PMU_PWR_POLICY_DOMGRP_MAP_MAX_CLK_DOMAINS];
#endif
#if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK))
    /*!
     * Mask of all supported Domain Group Limits by all policies
     */
    BOARDOBJGRPMASK_E32 domGrpLimitPolicyMask;
#endif
} PWR_POLICIES_35;

/* ------------------------- Macros ---------------------------------------- */
/*!
 * Helper macro to get pointer to PWR_POLICIES_3X object.
 *
 * CRPTODO - implement dynamic type-checking/casting.
 */
#define PWR_POLICIES_35_GET()                                                 \
    ((PWR_POLICIES_35 *)Pmgr.pPwrPolicies)

/*!
 * @brief   List of additional overlay descriptors required by pwr policies
 *          construct code when PWR_POLICY_35 is enabled.
 *
 * @note    Review the use cases in Tasks.pm and update MAX_OVERLAYS_IMEM after
 *          updates to this macro. Failure to do so might result in falcon halt.
 *
 * @todo    kyleo - I'm not so certain that pmgrLibPwrPolicy is needed here.
 *          maybe remove this in some followup as necessary.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICY_35_CONSTRUCT                         \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrPolicy35MultData)             \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, pmgrLibPwrPolicy)
#else
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICY_35_CONSTRUCT                         \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * List of overlays required to set @ref PERF_CF_CONTROLLERS_REDUCED_STATUS from
 * PWR_POLICY_35 code.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35_FILTER_PAYLOAD_PERF_CF_CONTROLLERS_REDUCED_STATUS)
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICY_PERF_CF_CONTROLLER_REDUCED_STATUS_GET        \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfCfBoardObj)
#else
#define OSTASK_OVL_DESC_DEFINE_PWR_POLICY_PERF_CF_CONTROLLER_REDUCED_STATUS_GET        \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * List of overlays required to set @ref PWR_POLICY_MULTIPLIER_DATA from
 * PWR_POLICY_35 code.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
#define PWR_POLICIES_35_OVERLAYS_MULT_DATA                                     \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrPolicy35MultData)             \
    OSTASK_OVL_DESC_DEFINE_PWR_CHANNELS_STATUS_GET                             \
    OSTASK_OVL_DESC_DEFINE_PWR_POLICY_PERF_CF_CONTROLLER_REDUCED_STATUS_GET
#else
#define PWR_POLICIES_35_OVERLAYS_MULT_DATA                                     \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * List of overlay descriptors referenced from @ref pwrPolicies3xLoad_35().
 *
 * Because the LOAD overlay set is so large, this will be attached from within
 * @ref pwrPolicies3xLoad_35().
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
#define PWR_POLICIES_35_OVERLAYS_LOAD                                           \
    PWR_POLICIES_35_OVERLAYS_MULT_DATA
#else
#define PWR_POLICIES_35_OVERLAYS_LOAD                                           \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * List of overlay descriptors referenced from @ref s_pwrPolicies35Filter().
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
#define PWR_POLICIES_35_OVERLAYS_FILTER                                        \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, pmgrPwrPolicy35MultData)             \
    PWR_POLICY_PERF_CF_PWR_MODEL_OVERLAYS                                      \
    PWR_POLICIES_3X_FILTER_OVERLAYS
#else
#define PWR_POLICIES_35_OVERLAYS_FILTER                                        \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * List of additional overlay descriptors required to be attached from @ref
 * PWR_POLICIES_EVALUATE_PROLOGUE() when PWR_POLICY_35 is enabled.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
#define PWR_POLICIES_35_OVERLAYS_EVALUATE                                      \
    OVL_INDEX_IMEM(libBoardObj)
#else
#define PWR_POLICIES_35_OVERLAYS_EVALUATE                                      \
    OSTASK_OVL_DESC_ILWALID
#endif

/*!
 * Alias pwrPolicies3XCallbackBody virtual function for PWR_POLICY_35.
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35)
#define pwrPolicies3xCallbackBody(ticksExpired)                                \
    pwrPolicies3xCallbackBody_35((ticksExpired))
#endif

/* ------------------------- Inline Functions  ----------------------------- */
/*!
 * @brief   Accessor for PWR_POLICIES_35::perfCfPwrModelPolicyMask.
 *
 * @param[in]   pPolicies    PWR_POLICIES pointer.
 *
 * @return
 *     @ref PWR_POLICIES_35::perfCfPwrModelPolicyMask pointer if feature enabled
 *     NULL pointer if feature is disabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOARDOBJGRPMASK_E32 *
pwrPolicies35GetPerfCfPwrModelPolicyMask
(
    PWR_POLICIES *pPolicies
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL)
    PWR_POLICIES_35 *pPolicies35 = (PWR_POLICIES_35 *)pPolicies;
    return &(pPolicies35->perfCfPwrModelPolicyMask);
#else
    return NULL;
#endif
}

/*!
 * @brief   Accessor for specific instance of
 *          RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY from the map.
 *
 * @param[in]   pPolicies       Pointer to PWR_POLICIES_35
 * @param[in]   clkDomainIdx    Index to the clock domain
 * @param[out]  pDataMap        Instance of RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY
 *                              corresponding to the clkDomainIdx
 *
 * @return  FLCN_ERROR clkDomainIdx is invalid
 *          FLCN_OK    otherwise
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
pwrPolicies35DomGrpDataGet
(
    PWR_POLICIES_35 *pPolicies,
    RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY *pDataMap,
    LwU8 clkDomainIdx
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_DOMGRP_DATA_MAP)
    if (clkDomainIdx >= RM_PMU_PWR_POLICY_DOMGRP_MAP_MAX_CLK_DOMAINS)
    {
        PMU_BREAKPOINT();
        return FLCN_ERROR;
    }

    *pDataMap = pPolicies->domGrpDataMap[clkDomainIdx];
    if (pDataMap->regimeId == RM_PMU_PMGR_PWR_POLICY_DOMGRP_MAP_ENTRY_ILWALID)
    {
        PMU_BREAKPOINT();
        return FLCN_ERROR;
    }
    return FLCN_OK;
#else
    pDataMap = NULL;
    return FLCN_OK;
#endif
}

/*!
 * @brief   Accessor for PWR_POLICIES_35::domGrpLimitPolicyMask.
 *
 * @param[in]   pPolicies    PWR_POLICIES_35 pointer.
 *
 * @return
 *     @ref PWR_POLICIES_35::domGrpLimitPolicyMask pointer if feature enabled
 *     NULL pointer if feature is disabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline BOARDOBJGRPMASK_E32 *
pwrPolicies35GetDomGrpLimitsMask
(
    PWR_POLICIES_35 *pPolicies
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_DOMAIN_GROUP_LIMITS_MASK)
    return &(pPolicies->domGrpLimitPolicyMask);
#else
    return NULL;
#endif
}

/*!
 * @brief   Accessor for PWR_POLICY_MULTIPLIER_DATA::perfCfControllersStatus.
 *
 * @param[in]   pPayload  PWR_POLICY_3X_FILTER_PAYLOAD_TYPE pointer.
 *
 * @return
 *     @ref PWR_POLICIES_35::perfCfControllersStatus pointer if feature enabled
 *     NULL pointer if feature is disabled
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline PERF_CF_CONTROLLERS_REDUCED_STATUS *
pwrPolicy35FilterPayloadPerfCfControllersStatusGet
(
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35_FILTER_PAYLOAD_PERF_CF_CONTROLLERS_REDUCED_STATUS)
    return &(pPayload->perfCfControllersStatus);
#else
    return NULL;
#endif
}

/* ------------------------- Function Prototypes  -------------------------- */
/* ------------------------- Global Variables ------------------------------ */
/* ------------------------- Public Functions ------------------------------ */
/*!
 * Global PWR_POLICIES_35 interfaces
 */
PwrPoliciesConstruct (pwrPoliciesConstruct_35)
    GCC_ATTRIB_SECTION("imem_libPmgrInit", "pwrPoliciesConstruct_35");
PwrPoliciesConstructHeader (pwrPoliciesConstructHeader_35)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPoliciesConstructHeader_35");
FLCN_STATUS pwrPolicies35MultDataConstruct(PWR_POLICIES_35 *pPolicies)
    GCC_ATTRIB_SECTION("imem_pmgrLibPwrPolicyConstruct", "pwrPolicies35MultDataConstruct");
PwrPolicies3xLoad   (pwrPolicies3xLoad_35)
    GCC_ATTRIB_SECTION("imem_pmgrLibLoad", "pwrPolicies3xLoad_35");
PwrPolicies3xCallbackBody (pwrPolicies3xCallbackBody_35)
    GCC_ATTRIB_SECTION("imem_pmgrPwrPolicyCallback", "pwrPolicies3xCallbackBody_35");
PERF_CF_CONTROLLERS_REDUCED_STATUS * pwrPolicy35FilterPayloadPerfCfControllersStatusGet(
PWR_POLICY_3X_FILTER_PAYLOAD_TYPE *pPayload)
    GCC_ATTRIB_SECTION("imem_pmgr", "pwrPolicy35FilterPayloadPerfCfControllersStatusGet");

#endif // PWRPOLICIES_35_H

