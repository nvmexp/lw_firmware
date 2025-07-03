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
 * @file    perf_limit.c
 * @copydoc perf_limit.h
 */

//CRPTODO - Temporary alias until it can be merged into PMU/uproc infrastructure.
#define LWMISC_MEMSET(_s, _c, _n) memset((_s), (_c), (_n))

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "cmdmgmt/cmdmgmt.h"
#include "dmemovl.h"
#include "pmu_objperf.h"
#include "perf/perf_limit.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"
#include "pmu_objclk.h"

#include "g_pmurpc.h"
#include "g_pmurmifrpc.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static PerfLimitsConstruct (s_perfLimitsConstruct)
    // Called only at init time -> init overlay.
    GCC_ATTRIB_SECTION("imem_libPerfInit", "s_perfLimitsConstruct");
static BoardObjGrpIfaceModel10SetHeader   (s_perfLimitIfaceModel10SetHeader)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "s_perfLimitIfaceModel10SetHeader");
static BoardObjGrpIfaceModel10SetEntry    (s_perfLimitIfaceModel10SetEntry)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "s_perfLimitIfaceModel10SetEntry");
static BoardObjGrpIfaceModel10GetStatusHeader   (s_perfLimitIfaceModel10GetStatusHeader)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "s_perfLimitIfaceModel10GetStatusHeader");

static FLCN_STATUS s_perfLimitsProcessConstructCommand(PERF_LIMITS *pLimits, RM_PMU_PERF_PERF_LIMITS_CONSTRUCT_CMD *pConstructCmd)
    GCC_ATTRIB_SECTION("imem_libPerfLimitBoardobj", "s_perfLimitsProcessConstructCommand");

static FLCN_STATUS s_perfLimitsArbitrateGpuBoostSyncLimits(PERF_LIMITS *pLimits)
    GCC_ATTRIB_SECTION("imem_libPerfLimit", "s_perfLimitsArbitrateGpuBoostSyncLimits");

/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
// Ensure that MAX_LIMITS_ACTIVE will fit within tracking BOARODBJGRPMASK size.
ct_assert(LW2080_CTRL_BOARDOBJGRP_E32_MAX_OBJECTS >=
            PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE);

//
// Sanity check PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS,
// PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS,
// PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE to ensure that sizes have
// been chosen for the specific PERF_LIMITS implemetnation.
//
ct_assert(PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS !=
            PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS_DISABLED);
ct_assert(PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS !=
            PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS_DISABLED);
ct_assert(PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE !=
            PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE_DISABLED);
/*!
 * Helper macro to set the corresponding field within a @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT struct's tuples[] array from
 * a @ref PERF_LIMIT_<XYZ>_RANGE structure.  This is a general macro
 * which is used by type-specific macros and functions below, which
 * will specify the @ref _field parameter.
 *
 * @param[in] _pArbOutputDefault
 *      @ref PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT pointer.
 * @param[in] _pRange
 *     Pointer to @ref PERF_LIMITS_<XYZ>_RANGE from which which to populate the
 *     min and max @ref
 *     PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT::_field.
 * @param[in] _field
 *     Field name within @ref
 *     PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT to from @ref
 *     _pRange.  This will be appended to the references to the @ref
 *     PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT::tuples[].  An example value would be
 *     something like "pstateIdx" which would lead to something like
 *     "(_pArbOutputDefault)->tuples[_i].pstateIdx".
 */
#define PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLES_SET_RANGE(               \
                                          _pArbOutputDefault, _pRange, _field) \
    do {                                                                       \
        LwU8 _i;                                                               \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(_i)                      \
        {                                                                      \
            (_pArbOutputDefault)->tuples[_i]._field = (_pRange)->values[_i];   \
        }                                                                      \
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;                     \
    } while (LW_FALSE)

/* ------------------------ Inline Functions  -------------------------------- */
/*!
 * Helper function to set the CLK_DOMAIN's, specified by CLK_DOMAIN
 * index, frequency values within a @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT::tuples[] array.
 *
 * Wrapper to @ref
 * PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLES_SET_RANGE.  Provides
 * sanity checking of array indexes.
 *
 * @param[in] pArbOutputDefault
 *     @reff PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT pointer.
 * @param[in] clkDomIdx
 *     CLK_DOMAIN index of the structure to retrieve
 * @param[in]      pVfDomainRange
 *     Pointer to @ref PERF_LIMITS_VF_RANGE.  @ref PERF_LIMITS_VF_RANGE::idx
 *     specifies the CLK_DOMAIN index as input.  @ref
 *     PERF_LIMITS_VF_RANGE::values specifies the range of values to set.
 *
 * @return FLCN_OK
 *     @ref PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT::tuples[]
 *     CLK_DOMAIN values set successfully.
 * @return FLCN_ERR_ILWALID_ARGUMENT
 *     Could not return value due to invalid arguments (likely invalid index).
 */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLES_CLK_DOMAIN_SET_RANGE
(
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT *pArbOutputDefault,
    PERF_LIMITS_VF_RANGE                   *pVfDomainRange
)
{
    if ((pArbOutputDefault == NULL) ||
        (pVfDomainRange == NULL) ||
        (pVfDomainRange->idx >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS))
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLES_SET_RANGE(pArbOutputDefault,
        pVfDomainRange, clkDomains[pVfDomainRange->idx]);
    return FLCN_OK;
}


/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @copydoc perfLimitsConstruct
 */
void
perfLimitsConstruct(void)
{
    FLCN_STATUS status;

    OSTASK_OVL_DESC ovlDescList[] = {
        PERF_LIMIT_OVERLAYS_BASE
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfLimitChangeSeqChange)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    // Construct/allocate PERF_LIMITS structure.
    status = s_perfLimitsConstruct(&Perf.pLimits);
    if ((status != FLCN_OK) ||
        (Perf.pLimits == NULL))
    {
        PMU_HALT();
    }

    // Allocate perf change sequence buffer.
    Perf.pLimits->pChangeSeqChange =
        lwosCallocType(OVL_INDEX_DMEM(perfLimitChangeSeqChange), 1U,
                       LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT);
    if (Perf.pLimits->pChangeSeqChange == NULL)
    {
        PMU_HALT();
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
}

/*!
 * PERF_LIMITS_SUPER implementation
 *
 * @copydoc PerfLimitsConstruct
 */
FLCN_STATUS
perfLimitsConstruct_SUPER
(
    PERF_LIMITS **ppLimits
)
{
    FLCN_STATUS status = FLCN_OK;
    LwU8 i;

    // Initialize the PERF_LIMIT_ACTIVE_DATA mask and data.
    boardObjGrpMaskInit_E32(&((*ppLimits)->activeDataReleasedMask));
    // Initialize the PERF_LIMIT_ACTIVE_DATA array
    for (i = 0; i < PERF_PERF_LIMITS_PMU_MAX_LIMITS_ACTIVE; i++)
    {
        PERF_LIMIT_ACTIVE_DATA *pActiveData = perfLimitsActiveDataGet(*ppLimits, i);
        if (pActiveData == NULL)
        {
            //
            // If we hit this case, then we encountered a PERF_LIMIT version
            // that is not supported.
            //
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_HALT();
            goto perfLimitsConstruct_SUPER_exit;
        }
        boardObjGrpMaskBitSet(&((*ppLimits)->activeDataReleasedMask), i);
    }

    // Initialize the various PERF_LIMITS masks.
    boardObjGrpMaskInit_E255(&((*ppLimits)->limitMaskActive));
    boardObjGrpMaskInit_E255(&((*ppLimits)->limitMaskDirty));
    boardObjGrpMaskInit_E255(&((*ppLimits)->limitMaskGpuBoost));

    // Initialize GPU Boost Sync values.
    LW2080_CTRL_PERF_PERF_LIMITS_STATUS_PMU_INIT(&((*ppLimits)->gpuBoostSync));

    //
    // Initialize arbSeqId to _ILWALID.  Will be incremented with any
    // change to PERF_LIMITS' dependent state.
    //
    (*ppLimits)->arbSeqId = LW2080_CTRL_PERF_PERF_LIMITS_ARB_SEQ_ID_ILWALID;

    // Initialize ARBITRATION_OUTPUT_DEFAULT structs.
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_INIT(&((*ppLimits)->arbOutputDefault));

perfLimitsConstruct_SUPER_exit:
    return status;
}

/*!
 * PERF_LIMITS virtual wrapper
 *
 * @copydoc PerfLimitsActiveDataGet
 */
PERF_LIMIT_ACTIVE_DATA *
perfLimitsActiveDataGet
(
    PERF_LIMITS *pLimits,
    LwU8         idx
)
{
    switch (pLimits->version)
    {
        case LW2080_CTRL_PERF_PERF_LIMITS_VERSION_35_10:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_35_10);
            return perfLimitsActiveDataGet_35(pLimits, idx);
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMITS_VERSION_40:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_40);
            return perfLimitsActiveDataGet_35(pLimits, idx);
            break;
        }
    }

    PMU_HALT();
    return NULL;
}

