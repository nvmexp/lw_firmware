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
 * @file    perf_limit.h
 * @brief   Arbitration interface file.
 */

#ifndef PERF_LIMIT_H
#define PERF_LIMIT_H

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "dbgprintf.h"
#include "task_perf.h"
#include "boardobj/boardobjgrp.h"
#include "perf/perf_limit_client.h"
#include "perf/perf_limit_vf.h"
#include "ctrl/ctrl2080/ctrl2080perf.h"
#include "boardobj/boardobjgrp_iface_model_10.h"
#include "boardobj/boardobj_iface_model_10.h"

/* ------------------------ Forward Definitions ----------------------------- */
typedef struct PERF_LIMIT  PERF_LIMIT, PERF_LIMIT_BASE;
typedef struct PERF_LIMITS PERF_LIMITS;

/* ------------------------ Macros and Defines ------------------------------ */
/*!
 * Special value for PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS to
 * indicate the sizing of CLK_DOMAIN support for PERF_LIMITS has not
 * been properly sized for the given implementation.
 */
#define PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS_DISABLED    0
/*!
 * Special value for PERF_PERF_LIMITS_VOLT_VOLT_RAILS_DOMAIN_MAX_RAILS
 * to indicate the sizing of VOLT_RAIL support for PERF_LIMITS has not
 * been properly sized for the given implementation.
 */
#define PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS_DISABLED      0
/*!
 * Special value for PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE
 * to indicate the sizing of active PERF_LIMITS buffer has not
 * been properly sized for the given implementation.
 */
#define PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE_DISABLED         0

/*!
 * Until we have a nice solution for defining constants based on the GPU,
 * this file shall define values needed by the arbiter. These values are
 * defined to optimize memory usage (not waste memory by over allocating).
 *
 * PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS
 *      The number of clock domains recognized by SW. For chips not explicitly
 *      specified, the value is LW2080_CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS.
 * PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS
 *      The number of volt rails recognized by SW. For chips not explicitly
 *      specified, the value isLW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS.
 *
 * PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE
 *      Maximum number of simultaneous active
 *      (i.e. LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE != _DISABLED)
 *      PERF_LIMITS.  This limit is due to pre-allocation of PERF_LIMIT data within
 *      the PMU - the PMU does not have enough DMEM to statically allocate this data
 *      for every possible PERF_LIMIT object.
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT_35_10))
#define PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS     10U
#define PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS       1U
#define PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE          24U
#elif (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT_40))
#define PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS     10U
#define PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS       2U
#define PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE          30U
#else
#define PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS     PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS_DISABLED
#define PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS       PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS_DISABLED
#define PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE          PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE_DISABLED
#endif

//
// Verify the values defined are smaller than or equal to their LW2080
// equivalents. The messages data passed between the RM and PMU use the
// LW2080 values.
//
PMU_CT_ASSERT(PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS <= LW2080_CTRL_CLK_CLK_DOMAIN_CLIENT_MAX_DOMAINS);
PMU_CT_ASSERT(PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS <= LW2080_CTRL_VOLT_VOLT_RAIL_CLIENT_MAX_RAILS);

/*!
 * Base set of common PERF_LIMIT overlays (both IMEM and DMEM).  Expected to be
 * used with @ref OSTASK_ATTACH_OR_DETACH_OVERLAYS_<XYZ>().
 */
#define PERF_LIMIT_OVERLAYS_BASE                                               \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfLimit)                        \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfVfeEquMonitor)                   \
    OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfLimit)

/*!
 * Set of common overlays (both IMEM and DMEM) required to call
 * perfLimitsArbitrate() (and all its called functions).
 */
#define PERF_LIMIT_OVERLAYS_ARBITRATE                                          \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfLimitArbitrate)                  \
    OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, perfLimitVf)                         \
    PERF_POLICY_OVERLAYS_BASE

/*!
 * Helper macro accessor to the PERF_LIMITS object.
 *
 * @return pointer to PERF_LIMITS object
 */
#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT))
#define PERF_PERF_LIMITS_GET()                                                \
    (Perf.pLimits)
#else
#define PERF_PERF_LIMITS_GET()                                                \
    ((PERF_LIMITS *)NULL)
#endif

/*!
 * Helper macro specifying the arbiter is locked or not. If the arbiter is
 * locked, it will not change the clocks after an arbitrate and apply request.
 * Clients are still allowed to change limits, but those changes will not be
 * imposed until the arbiter becomes unlocked.
 *
 * @param[in]   pPerf   OBJPERF pointer.
 *
 * @return a boolean specifying if the arbiter is locked or not; if PERF_LIMITS
 * have not been initialized, return a locked state.
 */
#define PERF_PERF_LIMITS_IS_LOCKED()                                           \
    ((PERF_PERF_LIMITS_GET() != NULL) ?                                        \
        PERF_PERF_LIMITS_GET()->bArbitrateAndApplyLock : LW_TRUE))

/*!
 * Macro to locate PERF_LIMITS BOARDOBJGRP.
 *
 * @copydoc BOARDOBJGRP_OBJ_GET
 */
#define BOARDOBJGRP_DATA_LOCATION_PERF_LIMIT                                   \
    (&(PERF_PERF_LIMITS_GET()->super.super))

/*!
 * Helper macro accessor to the PERF_LIMIT object.
 *
 * @param[in]  _idx  Limit ID of PERF_LIMIT
 *
 * @return pointer to PERF_LIMIT object
 */
