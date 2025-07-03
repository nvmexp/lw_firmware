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
 * @file clk_vf_rel_table.c
 *
 * @brief Module managing all state related to the CLK_VF_REL_TABLE structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_REL_TABLE class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfRelGrpIfaceModel10ObjSet_TABLE
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_VF_REL_TABLE_BOARDOBJ_SET *pVfRelTableSet =
        (RM_PMU_CLK_CLK_VF_REL_TABLE_BOARDOBJ_SET *)pBoardObjSet;
    CLK_VF_REL_TABLE   *pVfRelTable;
    FLCN_STATUS         status;

    status = clkVfRelGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        // Super class triggers PMU Break point on error.
        goto clkVfRelGrpIfaceModel10ObjSet_TABLE_exit;
    }
    pVfRelTable = (CLK_VF_REL_TABLE *)*ppBoardObj;

    // Copy the CLK_VF_REL_TABLE-specific data.
    memcpy(pVfRelTable->secondaryEntries, pVfRelTableSet->secondaryEntries,
        (sizeof(LW2080_CTRL_CLK_CLK_VF_REL_TABLE_SECONDARY_ENTRY)
            * LW2080_CTRL_CLK_CLK_VF_REL_TABLE_SECONDARY_ENTRIES_MAX));

clkVfRelGrpIfaceModel10ObjSet_TABLE_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelFreqTranslatePrimaryToSecondary
 */
FLCN_STATUS
clkVfRelFreqTranslatePrimaryToSecondary_TABLE
(
    CLK_VF_REL     *pVfRel,
    LwU8            secondaryIdx,
    LwU16          *pFreqMHz,
    LwBool          bQuantize
)
{
    LW2080_CTRL_CLK_CLK_VF_REL_TABLE_SECONDARY_ENTRY *pSecondaryEntry;
    CLK_VF_REL_TABLE   *pVfRelTable = (CLK_VF_REL_TABLE *)pVfRel;
    LwU8                i;
    FLCN_STATUS         status = FLCN_ERR_ILWALID_ARGUMENT;

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.vfRels.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &pVfRelTable->secondaryEntries[i];

        // Found matching clkDomIdx.  Compute the ratio frequency for this domain.
        if (secondaryIdx == pSecondaryEntry->clkDomIdx)
        {
            (*pFreqMHz) = pSecondaryEntry->freqMHz;
            status      = FLCN_OK;

            // Colwersion success.
            goto clkVfRelFreqTranslatePrimaryToSecondary_TABLE_exit;
        }
    }

clkVfRelFreqTranslatePrimaryToSecondary_TABLE_exit:
    return status;
}


/*!
 * @copydoc ClkVfRelFreqTranslatePrimaryToSecondary
 */
FLCN_STATUS
clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_TABLE
(
    CLK_VF_REL     *pVfRel,
    LwU8            secondaryIdx,
    LwS16          *pFreqMHz,
    LwBool          bQuantize
)
{
    LW2080_CTRL_CLK_CLK_VF_REL_TABLE_SECONDARY_ENTRY *pSecondaryEntry;
    CLK_VF_REL_TABLE   *pVfRelTable = (CLK_VF_REL_TABLE *)pVfRel;
    LwU8                i;
    FLCN_STATUS         status = FLCN_ERR_ILWALID_ARGUMENT;

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.vfRels.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &pVfRelTable->secondaryEntries[i];

        // Found matching clkDomIdx.  Compute the ratio frequency for this domain.
        if (secondaryIdx == pSecondaryEntry->clkDomIdx)
        {
            //
            // Secondary lwrve takes the same offset as primary lwrve,
            // so no need to change pFreqMHz
            //

            status      = FLCN_OK;

            // Colwersion success.
            goto clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_TABLE_exit;
        }
    }

clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_TABLE_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelFreqTranslateSecondaryToPrimary
 */
FLCN_STATUS
clkVfRelFreqTranslateSecondaryToPrimary_TABLE
(
    CLK_VF_REL     *pVfRel,
    LwU8            secondaryIdx,
    LwU16          *pFreqMHz
)
{
    LW2080_CTRL_CLK_CLK_VF_REL_TABLE_SECONDARY_ENTRY *pSecondaryEntry;
    CLK_VF_REL_TABLE   *pVfRelTable = (CLK_VF_REL_TABLE *)pVfRel;
    LwU8                i;
    FLCN_STATUS         status = FLCN_ERR_ILWALID_ARGUMENT;

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.vfRels.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &pVfRelTable->secondaryEntries[i];

        // Found matching clkDomIdx.  Compute the ratio frequency for this domain.
        if (secondaryIdx == pSecondaryEntry->clkDomIdx)
        {
            // Retrieve the PRIMARY CLK_DOMAIN's TABLE value.
            (*pFreqMHz) = pVfRel->freqMaxMHz;
            status      = FLCN_OK;

            // Colwersion success.
            goto clkVfRelFreqTranslateSecondaryToPrimary_TABLE_exit;
        }
    }

clkVfRelFreqTranslateSecondaryToPrimary_TABLE_exit:
    return status;
}


/*!
 * @copydoc ClkVfRelGetIdxFromFreq
 */
LwU8
clkVfRelGetIdxFromFreq_TABLE
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwU16               freqMHz
)
{
    LW2080_CTRL_CLK_CLK_VF_REL_TABLE_SECONDARY_ENTRY *pSecondaryEntry;
    CLK_VF_REL_TABLE   *pVfRelTable = (CLK_VF_REL_TABLE *)pVfRel;
    LwBool              bValidMatch = LW_FALSE;
    LwU8                i;

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.vfRels.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &pVfRelTable->secondaryEntries[i];

        // Found matching secondaryClkDomIdx.
        if (BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain40Prog->super.super.super.super) == pSecondaryEntry->clkDomIdx)
        {
            if (freqMHz <= pSecondaryEntry->freqMHz)
            {
                bValidMatch = LW_TRUE;
                break;
            }
        }
    }

    return bValidMatch;
}

/* ------------------------- Private Functions ------------------------------ */
