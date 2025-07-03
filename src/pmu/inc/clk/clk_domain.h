/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_DOMAIN_H
#define CLK_DOMAIN_H

#include "g_clk_domain.h"

#ifndef G_CLK_DOMAIN_H
#define G_CLK_DOMAIN_H

/*!
 * @file clk_domain.h
 * @brief @copydoc clk_domain.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_DOMAIN CLK_DOMAIN, CLK_DOMAIN_BASE;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Helper macro to return a pointer to the CLK_DOMAINS object.
 *
 * @return Pointer to the CLK_DOMAINS object.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN))
#define CLK_CLK_DOMAINS_GET()                                                  \
    (&Clk.domains)
#else
#define CLK_CLK_DOMAINS_GET()                                                  \
    ((CLK_DOMAINS *)(NULL))
#endif


/*!
 * @brief       Accesor for a CLK_DOMAIN BOARDOBJ by BOARDOBJGRP index.
 *
 * @param[in]   _pstateIdx  BOARDOBJGRP index for a CLK_DOMAIN BOARDOBJ.
 *
 * @return      Pointer to a CLK_DOMAIN object at the provided BOARDOBJGRP index.
 *
 * @memberof    CLK_DOMAIN
 *
 * @public
 */
#define CLK_DOMAIN_GET(_idx)                                                  \
    (BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, (_idx)))

/*!
 * Helper macro to retrieve the version @ref LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_<xyz>
 * of a CLK_DOMAINS.
 *
 *
 * @return @ref CLK_DOMAINS::version
 */
#define clkDomainsVersionGet()                                                \
    CLK_CLK_DOMAINS_GET()->version

/*!
 * @brief       Helper macro to check whether secondacy VF Lwrves are enabled.
 *
 * @return      LW_TRUE     If secondacy VF Lwrves are enabled.
 *              LW_FALSE    Otherwise.
 *
 * @memberof    CLK_DOMAINS
 *
 * @public
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL)
#define clkDomainsIsSecVFLwrvesEnabled()    (clkVfRelsVfEntryCountSecGet() != 0U)
#else
#define clkDomainsIsSecVFLwrvesEnabled()    (clkProgsVfSecEntryCountGet() != 0U)
#endif

/*!
 * @brief       Helper macro to return the number of secondacy VF Lwrves present.
 *
  * @memberof    CLK_DOMAINS
 *
 * @public
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_MULTIPLE_SEC_LWRVE)
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL)
#define clkDomainsNumSecVFLwrves()    (clkVfRelsVfEntryCountSecGet())
#else
#define clkDomainsNumSecVFLwrves()    (clkProgsVfSecEntryCountGet())
#endif
#else // Retain legacy behavior
#define clkDomainsNumSecVFLwrves()    (1)
#endif

/*!
 * Macro to locate CLK_DOMAINS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_CLK_DOMAIN                                  \
    (&(CLK_CLK_DOMAINS_GET()->super.super))

/*!
 * Accessor macro for CLK_DOMAINS::deltas.freqDelta.
 *
 * @return pointer to @ref CLK_DOMAINS::deltas.freqDelta
 */
#define clkDomainsFreqDeltaGet()                                              \
    &CLK_CLK_DOMAINS_GET()->deltas.freqDelta

/*!
 * Accessor macro for CLK_DOMAINS::bDebugMode
 *
 * @return @ref CLK_DOMAINS::bDebugMode
 */
#define clkDomainsDebugModeEnabled()                                          \
    (CLK_CLK_DOMAINS_GET()->bDebugMode)

/*!
 * Accessor macro for CLK_DOMAINS::bEnforceVfMonotonicity
 *
 * @return @ref CLK_DOMAINS::bEnforceVfMonotonicity
 */
#define clkDomainsVfMonotonicityEnforced()                                   \
    (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_VF_MONOTONICITY_SUPPORTED) && \
     (CLK_CLK_DOMAINS_GET()->bEnforceVfMonotonicity))

/*!
 * Accessor macro for CLK_DOMAINS::bEnforceVfSmoothening
 *
 * @return @ref CLK_DOMAINS::bEnforceVfSmoothening
 */
