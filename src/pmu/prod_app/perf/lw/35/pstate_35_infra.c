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
 * @file    pstate_35_infra.c
 * @brief   Function definitions for the PSTATE_35 SW module's core infrastructure.
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"
#include "dmemovl.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objperf.h"
#include "pmu_objclk.h"

/* ------------------------ Static Function Prototypes ---------------------- */
static LwBool       s_perfPstateIsFreqQuantized(CLK_DOMAIN *pDomain, LwU32 freqkHz)
    GCC_ATTRIB_SECTION("imem_libPstateBoardObj", "s_perfPstateIsFreqQuantized");

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Prototypes ------------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * @copydoc     PerfPstate35ClkFreqGetRef()
 *
 * @memberof    PSTATE_35
 *
 * @protected
 */
FLCN_STATUS
perfPstate35ClkFreqGetRef
(
    PSTATE_35                                  *pPstate35,
    LwU8                                        clkDomainIdx,
    const PERF_PSTATE_35_CLOCK_ENTRY          **ppcPstateClkEntry
)
{
    FLCN_STATUS status = FLCN_OK;

    if ((pPstate35 == NULL) ||
        (!BOARDOBJGRP_IS_VALID(CLK_DOMAIN, clkDomainIdx)) ||
        (ppcPstateClkEntry == NULL))
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_ARGUMENT;
        goto perfPstate35ClkFreqGetRef_exit;
    }

    *ppcPstateClkEntry = &pPstate35->pClkEntries[clkDomainIdx];

perfPstate35ClkFreqGetRef_exit:
    return status;
}

/*!
 * @brief       PSTATE_35 implementation of @ref PerfPstateCache()
 *
 * @copydoc     PerfPstateCache()
 *
 * @memberof    PSTATE_35
 *
 * @protected
 */