/*!
 * @copydoc PerfLimitsActiveDataAcquire
 */
FLCN_STATUS
perfLimitsActiveDataAcquire
(
    PERF_LIMITS                               *pLimits,
    PERF_LIMIT                                *pLimit,
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT  *pClientInput
)
{
    PERF_LIMIT_ACTIVE_DATA *pActiveData;
    FLCN_STATUS             status = FLCN_ERR_NOT_SUPPORTED;

    // First check that at least one un-acquired ACTIVE_DATA is available.
    if (boardObjGrpMaskIsZero(&(pLimits->activeDataReleasedMask)))
    {
        status = FLCN_ERR_NO_FREE_MEM;
        PMU_BREAKPOINT();
        goto perfLimitsActiveDataAcquire_exit;
    }

    // Find un-acquired ACTIVE_DATA at lowest set bit in activeDataReleasedMask.
    pLimit->activeDataIdx = boardObjGrpMaskBitIdxLowest(&(pLimits->activeDataReleasedMask));
    pActiveData = perfLimitsActiveDataGet(pLimits, pLimit->activeDataIdx);
    if (pActiveData == NULL)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto perfLimitsActiveDataAcquire_exit;
    }
    boardObjGrpMaskBitClr(&(pLimits->activeDataReleasedMask), pLimit->activeDataIdx);

    // Call class-specific handling for ACTIVE_DATA acquire.
    switch (BOARDOBJ_GET_TYPE(&pLimit->super))
    {
        case LW2080_CTRL_PERF_PERF_LIMIT_TYPE_35_10:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_35_10);
            status = perfLimitsActiveDataAcquire_35(pLimits, pLimit, pClientInput);
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_TYPE_40:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_40);
            status = perfLimitsActiveDataAcquire_35(pLimits, pLimit, pClientInput);
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_HALT();
        goto perfLimitsActiveDataAcquire_exit;
    }

    // Save off the CLIENT_INPUT within the ACTIVE_DATA.
    pActiveData->clientInput = *pClientInput;
    pClientInput = &pActiveData->clientInput;

    // Init/Acquire any _TYPE-specific resources.
    switch (pClientInput->type)
    {
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VOLTAGE:
        {
            LwU8 e;

            for (e = 0; e < pClientInput->data.voltage.numElements; e++)
            {
                LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT *pElement =
                    &pClientInput->data.voltage.elements[e];
                LwVfeEquIdx elementVfeIdxToUse;
                switch (pElement->type)
                {
                    //
                    // _ELEMENT_TYPE_VFE needs to reserve a handle in the
                    // VFE_EQU_MONITOR to evaluate the corresponding VFE index.
                    //
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_VFE:
                    {
                        // Allocate the VFE_EQU_MONITOR index.
                        status = vfeEquMonitorAcquire(PERF_PMU_VFE_EQU_MONITOR_GET(),
                                    &pElement->data.vfe.vfeEquMonIdx);
                        if (status != FLCN_OK)
                        {
                            PMU_BREAKPOINT();
                            pElement->data.vfe.vfeEquMonIdx =
                                VFE_EQU_MONITOR_IDX_ILWALID;
                            goto perfLimitsActiveDataAcquire_exit;
                        }

                        // Specify the input to the VFE_EQU_MONITOR
                        VFE_EQU_IDX_COPY_RM_TO_PMU(elementVfeIdxToUse, pElement->data.vfe.vfeEquIdx);
                        status = vfeEquMonitorSet(PERF_PMU_VFE_EQU_MONITOR_GET(),
                                    pElement->data.vfe.vfeEquMonIdx,
                                    elementVfeIdxToUse,
                                    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_VOLT_UV,
                                    0,
                                    NULL);
                        if (status != FLCN_OK)
                        {
                            PMU_BREAKPOINT();
                            // Must release the previously allocated VFE_EQU_MONITOR handle.
                            (void)vfeEquMonitorRelease(PERF_PMU_VFE_EQU_MONITOR_GET(),
                                                 pElement->data.vfe.vfeEquMonIdx);
                            pElement->data.vfe.vfeEquMonIdx =
                                VFE_EQU_MONITOR_IDX_ILWALID;
                            goto perfLimitsActiveDataAcquire_exit;
                        }

                        pLimits->bVfeEquMonitorDirty = LW_TRUE;
                        break;
                    }
                    // Other _ELEMENT_TYPEs don't require special handling.
                    default:
                    {
                        break;
                    }
                }
            }
            break;
        }
        // Other _TYPEs don't require special handling.
        default:
        {
            break;
        }
    }

perfLimitsActiveDataAcquire_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsActiveDataRelease
 */
void
perfLimitsActiveDataRelease
(
    PERF_LIMITS *pLimits,
    PERF_LIMIT  *pLimit
)
{
    PERF_LIMIT_ACTIVE_DATA *pActiveData;

    // Sanity check that the ACTIVE_DATA element is acquired before freeing.
    if (boardObjGrpMaskBitGet(&(pLimits->activeDataReleasedMask),
                              pLimit->activeDataIdx))
    {
        PMU_BREAKPOINT();
        return;
    }

    // Release any _TYPE-specific resources.
    pActiveData = perfLimitsActiveDataGet(pLimits, pLimit->activeDataIdx);
    if (pActiveData == NULL)
    {
        PMU_BREAKPOINT();
        return;
    }

    switch (pActiveData->clientInput.type)
    {
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VOLTAGE:
        {
            LwU8 e;

            for (e = 0; e < pActiveData->clientInput.data.voltage.numElements; e++)
            {
                LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT *pElement =
                    &pActiveData->clientInput.data.voltage.elements[e];
                switch (pElement->type)
                {
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_VFE:
                    {
                        if (VFE_EQU_MONITOR_IDX_ILWALID == pElement->data.vfe.vfeEquMonIdx)
                        {
                            break;
                        }

                        if (FLCN_OK != vfeEquMonitorRelease(PERF_PMU_VFE_EQU_MONITOR_GET(),
                                        pElement->data.vfe.vfeEquMonIdx))
                        {
                            PMU_BREAKPOINT();
                        }
                        break;
                    }
                    // Other _ELEMENT_TYPEs don't require special handling.
                    default:
                    {
                        break;
                    }
                }
            }
            break;
        }
        // Other _TYPEs don't require special handling.
        default:
        {
            break;
        }
    }

    //
    // Clear this ACTIVE_DATA's index in the acquired mask to enable for
    // allocation by other PERF_LIMITs.
    //
    boardObjGrpMaskBitSet(&(pLimits->activeDataReleasedMask), pLimit->activeDataIdx);
}

/*!
 * @copydoc PerfLimitsLoad
 */
FLCN_STATUS
perfLimitsLoad
(
    PERF_LIMITS *pLimits,
    LwBool       bLoad,
    LwBool       bApply,
    LwU32        applyFlags
)
{
    FLCN_STATUS status = FLCN_OK;

    pLimits->bLoaded = bLoad;
    if (bLoad)
    {
        OSTASK_OVL_DESC ovlDescListArbitrate[] = {
            PERF_LIMIT_OVERLAYS_ARBITRATE
        };

        // Cache the default arbitration output.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListArbitrate);
        status = perfLimitsArbOutputDefaultCache(pLimits, &pLimits->arbOutputDefault);
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListArbitrate);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsLoad_exit;
        }

        //
        // If requested, call into @ref perfLimitsrbitrateAndApply() to kick off
        // first arbitration run and apply the output to the CHANGE_SEQ.
        //
        if (bApply)
        {
            PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE changeRequest;
            PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE_INIT(&changeRequest, PMU);

            if ((LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC & applyFlags) == 0)
            {
                //
                // Should not be making synchronous requests in this code path.
                // Need to re-evaluation that assumption if we are.
                //
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_ARGUMENT;
                goto perfLimitsLoad_exit;
            }

            status = perfLimitsArbitrateAndApply(pLimits, applyFlags, &changeRequest);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitsLoad_exit;
            }
        }
    }

perfLimitsLoad_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsIlwalidate
 */