#define clkDomainsVfSmootheningEnforced()                                    \
    (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_VF_SMOOTHENING_SUPPORTED) &&  \
     (CLK_CLK_DOMAINS_GET()->bEnforceVfSmoothening))

/*!
 * Accessor macro for CLK_DOMAINS::bOverrideOVOC
 *
 * @return @ref CLK_DOMAINS::bOverrideOVOC
 */
#define clkDomainsOverrideOVOCEnabled()                                       \
    (CLK_CLK_DOMAINS_GET()->bOverrideOVOC)

/*!
 * Accessor macro for CLK_DOMAINS::bGrdFreqOCEnabled
 *
 * @return @ref CLK_DOMAINS::bGrdFreqOCEnabled
 */
#define clkDomainsGrdFreqOCEnabled()                                          \
    (CLK_CLK_DOMAINS_GET()->bGrdFreqOCEnabled)

/*!
 * Accessor macro for CLK_DOMAINS::xbarBoostVfeIdx
 *
 * @return @ref CLK_DOMAINS::xbarBoostVfeIdx
 */
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)
#define clkDomainsXbarBoostVfeIdxGet()                                        \
    (CLK_CLK_DOMAINS_GET()->xbarBoostVfeIdx)
#else
#define clkDomainsXbarBoostVfeIdxGet()                                        \
    (LW2080_CTRL_BOARDOBJ_IDX_ILWALID)
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)

/*!
 * Helper macro to retrieve the API domain @ref LW2080_CTRL_CLK_DOMAIN_<xyz> of
 * a CLK_DOMAIN object.
 *
 * @param[in]  pDomain  CLK_DOMAIN pointer
 *
 * @return @ref CLK_DOMAIN::apiDomain
 */
#define clkDomainApiDomainGet(pDomain)                                        \
    (((CLK_DOMAIN *)(pDomain))->apiDomain)

/*!
 * Helper CLK DOMAIN MACRO for @copydoc BOARDOBJ_TO_INTERFACE_CAST
 */
#define CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(_pDomain, _type)                \
    BOARDOBJ_TO_INTERFACE_CAST((_pDomain), CLK, CLK_DOMAIN, _type)

/*!
 * Helper macro returning the scaling factor required to colwert to kHz
 * (commonly used in PERF code) from units used by the CLK_DOMAIN (usually MHz).
 *
 * For most CLK_DOMAINs, this will return 1000.  The main exception is
 * PCIEGENCLK, where the units aren't actually Hz, but the PCIE Gen Speed.
 *
 * @param[in]  _pDomain
 *     Pointer to the CLK_DOMAIN for which to retrieve the scaling
 *     factor.
 *
 * @return Scaling factor
 */
#define clkDomainFreqkHzScaleGet(_pDomain)                                      \
    ((LW2080_CTRL_CLK_DOMAIN_PCIEGENCLK !=                                      \
      clkDomainApiDomainGet(_pDomain) ? 1000 : 1))

/*!
 * Accessor macro for CLK_DOMAINS::bClkMonEnabled
 *
 * @return @ref CLK_DOMAINS::bClkMonEnabled
 */
#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLOCK_MON)
#define clkDomainsIsClkMonEnabled() Clk.domains.bClkMonEnabled
#else
#define clkDomainsIsClkMonEnabled() LW_FALSE
#endif // PMUCFG_FEATURE_ENABLED(PMU_CLK_CLOCK_MON)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Structure describing deviation of given parameter (voltage, frequency, ...)
 * from it's nominal value.
 */
typedef struct
{
    /*!
     * This will give the deviation of given freq from it's nominal value.
     * NOTE: we have one single freq delta that will apply to all voltage rails
     */
    LW2080_CTRL_CLK_FREQ_DELTA  freqDelta;
    /*!
     * This will give the deviation of given voltage from it's nominal value.
     * This is an array with each index pointing to the deviation in voltage
     * for given specific voltage rail.
     */
    LwS32                      *pVoltDeltauV;
} CLK_DOMAIN_DELTA;

/*!
 * Clock domain info entry.  Defines the how the RM will control a given clock
 * domain per the VBIOS specification.
 */
struct CLK_DOMAIN
{
    /*!
     * BOARDOBJ_VTABLE super class. Must always be the first element in the structure.
     */
    BOARDOBJ_VTABLE super;