FLCN_STATUS
perfPstateCache_35
(
    PSTATE *pPstate
)
{
    FLCN_STATUS                             status  = FLCN_OK;
    CLK_DOMAIN                             *pDomain;
    CLK_DOMAIN_PROG                        *pDomainProg;
    PERF_PSTATE_35_CLOCK_ENTRY             *pPstateClkEntry;
    LwBoardObjIdx                           clkIdx, idx;
    VOLT_RAIL                              *pRail;
    LwU32                                   freqMaxkHz;
    PSTATE_35                              *pPstate35;
    BOARDOBJGRPMASK_E32                    *pMask;
    LW2080_CTRL_PERF_PSTATE_VOLT_RAIL      *pVoltEntry = NULL;
    LwU32                                  *pFreqMaxAtVminkHz = NULL;

    BOARDOBJ_DYNAMIC_CAST_OR_GOTO(pPstate35, &pPstate->super, PERF, PSTATE, 35,
                                  &status, perfPstateCache_35_exit);

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN))
    {
        // Reset Vmin entries before recallwlation
        BOARDOBJGRP_ITERATOR_BEGIN(VOLT_RAIL, pRail, idx, NULL)
        {
            pVoltEntry = perfPstateVoltEntriesGetIdxRef(pPstate35, idx);
            pVoltEntry->vMinuV = 0;
        }
        BOARDOBJGRP_ITERATOR_END;
    }

    // Iterate through programmable clk domains.
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
        &Clk.domains.progDomainsMask.super)
    {
        pPstateClkEntry = &pPstate35->pClkEntries[clkIdx];

        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfPstateCache_35_exit;
        }

        // Use un-offseted PSTATE frequency under debug mode
        if (clkDomainsDebugModeEnabled())
        {
            //
            // PCIe is consider special case as HW may not support all the POR PCIe Gen
            // speed and therefore we may have PSTATE frequency tuple trimmed down to
            // the only supported PCIe Gen speeds. This is not an error and therefore we
            // must use the POR PCIe gen speed under debug mode.
            //
            if (pDomain->domain == clkWhich_PCIEGenClk)
            {
                pPstateClkEntry->nom.freqkHz = pPstateClkEntry->nom.porFreqkHz;
                pPstateClkEntry->min.freqkHz = pPstateClkEntry->min.porFreqkHz;
                pPstateClkEntry->max.freqkHz = pPstateClkEntry->max.porFreqkHz;
            }
            else
            {
                // In debug mode, use the LW POR frequency tuple
                pPstateClkEntry->nom.freqkHz = pPstateClkEntry->nom.origFreqkHz;
                pPstateClkEntry->min.freqkHz = pPstateClkEntry->min.origFreqkHz;
                pPstateClkEntry->max.freqkHz = pPstateClkEntry->max.origFreqkHz;
            }
        }
        else if ((pDomain->domain == clkWhich_PCIEGenClk) ||
                ((!PSTATE_OCOV_ENABLED(pPstate)) &&
                 (!clkDomainsOverrideOVOCEnabled())))
        {
            pPstateClkEntry->nom.freqkHz = pPstateClkEntry->nom.baseFreqkHz;
            pPstateClkEntry->min.freqkHz = pPstateClkEntry->min.baseFreqkHz;
            pPstateClkEntry->max.freqkHz = pPstateClkEntry->max.baseFreqkHz;
        }
        else
        {
            LwU16   targetFreqMHz   = (LwU16)(pPstateClkEntry->nom.baseFreqkHz / 1000U);
            LwU16   freqRangeMinMHz = (LwU16)(pPstateClkEntry->min.baseFreqkHz / 1000U);
            LwU16   freqRangeMaxMHz = (LwU16)(pPstateClkEntry->max.baseFreqkHz / 1000U);

            // Adjust PSTATE::min frequency tuple
            status = clkDomainProgClientFreqDeltaAdj(
                        pDomainProg,
                        &targetFreqMHz);    // pFreqMHz
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfPstateCache_35_exit;
            }

            // Adjust PSTATE::nom frequency tuple
            status = clkDomainProgClientFreqDeltaAdj(
                        pDomainProg,
                        &freqRangeMinMHz);  // pFreqMHz
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfPstateCache_35_exit;
            }

            status = clkDomainProgClientFreqDeltaAdj(
                        pDomainProg,
                        &freqRangeMaxMHz);  // pFreqMHz
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfPstateCache_35_exit;
            }

            pPstateClkEntry->nom.freqkHz = (LwU32)targetFreqMHz   * 1000U;
            pPstateClkEntry->min.freqkHz = (LwU32)freqRangeMinMHz * 1000U;
            pPstateClkEntry->max.freqkHz = (LwU32)freqRangeMaxMHz * 1000U;
        }

        //
        // Trim and cache all VF Max frequency.
        //
        // First "correct" the pstate range coherancy issues that may have resulted from OC
        // Enforce min <= nom <= max
        //
        if ((pPstateClkEntry->max.freqkHz < pPstateClkEntry->min.freqkHz) ||
            (pPstateClkEntry->max.freqkHz < pPstateClkEntry->nom.freqkHz) ||
            (pPstateClkEntry->nom.freqkHz < pPstateClkEntry->min.freqkHz))
        {
            pPstateClkEntry->min.freqkHz = LW_MIN(pPstateClkEntry->min.freqkHz, pPstateClkEntry->nom.freqkHz);
            pPstateClkEntry->min.freqkHz = LW_MIN(pPstateClkEntry->min.freqkHz, pPstateClkEntry->max.freqkHz);
            pPstateClkEntry->nom.freqkHz = LW_MIN(pPstateClkEntry->nom.freqkHz, pPstateClkEntry->max.freqkHz);

            // Breakpoint here to raise the issue, do not return bad status.
            PMU_BREAKPOINT();
        }

        status = clkDomainProgVfMaxFreqMHzGet(
                    pDomainProg, &freqMaxkHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfPstateCache_35_exit;
        }

        // Scale the value from MHz -> kHz.
        freqMaxkHz *= clkDomainFreqkHzScaleGet(pDomain);

        pPstateClkEntry->nom.freqVFMaxkHz = LW_MIN(freqMaxkHz, pPstateClkEntry->nom.freqkHz);
        pPstateClkEntry->min.freqVFMaxkHz = LW_MIN(freqMaxkHz, pPstateClkEntry->min.freqkHz);
        pPstateClkEntry->max.freqVFMaxkHz = LW_MIN(freqMaxkHz, pPstateClkEntry->max.freqkHz);

        if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN))
        {
            // Find Vmin for each clock domain
            pMask = CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK_GET(pDomainProg);
            if (pMask == NULL || boardObjGrpMaskIsZero(pMask))
            {
                continue;
            }

            BOARDOBJGRPMASK_FOR_EACH_BEGIN(pMask, idx)
            {
                pVoltEntry = perfPstateVoltEntriesGetIdxRef(pPstate35, idx);
                PERF_LIMITS_VF  vfDomain;
                PERF_LIMITS_VF  vfRail;

                vfDomain.idx = clkIdx;
                vfDomain.value = pPstateClkEntry->min.freqVFMaxkHz;
                vfRail.idx  = (LwU8)idx;

                PMU_ASSERT_OK_OR_GOTO(status,
                    perfLimitsFreqkHzToVoltageuV(&vfDomain, &vfRail, LW_TRUE),
                    perfPstateCache_35_exit);

                pVoltEntry->vMinuV = LW_MAX(pVoltEntry->vMinuV, vfRail.value);
            }
            BOARDOBJGRPMASK_FOR_EACH_END;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_PSTATES_35_FMAX_AT_VMIN))
    {
        PERF_LIMITS_PSTATE_RANGE pStateRange;
        PERF_LIMITS_RANGE_SET_VALUE(&pStateRange,
            BOARDOBJ_GET_GRP_IDX(&pPstate->super));

        // Do math for Fmax@Vmin
        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx,
            &Clk.domains.progDomainsMask.super)
        {
            pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
            if (pDomainProg == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto perfPstateCache_35_exit;
            }

            pMask = CLK_DOMAIN_PROG_VOLT_RAIL_VMIN_MASK_GET(pDomainProg);
            pFreqMaxAtVminkHz = perfPstateClkEntryFmaxAtVminGetRef(&pPstate35->pClkEntries[clkIdx]);
            *pFreqMaxAtVminkHz = PERF_PSTATE_FMAX_AT_VMIN_UNSUPPORTED;
            if (pMask == NULL || boardObjGrpMaskIsZero(pMask))
            {
                continue;
            }

            BOARDOBJGRPMASK_FOR_EACH_BEGIN(pMask, idx)
            {
                pVoltEntry = perfPstateVoltEntriesGetIdxRef(pPstate35, idx);

                PERF_LIMITS_VF  vfDomain;
                PERF_LIMITS_VF  vfRail;

                vfRail.idx = (LwU8) idx;
                vfRail.value = pVoltEntry->vMinuV;
                vfDomain.idx = (LwU8)clkIdx;

                PMU_ASSERT_OK_OR_GOTO(status,
                    perfLimitsVoltageuVToFreqkHz(&vfRail, &pStateRange, &vfDomain, LW_TRUE, LW_TRUE),
                    perfPstateCache_35_exit);

                *pFreqMaxAtVminkHz = LW_MIN(*pFreqMaxAtVminkHz, vfDomain.value);
            }
            BOARDOBJGRPMASK_FOR_EACH_END;

            if (*pFreqMaxAtVminkHz == PERF_PSTATE_FMAX_AT_VMIN_UNSUPPORTED)
            {
                status = FLCN_ERR_FREQ_NOT_SUPPORTED;
                PMU_BREAKPOINT();
                goto perfPstateCache_35_exit;
            }
        }
        BOARDOBJGRP_ITERATOR_END;
    }