#define PERF_PERF_LIMIT_GET(_idx)                                              \
    (BOARDOBJGRP_OBJ_GET(PERF_LIMIT, (_idx)))

/*!
 * Helper macro to increment @ref PERF_LIMITS::arbSeqId
 *
 * @param[in] pLimits    PERF_LIMITS pointer
 */
#define PERF_LIMITS_ARB_SEQ_ID_INCREMENT(pLimits)                              \
    LW2080_CTRL_PERF_PERF_LIMITS_ARB_SEQ_ID_INCREMENT(                         \
        &((pLimits)->arbSeqId))

/*!
 * Helper macro to iterate over the tuples in an ARBITRATION_OUTPUT structure.
 * Iterates the iterator variable @ref _iter from 0 up to
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS.
 *
 * Intended to be used with structures such as @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT or @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_35.
 *
 * @note Must be used in conjunction with @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END.
 *
 * @param[out]  _iter
 *     Iterator variable.  Will iterate from 0 to
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(_iter)                           \
    do {                                                                               \
        for ((_iter) = 0;                                                              \
            (_iter) <                                                                  \
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS;    \
            (_iter)++)                                                                 \
        {

/*!
 * Iteration end macro.  To be used to end @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR().
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END                              \
        }                                                                              \
    } while (LW_FALSE)


/*!
 * Helper macro to init at @ref PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT struct.
 *
 * @param[in] _pArbOutputDefault
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT struct to init.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_INIT(_pArbOutputDefault)              \
    do {                                                                             \
        LwU8 _i;                                                                     \
        boardObjGrpMaskInit_E32(&((_pArbOutputDefault)->clkDomainsMask));            \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(_i)                            \
        {                                                                            \
            memset(&((_pArbOutputDefault)->tuples[_i]), 0x0,                         \
                sizeof((_pArbOutputDefault)->tuples[_i]));                           \
        }                                                                            \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;                           \
    } while (LW_FALSE)

/*!
 * Helper macro to export a @ref PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT struct
 * to a @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT.
 *
 * @param[in]  _pArbOutputDefault
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT struct to export.
 * @param[out] _pArbOutputDefaultRmctrl
 *     Pointer to LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT populate.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_EXPORT(_pArbOutputDefault, _pArbOutputDefaultRmctrl) \
    do {                                                                                            \
        LwU8 _i;                                                                                    \
        (void)boardObjGrpMaskExport_E32(&((_pArbOutputDefault)->clkDomainsMask),                    \
            &((_pArbOutputDefaultRmctrl)->clkDomainsMask));                                         \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(_i)                                           \
        {                                                                                           \
            (_pArbOutputDefaultRmctrl)->tuples[_i].pstateIdx =                                      \
                (_pArbOutputDefault)->tuples[_i].pstateIdx;                                         \
            memcpy((_pArbOutputDefaultRmctrl)->tuples[_i].clkDomains,                               \
                   (_pArbOutputDefault)->tuples[_i].clkDomains,                                     \
                   (sizeof(LwU32) * PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS));                  \
        }                                                                                           \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;                                          \
    } while (LW_FALSE)

/*!
 * Helper macro to initialize a LW2080_CTRL_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE
 * structure.
 *
 * @param[in] _pTuple
 *     Pointer to LW2080_CTRL_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE to init.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_INIT(_pTuple)                  \
    do {                                                                    \
        LWMISC_MEMSET((_pTuple), 0x0,                                       \
            sizeof(PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE));                  \
        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_CACHE_DATA_INIT(    \
            &((_pTuple)->cacheData));                                       \
    } while (LW_FALSE)

/*!
 * Helper macro to import a @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE
 * struct to a @ref PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE struct.
 *
 * @param[in]  _pArbOutput
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE struct to populate.
 * @param[out] _pArbOutputRmctrl
 *     Pointer to LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE to import.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IMPORT(_pArbOutput, _pArbOutputRmctrl) \
    do {                                                                            \
        (_pArbOutput)->cacheData = (_pArbOutputRmctrl)->cacheData;                  \
        (_pArbOutput)->pstateIdx = (_pArbOutputRmctrl)->pstateIdx;                  \
        memcpy(&((_pArbOutput)->clkDomains), &((_pArbOutputRmctrl)->clkDomains),    \
            sizeof((_pArbOutput)->clkDomains));                                     \
        memcpy(&((_pArbOutput)->voltRails), &((_pArbOutputRmctrl)->voltRails),      \
            sizeof((_pArbOutput)->voltRails));                                      \
    } while (LW_FALSE)

/*!
 * Helper macro to export a @ref PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE struct to
 * a @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE.
 *
 * @param[in]  _pArbOutput
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE struct to export.
 * @param[out] _pArbOutputRmctrl
 *     Pointer to LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE populate.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_EXPORT(_pArbOutput, _pArbOutputRmctrl) \
    do {                                                                            \
        (_pArbOutputRmctrl)->cacheData = (_pArbOutput)->cacheData;                  \
        (_pArbOutputRmctrl)->pstateIdx = (_pArbOutput)->pstateIdx;                  \
        memcpy(&((_pArbOutputRmctrl)->clkDomains), &((_pArbOutput)->clkDomains),    \
            sizeof((_pArbOutput)->clkDomains));                                     \
        memcpy(&((_pArbOutputRmctrl)->voltRails), &((_pArbOutput)->voltRails),      \
            sizeof((_pArbOutput)->voltRails));                                      \
    } while (LW_FALSE)

/*!
 * Helper macro to init at @ref PERF_LIMITS_ARBITRATION_OUTPUT struct.
 *
 * @param[out] _pArbOutput
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT struct to init.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_INIT(_pArbOutput)                       \
    do {                                                                       \
        (_pArbOutput)->version = LW2080_CTRL_PERF_PERF_LIMITS_VER_UNKNOWN;     \
        boardObjGrpMaskInit_E32(&((_pArbOutput)->clkDomainsMask));             \
        boardObjGrpMaskInit_E32(&((_pArbOutput)->voltRailsMask));              \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_INIT(&((_pArbOutput)->tuple));    \
    } while (LW_FALSE)


/*!
 * Helper macro to import a @ref LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT
 * struct to a @ref PERF_LIMITS_ARBITRATION_OUTPUT struct.
 *
 * @param[in]  _pArbOutput
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT struct to export.
 * @param[out] _pArbOutputRmctrl
 *     Pointer to LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT populate.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_IMPORT(_pArbOutput, _pArbOutputRmctrl)  \
    do {                                                                       \
        (_pArbOutput)->version = (_pArbOutputRmctrl)->version;                 \
        (void)boardObjGrpMaskImport_E32(&((_pArbOutput)->clkDomainsMask),      \
            &((_pArbOutputRmctrl)->clkDomainsMask));                           \
        (void)boardObjGrpMaskImport_E32(&((_pArbOutput)->voltRailsMask),       \
            &((_pArbOutputRmctrl)->voltRailsMask));                            \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IMPORT(                           \
            &((_pArbOutput)->tuple), &((_pArbOutputRmctrl)->tuple));           \
    } while (LW_FALSE)

/*!
 * Helper macro to export a @ref PERF_LIMITS_ARBITRATION_OUTPUT struct to a @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT.
 *
 * @param[in]  _pArbOutput
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT struct to export.
 * @param[out] _pArbOutputRmctrl
 *     Pointer to LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT populate.
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_EXPORT(_pArbOutput, _pArbOutputRmctrl)  \
    do {                                                                       \
        (_pArbOutputRmctrl)->version = (_pArbOutput)->version;                 \
        (void)boardObjGrpMaskExport_E32(&((_pArbOutput)->clkDomainsMask),      \
            &((_pArbOutputRmctrl)->clkDomainsMask));                           \
        (void)boardObjGrpMaskExport_E32(&((_pArbOutput)->voltRailsMask),       \
            &((_pArbOutputRmctrl)->voltRailsMask));                            \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_EXPORT(                           \
            &((_pArbOutput)->tuple), &((_pArbOutputRmctrl)->tuple));           \
    } while (LW_FALSE)

/*!
 * Constant value to specify an invalid index into the active data.
 */