FLCN_STATUS
perfLimitsIlwalidate
(
    PERF_LIMITS *pLimits,
    LwBool       bApply,
    LwU32        applyFlags
)
{
    // Mark all limits as dirty so they get re-evaluated on next arbitration
    (void)boardObjGrpMaskCopy(&(pLimits->limitMaskDirty),
                              &(pLimits->limitMaskActive));

    // Increment arbSeqId
    PERF_LIMITS_ARB_SEQ_ID_INCREMENT(pLimits);

    //
    // If perf limit is NOT loaded, bail early. Perf limit load cmd
    // from within the pmuRpcPerfLoad will do the loading.
    //
    if (!pLimits->bLoaded)
    {
        return FLCN_OK;
    }

    //
    // Call perfLimitsLoad() which will reload all the ilwalidated state and
    // trigger perfLimitsArbitrateAndApply() with flags | _FORCE.
    //
    return perfLimitsLoad(pLimits, LW_TRUE, bApply,
            LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE |
                LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC |
                applyFlags);
}

/*!
 * PERF_LIMITS super implementation.
 *
 * @copydoc PerfLimitsArbitrate
 */
FLCN_STATUS
perfLimitsArbitrate
(
    PERF_LIMITS                    *pLimits,
    PERF_LIMITS_ARBITRATION_OUTPUT *pArbOutput,
    BOARDOBJGRPMASK_E255           *pLimitMaskExclude,
    LwBool                          bMin
)
{
    CLK_DOMAINS     *pDomains   = CLK_CLK_DOMAINS_GET();
    BOARDOBJGRP_E32 *pVoltRails = &(VOLT_RAILS_GET()->super);
    FLCN_STATUS      status     = FLCN_OK;

    //
    // Can't run arbitration algorithm until PERF_LIMITS are loaded.  Abort if
    // not loaded.
    //
    if (!pLimits->bLoaded)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_exit;
    }

    // If all clock domains are fixed, short circuit.
    if ((pDomains == NULL) ||
        boardObjGrpMaskIsZero(&(pDomains->progDomainsMask)))
    {
        status = FLCN_OK;
        goto perfLimitsArbitrate_exit;
    }

    //
    // Sanity check that PERF_LIMITS is sized to support the
    // appropriate number of CLK_DOMAINs and VOLT_RAILs for this GPU.
    //
    if (boardObjGrpMaskBitIdxHighest(&(pDomains->progDomainsMask))
            >= PERF_PERF_LIMITS_CLK_CLK_DOMAIN_MAX_DOMAINS)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_exit;
    }

    if ((pVoltRails == NULL) ||
        (boardObjGrpMaskBitIdxHighest(&(pVoltRails->objMask))
            >= PERF_PERF_LIMITS_VOLT_VOLT_RAIL_MAX_RAILS))
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_exit;
    }

    //
    // Determine if caching state is valid and if the cached result tuple can be
    // reused or arbitration algorithm must be re-used.
    //
    if (pLimits->bCachingEnabled)
    {
        BOARDOBJGRPMASK_E255 limitMaskExcludeLast;

        boardObjGrpMaskInit_E255(&limitMaskExcludeLast);
        status = boardObjGrpMaskImport_E255(&limitMaskExcludeLast,
                                            &(pArbOutput->tuple.cacheData.limitMaskExclude));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrate_exit;
        }

        //
        // Can re-use cached data if the following CACHE_DATA matches:
        // 1. arbSeqId
        // 2. limitMaskExclude
        //
        if ((pLimits->arbSeqId == pArbOutput->tuple.cacheData.arbSeqId) &&
            (((pLimitMaskExclude != NULL) &&
              boardObjGrpMaskIsEqual(pLimitMaskExclude, &limitMaskExcludeLast)) ||
             ((pLimitMaskExclude == NULL) &&
              boardObjGrpMaskIsZero(&limitMaskExcludeLast))))
        {
            pArbOutput->tuple.cacheData.bDirty = LW_FALSE;
            goto perfLimitsArbitrate_exit;
        }
    }

    // Going to re-run the arbitration algorithm.  Mark the output as dirty.
    pArbOutput->tuple.cacheData.bDirty = LW_TRUE;

    // Initialize all the elements of the PERF_LIMITS_ARBITRATION_OUTPUT structure.
    pArbOutput->version = pLimits->version;
    status = boardObjGrpMaskCopy(&(pArbOutput->clkDomainsMask),
                                 &(pDomains->progDomainsMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_exit;
    }
    status = boardObjGrpMaskCopy(&(pArbOutput->voltRailsMask),
                                 &(pVoltRails->objMask));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_exit;
    }

    //
    // If the PERF_LIMIT VFE_EQU_MONITOR state has been dirtied, update the
    // VFE_EQU_MONITOR.
    //
    if (pLimits->bVfeEquMonitorDirty)
    {
        //
        // ".dmem_perfVfe" and ".dmem_perfLimit" are too large to be coresident
        // with ".dmem_perf".  So, must temporarily detach ".dmem_perfLimit" to
        // attach ".dmem_perfVfe" and update the VFE_EQU_MONITOR.
        //
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_BUILD(_DMEM, _DETACH, perfLimit)
            OSTASK_OVL_DESC_DEFINE_VFE(FULL)
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            status = vfeEquMonitorUpdate(PERF_PMU_VFE_EQU_MONITOR_GET(),
                        LW_FALSE);
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrate_exit;
        }

        pLimits->bVfeEquMonitorDirty = LW_FALSE;
    }

    // Call version/implementation-specific arbitration algorithm.
    {
        OSTASK_OVL_DESC ovlDescListArbitrate[] = {
            PERF_LIMIT_OVERLAYS_ARBITRATE
        };

        status = FLCN_ERR_NOT_SUPPORTED;
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescListArbitrate);    
        switch (pLimits->version)
        {
            case LW2080_CTRL_PERF_PERF_LIMITS_VERSION_35_10:
            {
                PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_35_10);
                status = perfLimitsArbitrate_35(pLimits, pArbOutput,
                            pLimitMaskExclude, bMin);
                break;
            }
            case LW2080_CTRL_PERF_PERF_LIMITS_VERSION_40:
            {
                PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_40);
                status = perfLimitsArbitrate_35(pLimits, pArbOutput,
                            pLimitMaskExclude, bMin);
                break;
            }
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescListArbitrate);
    }
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsArbitrate_exit;
    }

    // Algorithm is complete, save the cache data.
    pArbOutput->tuple.cacheData.arbSeqId = pLimits->arbSeqId;
    if (pLimitMaskExclude != NULL)
    {
        status = boardObjGrpMaskExport_E255(pLimitMaskExclude,
            &(pArbOutput->tuple.cacheData.limitMaskExclude));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrate_exit;
        }
    }
    //
    // pLimitMaskExclude == NULL means limitMaskExclude is zero.  Init the
    // CACHE_DATA limitMaskExclude to zero.
    //
    else
    {
        LW2080_CTRL_BOARDOBJGRP_MASK_E255_INIT(
            &pArbOutput->tuple.cacheData.limitMaskExclude.super);
    }

perfLimitsArbitrate_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsArbitrateAndApply
 */
