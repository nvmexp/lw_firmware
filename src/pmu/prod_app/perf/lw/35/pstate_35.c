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
 * @file    pstate_35.c
 * @brief   Function definitions for the PSTATE_35 SW module.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"
#include "perf/35/pstate_35.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static BoardObjVirtualTableDynamicCast (s_perfPstateDynamicCast_35)
    GCC_ATTRIB_SECTION("imem_perf", "s_perfPstateDynamicCast_35")
    GCC_ATTRIB_USED();

static FLCN_STATUS s_perfPstateConstructVoltEntries(PSTATE_35 *pPstate35, LwU8 ovlIdxDmem);

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/*!
 * Virtual table for PSTATE_35 object interfaces.
 */
BOARDOBJ_VIRTUAL_TABLE PerfPstate35VirtualTable =
{
    BOARDOBJ_VIRTUAL_TABLE_ASSIGN_DYNAMIC_CAST(s_perfPstateDynamicCast_35)
};

/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @brief       PSTATE_35 specific constructor
 *
 *  - When called for the first time:
 *      -# Copy the set orig, base, and POR tuples.
 *      -# Sanity check that the orig tuple has been quantized correctly.
 *      -# Apply factory OC to the POR tuple if applicable.
 *
 *  - When called after initial construction (setControl):
 *      -# Copy the set base tuple to the Pstate's base tuple.
 *
 * @copydetails BoardObjGrpIfaceModel10ObjSet()
 *
 * @return  FLCN_ERR_NO_FREE_MEM    if DMEM heap allocation fails.
 * @return  OTHERS                  errors propagated from dependent function
 *                                  calls.
 * @memberof    PSTATE_35
 *
 * @protected
 */
FLCN_STATUS
perfPstateGrpIfaceModel10ObjSet_35
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjDesc
)
{
    BOARDOBJGRP *pBObjGrp = boardObjGrpIfaceModel10BoardObjGrpGet(pModel10);
    RM_PMU_PERF_PSTATE_35  *pSet;
    PSTATE_35              *pPstate35;
    LwU8                    clkIdx;
    FLCN_STATUS             status;

    status = perfPstateIfaceModel10Set_3X(pModel10, ppBoardObj, size, pBoardObjDesc);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfPstateGrpIfaceModel10ObjSet_35_exit;
    }

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPstate35, *ppBoardObj, PERF, PSTATE, 35,
                                  &status, perfPstateGrpIfaceModel10ObjSet_35_exit);
    pSet = (RM_PMU_PERF_PSTATE_35 *)pBoardObjDesc;

    // Allocate the clk entries array if not previously allocated.
    if (pPstate35->pClkEntries == NULL)
    {
        pPstate35->pClkEntries =
            lwosCallocType(pBObjGrp->ovlIdxDmem, (LwU32)Perf.pstates.numClkDomains,
                           PERF_PSTATE_35_CLOCK_ENTRY);
        if (pPstate35->pClkEntries == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_NO_FREE_MEM;
            goto perfPstateGrpIfaceModel10ObjSet_35_exit;
        }

        // Copy-in PSTATE_35 params
        for (clkIdx = 0; clkIdx < Perf.pstates.numClkDomains; ++clkIdx)
        {
            // Copy orig --> orig
            pPstate35->pClkEntries[clkIdx].min.origFreqkHz = pSet->clkEntries[clkIdx].super.min.origFreqkHz;
            pPstate35->pClkEntries[clkIdx].max.origFreqkHz = pSet->clkEntries[clkIdx].super.max.origFreqkHz;
            pPstate35->pClkEntries[clkIdx].nom.origFreqkHz = pSet->clkEntries[clkIdx].super.nom.origFreqkHz;

            // Copy por --> por
            pPstate35->pClkEntries[clkIdx].min.porFreqkHz  = pSet->clkEntries[clkIdx].super.min.porFreqkHz;
            pPstate35->pClkEntries[clkIdx].max.porFreqkHz  = pSet->clkEntries[clkIdx].super.max.porFreqkHz;
            pPstate35->pClkEntries[clkIdx].nom.porFreqkHz  = pSet->clkEntries[clkIdx].super.nom.porFreqkHz;

            // Copy base --> base
            pPstate35->pClkEntries[clkIdx].min.baseFreqkHz = pSet->clkEntries[clkIdx].super.min.baseFreqkHz;
            pPstate35->pClkEntries[clkIdx].max.baseFreqkHz = pSet->clkEntries[clkIdx].super.max.baseFreqkHz;
            pPstate35->pClkEntries[clkIdx].nom.baseFreqkHz = pSet->clkEntries[clkIdx].super.nom.baseFreqkHz;
        }

        pPstate35->pcieIdx   = pSet->pcieIdx;
        pPstate35->lwlinkIdx = pSet->lwlinkIdx;

        // Sanity check orig
        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_INFRA))
        {
            status = perfPstate35SanityCheckOrig(pPstate35);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfPstateGrpIfaceModel10ObjSet_35_exit;
            }

            // Adjust por tuple to reflect any applied factory OC
            status = perfPstate35AdjFreqTupleByFactoryOCOffset(pPstate35);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfPstateGrpIfaceModel10ObjSet_35_exit;
            }
        }
    }
    else
    {
        // Copy only the base tuple on conselwtive constructs (setControl)
        for (clkIdx = 0; clkIdx < Perf.pstates.numClkDomains; ++clkIdx)
        {
            pPstate35->pClkEntries[clkIdx].min.baseFreqkHz = pSet->clkEntries[clkIdx].super.min.baseFreqkHz;
            pPstate35->pClkEntries[clkIdx].max.baseFreqkHz = pSet->clkEntries[clkIdx].super.max.baseFreqkHz;
            pPstate35->pClkEntries[clkIdx].nom.baseFreqkHz = pSet->clkEntries[clkIdx].super.nom.baseFreqkHz;
        }
    }

    // Init freq --> base
    for (clkIdx = 0; clkIdx < Perf.pstates.numClkDomains; ++clkIdx)
    {
        pPstate35->pClkEntries[clkIdx].min.freqkHz = pSet->clkEntries[clkIdx].super.min.baseFreqkHz;
        pPstate35->pClkEntries[clkIdx].max.freqkHz = pSet->clkEntries[clkIdx].super.max.baseFreqkHz;
        pPstate35->pClkEntries[clkIdx].nom.freqkHz = pSet->clkEntries[clkIdx].super.nom.baseFreqkHz;
    }

    status = s_perfPstateConstructVoltEntries(pPstate35, pBObjGrp->ovlIdxDmem);

