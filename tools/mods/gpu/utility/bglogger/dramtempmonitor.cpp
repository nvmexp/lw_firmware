/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   dramtempmonitor.cpp
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

class DramTempMonitor : public PerGpuMonitor
{
public:
    explicit DramTempMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "DramTemp")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        Thermal* const pTherm = pSubdev->GetThermal();
        FrameBuffer* pFb = pSubdev->GetFB();
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        ThermalSensors::DramTempMap dramTemps;

        // Fetch all temps
        if (pTherm->GetDramTemps(&dramTemps) == RC::OK)
        {
            for (const auto& entry : dramTemps)
            {
                const string descStr =
                    Utility::StrPrintf("%c%1d PC%1d%s",
                                       pFb->VirtualFbioToLetter(std::get<0>(entry.first)),
                                       std::get<1>(entry.first),
                                       std::get<3>(entry.first),
                                       pFb->IsClamshell() ?
                                           Utility::StrPrintf(" (Chip %d)", std::get<2>(entry.first)).c_str() :
                                           "");

                Range<INT32> range;
                m_DramTempStats[gpuInst].push_back(range);
                m_DramTempDescs[gpuInst].push_back(descStr);

                descs.push_back({descStr, "C", true, INT});
            }
        }

        return descs;
    }

    RC HandleParamsPreInit(const vector<UINT32> &Param,
                           const set<UINT32>    &MonitorDevices) override
    {
        return RC::OK;
    }

    RC HandleParamsPostInit(const vector<UINT32>& params, const set<UINT32>&) override
    {
        return RC::OK;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;

        ThermalSensors::DramTempMap dramTemps;
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        CHECK_RC(pSubdev->GetThermal()->GetDramTemps(&dramTemps));
        UINT32 i = 0;
        for (const auto& entry : dramTemps)
        {
            pSample->Push(entry.second);
            m_DramTempStats[gpuInst][i++].Update(entry.second);
        }
        return rc;
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

            Printf(GetStatsPri(), "GPU temperature by DRAM sensor [%s]\n",
                    pSubdev->GpuIdentStr().c_str());
            for (UINT32 i = 0; i < m_DramTempDescs[gpuInst].size(); i++)
            {
                Printf(GetStatsPri(), " %s Current: %d, Max: %d, Min: %d\n",
                    m_DramTempDescs[gpuInst][i].c_str(),
                    m_DramTempStats[gpuInst][i].last,
                    m_DramTempStats[gpuInst][i].max,
                    m_DramTempStats[gpuInst][i].min);
            }
        }
        return RC::OK;
    }

private:
    map<UINT32, vector<string>> m_DramTempDescs;
    map<UINT32, vector<Range<INT32>>> m_DramTempStats;
};

BgMonFactoryRegistrator<DramTempMonitor> RegisterDramTempMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::DRAM_TEMP_SENSOR
);