FLCN_STATUS
perfLimitsArbitrateAndApply
(
    PERF_LIMITS                              *pLimits,
    LwU32                                     applyFlags,
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE *pChangeRequest
)
{
    CLK_DOMAINS            *pDomains   = CLK_CLK_DOMAINS_GET();
    BOARDOBJGRPMASK_E255    limitMaskGpuBoostDirty;
    CLK_DOMAIN             *pDomain;
    VOLT_RAIL              *pVoltRail;
    LwBoardObjIdx           i;
    LwBool                  bSync      =
        ((applyFlags & LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC) == 0);
    LwBool                  bChangeRequired         = LW_FALSE;
    FLCN_STATUS             status                  = FLCN_OK;
    OSTASK_OVL_DESC         perfChangeOvlDescList[] =
    {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, perfVfeEquMonitor)
        OSTASK_OVL_DESC_BUILD(_DMEM, _DETACH, perfLimit)
        CHANGE_SEQ_OVERLAYS_BASE
    };
    OSTASK_OVL_DESC         ovlDescList[]           =
    {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfLimitChangeSeqChange)
    };

    //
    // If PERF_LIMITS not loaded, return early with an error code dependent on
    // the type of request was made.
    //
    if (!pLimits->bLoaded)
    {
        status = (!bSync) ? FLCN_OK : FLCN_ERROR;
        goto perfLimitsArbitrateAndApply_exit;
    }

    // If all clock domains are fixed, short circuit.
    if ((pDomains == NULL) ||
        boardObjGrpMaskIsZero(&(pDomains->progDomainsMask)))
    {
        status = FLCN_OK;
        goto perfLimitsArbitrateAndApply_exit;
    }

    //
    // Sanity check - if synchronous change, verify the following:
    // 1. the change request pointer != NULL
    // 2. if request comes from the PMU, verify the queue handle != NULL
    //
    if (bSync)
    {
        if (pChangeRequest == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfLimitsArbitrateAndApply_exit;
        }
        if ((LW2080_CTRL_PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_PMU == pChangeRequest->clientId) &&
            (pChangeRequest->data.pmu.queueHandle == NULL))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfLimitsArbitrateAndApply_exit;
        }
    }

    //
    // Return early if the arbiter is locked (not allowed to submit changes to
    // change sequencer).
    //
    if (pLimits->bArbitrateAndApplyLock)
    {
        status = FLCN_OK;
        goto perfLimitsArbitrateAndApply_exit;
    }

    // Determine if any GPU Boost limits have been modified.
    boardObjGrpMaskInit_E255(&limitMaskGpuBoostDirty);
    if (pLimits->gpuBoostSync.bEnabled)
    {
        status = boardObjGrpMaskAnd(&limitMaskGpuBoostDirty,
            &pLimits->limitMaskDirty,
            &pLimits->limitMaskGpuBoost);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrateAndApply_exit;
        }
    }

    // Call into perfLimitsArbitrate() to find target result tuple.
    status = perfLimitsArbitrate(pLimits, pLimits->pArbOutputApply, NULL,
                pLimits->bApplyMin);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsArbitrateAndApply_exit;
    }

    // Update the GPU Boost Sync limits
    if ((pLimits->gpuBoostSync.bEnabled &&
        !boardObjGrpMaskIsZero(&limitMaskGpuBoostDirty)) ||
        pLimits->gpuBoostSync.bForce)
    {
        pLimits->gpuBoostSync.bForce = LW_FALSE;
        status = s_perfLimitsArbitrateGpuBoostSyncLimits(pLimits);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrateAndApply_exit;
        }
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT *pChangeSeqChange =
            pLimits->pChangeSeqChange;

        // Verify a change request is present.
        if (pChangeRequest == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfLimitsArbitrateAndApply_changeSeqDetach;
        }

        // If _FORCE flag is set, always inject change.
        if (LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE ==
                (LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_FORCE & applyFlags))
        {
            bChangeRequired = LW_TRUE;
        }
        else
        {
            //
            // Compare the next request with last request and drop the duplicate request
            // if NOT forced by client.
            //
            if ((pChangeSeqChange->flags != applyFlags) ||
                (pChangeSeqChange->vfPointsCacheCounter != clkVfPointsVfPointsCacheCounterGet()) ||
                (pChangeSeqChange->pstateIndex !=
                    pLimits->pArbOutputApply->tuple.pstateIdx.value))
            {
                bChangeRequired = LW_TRUE;
            }

            // This code assumes that the clock domain mask and volt rail mask will NOT change.

            BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
                &pLimits->pArbOutputApply->clkDomainsMask.super)
            {
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN *pTupleClkDomain;

                PMU_ASSERT_OK_OR_GOTO(status,
                    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_GET(
                        &pLimits->pArbOutputApply->tuple, i, &pTupleClkDomain),
                    perfLimitsArbitrateAndApply_changeSeqDetach);

                if (pChangeSeqChange->clk[i].clkFreqkHz !=
                        pTupleClkDomain->freqkHz.value)
                {
                    bChangeRequired = LW_TRUE;
                }
            }
            BOARDOBJGRP_ITERATOR_END;

            BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pVoltRail, i,
                &pLimits->pArbOutputApply->voltRailsMask.super)
            {
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL *pTupleVoltRail;

                PMU_ASSERT_OK_OR_GOTO(status,
                    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_GET(
                        &pLimits->pArbOutputApply->tuple, i, &pTupleVoltRail),
                    perfLimitsArbitrateAndApply_changeSeqDetach);

                if ((pChangeSeqChange->volt[i].voltageuV !=
                        pTupleVoltRail->voltageuV.value) ||
                (pChangeSeqChange->volt[i].voltageMinNoiseUnawareuV !=
                        pTupleVoltRail->voltageNoiseUnawareMinuV.value))
                {
                    bChangeRequired = LW_TRUE;
                }
            }
            BOARDOBJGRP_ITERATOR_END;
        }

        // Build change input struct if perf change is required.
        if (bChangeRequired)
        {
            // Queue the results for the change sequencer to handle.
            memset(pChangeSeqChange, 0x00, sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT));
            pChangeSeqChange->flags = applyFlags;
            pChangeSeqChange->vfPointsCacheCounter = clkVfPointsVfPointsCacheCounterGet();

            // Populate P-state information
            pChangeSeqChange->pstateIndex =
                pLimits->pArbOutputApply->tuple.pstateIdx.value;

            // Populate clock domain information
            status = boardObjGrpMaskExport_E32(&(pLimits->pArbOutputApply->clkDomainsMask),
                        &(pChangeSeqChange->clkDomainsMask));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitsArbitrateAndApply_changeSeqDetach;
            }

            BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
                &pLimits->pArbOutputApply->clkDomainsMask.super)
            {
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN *pTupleClkDomain;

                PMU_ASSERT_OK_OR_GOTO(status,
                    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_GET(
                        &pLimits->pArbOutputApply->tuple, i, &pTupleClkDomain),
                    perfLimitsArbitrateAndApply_changeSeqDetach);

                pChangeSeqChange->clk[i].clkFreqkHz = pTupleClkDomain->freqkHz.value;
            }
            BOARDOBJGRP_ITERATOR_END;

            // Populate volt rail information
            status = boardObjGrpMaskExport_E32(&(pLimits->pArbOutputApply->voltRailsMask),
                        &(pChangeSeqChange->voltRailsMask));
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitsArbitrateAndApply_changeSeqDetach;
            }

            BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pVoltRail, i,
                &pLimits->pArbOutputApply->voltRailsMask.super)
            {
                LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL *pTupleVoltRail;

                PMU_ASSERT_OK_OR_GOTO(status,
                    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_VOLT_RAIL_GET(
                        &pLimits->pArbOutputApply->tuple, i, &pTupleVoltRail),
                    perfLimitsArbitrateAndApply_changeSeqDetach);

                pChangeSeqChange->volt[i].voltageuV =
                    pTupleVoltRail->voltageuV.value;
                pChangeSeqChange->volt[i].voltageMinNoiseUnawareuV =
                    pTupleVoltRail->voltageNoiseUnawareMinuV.value;
            }
            BOARDOBJGRP_ITERATOR_END;

            // Temporary HACK to test MSVDD on TURING_AMPERE.
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_40_TEST))
            {
                //
                // LWVDD must not go below the MSVDD voltage requirement.
                // Also LWVDD noise unaware Vmin = 0 as it does not have any noise
                // unaware clocks. This is not acceptable to voltage sanity so for
                // now set LWVDD noise unaware Vmin equal to MSVDD noise unaware Vmin.
                //
                pChangeSeqChange->volt[0].voltageuV =
                    LW_MAX(pChangeSeqChange->volt[0].voltageuV,
                           pChangeSeqChange->volt[1].voltageuV);
                pChangeSeqChange->volt[0].voltageMinNoiseUnawareuV =
                    LW_MAX(pChangeSeqChange->volt[0].voltageMinNoiseUnawareuV,
                           pChangeSeqChange->volt[1].voltageMinNoiseUnawareuV);

                //
                // This patch is required if someone just enable MSVDD but no clock domain
                // is running on MSVDD.
                //
                if ((pChangeSeqChange->volt[1].voltageuV == 0U) ||
                    (pChangeSeqChange->volt[1].voltageMinNoiseUnawareuV == 0U))
                {
                    pChangeSeqChange->volt[1].voltageuV =
                        LW_MAX(pChangeSeqChange->volt[0].voltageuV,
                               pChangeSeqChange->volt[1].voltageuV);
                    pChangeSeqChange->volt[1].voltageMinNoiseUnawareuV =
                        LW_MAX(pChangeSeqChange->volt[0].voltageMinNoiseUnawareuV,
                               pChangeSeqChange->volt[1].voltageMinNoiseUnawareuV);
                }
            }
        }

        // Attach change sequencer overlays.
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(perfChangeOvlDescList);
        {
            // If change is not required, just send completion signal and bail.
            if (!bChangeRequired)
            {
                status = perfChangeSeqSendCompletionNotification(bSync, pChangeRequest);
            }
            else
            {
                // Queue the target result tuple to Perf Change Sequencer.
                status = perfChangeSeqQueueChange(pChangeSeqChange, pChangeRequest);
            }
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(perfChangeOvlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbitrateAndApply_changeSeqDetach;
        }
    }