#define PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID                               0xFF

/* ------------------------ Types Definitions ------------------------------- */
/*!
 * Wrapper sturcture for all PERF_LIMIT data which is needed for an active PERF_LIMIT,
 * including tracking data for where this structure was allocated in the global
 * array of ACTIVE_DATA stuctures.
 *
 * This data is stored separately from the PERF_LIMIT because the PMU does not
 * have enough DMEM available to statically allocate these structures for every
 * possible PERF_LIMIT.  The memory requirements would be around 40K.  Instead,
 * these structures are pre-allocated for some maximum number of simultaneous
 * active structures (@ref PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE).
 */
typedef struct
{
    /*!
     *  Client input for the PERF_LIMIT.
     *
     * When a client provides an input, the input is stored in the raw form
     * in this data member. A client may specify a P-state, frequency, virtual
     * P-state, or voltage limit.
     */
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT clientInput;
} PERF_LIMIT_ACTIVE_DATA;

/*!
 * Default arbitration output tuple.  A tuple of pstate index and CLK_DOMAIN
 * frequencies.
 *
 * This struct differs from @ref PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE in the
 * following ways:
 *
 * 1. This struct does not contain any VOLT_RAIL values.  VOLT_RAIL voltages cannot
 * be directly compared.  They are instead a function of the final arbitrated
 * CLK_DOMAIN frequencies.  So, default arbitration doesn't bother with them.
 *
 * 2. This struct doesn't contain any limitIdxs.  There is no concept of
 * PERF_LIMIT priorities before the arbitration algorithm runs.
 */
typedef struct
{
    /*!
     * PSTATE index for this tuple.
     */
    LwU32 pstateIdx;
    /*!
     * CLK_DOMAIN frequency values (kHz).  Indexes correspond to indexes into
     * CLK_DOMAINS BOARDOBJGRP.  This array has valid indexes corresponding to
     * bits set in @ref
     * PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT::clkDomainsMask.
     */
    LwU32 clkDomains[PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS];
} PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE;

/*!
 * PMU implementation of LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT
 * (basically just PMU versions of BOARDOBJGRPMASK).
 *
 * @copydoc LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT
 */
typedef struct
{
    /*!
     * Mask of indexes of CLK_DOMAINs which have frequency settings specified in
     * this struct.
     */
    BOARDOBJGRPMASK_E32  clkDomainsMask;
    /*!
     * Array of LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE
     * structures.  Indexed per @ref
     * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX - i.e. 0 =
     * _MIN, 1 = _MAX.
     */
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE tuples[
        LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_NUM_IDXS];
} PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT;

