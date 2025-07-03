/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   gputempmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/thermsub.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "core/include/platform.h"

// This uses the primary sensor - RM picks which ever VBIOS selects as primary
// This is the Parent class for Int/Ext monitors
class GpuTempMonitor : public PerGpuMonitor
{
public:
    explicit GpuTempMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "GpuTemp")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "GpuTemp", "C", true, FLOAT });
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        Thermal *pTherm = pSubdev->GetThermal();
        if (pSubdev->IsSOC())
        {
            FLOAT32 temp = 0;
            if (OK != pTherm->GetSocGpuTemp(&temp))
            {
                Printf(Tee::PriHigh, "Error: Unable to provide temperature readings\n");
                return OK;
            }
        }
        else
        {
            bool ExtPresent = false;
            bool IntPresent = false;
            if ((OK != pTherm->GetInternalPresent(&IntPresent)) &&
                (OK != pTherm->GetExternalPresent(&ExtPresent)))
            {
                Printf(Tee::PriNormal,
                       "GpuTempMonitor : RM call to get detected @%s failed\n",
                       pSubdev->GpuIdentStr().c_str());
                return RC::THERMAL_SENSOR_ERROR;
            }

            if (!IntPresent && !ExtPresent)
            {
                Printf(Tee::PriNormal,
                       "GpuTempMonitor : No sensor detected @%s\n",
                       pSubdev->GpuIdentStr().c_str());
                return RC::THERMAL_SENSOR_ERROR;
            }
        }

        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        FLOAT32 temp = -273.15f;
        RC rc;
        CHECK_RC(pSubdev->GetThermal()->GetChipTempViaPrimary(&temp));

        pSample->Push(temp);

        m_GpuTempStatsList[pSubdev->GetGpuInst()].Update(temp);

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, FLOAT, 1);
    }

    RC StatsChecker() override
    {
        for (auto* pSubdev : *DevMgrMgr::d_GraphDevMgr)
        {
            const UINT32 gpuInst = pSubdev->GetGpuInst();
            if (!IsMonitoringGpu(gpuInst))
            {
                continue;
            }

            Printf(GetStatsPri(), "GPU temperature by primary sensor [%s]\n",
                    pSubdev->GpuIdentStr().c_str());
            Printf(GetStatsPri(), "  Current temp : %.1f\n"
                    "  Maximum temp : %.1f\n"
                    "  Minimum temp : %.1f\n\n",
                    m_GpuTempStatsList[gpuInst].last,
                    m_GpuTempStatsList[gpuInst].max,
                    m_GpuTempStatsList[gpuInst].min);
        }
        return OK;
    }

private:
    map<UINT32, Range<FLOAT32>> m_GpuTempStatsList;
};

BgMonFactoryRegistrator<GpuTempMonitor> RegisterGpuTempMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::GPUTEMP_SENSOR
);
