/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

/*!
 * @file   pwrpolicy_workload_multirail_iface.c
 * @brief  Interface specification for a PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE.
 *
 */
/* ------------------------- System Includes -------------------------------- */
#include "pmusw.h"

/* ------------------------- Application Includes --------------------------- */
#include "pmu_objhal.h"
#include "pmu_objperf.h"
#include "pmu_objpmgr.h"
#include "pmu_objpg.h"
#include "dbgprintf.h"

/* ------------------------- Type Definitions ------------------------------- */
/* ------------------------- External Definitions --------------------------- */
/* ------------------------- Static Variables ------------------------------- */
/* ------------------------- Global Variables ------------------------------- */
/* ------------------------- Macros and Defines ----------------------------- */
/* ------------------------- Private Functions ------------------------------ */
/* ------------------------- Public Functions ------------------------------- */
/*!
 * Construct a _WORKLOAD_MULTIRAIL_INTERFACE PWR_POLICY object.
 *
 * @copydoc BoardObjInterfaceConstruct
 */
FLCN_STATUS
pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE
(
    BOARDOBJGRP                *pBObjGrp,
    BOARDOBJ_INTERFACE         *pInterface,
    RM_PMU_BOARDOBJ_INTERFACE  *pInterfaceDesc,
    INTERFACE_VIRTUAL_TABLE    *pVirtualTable
)
{
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE_BOARDOBJ_SET *pWorkloadSet =
        (RM_PMU_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE_BOARDOBJ_SET *)pInterfaceDesc;
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE  *pWorkload;
    FLCN_STATUS  status;
    LwU8         railIdx;

    // Construct and initialize parent-class object.
    status = boardObjInterfaceConstruct(pBObjGrp, pInterface, pInterfaceDesc, pVirtualTable);
    if (status != FLCN_OK)
    {
        PMU_BREAKPOINT();
        goto pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE_exit;
    }
    pWorkload = (PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *)pInterface;

    // Copy in the _WORKLOAD type-specific data.
    pWorkload->bMscgResidencyEnabled = pWorkloadSet->bMscgResidencyEnabled;
    pWorkload->bPgResidencyEnabled   = pWorkloadSet->bPgResidencyEnabled;
    pWorkload->bSensedVoltage        = pWorkloadSet->sensedVoltage.bEnabled;
    pWorkload->nonGpcClkDomainMask   = pWorkloadSet->nonGpcClkDomainMask;

    // If PG residency is enabled allocate memory for the filter
    pWorkload->pgResFilter.idx = 0;
    pWorkload->pgResFilter.sizeLwrr = 0;
    if (pWorkload->bPgResidencyEnabled)
    {
        if (pWorkloadSet->pgLookBackWindowSize == 0)
        {
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE_exit;
        }
        if (pWorkload->pgResFilter.pEntries == NULL)
        {
            pWorkload->pgResFilter.bufferSize = pWorkloadSet->pgLookBackWindowSize;
            pWorkload->pgResFilter.pEntries = lwosCallocResidentType(
                pWorkload->pgResFilter.bufferSize, LwUFXP20_12);
            if (pWorkload->pgResFilter.pEntries == NULL)
            {
                status = FLCN_ERR_NO_FREE_MEM;
                PMU_BREAKPOINT();
                goto pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE_exit;
            }
        }
        else if (pWorkloadSet->pgLookBackWindowSize != pWorkload->pgResFilter.bufferSize)
        {
            // Cannot change buffer of already constructed data.
            status = FLCN_ERR_ILWALID_ARGUMENT;
            PMU_BREAKPOINT();
            goto pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE_exit;
        }
    }
    else if (pWorkload->pgResFilter.bufferSize != 0)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE_exit;
    }

    // Copy in per rail specific parameters.
    FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkload, railIdx)
    {
        pWorkload->rail[railIdx].rail.chIdx       =
            pWorkloadSet->rail[railIdx].chIdx;
        pWorkload->rail[railIdx].rail.voltRailIdx =
            pWorkloadSet->rail[railIdx].voltRailIdx;
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

    // If sensed voltage is enabled, allocate memory for CLK_ADC_ACC_SAMPLEs
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE) &&
        pWorkload->bSensedVoltage)
    {
        FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkload, railIdx)
        {
            VOLT_RAIL_SENSED_VOLTAGE_DATA *pSensed =
                PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IFACE_SENSED_GET(pWorkload, railIdx);

            // Allocate memory for the CLK_ADC_ACC_SAMPLEs only if not allocated
            if (pSensed->pClkAdcAccSample == NULL)
            {
                status = voltRailSensedVoltageDataConstruct(pSensed,
                    VOLT_RAIL_GET(pWorkload->rail[railIdx].rail.voltRailIdx),
                    pWorkloadSet->sensedVoltage.mode, pBObjGrp->ovlIdxDmem);
                if (status != FLCN_OK)
                {
                    PMU_BREAKPOINT();
                    goto pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE_exit;
                }
            }
        }
        FOR_EACH_VALID_VOLT_RAIL_IFACE_END
    }

pwrPolicyConstructImpl_WORKLOAD_MULTIRAIL_INTERFACE_exit:
    return status;
}