perfLimitsArbitrateAndApply_changeSeqDetach:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitsArbitrateAndApply_exit;
    }

perfLimitsArbitrateAndApply_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsDefaultArbOutputCache
 */
FLCN_STATUS
perfLimitsArbOutputDefaultCache
(
    PERF_LIMITS                            *pLimits,
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT *pArbOutputDefault
)
{
    CLK_DOMAINS *pDomains   = CLK_CLK_DOMAINS_GET();
    CLK_DOMAIN  *pDomain;
    PERF_LIMITS_PSTATE_RANGE pstateRange;
    LwBoardObjIdx d;
    FLCN_STATUS   status     = FLCN_OK;
    LwU8          i;

    // Sanity check the pDomains pointer.
    if (pDomains == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_STATE;
        goto perfLimitsArbOutputDefaultCache_exit;
    }

    // Reinit the ARBITRATION_OUTPUT_DEFAULT structure to erase any old state.
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_INIT(pArbOutputDefault);

    // Initialize the PSTATE range to all pstates
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR(i)
    {
        if (LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MIN == i)
        {
            pstateRange.values[i] =
                boardObjGrpMaskBitIdxLowest(&(PSTATES_GET()->super.objMask));
        }
        else if (LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_IDX_MAX == i)
        {
            pstateRange.values[i] =
                boardObjGrpMaskBitIdxHighest(&(PSTATES_GET()->super.objMask));
        }
        else
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_STATE;
            goto perfLimitsArbOutputDefaultCache_exit;
        }
    }
    PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_ITERATOR_END;

    // Store the PSTATE range in ARBITRATION_OUTPUT_DEFAULT::tuples[].pstateIdx
    PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLES_SET_RANGE(pArbOutputDefault,
        &pstateRange, pstateIdx);

    //
    // Iterate over each programmable CLK_DOMAIN (i.e. CLK_DOMAIN_3X_PROG) and
    // set its bounds to the range corresponding to the PSTATEs.
    //
    (void)boardObjGrpMaskCopy(&(pArbOutputDefault->clkDomainsMask),
                              &(pDomains->progDomainsMask));
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, d,
        &pArbOutputDefault->clkDomainsMask.super)
    {
        PERF_LIMITS_VF_RANGE vfDomainRange;

        // Look up the frequency range corresponding to the PSTATE range.
        vfDomainRange.idx = BOARDOBJ_GRP_IDX_TO_8BIT(d);
        status = perfLimitsPstateIdxRangeToFreqkHzRange(&pstateRange,
                    &vfDomainRange);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitsArbOutputDefaultCache_exit;
        }

        //
        // Unpack the PSTATE frequency range values VF_RANGE ->
        // ARBITRATION_OUTPUT_DEFAULT::tuples[].clkDomains[].
        //
        PMU_ASSERT_OK_OR_GOTO(status,
            PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_TUPLES_CLK_DOMAIN_SET_RANGE(
                pArbOutputDefault, &vfDomainRange),
            perfLimitsArbOutputDefaultCache_exit);
    }
    BOARDOBJGRP_ITERATOR_END;

perfLimitsArbOutputDefaultCache_exit:
    return status;
}

/*!
 * @copydoc PerfLimitsEvtHandlerClient
 */
FLCN_STATUS
perfLimitsEvtHandlerClient
(
    DISPATCH_PERF_LIMITS_CLIENT *pDispatch
)
{
    PERF_LIMITS_CLIENT *pClient = pDispatch->pClient;
    PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE changeRequest;
    FLCN_STATUS status = FLCN_OK;
    LwU32       applyFlags = 0;
    LwU8        i;

    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD_IDX(_DMEM, _ATTACH, _LS, pDispatch->dmemOvl)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    PERF_LIMITS_CLIENT_SEMAPHORE_TAKE(pClient);
    {
        if (!PERF_PERF_LIMITS_GET()->bLoaded)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfLimitsEvtHandlerClient_exit;
        }

        // Copy out the arbitration flags.
        applyFlags = pClient->hdr.applyFlags;

        // Copy out PMU client limits
        for (i = 0; i < pClient->hdr.numEntries; i++)
        {
            PERF_LIMIT *pLimit;

            // Skip over unset limits
            if (LW2080_CTRL_PERF_PERF_LIMIT_ID_NOT_SET == pClient->pEntries[i].limitId)
            {
                continue;
            }

            pLimit = PERF_PERF_LIMIT_GET(pClient->pEntries[i].limitId);
            if (pLimit == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto perfLimitsEvtHandlerClient_exit;
            }

            // Set the CLIENT_INPUT from the PMU.
            if (FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _SOURCE, _PMU, pLimit->flags))
            {
                status = perfLimitClientInputSet(pLimit, &pClient->pEntries[i].clientInput);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto perfLimitsEvtHandlerClient_exit;
                }
            }
            else
            {
                // Encountered a CPU-based limit being set.
                PMU_BREAKPOINT();
            }
        }

perfLimitsEvtHandlerClient_exit:
        // Mark limits as processed
        pClient->hdr.bQueued = LW_FALSE;

        // Fill out the change request.
        PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE_INIT(&changeRequest, PMU);
        if ((pClient->hdr.applyFlags & LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_ASYNC) == 0)
        {
            changeRequest.data.pmu.queueHandle = pClient->hdr.syncQueueHandle;
        }

        changeRequest.timeStart = pClient->hdr.timeStart;
    }
    PERF_LIMITS_CLIENT_SEMAPHORE_GIVE(pClient);
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    // Initiate arbitration if the client limits were successfully updated.
    if (status == FLCN_OK)
    {
        PERF_LIMITS *pLimits = PERF_PERF_LIMITS_GET();
        status = perfLimitsArbitrateAndApply(pLimits, applyFlags, &changeRequest);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
        }
    }

    return status;
}

/*!
 * @copydoc PerfLimitClientInputSet
 */
FLCN_STATUS
perfLimitClientInputSet
(
    PERF_LIMIT                               *pLimit,
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput
)
{
    PERF_LIMITS *pLimits          = PERF_PERF_LIMITS_GET();
    FLCN_STATUS  status           = FLCN_OK;
    LwBool       bChangeRequested = LW_FALSE;

    // If new CLIENT_INPUT is ACTIVE, set the PERF_LIMIT's index in the active mask.
    if (LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_ACTIVE(pClientInput))
    {
        //
        // Dirty the PERF_LIMIT, acquire ACTIVE_DATA and store the CLIENT_INPUT,
        // for the following cases:
        // 1. INACTIVE -> ACTIVE transitions.
        // 2. ACTIVE -> ACTIVE, but CLIENT_INPUT is changing.
        //
        if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID != pLimit->activeDataIdx)
        {
            PERF_LIMIT_ACTIVE_DATA *pActiveData =
                perfLimitsActiveDataGet(pLimits, pLimit->activeDataIdx);
            if (pActiveData == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto perfLimitClientInputSet_exit;
            }
            bChangeRequested = !perfLimitClientInputCompare(&pActiveData->clientInput, pClientInput);
        }

        if ((PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID == pLimit->activeDataIdx) ||
            (bChangeRequested))
        {
            //
            // If previous ACTIVE_DATA acquired, release it to free any of the
            // other resources acquired (e.g. VFE_PMU_MONITOR).  Will re-acquire
            // ACTIVE_DATA next.
            //
            if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID != pLimit->activeDataIdx)
            {
                perfLimitsActiveDataRelease(pLimits, pLimit);
                pLimit->activeDataIdx = PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID;
            }

            //
            // Acquire an ACTIVE_DATA structure and initialize it with the
            // CLIENT_INPUT.
            //
            status = perfLimitsActiveDataAcquire(pLimits, pLimit, pClientInput);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfLimitClientInputSet_exit;
            }

            if (BOARDOBJ_GET_GRP_IDX(&pLimit->super) == LW2080_CTRL_PERF_PERF_LIMIT_ID_NOT_SET)
            {
                PMU_BREAKPOINT();
            }

            boardObjGrpMaskBitSet(&(pLimits->limitMaskDirty),  BOARDOBJ_GET_GRP_IDX(&pLimit->super));
        }

        boardObjGrpMaskBitSet(&(pLimits->limitMaskActive), BOARDOBJ_GET_GRP_IDX(&pLimit->super));
    }
    // If new client input is DISABLED, clear the PERF_LIMIT's index in the active mask.
    else
    {
        // If ACTIVE_DATA structure is acquired for this PERF_LIMIT, release it.
        if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID != pLimit->activeDataIdx)
        {
            perfLimitsActiveDataRelease(pLimits, pLimit);
            pLimit->activeDataIdx = PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID;
        }

        boardObjGrpMaskBitClr(&(pLimits->limitMaskActive), BOARDOBJ_GET_GRP_IDX(&pLimit->super));
    }

    // Increment arbSeqId
    PERF_LIMITS_ARB_SEQ_ID_INCREMENT(pLimits);

