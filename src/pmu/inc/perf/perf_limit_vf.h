/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    perf_limit_vf.h
 * @brief   Arbitration interface file.
 */

#ifndef PERF_LIMIT_VF_H
#define PERF_LIMIT_VF_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */

#include "boardobj/boardobjgrpmask.h"

/* ------------------------ Forward Definitions ----------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * @defgroup PERF_LIMITS_VF_RANGE_IDX
 *
 * Enumerations specifying indexes into PERF_LIMITS_VF_RANGE or
 * PERF_LIMITS_PSTATE_RANGE structure.
 *
 * @{
 */
#define PERF_LIMITS_VF_RANGE_IDX_MIN                                           0
#define PERF_LIMITS_VF_RANGE_IDX_MAX                                           1
#define PERF_LIMITS_VF_RANGE_IDX_NUM_IDXS                                      2
/*!@}*/

/*!
 * Constant value specifying the array bound of PERF_PERF_LIMITS_VF_ARRAY::values[]
 */
#define PERF_PERF_LIMITS_VF_ARRAY_VALUES_MAX                                  10

/*!
 * Helper macro to init at @ref PERF_LIMITS_VF_ARRAY struct.
 *
 * @param[in] _pVfArray    Pointer to PERF_LIMITS_VF_ARRAY struct to init.
 */
#define PERF_LIMITS_VF_ARRAY_INIT(_pVfArray)                                   \
    do {                                                                       \
        (_pVfArray)->pMask = NULL;                                             \
        memset((_pVfArray)->values, 0x0,                                       \
            sizeof(LwU32) * PERF_PERF_LIMITS_VF_ARRAY_VALUES_MAX);             \
    } while (LW_FALSE)

/*!
 * Helper macro which initializes a @ref PERF_LIMITS_<XYZ>_RANGE structure from
 * a single value - i.e. @ref _pRange = [ @ref _value, @ref _value ].
 *
 * @param[out]      _pRange
 *     Pointer to @ref PERF_LIMITS_<XYZ>_RANGE to initialize.
 * @param[in]       _value
 *     Value to set in the range.
 */
#define PERF_LIMITS_RANGE_SET_VALUE(_pRange, _value)                            \
    do {                                                                        \
        (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MIN] =                       \
        (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MAX] =                       \
                (_value);                                                       \
    } while (LW_FALSE)

/*!
 * Helper macro which bounds (or "trims") a value to the specified range.
 *
 * @param[in]       _pRange
 *     Pointer to @ref PERF_LIMITS_<XYZ>_RANGE by which to bound.
 * @param[out]      _pValue
 *     Pointer to value to bound.
 */
#define PERF_LIMITS_RANGE_VALUE_BOUND(_pRange, _pValue)                         \
    do {                                                                        \
        *(_pValue) = LW_MAX(*(_pValue),                                         \
            (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MIN]);                   \
        *(_pValue) = LW_MIN(*(_pValue),                                         \
            (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MAX]);                   \
    } while (LW_FALSE)

/*!
 * Helper macro which bounds (or "trims") a range to the specified range.
 *
 * @param[in]       _pRange
 *     Pointer to @ref PERF_LIMITS_<XYZ>_RANGE by which to bound.
 * @param[out]      _pRangeOut
 *     Pointer to value to bound.
 */
#define PERF_LIMITS_RANGE_RANGE_BOUND(_pRange, _pRangeOut)                      \
    do {                                                                        \
        (_pRangeOut)->values[PERF_LIMITS_VF_RANGE_IDX_MIN] =                    \
            LW_MAX(                                                             \
                (_pRangeOut)->values[PERF_LIMITS_VF_RANGE_IDX_MIN],             \
                (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MIN]);               \
        (_pRangeOut)->values[PERF_LIMITS_VF_RANGE_IDX_MAX] =                    \
            LW_MIN(                                                             \
                (_pRangeOut)->values[PERF_LIMITS_VF_RANGE_IDX_MAX],             \
                (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MAX]);               \
    } while (LW_FALSE)

/*!
 * Helper macro to determine if a value is within the specified range.
 *
 * @param[in]       _pRange
 *     Pointer to @ref PERF_LIMITS_<XYZ>_RANGE to test.
 * @param[in]       _value
 *     Value to test.
 *
 * @return LW_TRUE
 *     @ref _value is within @ref _pRange.
 * @return LW_FALSE
 *     @ref _value is not within @ref _pRange.
 */
