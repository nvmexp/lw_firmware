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
 * @file    perf_limit_35.h
 * @brief   Arbitration interface file for P-States 3.5.
 */

#ifndef PERF_LIMIT_35_H
#define PERF_LIMIT_35_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "perf/perf_limit.h"
#include "boardobj/boardobjgrpmask.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_LIMIT_35  PERF_LIMIT_35;
typedef struct PERF_LIMITS_35 PERF_LIMITS_35;

/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Default arbitration output tuple. A tuple of pstate index and CLK_DOMAIN
 * frequencies.
 *
 * This struct differs from @ref
 * PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE in the following ways:
 *
 * 1. This struct does not contain any VOLT_RAIL values. VOLT_RAIL voltages cannot
 * be directly compared. They are instead a function of the final arbitrated
 * CLK_DOMAIN frequencies. So, default arbitration doesn't bother with them.
 *
 * 2. This struct doesn't contain any limitIdxs. There is no concept of
 * PERF_LIMIT priorities before the arbitration algorithm runs.
 */
typedef struct
{
    /*!
     * PSTATE index for this tuple.
     */
    LwU32 pstateIdx;
    /*!
     * CLK_DOMAIN frequency values (kHz). Indexes correspond to indexes into
     * CLK_DOMAINS BOARDOBJGRP. This array has valid indexes corresponding to
     * bits set in @ref
     * PERF_LIMIT_ARBITRATION_INPUT_35::clkDomainsMask.
     */
    LwU32 clkDomains[PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS];
    /*!
     * VOLT_RAIL voltage values (uV). Indexes correspond to indexes into
     * VOLT_RAILS BOARDOBJGRP. This array has valid indexes corresponding to
     * bits set in @ref
     * PERF_LIMIT_ARBITRATION_INPUT_35::voltRailsMask.
     */
    LwU32 voltRails[PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS];
} PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE;

/*!
 */
typedef struct
{
    /*!
     * Mask of CLK_DOMAINs in @ref super which have been set via @ref
     * s_perfLimit35ClientInputToArbInput().  This book-keeping mask is used for
     * determining which CLK_DOMAIN values still need to be determined.
     *
     * As a sanity check, @ref s_perfLimit35ClientInputToArbInput() will confirm
     * that this mask is equal to PERF_LIMITS_ARBITRATION_OUTPUT::clkDomainsMask
     * after all CLIENT_COLWERSION is complete - i.e. that code has determind a
     * complete tuple of PSTATE and CLK_DOMAIN values which can be applied to
     * PERF_LIMITS_ARBITRATION_OUTPUT.
     */
    BOARDOBJGRPMASK_E32 clkDomainsMask;
    /*!
     * Mask of VOLT_RAILS in @ref super which have been set via @ref
     * s_perfLimit35ClientInputToArbInput().
     *
     * Most PERF_LIMIT_35 objects won't specify any PERF_LIMIT_ARBITRATION_INPUT
     * values for VOLT_RAILS.  Most VOLT_RAIL values are determined from the
     * (PSTATE, CLK_DOMAINS) tuple values in PERF_LIMITS_ARBITRATION_OUTPUT.
     * The exception is PERF_LIMIT_35 objects with
     * LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE = _VOLTAGE, which want to
     * bound VOLT_RAILs.  This masks tracks when that data is specified in the
     * PERF_LIMIT_ARBITRATION_INPUT and must be applied to
     * PERF_LIMITS_ARBITRATION_OUTPUT.
     */
    BOARDOBJGRPMASK_E32 voltRailsMask;
    /*!
     * Array of PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE structures.
     * Indexed per @ref
     * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX - i.e. 0 =
     * _MIN, 1 = _MAX.
     */
    PERF_LIMIT_ARBITRATION_INPUT_35_TUPLE tuples[
        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS];
    /*!
     * Contains the minimum loose voltage value of higher priority limits.
     * This value is used when callwlating the voltages required for the
     * arbitration output.
     */
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE voltageMaxLooseuV[
        LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS];
} PERF_LIMIT_ARBITRATION_INPUT_35;

/*!
 * Wrapper sturcture for all PERF_LIMIT_35 data which is needed for an active
 * PERF_LIMIT_35.
 *
 * @copydoc PERF_LIMIT_ACTIVE_DATA
 */
typedef struct
{
    /*!
     * Global/common PERF_LIMIT active data.  Must always be first element in
     * structure.
     */
    PERF_LIMIT_ACTIVE_DATA super;
    /*!
     * @brief Arbitration-based input for the limit.
     *
     * This data member is the input used by the arbiter for the arbitration
     * process. This data member is updated whenever the client updates its
     * input or whenever the VF lwrve is updated.
     */
    PERF_LIMIT_ARBITRATION_INPUT_35 arbInput;
} PERF_LIMIT_ACTIVE_DATA_35;

