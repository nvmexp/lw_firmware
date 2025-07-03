/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file clk_clockmon.c
 *
 * @brief Module managing all the clock monitors related interfaces for
 * configuring, programming, enabling & disabling clock monitors.
 */

/* ------------------------ Includes --------------------------------------- */
#include "pmu_objclk.h"
#include "dmemovl.h"
#include "pmu_objperf.h"
#include "clk/clk_clockmon.h"

/* ------------------------ Global Variables ------------------------------- */
/* ------------------------ Static Function Prototypes --------------------- */
/* ------------------------ Macros ----------------------------------------- */
//
//! Bug : http://lwbugs/1971316
// Rail Index set to default value of zero for all clock domains.
// TODO : AI on Pratikkumar Patel to derive the rail index for FMON threshold VFE evaluation
//
#define CLK_CLOCK_MON_VOLT_LIST_RAIL_IDX        0x00u

/* ------------------------ Public Functions ------------------------------- */
/*!
 * @brief Clock monitors interface to evaluate threshold VFE idx.
 *
 * @param[in]   pClkDomainList  Pointer to the clock domain list
 * @param[in]   pVoltRailList   Pointer to the voltage rail list
 * @param[out]  pStepClkMon     Pointer to the clock monitor step
 *
 * @return FLCN_OK              Clock Monitor threshold VFE idx evaluation is successful
 * @note                        Parent interface calling this code shall attach VFE overlays
 */
FLCN_STATUS
clkClockMonsThresholdEvaluate_IMPL
(
    LW2080_CTRL_CLK_CLK_DOMAIN_LIST * const                 pClkDomainList,
    LW2080_CTRL_VOLT_VOLT_RAIL_LIST * const                 pVoltRailList,
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON    *pStepClkMon
)
{
    CLK_DOMAIN   *pDomain;
    FLCN_STATUS   status   = FLCN_OK;
    LwBoardObjIdx idx;

    // Initialize list to zero
    pStepClkMon->clkMonList.numDomains = 0;

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, idx,
        &(CLK_CLK_DOMAINS_GET()->clkMonDomainsMask.super))
    {
        LW2080_CTRL_CLK_DOMAIN_CLK_MON_LIST_ITEM clkMonListItemTarget = {0};
        LW2080_CTRL_CLK_CLK_DOMAIN_LIST_ITEM    *pClkListItemLwrr  = &pClkDomainList->clkDomains[idx];
        LW2080_CTRL_VOLT_VOLT_RAIL_LIST_ITEM    *pVoltListItemLwrr = &pVoltRailList->rails[CLK_CLOCK_MON_VOLT_LIST_RAIL_IDX];
        CLK_DOMAIN_35_PROG                      *pDomain35Prog     = (CLK_DOMAIN_35_PROG *)pDomain;
        RM_PMU_PERF_VFE_VAR_VALUE                values[2];
        RM_PMU_PERF_VFE_EQU_RESULT               result;

        // Update the clock mon domain info
        clkMonListItemTarget.clkApiDomain = pDomain->apiDomain;

        // Index 0: Frequency
        values[0].frequency.varType      = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_FREQUENCY;
        values[0].frequency.varValue     = pClkListItemLwrr->clkFreqKHz;
        values[0].frequency.clkDomainIdx = BOARDOBJ_GRP_IDX_TO_8BIT(idx);

        // Index 1: Voltage
        values[1].voltage.varType  = LW2080_CTRL_PERF_VFE_VAR_TYPE_SINGLE_VOLTAGE;
        values[1].voltage.varValue = pVoltListItemLwrr->voltageuV;

        // Evaluate low threshold VFE idx if override value is not set
        if (pDomain35Prog->clkMonCtrl.lowThresholdOverride ==
            LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON_THRESHOLD_OVERRIDE_IGNORE)
        {
            // Evaluate the low threshold VFE index
            status = vfeEquEvaluate(pDomain35Prog->clkMonInfo.lowThresholdVfeIdx,
                                    values,
                                    2,
                                    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_THRESH_PERCENT,
                                    &result);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkClockMonsThresholdEvaluate_exit;
            }

            // Update the low threshold percent
            clkMonListItemTarget.lowThresholdPercent = result.threshPercent;
        }
        else
        {
            // Override the low threshold percent
            clkMonListItemTarget.lowThresholdPercent = pDomain35Prog->clkMonCtrl.lowThresholdOverride;
        }

        // Evaluate high threshold VFE idx if override value is not set
        if (pDomain35Prog->clkMonCtrl.highThresholdOverride ==
            LW2080_CTRL_CLK_CLK_DOMAIN_CONTROL_35_PROG_CLK_MON_THRESHOLD_OVERRIDE_IGNORE)
        {
            // Evaluate the high threshold VFE index
            status = vfeEquEvaluate(pDomain35Prog->clkMonInfo.highThresholdVfeIdx,
                                    values,
                                    2,
                                    LW2080_CTRL_PERF_VFE_EQU_OUTPUT_TYPE_THRESH_PERCENT,
                                    &result);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto clkClockMonsThresholdEvaluate_exit;
            }

            // Update the high threshold percent
            clkMonListItemTarget.highThresholdPercent = result.threshPercent;
        }
        else
        {
            // Override the high threshold percent
            clkMonListItemTarget.highThresholdPercent = pDomain35Prog->clkMonCtrl.highThresholdOverride;
        }

        // Insert the clock mon domain info
        pStepClkMon->clkMonList.clkDomains[pStepClkMon->clkMonList.numDomains] =
                                                            clkMonListItemTarget;

        // Increment the count
        pStepClkMon->clkMonList.numDomains++;
    }
    BOARDOBJGRP_ITERATOR_END;

