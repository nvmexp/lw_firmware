/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_prop_top_rel_1x_table.c
 *
 * @brief Module managing all state related to the CLK_PROP_TOP_REL_1X_TABLE structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
clkPropTopRelsTableRelOffAdjTupleFreqMHzSrcSet
(
    LwU8    tableRelIdx,
    LwU16   freqMHz
)
{
    CLK_PROP_TOP_RELS *pClkPropTopRels = CLK_CLK_PROP_TOP_RELS_GET();

    if (pClkPropTopRels == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }

    if (tableRelIdx >= pClkPropTopRels->tableRelTupleCount)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pClkPropTopRels->offAdjTableRelTuple[tableRelIdx].freqMHzSrc  = freqMHz;

    return FLCN_OK;
}

GCC_ATTRIB_ALWAYSINLINE()
static inline FLCN_STATUS
clkPropTopRelsTableRelOffAdjTupleFreqMHzDstSet
(
    LwU8    tableRelIdx,
    LwU16   freqMHz
)
{
    CLK_PROP_TOP_RELS *pClkPropTopRels = CLK_CLK_PROP_TOP_RELS_GET();

    if (pClkPropTopRels == NULL)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_STATE;
    }

    if (tableRelIdx >= pClkPropTopRels->tableRelTupleCount)
    {
        PMU_BREAKPOINT();
        return FLCN_ERR_ILWALID_ARGUMENT;
    }

    pClkPropTopRels->offAdjTableRelTuple[tableRelIdx].freqMHzDst  = freqMHz;

    return FLCN_OK;
}

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_PROP_TOP_REL_1X_TABLE class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkPropTopRelGrpIfaceModel10ObjSet_1X_TABLE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_PROP_TOP_REL_1X_TABLE_BOARDOBJ_SET *pRel1xTableSet =
        (RM_PMU_CLK_CLK_PROP_TOP_REL_1X_TABLE_BOARDOBJ_SET *)pBoardObjSet;
    CLK_PROP_TOP_REL_1X_TABLE *pRel1xTable;
    FLCN_STATUS                status;

    status = clkPropTopRelGrpIfaceModel10ObjSet_1X(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        // Super class triggers PMU Break point on error.
        goto clkPropTopRelGrpIfaceModel10ObjSet_1X_TABLE_exit;
    }
    pRel1xTable = (CLK_PROP_TOP_REL_1X_TABLE *)*ppBoardObj;

    // Copy the CLK_PROP_TOP_REL_1X_TABLE-specific data.
    pRel1xTable->tableRelIdxFirst = pRel1xTableSet->tableRelIdxFirst;
    pRel1xTable->tableRelIdxLast  = pRel1xTableSet->tableRelIdxLast;

clkPropTopRelGrpIfaceModel10ObjSet_1X_TABLE_exit:
    return status;
}

/*!
 * @copydoc ClkPropTopRelFreqPropagate
 */
FLCN_STATUS
clkPropTopRelFreqPropagate_1X_TABLE
(
    CLK_PROP_TOP_REL   *pPropTopRel,
    LwBool              bSrcToDstProp,
    LwU16              *pFreqMHz
)
{
    CLK_PROP_TOP_REL_1X_TABLE  *pRel1xTable = (CLK_PROP_TOP_REL_1X_TABLE *)pPropTopRel;
    FLCN_STATUS                 status      = FLCN_OK;
    LwU8                        tableRelIdx;
    LwBool                      bMatchFound = LW_FALSE;

    // Find the matching table entry.
    for (tableRelIdx  = pRel1xTable->tableRelIdxFirst;
         tableRelIdx <= pRel1xTable->tableRelIdxLast;
         tableRelIdx++)
    {
        if ((bSrcToDstProp) &&
            ((*pFreqMHz) <= clkPropTopRelsTableRelOffAdjTupleFreqMHzSrcGet(tableRelIdx)))
        {
            (*pFreqMHz) = clkPropTopRelsTableRelOffAdjTupleFreqMHzDstGet(tableRelIdx);
            bMatchFound = LW_TRUE;
        }
        else if ((!bSrcToDstProp) &&
            ((*pFreqMHz) <= clkPropTopRelsTableRelOffAdjTupleFreqMHzDstGet(tableRelIdx)))
        {
            (*pFreqMHz) = clkPropTopRelsTableRelOffAdjTupleFreqMHzSrcGet(tableRelIdx);
            bMatchFound = LW_TRUE;
        }
    }

    // Set to zero if no match found.
    if (!bMatchFound)
    {
        (*pFreqMHz) = LW_U16_MIN;
        status      = FLCN_ERR_OUT_OF_RANGE;
        PMU_BREAKPOINT();
        goto clkPropTopRelFreqPropagate_1X_TABLE_exit;
    }

clkPropTopRelFreqPropagate_1X_TABLE_exit:
    return status;
}

/*!
 * @copydoc ClkPropTopRelCache
 */
FLCN_STATUS
clkPropTopRelCache_1X_TABLE
(
    CLK_PROP_TOP_REL   *pPropTopRel
)
{
    CLK_PROP_TOP_REL_1X_TABLE  *pRel1xTable = (CLK_PROP_TOP_REL_1X_TABLE *)pPropTopRel;
    FLCN_STATUS                 status      = FLCN_OK;
    CLK_DOMAIN_40_PROG         *pDomain40Prog;
    LwU16                       freqMHz;
    LwU8                        tableRelIdx;

    // Find the matching table entry.
    for (tableRelIdx  = pRel1xTable->tableRelIdxFirst;
         tableRelIdx <= pRel1xTable->tableRelIdxLast;
         tableRelIdx++)
    {
        // Get Src frequency.
        freqMHz = clkPropTopRelsTableRelTupleFreqMHzSrcGet(tableRelIdx);
        if (freqMHz == LW_U16_MIN)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkPropTopRelCache_1X_TABLE_exit;
        }

        // Get Src clock domain.
        pDomain40Prog = CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(
                            clkPropTopRelClkDomainIdxSrcGet(pPropTopRel));
        if (pDomain40Prog == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkPropTopRelCache_1X_TABLE_exit;
        }

        // Adjust frequency with the offset.
        status = clkDomainProgClientFreqDeltaAdj_40(
            CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
            &freqMHz);                                          // pFreqMHz
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPropTopRelCache_1X_TABLE_exit;
        }

        // Update Src frequency.
        status = clkPropTopRelsTableRelOffAdjTupleFreqMHzSrcSet(tableRelIdx, freqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPropTopRelCache_1X_TABLE_exit;
        }

        // Get Dst frequency.
        freqMHz = clkPropTopRelsTableRelTupleFreqMHzDstGet(tableRelIdx);
        if (freqMHz == LW_U16_MIN)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkPropTopRelCache_1X_TABLE_exit;
        }

        // Get Dst clock domain.
        pDomain40Prog = CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(
                            clkPropTopRelClkDomainIdxDstGet(pPropTopRel));
        if (pDomain40Prog == NULL)
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto clkPropTopRelCache_1X_TABLE_exit;
        }

        // Adjust frequency with the offset.
        status = clkDomainProgClientFreqDeltaAdj_40(
            CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
            &freqMHz);                                          // pFreqMHz
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPropTopRelCache_1X_TABLE_exit;
        }

        // Update Dst frequency.
        status = clkPropTopRelsTableRelOffAdjTupleFreqMHzDstSet(tableRelIdx, freqMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto clkPropTopRelCache_1X_TABLE_exit;
        }
    }

clkPropTopRelCache_1X_TABLE_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