#define PERF_LIMITS_RANGE_CONTAINS_VALUE(_pRange, _value)                      \
    (((_value) >= (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MIN]) &&          \
     ((_value) <= (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MAX]))

/*!
 * Helper macro to determine if a range is above the specified value, i.e. if
 * the value is below the range.
 *
 * @param[in]       _pRange
 *     Pointer to @ref PERF_LIMITS_<XYZ>_RANGE to test.
 * @param[in]       _value
 *     Value to test.
 *
 * @return LW_TRUE
 *     @ref _pRange is above @ref _value.
 * @return LW_FALSE
 *     @ref _pRange is not above @ref _value.
 */
#define PERF_LIMITS_RANGE_ABOVE_VALUE(_pRange, _value)                          \
    ((_value) < (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MIN])

/*!
 * Helper macro to determine if range is cohererent - i.e. max >= min.
 *
 * @param[in]       _pRange
 *     Pointer to @ref PERF_LIMITS_<XYZ>_RANGE to test.
 *
 * @return LW_TRUE
 *     @ref _pRange is coherent.
 * @return LW_FALSE
 *     @ref _pRange is not coherent.
 */
#define PERF_LIMITS_RANGE_IS_COHERENT(_pRange)                                  \
    ((_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MIN] <=                         \
     (_pRange)->values[PERF_LIMITS_VF_RANGE_IDX_MAX])

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * PERF_LIMITS PSTATE I/O range struture.  Specifying input/outut to PERF_LIMITS
 * PSTATE routines.
 */
typedef struct
{
    /*!
     * Specifies a range of PSTATE indexes.  Indexed by @ref
     * PERF_LIMITS_VF_RANGE_IDX.
     *
     * @note The name of this variable must be kept in sync with @ref
     * PERF_LIMITS_VF_RANGE as well as the @ref PERF_LIMITS_RANGE_<XYZ> macros
     * above.
     */
    LwU32 values[PERF_LIMITS_VF_RANGE_IDX_NUM_IDXS];
} PERF_LIMITS_PSTATE_RANGE;

/*!
 * PERF_LMITS VF I/O structure.  A generic structure for specifying input/output
 * to/from the PERF_LIMITS VF routines.
 */
typedef struct
{
    /*!
     * Specifies the index corresponding to the specified value.  This will be
     * an index into either the CLK_DOMAINS or VOLT_RAILS BOARDOBJGRPs.
     */
    LwU8  idx;
    /*!
     * Specifies the value on the corresponding index.  This be in units of
     * either kHz (CLK_DOMAINS) or uV (VOLT_RAILS).
     */
    LwU32 value;
} PERF_LIMITS_VF;

/*!
 * PERF_LMITS VF I/O Range structure.  A generic structure for specifying
 * input/output to/from the PERF_LIMITS VF routines.
 */
typedef struct
{
    /*!
     * Specifies the index corresponding to the specified value.  This will be
     * an index into either the CLK_DOMAINS or VOLT_RAILS BOARDOBJGRPs.
     */
    LwU8 idx;
    /*!
     * Specifies a range of values for the corresponding index.  This be in
     * units of either kHz (CLK_DOMAINS) or uV (VOLT_RAILS).  Indexed by @ref
     * PERF_LIMITS_VF_RANGE_IDX.
     *
     * @note The name of this variable must be kept in sync with @ref
     * PERF_LIMITS_PSTATE_RANGE as well as the @ref PERF_LIMITS_RANGE_<XYZ>
     * macros above.
     */
    LwU32 values[PERF_LIMITS_VF_RANGE_IDX_NUM_IDXS];
} PERF_LIMITS_VF_RANGE;

/*!
 * PERF_LMITS PROP I/O structure.  A generic structure for specifying input/output
 * to/from the PERF_LIMITS PROP routines.
 */