/*!
 * PMU implementation of LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE.
 * This version uses less memory as the size of the arrays match the actual
 * number of clock domains and volt rails for the GPU.
 *
 * @copydoc LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE
 */
typedef struct
{
    /*!
     * ARBITATION_OUTPUT caching data.
     */
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_CACHE_DATA cacheData;
    /*!
     * Arbitrated PSTATE output.   Current usecase use units of PSTATE_LEVEL.
     *
     * CRPTODO - Switch over to pstateIdx.
     */
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VALUE pstateIdx;
    /*!
     * Arbitrated CLK_DOMAINS output.  Indexed by the indexes of the CLK_DOMAINS
     * BOARDOBJGRP.
     *
     * Has valid indexes corresonding to @ref
     * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT::clkDomainsMask.
     */
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN
        clkDomains[PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS];
    /*!
     * Arbitrated VOLT_RAILS output.  Indexed by the indexes of the VOLT_RAILS
     * BOARDOBJGRP.
     *
     * Has valid indexes corresonding to @ref
     * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT::voltRailMask.
     */
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL
        voltRails[PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS];
} PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE;

/*!
 * Generic PERF_LIMITS arbitration output structure.
 *
 * Output of the arbitration routine (@ref perfLimitsArbitrate())
 * which contains the min and max tuples of arbitrated/target clocks and
 * voltages for the GPU.  This structure also contains various deubgging
 * information about the PERF_LIMITs which determined that output tuple.
 *
 * This structure is completely PERF_LIMITS-type/version-independent, it
 * contains only generic arbitration output which is expected from any/all
 * versions of PERF_LIMITS.  Sub-classes may implement/extend this structure for
 * their type-specific information.
 */
typedef struct
{
    /*!
     * PERF_LIMITS version which populated this ARBITRATION_OUTPUT struct.  Used
     * for dyanmic casting of this generic ARBITRATION_OUTPUT structure to any
     * type-specific ARBITRATION_OUTPUT structure which might extend it.
     *
     * Values specified in @ref LW2080_CTRL_PERF_PERF_LIMITS_VERSION_<XYZ>.
     */
    LwU8                version;
    /*!
     * Mask of arbitrated CLK_DOMAINs, corresponding to indexes in the
     * CLK_DOMAINS BOARDOBJGRP.
     *
     * Will be a subset of the overall CLK_DOMAINS BOARDOBJGRP.  Valid indexes
     * may be omitted if the arbiter decides they should not be arbitrated
     * (e.g. FIXED domains are not arbitrated).
     */
    BOARDOBJGRPMASK_E32 clkDomainsMask;
    /*!
     * Mask of arbitrated VOLT_RAILs, corresponding to indexes in the
     * VOLT_RAILS BOARDOBJGRP.
     *
     * Will be a subset of the overall VOLT_RAILS BOARDOBJGRP.
     */
    BOARDOBJGRPMASK_E32 voltRailsMask;
    /*!
     * Arbitrated result tuple.
     */
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE tuple;
} PERF_LIMITS_ARBITRATION_OUTPUT;

/*!
 * @brief An individual limit's state.
 *
 * Contains data about the behavior of the individual limit, including clock
 * domains to strictly limit, client input, and input into the arbitration
 * process.
 */
struct PERF_LIMIT
{
    /*!
     * Base class. Must always be first.
     */
    BOARDOBJ super;

    /*!
     * @ref LW2080_CTRL_PERF_LIMIT_INFO_FLAGS_<xyz>.
     */
    LwU8        flags;

    /*!
     * Propagation regime to be used for propagation of settings to
     * CLK_DOMAINs.
     */
    LwU8        propagationRegime;

    /*!
     * PERF_POLICY the PERF_LIMIT is associated with.
     */
    LW2080_CTRL_PERF_POLICY_SW_ID   perfPolicyId;

    /*!
     * Index to globally pre-allocated PERF_LIMIT_ACTIVE_DATA for this
     * PERF_LIMIT object.  When the PERF_LIMIT object is active
     * (i.e. LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE != _DISABLED), this
     * index will be != INVALID.  When the PERF_LIMIT object is inactive
     * (i.e. LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE == _DISABLED), this
     * index will be INVALID.
     *
     * On an inactive->active transition, the PERF_LIMIT object will acquire a
     * pre-allocated PERF_LIMIT_ACTIVE_DATA structure via @ref
     * PerfLimitsActiveDataAcquire and store the pointer here.  On an
     * active->inactive transition, the PERF_LIMIT object will release the
     * previously acquired PERF_LIMIT_ACTIVE_DATA structure via @ref
     * PerfLimitsActiveDataRelease and set this pointer to NULL.
     */
    LwU8        activeDataIdx;
};

/*!
 * @brief Container of limits.
 *
 * Includes additional information to properly function
 */
struct PERF_LIMITS
{
    /*!
     * Base class. Must always be first.
     */
    BOARDOBJGRP_E255 super;

    /*!
     * Version of the limits structure. Values specified in
     * @ref LW2080_CTRL_PERF_PERF_LIMITS_VERSION.
     */
    LwU8 version;

    /*!
     * Boolean indicating whether the PERF_LIMITS subsystem has been loaded by
     * the RM/CPU.  When loaded, PERF_LIMITS is able to run the arbitration
     * routine and apply those changes to the CHANGE_SEQ.
     */
    LwBool bLoaded;

