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
 * @file clk_vf_rel_ratio.c
 *
 * @brief Module managing all state related to the CLK_VF_REL_RATIO structure.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"

/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Macros ----------------------------------------- */
/* ------------------------ Public Functions ------------------------------- */
/*!
 * CLK_VF_REL_RATIO class constructor.
 *
 * @copydoc BoardObjGrpIfaceModel10ObjSet
 */
FLCN_STATUS
clkVfRelGrpIfaceModel10ObjSet_RATIO
(
    BOARDOBJGRP_IFACE_MODEL_10        *pModel10,
    BOARDOBJ          **ppBoardObj,
    LwLength            size,
    RM_PMU_BOARDOBJ    *pBoardObjSet
)
{
    RM_PMU_CLK_CLK_VF_REL_RATIO_BOARDOBJ_SET *pVfRelRatioSet =
        (RM_PMU_CLK_CLK_VF_REL_RATIO_BOARDOBJ_SET *)pBoardObjSet;
    CLK_VF_REL_RATIO   *pVfRelRatio;
    FLCN_STATUS         status;

    status = clkVfRelGrpIfaceModel10ObjSet_SUPER(pModel10, ppBoardObj, size, pBoardObjSet);
    if (status != FLCN_OK)
    {
        // Super class triggers PMU Break point on error.
        goto clkVfRelGrpIfaceModel10ObjSet_RATIO_exit;
    }
    pVfRelRatio = (CLK_VF_REL_RATIO *)*ppBoardObj;

    // Copy the CLK_VF_REL_RATIO-specific data.
    memcpy(pVfRelRatio->secondaryEntries, pVfRelRatioSet->secondaryEntries,
        (sizeof(LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRY)
            * LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRIES_MAX));

clkVfRelGrpIfaceModel10ObjSet_RATIO_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelFreqTranslatePrimaryToSecondary
 */
FLCN_STATUS
clkVfRelFreqTranslatePrimaryToSecondary_RATIO
(
    CLK_VF_REL     *pVfRel,
    LwU8            secondaryIdx,
    LwU16          *pFreqMHz,
    LwBool          bQuantize
)
{
    LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRY *pSecondaryEntry;
    CLK_VF_REL_RATIO   *pVfRelRatio = (CLK_VF_REL_RATIO *)pVfRel;
    LwU8                i;
    FLCN_STATUS         status = FLCN_ERR_ILWALID_ARGUMENT;

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.vfRels.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &pVfRelRatio->secondaryEntries[i];

        // Found matching clkDomIdx.  Compute the ratio frequency for this domain.
        if (secondaryIdx == pSecondaryEntry->clkDomIdx)
        {
            LwU32 freqMHz;

            //
            // Do ratio callwlation in 32-bit to avoid overflow.
            //
            // Note: Intentionally not using LW_UNSIGNED_ROUNDED_DIV() - don't
            // want to round up in frequency, as that could cause timing
            // failures (F too high for V).
            //
            freqMHz     = ((*pFreqMHz) * (LwU32)pSecondaryEntry->ratio) / 100;
            (*pFreqMHz) = (LwU16)freqMHz;
            status      = FLCN_OK;

            if (bQuantize)
            {
                CLK_DOMAIN_40_PROG         *pDomain40Prog =
                    CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(secondaryIdx);
                LW2080_CTRL_CLK_VF_INPUT    inputFreq;
                LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;

                if (pDomain40Prog == NULL)
                {
                    PMU_BREAKPOINT();
                    status = FLCN_ERR_ILWALID_INDEX;
                    goto clkVfRelFreqTranslatePrimaryToSecondary_RATIO_exit;
                }

                // Init
                LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
                LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

                inputFreq.value = (*pFreqMHz);

                // Quantize the output secondary frequency if requested.
                status = clkDomainProgFreqQuantize(
                            CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                            &inputFreq,                                         // pInputFreq
                            &outputFreq,                                        // pOutputFreq
                            LW_TRUE,                                            // bFloor
                            LW_FALSE);                                          // bBound
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto clkVfRelFreqTranslatePrimaryToSecondary_RATIO_exit;
                }
                (*pFreqMHz) = (LwU16) outputFreq.value;
            }

            // Colwersion success.
            goto clkVfRelFreqTranslatePrimaryToSecondary_RATIO_exit;
        }
    }

clkVfRelFreqTranslatePrimaryToSecondary_RATIO_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelFreqTranslatePrimaryToSecondary
 */
