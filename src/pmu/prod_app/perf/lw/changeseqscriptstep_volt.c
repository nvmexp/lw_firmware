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
 * @file    changeseqscriptstep_volt.h
 * @brief   Implementation for handling VOLT-based steps by the change sequencer.
 */

/* ------------------------ System Includes --------------------------------- */
/* ------------------------ Application Includes ---------------------------- */
#include "lib/lib_avg.h"
#include "boardobj/boardobjgrp.h"
#include "pmu_objclk.h"
#include "perf/changeseqscriptstep_volt.h"
#include "volt/objvolt.h"

/* ------------------------ Static Function Prototypes ---------------------- */
/* ------------------------ Macros and Defines ------------------------------ */
/* ------------------------ Function Prototypes ----------------------------- */
/* ------------------------ Types Definitions ------------------------------- */
/* ------------------------ Global Variables -------------------------------- */
/* ------------------------ Public Functions -------------------------------- */
/*!
 * @brief Builds a voltage-based step for a change sequencer script.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT
 * @public
 * @copydetails PerfDaemonChangeSeqScriptBuildStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptBuildStep_VOLT_IMPL
(
    CHANGE_SEQ                         *pChangeSeq,
    CHANGE_SEQ_SCRIPT                  *pScript,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLwrr,
    LW2080_CTRL_PERF_CHANGE_SEQ_CHANGE *pChangeLast
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT *pStepVolt =
        &pScript->pStepLwrr->data.volt;

    pStepVolt->voltList = pChangeLwrr->voltList;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_ADC_OVERRIDE_20))
    {
        // Callwlate ADC voltage override.
        LwU8 voltRailIdx;
        for (voltRailIdx = 0; voltRailIdx < pChangeLwrr->voltList.numRails; voltRailIdx++)
        {
            LwS32 tempVoltageuV;

            // Update the volt rail mask.
            LW2080_CTRL_BOARDOBJGRP_MASK_BIT_SET(&(pStepVolt->adcSwOverrideList.voltRailsMask.super),
                                                 voltRailIdx);

            // Update requested mode of operation to SW override mode.
            pStepVolt->adcSwOverrideList.volt[voltRailIdx].overrideMode =
                LW2080_CTRL_CLK_ADC_SW_OVERRIDE_ADC_USE_SW_REQ;

            //
            // Update the voltage to be programmed.
            // In POST_VOLT step, update ADC override cap to final target voltage.
            //
            tempVoltageuV = (LwS32)pChangeLwrr->voltList.rails[voltRailIdx].voltageuV +
                pChangeLwrr->voltList.rails[voltRailIdx].voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC];

            // Done in two steps to avoid MISRA Rule 10.8 violations
            pStepVolt->adcSwOverrideList.volt[voltRailIdx].voltageuV = (LwU32)tempVoltageuV;
        }
    }

    return FLCN_OK;
}

/*!
 * @brief Exelwtes a voltage step to set the requested voltage values.
 * @memberof LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT
 * @public
 * @copydetails PerfDaemonChangeSeqScriptExelwteStep()
 */
FLCN_STATUS
perfDaemonChangeSeqScriptExelwteStep_VOLT_IMPL
(
    CHANGE_SEQ                                         *pChangeSeq,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_SUPER  *pStepSuper
)
{
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT    *pStepVolt =
        (LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_VOLT *)pStepSuper;
    FLCN_STATUS status;

    if (PMUCFG_FEATURE_ENABLED(PMU_PERF_NAFLL_ADC_OVERRIDE_20))
    {
        // Program ADC to final target voltage overrides in SW override mode.
        status = clkAdcsProgram(&pStepVolt->adcSwOverrideList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqScriptExelwteStep_VOLT_exit;
        }
    }

    status = voltVoltSetVoltage(LW2080_CTRL_VOLT_VOLT_POLICY_CLIENT_PERF_CORE_VF_SEQ,
                                    &(pStepVolt->voltList));
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto perfDaemonChangeSeqScriptExelwteStep_VOLT_exit;
    }

    // Update average voltage
    if (PMUCFG_FEATURE_ENABLED(PMU_LIB_AVG))
    {
        LwU8            voltRailIdx;
        OSTASK_OVL_DESC ovlDescList[] = {
            OSTASK_OVL_DESC_DEFINE_LIB_AVG
        };

        OSTASK_ATTACH_OR_DETACH_OVERLAYS_ENTER(ovlDescList);
        {
            for (voltRailIdx = 0; voltRailIdx < pStepVolt->voltList.numRails; voltRailIdx++)
            {
                LwS32 tempVoltageuV =
                    ((LwS32)pStepVolt->voltList.rails[voltRailIdx].voltageuV +
                        pStepVolt->voltList.rails[voltRailIdx].voltOffsetuV[LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC]);

                status = swCounterAvgUpdate(COUNTER_AVG_CLIENT_ID_VOLT,
                    pStepVolt->voltList.rails[voltRailIdx].railIdx,
                    (LwU32)tempVoltageuV);
                if (status != FLCN_OK)
                {
                    break;
                }
            }
        }
        OSTASK_ATTACH_OR_DETACH_OVERLAYS_EXIT(ovlDescList);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto perfDaemonChangeSeqScriptExelwteStep_VOLT_exit;
        }
    }

perfDaemonChangeSeqScriptExelwteStep_VOLT_exit:
    return status;
}

/* ------------------------ Private Functions ------------------------------- */