    /*!
     * Specifies whether arbitration is prevented from running or not. If
     * set to LW_TRUE, the arbiter will not run the arbitration algorithm.
     * Clients may continue to set/clear limits, but the changes will not get
     * applied during an arbitrationAndApply().
     */
    LwBool bArbitrateAndApplyLock;

    /*!
     * Boolean indiciating whether PERF_LIMIT caching is enabled or disabled.
     * Caching helps optimize arbiter run-time by caching (and thus not
     * re-evaluating) all state which should not change between arbiter runs.
     */
    LwBool bCachingEnabled;

    /*!
     * Boolean indicating whether the arbiter should apply the minimum or
     * maximum arbitrated results.
     */
    LwBool bApplyMin;

    /*!
     * Boolean indicating whether any PERF_LIMIT handles in the VFE_EQU_MONITOR
     * are dirty.  When this flag is LW_TRUE, @ref perfLimitsArbitrate() will
     * trigger @ref vfeEquMonitorUpdate() to cache the latest values before
     * running the arbitration algorithm.
     */
    LwBool bVfeEquMonitorDirty;

    /*!
     * Sequence ID for @ref perfLimitsArbitrate which is used to track
     * the dependent state of PERF_LIMITS.  Whenever any of the dependent state
     * changes, this sequence ID must be incremented.
     *
     * This sequence ID will be stored in the @ref
     * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT struct to track the
     * snapshot of the state at which the ARBITRATION_OUTPUT was computed.
     * Subsequent calls to @ref perfLimitsArbitrate will short-circuit
     * when the sequence ID of the input struct matches the global sequence ID
     * (i.e. no dependent state has changed).
     */
    LwU32 arbSeqId;

    /*!
     * Mask of indexes in the global PERF_LIMIT_ACTIVE_DATA array which are not
     * acquired by a PERF_LIMIT via @ref PerfLimitsActiveDataAcquire.  Maximum
     * number of bits set in this mask must always be <=
     * PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE.
     *
     * @Note The global PERF_LIMIT_ACTIVE_DATA array is virtually implemented,
     * as there may be type-specific active data.  So, the
     * PERF_LIMIT_ACTIVE_DATA array will be found in child implementations of
     * the PERF_LIMITS object.
     */
    BOARDOBJGRPMASK_E32 activeDataReleasedMask;

    /*!
     * @brief Mask of PERF_LIMITs which are lwrrently active.
     */
    BOARDOBJGRPMASK_E255 limitMaskActive;

    /*!
     * @brief Mask of limits that have been modified since last arbitration.
     */
    BOARDOBJGRPMASK_E255 limitMaskDirty;

    /*!
     * @brief Mask of the limits used to sync GPUs with GPU boost.
     */
    BOARDOBJGRPMASK_E255 limitMaskGpuBoost;

    /*!
     * @brief Default arbitration values.
     *
     * Initial values used during arbitration. The range of frequencies,
     * voltages, and P-states are set to their full range.
     */
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT      arbOutputDefault;

    /*!
     * Pointer to PERF_LIMITS_ARBITRATION_OUTPUT struct to use for @ref
     * perfLimitsArbitrateAndApply().  This points to a PERF_LIMITS
     * version-speicific structure which should have been allocated by the
     * PERF_LIMITS version-specific constructor and pointed to.
     */
    PERF_LIMITS_ARBITRATION_OUTPUT             *pArbOutputApply;

    /*!
     * Pointer to PERF_LIMITS_ARBITRATION_OUTPUT struct to use as a scratch
     * variable @ref perfLimitsArbitrate() calls which are going to be returned
     * out to clients.  This points to a PERF_LIMITS version-speicific structure
     * which should have been allocated by the PERF_LIMITS version-specific
     * constructor and pointed to.
     */
    PERF_LIMITS_ARBITRATION_OUTPUT             *pArbOutputScratch;

    /*!
     * @brief Change sequencer request.
     */
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT   *pChangeSeqChange;

    /*!
     * The last set of GPU Boost Sync limits sent to RM.
     */
    LW2080_CTRL_PERF_PERF_LIMITS_GPU_BOOST_SYNC gpuBoostSync;
};

/* ------------------------ Function Prototypes ----------------------------- */
/*!
 * @interface PERF_LIMITS
 *
 * Constructs all necessary object state for PERF_LIMITS functionality,
 * including allocating necessary DMEM for PERF_LIMITS BOARDOBJGRP as well as
 * any type-specific state.
 */
#define PerfLimitsConstruct(fname) FLCN_STATUS (fname)(PERF_LIMITS **ppLimits)

/*!
 * @interface PERF_LIMIT
 *
 * This interface sets the CLIENT_INPUT for a given PERF_LIMIT.  It stores the
 * CLIENT_INPUT within the PERF_LIMIT and updates any necessary metadata related
 * to the state of the new CLIENT_INPUT.
 *
 * @param[in]  pLimit       PERF_LIMIT pointer
 * @param[in]  pClientInput
 *     LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT pointer to set for this
 *     PERF_LIMIT.
 *
 * @return FLCN_OK
 *     CLIENT_INPUT was successfully set for the
 *     PERF_LIMIT.
 * @return Other errors
 *     An unexpected error oclwrred while setting the CLIENT_INPUT for the
 *     PERF_LIMIT.
 */
#define PerfLimitClientInputSet(fname) FLCN_STATUS (fname)(PERF_LIMIT *pLimit, LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput)

