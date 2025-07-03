/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   bjttempmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "inc/bytestream.h"
#include "core/include/mgrmgr.h"
#include "core/include/utility.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/thermsub.h"

class BjtTempMonitor : public PerGpuMonitor
{
public:
    explicit BjtTempMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "BjtTemp")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        Thermal* const pTherm = pSubdev->GetThermal();

        map<string, ThermalSensors::TsenseBJTTempMap> bjtTemps;

        // Fetch all temps
        if (pTherm->GetChipTempsPerBJT(&bjtTemps) != RC::OK)
        {
            return descs;
        }

        string chanStr = "";
        for (const auto& bjtTempSensor : bjtTemps)
        {
            if (bjtTempSensor.first == "SCI")
            {
                for (const auto& tempSnap : bjtTempSensor.second)
                {
                    UINT32 bjtIdx = 0;
                    for (const auto& tsenseTemp : tempSnap.second)
                    {
                        // SCI temperature sensor doesn't support hotspot offset
                        if (tsenseTemp.first != GpuBJTSensors::ILWALID_TEMP)
                        {
                            chanStr = Utility::StrPrintf("%s %u -> BJT %u ",
                            bjtTempSensor.first.c_str(), tempSnap.first, bjtIdx);
                            descs.push_back({ chanStr, "C", true, FLOAT });
                        }
                        bjtIdx++;
                    }
                }
            }
            else
            {
                for (const auto& tempSnap : bjtTempSensor.second)
                {
                    UINT32 bjtIdx = 0;
                    for (const auto& tsenseTemp : tempSnap.second)
                    {
                        if (tsenseTemp.first != GpuBJTSensors::ILWALID_TEMP)
                        {
                            chanStr = Utility::StrPrintf("%s %u -> BJT %u ",
                            bjtTempSensor.first.c_str(), tempSnap.first, bjtIdx);
                            descs.push_back({ chanStr, "C", true, FLOAT });
                            chanStr = Utility::StrPrintf("%s %u -> BJT %u OFFSET",
                            bjtTempSensor.first.c_str(), tempSnap.first, bjtIdx);
                            descs.push_back({ chanStr, "C", true, FLOAT });
                        }
                        bjtIdx++;
                    }
                }
            }
        }
        return descs;
    }

    RC HandleParamsPreInit(const vector<UINT32> &Param,
                           const set<UINT32>    &MonitorDevices) override
    {
        // TODO: Handle specific GPC mask later if requested
        return RC::OK;
    }

    RC HandleParamsPostInit(const vector<UINT32>& params, const set<UINT32>&) override
    {
        // TODO : Handle this later
        // Nothing to be done for BJT
        return RC::OK;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        // Nothing to be done for BJT
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;

        map<string, ThermalSensors::TsenseBJTTempMap> bjtTemps;

        CHECK_RC(pSubdev->GetThermal()->GetChipTempsPerBJT(&bjtTemps));

        for (const auto& bjtTempSensor : bjtTemps)
        {
            if (bjtTempSensor.first == "SCI")
            {
                for (const auto& tempSnap : bjtTempSensor.second)
                {
                    // Parse Vector of temp pairs
                    for (const auto& tsenseTemp : tempSnap.second)
                    {
                        if (tsenseTemp.first != GpuBJTSensors::ILWALID_TEMP)
                        {
                            // Sample temperature
                            pSample->Push(tsenseTemp.first);
                        }
                    }
                }
            }
            else
            {
                for (const auto& tempSnap : bjtTempSensor.second)
                {
                    // Parse Vector of temp pairs
                    for (const auto& tsenseTemp : tempSnap.second)
                    {
                        if (tsenseTemp.first != GpuBJTSensors::ILWALID_TEMP)
                        {
                            // Sample temperature
                            pSample->Push(tsenseTemp.first);
                            // Sample temperature offset
                            pSample->Push(tsenseTemp.second);
                        }
                    }
                }
            }
        }

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, FLOAT, 2);
    }

    RC StatsChecker() override
    {
        // Nothing to be done for StatsChecker
        return RC::OK;
    }
};

BgMonFactoryRegistrator<BjtTempMonitor> RegisterBjtTempMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::BJT_TEMP_SENSOR
);
