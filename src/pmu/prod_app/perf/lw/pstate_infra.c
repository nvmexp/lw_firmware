/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pstate_infra.c
 * @brief  Function definitions for the PSTATES SW module's core infrastructure.
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "dbgprintf.h"
#include "cmdmgmt/cmdmgmt.h"
#include "g_pmurmifrpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc     PerfGetPstateCount()
 *
 * @memberof    PSTATES
 *
 * @public
 */
LwU8
perfGetPstateCount(void)
{
    return (LwU8)lwPopCount32(Perf.pstates.super.objMask.super.pData[0]);
}

/*!
 * @copydoc     PerfPstatePackPstateCeilingToDomGrpLimits()
 *
 * @memberof    PSTATES
 *
 * @public
 */
FLCN_STATUS
perfPstatePackPstateCeilingToDomGrpLimits
(
    PERF_DOMAIN_GROUP_LIMITS *pDomGrpLimits
)
{
    PSTATE         *pPstate;
    LwU8            pstateLevel;
    FLCN_STATUS     status = FLCN_ERR_NOT_SUPPORTED;

    // Get the MAX PSTATE index
    pstateLevel = (perfGetPstateCount() - 1U);

    // Get the MAX PSTATE Pointer
    pPstate = PSTATE_GET_BY_LEVEL(pstateLevel);

    // Check that pPstate is valid
    if (pPstate == NULL)
    {
        status = FLCN_ERR_OBJECT_NOT_FOUND;
        goto perfPstatePackPstateCeilingToDomGrpLimits_exit;
    }

    // Set level in DomGrp
    pDomGrpLimits->values[RM_PMU_DOMAIN_GROUP_PSTATE] = pstateLevel;

    // Call into type-specific implementations
    switch (BOARDOBJ_GET_TYPE(pPstate))
    {
        case LW2080_CTRL_PERF_PSTATE_TYPE_30:
        case LW2080_CTRL_PERF_PSTATE_TYPE_35:
        {
            LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY pstateClkEntry = {{0,},};

            status = perfPstateClkFreqGet(pPstate,
                        clkDomainsGetIdxByDomainGrpIdx(RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK),
                        &pstateClkEntry);
            if (status != FLCN_OK)
            {
                pDomGrpLimits->values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK] = 0;
            }
            else
            {
                pDomGrpLimits->values[RM_PMU_PERF_DOMAIN_GROUP_GPC2CLK] =
                    pstateClkEntry.max.freqVFMaxkHz;
            }
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_DOMAIN_GROUP_LIMITS_XBAR))
            {
                LwU32 clkIdx;

                PMU_HALT_OK_OR_GOTO(status,
                    clkDomainsGetIndexByApiDomain(LW2080_CTRL_CLK_DOMAIN_XBARCLK,
                                                  &clkIdx),
                    perfPstatePackPstateCeilingToDomGrpLimits_exit);

                status = perfPstateClkFreqGet(pPstate, clkIdx,
                    &pstateClkEntry);
                if (status != FLCN_OK)
                {
                    pDomGrpLimits->values[RM_PMU_PERF_DOMAIN_GROUP_XBARCLK] = 0;
                }
                else
                {
                    pDomGrpLimits->values[RM_PMU_PERF_DOMAIN_GROUP_XBARCLK] =
                        pstateClkEntry.max.freqVFMaxkHz;
                }
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

perfPstatePackPstateCeilingToDomGrpLimits_exit:
    return status;
}

/*!
 * @copydoc     PerfPstatesIlwalidate()
 *
 * @memberof    PSTATES
 *
 * @public
 */
FLCN_STATUS
perfPstatesIlwalidate
(
    LwBool  bTriggerMsg
)
{
    PMU_RM_RPC_STRUCT_PERF_PSTATES_ILWALIDATE rpc;
    FLCN_STATUS                               status  = FLCN_OK;

    if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_35))
    {
        // Caching not supported on PSTATE_30
        goto perfPstatesIlwalidate_exit;
    }

    // KO-TODO: Bail out if not loaded from the RM

    // Skip further processing if pstates has not been inititalized
    if (BOARDOBJGRP_IS_EMPTY(PSTATE))
    {
        goto perfPstatesIlwalidate_exit;
    }

    // Ilwalidate Vpstates since it contains Pstate indexes
    status = vpstatesIlwalidate();
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPstatesIlwalidate_exit;
    }

    // Notify RM to update its pstate cache.
    if (bTriggerMsg)
    {
        // RC-TODO, properly handle status here.
        PMU_RM_RPC_EXELWTE_BLOCKING(status, PERF, PSTATES_ILWALIDATE, &rpc);
    }

perfPstatesIlwalidate_exit:
    return status;
}

/*!
 * @copydoc     PerfPstatesCache()
 *
 * @memberof    PSTATES
 *
 * @public
 */
FLCN_STATUS
perfPstatesCache(void)
{
    PSTATE       *pPstate;
    LwBoardObjIdx idx;
    FLCN_STATUS   status  = FLCN_OK;

    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_BEGIN(PERF, PSTATE, BASE, pPstate,
                                            idx, NULL, &status,
                                            perfPstatesCache_exit)
    {
        status = perfPstateCache(pPstate);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfPstatesCache_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_END;

perfPstatesCache_exit:
    return status;
}

/*!
 * @copydoc     PerfPstateCache()
 *
 * @memberof    PSTATE
 *
 * @public
 */
FLCN_STATUS
perfPstateCache
(
    PSTATE *pPstate
)
{
    FLCN_STATUS status = FLCN_ERR_NOT_SUPPORTED;

    // Call into type-specific implementations
    switch (BOARDOBJ_GET_TYPE(pPstate))
    {
        case LW2080_CTRL_PERF_PSTATE_TYPE_35:
        {
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35))
            {
                status = perfPstateCache_35(pPstate);
            }
            break;
        }
        default:
        {
            status = FLCN_ERR_NOT_SUPPORTED;
            break;
        }
    }

    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
    }

    return status;
}

/*!
 * @brief       Helper interface to get PSTATE::level from PSTATE::index.
 *
 * Perf level of the PSTATE associated with this object. This parameter is
 * the index in the dense legacy PSTATE packing, which is the VBIOS indexing
 * but compressed so that there are no gaps.
 *
 * @param[in]   pstateIdx   index of PSTATE object that caller would like level for.
 *
 * @return      PSTATE perf level corresponding to the input PSTATE index.
 *
 * @memberof    PSTATES
 *
 * @public
 */
LwU8
perfPstateGetLevelByIndex
(
    LwU8 pstateIdx
)
{
    PSTATE       *pPstate;
    LwBoardObjIdx idx;
    FLCN_STATUS   status;
    LwU8          pstateLevel = 0;

    // Validate P-state index
    if (!PSTATE_INDEX_IS_VALID(pstateIdx))
    {
        pstateLevel = LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;
        goto perfPstateGetLevelByIndex_exit;
    }

    // Get level by index.
    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_BEGIN(PERF, PSTATE, BASE, pPstate,
                                            idx, NULL, &status,
                                            perfPstateGetLevelByIndex_exit)
    {
        if (pstateIdx == BOARDOBJ_GRP_IDX_TO_8BIT(idx))
        {
            goto perfPstateGetLevelByIndex_exit;
        }

        ++pstateLevel;
    }
    BOARDOBJGRP_ITERATOR_DYNAMIC_CAST_END;

perfPstateGetLevelByIndex_exit:
    return pstateLevel;
}

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- End of File ------------------------------------ */
