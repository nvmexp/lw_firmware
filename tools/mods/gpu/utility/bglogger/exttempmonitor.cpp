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
 * @file   exttempmonitor.cpp
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
#include "core/include/utility.h"

class ExtTempMonitor : public PerGpuMonitor
{
public:
    explicit ExtTempMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "ExtTemp")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "ExtTemp", "C", true, FLOAT });
        return descs;
    }

    RC HandleParamsPostInit(const vector<UINT32>& params, const set<UINT32>&) override
    {
        // Param[0]=print mask, rest of the following elements are max, min
        // value for each gpu.  Min/Max are optional, but if present must be
        // specified for all GPUs and print mask must have been specified
        if ((params.size() > 1) &&
            (params.size() != (GetNumDevices() * 2 + 1)))
        {
            Printf(Tee::PriHigh, "Invalid Params passed to ExtTemp bglogger\n");
            return RC::SOFTWARE_ERROR;
        }
        const auto& monitored = MonitoredGpus();
        UINT32 pos = 1;
        for (UINT32 gpuInst = 0; gpuInst < monitored.size() && pos + 1 < params.size(); gpuInst++)
        {
            if (monitored[gpuInst])
            {
                m_ExpectedMinMax[gpuInst].max = static_cast<FLOAT32>(params[pos++]);
                m_ExpectedMinMax[gpuInst].min = static_cast<FLOAT32>(params[pos++]);
            }
        }
        return OK;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        Thermal *pTherm = pSubdev->GetThermal();
        bool ForceExt = false;
        bool ExtPresent = false;
        // if user forces a sensor, attempt to read the sensor regardless
        pTherm->GetForceExt(&ForceExt);
        if (!ForceExt)
        {
            if ((OK != pTherm->GetExternalPresent(&ExtPresent)) || !ExtPresent)
            {
                Printf(Tee::PriHigh,
                       "ExtTempMonitor: No external temperature sensor detected @%s\n",
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
        CHECK_RC(pSubdev->GetThermal()->GetChipTempViaExt(&temp));

        pSample->Push(temp);

        m_ExtTempStatsList[pSubdev->GetGpuInst()].Update(temp);

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

            string statsStr =
                Utility::StrPrintf("GPU temperature by external sensor [%s]\n",
                                   pSubdev->GpuIdentStr().c_str());
            statsStr += Utility::StrPrintf("  Current temp : %.1f\n"
                                           "  Maximum temp : %.1f",
                                           m_ExtTempStatsList[gpuInst].last,
                                           m_ExtTempStatsList[gpuInst].max);
            if (m_ExpectedMinMax.find(gpuInst) != m_ExpectedMinMax.end())
                statsStr += Utility::StrPrintf(" (Allowed: %.1f)",
                                               m_ExpectedMinMax[gpuInst].max);

            statsStr += Utility::StrPrintf("\n  Minimum temp : %.1f",
                                           m_ExtTempStatsList[gpuInst].min);
            if (m_ExpectedMinMax.find(gpuInst) != m_ExpectedMinMax.end())
                statsStr += Utility::StrPrintf(" (Allowed: %.1f)",
                                               m_ExpectedMinMax[gpuInst].min);
            Printf(GetStatsPri(), "%s\n\n", statsStr.c_str());
        }
        return RC::OK;
    }

private:
    map<UINT32, Range<FLOAT32> > m_ExtTempStatsList;
    map<UINT32, Range<FLOAT32> > m_ExpectedMinMax;
    vector<UINT32> m_AllowedTemp;
};

BgMonFactoryRegistrator<ExtTempMonitor> RegisterExtTempMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::EXT_TEMP_SENSOR
);
