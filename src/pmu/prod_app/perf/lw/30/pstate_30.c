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
 * @file    pstate_30.c
 * @brief   Function definitions for the PSTATE_30 SW module.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "perf/30/pstate_30.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc     BoardObjGrpIfaceModel10ObjSet()
 *
 * @memberof    PSTATE_30
 *
 * @protected
 */
FLCN_STATUS
perfPstateGrpIfaceModel10ObjSet_30
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_PSTATE_30  *pSet;
    PSTATE_30              *pPstate30;
    FLCN_STATUS             status;

    status = perfPstateIfaceModel10Set_3X(pModel10, ppBoardObj, size, pDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPstateGrpIfaceModel10ObjSet_30_exit;
    }
    pPstate30 = (PSTATE_30 *)*ppBoardObj;
    pSet      = (RM_PMU_PERF_PSTATE_30 *)pDesc;

    // Allocate the Domain Group entries array if not previously allocated.
    if (pPstate30->pClkEntries == NULL)
    {
        pPstate30->pClkEntries =
            lwosCallocType(pBObjGrp->ovlIdxDmem, Perf.pstates.numClkDomains,
                           LW2080_CTRL_PERF_PSTATE_30_CLOCK_ENTRY);
        if (pPstate30->pClkEntries == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto perfPstateGrpIfaceModel10ObjSet_30_exit;
        }
    }

    // Copy-in the PSTTAE frequency tuple.
    memcpy(pPstate30->pClkEntries, pSet->clkEntries,
        sizeof(LW2080_CTRL_PERF_PSTATE_30_CLOCK_ENTRY) *
                            Perf.pstates.numClkDomains);

perfPstateGrpIfaceModel10ObjSet_30_exit:
    return status;
}

/*!
 * @copydoc     PerfPstateClkFreqGet()
 *
 * @memberof    PSTATE_30
 *
 * @protected
 */
FLCN_STATUS
perfPstateClkFreqGet_30
(
    PSTATE                                 *pPstate,
    LwU8                                    clkDomainIdx,
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY    *pPstateClkEntry
)
{
    PSTATE_30                              *pPstate30   = (PSTATE_30 *)pPstate;
    LW2080_CTRL_PERF_PSTATE_30_CLOCK_ENTRY *pClkEntry   = &pPstate30->pClkEntries[clkDomainIdx];
    CLK_DOMAIN                             *pDomain     =
        BOARDOBJGRP_OBJ_GET(CLK_DOMAIN, clkDomainIdx);
    CLK_DOMAIN_PROG                        *pDomainProg;
    FLCN_STATUS                             status      = FLCN_OK;

    if (pDomain == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfPstateClkFreqGet_30_exit;
    }

    if ((pDomain->domain == clkWhich_PCIEGenClk) ||
        ((!PSTATE_OCOV_ENABLED(pPstate)) &&
         (!clkDomainsOverrideOVOCEnabled())))
    {
        pPstateClkEntry->nom.freqkHz = pClkEntry->targetFreqKHz;
        pPstateClkEntry->min.freqkHz = pClkEntry->freqRangeMinKHz;
        pPstateClkEntry->max.freqkHz = pClkEntry->freqRangeMaxKHz;
    }
    else
    {
        LwU16   targetFreqMHz   = (LwU16)(pClkEntry->targetFreqKHz   / 1000);
        LwU16   freqRangeMinMHz = (LwU16)(pClkEntry->freqRangeMinKHz / 1000);
        LwU16   freqRangeMaxMHz = (LwU16)(pClkEntry->freqRangeMaxKHz / 1000);

        if (LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED !=
                BOARDOBJ_GET_TYPE(pDomain))
        {
            pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
            if (pDomainProg == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto perfPstateClkFreqGet_30_exit;
            }

            // Adjust PSTATE::min frequency tuple
            status = clkDomainProgClientFreqDeltaAdj(
                        pDomainProg,
                        &targetFreqMHz);    // pFreqMHz
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfPstateClkFreqGet_30_exit;
            }

            // Adjust PSTATE::nom frequency tuple
            status = clkDomainProgClientFreqDeltaAdj(
                        pDomainProg,
                        &freqRangeMinMHz);  // pFreqMHz
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfPstateClkFreqGet_30_exit;
            }

            status = clkDomainProgClientFreqDeltaAdj(
                        pDomainProg,
                        &freqRangeMaxMHz);  // pFreqMHz
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfPstateClkFreqGet_30_exit;
            }
        }

        pPstateClkEntry->nom.freqkHz = (LwU32)targetFreqMHz   * 1000;
        pPstateClkEntry->min.freqkHz = (LwU32)freqRangeMinMHz * 1000;
        pPstateClkEntry->max.freqkHz = (LwU32)freqRangeMaxMHz * 1000;
    }
    // Trim and cache all VF Max frequency.
    if (LW2080_CTRL_CLK_CLK_DOMAIN_TYPE_3X_FIXED !=
            BOARDOBJ_GET_TYPE(pDomain))
    {
        LwU32 freqMaxkHz;

        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfPstateClkFreqGet_30_exit;
        }

        status = clkDomainProgVfMaxFreqMHzGet(
                    pDomainProg, &freqMaxkHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfPstateClkFreqGet_30_exit;
        }
        // Scale the value from MHz -> kHz.
        freqMaxkHz *= clkDomainFreqkHzScaleGet(pDomain);

        pPstateClkEntry->nom.freqVFMaxkHz = LW_MIN(freqMaxkHz, pPstateClkEntry->nom.freqkHz);
        pPstateClkEntry->min.freqVFMaxkHz = LW_MIN(freqMaxkHz, pPstateClkEntry->min.freqkHz);
        pPstateClkEntry->max.freqVFMaxkHz = LW_MIN(freqMaxkHz, pPstateClkEntry->max.freqkHz);
    }

perfPstateClkFreqGet_30_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- End of File ------------------------------------ */
