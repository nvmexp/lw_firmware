/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#ifndef CLK_VF_REL_H
#define CLK_VF_REL_H

/*!
 * @file clk_vf_rel.h
 * @brief @copydoc clk_vf_rel.c
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "boardobj/boardobjgrp.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct CLK_VF_POINT_40      CLK_VF_POINT_40;
typedef struct CLK_VF_POINT_40_VOLT CLK_VF_POINT_40_VOLT;

/* ------------------------ Macros ----------------------------------------- */
/*!
 * Macro to locate CLK_VF_RELS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))
#define BOARDOBJGRP_DATA_LOCATION_CLK_VF_REL                                  \
    (&Clk.vfRels.super.super)
#else
#define BOARDOBJGRP_DATA_LOCATION_CLK_VF_REL                                  \
    (NULL)
#endif

/*!
 * @brief       Accessor for a CLK_VF_REL BOARDOBJ by BOARDOBJGRP index.
 *
 * @param[in]   _idx BOARDOBJGRP index for a CLK_VF_REL BOARDOBJ
 *
 * @return      Pointer to a CLK_VF_REL object at the provided BOARDOBJGRP index.
 *
 * @memberof    CLK_VF_REL
 *
 * @public
 */
#define CLK_VF_REL_GET(_idx)                                                  \
    (BOARDOBJGRP_OBJ_GET(CLK_VF_REL, (_idx)))

/*!
 * Accessor macro for CLK_VF_RELS::vfEntryCountSec
 *
 * @return @ref CLK_VF_RELS::vfEntryCountSec
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_VF_REL))
#define clkVfRelsVfEntryCountSecGet()                                         \
    (Clk.vfRels.vfEntryCountSec)
#else
#define clkVfRelsVfEntryCountSecGet()                                         \
    (NULL)
#endif

/*!
 * Accessor macro for @ref vfPointIdxFirst based on input lwrve index.
 *
 * @param[in] pVfRel    CLK_VF_REL pointer
 * @param[in] lwrveIdx  CLK DOMAIN VF Lwrve index
 *
 * @return First VF Point index.
 */