    /*!
     * Enumeration of the Clock Domain this CLK_DOMAIN object represents.
     */
    RM_PMU_CLK_CLKWHICH  domain;
    /*!
     * Enumeration of the Clock Domain this CLK_DOMAIN object represents as a
     * LW2080_CTRL_CLK_DOMAIN_<XZY>.
     */
    LwU32   apiDomain;
    /*!
     * Domain group index represented as RM_PMU_DOMAIN_GROUP_<XZY>
     * Used by Power policy domain group to access PSTATE and GPC2CLK.
     */
    LwU8    perfDomainGrpIdx;
};

/*!
 * Clock domain info.  Specifies all clock domains that the RM will control per
 * the VBIOS specification.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E32 super class.  Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E32             super;
    /*!
     * Mask of domains which implement CLK_DOMAIN_3X_PROG interface.
     */
    BOARDOBJGRPMASK_E32         progDomainsMask;
    /*!
     * Mask of domains which implement CLK_DOMAIN_3X_PRIMARY interface.
     */
    BOARDOBJGRPMASK_E32         clkMonDomainsMask;
    /*!
     * CLK_CNTR sampling period in the PMU (ms).  Used for the period at which
     * the CLK_DOMAINs will be periodically sampled by the PMU CLK code.
     */
    LwU16                       cntrSamplingPeriodms;

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLOCK_MON)
    /*!
     * Boolean indicating whether CLK_MONITOR is enabled.
     */
    LwBool                      bClkMonEnabled;
    /*!
     * CLK_MONITOR refernce window period (us). Used for the period at which
     * the CLK_MONITORs will be periodically sampled by the PMU CLK code.
     */
    LwU16                       clkMonRefWinUsec;
#endif // PMUCFG_FEATURE_ENABLED(PMU_CLK_CLOCK_MON)

    /*!
     * Clock Domains Structure Version - @ref LW2080_CTRL_CLK_CLK_DOMAIN_VERSION_<xyz>.
     */
    LwU8                        version;

    /*!
     * Set to LW_TRUE if clock domains and all its dependencies are loaded.
     */
    LwBool                      bLoaded;

    /*!
     * Boolean flag that will be use by MODS team to override the domain's
     * OV/OC limits.
     */
    LwBool                      bOverrideOVOC;
    /*!
     * Boolean flag that will be use by client to disable the domain's
     * OC offset including the factory OC offset.
     */
    LwBool                      bDebugMode;
    /*!
     * Boolean indicating whether to enforce VF lwrve increasing monotonicity.
     * If LW_TRUE, RM/PMU will post-process all VF lwrve to make sure that both
     * V and F increase with every subsequent point (including accounting for
     * the various offsets).
     */
    LwBool                      bEnforceVfMonotonicity;
    /*!
     * Boolean indicating whether to enforce VF lwrve smoothening to reduce
     * large discountinuities.
     * If LW_TRUE, RM/PMU will post-process all VF lwrve to make sure that large
     * frequency jumps in VF lwrve will be reduced (including accounting for the
     * various offset).
     */
    LwBool                      bEnforceVfSmoothening;
    /*!
     * Boolean indicating whether to use the grdFreqOffset value.
     * Final Offset =
     *     MAX(AIC factory offset, (bGrdFreqOCEnabled ? grdFreqOffset : 0))
     *     + End user applied frequency offset
     */
    LwBool                      bGrdFreqOCEnabled;
    /*!
     * Represent the maximum number of Voltage rails supported
     */
    LwU8                        voltRailsMax;

#if PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)
    /*!
     * VFE index to get the XBAR boost required for MCLK switch.
     */
    LwBoardObjIdx               xbarBoostVfeIdx;
#endif // PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_XBAR_BOOST_FOR_MCLK)
    /*!
     * Global delta for all the CLK Domains
     */
    CLK_DOMAIN_DELTA            deltas;

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_CLIENT_INDEX_MAP)
    /*!
     * Mapping of CLIENT_CLK_DOMAIN boardobj indexes to CLK_DOMAIN boardobj
     * indexes. Used in PMU for translations on PMU profiles where client clk
     * domains boardobjgrp is not available.
     */
    LwU8                        clientToInternalIdxMap[LW2080_CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS];