/*!
 * WORKLOAD_MULTIRAIL_INTERFACE specific load.
 *
 * @param[in] pWorkIface   PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pointer.
 *
 * @return FLCN_OK
 *     Load exelwted successfully.
 * @return Other errors
 *     Unexpected errors propagated from other functions.
 */
FLCN_STATUS
pwrPolicyLoad_WORKLOAD_MULTIRAIL_INTERFACE
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface
)
{
    LwU8        railIdx;
    FLCN_STATUS status = FLCN_OK;

    //
    // If sensed voltage feature is enabled, we need to discard the first
    // sensed voltage read to ensure that the adcAclwmulatorVal and
    // adcNumSamplesVal are valid for subsequent reads.
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE) &&
        pWorkIface->bSensedVoltage)
    {
        FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkIface, railIdx)
        {
            VOLT_RAIL *pRail = VOLT_RAIL_GET(pWorkIface->rail[railIdx].rail.voltRailIdx);
            VOLT_RAIL_SENSED_VOLTAGE_DATA *pSensed =
                PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IFACE_SENSED_GET(pWorkIface, railIdx);

            if (pRail == NULL)
            {
                PMU_BREAKPOINT();
                status = FLCN_ERR_ILWALID_INDEX;
                goto pwrPolicyLoad_WORKLOAD_MULTIRAIL_INTERFACE_exit;
            }

            status = voltRailGetVoltageSensed(pRail, pSensed);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrPolicyLoad_WORKLOAD_MULTIRAIL_INTERFACE_exit;
            }
        }
        FOR_EACH_VALID_VOLT_RAIL_IFACE_END
    }

pwrPolicyLoad_WORKLOAD_MULTIRAIL_INTERFACE_exit:
    return status;
}

/*!
 * Computes the current workload/active capacitance term (w) given the observed
 * power and GPU state.
 *
 * @param[in] pWorkIface   PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pointer.
 * @param[in] pGetStatus   RM_PMU_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE_BOARDOBJ_GET_STATUS pointer
 *
 * @return FLCN_OK
 */
FLCN_STATUS
pwrPolicyGetStatus_WORKLOAD_MULTIRAIL_INTERFACE
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE                                  *pWorkIface,
    RM_PMU_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE_BOARDOBJ_GET_STATUS  *pGetStatus
)
{
    LwU8                            railIdx;
    FLCN_STATUS                     status = FLCN_OK;

    pGetStatus->work = pWorkIface->work;
    pGetStatus->freq = pWorkIface->freq;

    FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkIface, railIdx)
    {
        pGetStatus->workload[railIdx] = pWorkIface->rail[railIdx].workload;
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

    return status;
}

/*!
 * Computes the current workload/active capacitance term (w) given the observed
 * power and GPU state.
 *
 * @param[in] pWorkload   PWR_POLICY_WORKLOAD_MULTIRAIL pointer.
 * @param[in] pInput
 *     RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_MULTIRAIL_WORK_INPUT
 *     pointer to containing the current GPU state.
 * @param[in] railIdx     Rail index.
 *
 * @return Workoad/active capacitance term (w) in unsigned FXP 20.12 format,
 *     units mW / Mhz / mV^2 or mA / MHz / mV
 */