perfLimitClientInputSet_exit:
    return status;
}

/*!
 * @copydoc PerfLimitClientInputCompare
 */
LwBool
perfLimitClientInputCompare
(
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput0,
    LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT *pClientInput1
)
{
    //
    // Set equality to LW_FALSE by default.  Must pass all checks below and then
    // can assume equality.
    //
    LwBool bEqual = LW_FALSE;

    // 1. Check that _TYPE is equivalent.
    if (pClientInput0->type != pClientInput1->type)
    {
        goto perfLimitClientInputCompare_exit;
    }

    // 2. Do _TYPE-specific checking.
    switch (pClientInput0->type)
    {
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_PSTATE_IDX:
        {
            if (memcmp(&pClientInput0->data.pstate, &pClientInput1->data.pstate,
                       sizeof(pClientInput0->data.pstate)) != 0)
            {
                goto perfLimitClientInputCompare_exit;
            }
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_FREQUENCY:
        {
            if (memcmp(&pClientInput0->data.freq, &pClientInput1->data.freq,
                       sizeof(pClientInput0->data.freq)) != 0)
            {
                goto perfLimitClientInputCompare_exit;
            }
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VPSTATE_IDX:
        {
            if (memcmp(&pClientInput0->data.vpstate, &pClientInput1->data.vpstate,
                       sizeof(pClientInput0->data.vpstate)) != 0)
            {
                goto perfLimitClientInputCompare_exit;
            }
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_VOLTAGE:
        {
            LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT *pElement0;
            LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT *pElement1;
            LwU8 e;

            // Check the non-ELEMENT-specific data.
            if ((pClientInput0->data.voltage.voltRailIdx != pClientInput1->data.voltage.voltRailIdx) ||
                (pClientInput0->data.voltage.numElements != pClientInput1->data.voltage.numElements) ||
                (pClientInput0->data.voltage.deltauV     != pClientInput1->data.voltage.deltauV))
            {
                goto perfLimitClientInputCompare_exit;
            }

            // Loop over all ELEMENTs and confirm they are equal
            for (e = 0; e < pClientInput0->data.voltage.numElements; e++)
            {
                pElement0 = &pClientInput0->data.voltage.elements[e];
                pElement1 = &pClientInput1->data.voltage.elements[e];

                // Check non-ELEMENT_TYPE-specific data.
                if ((pElement0->type != pElement1->type) ||
                    (pElement0->deltauV != pElement1->deltauV))
                {
                    goto perfLimitClientInputCompare_exit;
                }

                // Check ELEMENT_TYPE-specific data.
                switch (pElement0->type)
                {
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_LOGICAL:
                    {
                        if (memcmp(&pElement0->data.logical, &pElement1->data.logical,
                                   sizeof(pElement0->data.logical)) != 0)
                        {
                            goto perfLimitClientInputCompare_exit;
                        }
                        break;
                    }
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_VFE:
                    {
                        if (pElement0->data.vfe.vfeEquIdx != pElement1->data.vfe.vfeEquIdx)
                        {
                            goto perfLimitClientInputCompare_exit;
                        }
                        break;
                    }
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_PSTATE:
                    {
                        if (memcmp(&pElement0->data.pstate, &pElement1->data.pstate,
                                   sizeof(pElement0->data.pstate)) != 0)
                        {
                            goto perfLimitClientInputCompare_exit;
                        }
                        break;
                    }
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_VPSTATE:
                    {
                        if (memcmp(&pElement0->data.vpstateIdx, &pElement1->data.vpstateIdx,
                                   sizeof(pElement0->data.vpstateIdx)) != 0)
                        {
                            goto perfLimitClientInputCompare_exit;
                        }
                        break;
                    }
                    case LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_DATA_VOLTAGE_ELEMENT_TYPE_FREQUENCY:
                    {
                        if (memcmp(&pElement0->data.frequency, &pElement1->data.frequency,
                                   sizeof(pElement0->data.frequency)) != 0)
                        {
                            goto perfLimitClientInputCompare_exit;
                        }
                        break;
                    }
                    //
                    // Unknown _ELEMENT_TYPE.  This is to catch errors if new
                    // _TYPEs are added and this function is not correctly
                    // updated.
                    //
                    default:
                    {
                        PMU_BREAKPOINT();
                        goto perfLimitClientInputCompare_exit;
                        break;
                    }
                }
            }
            break;
        }
        //
        // Unknown _TYPE.  This is to catch errors if new _TYPEs are added and
        // this function is not correctly updated.
        //
        default:
        {
            PMU_BREAKPOINT();
            goto perfLimitClientInputCompare_exit;
            break;
        }
    }

    // All comparison checks above have passed, so structures are equal.
    bEqual = LW_TRUE;

perfLimitClientInputCompare_exit:
    return bEqual;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfLimitBoardObjGrpIfaceModel10Set
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_SET_HEADER *pHeader =
        (RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_SET_HEADER *)pBuffer->pBuf;
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfLimitBoardobj)
        PERF_LIMIT_OVERLAYS_BASE
    };

#if PMUCFG_FEATURE_ENABLED(PMU_FBQ) && PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING)
    if (pBuffer->size <
        sizeof(RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_SET_HEADER_ALIGNED))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_OUT_OF_RANGE;
        goto perfLimitBoardObjGrpIfaceModel10Set_done; // NJ??
    }
#endif

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    status = BOARDOBJGRP_IFACE_MODEL_10_SET(E255,            // _grpType
        PERF_LIMIT,                                 // _class
        pBuffer,                                    // _pBuffer
        s_perfLimitIfaceModel10SetHeader,                 // _hdrFunc
        s_perfLimitIfaceModel10SetEntry,                  // _entryFunc
        turingAndLater.perf.perfLimitGrpSet,        // _ssElement
        OVL_INDEX_DMEM(perfLimit),                  // _ovlIdxDmem
        BoardObjGrpVirtualTablesNotImplemented);    // _ppObjectVtables
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitBoardObjGrpIfaceModel10Set_done;
    }

    // Read in the header to get the command parameters
    status = boardObjGrpIfaceModel10ReadHeader((void *)pHeader,
        sizeof(RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_SET_HEADER_ALIGNED),
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.perf.perfLimitGrpSet),
        LW_OFFSETOF(RM_PMU_PERF_PERF_LIMIT_BOARDOBJ_GRP_SET, hdr));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitBoardObjGrpIfaceModel10Set_done;
    }

    status = s_perfLimitsProcessConstructCommand(Perf.pLimits, &pHeader->constructCmd);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitBoardObjGrpIfaceModel10Set_done;
    }

    // Write the results out
    status = boardObjGrpIfaceModel10WriteHeader((void *)pHeader,
        sizeof(RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_SET_HEADER_ALIGNED),
        RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(turingAndLater.perf.perfLimitGrpSet),
        LW_OFFSETOF(RM_PMU_PERF_PERF_LIMIT_BOARDOBJ_GRP_SET, hdr));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitBoardObjGrpIfaceModel10Set_done;
    }

perfLimitBoardObjGrpIfaceModel10Set_done:
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10CmdHandler
 */
