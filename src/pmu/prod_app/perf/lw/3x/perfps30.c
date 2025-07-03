/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   perfps30.c
 * @brief  Pstate 3.0 Interfaces
 */

/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "dbgprintf.h"
#include "g_pmurpc.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */

/*!
 * Processes RM_PMU_PERF_PSTATE_STATUS objects received from the RM as 
 * pstates/clock domains values are updated.
 *
 * @param[in]  pStatus  RM_PMU_PERF_PSTATE_STATUS object to process
 *
 * @return 'FLCN_OK'     Upon successful processing of the object
 * @return 'FLCN_ERROR'  Reserved for future error-checking
 */
FLCN_STATUS
perfProcessPerfStatus_3X
(
    RM_PMU_PERF_PSTATE_STATUS *pStatus
)
{
    FLCN_STATUS status = FLCN_OK;
    PERF_PSTATE_STATUS *pPerfStatus = (&Perf.status);
    BOARDOBJGRP        *pDomains    =
        (BOARDOBJGRP *)BOARDOBJGRP_GRP_GET(CLK_DOMAIN);
    LwU8                i;

    if (pDomains == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfProcessPerfStatus_3X_exit;
    }

    // Sanity check that buffers have been allocated.
    if (pPerfStatus->pClkDomains == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        goto perfProcessPerfStatus_3X_exit;
    }

#if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ELW_PSTATES_2X))
    if (PMUCFG_FEATURE_ENABLED(PMU_SELWRITY_HARDENING))
    {
        if (pStatus->pstateLwrrIdx >= Perf.tables.numPstates)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            goto perfProcessPerfStatus_3X_exit;
        }
    }
#endif

    pPerfStatus->pstateLwrrIdx = pStatus->pstateLwrrIdx;

    // Copy in the latest clock domain freq value.
    for (i = 0; i < pDomains->objSlots; i++)
    {
        pPerfStatus->pClkDomains[i].clkFreqKHz      =
            pStatus->clkDomains[i].clkFreqKHz;
        pPerfStatus->pClkDomains[i].clkFlags        =
            pStatus->clkDomains[i].clkFlags;
        //
        // lwrrentRegimeId is not being validated since it is not used
        // Programming uses the regimeId, but reading does not
        // care about regimeId, reading only cares about frequency
        //
        pPerfStatus->pClkDomains[i].lwrrentRegimeId =
            pStatus->clkDomains[i].lwrrentRegimeId;
    }

perfProcessPerfStatus_3X_exit:
    return status;
}

/*!
 * Handler for CLK_DOMAINS GRP_SET to do any necessary post-processing for
 * PSTATE 3.0 state.
 *
 * @param[in]  pDomains
 *     BOARDOBJGRP pointer to the CLK_DOMAINS group's super class.
 *
 * @return FLCN_OK
 *     All post-processing completed.
 * @return FLCN_ERR_NO_FREE_MEM
 *     Could not allocate the CLK_DOMAIN frequency array, as out of DMEM.
 */
FLCN_STATUS
perfClkDomainsBoardObjGrpSet_30
(
    BOARDOBJGRP *pDomains
)
{
    PERF_PSTATE_STATUS *pPerfStatus = (&Perf.status);

    // Allocate the memory for pstate status if not previously allocated.
    if ((pPerfStatus->pClkDomains == NULL) &&
        (pDomains->objSlots != 0))
    {
        pPerfStatus->pClkDomains =
            lwosCallocType(OVL_INDEX_DMEM(perf), pDomains->objSlots,
                           RM_PMU_PERF_PSTATE_CLK_DOMAIN_STATUS);
        if (pPerfStatus->pClkDomains == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NO_FREE_MEM;
        }
    }

    return FLCN_OK;
}

/*!
 * copydoc@ PerfDomGrpGetValue
 */
LwU32
perfDomGrpGetValue
(
    LwU8 domGrpIdx
)
{
    BOARDOBJGRP    *pDomains;
    LwU8            clkDomainIdx;

    if (domGrpIdx == RM_PMU_PERF_DOMAIN_GROUP_PSTATE)
    {
        return Perf.status.pstateLwrrIdx;
    }

    pDomains = (BOARDOBJGRP *)BOARDOBJGRP_GRP_GET(CLK_DOMAIN);
    if (pDomains == NULL)
    {
        PMU_BREAKPOINT();
        return LW_U8_MIN;
    }

    // Colwert the domain grout index to clock domain index
    clkDomainIdx = clkDomainsGetIdxByDomainGrpIdx(domGrpIdx);

    if (clkDomainIdx < pDomains->objSlots)
    {
        return Perf.status.pClkDomains[clkDomainIdx].clkFreqKHz;
    }

    PMU_HALT();
    return LW_U8_MIN;
}

/*!
 * @brief   Exelwtes generic PERF_PSTATE_STATUS_UPDATE RPC.
 *
 * @param[in/out]   pParams Pointer to RM_PMU_RPC_STRUCT_PERF_PERF_PSTATE_STATUS_UPDATE
 */
FLCN_STATUS
pmuRpcPerfPerfPstateStatusUpdate
(
    RM_PMU_RPC_STRUCT_PERF_PERF_PSTATE_STATUS_UPDATE *pParams
)
{
    FLCN_STATUS status;

    //
    // There can be a race condition since PMGR code will access Perf.status
    // in CWC (PMGR Task). Sync accessing to Perf.status with a semaphore.
    //
    perfWriteSemaphoreTake();
    {
        status = perfProcessPerfStatus_3X(&(pParams->perfPstateStatus));
    }
    perfWriteSemaphoreGive();

    return status;
}

/* ------------------------- Private Functions ------------------------------ */