/*!
 * PMU implementation of @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35.
 * Essentially a less memory intensive version of the structure, the size of
 * the arrays have been trimmed to use the minimum number of clock domains and
 * volt rails.
 */
typedef struct
{
    /*!
     * Arbitrated PSTATE index output.
     */
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE pstateIdx;
    /*!
     * Arbitrated CLK_DOMAINS output.  Indexed by the indexes of the CLK_DOMAINS
     * BOARDOBJGRP.
     *
     * Has valid indexes corresonding to @ref
     * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT::clkDomainsMask.
     */
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
        clkDomains[PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS];
    /*!
     * Arbitrated VOLT_RAILS output.  Indexed by the indexes of the VOLT_RAILS
     * BOARDOBJGRP.
     *
     * Has valid indexes corresonding to @ref
     * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT::voltRailMask.
     */
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
        voltRails[PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS];
} PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35;

/*!
 * PERF_LIMITS_35 arbitration output structure.
 *
 * This structure extends @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT
 * to provide both generic PERF_LIMITS-type/version-independent output, as well
 * as PERF_LIMITS_35-specific arbitration algorithm state and debugging data.
 */
typedef struct
{
    /*!
     * PERF_LIMITS_ARBITRATION_OUTPUT super struct.  Must always be first
     * element in structure.
     */
    PERF_LIMITS_ARBITRATION_OUTPUT super;
    /*!
     * Array of PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 structures. Indexed
     * per @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX
     * (i.e. 0 =_MIN, 1 = _MAX).
     */
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 tuples[
        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS];
} PERF_LIMITS_ARBITRATION_OUTPUT_35;

/*!
 * @brief An individual limit's state for P-states 3.5.
 *
 * Contains data about the behavior of the individual limit, including clock
 * domains to strictly limit, client input, and input into the arbitration
 * process.
 */
struct PERF_LIMIT_35
{
    /*!
     * Base class. Must always be first.
     */
    PERF_LIMIT super;

    /*!
     * Boolean indicating whether STRICT CLK_DOMAIN propgation should be
     * constrained to the ARBITRATION_INPUT PSTATE or not.
     */
    LwBool bStrictPropagationPstateConstrained;

    /*!
     * Boolean indicating whether V_{min, noise-unaware} needs to be updated
     * based on V_{min} value determined using the clock domain strcit
     * propagation mask.
     */
    LwBool bForceNoiseUnawareStrictPropgation;
};

/*!
 * @brief Container of 3.5 limits.
 */
struct PERF_LIMITS_35
{
    /*!
     * Base class.  Must always be first.
     */
    PERF_LIMITS super;

    /*!
     * Global array of pre-allocated PERF_LIMIT_ACTIVE_DATA_35.  PERF_LIMITs
     * will acquire ACTIVE_DATA structures from this array via @ref
     * PerfLimitsActiveDataAcquire and released via @ref
     * PerfLimitsActiveDataRelease.
     *
     * @ref PERF_LIMITS::activeDataReleasedMask specifies the mask of indexes
     * in this array which are released and able to acquired by PERF_LIMITs.
     */
    PERF_LIMIT_ACTIVE_DATA_35 activeData[PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE];

    /*!
     * PERF_LIMITS_ARBITRATION_OUTPUT_35 struct to use for @ref
     * perfLimitsArbitrateAndApply().
     *
     * @ref perfLimitsConstruct_35() will point @ref
     * PERF_LIMITS::pArbOutputApply to this struct.
     */
    PERF_LIMITS_ARBITRATION_OUTPUT_35 arbOutput35Apply;

    /*!
     * PERF_LIMITS_ARBITRATION_OUTPUT_35 struct to use for as a scratch variable
     * @ref perfLimitsArbitrate() calls which are going to be returned out to
     * clients.
     *
     * @ref perfLimitsConstruct_35() will point @ref
     * PERF_LIMITS::pArbOutputScratch to this struct.
     */
    PERF_LIMITS_ARBITRATION_OUTPUT_35 arbOutput35Scratch;
};