LwUFXP20_12
pwrPolicyWorkloadMultiRailComputeWorkload
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE  *pWorkload,
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_WORK_INPUT *pInput,
    LwU8                                      railIdx,
    LwU8                                      limitUnit
)
{
    LwUFXP20_12           workload = LW_TYPES_FXP_ZERO;
    LwU8                  dynamicEquIdx = LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID;

    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION_DYNAMIC))
    {
        VOLT_RAIL *pRail;

        // Fetch the dynamic equation index
        pRail = VOLT_RAIL_GET(pWorkload->rail[railIdx].rail.voltRailIdx);
        if (pRail == NULL)
        {
            PMU_BREAKPOINT();
            goto pwrPolicyWorkloadMultiRailComputeWorkload_exit;
        }
        dynamicEquIdx = voltRailVoltScaleExpPwrEquIdxGet(pRail);
    }

    //
    // Assume 0 workload for the following tricky numerical cases (i.e. avoiding
    // negative numbers or dividing by zero):
    // 1. Power is less than leakage callwlation.
    // 2. Measured frequency is zero.
    // 3. Measured voltage is zero.
    //
    if ((pWorkload->rail[railIdx].valueLwrr < pInput->rail[railIdx].leakagemX) ||
        (pInput->freqMHz == 0) ||
        (pInput->rail[railIdx].voltmV == 0))
    {
        goto pwrPolicyWorkloadMultiRailComputeWorkload_exit;
    }

    pInput->rail[railIdx].observedVal = pWorkload->rail[railIdx].valueLwrr;

    //
    // This case is for new SW model which has introduced the concept of
    // DYNAMIC equations
    //
    if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION_DYNAMIC) &&
        (dynamicEquIdx != LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID))
    {
        LwU64                 factor = 1000;
        LwUFXP40_24           workloadFXP40_24;
        LwUFXP52_12           denomFXP52_12;
        LwUFXP52_12           quotientFXP52_12;
        PWR_EQUATION_DYNAMIC *pDyn =
            (PWR_EQUATION_DYNAMIC *)PWR_EQUATION_GET(dynamicEquIdx);

        if (pDyn == NULL)
        {
            PMU_BREAKPOINT();
            goto pwrPolicyWorkloadMultiRailComputeWorkload_exit;
        }

        workloadFXP40_24 = LW_TYPES_U64_TO_UFXP_X_Y_SCALED(40, 24,
            (pWorkload->rail[railIdx].valueLwrr - pInput->rail[railIdx].leakagemX),
            pInput->freqMHz);

        if (limitUnit==
            LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
        {
            //
            //     w   = ((Ptotal - Pleakage) / f  ) / V^(dynamicExp)
            // Units:
            //         = (         mW         / MHz) / V^(dynamicExp)
            //         = (         mW         / MHz) / mV^(dynamicExp) * 1000
            // Numerical Analysis:
            //   52.12 = (        40.24            ) / 52.12
            //
            // Integer overflow:
            // Assuming Vmin = 600mV and dynamicExp = 2.4, for 20 integer bits
            // to overflow, the numerator in above callwlation should be greater
            // than 0.6^2.4 * 1000 * 2^20 = 307725000. This value of numerator
            // is not expected but is not impossible, so we should detect for
            // integer overflow.
            //
            // Fractional underflow:
            // Assuming Vmax = 1.2V and dynamicExp = 2.4, for 12 fractional bits
            // to underflow, numerator has to be less than 1.2^2.4 * 1000 * 2^-12
            // = 0.378. This means the numerator has to be 1 for underflow.
            // We can assume workload to be zero for such low dynamic power.
            //
            denomFXP52_12 = (LwUFXP52_12)(pwrEquationDynamicScaleVoltagePower(pDyn,
                LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
                    pInput->rail[railIdx].voltmV, 1000)));
            lwU64Mul(&denomFXP52_12, &denomFXP52_12, &factor);
        }
        else
        {
            //
            //     w   = ((Itotal - Ileakage) / f  ) / V^(dynamicExp)
            // Units:
            //         = (         mA         / MHz) / V^(dynamicExp)
            //         = (         mA         / MHz) / mV^(dynamicExp) * 1000
            // Numerical Analysis:
            //   52.12 = (        40.24            ) / 52.12
            //
            // Overflow callwlation similar to power case above
            //
            denomFXP52_12 = (LwUFXP52_12)(pwrEquationDynamicScaleVoltageLwrrent(pDyn,
                LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
                    pInput->rail[railIdx].voltmV, 1000)));
            lwU64Mul(&denomFXP52_12, &denomFXP52_12, &factor);
        }

        // Check for division by zero and return 0 workload as this is undefined.
        if (denomFXP52_12 == LW_TYPES_FXP_ZERO)
        {
            goto pwrPolicyWorkloadMultiRailComputeWorkload_exit;
        }
        lwU64DivRounded(&quotientFXP52_12, &workloadFXP40_24, &denomFXP52_12);
        if (LwU64_HI32(quotientFXP52_12) != LW_TYPES_FXP_ZERO)
        {
            // Overflow detected, cap workload to LW_U32_MAX
            workload = LW_U32_MAX;
        }
        else
        {
            workload = LwU64_LO32(quotientFXP52_12);
        }
    }
    else
    {
        //
        // POWER:
        // Ptotal = Pdynamic + Pleakage
        //        = V^2 * f * w + Pleakage
        //
        // w = (Ptotal - Pleakage) / (V^2 * f)
        //
        //     mW
        // -----------
        //   mV^2 MHz
        //
        //
        // CURRENT:
        // Itotal = Idynamic + Ileakage
        //        = V * f * w + Ileakage
        //
        // w = (Itotal - Ileakage) / (V * f)
        //
        // Numerical Analysis:
        //
        //     32.0 << 12    => 20.12
        //  / 32.0           => 32.0
        //  / 32.0           => 32.0
        // ----------------------------
        //                      20.12
        //
        //
        // Will overflow when power/current difference is > 1048575 mW/mA, this
        // should a safe assumption.
        //
        // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
        // because this code segment is tied to the
        // @ref PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD feature which is not
        // going to be used on AD10X and later chips.
        //
        // TODO-Chandrashis: Add the @ref PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD
        // feature to the CONFLICTS list for @ref PMU_PMGR_32BIT_OVERFLOW_FIXES
        // feature once the @ref PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD feature
        // is not enabled on AD10X and later chips any more. 
        //
        //
        workload = LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
            (pWorkload->rail[railIdx].valueLwrr - pInput->rail[railIdx].leakagemX),
            pInput->freqMHz);

        if (limitUnit ==
            LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
        {
            workload = LW_UNSIGNED_ROUNDED_DIV(workload,
                LW_UNSIGNED_ROUNDED_DIV(
                (pInput->rail[railIdx].voltmV * pInput->rail[railIdx].voltmV),
                    1000));
        }
        else
        {
            workload = LW_UNSIGNED_ROUNDED_DIV(workload,
                pInput->rail[railIdx].voltmV);
        }
    }

pwrPolicyWorkloadMultiRailComputeWorkload_exit:
    return workload;
}

