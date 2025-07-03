/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file    changeseqscriptstep_clkmon.c
 * @brief   Implementation for handling CLK_MON-based steps by the change sequencer.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objclk.h"
#include "perf/35/changeseqscriptstep_clk.h"
#include "ctrl/ctrl2080/ctrl2080boardobj.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Builds a pre-volt clock step for a change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS
 * @public
 * @copydetails PerfDaemonChangeSeqScriptBuildStep()
 *
 * For more details, check the Confluence page for the design and pseudocode:<br/>
 */
//! @li Design : https://confluence.lwpu.com/display/RMPER/VF+Switch+Programming+Sequence
//! @li Pseudocode : https://confluence.lwpu.com/display/RMPER/VF+Switch+Programming+Sequence#VFSwitchProgrammingSequence-Pseudocode.1
FLCN_STATUS
perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *pStepClk  =
        &pScript->pStepLwrr->data.clk;
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx idx;
    FLCN_STATUS   status         = FLCN_OK;

    // Update DVCO min for all NAFLL clocks.
    status = clkNafllGrpDvcoMinCompute(&pChangeLwrr->voltList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
    }

    // Compute ADC code correction offset
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION))
    {
        status = clkAdcsComputeCodeOffset(LW_FALSE, LW_FALSE, &pChangeLwrr->voltList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
        }
    }

    // Init
    pStepClk->clkList.numDomains = 0U;

    // Build clock list
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, idx, &pScript->clkDomainsActiveMask.super)
    {
        LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM    clkListItemTarget = {0};
        LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM   *pClkListItemLwrr  =
            &pChangeLwrr->clkList.clkDomains[idx];
        LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM   *pClkListItemLast  =
            &pChangeLast->clkList.clkDomains[idx];
        CLK_DOMAIN_3X                          *pDomain3x = (CLK_DOMAIN_3X *)pDomain;
        CLK_DOMAIN_PROG                        *pDomainProg;
        LwU8                                    hwRegimeIdLwrr;
        LwU8                                    hwRegimeIdLast;
        LwU8                                    preVoltOrderingIndex;

        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
        }

        // Sanity check
        if ((clkDomainApiDomainGet(pDomain) != pClkListItemLwrr->clkDomain) ||
            (clkDomainApiDomainGet(pDomain) != pClkListItemLast->clkDomain))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
        }

        // Update the clock domain info.
        clkListItemTarget.clkDomain = pClkListItemLwrr->clkDomain;

        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_3X))
        {
            // Update the source info.
            clkListItemTarget.source =
                clkDomainGetSource(pDomain, (pClkListItemLwrr->clkFreqKHz / 1000U));
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS))
        {
            // Always NAFLL for Auto profiles
            clkListItemTarget.source = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;
        }
        else
        {
            // GA10x+ Perf will not provide clock source info.
            clkListItemTarget.source = LW2080_CTRL_CLK_PROG_1X_SOURCE_DEFAULT;
        }

        //
        // Iterate over each clock to decide if it is required to be updated
        // in PRE_VOLT step based on current vs target frequency (and regime)
        //

        // Noise Aware Clocks programming.
        if (pDomain3x->bNoiseAwareCapable)
        {
            LwBool bSkipPldivBelowDvcoMin;

            //
            // Update the current change struct dvco min frequency.
            // The PRE_VOLT change step will callwlate the DVCO min and the
            // POST_VOLT step will use the same readings instead of re-callwlation
            // to ensure the temperature variation does not clobber the results.
            //
            status = clkNafllDvcoMinFreqMHzGet(pClkListItemLwrr->clkDomain,
                                         &pClkListItemLwrr->dvcoMinFreqMHz,
                                         &bSkipPldivBelowDvcoMin);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
            }
            (void)bSkipPldivBelowDvcoMin;

            clkListItemTarget.dvcoMinFreqMHz = pClkListItemLwrr->dvcoMinFreqMHz;

            //
            // Determine the target frequency regime and update the input
            // current change struct
            // Regime determination must be done during the pre-volt step
            // and use the cached value in @ref pChangeLwrrduring the post
            // volt step.
            //
            status = clkNafllTargetRegimeGet(pClkListItemLwrr,
                                             &pChangeLwrr->voltList);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
            }

            // Get current HW regime id from current SW regime id
            status = clkNafllGetHwRegimeBySwRegime(pClkListItemLwrr->regimeId, &hwRegimeIdLwrr);
            if (status != FLCN_OK)
            {
                goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
            }

            // get last HW regime id from last SW regime id
            status = clkNafllGetHwRegimeBySwRegime(pClkListItemLast->regimeId, &hwRegimeIdLast);
            if (status != FLCN_OK)
            {
                goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
            }

            //
            // In case of FFR -> FFR switch, we want NAFLL to behave more like
            // noise unaware clocks and do not track voltage. By Setting the min
            // of current and target frequency in FR during the pre-volt step,
            // we will ensure this behavior. In post-volt step, we will set the
            // actual target regime and target frequency.
            //
            if (((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ) &&
                (hwRegimeIdLast  ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ)))
            {
                clkListItemTarget.clkFreqKHz =
                    LW_MIN(pClkListItemLwrr->clkFreqKHz,
                           pClkListItemLast->clkFreqKHz);
                clkListItemTarget.regimeId   = pClkListItemLwrr->regimeId;

                // Sanity check
                if ((pClkListItemLwrr->regimeId != LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR) &&
                    (pClkListItemLwrr->regimeId != LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR_BELOW_DVCO_MIN))
                {
                    status = FLCN_ERR_ILWALID_STATE;
                    PMU_BREAKPOINT();
                    goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
                }
            }
            //
            // If the current and / or target regime is FR or the current
            // regime in FFR and target regime is VF, program the target
            // regime to FR with target frequency equal to max of current
            // and target frequency. This is done to ensure that we get
            // maximum frequency change due to change in voltage.
            // In post voltage clock step, we will set the actual target
            // regime and target frequency.
            //
            else if ((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN) ||
                    (hwRegimeIdLast  ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN) ||
                    ((hwRegimeIdLast ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ) &&
                     (hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ)))
            {
                clkListItemTarget.clkFreqKHz     =
                    LW_MAX(pClkListItemLwrr->clkFreqKHz,
                           pClkListItemLast->clkFreqKHz);
                clkListItemTarget.regimeId = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FR;
            }
            //
            // In above case with FR, by setting max of current and target
            // frequency, we are going to higher frequency in pre-volt step
            // whereas lower freq in post volt step. To maintain the same
            // behavior, we are required to reverse the programming order
            // in VR -> VR switch.
            //
            else if ((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ) &&
                    (hwRegimeIdLast ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ) &&
                    (pClkListItemLwrr->clkFreqKHz > pClkListItemLast->clkFreqKHz))
            {
                clkListItemTarget.clkFreqKHz = pClkListItemLwrr->clkFreqKHz;
                clkListItemTarget.regimeId   = pClkListItemLwrr->regimeId;
            }
            // VR -> VR with decreasing frequency will be programmed in post volt step.
            else if ((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ) &&
                     (hwRegimeIdLast ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ) &&
                     (pClkListItemLwrr->clkFreqKHz <= pClkListItemLast->clkFreqKHz))
            {
                continue;
            }
            //
            // For VR -> FFR, We want to track the voltage and set the target
            // in post volt step.
            //
            else if ((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ) &&
                     (hwRegimeIdLast ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ))
            {
                continue;
            }
            else
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
            }
        }
        // Noise Unaware Clocks
        else
        {
            // Program all decreasing clocks in pre voltage step.
            if (pClkListItemLwrr->clkFreqKHz <= pClkListItemLast->clkFreqKHz)
            {
                clkListItemTarget.clkFreqKHz = pClkListItemLwrr->clkFreqKHz;
            }
            else
            {
                continue;
            }
        }

        preVoltOrderingIndex = clkDomainProgPreVoltOrderingIndexGet(pDomainProg);
        if ((preVoltOrderingIndex == LW2080_CTRL_CLK_CLK_DOMAIN_3X_PROG_ORDERING_INDEX_ILWALID) ||
            (preVoltOrderingIndex >= LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS))
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit;
        }

        // Insert the clock domain in the correct order based on step.
        pStepClk->clkList.clkDomains[preVoltOrderingIndex] =
                     clkListItemTarget;

        // Update the numDomains.
        pStepClk->clkList.numDomains =
            LW_MAX(pStepClk->clkList.numDomains, (preVoltOrderingIndex + 1U));
    }
    BOARDOBJGRP_ITERATOR_END;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_ADC_OVERRIDE_20))
    {
        // Callwlate ADC voltage override.
        LwU8 voltRailIdx;
        for (voltRailIdx = 0; voltRailIdx < pChangeLwrr->voltList.numRails; voltRailIdx++)
        {
            LwS32 tempChangeLwrrVoltageuV;
            LwS32 tempChangeLastVoltageuV;

            // Update the volt rail mask.
            LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&(pStepClk->adcSwOverrideList.voltRailsMask.super),
                                                 voltRailIdx);

            // Update requested mode of operation to MIN(HW, SW)
            pStepClk->adcSwOverrideList.volt[voltRailIdx].overrideMode =
                LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_MIN;

            //
            // Update the voltage to be programmed.
            // In PRE_VOLT step, ADC must relax it's override cap to the max of current
            // and target voltage and therefore allow NAFLL clocks to follow voltage step
            // up / step down lwrve.
            //
            tempChangeLwrrVoltageuV = (LwS32)pChangeLwrr->voltList.rails[voltRailIdx].voltageuV +
                pChangeLwrr->voltList.rails[voltRailIdx].voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC];
            tempChangeLastVoltageuV = (LwS32)pChangeLast->voltList.rails[voltRailIdx].voltageuV +
                pChangeLast->voltList.rails[voltRailIdx].voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC];

            // Done in multiple steps to avoid MISRA Rule 10.8 violations
            pStepClk->adcSwOverrideList.volt[voltRailIdx].voltageuV =
                (LwU32)LW_MAX(tempChangeLwrrVoltageuV, tempChangeLastVoltageuV);
        }
    }