#endif
} CLK_DOMAINS;

/*!
 * @memberof CLK_DOMAINS
 *
 * @brief Accessor function which provides a pointer to the CLK_DOMAIN structure
 * for the requested domain (as represented by the @ref RM_PMU_CLK_CLKWHICH
 * enumeration).
 *
 * @param[in] domain  RM_PMU_CLK_CLKWHICH domain requested
 *
 * @return Pointer to CLK_DOMAIN structure
 *     If requested domain is supported on this GPU.
 * @return NULL
 *     If requested domain is not supported on this GPU.
 */
#define ClkDomainsGetByDomain(fname) CLK_DOMAIN * (fname)(RM_PMU_CLK_CLKWHICH domain)

/*!
 * @memberof CLK_DOMAINS
 *
 * @brief Accessor function which provides a pointer to the CLK_DOMAIN structure
 * for the requested domain (as represented by the @ref LW2080_CTRL_CLK_DOMAIN_<XZY>
 * enumeration).
 *
 * @param[in] domain  LW2080_CTRL_CLK_DOMAIN_<XZY> domain requested
 *
 * @return Pointer to CLK_DOMAIN structure
 *     If requested domain is supported on this GPU.
 * @return NULL
 *     If requested domain is not supported on this GPU.
 */
#define ClkDomainsGetByApiDomain(fname) CLK_DOMAIN * (fname)(LwU32 apiDomain)

/*!
 * @memberof CLK_DOMAINS
 *
 * @brief Accessor function which provides clock domain index for
 * the requested domain (as represented by the @ref LW2080_CTRL_CLK_DOMAIN_<XZY>
 * enumeration).
 *
 * @param[in]  domain  LW2080_CTRL_CLK_DOMAIN_<XZY> domain requested
 * @param[out] pIndex  Clock domain index for the requested domain
 *
 * @return FLCN_OK
 *     If the clock domain index for the requested domain is found.
 * @return FLCN_ERROR
 *     If the clock domain index for the requested domain is NOT found.
 */
#define ClkDomainsGetIndexByApiDomain(fname) FLCN_STATUS (fname)(LwU32 apiDomain, LwU32 *pIndex)

/*!
 * @memberof CLK_DOMAINS
 *
 * @brief Accessor function which provides CLK_DOMAIN index for the requested domain
 * group index (as represented by the @ref RM_PMU_DOMAIN_GROUP_<XZY> enumeration).
 *
 * @param[in] domainGrpIdx  RM_PMU_DOMAIN_GROUP_<XZY> requested
 *
 * @return  CLK_DOMAIN index
 *     If requested domain group index is supported on this GPU.
 * @return LW_U8_MAX
 *     If requested domain is not supported on this GPU.
 */
#define ClkDomainsGetIdxByDomainGrpIdx(fname) LwU8 (fname)(LwU8 domainGrpIdx)

/*!
 * @memberof CLK_DOMAINS
 *
 * @brief Caches the CLK_DOMAIN objects with their latest values from their
 * respective VFE Equations and applied over clocking offsets.
 *
 * This interface is expected to be called form the VFE polling loop and
 * all other code path that could tweak the VF lwrve such as over
 * clocking of clk domain, programming and/or VF point.
 *
 * @param[in] bVFEEvalRequired  Whether VFE Evaluation is required?
 *
 * @return FLCN_OK
 *     CLK_DOMAIN objects successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkDomainsCache(fname) FLCN_STATUS (fname)(LwBool bVFEEvalRequired)

/*!
 * @memberof CLK_DOMAINS
 *
 * @brief Caches the CLK_DOMAIN objects with their client provided temperature
 * values (evaluated via the hash of cache index) from their respective VFE Equations.
 *
 * This interface is expected to be called form the worker task running at
 * lower priority to cache the base VF lwrves across the POR defined temperature
 * range.
 *
 * @param[in] cacheIdx  VF lwrve cache buffer index (temperature / step size).
 *
 * @return FLCN_OK
 *     CLK_DOMAIN objects successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkDomainsBaseVFCache(fname) FLCN_STATUS (fname)(LwU8 cacheIdx)

/*!
 * @memberof CLK_DOMAINS
 *
 * @brief Caches the CLK_DOMAIN objects with their client provided offset
 * values.
 *
 *
 * @return FLCN_OK
 *     CLK_DOMAIN objects successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkDomainsOffsetVFCache(fname) FLCN_STATUS (fname)(void)

/*!
 * @memberof CLK_DOMAINS
 *
 * @brief Load the CLK_DOMAIN objects and all its dependencies (CLK_PROG,
 * CLK_VF_POINTS).
 *
 * @return FLCN_OK
 *     CLK_DOMAIN objects successfully loaded.
 * @return Other errors
 *     Unexpected errors oclwrred during loading.
 */