/*!
 * Retrieves the voltage floor limit for the given voltage rail based on the
 * lastest perf status received from RM.
 *
 * @param[in]     pWorkload   PWR_POLICY_WORKLOAD_MULTIRAIL pointer.
 * @param[in]     mclkkHz     memory clock in kHz.
 *
 * @note:
 * If mclkkHz is zero, latest perf status received from RM is used
 *     to retrieve the voltage floor limit.
 * Else mclkkHz value is passed as input to the arbiter to retrieve the
 *     voltage floor limit.
 */
FLCN_STATUS
pwrPolicyWorkloadMultiRailGetVoltageFloor
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE  *pWorkload,
    LwU32                                     mclkkHz
)
{
    VOLT_RAIL                  *pRail;
    CLK_DOMAIN                 *pDomain = NULL;
    CLK_DOMAIN_PROG            *pDomainProg;
    LW2080_CTRL_CLK_VF_INPUT    clkArbInput;
    LW2080_CTRL_CLK_VF_OUTPUT   clkArbOutput;
    BOARDOBJGRPMASK_E32         bMask;
    LwBoardObjIdx               clkIdx;
    LwU8                        voltRailIdx;
    LwU8                        railIdx;
    FLCN_STATUS                 status = FLCN_OK;

    LW2080_CTRL_CLK_VF_INPUT_INIT(&clkArbInput);
    LW2080_CTRL_CLK_VF_OUTPUT_INIT(&clkArbOutput);

    //
    // Always make sure there is a match even if the input Freq is outside the range.
    // NOTE: VF points and Perf.Status are updated in Perf Task and CWC is accessing
    // them asynchronously. This can lead to race condition between CWC and Perf data.
    // clkVfPointsWriteSemaphoreGive semaphore is used to synchronize the access.
    //
    clkArbInput.flags = FLD_SET_DRF(2080_CTRL_CLK, _VF_INPUT_FLAGS, _VF_POINT_SET_DEFAULT,
                                    _YES, clkArbInput.flags);

    // For each rail callwlate the voltage floor
    FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkload, railIdx)
    {

        LwU32       railVminuV;

        //
        // Directly query the VOLTAGE_RAIL Vmin, which is separate
        // from any CLK_DOMAIN Vmin (i.e. a loose VOLTAGE min which
        // won't re-target the frequencies)
        //
        pRail  = VOLT_RAIL_GET(pWorkload->rail[railIdx].rail.voltRailIdx);
        if (pRail == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrPolicyWorkloadMultiRailGetVoltageFloor_exit;
        }

        // Store the MASK in boardObj format
        boardObjGrpMaskInit_E32(&bMask);
        bMask.super.pData[0] = pWorkload->nonGpcClkDomainMask;
        //
        // Intersect the mask with the set of CLK_DOMAINs which have a
        // VMIN on the given RAIL.
        //
        // @note, '.imem_libBoardObj' is only attached for
        // PWR_POLICY_35 evaluation paths, so limiting this code to
        // that feature.  On all pre-35 implementations, the Vmin mask
        // will be either unsuported or all CLK_DOMAINS.  So, this
        // step is a no-op and skipping it is fine.
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_35) &&
            (voltRailGetClkDomainsProgMask(pRail) != NULL))
        {
            PMU_HALT_OK_OR_GOTO(status,
                boardObjGrpMaskAnd(&bMask, &bMask, voltRailGetClkDomainsProgMask(pRail)),
                pwrPolicyWorkloadMultiRailGetVoltageFloor_exit);
        }

        // Initialize the voltFlooruV to minimum.
        pWorkload->freq.rail[railIdx].voltFlooruV = LW_U32_MIN;

        BOARDOBJGRP_ITERATOR_BEGIN(CLK_DOMAIN, pDomain, clkIdx, &bMask.super)
        {
            pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
            if (pDomainProg == NULL)
            {
                status = FLCN_ERR_ILWALID_STATE;
                PMU_BREAKPOINT();
                goto pwrPolicyWorkloadMultiRailGetVoltageFloor_exit;
            }

            // Callwlate the voltage required to support given freq
            if (PMUCFG_FEATURE_ENABLED(PMU_PERF_CHANGE_SEQ))
            {
                if ((pDomain->domain == clkWhich_MClk) &&
                    (mclkkHz != 0))
                {
                    clkArbInput.value = mclkkHz;
                }
                else
                {
                    clkArbInput.value = perfChangeSeqChangeLastClkFreqkHzGet(clkIdx);
                }
            }
            else
            {
                clkArbInput.value = perfStatusGetClkFreqkHz(clkIdx);
            }

            if (pDomain->domain != clkWhich_PCIEGenClk)
            {
                clkArbInput.value = (clkArbInput.value / 1000);
            }

            voltRailIdx = pWorkload->rail[railIdx].rail.voltRailIdx;

            status = clkDomainProgFreqToVolt(
                        pDomainProg,
                        voltRailIdx,
                        LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                        &clkArbInput,
                        &clkArbOutput,
                        NULL);
            if ((status != FLCN_OK) ||
                (clkArbOutput.inputBestMatch ==
                    LW2080_CTRL_CLK_VF_VALUE_ILWALID))
            {
                PMU_BREAKPOINT();
                goto pwrPolicyWorkloadMultiRailGetVoltageFloor_exit;
            }

            // Store the floor value in voltageFlooruV
            pWorkload->freq.rail[railIdx].voltFlooruV =
                LW_MAX(pWorkload->freq.rail[railIdx].voltFlooruV,
                        clkArbOutput.value);
        }
        BOARDOBJGRP_ITERATOR_END;

        status = voltRailGetVoltageMin(pRail, &railVminuV);
        //
        // FLCN_ERR_NOT_SUPPORTED is an expected error - i.e. no
        // VOLTAGE_RAIL Vmin specified for this GPU.
        //
        if (FLCN_ERR_NOT_SUPPORTED == status)
        {
            status = FLCN_OK;
        }
        // Unexpected errors
        else if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrPolicyWorkloadMultiRailGetVoltageFloor_exit;
        }
        // Include the VOLTAGE_RAIL Vmin in the voltage floor.
        else
        {
            pWorkload->freq.rail[railIdx].voltFlooruV =
                LW_MAX(pWorkload->freq.rail[railIdx].voltFlooruV, railVminuV);
        }
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