/*!
 * @interface PERF_LIMITS
 *
 * Compares two CLIENT_INPUT structs for equality.
 *
 * @note Ignores several fields in the CLIENT_INPUT struct which aren't actually
 * input, but are output record-keeping/debugging structures (mostly related to
 * voltage).  In this manner, it's not a straight memcmp().
 *
 * @param[in] pClientInput0
 *     Pointer to first LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT structure to
 *     compare.
 * @param[in] pClientInput1
 *     Pointer to second LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT structure to
 *     compare.
 *
 * @return LW_TRUE
 *     CLIENT_INPUT structures are equal.
 * @return LW_FALSE
 *     CLIENT_INPUT structures are not equal.
 */
#define PerfLimitClientInputCompare(fname) LwBool (fname)(LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput0, LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput1)

/*!
 * @interface PERF_LIMITS
 *
 * Returns a pointer to the PERF_LIMIT_ACTIVE_DATA at the corresponding index.
 *
 * @note This is a virtual interface to to return back PERF_LIMIT_ACTIVE_DATA
 * from where it was implemented within the child class of PERF_LIMITS.
 *
 * @param[in] pLimits    PERF_LIMITS pointer
 * @param[in] idx        Index of the PERF_LIMIT_ACTIVE_DATA object to retrieve
 *
 * @return PERF_LIMIT_ACTIVE_DATA pointer to at the corresponding index.
 */
#define PerfLimitsActiveDataGet(fname) PERF_LIMIT_ACTIVE_DATA * (fname)(PERF_LIMITS *pLimits, LwU8 idx)

/*!
 * @interface PERF_LIMITS
 *
 * Acquires a globally, pre-allocated PERF_LIMIT_ACTIVE_DATA structure.  Called
 * by a PERF_LIMIT when accepting CLIENT_INPUT and does not already have
 * PERF_LIMIT_ACTIVE_DATA acquired for it.
 *
 * After acquiring, initializes the PERF_LIMIT_ACTIVE_DATA structure and assigns
 * the CLIENT_INPUT structure to it.
 *
 * @param[in] pLimits      PERF_LIMITS pointer
 * @param[in] pLimit       PERF_LIMIT pointer
 * @param[in] pClientInput
 *     Pointer to LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT structure to store
 *     within the ACTIVE_DATA.
 *
 * @return FLCN_OK
 *     PERF_LIMIT_ACTIVE_DATA structure acquired and the pointer is returned in
 *     @ref ppActiveData.
 * @return FLCN_ERR_NO_FREE_MEM
 *     Could not acquire PERF_LIMIT_ACTIVE_DATA because no released/un-acquired
 *     PERF_LIMIT_ACTIVE_DATA structures remain in the global pre-allocated
 *     buffer.
 */
#define PerfLimitsActiveDataAcquire(fname) FLCN_STATUS (fname)(PERF_LIMITS *pLimits, PERF_LIMIT *pLimit, LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput)

/*!
 * @interface PERF_LIMITS
 *
 * Releases a globally, pre-allocated PERF_LIMIT_ACTIVE_DATA structure which had
 * been previously acquired by a PERF_LIMIT.  Releases the
 * PERF_LIMIT_ACTIVE_DATA so that it can be reused/reacquired by other
 * PERF_LIMITs.
 *
 * @param[in] pLimits      PERF_LIMITS pointer
 * @param[in] pLimit       PERF_LIMIT pointer
 */
#define PerfLimitsActiveDataRelease(fname) void (fname)(PERF_LIMITS *pLimits, PERF_LIMIT *pLimit)

/*!
 * @interface PERF_LIMITS
 *
 * Loads/unloads the PERF_LIMITS object.
 *
 * When loading, caches any necessary
 * runtime SW state and then calls first call to @ref
 * perfLimitsArbitrateAndApply().  After this call returns, the PERF_LIMITS
 * feature is running and will manage pstate, clocks, and voltages based on the
 * run-time PERF_LIMIT input from clients and subsequent calls to @ref
 * perfLimitsArbitrateAndApply().
 *
 * When unloading, after this call returns, the PERF_LIMITS feature is no longer
 * running and will not manage pstate, clocks, and voltages.
 *
 * @param[in] pLimits      PERF_LIMITS pointer
 * @param[in] bLoad
 *     Boolean indicating whether PERF_LIMITS is loading or unloading.
 * @param[in] bApply
 *     Boolean indicating whether should call @ref perfLimitsArbitrateAndApply()
 *     after loading.  Requires @ref bLoad == LW_TRUE.
 * @param[in] applyFlags
 *     Flags to pass in as argument to @ref perfLimitsArbitrateAndApply().
 *     Requires @ref bLoad == LW_TRUE.
 *
 * @return FLCN_OK  Loading/unloading operation successful.
 * @return Other errors   Unexpected errors oclwred during loading/unloading.
 */
#define PerfLimitsLoad(fname) FLCN_STATUS (fname)(PERF_LIMITS *pLimits, LwBool bLoad, LwBool bApply, LwU32 applyFlags)