typedef struct
{
    /*!
     * Specifies the mask of input input/output domains. 
     */
    BOARDOBJGRPMASK_E32 *pMask;
    /*!
     * Specifies input/output values indexed by bit position in @ref pMask.
     */
    LwU32 values[PERF_PERF_LIMITS_VF_ARRAY_VALUES_MAX];
} PERF_LIMITS_VF_ARRAY;

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @interface PERF_LIMITS
 *
 * Public interface to propage frequency from the set of specific input
 * clock domain's frequency to the set of requested output clock domain's
 * frequency.
 *
 * @param[in]     pLimits             PERF_LIMITS pointer
 * @param[in]     pCPstateRange
 *     Pointer to @ref PERF_LIMITS_PSTATE_RANGE struct in which the caller
 *     specifies the range of PSTATEs to consider for quantization.  If @ref
 *     pCPstateRange is NULL, this interface will use the entire PSTATE range. This
 *     argument is passed directly to @ref PerfLimitsFreqkHzQuantize().
 * @param[in]     pCVfArrayIn
 *     Pointer to @ref PERF_LIMITS_VF_ARRAY struct specifying the
 *     propagation input values - @ref PERF_LIMITS_VF_ARRAY::pMask specifies
 *     the CLK_DOMAINS mask, @ref PERF_LIMITS_VF_ARRAY::values specifies
 *     the frequency (kHz).
 * @param[in/out] pVfArrayOut
 *     Pointer to @ref PERF_LIMITS_VF_ARRAY struct in which the caller
 *     specifies the mask of output clock domain for which to look-up as @ref
 *     PERF_LIMITS_VF_ARRAY::pMask.  This interface will return the
 *     corresponding propagated frequency (kHz) in @ref
 *     PERF_LIMITS_VF_ARRAY::values.
 * @param[in]     clkProptopIdx
 *     Index to the clock Propogation Topology
 *
 * @return FLCN_OK
 *     Clock propagation successful.
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsFreqPropagate(fname) FLCN_STATUS (fname)(const PERF_LIMITS_PSTATE_RANGE *pCPstateRange, const PERF_LIMITS_VF_ARRAY *pCVfArrayIn, PERF_LIMITS_VF_ARRAY *pVfArrayOut, LwBoardObjIdx clkPropTopIdx)