pwrPolicyWorkloadMultiRailGetVoltageFloor_exit:
    return status;
}

/*!
 * Evaluate the total power (sum of power required per rail) at the given frequency
 * based on the power equation:
 *
 *      Given frequency f:
 *          P_TotalPwr = P_Rail0 + P_Rail1
 *      where,
 *          P_Rail0 = v_Rail0 ^ 2 * f * w_Rail0 + P_Leakage_Rail0
 *          P_Rail1 = v_Rail1 ^ 2 * f * w_Rail1 + P_Leakage_Rail1
 *
 *      Here,
 *      v_Rail0 = voltage value needed at rail0 to support f
 *      v_Rail1 = voltage value needed at rail1 to support f
 *
 *
 * @param[in]   pWorkload     PWR_POLICY_WORKLOAD_MULTIRAIL pointer.
 * @param[in]   pDomain       CLK_DOMAIN *pointer.
 * @param[in]   freqMHz       Freq (MHz) at which totalPwr needs to be evaluated.
 * @param[out]  pTotalPwr     Pointer to buffer to return totalPwr.
 */
void
pwrPolicyWorkloadMultiRailEvaluateTotalPowerAtFreq
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE  *pWorkload,
    CLK_DOMAIN                               *pDomain,
    LwU32                                     freqMHz,
    LwU32*                                    pTotalPwr,
    LwU8                                      limitUnit
)
{
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_FREQ_INPUT *pInput =
        &pWorkload->freq;
    VOLT_RAIL                  *pRail;
    CLK_DOMAIN_PROG            *pDomainProg;
    LW2080_CTRL_CLK_VF_INPUT    clkArbInput;
    LW2080_CTRL_CLK_VF_OUTPUT   clkArbOutput;
    LwU32           voltageuV[
        LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IDX_MAX];
    LwU8            railIdx;
    LwU8            dynamicEquIdx = LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID;
    LwU32           totalPwr      = 0;
    LwS32           voltDeltauV   = RM_PMU_VOLT_VALUE_0V_IN_UV_SIGNED;
    FLCN_STATUS     status;

    pDomainProg = CLK_DOMAIN_BOARDOBJ_TO_INTERFACE_CAST(pDomain, PROG);
    if (pDomainProg == NULL)
    {
        PMU_BREAKPOINT();
        return;
    }

    // Initialize the required parameters.
    LW2080_CTRL_CLK_VF_INPUT_INIT(&clkArbInput);

    // Callwlate the per rail voltage required to support given freq
    clkArbInput.value = freqMHz;

    // Translate the graphics clock reading to target 1x clock (GPC)
    freqMHz = s_pwrPolicyDomGrpFreqTranslate(freqMHz,
        pwrPoliciesGetGraphicsClkDomain(Pmgr.pPwrPolicies),
        LW2080_CTRL_CLK_DOMAIN_GPCCLK);

    FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkload, railIdx)
    {
        status = clkDomainProgFreqToVolt(
                    pDomainProg,
                    pWorkload->rail[railIdx].rail.voltRailIdx,
                    LW2080_CTRL_CLK_VOLTAGE_TYPE_POR,
                    &clkArbInput,
                    &clkArbOutput,
                    NULL);
        if ((status != FLCN_OK) ||
            (clkArbOutput.inputBestMatch ==
                LW2080_CTRL_CLK_VF_VALUE_ILWALID))
        {
            PMU_BREAKPOINT();
        }
        voltageuV[railIdx] =
            LW_MAX(pInput->rail[railIdx].voltFlooruV, clkArbOutput.value);

        // Update the voltage value with the voltage delta value
#if PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)
        // Get the voltage delta applied by close loop frequency controller
        if (FLCN_OK != voltPolicyOffsetGet(pWorkload->voltPolicyIdx, &voltDeltauV))
        {
            PMU_BREAKPOINT();
        }
#else
        {
            voltDeltauV = perfChangeSeqChangeLastVoltOffsetuVGet(railIdx,
                            LW2080_CTRL_VOLT_VOLT_RAIL_OFFSET_CLFC);
        }
#endif // PMUCFG_FEATURE_ENABLED(PMU_VOLT_VOLT_POLICY_OFFSET)

        voltageuV[railIdx] += voltDeltauV;
        pInput->rail[railIdx].voltmV =
            PWR_POLICY_WORKLOAD_UV_TO_MV(voltageuV[railIdx]);

        // Callwlate the new leakage using the new required voltage
        pRail  = VOLT_RAIL_GET(pWorkload->rail[railIdx].rail.voltRailIdx);
        if (pRail == NULL)
        {
            PMU_BREAKPOINT();
            return;
        }
        if (FLCN_OK != voltRailGetLeakage(pRail, voltageuV[railIdx],
                    pwrPolicyLimitUnitGet(pWorkload),
                    NULL, /* PGres = 0.0 */
                    &(pInput->rail[railIdx].leakagemX)))
        {
            PMU_BREAKPOINT();
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION_DYNAMIC))
        {
            // Fetch the dynamic equation index
            dynamicEquIdx = voltRailVoltScaleExpPwrEquIdxGet(pRail);
        }

        //
        // This case is for new SW model which has introduced the concept of
        // DYNAMIC equations
        //
        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_EQUATION_DYNAMIC) &&
            (dynamicEquIdx != LW2080_CTRL_PMGR_PWR_EQUATION_INDEX_ILWALID))
        {
            LwU64                 factor;
            LwU64                 estimatedValU64;
            LwUFXP52_12           tempFXP52_12;
            LwUFXP40_24           productFXP40_24;
            LwUFXP52_12           workloadFXP52_12 =
                (LwUFXP52_12)pInput->rail[railIdx].workload;
            PWR_EQUATION_DYNAMIC *pDyn =
                (PWR_EQUATION_DYNAMIC *)PWR_EQUATION_GET(dynamicEquIdx);

            if (pDyn == NULL)
            {
                PMU_BREAKPOINT();
                return;
            }

            if (limitUnit ==
                 LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
            {
                //
                // Compute the power required per rail
                // From previous callwlation of workload:
                //  w =          mW
                //      --------------------
                //      V^(dynamicExp) * MHz
                //
                // Pwr (per rail) = V^(dynamicExp) * f * w + Pleakage
                // i.e.
                //  mV^(dynamicExp) * MHz *        ( mW )        + mW
                //                         --------------------       = mW
                //                         mV^(dynamicExp) * MHz
                //
                // 52.12 * 32.0 * 52.12 = 40.24 => 20.12
                // Will overflow when power/current is > 1048575 mW/mA, this
                // should be a safe assumption.
                //
                factor = 1000;
                tempFXP52_12 =
                    (LwUFXP52_12)pwrEquationDynamicScaleVoltagePower(pDyn,
                    LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
                        pInput->rail[railIdx].voltmV, 1000));
                lwU64Mul(&tempFXP52_12, &tempFXP52_12, &factor);
                factor = freqMHz;
                lwU64Mul(&tempFXP52_12, &tempFXP52_12, &factor);
            }
            else
            {
                factor = 1000;
                tempFXP52_12 =
                    (LwUFXP52_12)pwrEquationDynamicScaleVoltageLwrrent(pDyn,
                    LW_TYPES_U32_TO_UFXP_X_Y_SCALED(20, 12,
                        pInput->rail[railIdx].voltmV, 1000));
                lwU64Mul(&tempFXP52_12, &tempFXP52_12, &factor);
                factor = freqMHz;
                lwU64Mul(&tempFXP52_12, &tempFXP52_12, &factor);
            }
            lwU64Mul(&productFXP40_24, &tempFXP52_12, &workloadFXP52_12);
            lw64Lsr(&estimatedValU64, &productFXP40_24, 24);

            if (LwU64_HI32(estimatedValU64) != LW_TYPES_FXP_ZERO)
            {
                // Overflow detected, cap estimatedVal to LW_U32_MAX
                pInput->rail[railIdx].estimatedVal = LW_U32_MAX;
            }
            else
            {
                pInput->rail[railIdx].estimatedVal =
                    LwU64_LO32(estimatedValU64);
            }
        }
        else
        {
            //
            // Compute the power required per rail
            // From previous callwlation of workload:
            //  w =   mW
            //      -------
            //    mV^2 * MHz
            //
            // Pwr (per rail) = V^2 * f * w + Pleakage
            // i.e.
            //  mV * mV * MHz * ( mW ) + mW
            //    ----          ------        = mW
            //    1000        mV^2 * MHz
            //
            // 20.12 * 32.0 = 20.12
            // Will overflow when power/current is > 1048575 mW/mA, this
            // should be a safe assumption.
            //
            if (limitUnit ==
                LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW)
            {
                pInput->rail[railIdx].estimatedVal =
                    freqMHz * LW_UNSIGNED_ROUNDED_DIV(
                    (pInput->rail[railIdx].voltmV * pInput->rail[railIdx].voltmV),
                    1000);
            }
            else
            {
                pInput->rail[railIdx].estimatedVal = freqMHz *
                    pInput->rail[railIdx].voltmV;
            }

            //
            // Note: 32BIT_OVERFLOW - Possible FXP20.12 overflow, not going to fix
            // because this code segment is tied to the
            // @ref PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD feature which is not
            // going to be used on AD10X and later chips.
            //
            // TODO-Chandrashis: Add the @ref PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD
            // feature to the CONFLICTS list for @ref PMU_PMGR_32BIT_OVERFLOW_FIXES
            // feature once the @ref PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD feature
            // is not enabled on AD10X and later chips any more. 
            //
            pInput->rail[railIdx].estimatedVal =
                LW_TYPES_UFXP_X_Y_TO_U32_ROUNDED(20, 12,
                (pInput->rail[railIdx].estimatedVal * pInput->rail[railIdx].workload));
        }

        pInput->rail[railIdx].estimatedVal =
            pInput->rail[railIdx].estimatedVal + pInput->rail[railIdx].leakagemX;
        totalPwr += pInput->rail[railIdx].estimatedVal;
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

    *pTotalPwr = totalPwr;
}