/*!
 * @interface PERF_LIMITS
 *
 * Ilwalidates all the active PERF_LIMIT, requiring each to be re-evaluated
 * on the next arbitration request.
 *
 * @param[in] pLimits      PERF_LIMITS pointer
 * @param[in] bApply
 *     Boolean specifying to run arbitration after the limits have been
 *     ilwalidated.
 * @param[in]  applyFlags
 *      If @ref bApply is LW_TRUE, this specifies extra flags to pass @ref
 *      PERF_LIMITS::arbitrateAndApply() - @see
 *      LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_<xyz>.  @ref
 *      LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE is implied by @ref bApply and
 *      cannot be overridden by callers, only additional flags can be specified.
 *
 * @return FLCN_OK if the flags were successfully pre-parsed; otherwise, a
 *                 detailed error code is returned.
 */
#define PerfLimitsIlwalidate(fname) FLCN_STATUS (fname)(PERF_LIMITS *pLimits, LwBool bApply, LwU32 applyFlags)

/*!
 * @interface PERF_LIMITS
 *
 * Interface to run arbitration algorithm to determine the target (pstate, clocks,
 * voltage) tuple.
 *
 * This is a virtual interface with both a common "super" PERF_LIMITS
 * entry-point and version/type-specific implementations to handle algorithm
 * differences.
 *
 * @param[in]  pLimits      PERF_LIMITS pointer
 * @param[out] pArbOutput
 *     Pointer to PERF_LIMITS_ARBITRATION_OUTPUT in which to return the
 *     arbitration results.  This structure needs to be an appropriately-typed
 *     sub-structure of PERF_LIMITS_ARBITRATION_OUTPUT, providing the necessary
 *     metadata for the type/version-specific arbitration algorithm.
 * @param[in]  pLimitMaskExclude
 *     Pointer to BOARDOBJGRPMASK_E255 specifying the mask of PERF_LIMITs to
 *     exclude from the arbitration algorithm.  If NULL, no limits should be
 *     excluded.
 * @parma[in]  bMin
 *     Boolean indicating whether the arbitration should choose min (LW_TRUE) or
 *     max (LW_FALSE) results of the arbitration algorithm to populate the
 *     result tuple @ref PERF_LIMITS_ARBITRATION_OUTPUT::tuple.
 *
 * @return FLCN_OK
 *     Aribtration algorithm successfully run
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsArbitrate(fname) FLCN_STATUS (fname)(PERF_LIMITS *pLimits, PERF_LIMITS_ARBITRATION_OUTPUT *pArbOutput, BOARDOBJGRPMASK_E255 *pLimitMaskExclude, LwBool bMin)

/*!
 * @interface PERF_LIMITS
 *
 * Interface to run arbitration algorithm to determine the target (pstate, clocks,
 * voltage) tuple and then queue that new tuple to the CHANGE_SEQ, if necessary.
 *
 * This is the main entry point to the VF switch path in our production code
 * path (i.e. production code never directly injects VF switches to the
 * CHANGE_SEQ).  This interface is expected to be called after a client
 * specifies new CLIENT_INPUT to PERF_LIMIT objects, arbitrating with that new
 * input and then applying, if necessary.
 *
 * @param[in]     pLimits      PERF_LIMITS pointer
 * @param[in]     applyFlags
 *     @ref LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_<xyz>
 * @param[in,out] pChangeSeqId
 *     If a synchronous operation is requested, contains the queue handle and
 *     DMEM overlay the change sequencer shall use to notify of the operation's
 *     completion. Returns the ID of the change request returned by the change
 *     sequencer.
 *
 * @return FLCN_OK
 *     Aribtration algorithm successfully run and, if necessary, a new VF change
 *     has been queued to CHANGE_SEQ.
 * @return Other errors
 *     Unexpected error encountered.
 */
#define PerfLimitsArbitrateAndApply(fname) FLCN_STATUS (fname)(PERF_LIMITS *pLimits, LwU32 applyFlags, PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE *pChangeRequest)

/*!
 * @interface PERF_LIMITS
 *
 * Cache the default arbitration output values within the provided
 * PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT structure.  These are the default
 * min/max arbitration bounds - i.e. the loosest possible bounds, within which
 * the PERF_LIMITs will further bound per their input.
 *
 * @param[in]  pLimits             PERF_LIMITS pointer
 * @param[out] pArbOutputDefault
 *     Pointer in which to return the default arbitration output.
 *
 * @return FLCN_OK
 *     Default arbitration output successfully cached.
 * @return Other errors
 *     Unexpected error encountered while generating default arbitratoin output.
 */
#define PerfLimitsArbOutputDefaultCache(fname) FLCN_STATUS (fname)(PERF_LIMITS *pLimits, PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT *pArbOutputDefault)

/*!
 * @interface PERF_LIMITS
 *
 * Handles requests from other PMU tasks.
 *
 * @param[in]  pDispatch  Pointer to the dispatch containing the PMU client's
 *                        limit data.
 *
 * @return FLCN_OK if the event was handled successfully; otherwise, a detailed
 *                 error code is returned.
 */
#define PerfLimitsEvtHandlerClient(fname) FLCN_STATUS (fname)(DISPATCH_PERF_LIMITS_CLIENT *pDispatch)