perfDaemonChangeSeq35ScriptBuildStep_PRE_VOLT_CLKS_exit:
    return status;
}

/*!
 * @brief Builds a post-volt clock step for a change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS
 * @public
 * @copydetails PerfDaemonChangeSeqScriptBuildStep()
 *
 * For more details, check the Confluence page for the design and pseudocode:<br/>
 */
//! @li Design : https://confluence.lwpu.com/display/RMPER/VF+Switch+Programming+Sequence
//! @li Pseudocode : https://confluence.lwpu.com/display/RMPER/VF+Switch+Programming+Sequence#VFSwitchProgrammingSequence-Pseudocode.1
FLCN_STATUS
perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *pStepClk  =
        &pScript->pStepLwrr->data.clk;
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx idx;
    FLCN_STATUS   status  = FLCN_OK;

    // Init
    pStepClk->clkList.numDomains = 0U;

    // Build clock list
    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, idx, &pScript->clkDomainsActiveMask.super)
    {
        LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM    clkListItemTarget = {0};
        LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM   *pClkListItemLwrr  =
            &pChangeLwrr->clkList.clkDomains[idx];
        LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM   *pClkListItemLast  =
            &pChangeLast->clkList.clkDomains[idx];
        CLK_DOMAIN_3X                          *pDomain3x = (CLK_DOMAIN_3X *)pDomain;
        CLK_DOMAIN_PROG                        *pDomainProg;
        LwU8                                    hwRegimeIdLwrr;
        LwU8                                    hwRegimeIdLast;
        LwU8                                    postVoltOrderingIndex;

        pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
        if (pDomainProg == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_exit;
        }

        // Sanity check
        if ((clkDomainApiDomainGet(pDomain) != pClkListItemLwrr->clkDomain) ||
            (clkDomainApiDomainGet(pDomain) != pClkListItemLast->clkDomain))
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_exit;
        }

        // Update the clock domain info.
        clkListItemTarget.clkDomain = pClkListItemLwrr->clkDomain;

        if (PMUCFG_FEATURE_ENABLED(PMU_CLK_CLK_DOMAIN_EXTENDED_3X))
        {
            // Update the source info.
            clkListItemTarget.source =
                clkDomainGetSource(pDomain, (pClkListItemLwrr->clkFreqKHz / 1000U));
        }
        else if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_STEP_PRE_POST_VOLT_NAFLL_CLKS))
        {
            // Always NAFLL for Auto profiles
            clkListItemTarget.source = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;
        }
        else
        {
            // GA10x+ Perf will not provide clock source info.
            clkListItemTarget.source = LW2080_CTRL_CLK_PROG_1X_SOURCE_DEFAULT;
        }

        //
        // Iterate over each clock to decide if it is required to be updated
        // in POST_VOLT step based on current vs target frequency (and regime)
        //

        // Noise Aware Clocks programming.
        if (pDomain3x->bNoiseAwareCapable)
        {
            // Update the DVCO min frequency.
            clkListItemTarget.dvcoMinFreqMHz = pClkListItemLwrr->dvcoMinFreqMHz;

            // Get current HW regime id from current SW regime id
            status = clkNafllGetHwRegimeBySwRegime(pClkListItemLwrr->regimeId, &hwRegimeIdLwrr);
            if (status != FLCN_OK)
            {
                goto perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_exit;
            }

            // get last HW regime id from last SW regime id
            status = clkNafllGetHwRegimeBySwRegime(pClkListItemLast->regimeId, &hwRegimeIdLast);
            if (status != FLCN_OK)
            {
                goto perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_exit;
            }

            //
            // In case of FFR -> FFR switch, we want NAFLL to behave more like
            // noise unaware clocks and do not track voltage. By Setting the min
            // of current and target frequency in FR during the pre-volt step,
            // we will ensure this behavior. In post-volt step, we will set the
            // actual target regime and target frequency.
            //
            if (((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ) &&
                (hwRegimeIdLast  ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ)))
            {
                clkListItemTarget.clkFreqKHz = pClkListItemLwrr->clkFreqKHz;
                clkListItemTarget.regimeId   = pClkListItemLwrr->regimeId;
            }
            //
            // If the current and / or target regime is FR or the current
            // regime in FFR and target regime is VF, program the target
            // regime to FR with target frequency equal to max of current
            // and target frequency. This is done to ensure that we get
            // maximum frequency change due to change in voltage.
            // In post voltage clock step, we will set the actual target
            // regime and target frequency.
            //
            else if ((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN) ||
                    (hwRegimeIdLast  ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_MIN) ||
                    ((hwRegimeIdLast ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ) &&
                     (hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ)))
            {
                clkListItemTarget.clkFreqKHz = pClkListItemLwrr->clkFreqKHz;
                clkListItemTarget.regimeId   = pClkListItemLwrr->regimeId;
            }
            // VR -> VR with increasing frequency was programmed in pre volt step.
            else if ((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ) &&
                    (hwRegimeIdLast ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ) &&
                    (pClkListItemLwrr->clkFreqKHz > pClkListItemLast->clkFreqKHz))
            {
                continue;
            }
            // VR -> VR with decreasing frequency will be programmed in post volt step.
            else if ((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ) &&
                     (hwRegimeIdLast ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ) &&
                     (pClkListItemLwrr->clkFreqKHz <= pClkListItemLast->clkFreqKHz))
            {
                clkListItemTarget.clkFreqKHz = pClkListItemLwrr->clkFreqKHz;
                clkListItemTarget.regimeId   = pClkListItemLwrr->regimeId;
            }
            //
            // For VR -> FFR, We want to track the voltage and set the target
            // in post volt step.
            //
            else if ((hwRegimeIdLwrr ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_SW_REQ) &&
                     (hwRegimeIdLast ==
                        LW2080_CTRL_CLK_NAFLL_SW_OVERRIDE_LUT_USE_HW_REQ))
            {
                clkListItemTarget.clkFreqKHz = pClkListItemLwrr->clkFreqKHz;
                clkListItemTarget.regimeId   = pClkListItemLwrr->regimeId;
            }
            else
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_exit;
            }
        }
        // Noise Unaware Clocks
        else
        {
            // Program all increasing clocks in pre voltage step.
            if (pClkListItemLwrr->clkFreqKHz > pClkListItemLast->clkFreqKHz)
            {
                clkListItemTarget.clkFreqKHz = pClkListItemLwrr->clkFreqKHz;
            }
            else
            {
                continue;
            }
        }

        postVoltOrderingIndex = clkDomainProgPostVoltOrderingIndexGet(pDomainProg);
        if ((postVoltOrderingIndex == LW2080_CTRL_CLK_CLK_DOMAIN_3X_PROG_ORDERING_INDEX_ILWALID) ||
            (postVoltOrderingIndex >= LW2080_CTRL_BOARDOBJ_MAX_BOARD_OBJECTS))
        {
            status = FLCN_ERR_ILWALID_INDEX;
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_exit;
        }

        // Insert the clock domain in the correct order based on step.
        pStepClk->clkList.clkDomains[postVoltOrderingIndex] =
             clkListItemTarget;

        // Update the numDomains.
        pStepClk->clkList.numDomains =
            LW_MAX(pStepClk->clkList.numDomains, (postVoltOrderingIndex + 1U));
    }
    BOARDOBJGRP_ITERATOR_END;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_ADC_OVERRIDE_20))
    {
        // Callwlate ADC voltage override.
        LwU8 voltRailIdx;
        for (voltRailIdx = 0; voltRailIdx < pChangeLwrr->voltList.numRails; voltRailIdx++)
        {
            LwS32 tempVoltageuV;

            // Update the volt rail mask.
            LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&(pStepClk->adcSwOverrideList.voltRailsMask.super),
                                                 voltRailIdx);

            // Update requested mode of operation to MIN(HW, SW)
            pStepClk->adcSwOverrideList.volt[voltRailIdx].overrideMode =
                LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_MIN;

            //
            // Update the voltage to be programmed.
            // In POST_VOLT step, update ADC override cap to final target voltage.
            //
            tempVoltageuV = (LwS32)pChangeLwrr->voltList.rails[voltRailIdx].voltageuV +
                pChangeLwrr->voltList.rails[voltRailIdx].voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC];

            // Done in two steps to avoid MISRA Rule 10.8 violations
            pStepClk->adcSwOverrideList.volt[voltRailIdx].voltageuV = (LwU32)tempVoltageuV;
        }
    }

