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

class FanCoolerStatusMonitor : public PerGpuMonitor
{
public:
    explicit FanCoolerStatusMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Fan Cooler")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        UINT32 inst = pSubdev->GetGpuInst();
        for (UINT32 index = 0; index < LW2080_CTRL_FAN_POLICY_MAX_POLICIES; index++)
        {
            if (BIT(index) & m_CoolerMask[inst])
            {
                string name = Utility::StrPrintf("%u ", index);
                descs.push_back({ name + "rpmLast",       "",  true, INT });
                descs.push_back({ name + "rpmTarget",     "",  true, INT });
                descs.push_back({ name + "pwmActual",     "%", true, FLOAT });
                descs.push_back({ name + "pwmLwrr",       "%", true, FLOAT });
                descs.push_back({ name + "bMaxFanActive", "",  true, INT });
                descs.push_back({ name + "pwmRequested",  "%", true, FLOAT });
                descs.push_back({ name + "rpmLwrr",       "",  true, INT });
                descs.push_back({ name + "levelMin",      "%", true, FLOAT });
                descs.push_back({ name + "levelMax",      "%", true, FLOAT });
                descs.push_back({ name + "levelLwrrent",  "%", true, FLOAT });
                descs.push_back({ name + "levelTarget",   "%", true, FLOAT });
            }
        }

        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        RC rc;
        UINT32 inst = pSubdev->GetGpuInst();
        Thermal* pTherm = pSubdev->GetThermal();
        UINT32 coolerMask = 0;
        if ((RC::OK != pTherm->GetFanCoolerMask(&coolerMask)) || coolerMask == 0)
        {
            Printf(Tee::PriError,
                   "FanCoolerStatusMonitor : Fan Cooler not supported on %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return RC::FAN_NOT_WORKING;
        }
        m_CoolerMask[inst] = coolerMask;
        return rc;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;
        UINT32 inst = pSubdev->GetGpuInst();
        Thermal *pTherm = pSubdev->GetThermal();
        LW2080_CTRL_FAN_COOLER_STATUS_PARAMS statusParams = {};
        CHECK_RC(pTherm->GetFanCoolerStatus(&statusParams));
        for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
        {
            if (BIT(index) & m_CoolerMask[inst])
            {
                LW2080_CTRL_FAN_COOLER_STATUS status = statusParams.coolers[index];
                switch (status.type)
                {
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                        // status.data.activePwm
                        pSample->Push(0U);   // N/A
                        pSample->Push(0U);   // N/A
                        pSample->Push(0.0F); // N/A
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwm.pwmLwrr, 16, 16) * 100));
                        pSample->Push(static_cast<UINT32>(status.data.activePwm.bMaxFanActive));
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwm.pwmRequested, 16, 16) * 100));
                        pSample->Push(status.data.activePwm.active.rpmLwrr);
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwm.active.levelMin, 16, 16) * 100));
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwm.active.levelMax, 16, 16) * 100));
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwm.active.levelLwrrent, 16, 16) * 100));
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwm.active.levelTarget, 16, 16) * 100));
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                        // status.data.activePwmTachCorr
                        pSample->Push(status.data.activePwmTachCorr.rpmLast);
                        pSample->Push(status.data.activePwmTachCorr.rpmTarget);
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwmTachCorr.pwmActual, 16, 16) * 100));
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwmTachCorr.activePwm.pwmLwrr, 16, 16) * 100));
                        pSample->Push(static_cast<UINT32>(status.data.activePwm.bMaxFanActive));
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwmTachCorr.activePwm.pwmRequested, 16, 16) * 100));
                        pSample->Push(status.data.activePwmTachCorr.activePwm.active.rpmLwrr);
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwmTachCorr.activePwm.active.levelMin, 16, 16) * 100));
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwmTachCorr.activePwm.active.levelMax, 16, 16) * 100));
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwmTachCorr.activePwm.active.levelLwrrent, 16, 16) * 100));
                        pSample->Push(static_cast<FLOAT32>(Utility::ColwertFXPToFloat(status.data.activePwmTachCorr.activePwm.active.levelTarget, 16, 16) * 100));
                        break;
                    default:
                        Printf(Tee::PriError, "Unknown Fan Cooler Status type: %u\n", status.type);
                        return RC::SOFTWARE_ERROR;
                }
            }
        }
        return rc;
    }

private:
    map<UINT32, UINT32> m_CoolerMask;
};

BgMonFactoryRegistrator<FanCoolerStatusMonitor> RegisterFanCoolerStatusMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::FAN_COOLER_STATUS
);