#define ClkDomainsLoad(fname) FLCN_STATUS (fname)(void)

/*!
 * @memberof CLK_DOMAIN
 *
 * @brief Given a clock domain and frequency in MHz, return the source
 * @ref LW2080_CTRL_CLK_PROG_1X_SOURCE_<XYZ> for that frequency by looking up
 * the valid clock programming table entries pointed by that clock domain in
 * the clocks table.
 *
 * @param[in]  pDomain     CLK_DOMAIN pointer
 * @param[in]  freqMHz     Clock frequency in MHz
 *
 * @return LW2080_CTRL_CLK_PROG_1X_SOURCE_<XYZ> if the source is found
 * @return LW2080_CTRL_CLK_PROG_1X_SOURCE_ILWALID otherwise
 */
#define ClkDomainGetSource(fname) LwU8 (fname)(CLK_DOMAIN *pDomain, LwU32 freqMHz)

/*!
 * @memberof CLK_DOMAIN
 *
 * Load the clock domain object and all its all its dependencies (CLK_PROG, CLK_VF_POINTS).
 *
 * @param[in]   pDomain   CLK_DOMAIN pointer
 *
 * @return FLCN_OK
 *     CLK_DOMAIN successfully loaded.
 * @return Other errors
 *     Unexpected errors oclwrred during loading.
 */
#define ClkDomainLoad(fname) FLCN_STATUS (fname)(CLK_DOMAIN *pDomain)

/*!
 * @memberof CLK_DOMAIN
 *
 * Evaluates and Caches the latest VF lwrve for this clock domain.
 *
 * This interface only supported on primary clock domains.
 *
 * @param[in]   pDomain             CLK_DOMAIN pointer
 * @param[in]   bVFEEvalRequired    Whether VFE Evaluation is required?
 *
 * @return FLCN_OK
 *     CLK_DOMAIN successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkDomainCache(fname) FLCN_STATUS (fname)(CLK_DOMAIN *pDomain, LwBool bVFEEvalRequired)

/*!
 * @memberof CLK_DOMAIN
 *
 * Evaluates and Caches the base VF lwrve for this clock domain for given
 * client input temperature value (evaluated via the hash of cache index).
 *
 * This interface only supported on primary clock domains.
 *
 * @param[in]   pDomain     CLK_DOMAIN pointer
 * @param[in]   cacheIdx    VF lwrve cache buffer index (temperature / step size).
 *
 * @return FLCN_OK
 *     CLK_DOMAIN successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkDomainBaseVFCache(fname) FLCN_STATUS (fname)(CLK_DOMAIN *pDomain, LwU8 cacheIdx)

/*!
 * @memberof CLK_DOMAIN
 *
 * Evaluates and Caches the offset VF lwrve for this clock domain
 *
 * This interface only supported on primary clock domains.
 *
 * @param[in]   pDomain     CLK_DOMAIN pointer
 *
 * @return FLCN_OK
 *     CLK_DOMAIN successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkDomainOffsetVFCache(fname) FLCN_STATUS (fname)(CLK_DOMAIN *pDomain)

/*!
 * @memberof CLK_DOMAIN
 *
 * @brief Evaluates if this clock domain is of type _FIXED.
 *
 * @param[in]   pDomain             CLK_DOMAIN pointer
 *
 * @return LW_TRUE   If the CLK_DOMAIN is type _FIXED.
 * @return LW_FALSE  Otherwise
 */