/*!
 * Interface implementation to provide a mask of topologies
 * that this policy evaluates.
 *
 * @param[in] pWorkIface   PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pointer.
 *
 * @return LwU32 channel mask for all the related rails.
 */
LwU32
pwrPolicy3XChannelMaskGet_WORKLOAD_MULTIRAIL_INTERFACE
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface
)
{
    LwU8  railIdx;
    LwU32 channelMask = 0;

    FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkIface, railIdx)
    {
        channelMask |= BIT(pWorkIface->rail[railIdx].rail.chIdx);
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

    return channelMask;
}

/*!
 * Interface implmentation to retrieve the current GPU state (voltage, leakage power, etc.)
 * common to interface, which will be used to callwlate the current workload/active capacitance term (w).
 *
 * @param[in]     pWorkIface   PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pointer.
 * @param[in/out] pInput
 *     RM_PMU_PMGR_PWR_POLICY_QUERY_POLICY_WORKLOAD_MULTIRAIL_WORK_INPUT pointer to
 *     populate the with the current GPU state.
 * @param[in]     limitUnit    @LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_<xyz>
 *
 * @return FLCN_OK
 *     All state successfully retrieved.
 * @return FLCN_ERR_ILWALID_STATE
 *     Invalid GPU state returned from the PERF engine.
 */