clkClockMonsThresholdEvaluate_exit:
    return status;
}

/*!
 * @brief Clock monitors interface to handle pre / post change notifications.
 *
 * @param[in]   bIsPreChange    Boolean to indicate if this is a pre voltage
 *                              change notification.
 *                              If false, clock monitors would be enabled
 *                              If true, clock monitors would be disabled
 *
 * @return FLCN_OK              Clock Monitor Programming is successful
 */
FLCN_STATUS
clkClockMonsHandlePrePostPerfChangeNotification
(
    LwBool bIsPreChange
)
{
    CLK_DOMAIN   *pDomain;
    LwBoardObjIdx i;
    FLCN_STATUS   status = FLCN_OK;
    LwBool        bEnableClockMon;

    //
    // Disable all programmable clock monitors around Perf Change.
    // In other words, disable on pre change notification and re-enable
    // on post change notification.
    //
    bEnableClockMon = (!bIsPreChange);

    BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, i,
        &(CLK_CLK_DOMAINS_GET()->clkMonDomainsMask.super))
    {
        status = clkClockMonEnable_HAL(&Clk, pDomain->apiDomain, bEnableClockMon);
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            return status;
        }
    }
    BOARDOBJGRP_ITERATOR_END;

    return status;
}

/*!
 * @brief Clock monitors interface to program their updated threshold.
 *
 * @param[in]   pStepClkMon     Pointer to the clock monitor step.
 *
 * @return FLCN_OK              Clock Monitor Programming is successful
 */
FLCN_STATUS
clkClockMonsProgram
(
    LW2080_CTRL_PERF_CHANGE_SEQ_PMU_SCRIPT_STEP_CLK_MON *pStepClkMon
)
{
    LW2080_CTRL_CLK_DOMAIN_CLK_MON_LIST_ITEM *pClkDomainMonListItem;
    FLCN_STATUS status   = FLCN_OK;
    LwU8        i;

    for (i = 0; i < pStepClkMon->clkMonList.numDomains; i++)
    {
        pClkDomainMonListItem = &pStepClkMon->clkMonList.clkDomains[i];
        status = clkClockMonProgram_HAL(&Clk, pClkDomainMonListItem);
        if (status != FLCN_OK)
        {
            return status;
        }
    }

    return status;
}

/*!
 * @brief Clock monitors interface to check Fault status for all supported domains
 *
 * @return FLCN_OK              Clock Monitor Faults are clear
 */
FLCN_STATUS
clkClockMonSanityCheckFaultStatusAll
(
    void
)
{
    FLCN_STATUS status   = FLCN_OK;

    // Check Clock Monitor Primary Fault status for each domain
    status = clkClockMonCheckFaultStatus_HAL(&Clk);
    if (status != FLCN_OK)
    {
        return status;
    }

    return status;
}