#define ClkDomainIsFixed(fname) LwBool (fname)(CLK_DOMAIN *pDomain)

/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler  (clkDomainBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpSet");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet (clkDomainGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkDomainGrpIfaceModel10ObjSet_SUPER");

// CLK_DOMAINS interfaces
ClkDomainsGetByDomain           (clkDomainsGetByDomain)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainsGetByDomain");
ClkDomainsGetByApiDomain        (clkDomainsGetByApiDomain)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainsGetByApiDomain");
ClkDomainsGetIndexByApiDomain   (clkDomainsGetIndexByApiDomain)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainsGetIndexByApiDomain");
ClkDomainsGetIdxByDomainGrpIdx  (clkDomainsGetIdxByDomainGrpIdx)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainsGetIdxByDomainGrpIdx");
ClkDomainsCache                 (clkDomainsCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainsCache");
ClkDomainsOffsetVFCache           (clkDomainsOffsetVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainsOffsetVFCache");
ClkDomainsBaseVFCache           (clkDomainsBaseVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkDomainsBaseVFCache");
ClkDomainsLoad                  (clkDomainsLoad)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkDomainsLoad");
ClkDomainGetSource              (clkDomainGetSource)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainGetSource");
FLCN_STATUS clkDomainsClkListSanityCheck(LW2080_CTRL_CLK_CLK_DOMAIN_LIST *pClkList, LW2080_CTRL_VOLT_VOLT_RAIL_LIST *pVoltList, LwBool bVfPointCheckIgnore)
    GCC_ATTRIB_SECTION("imem_perfDaemonChangeSeq", "clkDomainsClkListSanityCheck");

// CLK_DOMAIN interface
mockable ClkDomainLoad          (clkDomainLoad)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkDomainLoad");
mockable ClkDomainIsFixed                (clkDomainIsFixed)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkDomainIsFixed");

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief      Colwert a CLIENT_CLK_DOMAIN boardobj index to it's corresponding
 *             CLK_DOMAIN boardobj index.
 *
 * @param      pClkDomains     Pointer to CLK_DOMAINS boardobjgrp
 * @param[in]  clientIndex     The CLIENT_CLK_DOMAIN boardobj index
 * @param[out] pInternalIndex  The CLK_DOMAIN boardobj index
 *
 * @return     FLCN_ERR_NOT_SUPPORTED - if PMU_CLK_CLK_DOMAIN_CLIENT_INDEX_MAP
 *                                      feature is not supported.
 * @return     FLCN_ERR_ILWALID_ARGUMENT - if NULL provided null pointer inputs.
 * @return     FLCN_ERR_ILWALID_INDEX - if given invalid index.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
clkDomainClientIndexToInternalIndex
(
    CLK_DOMAINS    *pClkDomains,
    LwU8            clientIndex,
    LwU8           *pInternalIndex
)
{
    FLCN_STATUS status;

    PMU_ASSERT_TRUE_OR_GOTO(status,
        ((pClkDomains    != NULL) &&
         (pInternalIndex != NULL)),
        FLCN_ERR_ILWALID_ARGUMENT,
        clkDomainClientIndexToInternalIndex_exit);

    PMU_ASSERT_TRUE_OR_GOTO(status,
        clientIndex < LW2080_CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS,
        FLCN_ERR_ILWALID_INDEX,
        clkDomainClientIndexToInternalIndex_exit);

#if PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_CLIENT_INDEX_MAP)
    *pInternalIndex = pClkDomains->clientToInternalIdxMap[clientIndex];

    PMU_ASSERT_TRUE_OR_GOTO(status,
        *pInternalIndex != LW2080_CTRL_CLK_CLK_DOMAIN_INDEX_ILWALID,
        FLCN_ERR_ILWALID_INDEX,
        clkDomainClientIndexToInternalIndex_exit);
#else
    PMU_TRUE_BP();
    status = FLCN_ERR_NOT_SUPPORTED;
    goto clkDomainClientIndexToInternalIndex_exit;
#endif

clkDomainClientIndexToInternalIndex_exit:
    return status;
}

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_domain_3x.h"

#endif // G_CLK_DOMAIN_H
#endif // CLK_DOMAIN_H