FLCN_STATUS
pwrPolicyWorkloadMultiRailInputStateGetCommonParams
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface,
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_DATA_WORKLOAD_MULTIRAIL_WORK_INPUT *pInput,
    LwU8 limitUnit
)
{
    VOLT_RAIL   *pRail;
    LwU32           voltageuV;
    LwU8            railIdx;
    FLCN_STATUS     status = FLCN_OK;

    FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkIface, railIdx)
    {
        pRail = VOLT_RAIL_GET(pWorkIface->rail[railIdx].rail.voltRailIdx);
        if (pRail == NULL)
        {
            PMU_BREAKPOINT();
            status = FLCN_ERR_ILWALID_INDEX;
            goto pwrPolicyWorkloadMultiRailInputStateGetCommonParams_exit;
        }

        if (PMUCFG_FEATURE_ENABLED(PMU_PMGR_PWR_POLICY_MULTIRAIL_WORKLOAD_SENSED_VOLTAGE) &&
            pWorkIface->bSensedVoltage)
        {
            VOLT_RAIL_SENSED_VOLTAGE_DATA *pSensed =
                PWR_POLICY_WORKLOAD_MULTIRAIL_VOLT_RAIL_IFACE_SENSED_GET(pWorkIface, railIdx);

            status = voltRailGetVoltageSensed(pRail, pSensed);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrPolicyWorkloadMultiRailInputStateGetCommonParams_exit;
            }

            voltageuV = pSensed->voltageuV;
        }
        else
        {
            status = voltRailGetVoltage(pRail, &voltageuV);
            if (status != FLCN_OK)
            {
                PMU_BREAKPOINT();
                goto pwrPolicyWorkloadMultiRailInputStateGetCommonParams_exit;
            }
        }

        // Callwlate leakage power for given voltage.
        status = voltRailGetLeakage(pRail, voltageuV,
                    limitUnit,
                    NULL,
                    &(pInput->rail[railIdx].leakagemX));
        if (status != FLCN_OK)
        {
            PMU_BREAKPOINT();
            goto pwrPolicyWorkloadMultiRailInputStateGetCommonParams_exit;
        }

        // Get voltage in mV.
        pInput->rail[railIdx].voltmV = PWR_POLICY_WORKLOAD_UV_TO_MV(voltageuV);
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

pwrPolicyWorkloadMultiRailInputStateGetCommonParams_exit:
    return status;
}

/*!
 * Interface implmentation for caching channels reading.
 *
 * @param[in]   pWorkIface   PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE pointer.
 * @param[in]   pPayload     RM_PMU_PMGR_PWR_CHANNELS_TUPLE_QUERY_PAYLOAD pointer
 *
 * @return FLCN_OK
 *     All state successfully retrieved.
 * @return FLCN_ERR_ILWALID_STATE
 *     Invalid GPU state returned from the PERF engine.
 */