/*!
 * @interface PERF_LIMITS
 *
 * Looks up the corresponding minimum (V_{min}) voltage (uV) on a given VOLT_RAIL to
 * support a given frequency (kHz) on a CLK_DOMAIN.  In other words, F->V.
 *
 * @param[in]     pLimits             PERF_LIMITS pointer
 * @param[in]     pCVfDomain
 *     Pointer to @ref PERF_LIMITS_VF struct specifying the
 *     CLK_DOMAIN input values - @ref PERF_LIMITS_VF::idx specifies
 *     the CLK_DOMAIN index, @ref PERF_LIMITS_VF::value specifies
 *     the frequency (kHz).
 * @param[in/out] pVfRail
 *     Pointer to @ref PERF_LIMITS_VF struct in which the caller
 *     specifies the VOLT_RAIL index for which to look-up as @ref
 *     PERF_LIMITS_VF::idx.  This interface will return the
 *     corresponding minimum voltage (uV) in @ref
 *     PERF_LIMITS_VF::value.
 * @param[in]     bDefault
 *     Boolean indicating whether the caller wants the frequency -> voltage
 *     intersection to bound to the "default VF point" (@ref
 *     LW2080_CTRL_CLK_VF_FLAGS_DEFAULT_VF_POINT_SET_TRUE) if no intersection is
 *     found, when the input frequency is above the highest point on the
 *     CLK_DOMAIN's VF lwrve.  If LW_TRUE, the highest point on the CLK_DOMAIN's
 *     VF lwrve will be chosen.
 *
 * @return FLCN_OK
 *     F->V look-up successful.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     @ref pVfDomain->idx specifies an invalid CLK_DOMAIN index.
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsFreqkHzToVoltageuV(fname) FLCN_STATUS (fname)(const PERF_LIMITS_VF *pCVfDomain, PERF_LIMITS_VF *pVfRail, LwBool bDefault)

/*!
 * @interface PERF_LIMITS
 *
 * Looks up the corresponding maximum (F_{max}) frequency (kHz) on a given
 * CLK_DOMAIN which can be supported by a voltage (uV) on a given VOLT_RAIL.  In
 * other words, V->F.
 *
 * @note This interface also takes care of quantizing the interesected frequency
 * to the available PSTATE and VF ranges via @ref PerfLimitsFreqkHzQuantize().
 *
 * @param[in]     pLimits             PERF_LIMITS pointer
 * @param[in]     pCVfRail
 *     Pointer to @ref PERF_LIMITS_VF struct in which the caller specifies the
 *     VOLT_RAIL index for which to look-up as @ref PERF_LIMITS_VF::idx and the
 *     voltage (uV) as @ref PERF_LIMITS_VF::value.
 * @param[in]     pCPstateRange
 *     Pointer to @ref PERF_LIMITS_PSTATE_RANGE struct in which the caller
 *     specifies the range of PSTATEs to consider for quantization.  If @ref
 *     pCPstateRange is NULL, this interface will use the entire PSTATE range.  This
 *     argument is passed directly to @ref PerfLimitsFreqkHzQuantize().
 * @param[in/out] pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF struct in which the caller specifies the
 *     CLK_DOMAIN index for which to look-up as @ref PERF_LIMITS_VF::idx.  The
 *     interface will return the corresponding frequency (kHz) value as @ref
 *     PERF_LIMITS_VF::value.
 * @param[in]     bDefault
 *     Boolean indicating whether the caller wants the voltage -> frequency
 *     intersection to bound to the "default VF point" (@ref
 *     LW2080_CTRL_CLK_VF_FLAGS_DEFAULT_VF_POINT_SET_TRUE) if no intersection is
 *     found, when the input voltage is below the lowest point on the
 *     CLK_DOMAIN's VF lwrve.  If LW_TRUE, the lowest point on the CLK_DOMAIN's
 *     VF lwrve will be chosen If LW_FALSE, will fall back to the minimum
 *     CLK_DOMAIN frequency of the lowest PSTATE in @ref pCPstateRange.
 * @param[in]     bQuantize
 *     Boolean indicating whether the caller wants to quantize the frequency from
 *     the V->F lookup to available PSTATE ranges and VF lwrve.
 *
 * @return FLCN_OK
 *     V->F look-up  and quantization successfull.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     @ref pCVfDomain->idx specifies an invalid CLK_DOMAIN index.
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsVoltageuVToFreqkHz(fname) FLCN_STATUS (fname)(const PERF_LIMITS_VF *pCVfRail, const PERF_LIMITS_PSTATE_RANGE *pCPstateRange, PERF_LIMITS_VF *pVfDomain, LwBool bDefault, LwBool bQuantize)

/*!
 * @interface PERF_LIMITS
 *
 * Quantizes a CLK_DOMAIN frequency (kHz) to a supported frequency for
 * arbitration.  This quantization accounts for the following factors:
 *
 * 1.  VF lwrve frequency max - i.e. the maximum frequency in the VF lwrve.
 *
 * 2.  VF frequency quantization - i.e. the frequency steps specified in the
 * CLK_PROGs for the CLK_DOMAIN.
 *
 * 3.  PSTATE ranges - i.e. to some value within a [PSTATE.min, PSTATE.max]
 * range for the CLK_DOMAIN.  This interface will only attempt to quantize to
 * the set of PSTATEs by @ref pCPstateRange.  If @ref pCPstateRange is NULL,
 * this interface will use the entire PSTATE range.
 *
 * @param[in]     pLimits             PERF_LIMITS pointer
 * @param[in/out] pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF struct specifying the CLK_DOMAIN input
 *     values - @ref PERF_LIMITS_VF::idx specifies the CLK_DOMAIN index, @ref
 *     PERF_LIMITS_VF::value specifies the frequency (kHz) to quantize.  The
 *     interface will return the quantized frequency (kHz) in the same @ref
 *     PERF_LIMITS_VF::value.
 * @param[in]     pCPstateRange
 *     Pointer to @ref PERF_LIMITS_PSTATE_RANGE specifying the range of PSTATEs
 *     to use for quantization.  If NULL, this interfac ewill use entire PSTATE
 *     range.
 * @param[in]     bFloor
 *     Boolean flag indicating whether quantization should round down (LW_TRUE)
 *     or up (LW_FALSE).
 *
 * @return FLCN_OK
 *     Frequency value successfully quantized.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     @ref pVfDomain->idx specifies an invalid CLK_DOMAIN index.
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsFreqkHzQuantize(fname) FLCN_STATUS (fname)(PERF_LIMITS_VF *pVfDomain, const PERF_LIMITS_PSTATE_RANGE *pCPstateRange, LwBool bFloor)

/*!
 * @interface PERF_LIMITS
 *
 * Returns the frequency (kHz) range on a CLK_DOMAIN corresponding to a PSTATE
 * (index) range.  This frequency range is defined by the corresponding PSTATEs'
 * ranges for the CLK_DOMAIN: [P#min.min, P#max.max].
 *
 * Additionally, the frequency range is trimmed to the CLK_DOMAIN's VF lwrve max
 * frequency.
 *
 * @param[in]     pLimits             PERF_LIMITS pointer
 * @param[in]     pCPStateRange
 *     Pointer to @ref PERF_LIMITS_PSTATE_RANGE specifying the
 *     PSTATE range.
 * @param[in/out] pVfDomainRange
 *     Pointer to @ref PERF_LIMITS_VF_RANGE struct in which the caller
 *     specifies the CLK_DOMAIN index for which to look-up as @ref
 *     PERF_LIMITS_VF_RANGE::idx.  This interface will return the
 *     corresponding frequency (kHz) range in
 *     @ref PERF_LIMITS_VF_RANGE::values.
 *
 * @return FLCN_OK
 *     Frequency range look-up successful.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     @ref pVfDomainRange->idx specifies an invalid CLK_DOMAIN index.
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsPstateIdxRangeToFreqkHzRange(fname) FLCN_STATUS (fname)(const PERF_LIMITS_PSTATE_RANGE *pCPstateRange, PERF_LIMITS_VF_RANGE *pVfDomainRange)

/*!
 * @interface PERF_LIMITS
 *
 * Returns a PSTATE (index) range of PSTATEs whose frequency ranges contain a
 * specified CLK_DOMAIN frequency (kHz).
 *
 * @param[in]     pLimits             PERF_LIMITS pointer
 * @param[in]     pCVfDomain
 *     Pointer to @ref PERF_LIMITS_VF struct in which the caller specifies the
 *     CLK_DOMAIN index for which to look-up as @ref PERF_LIMITS_VF::idx and the
 *     frequency (kHz) value as @ref PERF_LIMITS_VF::value.
 * @param[out]     pPStateRange
 *     Pointer to @ref PERF_LIMITS_PSTATE_RANGE in which to return the PSTATE
 *     (index) range.
 *
 * @return FLCN_OK
 *     Pstate range look-up successful.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     @ref pVfDomainRange->idx specifies an invalid CLK_DOMAIN index.
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsFreqkHzToPstateIdxRange(fname) FLCN_STATUS (fname)(const PERF_LIMITS_VF *pCVfDomain, PERF_LIMITS_PSTATE_RANGE *pPstateRange)

/*!
 * @interface PERF_LIMITS
 *
 * Returns a mask of CLK_DOMAINs specified within a given VPSTATE, as specified
 * by its VPSTATE index.
 *
 * @param[in]     pLimits             PERF_LIMITS pointer
 * @param[in]     vpstateIdx
 *     Index of @ref VPSTATE object.
 * @param[in]     pArbOutput
 *     Pointer to @ref PERF_LIMITS_ARBITRATION_OUTPUT which provides CLK_DOMAIN
 *     mask to intersect with VPSTATE mask (i.e. to make sure non-arbitrated
 *     CLK_DOMAINs won't be accidentally included).
 * @param[out]    pClkDomainsMask
 *     Pointer to @ref BOARDOBJGRPMASK_E32 in which to return the mask of @ref
 *     CLK_DOMAINs specified by the given @ref VPSTATE.
 *
 * @return FLCN_OK
 *     @ref CLK_DOMAIN mask successfully retrieved.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     @ref vspstateIdx specifies an invalid VPSTATE index.
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsVpstateIdxToClkDomainsMask(fname) FLCN_STATUS (fname)(LwU8 vpstateIdx, BOARDOBJGRPMASK_E32 *pClkDomainsMask)

/*!
 * @interface PERF_LIMITS
 *
 * Returns the CLK_DOMAIN frequency (kHz) specified within a given VPSTATE, as specified
 * by its VPSTATE index.
 *
 * @param[in]     pLimits             PERF_LIMITS pointer
 * @param[in]     vpstateIdx
 *     Index of @ref VPSTATE object.
 * @param[in]     bFloor
 *     Boolean indicating whether frequency value should be quantized via floor
 *     (LW_TRUE) or ceiling (LW_FALSE).
 * @param[in]     bFallbackToPstateRange
 *     Boolean indicating whether frequency value should fall back to
 *     the VPSTATE's PSTATE's CLK_DOMAIN range values, if the VPSTATE
 *     does not specify the CLK_DOMAIN frequency directly.
 * @param[in/out] pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF struct in which the caller specifies the
 *     CLK_DOMAIN index for which to look-up as @ref PERF_LIMITS_VF::idx.  The
 *     resulting frequency (kHz) will be returned as @ref PERF_LIMITS_VF::value.
 *
 * @return FLCN_OK
 *     @ref CLK_DOMAIN frequency successfully retrieved.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     @ref vspstateIdx specifies an invalid VPSTATE index.
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsVpstateIdxToFreqkHz(fname) FLCN_STATUS (fname)(LwU8 vpstateIdx, LwBool bFloor, LwBool bFallbackToPstateRange, PERF_LIMITS_VF *pVfDomain)

/*!
 * @interface PERF_LIMITS
 *
 * Determines whether a given frequency (kHz) on a CLK_DOMAIN is generated in a
 * noise-unaware or noise-aware manner.
 *
 * @param[in]     pLimits             PERF_LIMITS pointer
 * @param[in]     pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF struct specifying the
 *     CLK_DOMAIN input values - @ref PERF_LIMITS_VF::idx specifies
 *     the CLK_DOMAIN index, @ref PERF_LIMITS_VF::value specifies
 *     the frequency (kHz).
 * @param[out]    pBNoiseUnaware
 *     Pointer to LwBool in which to return whether the frequency is
 *     noise-unaware (LW_TRUE) or noise-aware (LW_FALSE).
 *
 * @return FLCN_OK
 *     Frequency noise-unaware determination completed successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     @ref pVfDomain->idx specifies an invalid CLK_DOMAIN index.
 * @return Other errors
 *     Unexpected error.
 */
