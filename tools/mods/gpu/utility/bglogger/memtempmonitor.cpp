/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   memtempmonitor.cpp
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

//-----------------------------------------------------------------------------
// This uses the memory sensor - RM picks which ever VBIOS selects as memory
// sensor.
class MemTempMonitor : public PerGpuMonitor
{
public:
    explicit MemTempMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "MemTemp")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "MemTemp", "C", true, FLOAT });
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        Thermal *pTherm = pSubdev->GetThermal();

        bool sensorPresent = false;
        if (OK != pTherm->GetMemorySensorPresent(&sensorPresent))
        {
            Printf(Tee::PriNormal,
                   "RM call to detect memory sensor @%s failed\n",
                   pSubdev->GpuIdentStr().c_str());
            return RC::THERMAL_SENSOR_ERROR;
        }
        if (!sensorPresent)
        {
            Printf(Tee::PriNormal,
                   "No memory sensor detected @%s\n",
                   pSubdev->GpuIdentStr().c_str());
            return RC::THERMAL_SENSOR_ERROR;
        }

        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        FLOAT32 temp = -273.15f;
        RC rc;
        CHECK_RC(pSubdev->GetThermal()->GetChipMemoryTemp(&temp));

        pSample->Push(temp);

        m_MemTempStatsList[pSubdev->GetGpuInst()].Update(temp);

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

            Printf(GetStatsPri(), "GPU temperature by memory sensor [%s]\n",
                    pSubdev->GpuIdentStr().c_str());
            Printf(GetStatsPri(), "  Current temp : %.1f\n"
                    "  Maximum temp : %.1f\n"
                    "  Minimum temp : %.1f\n\n",
                    m_MemTempStatsList[gpuInst].last,
                    m_MemTempStatsList[gpuInst].max,
                    m_MemTempStatsList[gpuInst].min);
        }
        return OK;
    }

private:
    map<UINT32, Range<FLOAT32>> m_MemTempStatsList;
};

BgMonFactoryRegistrator<MemTempMonitor> RegisterMemTempMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::MEM_TEMP_SENSOR
);