/* ------------------------ Inline Functions  -------------------------------- */
/*!
 * Helper function to retrive a pointer to the @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
 * structure, specified by CLK_DOMAIN index, from a @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 structure.  Provides
 * index-safe accesses.
 *
 * @param[in] pArbOutputTuple35
 *     PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 pointer
 * @param[in] clkDomIdx
 *     CLK_DOMAIN index of the structure to retrieve
 * @param[out] ppTuple35ClkDomain
 *     Pointer in which to return the
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
 *     pointer.
 *
 * @return FLCN_OK
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
 *     pointer successfully returned.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35_GET_CLK_DOMAIN
(
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 *pArbOutputTuple35,
    LwBoardObjIdx                            clkDomIdx,
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
        **ppTuple35ClkDomain
)
{
    if ((pArbOutputTuple35 == NULL) ||
        (ppTuple35ClkDomain == NULL) ||
        (clkDomIdx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *ppTuple35ClkDomain = &pArbOutputTuple35->clkDomains[clkDomIdx];
    return FLCN_OK;
}

/*!
 * Helper function to retrive a pointer to the @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
 * structure, specified by VOLT_RAIL index, from a @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 structure.  Provides
 * index-safe accesses.
 *
 * @param[in] pArbOutputTuple35
 *     PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 pointer
 * @param[in] clkDomIdx
 *     CLK_DOMAIN index of the structure to retrieve
 * @param[out] ppTuple35ClkDomain
 *     Pointer in which to return the
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_VOLT_RAIL_35
 *     pointer.
 *
 * @return FLCN_OK
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_35
 *     pointer successfully returned.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35_GET_VOLT_RAIL
(
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_35 *pArbOutputTuple35,
    LwBoardObjIdx                            voltRailIdx,
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_35
        **ppTuple35VoltRail
)
{
    if ((pArbOutputTuple35 == NULL) ||
        (ppTuple35VoltRail == NULL) ||
        (voltRailIdx >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *ppTuple35VoltRail = &pArbOutputTuple35->voltRails[voltRailIdx];
    return FLCN_OK;
}

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// PERF_LIMITS interfaces
FLCN_STATUS perfLimitsConstruct_35 (PERF_LIMITS **ppLimits)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfLimitsConstruct_35");
PERF_LIMIT_ACTIVE_DATA *perfLimitsActiveDataGet_35(PERF_LIMITS *pLimits, LwU8 idx)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitsActiveDataGet_35");
FLCN_STATUS perfLimitsActiveDataAcquire_35(PERF_LIMITS *pLimits, PERF_LIMIT *pLimit, LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitsActiveDataAcquire_35");
FLCN_STATUS perfLimitsArbitrate_35(PERF_LIMITS *pLimits, PERF_LIMITS_ARBITRATION_OUTPUT *pArbOutput, BOARDOBJGRPMASK_E255 *pLimitMaskExclude, LwBool bMin)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "perfLimitsArbitrate_35");


// BOARDOBJ Interfaces
BoardObjGrpIfaceModel10ObjSet (perfLimitGrpIfaceModel10ObjSetImpl_35)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "perfLimitGrpIfaceModel10ObjSetImpl_35");
BoardObjGrpIfaceModel10GetStatusHeader (perfLimitsIfaceModel10GetStatusHeader_35)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "perfLimitsIfaceModel10GetStatusHeader_35");
BoardObjIfaceModel10GetStatus (perfLimitIfaceModel10GetStatus_35)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "perfLimitIfaceModel10GetStatus_35");

/* ------------------------ Include Derived Types --------------------------- */
#include "perf/35/perf_limit_35_10.h"
#include "perf/40/perf_limit_40.h"

/* ------------------------ Inline Functions -------------------------------- */
/*!
 * @brief   Retrieves the @ref clkDomainStrictPropagationMask from PERF_LIMIT_35.
 *
 * @param[in]   pLimit35    PERF_LIMIT_35 pointer.
 *
 * @return  @ref clkDomainStrictPropagationMask pointer.
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
perfLimit35ClkDomainStrictPropagationMaskGet
(
    PERF_LIMIT_35          *pLimit35,
    BOARDOBJGRPMASK_E32    *pClkDomainStrictPropagationMask
)
{
    switch (BOARDOBJ_GET_TYPE(&pLimit35->super.super))
    {
        case LW2080_CTRL_PERF_PERF_LIMIT_TYPE_35_10:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_35_10);
            return perfLimit35ClkDomainStrictPropagationMaskGet_10(pLimit35,
                                                                   pClkDomainStrictPropagationMask);
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_TYPE_40:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_40);
            return perfLimit35ClkDomainStrictPropagationMaskGet_40(pLimit35,
                                                                   pClkDomainStrictPropagationMask);
        }
    }

    PMU_HALT();

    return FLCN_ERR_NOT_SUPPORTED;
}

#endif // PERF_LIMIT_35_H