perfDaemonChangeSeq35ScriptBuildStep_POST_VOLT_CLKS_exit:
    return status;
}

/*!
 * @brief Exelwtes a pre-volt clock step for a change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS
 * @public
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *pClkStep =
        (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *)pStepSuper;
    FLCN_STATUS status = FLCN_OK;

    //
    // Attach all the IMEM overlays required.
    // imem_perf
    // imem_perfDaemon
    // imem_perfDaemonChangeSeq
    // imem_clkLibClk3
    // imem_perfClkAvfs
    // imem_libClkFreqController
    // imem_libClkFreqCountedAvg
    // imem_clkLibCntr
    // imem_clkLibCntrSwSync
    // imem_libLw64
    // imem_libLw64Umul
    //

    //
    // Detach all the IMEM overlays that are NOT required.
    // imem_libVolt
    // imem_libBoardObj - Permanently attached to perfdaemon task.
    // imem_perfVf - Next low hanging fruit, only used for clkNafllNdivFromFreqCompute
    //

    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libBoardObj)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libVolt)
        OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
        OSTASK_OVL_DESC_DEFINE_LIB_FREQ_COUNTED_AVG
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = clkProgramDomainList_PerfDaemon(&pClkStep->clkList);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS_exit;
    }

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_ADC_OVERRIDE_20))
    {
        //
        // To ensure clocks follow voltage during VF switch, program ADC safe overrides
        // after programming safe clocks for voltage transition.
        //
        status = clkAdcsProgram(&pClkStep->adcSwOverrideList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS_exit;
        }
    }

    // Program ADC code correction offset pre voltage change
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION))
    {
        status = clkAdcsProgramCodeOffset(LW_TRUE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS_exit;
        }
    }

perfDaemonChangeSeq35ScriptExelwteStep_PRE_VOLT_CLKS_exit:
    return status;
}

/*!
 * @brief Exelwtes a pre-volt clock step for a change sequencer script.
 * @memberof CHANGE_SEQ_35
 * @private
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *pClkStep =
        (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLKS *)pStepSuper;
    FLCN_STATUS status = FLCN_OK;

    //
    // Attach all the IMEM overlays required.
    // imem_perf
    // imem_perfDaemon
    // imem_perfDaemonChangeSeq
    // imem_clkLibClk3
    // imem_perfClkAvfs
    // imem_libClkFreqController
    // imem_libClkFreqCountedAvg
    // imem_clkLibCntr
    // imem_clkLibCntrSwSync
    // imem_libLw64
    // imem_libLw64Umul
    //

    //
    // Detach all the IMEM overlays that are NOT required.
    // imem_libVolt
    // imem_libBoardObj - Permanently attached to perfdaemon task.
    // imem_perfVf - Next low hanging fruit, only used for clkNafllNdivFromFreqCompute
    //

    // Attach overlays
    OSTASK_OVL_DESC ovlDescList[] = {
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libBoardObj)
        OSTASK_OVL_DESC_BUILD(_IMEM, _DETACH, libVolt)
        OSTASK_OVL_DESC_DEFINE_LIB_CLK_FREQ_CONTROLLER
        OSTASK_OVL_DESC_DEFINE_LIB_FREQ_COUNTED_AVG
    };

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_ADC_OVERRIDE_20))
    {
        // Program ADC to final target overrides before programming final target clocks.
        status = clkAdcsProgram(&pClkStep->adcSwOverrideList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS_exit;
        }
    }

    // Program ADC code correction offset pre voltage change
    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_ADC_RUNTIME_CALIBRATION))
    {
        status = clkAdcsProgramCodeOffset(LW_FALSE);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS_exit;
        }
    }

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        status = clkProgramDomainList_PerfDaemon(&pClkStep->clkList);
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS_exit;
    }

perfDaemonChangeSeq35ScriptExelwteStep_POST_VOLT_CLKS_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