perfPstateCache_35_exit:
    return status;
}

/*!
 * @brief       Validates that the clock tuples passed to the PMU are valid.
 *
 * In short, this routine checks that all orig frequency values are
 * properly quantized for each programmable clock domain.
 *
 * @param[in]   pPstate35   pointer to PSTATE_35 object being sanity checked.
 *
 * @return      FLCN_OK if PSTATE's tuples are sane.
 * @return      FLCN_ERR_ILWALID_ARGUMENT if any clock entry is not sane.
 *
 * @memberof    PSTATE_35
 *
 * @protected
 */
FLCN_STATUS
perfPstate35SanityCheckOrig
(
    PSTATE_35 *pPstate35
)
{
    CLK_DOMAIN     *pDomain;
    BOARDOBJGRPMASK progDomainsMask = CLK_CLK_DOMAINS_GET()->progDomainsMask.super;
    LwBoardObjIdx   clkIdx;
    FLCN_STATUS     status          = FLCN_OK;

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx, &progDomainsMask)
    {
        PERF_PSTATE_35_CLOCK_ENTRY clkEntry = pPstate35->pClkEntries[clkIdx];

        // Check the min ORIG freq
        if (!(s_perfPstateIsFreqQuantized(pDomain, clkEntry.min.origFreqkHz)))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfPstate35SanityCheckOrig_exit;
        }

        // Check the max ORIG freq
        if (!(s_perfPstateIsFreqQuantized(pDomain, clkEntry.max.origFreqkHz)))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfPstate35SanityCheckOrig_exit;
        }

        // Check the nominal ORIG freq
        if (!(s_perfPstateIsFreqQuantized(pDomain, clkEntry.nom.origFreqkHz)))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_ARGUMENT;
            goto perfPstate35SanityCheckOrig_exit;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

perfPstate35SanityCheckOrig_exit:
    return status;
}

/* ------------------------- Private Functions ------------------------------ */
/*!
 * @brief       Check if a given frequency is correctly quantized as per its
 *              CLK_DOMAIN_3X_PROG quantization rules.
 *
 * @param[in]   pDomain         Pointer to the CLK_DOMAIN this frequency is
 *                              programmed by.
 * @param[in]   freqkHz         Frequency value in kHz to be checked.
 *
 * @return      LW_TRUE         if frequency is quantized.
 * @return      LW_FALSE        if frequency is NOT quantized.
 *
 * @memberof    PSTATE_35
 *
 * @private
 */
