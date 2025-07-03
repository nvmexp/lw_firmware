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
 * @file    changeseq_lpwr.c
 * @copydoc changeseq_lpwr.h
 */

/* ------------------------ System Includes --------------------------------- */
#include "pmusw.h"

/* ------------------------ Application Includes ---------------------------- */
#include "pmu_objperf.h"
#include "cmdmgmt/cmdmgmt_supersurface.h"
#include "rmpmusupersurfif.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */

/*!
 * @brief Constructor of change sequencer low power.
 * @memberof CHANGE_SEQ_LPWR
 * @protected
 *
 * @param[in]  pChangeSeq   Change Sequencer pointer.
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqConstruct_LPWR
(
    CHANGE_SEQ     *pChangeSeq
)
{
    CHANGE_SEQ_LPWR    *pChangeSeqLpwr = NULL;
    FLCN_STATUS         status         = FLCN_OK;
    OSTASK_OVL_DESC     ovlDescList[]  = {
        OSTASK_OVL_DESC_BUILD(_DMEM, _ATTACH, perfChangeSeqLpwr)
    };

    OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
    {
        // Sanity check
        if (pChangeSeq == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfChangeSeqConstruct_LPWR_exit;
        }

        pChangeSeq->pChangeSeqLpwr = lwosCallocType(
            OVL_INDEX_DMEM(perfChangeSeqLpwr), 1U, CHANGE_SEQ_LPWR);
        // Sanity check.
        if (pChangeSeq->pChangeSeqLpwr == NULL)
        {
            status = FLCN_ERR_ILWALID_STATE;
            PMU_BREAKPOINT();
            goto perfChangeSeqConstruct_LPWR_exit;
        }
        pChangeSeqLpwr = pChangeSeq->pChangeSeqLpwr;

        pChangeSeqLpwr->bPerfRestoreDisable   = LW_FALSE;

        pChangeSeqLpwr->pChangeSeqLpwrScripts =
            lwosCallocType(
                OVL_INDEX_DMEM(perfChangeSeqLpwr),
                1U,
                LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPTS);
        if (pChangeSeqLpwr->pChangeSeqLpwrScripts == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto perfChangeSeqConstruct_LPWR_exit;
        }

        // Allocate perf change sequence buffer for lpwr change.
        pChangeSeqLpwr->pChangeLpwr =
            lwosCallocType(OVL_INDEX_DMEM(perfChangeSeqLpwr),
                1U,
                LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE);
        if (pChangeSeqLpwr->pChangeLpwr == NULL)
        {
            status = FLCN_ERR_NO_FREE_MEM;
            PMU_BREAKPOINT();
            goto perfChangeSeqConstruct_LPWR_exit;
        }

        // Init the script
        boardObjGrpMaskInit_E32(&(pChangeSeqLpwr->script.clkDomainsActiveMask));

        pChangeSeqLpwr->script.scriptOffsetLwrr  =
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.changeSeqLpwr.scriptLwrr);
        pChangeSeqLpwr->script.scriptOffsetLast  =
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.changeSeqLpwr.scriptLast);
        pChangeSeqLpwr->script.scriptOffsetQuery =
            RM_PMU_SUPER_SURFACE_MEMBER_OFFSET(ga10xAndLater.changeSeqLpwr.scriptQuery);

        // Init the header structure
        (void)memset(&pChangeSeqLpwr->script.hdr, 0,
               sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_HEADER_ALIGNED));

#if (!PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ_ODP))
        // Set up scratch step data pointer.
        pChangeSeqLpwr->script.pStepLwrr = &pChangeSeq->script.stepLwrr;
        memset(pChangeSeqLpwr->script.pStepLwrr, 0x00,
            sizeof(LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_DATA_ALIGNED));
#endif

perfChangeSeqConstruct_LPWR_exit:
        lwosNOP();
    }
    OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);

    return status;
}

/*!
 * @brief Interface to build change sequencer low power script.
 * @memberof CHANGE_SEQ_LPWR
 * @protected
 *
 * @param[in]  pChangeSeq   Change Sequencer pointer.
 *
 * @return FLCN_OK if the function completed successfully
 * @return Detailed status code on error
 */