FLCN_STATUS
clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_RATIO
(
    CLK_VF_REL     *pVfRel,
    LwU8            secondaryIdx,
    LwS16          *pFreqMHz,
    LwBool          bQuantize
)
{
    LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRY *pSecondaryEntry;
    CLK_VF_REL_RATIO   *pVfRelRatio = (CLK_VF_REL_RATIO *)pVfRel;
    LwU8                i;
    FLCN_STATUS         status = FLCN_ERR_ILWALID_ARGUMENT;

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.vfRels.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &pVfRelRatio->secondaryEntries[i];

        // Found matching clkDomIdx.  Compute the ratio frequency for this domain.
        if (secondaryIdx == pSecondaryEntry->clkDomIdx)
        {
            LwS32 freqMHz;

            //
            // Do ratio callwlation in 32-bit to avoid overflow.
            //
            // Note: Intentionally not using LW_UNSIGNED_ROUNDED_DIV() - don't
            // want to round up in frequency, as that could cause timing
            // failures (F too high for V).
            //
            freqMHz     = ((*pFreqMHz) * (LwS32)pSecondaryEntry->ratio) / 100;
            (*pFreqMHz) = (LwS16)freqMHz;
            status      = FLCN_OK;

            // Colwersion success.
            goto clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_RATIO_exit;
        }
    }

clkVfRelOffsetVFFreqTranslatePrimaryToSecondary_RATIO_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelFreqTranslateSecondaryToPrimary
 */
FLCN_STATUS
clkVfRelFreqTranslateSecondaryToPrimary_RATIO
(
    CLK_VF_REL     *pVfRel,
    LwU8            secondaryIdx,
    LwU16          *pFreqMHz
)
{
    LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRY *pSecondaryEntry;
    CLK_VF_REL_RATIO   *pVfRelRatio = (CLK_VF_REL_RATIO *)pVfRel;
    LwU8                i;
    FLCN_STATUS         status = FLCN_ERR_ILWALID_ARGUMENT;

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.vfRels.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &pVfRelRatio->secondaryEntries[i];

        // Found matching clkDomIdx.  Compute the ratio frequency for this domain.
        if (secondaryIdx == pSecondaryEntry->clkDomIdx)
        {
            CLK_DOMAIN_40_PROG         *pDomain40Prog =
                CLK_CLK_DOMAIN_40_PROG_GET_BY_IDX(secondaryIdx);
            LW2080_CTRL_CLK_VF_INPUT    inputFreq;
            LW2080_CTRL_CLK_VF_OUTPUT   outputFreq;
            LwU32                       freqMHz;

            if (pDomain40Prog == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto clkVfRelFreqTranslateSecondaryToPrimary_RATIO_exit;
            }

            //
            // @note This interface must perform the reverse of Primary -> Secondary
            // colwersion. Therefore I am using LW_UNSIGNED_DIV_CEIL and quantizing
            // the result up to get back the original primary frequency.
            //
            freqMHz = (*pFreqMHz);
            freqMHz = LW_UNSIGNED_DIV_CEIL((freqMHz * 100), pSecondaryEntry->ratio);

            // Init
            status  = FLCN_OK;
            LW2080_CTRL_CLK_VF_INPUT_INIT(&inputFreq);
            LW2080_CTRL_CLK_VF_OUTPUT_INIT(&outputFreq);

            inputFreq.value = (LwU16)freqMHz;

            // Quantize the output primary frequency.
            status = clkDomainProgFreqQuantize(
                        CLK_DOMAIN_PROG_GET_FROM_40_PROG(pDomain40Prog),    // pDomainProg
                        &inputFreq,                                         // pInputFreq
                        &outputFreq,                                        // pOutputFreq
                        LW_FALSE,                                           // bFloor
                        LW_FALSE);                                          // bBound
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkVfRelFreqTranslateSecondaryToPrimary_RATIO_exit;
            }
            (*pFreqMHz) = (LwU16) outputFreq.value;

            // Colwersion success.
            goto clkVfRelFreqTranslateSecondaryToPrimary_RATIO_exit;
        }
    }

clkVfRelFreqTranslateSecondaryToPrimary_RATIO_exit:
    return status;
}

/*!
 * @copydoc ClkVfRelGetIdxFromFreq
 */
LwBool
clkVfRelGetIdxFromFreq_RATIO
(
    CLK_VF_REL         *pVfRel,
    CLK_DOMAIN_40_PROG *pDomain40Prog,
    LwU16               freqMHz
)
{
    LW2080_CTRL_CLK_CLK_VF_REL_RATIO_SECONDARY_ENTRY *pSecondaryEntry;
    CLK_VF_REL_RATIO   *pVfRelRatio = (CLK_VF_REL_RATIO *)pVfRel;
    LwBool              bValidMatch = LW_FALSE;
    LwU8                i;

    // Loop over the secondary entries to find matching clkDomIdx.
    for (i = 0; i < Clk.vfRels.secondaryEntryCount; i++)
    {
        pSecondaryEntry = &pVfRelRatio->secondaryEntries[i];

        // Found matching secondaryClkDomIdx.
        if (BOARDOBJ_GET_GRP_IDX_8BIT(&pDomain40Prog->super.super.super.super) == pSecondaryEntry->clkDomIdx)
        {
            if (freqMHz <= ((pSecondaryEntry->ratio * pVfRel->freqMaxMHz)/100U))
            {
                bValidMatch = LW_TRUE;
                break;
            }
        }
    }

    return bValidMatch;
}

/* ------------------------- Private Functions ------------------------------ */