FLCN_STATUS
perfLimitBoardObjGrpIfaceModel10GetStatus
(
    PMU_DMEM_BUFFER *pBuffer
)
{
    FLCN_STATUS     status        = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _ATTACH, libPerfLimitBoardobj)
        PERF_LIMIT_OVERLAYS_BASE
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = BOARDOBJGRP_IFACE_MODEL_10_GET_STATUS(E255,           // _grpType
            PERF_LIMIT,                                 // _class
            pBuffer,                                    // _pBuffer
            s_perfLimitIfaceModel10GetStatusHeader,                 // _hdrFunc
            perfLimitIfaceModel10GetStatus,                         // _entryFunc
            turingAndLater.perf.perfLimitGrpGetStatus); // _ssElement
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
perfLimitGrpIfaceModel10ObjSetImpl_SUPER
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    PERF_LIMIT             *pLimit;
    RM_PMU_PERF_PERF_LIMIT *pLimitDesc      = (RM_PMU_PERF_PERF_LIMIT *)pBoardObjDesc;
    FLCN_STATUS             status          = FLCN_OK;
    LwBool                  bFirstConstruct = LW_FALSE;

    if (*ppBoardObj == NULL)
    {
        bFirstConstruct = LW_TRUE;
    }

    // Construct SUPER class.
    status = boardObjGrpIfaceModel10ObjSet(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfLimitGrpIfaceModel10ObjSetImpl_SUPER_exit;
    }
    pLimit = (PERF_LIMIT *)*ppBoardObj;

    // Set PERF_LIMIT parameters.
    pLimit->flags             = pLimitDesc->flags;
    pLimit->propagationRegime = pLimitDesc->propagationRegime;
    pLimit->perfPolicyId      = pLimitDesc->perfPolicyId;

    if (bFirstConstruct)
    {
        pLimit->activeDataIdx = PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID;
    }

    // Set the CLIENT_INPUT from the RM.
    if (FLD_TEST_DRF(2080_CTRL_PERF_LIMIT_INFO, _FLAGS, _SOURCE, _CPU, pLimit->flags))
    {
        status = perfLimitClientInputSet(pLimit, &pLimitDesc->rmClientInput);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfLimitGrpIfaceModel10ObjSetImpl_SUPER_exit;
        }
    }
    else if (LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_ACTIVE(&pLimitDesc->rmClientInput))
    {
        // CPU client is attempting to set a PMU-based limit.
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
    }

perfLimitGrpIfaceModel10ObjSetImpl_SUPER_exit:
    return status;
}

/*!
 * PERF_LIMIT GET_STATUS dispatcher function.
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfLimitIfaceModel10GetStatus
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    // Call type-specific status functions.
    switch (BOARDOBJ_GET_TYPE(pBoardObj))
    {
        case LW2080_CTRL_PERF_PERF_LIMIT_TYPE_35_10:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_35_10);
        {
            return perfLimitIfaceModel10GetStatus_35(pModel10, pPayload);
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_TYPE_40:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_40);
        {
            return perfLimitIfaceModel10GetStatus_35(pModel10, pPayload);
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * PERF_LIMIT SUPER implementation
 *
 * @copydoc BoardObjIfaceModel10GetStatus
 */
FLCN_STATUS
perfLimitIfaceModel10GetStatus_SUPER
(
    BOARDOBJ_IFACE_MODEL_10              *pModel10,
    RM_PMU_BOARDOBJ       *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    RM_PMU_PERF_PERF_LIMIT_GET_STATUS *pLimitStatus =
        (RM_PMU_PERF_PERF_LIMIT_GET_STATUS *)pPayload;
    PERF_LIMIT_ACTIVE_DATA *pActiveData;
    PERF_LIMITS            *pLimits = PERF_PERF_LIMITS_GET();
    PERF_LIMIT             *pLimit  = (PERF_LIMIT *)pBoardObj;
    FLCN_STATUS             status  = FLCN_OK;

    // Call super-class impementation;
    status = boardObjIfaceModel10GetStatus(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto perfLimitIfaceModel10GetStatus_SUPER_exit;
    }

    // If ACTIVE_DATA acquired/non-NULL copy out STATUS data from that ACTIVE_DATA.
    if (PERF_LIMIT_ACTIVE_DATA_INDEX_ILWALID != pLimit->activeDataIdx)
    {
        pActiveData = perfLimitsActiveDataGet(pLimits, pLimit->activeDataIdx);
        if (pActiveData == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfLimitIfaceModel10GetStatus_SUPER_exit;
        }
        pLimitStatus->clientInput = pActiveData->clientInput;
    }
    // If ACTIVE_DATA not-acquired/NULL, instead indicate all STATUS data as DISABLED.
    else
    {
        pLimitStatus->clientInput.type =
            LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_DISABLED;
    }

perfLimitIfaceModel10GetStatus_SUPER_exit:
    return status;
}

/*!
 * @brief PERF RPC interface to ilwalidate PERF_LIMITS.
 *
 * @param[in]  pParams  RM_PMU_RPC_STRUCT_PERF_PERF_LIMITS_ILWALIDATE pointer
 */
FLCN_STATUS
pmuRpcPerfPerfLimitsIlwalidate
(
    RM_PMU_RPC_STRUCT_PERF_PERF_LIMITS_ILWALIDATE *pParams
)
{
    FLCN_STATUS status;
    OSTASK_OVL_DESC ovlDescList[] = {
        PERF_LIMIT_OVERLAYS_BASE
    };
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);

    status = perfLimitsIlwalidate(Perf.pLimits, pParams->bApply, pParams->applyFlags);

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
/*!
 * Virtual entry point function.  Will direct to various implemenation-specific
 * functions.
 *
 * @copydoc PerfLimitsConstruct
 */
static FLCN_STATUS
s_perfLimitsConstruct
(
    PERF_LIMITS **ppLimits
)
{
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT_35_10))
    {
        return perfLimitsConstruct_35_10(ppLimits);
    }
    else if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PERF_LIMIT_40))
    {
        return perfLimitsConstruct_40(ppLimits);
    }

    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * PERF_PERF_LIMIT implementation to parse BOARDOBJGRP header.
 *
 * @copydoc BoardObjGrpIfaceModel10SetHeader
 */
static FLCN_STATUS
s_perfLimitIfaceModel10SetHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pHdrDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_SET_HEADER *pLimitsDesc =
        (RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_SET_HEADER *)pHdrDesc;
    PERF_LIMITS   *pLimits = (PERF_LIMITS *)pBObjGrp;
    FLCN_STATUS    status;

    status = boardObjGrpIfaceModel10SetHeader(pModel10, pHdrDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimitIfaceModel10SetHeader_exit;
    }

    // Ensure that the RM version matches the PMU version.
    if (pLimitsDesc->version != pLimits->version)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto s_perfLimitIfaceModel10SetHeader_exit;
    }

    // Copy in PERF_LIMITS data.
    pLimits->bArbitrateAndApplyLock = pLimitsDesc->bArbitrateAndApplyLock;
    pLimits->bCachingEnabled        = pLimitsDesc->bCachingEnabled;
    pLimits->bApplyMin              = pLimitsDesc->bApplyMin;
    // Force a sync if enabling the GPU Boost Sync
    pLimits->gpuBoostSync.bForce    = pLimitsDesc->bEnableGpuBoostSync &&
                                      !pLimits->gpuBoostSync.bEnabled;
    pLimits->gpuBoostSync.bEnabled  = pLimitsDesc->bEnableGpuBoostSync;
    status = boardObjGrpMaskImport_E255(&(pLimits->limitMaskGpuBoost),
                                        &(pLimitsDesc->limitMaskGpuBoost));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimitIfaceModel10SetHeader_exit;
    }

s_perfLimitIfaceModel10SetHeader_exit:
    return status;
}

/*!
 * @copydoc BoardObjGrpIfaceModel10SetEntry
 */
static FLCN_STATUS
s_perfLimitIfaceModel10SetEntry
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    RM_PMU_BOARDOBJ    *pBuf
)
{
    switch (pBuf->type)
    {
        case LW2080_CTRL_PERF_PERF_LIMIT_TYPE_35_10:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_35_10);
        {
            return perfLimitGrpIfaceModel10ObjSetImpl_35_10(pModel10, ppBoardObj,
                        sizeof(PERF_LIMIT_35_10), pBuf);
        }
        case LW2080_CTRL_PERF_PERF_LIMIT_TYPE_40:
        PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_40);
        {
            return perfLimitGrpIfaceModel10ObjSetImpl_40(pModel10, ppBoardObj,
                        sizeof(PERF_LIMIT_40), pBuf);
        }
    }

    PMU_BREAKPOINT();
    return FLCN_ERR_NOT_SUPPORTED;
}

/*!
 * PERF_LIMIT implementation
 *
 * @copydoc BoardObjGrpIfaceModel10GetStatusHeader
 */