#define PerfLimitsFreqkHzIsNoiseUnaware(fname) FLCN_STATUS (fname)(PERF_LIMITS_VF *pVfDomain, LwBool *pBNoiseUnaware)

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// PERF_LIMITS interfaces
/*!
 * Entry point for PERF_LIMITS construction at PERF task INIT time to allocate
 * the necessary DMEM structures in PERF DMEM overlay.
 */
PerfLimitsFreqPropagate                 (perfLimitsFreqPropagate)
    GCC_ATTRIB_SECTION("imem_perfLimitVf", "perfLimitsFreqPropagate");
PerfLimitsFreqkHzToVoltageuV            (perfLimitsFreqkHzToVoltageuV)
    GCC_ATTRIB_SECTION("imem_perfLimitVf", "perfLimitsFreqkHzToVoltageuV");
PerfLimitsVoltageuVToFreqkHz            (perfLimitsVoltageuVToFreqkHz)
    GCC_ATTRIB_SECTION("imem_perfLimitVf", "perfLimitsVoltageuVToFreqkHz");
PerfLimitsFreqkHzQuantize               (perfLimitsFreqkHzQuantize)
    GCC_ATTRIB_SECTION("imem_perfLimitVf", "perfLimitsFreqkHzQuantize");
PerfLimitsPstateIdxRangeToFreqkHzRange  (perfLimitsPstateIdxRangeToFreqkHzRange)
    GCC_ATTRIB_SECTION("imem_perfLimitVf", "perfLimitsPstateIdxRangeToFreqkHzRange");
PerfLimitsFreqkHzToPstateIdxRange       (perfLimitsFreqkHzToPstateIdxRange)
    GCC_ATTRIB_SECTION("imem_perfLimitVf", "perfLimitsFreqkHzToPstateIdxRange");
PerfLimitsVpstateIdxToClkDomainsMask    (perfLimitsVpstateIdxToClkDomainsMask)
    GCC_ATTRIB_SECTION("imem_perfLimitVf", "perfLimitsVpstateIdxToClkDomainsMask");
PerfLimitsVpstateIdxToFreqkHz           (perfLimitsVpstateIdxToFreqkHz)
    GCC_ATTRIB_SECTION("imem_perfLimitVf", "perfLimitsVpstateIdxToFreqkHz");
PerfLimitsFreqkHzIsNoiseUnaware         (perfLimitsFreqkHzIsNoiseUnaware)
    GCC_ATTRIB_SECTION("imem_perfLimitVf", "perfLimitsFreqkHzIsNoiseUnaware");

/* ------------------------ Include Derived Types --------------------------- */

#endif // PERF_LIMIT_VF_H
