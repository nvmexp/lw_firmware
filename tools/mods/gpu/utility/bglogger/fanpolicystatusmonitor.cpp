/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation. All rights reserved. All information
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

class FanPolicyStatusMonitor : public PerGpuMonitor
{
public:
    explicit FanPolicyStatusMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Fan Policy")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        UINT32 inst = pSubdev->GetGpuInst();
        for (UINT32 index = 0; index < LW2080_CTRL_FAN_POLICY_MAX_POLICIES; index++)
        {
            if (BIT(index) & m_PolicyMask[inst])
            {
                string name = Utility::StrPrintf("%u ", index);
                descs.push_back({ name + "fanTjAvgShortTerm", "C",  true, FLOAT });
                descs.push_back({ name + "fanTjAvgLongTerm",  "C",  true, FLOAT });
                descs.push_back({ name + "targetPwm",         "%",  true, FLOAT });
                descs.push_back({ name + "targetRpm",         "",   true, INT });
                descs.push_back({ name + "tjLwrrent",         "",   true, FLOAT });
                descs.push_back({ name + "filteredPwr",       "mW", true, INT });
                descs.push_back({ name + "bFanStopActive",    "",   true, INT });
            }
        }

        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        RC rc;
        UINT32 inst = pSubdev->GetGpuInst();
        Thermal* pTherm = pSubdev->GetThermal();
        UINT32 policyMask = 0;
        if ((RC::OK != pTherm->GetFanPolicyMask(&policyMask)) || policyMask == 0)
        {
            Printf(Tee::PriError,
                   "FanPolicyStatusMonitor : Fan Policy not supported on %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return RC::FAN_NOT_WORKING;
        }
        m_PolicyMask[inst] = policyMask;
        return rc;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;
        UINT32 inst = pSubdev->GetGpuInst();
        Thermal *pTherm = pSubdev->GetThermal();
        LW2080_CTRL_FAN_POLICY_STATUS_PARAMS statusParams = {};
        CHECK_RC(pTherm->GetFanPolicyStatus(&statusParams));
        for (UINT32 index = 0; index < LW2080_CTRL_FAN_POLICY_MAX_POLICIES; index++)
        {
            if (BIT(index) & m_PolicyMask[inst])
            {
                LW2080_CTRL_FAN_POLICY_STATUS status = statusParams.policies[index];
                LW2080_CTRL_FAN_POLICY_STATUS_DATA_IIR_TJ_FIXED_DUAL_SLOPE_PWM iirTFDSP;
                switch (status.type)
                {
                    case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V20:
                        iirTFDSP = status.data.iirTFDSPV20.iirTFDSP;
                        break;
                    case LW2080_CTRL_FAN_POLICY_TYPE_IIR_TJ_FIXED_DUAL_SLOPE_PWM_V30:
                        iirTFDSP = status.data.iirTFDSPV30.iirTFDSP;
                        break;
                    default:
                        Printf(Tee::PriError, "Unknown Fan Policy Status type: %u\n", status.type);
                        return RC::SOFTWARE_ERROR;
                }

                pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(iirTFDSP.tjAvgShortTerm, 10, 22)));
                pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(iirTFDSP.tjAvgLongTerm, 10, 22)));
                pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(iirTFDSP.targetPwm, 16, 16) * 100));
                pSample->Push(iirTFDSP.targetRpm);
                pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(iirTFDSP.tjLwrrent, 24, 8)));
                pSample->Push(iirTFDSP.filteredPwrmW);
                pSample->Push(static_cast<UINT32>(iirTFDSP.bFanStopActive));
            }
        }
        return rc;
    }

private:
    map<UINT32, UINT32> m_PolicyMask;
};

BgMonFactoryRegistrator<FanPolicyStatusMonitor> RegisterFanPolicyStatusMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::FAN_POLICY_STATUS
);