/* ------------------------ Inline Functions  -------------------------------- */
/*!
 * Helper function to retrive the frequency for a CLK_DOMAIN,
 * specified by CLK_DOMAIN index, from a @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE structure.  Provides
 * index-safe accesses.
 *
 * @param[in] pDefaultTuple
 *     PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE pointer
 * @param[in/out]  pVfDomain
 *     Pointer to @ref PERF_LIMITS_VF.  @ref PERF_LIMITS_VF::idx
 *     specifies the CLK_DOMAIN index as input.  @ref
 *     PERF_LIMITS_VF::value returns the value as output.
 *
 * @return FLCN_OK
 *     PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE CLK_DOMAIN value
 *     successfully returned.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE_CLK_DOMAIN_GET
(
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLE *pDefaultTuple,
    PERF_LIMITS_VF                               *pVfDomain
)
{
    if ((pDefaultTuple == NULL) ||
        (pVfDomain == NULL) ||
        (pVfDomain->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pVfDomain->value = pDefaultTuple->clkDomains[pVfDomain->idx];
    return FLCN_OK;
}

/*!
 * Helper function to retrive a pointer to the @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN
 * structure, specified by CLK_DOMAIN index, from a @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE structure.  Provides
 * index-safe accesses.
 *
 * @param[in] pArbOutputTuple
 *     PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE pointer
 * @param[in] clkDomIdx
 *     CLK_DOMAIN index of the structure to retrieve
 * @param[out] ppTupleClkDomain
 *     Pointer in which to return the
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN
 *     pointer.
 *
 * @return FLCN_OK
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN
 *     pointer successfully returned.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_GET
(
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE                              *pArbOutputTuple,
    LwBoardObjIdx                                                      clkDomIdx,
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN **ppTupleClkDomain
)
{
    if ((pArbOutputTuple == NULL) ||
        (ppTupleClkDomain == NULL) ||
        (clkDomIdx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *ppTupleClkDomain = &pArbOutputTuple->clkDomains[clkDomIdx];
    return FLCN_OK;
}

/*!
 * Helper function to retrive a pointer to the @ref
 * LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL
 * structure, specified by VOLT_RAIL index, from a @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE structure.  Provides
 * index-safe accesses.
 *
 * @param[in] pArbOutputTuple
 *     PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE pointer
 * @param[in] voltRailIdx
 *     VOLT_RAIL index of the structure to retrieve
 * @param[out] ppTupleClkDomain
 *     Pointer in which to return the
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL
 *     pointer.
 *
 * @return FLCN_OK
 *     LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL
 *     pointer successfully returned.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_GET
(
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE                             *pArbOutputTuple,
    LwBoardObjIdx                                                     voltRailIdx,
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL **ppTupleVoltRail
)
{
    if ((pArbOutputTuple == NULL) ||
        (ppTupleVoltRail == NULL) ||
        (voltRailIdx >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    *ppTupleVoltRail = &pArbOutputTuple->voltRails[voltRailIdx];
    return FLCN_OK;
}

/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
// PERF_LIMITS interfaces
/*!
 * Entry point for PERF_LIMITS construction at PERF task INIT time to allocate
 * the necessary DMEM structures in PERF DMEM overlay.
 */
void perfLimitsConstruct(void)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfLimitsConstruct");
PerfLimitsConstruct                     (perfLimitsConstruct_SUPER)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_libPerfInit", "perfLimitsConstruct_SUPER");
PerfLimitsActiveDataGet                 (perfLimitsActiveDataGet)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitsActiveDataGet");
PerfLimitsActiveDataAcquire             (perfLimitsActiveDataAcquire)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitsActiveDataAcquire");
PerfLimitsActiveDataRelease             (perfLimitsActiveDataRelease)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitsActiveDataRelease");
PerfLimitsLoad                          (perfLimitsLoad)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitsLoad");
PerfLimitsIlwalidate                    (perfLimitsIlwalidate)
    GCC_ATTRIB_SECTION("imem_perfIlwalidation", "perfLimitsIlwalidate");
PerfLimitsArbitrate                     (perfLimitsArbitrate)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitsArbitrate");
PerfLimitsArbitrateAndApply             (perfLimitsArbitrateAndApply)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitsArbitrateAndApply");
PerfLimitsArbOutputDefaultCache         (perfLimitsArbOutputDefaultCache)
    GCC_ATTRIB_SECTION("imem_perfLimitArbitrate", "perfLimitsArbOutputDefaultCache");
PerfLimitsEvtHandlerClient              (perfLimitsEvtHandlerClient)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitsEvtHandlerClient");

// PERF_LIMIT interfaces
PerfLimitClientInputSet     (perfLimitClientInputSet)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitClientInputSet");
PerfLimitClientInputCompare (perfLimitClientInputCompare)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "perfLimitClientInputCompare");

// BOARDOBJGRP interfaces
BoardObjGrpIfaceModel10CmdHandler   (perfLimitBoardObjGrpIfaceModel10Set)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "perfLimitBoardObjGrpIfaceModel10Set");
BoardObjGrpIfaceModel10CmdHandler   (perfLimitBoardObjGrpIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libPerfBoardObj", "perfLimitBoardObjGrpIfaceModel10GetStatus");

// BOARDOBJ interfaces
BoardObjGrpIfaceModel10ObjSet       (perfLimitGrpIfaceModel10ObjSetImpl_SUPER)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "perfLimitGrpIfaceModel10ObjSetImpl_SUPER");
BoardObjIfaceModel10GetStatus           (perfLimitIfaceModel10GetStatus)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "perfLimitIfaceModel10GetStatus");
BoardObjIfaceModel10GetStatus           (perfLimitIfaceModel10GetStatus_SUPER)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "perfLimitIfaceModel10GetStatus_SUPER");


/* ------------------------ Include Derived Types --------------------------- */
#include "perf/35/perf_limit_35.h"

#endif // PERF_LIMIT_H