static FLCN_STATUS
s_perfLimitIfaceModel10GetStatusHeader
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    RM_PMU_BOARDOBJGRP *pBuf,
    BOARDOBJGRPMASK    *pMask
)
{
    BOARDOBJGRP *pBoardObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_GET_STATUS_HEADER *pLimitsStatus =
        (RM_PMU_PERF_PERF_LIMIT_BOARDOBJGRP_GET_STATUS_HEADER *)pBuf;
    PERF_LIMITS   *pLimits = (PERF_LIMITS *)pBoardObjGrp;
    FLCN_STATUS    status  = FLCN_OK;
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfLimitChangeSeqChange)
    };

    // No super-class to call.

    //
    // Copy out class-specific data.
    // Need to load in the overlay containing the last change sequencer input.
    //
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        pLimitsStatus->arbSeqId = pLimits->arbSeqId;
        status = boardObjGrpMaskExport_E255(&(pLimits->limitMaskActive),
            &(pLimitsStatus->limitMaskActive));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto s_perfLimitIfaceModel10GetStatusHeader_exit;
        }

        PERF_LIMITS_ARBITRATION_OUTPUT_DEFAULT_EXPORT(&pLimits->arbOutputDefault,
            &pLimitsStatus->arbOutputDefault);
        pLimitsStatus->gpuBoostSync = pLimits->gpuBoostSync;
        memcpy(&pLimitsStatus->changeSeqChange, pLimits->pChangeSeqChange,
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_INPUT));
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    // Call into PERF_LIMITS version-specific header parsing.
    switch (pLimits->version)
    {
        case LW2080_CTRL_PERF_PERF_LIMITS_VERSION_35_10:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_35_10);
            status = perfLimitsIfaceModel10GetStatusHeader_35(pModel10, pBuf, pMask);
            break;
        }
        case LW2080_CTRL_PERF_PERF_LIMITS_VERSION_40:
        {
            PMUCFG_FEATURE_ENABLED_OR_BREAK(PMU_PERF_PERF_LIMIT_40);
            status = perfLimitsIfaceModel10GetStatusHeader_35(pModel10, pBuf, pMask);
            break;
        }
    }
    // Catch any version-specific errors.
    if (LW_OK != status)
    {
        goto s_perfLimitIfaceModel10GetStatusHeader_exit;
    }

s_perfLimitIfaceModel10GetStatusHeader_exit:
    return status;
}

/*!
 * Processes the construct command sent down with the BOARDOBJGRP_SET.
 *
 * @param[in]  pLimits        PERF_LIMITS pointer.
 * @param[in]  pConstructCmd  Command and parameters to processs.
 *
 * @return FLCN_OK if the construct command was successfully processed;
 *         otherwise, a detailed error code is returned.
 */
FLCN_STATUS
s_perfLimitsProcessConstructCommand
(
    PERF_LIMITS                           *pLimits,
    RM_PMU_PERF_PERF_LIMITS_CONSTRUCT_CMD *pConstructCmd
)
{
    FLCN_STATUS status = FLCN_OK;

    switch (pConstructCmd->cmd)
    {
        case LW_RM_PMU_PERF_PERF_LIMITS_CONSTRUCT_CMD_NONE:
        {
            break;
        }
        case LW_RM_PMU_PERF_PERF_LIMITS_CONSTRUCT_CMD_ARBITRATE:
        {
            PERF_LIMITS_ARBITRATION_OUTPUT *pArbOutputScratch =
                pLimits->pArbOutputScratch;
            BOARDOBJGRPMASK_E255            limitMaskExclude;

            // Import the ARBITRATION_OUTPUT RM -> PMU structure types.
            PERF_LIMITS_ARBITRATION_OUTPUT_IMPORT(pArbOutputScratch,
                &pConstructCmd->arbitrate.arbOutput);
            // Import the previous exclude mask RM -> PMU structure types.
            boardObjGrpMaskInit_E255(&limitMaskExclude);
            status = boardObjGrpMaskImport_E255(&limitMaskExclude,
                        &pConstructCmd->arbitrate.limitMaskExclude);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_perfLimitsProcessConstructCommand_exit;
            }

            // Call into perfLimitsArbitrate() to run actual arbitration algorithm.
            status = perfLimitsArbitrate(pLimits, pArbOutputScratch,
                        &limitMaskExclude, pConstructCmd->arbitrate.bSetValuesMin);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto s_perfLimitsProcessConstructCommand_exit;
            }

            // Export the ARBITRATION_OUTPUT RM -> PMU structure types.
            PERF_LIMITS_ARBITRATION_OUTPUT_EXPORT(pArbOutputScratch,
                &pConstructCmd->arbitrate.arbOutput);
            break;
        }
        case LW_RM_PMU_PERF_PERF_LIMITS_CONSTRUCT_CMD_ARBITRATE_AND_APPLY:
        {
            // Since the command is initiated by the RM, treat it as such.
            PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE changeRequest;
            PERF_CHANGE_SEQ_SYNC_CHANGE_CLIENT_QUEUE_INIT(&changeRequest, RM);

            status = perfLimitsArbitrateAndApply(pLimits,
                        pConstructCmd->arbitrateAndApply.flags,
                        &changeRequest);

            pConstructCmd->arbitrateAndApply.changeSeqId = changeRequest.seqId;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            status = FLCN_ERROR;
            break;
        }
    }

s_perfLimitsProcessConstructCommand_exit:
    return status;
}

/*!
 * Sends the updated GPU Boost Sync limits to the RM.
 *
 * @param[in]  pLimits      PERF_LIMITS pointer
 *
 * @return FLCN_OK
 *      The GPU Boost Sync limits have been arbitrated and sent to the RM,
 *      if needed.
 * @return Other errors
 *      Unexpected error encountered.
 */
static FLCN_STATUS
s_perfLimitsArbitrateGpuBoostSyncLimits
(
    PERF_LIMITS *pLimits
)
{
    BOARDOBJGRPMASK_E255 limitMaskExclude;
    LwU32       clkDomIdx;
    FLCN_STATUS status;
    PERF_LIMITS_ARBITRATION_OUTPUT *pArbOutput =
        pLimits->pArbOutputScratch;
    LW2080_CTRL_PERF_PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN *pTupleClkDomain;

    //
    // Only want to arbitrate over the GPU Boost limits. Ilwert to get the
    // exclude mask
    //
    boardObjGrpMaskInit_E255(&limitMaskExclude);
    status = boardObjGrpMaskCopy(&limitMaskExclude, &pLimits->limitMaskGpuBoost);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimitsArbitrateGpuBoostSyncLimits_exit;
    }

    status = boardObjGrpMaskIlw(&limitMaskExclude);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimitsArbitrateGpuBoostSyncLimits_exit;
    }

    // Get arbitrated values for GPU Boost Sync limits
    status = perfLimitsArbitrate(pLimits, pArbOutput, &limitMaskExclude,
        LW_FALSE);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimitsArbitrateGpuBoostSyncLimits_exit;
    }

    //
    // If the arbitrated values have been modified, we need to send the new
    // values to the RM.
    //
    status = clkDomainsGetIndexByApiDomain(CLK_DOMAIN_MASK_GPC, &clkDomIdx);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto s_perfLimitsArbitrateGpuBoostSyncLimits_exit;
    }

    PMU_ASSERT_OK_OR_GOTO(status,
        PERF_LIMITS_ARBITRATION_OUTPUT_TUPLE_CLK_DOMAIN_GET(&pArbOutput->tuple,
            clkDomIdx, &pTupleClkDomain),
        s_perfLimitsArbitrateGpuBoostSyncLimits_exit);

    if ((pArbOutput->tuple.pstateIdx.value !=
            pLimits->gpuBoostSync.pstateIdx.value) ||
        (pTupleClkDomain->freqkHz.value !=
            pLimits->gpuBoostSync.gpcclkkHz.freqkHz.value))
    {
        PMU_RM_RPC_STRUCT_PERF_PERF_LIMITS_GPU_BOOST_SYNC_CHANGE rpc;
        memset(&rpc, 0x00, sizeof(rpc));

        rpc.pstateIdx = pArbOutput->tuple.pstateIdx.value;
        rpc.gpcclkkHz = pTupleClkDomain->freqkHz.value;

        // Send the Event as an RPC.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, PERF,
            PERF_LIMITS_GPU_BOOST_SYNC_CHANGE, &rpc);

        // Update values now they have been sent to RM
        pLimits->gpuBoostSync.pstateIdx = pArbOutput->tuple.pstateIdx;
        pLimits->gpuBoostSync.gpcclkkHz = *pTupleClkDomain;
    }

s_perfLimitsArbitrateGpuBoostSyncLimits_exit:
    return status;
}