perfPstateGrpIfaceModel10ObjSet_35_exit:
    return status;
}

/*!
 * @brief       PSTATE_35 implementation of @ref BoardObjIfaceModel10GetStatus
 *
 * @copydoc     BoardObjIfaceModel10GetStatus()
 *
 * @memberof    PSTATE_35
 *
 * @protected
 */
FLCN_STATUS
pstateIfaceModel10GetStatus_35
(
    BOARDOBJ_IFACE_MODEL_10           *pModel10,
    RM_PMU_BOARDOBJ    *pPayload
)
{
    BOARDOBJ *pBoardObj = boardObjIfaceModel10BoardObjGet(pModel10);
    PSTATE_35  *pPstate35;
    RM_PMU_PERF_PSTATE_35_GET_STATUS *pGetStatus =
        (RM_PMU_PERF_PSTATE_35_GET_STATUS *)pPayload;
    LwU8        clkIdx;
    LwBoardObjIdx idx;
    VOLT_RAIL    *pRail;
    FLCN_STATUS status    = FLCN_OK;
    LwU32*      pFreqMaxAtVminkHz = NULL;
    LW2080_CTRL_PERF_PSTATE_VOLT_RAIL *pVoltEntry;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPstate35, pBoardObj, PERF, PSTATE, 35,
                                  &status, pstateIfaceModel10GetStatus_35_exit);

    status = pstateIfaceModel10GetStatus_SUPER(pModel10, pPayload);
    if (status != FLCN_OK)
    {
        goto pstateIfaceModel10GetStatus_35_exit;
    }

    // set everything to 0
    (void)memset(pGetStatus->clkEntries, 0,
        sizeof(LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY) *
        LW2080_CTRL_PERF_PSTATE_MAX_CLK_DOMAINS);

    // only send the current values as basefreq
    for (clkIdx = 0; clkIdx < Perf.pstates.numClkDomains; ++clkIdx)
    {
        pGetStatus->clkEntries[clkIdx].super.min = pPstate35->pClkEntries[clkIdx].min;
        pGetStatus->clkEntries[clkIdx].super.max = pPstate35->pClkEntries[clkIdx].max;
        pGetStatus->clkEntries[clkIdx].super.nom = pPstate35->pClkEntries[clkIdx].nom;
        pFreqMaxAtVminkHz = perfPstateClkEntryFmaxAtVminGetRef(&pPstate35->pClkEntries[clkIdx]);
        if (pFreqMaxAtVminkHz != NULL)
        {
            pGetStatus->clkEntries[clkIdx].freqMaxAtVminkHz = *pFreqMaxAtVminkHz;
        }
    }

    BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, idx, NULL)
    {
        pVoltEntry = perfPstateVoltEntriesGetIdxRef(pPstate35, idx);
        if (pVoltEntry != NULL)
        {
            pGetStatus->voltEntries[idx] = *pVoltEntry;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

pstateIfaceModel10GetStatus_35_exit:
    return status;
}

/*!
 * @copydoc     PerfPstateClkFreqGet()
 *
 * @memberof    PSTATE_35
 *
 * @protected
 */
FLCN_STATUS
perfPstateClkFreqGet_35_IMPL
(
    PSTATE                                 *pPstate,
    LwU8                                    clkDomainIdx,
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY    *pPstateClkEntry
)
{
    PSTATE_35  *pPstate35;
    FLCN_STATUS status = FLCN_OK;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPstate35, &pPstate->super, PERF, PSTATE, 35,
                                  &status, perfPstateClkFreqGet_35_exit);

    // Validate used parameters
    if ((pPstate == NULL)           ||
        (pPstateClkEntry == NULL)   ||
        (!BOARDOBJGRP_IS_VALID(CLK_DOMAIN, clkDomainIdx)))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfPstateClkFreqGet_35_exit;
    }

    //
    // Copy out the clock entry
    // Intentionally skipping _35 entries since they wont be used internally
    //
    pPstateClkEntry->min = pPstate35->pClkEntries[clkDomainIdx].min;
    pPstateClkEntry->max = pPstate35->pClkEntries[clkDomainIdx].max;
    pPstateClkEntry->nom = pPstate35->pClkEntries[clkDomainIdx].nom;