FLCN_STATUS
perfChangeSeqLpwrScriptBuild
(
    CHANGE_SEQ     *pChangeSeq
)
{
    CHANGE_SEQ_LPWR                         *pChangeSeqLpwr = PERF_CHANGE_SEQ_LPWR_GET(pChangeSeq);
    LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_SCRIPT *pScript        = NULL;
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE      *pChangeLpwr    = NULL;
    LW2080_CTRL_PERF_PSTATE_CLOCK_ENTRY      pstateClkEntry;
    PSTATE     *pPstate;
    LwU32       gpcClkIdx;
    VOLT_RAIL  *pRail;
    LwU8        lwvddRailIdx;
    LwU32       vbiosBootVoltageuV;
    FLCN_STATUS status = FLCN_OK;

    // Get GPC clock index in CLK HAL.
    status = clkDomainsGetIndexByApiDomain(CLK_DOMAIN_MASK_GPC, &gpcClkIdx);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    // Get LWVDD volt rail index in VOLT HAL.
    lwvddRailIdx = voltRailVoltDomainColwertToIdx(LW2080_CTRL_VOLT_VOLT_DOMAIN_LOGIC);
    if (lwvddRailIdx == LW2080_CTRL_VOLT_VOLT_RAIL_INDEX_ILWALID)
    {
        status = FLCN_ERR_ILWALID_INDEX;
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    // Get volt rail pointer.
    pRail = VOLT_RAIL_GET(lwvddRailIdx);
    if (pRail == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    // Get boot voltage.
    status = voltRailGetVbiosBootVoltage(pRail, &vbiosBootVoltageuV);
    if ((status != FLCN_OK) ||
        (vbiosBootVoltageuV == RM_PMU_VOLT_VALUE_0V_IN_UV))
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    // Get the VBIOS boot pstate.
    pPstate = PSTATE_GET(perfPstatesBootPstateIdxGet());
    if (pPstate == NULL)
    {
        PMU_BREAKPOINT();
        status = FLCN_ERR_ILWALID_INDEX;
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    // Get the PSTATE Frequency tuple.
    status = perfPstateClkFreqGet(pPstate, gpcClkIdx, &pstateClkEntry);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    /*!----------------- Build LPWR Feature Entry Script --------------------*/

    // Init global script params.
    if (pChangeSeqLpwr->pChangeSeqLpwrScripts == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }
    pChangeSeqLpwr->pChangeSeqLpwrScripts->seqId    = LW2080_CTRL_PERF_PERF_LIMITS_ARB_SEQ_ID_INIT;
    pChangeSeqLpwr->pChangeSeqLpwrScripts->bEngaged = LW_FALSE;

    pScript = PERF_CHANGE_SEQ_LPWR_SCRIPT_GET(pChangeSeqLpwr,
                LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_ENTRY);
    if (pScript == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    // Init.
    (void)memset(pScript, 0, sizeof(*pScript));

    // Set the script id and seq id.
    pScript->hdr.scriptId = LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_ENTRY;
    pScript->seqId        = LW2080_CTRL_PERF_PERF_LIMITS_ARB_SEQ_ID_INIT;

    // Step 0 - De-init clocks.
    pScript->steps[pScript->hdr.numSteps].stepId            =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_DEINIT;
    pScript->steps[pScript->hdr.numSteps].data.super.stepId =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_DEINIT;

    //
    // De-init GPC clock.
    // @note SW is requesting boot frequency in FFR regime but ideally NAFLL
    // clocks programming is not required as GPC clock will be running on
    // BYPASS path and NAFLL will be powered down with voltage rail gating.
    //
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].clkDomain   = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].clkFreqKHz  = pstateClkEntry.nom.freqkHz;
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].regimeId    = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].source      = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;

    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.numDomains  = gpcClkIdx + 1U;

    //
    // No ADC MUX override programming required in this step as we will
    // disable LUT propagation by settings NAFLL_LTCLUT_CFG_RAM_READ_EN = NO
    //

    pScript->hdr.numSteps++;

    // Step 1 - Intermediate LPWR notification for updating LPWR task specific sequence.
    pScript->steps[pScript->hdr.numSteps].stepId             =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_PRE_VOLT_GATE_LPWR;
    pScript->steps[pScript->hdr.numSteps].data.super.stepId  =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_PRE_VOLT_GATE_LPWR;

    // Pstate index will not be used. Explicitely set it to INVALID.
    pScript->steps[pScript->hdr.numSteps].data.lpwr.pstateIndex =
        LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;

    pScript->hdr.numSteps++;

    // Step 2 - Voltage rail gating.
    pScript->steps[pScript->hdr.numSteps].stepId             =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_GATE;
    pScript->steps[pScript->hdr.numSteps].data.super.stepId  =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_GATE;

    // Rail gate LWVDD voltage rail.
    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.
        rails[lwvddRailIdx].railIdx                  = lwvddRailIdx;
    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.
        rails[lwvddRailIdx].railAction               = LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_GATE;
    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.
        rails[lwvddRailIdx].voltageuV                = RM_PMU_VOLT_VALUE_0V_IN_UV;
    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.
        rails[lwvddRailIdx].voltageMinNoiseUnawareuV = RM_PMU_VOLT_VALUE_0V_IN_UV;

    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.numRails = lwvddRailIdx + 1U;

    //
    // No ADC MUX override programming required in this step as we are
    // completely powering down the rail.
    //

    pScript->hdr.numSteps++;

    /*!----------------- Build LPWR Feature Exit Script --------------------*/

    pScript = PERF_CHANGE_SEQ_LPWR_SCRIPT_GET(pChangeSeqLpwr,
                LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_EXIT);
    if (pScript == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    // Init.
    (void)memset(pScript, 0, sizeof(*pScript));

    // Set the script id and seq id.
    pScript->hdr.scriptId = LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_EXIT;
    pScript->seqId        = LW2080_CTRL_PERF_PERF_LIMITS_ARB_SEQ_ID_INIT;

    // Step 0 - PRE LPWR notification for updating LPWR task specific sequence.
    pScript->steps[pScript->hdr.numSteps].stepId             =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_PRE_VOLT_UNGATE_LPWR;
    pScript->steps[pScript->hdr.numSteps].data.super.stepId  =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_PRE_VOLT_UNGATE_LPWR;

    // Pstate index will not be used. Explicitely set it to INVALID.
    pScript->steps[pScript->hdr.numSteps].data.lpwr.pstateIndex =
        LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;

    pScript->hdr.numSteps++;

    // Step 1 - Voltage rail ungating.
    pScript->steps[pScript->hdr.numSteps].stepId             =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_UNGATE;
    pScript->steps[pScript->hdr.numSteps].data.super.stepId  =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_VOLT_UNGATE;

    // Rail ungate LWVDD voltage rail.
    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.
        rails[lwvddRailIdx].railIdx                  = lwvddRailIdx;
    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.
        rails[lwvddRailIdx].railAction               = LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_UNGATE;
    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.
        rails[lwvddRailIdx].voltageuV                = vbiosBootVoltageuV;
    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.
        rails[lwvddRailIdx].voltageMinNoiseUnawareuV = vbiosBootVoltageuV;

    pScript->steps[pScript->hdr.numSteps].data.volt.voltList.numRails = lwvddRailIdx + 1U;

    //
    // No ADC MUX override programming required in this step as we are
    // operating NAFLL devices on FFR regime.
    //

    pScript->hdr.numSteps++;

    // Step 2 - Intermediate LPWR notification for updating LPWR task specific sequence.
    pScript->steps[pScript->hdr.numSteps].stepId             =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_POST_VOLT_UNGATE_LPWR;
    pScript->steps[pScript->hdr.numSteps].data.super.stepId  =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_POST_VOLT_UNGATE_LPWR;

    // Pstate index will not be used. Explicitely set it to INVALID.
    pScript->steps[pScript->hdr.numSteps].data.lpwr.pstateIndex =
        LW2080_CTRL_PERF_PSTATE_INDEX_ILWALID;

    pScript->hdr.numSteps++;

    // Step 3 - NAFLL clocks init.
    pScript->steps[pScript->hdr.numSteps].stepId            =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_INIT;
    pScript->steps[pScript->hdr.numSteps].data.super.stepId =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_INIT;

    // Init GPC NAFLL device to boot frequency in FFR regime.
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].clkDomain   = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].clkFreqKHz  = pstateClkEntry.nom.freqkHz;
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].regimeId    = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].source      = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;

    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.numDomains  = gpcClkIdx + 1U;

    //
    // No ADC MUX override programming required in this step as we will
    // disable LUT propagation by settings NAFLL_LTCLUT_CFG_RAM_READ_EN = NO
    //

    pScript->hdr.numSteps++;

    /*!-------------- Build LPWR Feature Post Processing Script -------------*/

    pScript = PERF_CHANGE_SEQ_LPWR_SCRIPT_GET(pChangeSeqLpwr,
                LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_POST);
    if (pScript == NULL)
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    // Init.
    (void)memset(pScript, 0, sizeof(*pScript));

    // Set the script id and seq id.
    pScript->hdr.scriptId = LW2080_CTRL_PERF_CHANGE_SEQ_SCRIPT_ID_LPWR_POST;
    pScript->seqId        = LW2080_CTRL_PERF_PERF_LIMITS_ARB_SEQ_ID_INIT;

    // Step 0 - Init clocks.
    pScript->steps[pScript->hdr.numSteps].stepId            =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_RESTORE;
    pScript->steps[pScript->hdr.numSteps].data.super.stepId =
        LW2080_CTRL_PERF_CHANGE_SEQ_LPWR_STEP_ID_CLK_RESTORE;

    // Init GPC clocks and all its dependencies.
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].clkDomain   = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].clkFreqKHz  = pstateClkEntry.nom.freqkHz;
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].regimeId    = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.
        clkDomains[gpcClkIdx].source      = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;

    pScript->steps[pScript->hdr.numSteps].data.clk.clkList.numDomains  = gpcClkIdx + 1U;

    // ADC MUX override will be programmed to MIN with boot voltage.
    pScript->steps[pScript->hdr.numSteps].data.clk.adcSwOverrideList.
        volt[lwvddRailIdx].overrideMode = LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_MIN;
    pScript->steps[pScript->hdr.numSteps].data.clk.adcSwOverrideList.
        volt[lwvddRailIdx].voltageuV    = vbiosBootVoltageuV;

    // Update the volt rail mask.
    LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(
        &(pScript->steps[pScript->hdr.numSteps].data.clk.adcSwOverrideList.voltRailsMask.super),
        lwvddRailIdx);

    pScript->hdr.numSteps++;

    // Build low power change struct for perf restore
    pChangeLpwr = pChangeSeqLpwr->pChangeLpwr;
    if (pChangeLpwr == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto perfChangeSeqLpwrScriptBuild_exit;
    }

    // Build the internal change struct from input change struct.
    pChangeLpwr->version               = pChangeSeq->version;
    pChangeLpwr->pstateIndex           = perfPstatesBootPstateIdxGet();
    pChangeLpwr->flags                 = LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE_NONE;
    pChangeLpwr->vfPointsCacheCounter  = LW2080_CTRL_CLK_CLK_VF_POINT_CACHE_COUNTER_TOOLS;

    // Populate clock domain information
    pChangeLpwr->clkList.numDomains = gpcClkIdx + 1U;
    pChangeLpwr->clkList.clkDomains[gpcClkIdx].clkDomain  = LW2080_CTRL_CLK_DOMAIN_GPCCLK;
    pChangeLpwr->clkList.clkDomains[gpcClkIdx].clkFreqKHz = pstateClkEntry.nom.freqkHz;
    pChangeLpwr->clkList.clkDomains[gpcClkIdx].regimeId   = LW2080_CTRL_CLK_NAFLL_REGIME_ID_FFR;
    pChangeLpwr->clkList.clkDomains[gpcClkIdx].source     = LW2080_CTRL_CLK_PROG_1X_SOURCE_NAFLL;

    // Populate volt rail information
    pChangeLpwr->voltList.numRails  = lwvddRailIdx + 1U;
    pChangeLpwr->voltList.rails[lwvddRailIdx].railIdx                  = lwvddRailIdx;
    pChangeLpwr->voltList.rails[lwvddRailIdx].railAction               = LW2080_CTRL_VOLT_VOLT_RAIL_ACTION_VF_SWITCH;
    pChangeLpwr->voltList.rails[lwvddRailIdx].voltageuV                = vbiosBootVoltageuV;
    pChangeLpwr->voltList.rails[lwvddRailIdx].voltageMinNoiseUnawareuV = vbiosBootVoltageuV;

    // Build active mask for the low power perf restore script.
    boardObjGrpMaskBitSet(&(pChangeSeqLpwr->script.clkDomainsActiveMask), gpcClkIdx);

perfChangeSeqLpwrScriptBuild_exit:
    return status;
}