FLCN_STATUS
pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_INTERFACE
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE *pWorkIface,
    PWR_POLICY_3X_FILTER_PAYLOAD_TYPE       *pPayload
)
{
    PWR_POLICY                     *pPolicy =
        (PWR_POLICY *)INTERFACE_TO_BOARDOBJ_CAST(pWorkIface);
    LW2080_CTRL_PMGR_PWR_TUPLE     *pTuple;
    FLCN_STATUS                     status = FLCN_OK;
    LwU8                            railIdx;

    // Reset the global pwr policy valueLwrr
    pPolicy->valueLwrr = 0;

    //
    // This is populating the valueLwrr for each rail and adding it up.
    // Do not call the super class implementation here.
    //
    FOR_EACH_VALID_VOLT_RAIL_IFACE(pWorkIface, railIdx)
    {
        // Fetch the reading depending on current or power based policy type.
        pTuple = pwrPolicy3xFilterPayloadTupleGet(pPayload, pWorkIface->rail[railIdx].rail.chIdx);
        switch (pwrPolicyLimitUnitGet(pPolicy))
        {
            case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW:
                pWorkIface->rail[railIdx].valueLwrr = pTuple->pwrmW;
                break;
            case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA:
                pWorkIface->rail[railIdx].valueLwrr = pTuple->lwrrmA;
                break;
            default:
                PMU_HALT();
                status = FLCN_ERR_NOT_SUPPORTED;
                goto pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_INTERFACE_exit;
        }

        // update the global pwr policy valueLwrr
        pPolicy->valueLwrr += pWorkIface->rail[railIdx].valueLwrr;
    }
    FOR_EACH_VALID_VOLT_RAIL_IFACE_END

pwrPolicy3XFilter_WORKLOAD_MULTIRAIL_INTERFACE_exit:
    return status;
}

/*!
 * Inserts a single entry into a moving average filter of PG residency values.  For
 * more information about moving average filters see:
 * https://en.wikipedia.org/wiki/Moving_average
 *
 * @param[in]      pWorkIface     PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE pointer
 * @param[in]      pMovingAvg     Pointer to moving average filter pointer
 * @param[in]      entryInput     Entry input (20.12) to the moving average filter
 *
 * @return FLCN_OK                   On succesful insertion
 *         FLCN_ERR_ILWALID_STATE    If moving average buffer is not valid
 *         FLCN_ERR_ILWALID_ARGUMENT If passed input is not valid
 *
 */
FLCN_STATUS
pwrPolicyWorkloadMultiRailMovingAvgFilterInsert
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE            *pWorkIface,
    PWR_POLICY_WORKLOAD_MULTIRAIL_MOVING_AVG_FILTER    *pMovingAvg,
    LwUFXP20_12                                         entryInput
)
{
    FLCN_STATUS status = FLCN_OK;

    if (pMovingAvg->pEntries == NULL)
    {
        status = FLCN_ERR_ILWALID_STATE;
        PMU_BREAKPOINT();
        goto pwrPolicyWorkloadMultiRailMovingAvgFilterInsert_exit;
    }

    // Sanity check input entry
    if (entryInput > LW_TYPES_U32_TO_UFXP_X_Y(20, 12, 1))
    {
        status = FLCN_ERR_ILWALID_ARGUMENT;
        PMU_BREAKPOINT();
        goto pwrPolicyWorkloadMultiRailMovingAvgFilterInsert_exit;
    }

    pMovingAvg->pEntries[pMovingAvg->idx] = entryInput;
    pMovingAvg->sizeLwrr = LW_MAX(++pMovingAvg->idx, pMovingAvg->sizeLwrr);
    pMovingAvg->idx %= pMovingAvg->bufferSize;

pwrPolicyWorkloadMultiRailMovingAvgFilterInsert_exit:
    return status;
}

/*!
 * Applies a moving average filter to the PG residency values.  For
 * more information about moving average filters see:
 * https://en.wikipedia.org/wiki/Moving_average
 *
 * @param[in]      pWorkIface     PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE pointer
 * @param[in]      pMovingAvg     Pointer to moving average filter pointer
 *
 * @return moving average of samples (20.12)
 *
 */
LwUFXP20_12
pwrPolicyWorkloadMultiRailMovingAvgFilterQuery
(
    PWR_POLICY_WORKLOAD_MULTIRAIL_INTERFACE            *pWorkIface,
    PWR_POLICY_WORKLOAD_MULTIRAIL_MOVING_AVG_FILTER    *pMovingAvg
)
{
    LwU8        i;
    LwUFXP20_12 avg = 0;

    if (pMovingAvg->pEntries == NULL)
    {
        goto pwrPolicyWorkloadMultiRailMovingAvgFilterQuery_exit;
    }

    if (pMovingAvg->sizeLwrr != 0)
    {
        for (i = 0; i < pMovingAvg->sizeLwrr; i++)
        {
            avg += pMovingAvg->pEntries[i];
        }
        avg /= pMovingAvg->sizeLwrr;
    }

pwrPolicyWorkloadMultiRailMovingAvgFilterQuery_exit:
    return avg;
}
/* ------------------------- Private Functions ------------------------------ */