perfPstateClkFreqGet_35_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief   PSTATE_35 implementation of @ref BoardObjVirtualTableDynamicCast()
 *
 * @copydoc BoardObjVirtualTableDynamicCast()
 */
static void *
s_perfPstateDynamicCast_35
(
    BOARDOBJ   *pBoardObj,
    LwU8        requestedType
)
{
    void       *pObject     = NULL;
    PSTATE_35  *pPstate35   = (PSTATE_35 *)pBoardObj;

    if (BOARDOBJ_GET_TYPE(pBoardObj) !=
        LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, 35))
    {
        PMU_BREAKPOINT();
        goto s_perfPstateDynamicCast_35_exit;
    }

    switch (requestedType)
    {
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, BASE):
        {
            PSTATE *pPstate = &pPstate35->super.super;
            pObject = (void *)pPstate;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, 3X):
        {
            PSTATE_3X *pPstate3X = &pPstate35->super;
            pObject = (void *)pPstate3X;
            break;
        }
        case LW2080_CTRL_BOARDOBJ_TYPE(PERF, PSTATE, 35):
        {
            pObject = (void *)pPstate35;
            break;
        }
        default:
        {
            PMU_BREAKPOINT();
            break;
        }
    }

s_perfPstateDynamicCast_35_exit:
    return pObject;
}

static FLCN_STATUS 
s_perfPstateConstructVoltEntries
(
    PSTATE_35 *pPstate35,
    LwU8       ovlIdxDmem
)
{
#if PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN)
    // Allocate volt entries
    if (pPstate35->pVoltEntries == NULL)
    {
        pPstate35->pVoltEntries =
            lwosCallocType(ovlIdxDmem, (LwU32)Perf.pstates.numVoltDomains,
                           LW2080_CTRL_PERF_PSTATE_VOLT_RAIL);
        if (pPstate35->pVoltEntries == NULL)
        {
            PMU_BREAKPOINT();
            return FLCN_ERR_NO_FREE_MEM;
        }
    }
#endif
    return FLCN_OK;
}

/* ------------------------- End of File ------------------------------------ */