#define clkVfRelVfPointIdxFirstGet(pVfRel, lwrveIdx)                          \
    (((lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?            \
        ((pVfRel)->vfEntryPri.vfPointIdxFirst) :                              \
        ((pVfRel)->vfEntriesSec[((lwrveIdx) - LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_SEC_0)].vfPointIdxFirst))

/*!
 * Accessor macro for @ref vfPointIdxLast based on input lwrve index.
 *
 * @param[in] pVfRel    CLK_VF_REL pointer
 * @param[in] lwrveIdx  CLK DOMAIN VF Lwrve index
 *
 * @return Last VF Point index.
 */
#define clkVfRelVfPointIdxLastGet(pVfRel, lwrveIdx)                           \
    (((lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?            \
        ((pVfRel)->vfEntryPri.vfPointIdxLast) :                               \
        ((pVfRel)->vfEntriesSec[((lwrveIdx) - LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_SEC_0)].vfPointIdxLast))

/*!
 * Accessor macro to get the VFE equation index based on lwrve index.
 *
 * @param[in] pVfRel    CLK_VF_REL pointer
 * @param[in] lwrveIdx  CLK DOMAIN VF Lwrve index
 *
 * @return VFE equation index based on rail index and lwrve index
 */
#define clkVfRelVfeIdxGet(pVfRel, lwrveIdx)                                   \
    (((lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?            \
        ((pVfRel)->vfEntryPri.vfeIdx) :                                       \
        ((pVfRel)->vfEntriesSec[((lwrveIdx) - LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_SEC_0)].vfeIdx))

/*!
 * Accessor macro to get the CPM max frequency offset VFE equation index.
 *
 * @param[in] pVfRel    CLK_VF_REL pointer
 *
 * @return CPM Max VFE equation index based on rail index
 */
#define clkVfRelCpmMaxFreqOffsetVfeIdxGet(pVfRel)                             \
        ((pVfRel)->vfEntryPri.cpmMaxFreqOffsetVfeIdx)

/*!
 * Accessor macro to get the DVCO Offset code VFE equation index lwrve index.
 *
 * @param[in] pVfRel    CLK_VF_REL pointer
 * @param[in] lwrveIdx  CLK DOMAIN VF Lwrve index
 *
 * @return DVCO Offset code VFE equation index based on rail index and lwrve index
 */
#define clkVfRelDvcoOffsetVfeIdxGet(pVfRel, lwrveIdx)                         \
    (((lwrveIdx) == LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_PRI) ?            \
        PMU_PERF_VFE_EQU_INDEX_ILWALID :                                      \
        ((pVfRel)->vfEntriesSec[((lwrveIdx) - LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_SEC_0)].dvcoOffsetVfeIdx))

/*!
 * Accessor macro for max frequency of this CLK_VF_REL.
 *
 * @param[in] pVfRel    CLK_VF_REL pointer
 *
 * @return CLK_VF_REL::freqMaxMHz
 */
#define clkVfRelFreqMaxMHzGet(pVfRel)                                         \
    ((pVfRel)->freqMaxMHz)

/*!
 * Accessor macro for offset adjusted max frequency of this CLK_VF_REL.
 *
 * @param[in] pVfRel    CLK_VF_REL pointer
 *
 * @return CLK_VF_REL::offsettedFreqMaxMHz
 */
#define clkVfRelOffsettedFreqMaxMHzGet(pVfRel)                                \
    ((pVfRel)->offsettedFreqMaxMHz)

/*!
 * Accessor macro for CLK_VF_REL::freqDelta
 *
 * @param[in] pVfRel     CLK_VF_REL pointer
 *
 * @return pointer to @ref CLK_VF_REL::freqDelta
 */
#define clkVfRelFreqDeltaGet(pVfRel)                                          \
    (&(pVfRel)->freqDelta)

/*!
 * Accessor macro for CLK_VF_REL::voltDeltauV
 *
 * @param[in] pVfRel    CLK_VF_REL pointer
 *
 * @return @ref CLK_VF_REL::voltDeltauV
 */
#define clkVfRelVoltDeltauVGet(pVfRel)                                        \
    ((pVfRel)->voltDeltauV)

/*!
 * Accessor macro for CLK_VF_REL::bOCOVEnabled
 *
 * @param[in] pVfRel    CLK_VF_REL pointer
 *
 * @return @ref CLK_VF_REL::bOCOVEnabled
 */
#define clkVfRelOVOCEnabled(pVfRel)                                           \
    (clkDomainsOverrideOVOCEnabled() || (pVfRel)->bOCOVEnabled)

/*!
 * Accessor macro for CLK_VF_REL::railIdx
 *
 * @param[in] _pVfRel    CLK_VF_REL pointer
 *
 * @return @ref CLK_VF_REL::railIdx
 */
#define clkVfRelVoltRailIdxGet(_pVfRel)                                       \
    ((_pVfRel)->railIdx)

/* ------------------------ Datatypes -------------------------------------- */
/*!
 * Clock Vf Relationship params.
 * Struct containing params common across all types of VF relationship.
 */
struct CLK_VF_REL
{
    /*!
     * BOARDOBJ super class. Must always be the first element in the structure.
     */
    BOARDOBJ    super;
    /*!
     * Index of the VOLTAGE_RAIL for this CLK_VF_REL object.
     */
    LwU8        railIdx;
    /*!
     * Boolean flag indicating whether this entry supports OC/OV when those
     * settings are applied to the corresponding CLK_DOMAIN object.
     */
    LwBool      bOCOVEnabled;

    /*!
     * Maximum frequency for this CLK_VF_REL entry. Entries for a given domain
     * need to be specified in ascending maxFreqMhz.
     */
    LwU16       freqMaxMHz;

    /*!
     * @ref freqMaxMHz + applied OC adjustments
     *
     * @note This is NOT enabled on PASCAL due to RM - PMU sync issues.
     */
    LwU16       offsettedFreqMaxMHz;

    /*!
     * This will give the deviation of given voltage from it's nominal value.
     */
    LwS32       voltDeltauV;

    /*!
     * Primary VF lwrve params.
     */
    LW2080_CTRL_CLK_CLK_VF_REL_VF_ENTRY_PRI vfEntryPri;

    /*!
     * Array of secondary VF lwrve's param.
     * Indexed per the LW2080_CTRL_CLK_CLK_VF_REL_VF_LWRVE_IDX_SEC_<X enumeration.
     *
     * Has valid indexes in the range [0, @ref
     * LW2080_CTRL_CLK_CLK_VF_RELS_INFO::vfEntryCountSec).
     */
    LW2080_CTRL_CLK_CLK_VF_REL_VF_ENTRY_SEC
        vfEntriesSec[LW2080_CTRL_CLK_CLK_VF_REL_VF_ENTRY_SEC_MAX];

    /*!
     * This will give the deviation of given freq from it's nominal value.
     */
    LW2080_CTRL_CLK_FREQ_DELTA freqDelta;
};

/*!
 * Clock Vf Relationships param.
 */
typedef struct
{
    /*!
     * BOARDOBJGRP_E255 super class.  Must always be the first element in the
     * structure.
     */
    BOARDOBJGRP_E255 super;

    /*!
     * Number of Secondary entries per VF Rel entry.
     */
    LwU8 secondaryEntryCount;

    /*!
     * Count of secondary VF entries.
     */
    LwU8 vfEntryCountSec;
} CLK_VF_RELS;

/*!
 * @interface CLK_VF_REL
 *
 * Load CLK_VF_REL object all its generated VF points.
 *
 * @param[in]   pVfRel          CLK_VF_REL pointer
 * @param[in]   pDomain40Prog   CLK_DOMAIN_40_PRIMARY pointer
 * @param[in]   lwrveIdx        VF lwrve index.
 *
 * @return FLCN_OK
 *     CLK_VF_REL successfully loaded.
 * @return Other errors
 *     Unexpected errors oclwrred during loading.
 */
#define ClkVfRelLoad(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 lwrveIdx)

/*!
 * VF look-up function which takes an input voltage value and finds the
 * corresponding frequency on the requested VF REL's generated VF lwrve.
 *
 * @param[in]  pVfRel           CLK_VF_REL pointer
 * @param[in]  pDomain40Prog
 *     CLK_DOMAIN_40_PROG pointer which references this CLK_VF_REL object.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the voltage value to look-up.
 * @param[out] pOutput
 *     Pointer to structure in which to return the frequency value corresponding
 *     to the input voltage value.
 *
 * @return FLCN_OK
 *     The input voltage was successfully translated to an output frequency.
 * @return FLCN_ERR_ITERATION_END
 *     This CLK_VF_REL encountered VF points above the @ref pInput->value.
 *     This means the search is done and can be short-cirlwited. The calling
 *     functions should stop their search. This is not an error, and the
 *     callers should colwert this code back to FLCN_OK.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkVfRelVoltToFreq(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput)

/*!
 * VF look-up function which takes an input frequency value on a given
 * voltage rail (@ref VOLT_RAIL) and finds the corresponding voltage on
 * the requested VF REL's generated VF lwrve.
 *
 * @param[in]  pVfRel           CLK_VF_REL pointer
 * @param[in]  pDomain40Prog
 *     CLK_DOMAIN_40_PROG pointer which references this CLK_VF_REL object.
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the frequency value to look-up.
 * @param[out] pOutput
 *     Pointer to structure in which to return the voltage value corresponding
 *     to the input frequency value.
 *
 * @return FLCN_OK
 *     The input frequency was successfully translated to an output voltage.
 * @return FLCN_ERR_ITERATION_END
 *     This CLK_VF_REL encountered VF points above the @ref pInput->value.
 *     This means the search is done and can be short-cirlwited. The calling
 *     functions should stop their search. This is not an error, and the
 *     callers should colwert this code back to FLCN_OK.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkVfRelFreqToVolt(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_OUTPUT *pOutput)

/*!
 * @interface CLK_VF_REL
 *
 * Translates the frequency on the PRIMARY CLK_DOMAIN to the corresponding value
 * on the SECONDARY CLK_DOMAIN per the semantics of the object's primary-secondary
 * relationship. For example, for the _RATIO class this will do freq_{secondary} =
 * freq_{primary} * ratio and for the _TABLE class this will do the table lookup.
 *
 * @note This interface does not perform frequency offset adjustments. It just
 *       colwert the input primary frequency to secondary frequency based on their
 *       primary -> secondary relationship and quantize down the output secondary freq.
 *       This interface also DO NOT bound the quantized frequency to POR ranges.
 *
 * @param[in]     pVfRel   CLK_VF_REL pointer
 * @param[in]     secondaryIdx
 *     Index of the secondary clk domain for which to look-up.
 * @param[in/out] pFreqMHz
 *     Pointer in which caller specifies the input frequency to translate and in
 *     which the function will return the translated frequency.
 * @param[in]     bQuantize
 *     Whether clients wants quantized vs un-quantized secondary frequency output. In
 *     general VF lwrve generation will prefer unquantized output as the frequency
 *     bounds are yet to be determined.
 *
 * @return FLCN_OK
 *     Frequency successfully translated per the primary->secondary relationship.
 * @return FLCN_ERR_NOT_SUPPORTED
 *     This CLK_VF_REL object does not implement this interface. This
 *     is a coding error.
 */
#define ClkVfRelFreqTranslatePrimaryToSecondary(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, LwU8 secondaryIdx, LwU16 *pFreqMHz, LwBool bQuantize)

/*!
 * @interface CLK_VF_REL
 *
 * Translates the frequency on the PRIMARY CLK_DOMAIN to the corresponding value
 * on the SECONDARY CLK_DOMAIN per the semantics of the object's primary-secondary
 * relationship of the offset VF tuple. For example, for the _RATIO class this 
 * will do freq_{secondary} = freq_{primary} * ratio and for the _TABLE class this 
 * will do the table lookup.
 *
 * @note This interface does not perform frequency offset adjustments. It just
 *       colwert the input primary frequency to secondary frequency based on their
 *       primary -> secondary relationship.
 *       This interface also DOES NOT quantize frequency to POR ranges.
 *
 * @param[in]     pVfRel   CLK_VF_REL pointer
 * @param[in]     secondaryIdx
 *     Index of the secondary clk domain for which to look-up.
 * @param[in/out] pFreqMHz
 *     Pointer in which caller specifies the signed input frequency to translate and in
 *     which the function will return the translated frequency.
 * @param[in]     bQuantize
 *     Whether clients wants quantized vs un-quantized secondary frequency output. In
 *     general VF lwrve generation will prefer unquantized output as the frequency
 *     bounds are yet to be determined.
 *
 * @return FLCN_OK
 *     Frequency successfully translated per the primary->secondary relationship.
 * @return FLCN_ERR_NOT_SUPPORTED
 *     This CLK_VF_REL object does not implement this interface. This
 *     is a coding error.
 */
#define ClkVfRelOffsetVFFreqTranslatePrimaryToSecondary(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, LwU8 secondaryIdx, LwS16 *pFreqMHz, LwBool bQuantize)

/*!
 * @interface CLK_VF_REL
 *
 * Translates the frequency on the SECONDARY CLK_DOMAIN to the corresponding value
 * on the PRIMARY CLK_DOMAIN per the semantics of the object's primary-secondary
 * relationship.
 *
 * @note This interface assumes that the input frequency is not adjusted with
 *       any frequency offsets. It also does not perform frequency offset adjustments.
 *
 * @param[in]     pVfRel   CLK_VF_REL pointer
 * @param[in]     secondaryIdx
 *     Index of the secondary clk domain for which to look-up.
 * @param[in/out] pFreqMHz
 *     Pointer in which caller specifies the input frequency to translate and in
 *     which the function will return the translated frequency.
 *
 * @return FLCN_OK
 *     Frequency successfully translated per the primary->secondary relationship.
 * @return FLCN_ERR_NOT_SUPPORTED
 *     This CLK_VF_REL object does not implement this interface. This
 *     is a coding error.
 */
#define ClkVfRelFreqTranslateSecondaryToPrimary(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, LwU8 secondaryIdx, LwU16 *pFreqMHz)

/*!
 * @interface CLK_VF_REL
 *
 * Caches the dynamically evaluated VFE Equation values for this
 * CLK_VF_REL object. This interface will also perform necessary
 * adjustment to the VF lwrve based on OVOC.
 *
 * Iterates over the set of CLK_VF_POINTs pointed to by this CLK_VF_REL
 * and caches their VF values. Ensures monotonically increasing by always
 * returning the last VF value in the @ref pVfPoint40Last param.
 *
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     bVFEEvalRequired  Whether VFE Evaluation is required?
 * @param[in]     lwrveIdx          Vf lwrve index.
 * @param[in/out] pVfPoint40Last
 *      Pointer to CLK_VF_POINT_40 structure containing the last VF point
 *      evaluated by previous CLK_VF_REL objects, and in which this
 *      CLK_VF_REL object will return latest evaluated VF point.
 *
 * @return FLCN_OK
 *     CLK_VF_REL successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfRelCache(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwBool bVFEEvalRequired, LwU8 lwrveIdx, CLK_VF_POINT_40 *pVfPoint40Last)

/*!
 * @interface CLK_VF_REL
 *
 * Caches the necessary adjust values to the VF lwrve based on OVOC for this
 * CLK_VF_REL object. This interface does not evaluate VFE Equation values
 *
 * Iterates over the set of CLK_VF_POINTs pointed to by this CLK_VF_REL
 * and caches their VF values. Ensures monotonically increasing by always
 * returning the last VF value in the @ref pVfPoint40Last param.
 *
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     lwrveIdx          Vf lwrve index.
 * @param[in/out] pVfPoint40Last
 *      Pointer to CLK_VF_POINT_40 structure containing the last VF point
 *      evaluated by previous CLK_VF_REL objects, and in which this
 *      CLK_VF_REL object will return latest evaluated VF point.
 *
 * @return FLCN_OK
 *     CLK_VF_REL successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfRelOffsetVFCache(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 lwrveIdx, CLK_VF_POINT_40 *pVfPoint40Last)

/*!
 * @interface CLK_VF_REL
 *
 * Caches the dynamically evaluated VFE Equation values for this
 * CLK_VF_REL object. This interface does not perform any OVOC
 * adjustment to the base VF lwrve.
 *
 * Iterates over the set of CLK_VF_POINTs pointed to by this CLK_VF_REL
 * and caches their VF values. Ensures monotonically increasing by always
 * returning the last VF value in the @ref pVfPoint40Last param.
 *
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     lwrveIdx          VF lwrve index.
 * @param[in]     cacheIdx          VF lwrve cache buffer index (temperature / step size).
 * @param[in/out] pVfPoint40Last
 *      Pointer to CLK_VF_POINT_40 structure containing the last VF point
 *      evaluated by previous CLK_VF_REL objects, and in which this
 *      CLK_VF_REL object will return latest evaluated VF point.
 *
 * @return FLCN_OK
 *     CLK_VF_REL successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfRelBaseVFCache(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 lwrveIdx, LwU8 cacheIdx, CLK_VF_POINT_40 *pVfPoint40Last)

/*!
 * @interface CLK_VF_REL
 *
 * Iterates over the set of CLK_VF_POINTs pointed to by this CLK_VF_REL
 * and smoothen their VF values. Ensures that the discontinuity between two
 * conselwtive VF points are within the max allowed bound.
 *
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     lwrveIdx          Vf lwrve index.
 * @param[in]     cacheIdx
 *      VF lwrve cache buffer index (temperature / step size). If the cacheIdx
 *      is marked "CLK_CLK_VF_POINT_VF_CACHE_IDX_ILWALID", it represents to use
 *      the main active buffers instead of cache buffers.
 * @param[in/out] pVfPoint40VoltLast
 *      Pointer to CLK_VF_POINT_40_VOLT structure containing the last VF point
 *      evaluated by previous CLK_VF_REL objects, and in which this
 *      CLK_VF_REL object will return latest evaluated VF point.
 *
 * @return FLCN_OK
 *     CLK_VF_REL successfully smoothen.
 * @return Other errors
 *     Unexpected errors oclwrred.
 */
#define ClkVfRelSmoothing(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 lwrveIdx, LwU8 cacheIdx, CLK_VF_POINT_40_VOLT *pVfPoint40VoltLast)

/*!
 * @interface CLK_VF_REL
 *
 * Cache secondary clock domains VF lwrve using cached primary clock domain
 * VF lwrve. This interface will also perform necessary adjustment to
 * the VF lwrve based on OVOC. Iterates over the set of CLK_VF_POINTs
 * pointed to by this CLK_VF_REL and caches their VF values.
 *
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     bVFEEvalRequired  Whether VFE Evaluation is required?
 * @param[in]     lwrveIdx          Vf lwrve index.
 * @param[in/out] pVfPoint40Last    CLK_VF_POINT_40 pointer pointing to the
 *                                  previous clk vf point.
 *
 * @return FLCN_OK
 *     CLK_VF_REL successfully cached.
 * @return Other errors
 *     Unexpected errors oclwred during caching.
 */
#define ClkVfRelSecondaryCache(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwBool bVFEEvalRequired, LwU8 lwrveIdx, CLK_VF_POINT_40 *pVfPoint40Last)

/*!
 * @interface CLK_VF_REL
 *
 * Cache secondary clock domains offset VF lwrve using cached primary clock domain
 * VF lwrve. Iterates over the set of CLK_VF_POINTs pointed to by this
 * CLK_VF_REL and caches their VF values.
 *
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     lwrveIdx          VF lwrve index.
 * @param[in/out] pVfPoint40Last    CLK_VF_POINT_40 pointer pointing to the
 *                                  previous clk vf point.
 *
 * @return FLCN_OK
 *     CLK_VF_REL successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfRelSecondaryOffsetVFCache(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 lwrveIdx, CLK_VF_POINT_40 *pVfPoint40Last)

/*!
 * @interface CLK_VF_REL
 *
 * Cache secondary clock domains base VF lwrve using cached primary clock domain
 * VF lwrve. Iterates over the set of CLK_VF_POINTs pointed to by this
 * CLK_VF_REL and caches their VF values.
 *
 * @param[in]     pVfRel            CLK_VF_REL pointer
 * @param[in]     pDomain40Prog     CLK_DOMAIN_40_PROG pointer
 * @param[in]     lwrveIdx          VF lwrve index.
 * @param[in]     cacheIdx          VF lwrve cache buffer index (temperature / step size).
 * @param[in/out] pVfPoint40Last    CLK_VF_POINT_40 pointer pointing to the
 *                                  previous clk vf point.
 *
 * @return FLCN_OK
 *     CLK_VF_REL successfully cached.
 * @return Other errors
 *     Unexpected errors oclwrred during caching.
 */
#define ClkVfRelSecondaryBaseVFCache(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 lwrveIdx, LwU8 cacheIdx, CLK_VF_POINT_40 *pVfPoint40Last)

/*!
 * @interface CLK_VF_REL
 *
 * Helper interface to get CLK_VF_REL index for given input frequency.
 *
 * @note This interface compares @ref freqMHz against the base (unoffseted)
 *       frequency max of the given VF REL.
 *
 * @param[in]   pVfRel          CLK_VF_REL pointer
 * @param[in]   pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 * @param[in]   freqMHz         Input Frequency (MHz)
 *
 * @return bValidMatch
 *          TRUE    Valid match found.
 *          FALSE   Not a valid match.
 *
 */
#define ClkVfRelGetIdxFromFreq(fname) LwBool (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU16 freqMHz)

/*!
 * @interface CLK_VF_REL
 *
 * Helper interface to apply frequency OC adjustment on input frequency using
 * base and offset adjust VF lwrves.
 *
 * @note This interface assumes all clients belong to primary VF lwrve.
 *
 * @param[in]       pVfRel          CLK_VF_REL pointer
 * @param[in]       pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 * @param[in/out]   pFreqMHz        Input frequency to adjust
 *
 * @return FLCN_OK
 *     Frequency successfully adjusted with the OC offsets.
 * @return Other errors
 *     An unexpected error oclwrred during adjustments.
 */
#define ClkVfRelClientFreqDeltaAdj(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU16 *pFreqMHz)

/*!
 * @interface CLK_VF_REL
 *
 * Helper function to get the max frequency supported by the given VF REL's
 * generated VF lwrve.
 *
 * @param[in]   pVfRel          CLK_VF_REL pointer
 * @param[in]   pDomain40Prog   CLK_DOMAIN_40_PROG pointer
 * @param[out]  pFreqMaxMHz     Output Max Freq value.
 *
 * @return FLCN_OK
 *     Successfully updated the ref@ pFreqMaxMHz with max freq value
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     The caller specified an invalid pointer.
 * @return Other errors
 *     An unexpected error oclwrred during VF look-up.
 */
#define ClkVfRelVfMaxFreqMHzGet(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU16 *pFreqMaxMHz)

/*!
 * @interface CLK_VF_REL
 *
 * Translates an input voltage to a corresponding frequency tuple for
 * this CLK_VF_REL, per the VF lwrve of this CLK_VF_REL.
 *
 * Clients to the VF look-up functions should not directly call this
 * interface. Please call @ref ClkDomain40ProgVoltToFreqTuple
 *
 * @param[in]  pVfRel           CLK_VF_REL pointer
 * @param[in]  pDomain40Prog    CLK_DOMAIN_40_PROG pointer
 * @param[in]  clkVfPointIdx    VF Point Index for V -> F tuple cache.
 * @param[out] pOutput
 *     Pointer to structure in which to return the frequency tuple corresponding
 *     to the input voltage value.
 *
 * @return FLCN_OK
 *     The input voltage was successfully translated to an output frequency tuple.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkVfRelVoltToFreqTuple(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwBoardObjIdx clkVfPointIdx, LW2080_CTRL_CLK_CLK_VF_POINT_LUT_FREQ_TUPLE *pOutput)

/*!
 * @interface CLK_VF_REL
 *
 * Helper interface to get the VF Point index that will be used by
 * @ref ClkVfRelVoltToFreqTuple
 *
 * Clients to the VF look-up functions should not directly call this
 * interface. Please call @ref ClkDomain40ProgVoltToFreqTuple
 *
 * @param[in]  pVfRel           CLK_VF_REL pointer
 * @param[in]  pDomain40Prog    CLK_DOMAIN_40_PROG pointer
 * @param[in]  voltageType
 *     Type of voltage value by which to look-up.  @ref
 *     LW2080_CTRL_CLK_VOLTAGE_TYPE_<xyz>.
 * @param[in]  pInput
 *     Pointer to structure specifying the voltage value to look-up.
 * @param[in/out] pVfIterState
 *     Pointer to structure containing the VF iteration state for the VF
 *     look-up. This tracks all data which must be preserved across this VF
 *     look-up and potentially across subsequent look-ups. In cases where
 *     subsequent searches are in order of increasing input criteria, each
 *     subsequent look-up can "resume" where the previous look-up finished using
 *     this iteration state.
 *
 * @return FLCN_OK
 *     The input voltage was successfully translated to an output frequency tuple.
 * @return Other errors
 *     An unexpected error oclwrred during look-up/translation.
 */
#define ClkVfRelVoltToFreqTupleVfIdxGet(fname) FLCN_STATUS (fname)(CLK_VF_REL *pVfRel, CLK_DOMAIN_40_PROG *pDomain40Prog, LwU8 voltageType, LW2080_CTRL_CLK_VF_INPUT *pInput, LW2080_CTRL_CLK_VF_ITERATION_STATE *pVfIterState)

/*!
 * @interface CLK_VF_REL
 *
 * Interface to check whether the clock VF relationship is noise aware.
 *
 * @param[in]  pVfRel   CLK_VF_REL pointer
 *
 * @return LW_TRUE
 *     If the clock VF relationship is noise aware.
 * @return LW_FALSE
 *     Otherwise
 */
#define ClkVfRelIsNoiseAware(fname) LwBool (fname)(CLK_VF_REL *pVfRel)


/* ------------------------ External Definitions --------------------------- */
/* ------------------------ Function Prototypes ---------------------------- */
// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler   (clkVfRelBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfRelGrpSet");
BoardObjGrpIfaceModel10CmdHandler   (clkVfRelBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libClkStatus", "clkVfRelBoardObjGrpIfaceModel10GetStatus");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet       (clkVfRelGrpIfaceModel10ObjSet_SUPER)
    GCC_ATTRIB_SECTION("imem_libClkConstruct", "clkVfRelGrpIfaceModel10ObjSet_SUPER");

// CLK_VF_REL interfaces
ClkVfRelLoad                    (clkVfRelLoad)
    GCC_ATTRIB_SECTION("imem_vfLoad", "clkVfRelLoad");
ClkVfRelVoltToFreq              (clkVfRelVoltToFreq)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelVoltToFreq");
ClkVfRelFreqToVolt              (clkVfRelFreqToVolt)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelFreqToVolt");
ClkVfRelVoltToFreqTuple         (clkVfRelVoltToFreqTuple)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkVfRelVoltToFreqTuple");
ClkVfRelVoltToFreqTupleVfIdxGet (clkVfRelVoltToFreqTupleVfIdxGet)
    GCC_ATTRIB_SECTION("imem_perfClkAvfs", "clkVfRelVoltToFreqTupleVfIdxGet");
ClkVfRelCache                   (clkVfRelCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelCache");
ClkVfRelOffsetVFCache           (clkVfRelOffsetVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelOffsetVFCache");
ClkVfRelBaseVFCache             (clkVfRelBaseVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelBaseVFCache");
ClkVfRelSmoothing               (clkVfRelSmoothing)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelSmoothing");
ClkVfRelSecondaryCache              (clkVfRelSecondaryCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelSecondaryCache");
ClkVfRelSecondaryBaseVFCache        (clkVfRelSecondaryBaseVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelSecondaryBaseVFCache");
ClkVfRelSecondaryOffsetVFCache      (clkVfRelSecondaryOffsetVFCache)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelSecondaryOffsetVFCache");
ClkVfRelGetIdxFromFreq          (clkVfRelGetIdxFromFreq)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelGetIdxFromFreq");
ClkVfRelVfMaxFreqMHzGet         (clkVfRelVfMaxFreqMHzGet)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelVfMaxFreqMHzGet");
ClkVfRelIsNoiseAware            (clkVfRelIsNoiseAware)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelIsNoiseAware");

ClkVfRelFreqTranslatePrimaryToSecondary          (clkVfRelFreqTranslatePrimaryToSecondary)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelFreqTranslatePrimaryToSecondary");
ClkVfRelOffsetVFFreqTranslatePrimaryToSecondary  (clkVfRelOffsetVFFreqTranslatePrimaryToSecondary)
    GCC_ATTRIB_SECTION("imem_perfVfIlwalidation", "clkVfRelOffsetVFFreqTranslatePrimaryToSecondary");
ClkVfRelFreqTranslateSecondaryToPrimary          (clkVfRelFreqTranslateSecondaryToPrimary)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelFreqTranslateSecondaryToPrimary");
ClkVfRelClientFreqDeltaAdj                  (clkVfRelClientFreqDeltaAdj)
    GCC_ATTRIB_SECTION("imem_perfVf", "clkVfRelClientFreqDeltaAdj");

/* ------------------------ Include Derived Types -------------------------- */
#include "clk/clk_vf_rel_ratio.h"
#include "clk/clk_vf_rel_table.h"

#endif // CLK_VF_REL_H
