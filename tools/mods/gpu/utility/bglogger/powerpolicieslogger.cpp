/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/thermsub.h"
#include "gpu/perf/pwrsub.h"

class PowerPolicyStatusMonitor : public PerGpuMonitor
{
public:
    explicit PowerPolicyStatusMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Power Policy")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        UINT32 inst = pSubdev->GetGpuInst();

        for (UINT32 polIdx = 0; polIdx < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES;
            polIdx++)
        {
            /*!
            * StatusParam.policies - Array of PWR_POLICY entries.
            * Has valid indexes corresponding to the bits set in @ref policyMask.
            */
            if (0 == ((1 << polIdx) & m_PolicyMask[inst]))
            {
                continue;
            }

            switch (m_PwrPolicyInfoParams.policies[polIdx].spec.v2x.limitUnit)
            {
                case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_POWER_MW:
                    m_UnitSuffix = "mW";
                    break;
                case LW2080_CTRL_PMGR_PWR_POLICY_LIMIT_UNIT_LWRRENT_MA:
                    m_UnitSuffix = "mA";
                    break;
                default:
                    m_UnitSuffix = "Unknown";
                    break;
            }
            descs.push_back({ Utility::StrPrintf("P%dLwrrLimit", polIdx), m_UnitSuffix.c_str(), true, INT });
            descs.push_back({ Utility::StrPrintf("P%dLwrrValue", polIdx), m_UnitSuffix.c_str(), true, INT });
            descs.push_back({ Utility::StrPrintf("P%dDelta", polIdx), m_UnitSuffix.c_str(), true, INT });
            descs.push_back({ Utility::StrPrintf("P%dLwrrRunningDiff", polIdx), "", true, INT });
            descs.push_back({ Utility::StrPrintf("P%dLwrrIntegralLimit", polIdx), "", true, INT });

            LW2080_CTRL_PMGR_PWR_POLICY_STATUS *pPolicy;
            pPolicy = &(m_StatusParams.policies[polIdx]);
            UINT32 numInputs = pPolicy->limitArbPmuLwrr.numInputs;
            descs.push_back({ Utility::StrPrintf("P%dNumInputs", polIdx), "", true, INT });
            for (UINT32 inpIdx = 0; inpIdx < numInputs; inpIdx++)
            {
                descs.push_back({ Utility::StrPrintf("P%dLimitInputs%dpwrPolIndex", polIdx, inpIdx), "", true, INT });
                descs.push_back({ Utility::StrPrintf("P%dLimitInputs%dlimitValue", polIdx, inpIdx), m_UnitSuffix.c_str(), true, INT });
            }
            switch (pPolicy->type)
            {
                // _DOMGRP class.
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BANG_BANG_VF:
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH_VF:
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
                // WORKLOAD_MULTIRAIL
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
                {
                    descs.push_back({ Utility::StrPrintf("P%dDomGrpLimitsPState", polIdx), "", true, INT });
                    descs.push_back({ Utility::StrPrintf("P%dDomGrpLimitsGpc2clk", polIdx), "", true, INT });
                    descs.push_back({ Utility::StrPrintf("P%dDomGrpCeilingPState", polIdx), "", true, INT });
                    descs.push_back({ Utility::StrPrintf("P%dDomGrpCeilingGpc2clk", polIdx), "", true, INT });
                    break;
                }
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
                {
                    descs.push_back({ Utility::StrPrintf("P%dDomGrpLimitsPState", polIdx), "", true, INT });
                    descs.push_back({ Utility::StrPrintf("P%dDomGrpLimitsGpc2clk", polIdx), "", true, INT });
                    descs.push_back({ Utility::StrPrintf("P%dDomGrpCeilingPState", polIdx), "", true, INT });
                    descs.push_back({ Utility::StrPrintf("P%dDomGrpCeilingGpc2clk", polIdx), "", true, INT });
                    descs.push_back({ Utility::StrPrintf("P%dobsMetObservedVal", polIdx), "", true, INT });
                    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_SINGLE_1X_MAX; i++)
                    {
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dVolt", polIdx, i), "mV", true, INT });
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dfrequency", polIdx, i), "MHz", true, INT });
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dobservedVal", polIdx, i), "", true, INT });
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dworkload", polIdx, i), "", true, INT });
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dClkCntStarttime", polIdx, i), "ns", true, INT });
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dClkCntStarttickCnt", polIdx, i), "", true, INT });
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dSensedNumSamples", polIdx, i), "", true, INT });
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dSensedMode", polIdx, i), "", true, INT });
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dSensedVoltage", polIdx, i), "uV", true, INT });
                        for (UINT32 j = 0; j < LW2080_CTRL_VOLT_CLK_ADC_ACC_SAMPLE_MAX; j++)
                        {
                            descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dSensedClkAdcSampleSampledCode%d", polIdx, i, j), "", true, INT });
                        }
                        descs.push_back({ Utility::StrPrintf("P%dobsMetSingles%dIndependentClkDomMask", polIdx, i), "", true, INT });

                    }
                    break;
                }
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT:
                {
                    descs.push_back({ Utility::StrPrintf("P%dbBalancedDirty", polIdx), "", true, INT });
                    break;
                }
                default:
                    break;
            }
        }

        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        RC rc;
        UINT32 inst = pSubdev->GetGpuInst();
        Power* pPwr = pSubdev->GetPower();
        UINT32 policyMask = 0;
        CHECK_RC(pPwr->PrepPmgrPowerPolicyInfoParam(&m_PwrPolicyInfoParams));
        policyMask = m_PwrPolicyInfoParams.policyMask;
        if (policyMask == 0)
        {
            Printf(Tee::PriError,
                   "PowerPolicyStatusMonitor : Power Policy not supported on %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return RC::BAD_PARAMETER;
        }
        m_PolicyMask[inst] = policyMask;
        memset(&m_StatusParams, 0, sizeof(LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS));
        CHECK_RC(pPwr->PrepPmgrPowerPolicyStatusParam(m_PolicyMask[inst], &m_StatusParams));
        return rc;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;
        UINT32 inst = pSubdev->GetGpuInst();
        Power* pPwr = pSubdev->GetPower();
        LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS StatusParam;
        memset(&StatusParam, 0, sizeof(LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS));
        CHECK_RC(pPwr->PrepPmgrPowerPolicyStatusParam(m_PolicyMask[inst], &StatusParam));

        for (UINT32 polIdx = 0; polIdx < LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES;
            polIdx++)
        {
            /*!
            * StatusParam.policies - Array of PWR_POLICY entries.
            * Has valid indexes corresponding to the bits set in @ref policyMask.
            */
            if (0 == ((1 << polIdx) & m_PolicyMask[inst]))
            {
                continue;
            }
            LW2080_CTRL_PMGR_PWR_POLICY_STATUS *pPolicy;
            pPolicy = &(StatusParam.policies[polIdx]);
            pSample->Push(pPolicy->limitLwrr);
            pSample->Push(pPolicy->valueLwrr);
            pSample->Push(pPolicy->limitDelta);
            pSample->Push(pPolicy->integral.lwrrRunningDiff);
            pSample->Push(pPolicy->integral.lwrrIntegralLimit);

            UINT32 numInputs = pPolicy->limitArbPmuLwrr.numInputs;
            pSample->Push(numInputs);
            for (UINT32 inpIdx = 0; inpIdx < numInputs; inpIdx++)
            {
                pSample->Push(pPolicy->limitArbPmuLwrr.inputs[inpIdx].pwrPolicyIdx);
                pSample->Push(pPolicy->limitArbPmuLwrr.inputs[inpIdx].limitValue);
            }
            switch (pPolicy->type)
            {
                // _DOMGRP class.
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_BANG_BANG_VF:
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_MARCH_VF:
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD:
                // WORKLOAD_MULTIRAIL
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL:
                {
                    pSample->Push(pPolicy->data.domGrp.domGrpLimits.values[0]);
                    pSample->Push(pPolicy->data.domGrp.domGrpLimits.values[1]);
                    pSample->Push(pPolicy->data.domGrp.domGrpCeiling.values[0]);
                    pSample->Push(pPolicy->data.domGrp.domGrpCeiling.values[1]);
                    break;
                }
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_COMBINED_1X:
                {
                    pSample->Push(pPolicy->data.combined1x.domGrp.domGrpLimits.values[0]);
                    pSample->Push(pPolicy->data.combined1x.domGrp.domGrpLimits.values[1]);
                    pSample->Push(pPolicy->data.combined1x.domGrp.domGrpCeiling.values[0]);
                    pSample->Push(pPolicy->data.combined1x.domGrp.domGrpCeiling.values[1]);
                    pSample->Push(pPolicy->data.combined1x.obsMet.observedVal);

                    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_POLICY_WORKLOAD_SINGLE_1X_MAX; i++)
                    {
                        UINT32 timeNs, tickCnt;
                        LwU64_ALIGN32_UNPACK(&timeNs, &pPolicy->data.combined1x.obsMet.singles[i].clkCntrStart.timens);
                        LwU64_ALIGN32_UNPACK(&tickCnt, &pPolicy->data.combined1x.obsMet.singles[i].clkCntrStart.tickCnt);
                        pSample->Push(pPolicy->data.combined1x.obsMet.singles[i].voltmV);
                        pSample->Push(pPolicy->data.combined1x.obsMet.singles[i].freqMHz);
                        pSample->Push(pPolicy->data.combined1x.obsMet.singles[i].observedVal);
                        pSample->Push(pPolicy->data.combined1x.obsMet.singles[i].workload);
                        pSample->Push(timeNs);
                        pSample->Push(tickCnt);
                        pSample->Push(pPolicy->data.combined1x.obsMet.singles[i].sensed.numSamples);
                        pSample->Push(pPolicy->data.combined1x.obsMet.singles[i].sensed.mode);
                        pSample->Push(pPolicy->data.combined1x.obsMet.singles[i].sensed.voltageuV);

                        for (UINT32 j = 0; j < LW2080_CTRL_VOLT_CLK_ADC_ACC_SAMPLE_MAX; j++)
                        {
                            pSample->Push(pPolicy->data.combined1x.obsMet.singles[i].sensed.clkAdcAccSample[j].sampledCode);
                        }
                        pSample->Push(pPolicy->data.combined1x.obsMet.singles[i].independentDomainsVmin.independentClkDomainMask.super.pData[0]);
                    }
                    break;
                }
                case LW2080_CTRL_PMGR_PWR_POLICY_TYPE_PROP_LIMIT:
                {
                    pSample->Push(pPolicy->data.propLimit.bBalanceDirty);
                    break;
                }
                default:
                    break;
            }
        }

        return rc;
    }

private:
    map<UINT32, UINT32> m_PolicyMask;
    string m_UnitSuffix;
    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS m_PwrPolicyInfoParams = {};
    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS m_StatusParams = {};
};

BgMonFactoryRegistrator<PowerPolicyStatusMonitor> RegisterPowerPolicyStatusMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::POWER_POLICIES_LOGGER
);