/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * @file   limitsmonitor.cpp
 *
 * @brief  Background logger for power and thermal gpcclk limits
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfutil.h"
#include <limits>

namespace
{
    map<LwU32, string> limitToStr =
    {
        { LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1, "Power" },
        { LW2080_CTRL_PERF_LIMIT_THERM_POLICY_DOM_GRP_1, "Thermal" }
    };
}

class GpcLimitsMonitor : public PerGpuMonitor
{
public:
    explicit GpcLimitsMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Limits")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(2);
        statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1;
        statuses[1].limitId = LW2080_CTRL_PERF_LIMIT_THERM_POLICY_DOM_GRP_1;
        if (PerfUtil::GetRmPerfLimitsStatus(statuses, pSubdev) != RC::OK)
        {
            return descs;
        }

        for (const auto& status : statuses)
        {
            if (status.output.bEnabled)
            {
                descs.push_back({ limitToStr[status.limitId], "MHz", true, INT });
            }
        }

        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override;
    RC StatsChecker() override;

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses;
        statuses.reserve(m_GpcLimits[gpuInst].size());
        for (auto limit : m_GpcLimits[gpuInst])
        {
            LW2080_CTRL_PERF_LIMIT_GET_STATUS status = { };
            status.limitId = limit;
            statuses.push_back(status);
        }
        RC rc;
        CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, pSubdev));

        UINT32 lowestCapMHz = (std::numeric_limits<UINT32>::max)();
        for (const auto& status : statuses)
        {
            // kHz to MHz as well as a possible 2x to 1x colwersion here
            UINT32 frequencyMHz = status.output.value / 1000;
            if (pSubdev->GetPerf()->IsRmClockDomain(Gpu::ClkGpc2))
            {
                frequencyMHz /= 2;
            }

            pSample->Push(frequencyMHz);

            lowestCapMHz = min(lowestCapMHz, frequencyMHz);
        }

        const auto freq = m_Frequencies[gpuInst].find(lowestCapMHz);
        if (freq == m_Frequencies[gpuInst].end())
        {
            m_Frequencies[gpuInst][lowestCapMHz] = 1;
        }
        else
        {
            m_Frequencies[gpuInst][lowestCapMHz]++;
        }

        return RC::OK;
    }

private:
    // map<gpuInst, map<frequencyMHz, count>> - keeps track of all the current
    // lowest gpcclk limits and their counts (i.e. how many times did a
    // 1600MHz cap occur?)
    map<UINT32, map<UINT32, UINT32>> m_Frequencies;

    map<UINT32, vector<UINT32>> m_GpcLimits;
};

BgMonFactoryRegistrator<GpcLimitsMonitor> RegisterGpcLimitsFactory(
    BgMonitorFactories::GetInstance(),
    BgMonitorType::GPC_LIMITS
);

RC GpcLimitsMonitor::InitPerGpu(GpuSubdevice* pSubdev)
{
    RC rc;

    const UINT32 gpuInst = pSubdev->GetGpuInst();

    vector<LW2080_CTRL_PERF_LIMIT_GET_STATUS> statuses(2);
    statuses[0].limitId = LW2080_CTRL_PERF_LIMIT_PMU_DOM_GRP_1;
    statuses[1].limitId = LW2080_CTRL_PERF_LIMIT_THERM_POLICY_DOM_GRP_1;
    CHECK_RC(PerfUtil::GetRmPerfLimitsStatus(statuses, pSubdev));

    bool hasPwrOrThrmLimit = false;
    for (const auto& status : statuses)
    {
        if (status.output.bEnabled)
        {
            hasPwrOrThrmLimit = true;
            m_GpcLimits[gpuInst].push_back(status.limitId);
        }
    }

    if (!hasPwrOrThrmLimit)
    {
        Printf(Tee::PriError,
               "Gpu %u does not have power or thermal gpcclk limits\n",
               gpuInst);
        return RC::UNSUPPORTED_FUNCTION;
    }

    m_Frequencies[gpuInst];

    return RC::OK;
}

RC GpcLimitsMonitor::StatsChecker()
{
    for (auto* pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        if (!IsMonitoringGpu(gpuInst) || m_Frequencies[gpuInst].empty())
        {
            continue;
        }

        UINT32 minFreqMHz = (std::numeric_limits<UINT32>::max)();
        UINT32 minCount = 0;
        UINT32 modeFreqMHz = 0;
        UINT32 modeCount = 0;

        UINT64 runningTotal = 0;
        UINT32 numSamples = 0;

        for (const auto& freq : m_Frequencies[gpuInst])
        {
            if (freq.first < minFreqMHz)
            {
                minFreqMHz = freq.first;
                minCount = freq.second;
            }
            if (freq.second > modeCount)
            {
                modeFreqMHz = freq.first;
                modeCount = freq.second;
            }
            runningTotal += freq.first * freq.second;
            numSamples += freq.second;
        }
        FLOAT32 averageMHz = static_cast<FLOAT32>(runningTotal) / numSamples;

        Printf(GetStatsPri(), "gpcclk capping stats for %s:\n",
               pSubdev->GpuIdentStr().c_str());
        Printf(GetStatsPri(), "  Lowest gpcclk cap (MHz): %u (count=%u)\n",
               minFreqMHz, minCount);
        Printf(GetStatsPri(), "  Most frequent gpcclk cap (MHz): %u (count=%u)\n",
               modeFreqMHz, modeCount);
        Printf(GetStatsPri(), "  Mean gpcclk cap (MHz): %0.3f\n", averageMHz);
    }

    return RC::OK;
}
