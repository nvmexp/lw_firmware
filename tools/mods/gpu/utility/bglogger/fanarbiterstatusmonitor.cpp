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

class FanArbiterStatusMonitor : public PerGpuMonitor
{
public:
    explicit FanArbiterStatusMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Fan Arbiter")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        UINT32 inst = pSubdev->GetGpuInst();
        for (UINT32 index = 0; index < LW2080_CTRL_FAN_ARBITER_MAX_ARBITERS; index++)
        {
            if (BIT(index) & m_ArbiterMask[inst])
            {
                string name = Utility::StrPrintf("%u ", index);
                descs.push_back({ name + "targetPwm",        "%", true, FLOAT });
                descs.push_back({ name + "targetRpm",        "",  true, INT });
                descs.push_back({ name + "fanCtrlAction",    "",  true, STR });
                descs.push_back({ name + "drivingPolicyIdx", "",  true, INT });
            }
        }

        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        RC rc;
        UINT32 inst = pSubdev->GetGpuInst();
        Thermal* pTherm = pSubdev->GetThermal();
        UINT32 arbiterMask = 0;
        if ((RC::OK != pTherm->GetFanArbiterMask(&arbiterMask)) || arbiterMask == 0)
        {
            Printf(Tee::PriError,
                   "FanArbiterStatusMonitor : Fan Arbiter not supported on %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return RC::FAN_NOT_WORKING;
        }
        m_ArbiterMask[inst] = arbiterMask;
        return rc;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;
        UINT32 inst = pSubdev->GetGpuInst();
        Thermal *pTherm = pSubdev->GetThermal();
        LW2080_CTRL_FAN_ARBITER_STATUS_PARAMS statusParams = {};
        CHECK_RC(pTherm->GetFanArbiterStatus(&statusParams));

        for (UINT32 index = 0; index < LW2080_CTRL_FAN_ARBITER_MAX_ARBITERS; index++)
        {
            if (BIT(index) & m_ArbiterMask[inst])
            {
                LW2080_CTRL_FAN_ARBITER_STATUS status = statusParams.arbiters[index];
                pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.targetPwm, 16, 16)));
                pSample->Push(status.targetRpm);
                pSample->Push(Thermal::FanArbiterCtrlActionToString(status.fanCtrlAction));
                pSample->Push(static_cast<UINT32>(status.drivingPolicyIdx));
            }
        }

        return rc;
    }

private:
    map<UINT32, UINT32> m_ArbiterMask;
};

BgMonFactoryRegistrator<FanArbiterStatusMonitor> RegisterFanArbiterStatusMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::FAN_ARBITER_STATUS
);