static LwBool
s_perfPstateIsFreqQuantized(CLK_DOMAIN *pDomain, LwU32 freqkHz)
{
    CLK_DOMAIN_PROG            *pDomainProg;
    LW2080_CTRL_CLK_VF_INPUT    preQuantizedClk;
    LW2080_CTRL_CLK_VF_OUTPUT   postQuantizedClk;
    LwBool                      bQuantized = LW_TRUE;
    FLCN_STATUS                 status;

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        bQuantized = LW_FALSE;
        PMU_BREAKPOINT();
        goto s_perfpStateIsFreqQuantized_exit;
    }

    LW2080_CTRL_CLK_VF_INPUT_INIT(&preQuantizedClk);
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&postQuantizedClk);

    // Request default value.
    preQuantizedClk.flags = (LwU8)FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS,
                                _VF_POINT_SET_DEFAULT, _YES, preQuantizedClk.flags);

    // Scale the input freq to MHz
    preQuantizedClk.value = freqkHz / clkDomainFreqkHzScaleGet(pDomain);

    // Quantize & compare
    status = clkDomainProgFreqQuantize(
                pDomainProg,                    // pDomainProg
                &preQuantizedClk,               // pInputFreq
                &postQuantizedClk,              // pOutputFreq
                LW_FALSE,                       // bFloor
                LW_TRUE);                       // bBound
    if ((status != FLCN_OK) ||
        (LW2080_CTRL_CLK_VF_VALUE_ILWALID == postQuantizedClk.inputBestMatch))
    {
        bQuantized = LW_FALSE;
        PMU_BREAKPOINT();
        goto s_perfpStateIsFreqQuantized_exit;
    }

    if (preQuantizedClk.value != postQuantizedClk.value)
    {
        bQuantized = LW_FALSE;
        goto s_perfpStateIsFreqQuantized_exit;
    }

s_perfpStateIsFreqQuantized_exit:
    return bQuantized;
}

/*!
 * @brief       For each programmable clock domain, adjust PSTATE's porFreqkHz
 *              by the factory OC.
 *
 * @param[in,out]   pPstate35   Pointer to PSTATE_35 object being adjusted.
 *
 * @return      FLCN_OK if success.
 * @return      OTHER   if nested function calls fail.
 *
 * @memberof    PSTATE_35
 *
 * @private
 */
FLCN_STATUS
perfPstate35AdjFreqTupleByFactoryOCOffset
(
    PSTATE_35 *pPstate35
)
{
    CLK_DOMAIN         *pDomain;
    CLK_DOMAIN_PROG    *pDomainProg;
    BOARDOBJGRPMASK     progDomainsMask =
        CLK_CLK_DOMAINS_GET()->progDomainsMask.super;
    LwBoardObjIdx       clkIdx;
    FLCN_STATUS         status          = FLCN_OK;

    // Check that the freq OC is enabled for this pstate
    if (!PSTATE_OCOV_ENABLED(&pPstate35->super.super))
    {
        goto perfPstate35AdjFreqTupleByFactoryOCOffset_exit;
    }

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx, &progDomainsMask)
    {
        // Map local pointers
        PERF_PSTATE_35_CLOCK_ENTRY *pPerfClkEntry = &pPstate35->pClkEntries[clkIdx];

        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfPstate35AdjFreqTupleByFactoryOCOffset_exit;
        }

        // Scale por tuple's freqs to MHz
        LwU32 scale  = clkDomainFreqkHzScaleGet(pDomain);
        LwU16 minMHz = (LwU16) (pPerfClkEntry->min.porFreqkHz / scale);
        LwU16 maxMHz = (LwU16) (pPerfClkEntry->max.porFreqkHz / scale);
        LwU16 nomMHz = (LwU16) (pPerfClkEntry->nom.porFreqkHz / scale);

        // Adjust the tuple's freqs by the domain's factory OC delta
        status = clkDomainProgFactoryDeltaAdj(
                    pDomainProg,
                    &minMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfPstate35AdjFreqTupleByFactoryOCOffset_exit;
        }
        status = clkDomainProgFactoryDeltaAdj(
                    pDomainProg,
                    &maxMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfPstate35AdjFreqTupleByFactoryOCOffset_exit;
        }
        status = clkDomainProgFactoryDeltaAdj(
                    pDomainProg,
                    &nomMHz);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfPstate35AdjFreqTupleByFactoryOCOffset_exit;
        }

        // Scale the adjusted tuple back to kHz and set
        pPerfClkEntry->min.porFreqkHz = minMHz * scale;
        pPerfClkEntry->max.porFreqkHz = maxMHz * scale;
        pPerfClkEntry->nom.porFreqkHz = nomMHz * scale;
    }
    BOARDOBJGRP_ITERATOR_END;

perfPstate35AdjFreqTupleByFactoryOCOffset_exit:
    return status;
}

/* ------------------------- End of File ------------------------------------ */
