/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
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
    const char* LimitToStr(UINT32 value)
    {
        switch (value)
        {
            case LW2080_CTRL_PERF_PERF_LIMIT_ID_PWR_POLICY_GPC: return "GPCPower";
            case LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_GPC: return "GPCThermal";
            case LW2080_CTRL_PERF_PERF_LIMIT_ID_PWR_POLICY_DRAM: return "DRAMPower";
            case LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_DRAM: return "DRAMThermal";
            case LW2080_CTRL_PERF_PERF_LIMIT_ID_PWR_POLICY_XBAR: return "XBARPower";
            case LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_XBAR: return "XBARThermal";
        }
        MASSERT(!"Unhandled value");
        return "unknown";
    }
}

class MultiRailCWCLimitsMonitor : public PerGpuMonitor
{
public:
    explicit MultiRailCWCLimitsMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Limits")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        LW2080_CTRL_PERF_PERF_LIMITS_STATUS limitsStatus;
        limitsStatus.super = m_PerfLimits.super;
        if (PerfUtil::GetRmPerfPerfLimitsStatus(limitsStatus, pSubdev) != RC::OK)
        {
            return descs;
        }
        for (UINT32 limitIdx: m_MultiRailPerfLimits[gpuInst])
        {
            const auto& limitStatus = limitsStatus.limits[limitIdx];
            const char* limitName = LimitToStr(limitIdx);
            if (limitStatus.clientInput.type == LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_PSTATE_IDX)
                descs.push_back({ limitName, "PState", true, INT });
            else if (limitStatus.clientInput.type == LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_TYPE_FREQUENCY)
                descs.push_back({ limitName, "Frequency", true, INT });
            else
            {
                MASSERT(!"limitStatus type is not supported");
                descs.push_back({ limitName, "UNKNOWN", true, INT });
            }
        }

        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override;
    RC StatsChecker() override;

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        LW2080_CTRL_PERF_PERF_LIMITS_STATUS limitsStatus;
        limitsStatus.super = m_PerfLimits.super;
        CHECK_RC(PerfUtil::GetRmPerfPerfLimitsStatus(limitsStatus, pSubdev));
        vector<UINT32> limitCapMhz;
        limitCapMhz.reserve(m_MultiRailPerfLimits[gpuInst].size());
        std::fill(limitCapMhz.begin(), limitCapMhz.end(), std::numeric_limits<UINT32>::max());

        UINT32 limitIdx;
        LW2080_CTRL_BOARDOBJGRP_MASK_E255_FOR_EACH_INDEX(limitIdx, &m_PerfLimits.super.objMask.super)
            if (std::binary_search(m_MultiRailPerfLimits[gpuInst].begin(), 
                                   m_MultiRailPerfLimits[gpuInst].end(), limitIdx))
            {
                const auto& limitStatus = limitsStatus.limits[limitIdx];
                const auto& limitData = limitStatus.clientInput.data;
                UINT32 frequencyMHz;
                // For DRAM it is a pstate value and not frequency
                if (limitIdx == LW2080_CTRL_PERF_LIMIT_PWR_POLICY_DRAM ||
                    limitIdx == LW2080_CTRL_PERF_LIMIT_THERM_POLICY_DRAM)
                {
                    frequencyMHz = limitData.pstate.pstateIdx;
                }
                else
                {
                    frequencyMHz = limitData.freq.freqKHz / 1000;
                }
                if (pSubdev->GetPerf()->IsRmClockDomain(Gpu::ClkGpc2)
                    && (limitIdx == LW2080_CTRL_PERF_LIMIT_PWR_POLICY_GPC ||
                        limitIdx == LW2080_CTRL_PERF_LIMIT_THERM_POLICY_GPC))
                {
                    frequencyMHz /= 2;
                }

                pSample->Push(frequencyMHz);
                std::vector<UINT32>::iterator it = std::find(m_RequiredLimitIdx.begin(), m_RequiredLimitIdx.end(), limitIdx);
                auto index = std::distance(m_RequiredLimitIdx.begin(), it);
                limitCapMhz[index] = frequencyMHz;
            }
        LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END

        for (UINT32 limit: m_MultiRailPerfLimits[gpuInst])
        {
            std::vector<UINT32>::iterator it = std::find(m_RequiredLimitIdx.begin(), m_RequiredLimitIdx.end(), limit);
            auto index = std::distance(m_RequiredLimitIdx.begin(), it);
            LimitFreq limitFreq;
            limitFreq.GpuInst = gpuInst;
            limitFreq.Limit = limit;
            limitFreq.FreqMHz = limitCapMhz[index];
            m_Frequencies[limitFreq]++;
        }

        return RC::OK;
    }

private:
    // lowest gpcclk/xbarclk/dramclk limits and their counts (i.e. how many times did a
    // 1600MHz cap occur?)
    struct LimitFreq
    {
        UINT32 GpuInst = 0;
        UINT32 Limit = 0;
        UINT32 FreqMHz = 0;

        bool operator<(const LimitFreq& rhs) const
        {
            if (GpuInst != rhs.GpuInst)
            {
                return GpuInst < rhs.GpuInst;
            }
            if (Limit != rhs.Limit)
            {
                return Limit < rhs.Limit;
            }
            return FreqMHz < rhs.FreqMHz;
        }
    };

    map<LimitFreq, UINT32> m_Frequencies;
    map<UINT32, vector<UINT32>> m_MultiRailPerfLimits;
    LW2080_CTRL_PERF_PERF_LIMITS_INFO m_PerfLimits;
    vector<UINT32> m_RequiredLimitIdx;
};

BgMonFactoryRegistrator<MultiRailCWCLimitsMonitor> RegisterMultiRailCWCLimitsMonitorFactory(
    BgMonitorFactories::GetInstance(),
    BgMonitorType::MULTIRAILCWC_LIMITS
);

RC MultiRailCWCLimitsMonitor::InitPerGpu(GpuSubdevice* pSubdev)
{
    RC rc;
    const UINT32 gpuInst = pSubdev->GetGpuInst();
    CHECK_RC(PerfUtil::GetRmPerfPerfLimitsInfo(m_PerfLimits, pSubdev));
    LW2080_CTRL_PERF_PERF_LIMITS_STATUS limitsStatus;
    limitsStatus.super = m_PerfLimits.super;
    CHECK_RC(PerfUtil::GetRmPerfPerfLimitsStatus(limitsStatus, pSubdev));
    m_RequiredLimitIdx = 
    {
        LW2080_CTRL_PERF_PERF_LIMIT_ID_PWR_POLICY_GPC, LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_GPC, 
        LW2080_CTRL_PERF_PERF_LIMIT_ID_PWR_POLICY_DRAM, LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_DRAM,
        LW2080_CTRL_PERF_PERF_LIMIT_ID_PWR_POLICY_XBAR, LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_XBAR
    };
    UINT32 limitIdx;
    LW2080_CTRL_BOARDOBJGRP_MASK_E255_FOR_EACH_INDEX(limitIdx, &m_PerfLimits.super.objMask.super)
        const auto& limitInfo = m_PerfLimits.limits[limitIdx];
        const auto& limitStatus = limitsStatus.limits[limitIdx];
        bool bEnabled = false;

        if (limitInfo.type == LW2080_CTRL_PERF_PERF_LIMIT_TYPE_2X)
        {
            bEnabled = (limitStatus.data.v2x.input.type !=
                LW2080_CTRL_PERF_LIMIT_INPUT_TYPE_DISABLED);
        }
        else // PMU perf limits implementation
        {
            bEnabled = LW2080_CTRL_PERF_PERF_LIMIT_CLIENT_INPUT_ACTIVE(
                &limitStatus.data.pmu.clientInput);
        }
        if (!bEnabled)
        {
            continue;
        }
        if (std::find(m_RequiredLimitIdx.begin(), m_RequiredLimitIdx.end(), limitIdx) != m_RequiredLimitIdx.end())
        {
            m_MultiRailPerfLimits[gpuInst].push_back(limitIdx);
        }
    LW2080_CTRL_BOARDOBJGRP_MASK_FOR_EACH_INDEX_END
    if (m_MultiRailPerfLimits[gpuInst].empty())
    {
        Printf(Tee::PriError,
               "Gpu %u does not have any power or thermal clock limits\n",
               gpuInst);
        return RC::UNSUPPORTED_FUNCTION;
    }

    return RC::OK;
}

RC MultiRailCWCLimitsMonitor::StatsChecker()
{
    for (auto* pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        for (UINT32 limit : m_MultiRailPerfLimits[gpuInst])
        {
            UINT32 minFreqMHz = (std::numeric_limits<UINT32>::max)();
            UINT32 minCount = 0;
            UINT32 modeFreqMHz = 0;
            UINT32 modeCount = 0;

            UINT64 runningTotal = 0;
            UINT32 numSamples = 0;
            for (const auto& freq : m_Frequencies)
            {
                if (freq.first.Limit == limit && freq.first.GpuInst == gpuInst)
                {
                    if (freq.first.FreqMHz < minFreqMHz)
                    {
                        minFreqMHz = freq.first.FreqMHz;
                        minCount = freq.second;
                    }
                    if (freq.second > modeCount)
                    {
                        modeFreqMHz = freq.first.FreqMHz;
                        modeCount = freq.second;
                    }
                    runningTotal += freq.first.FreqMHz * freq.second;
                    numSamples += freq.second;
                }
            }
            FLOAT32 averageMHz = static_cast<FLOAT32>(runningTotal) / numSamples;
            const char* unit = "MHz";
            if (limit == LW2080_CTRL_PERF_PERF_LIMIT_ID_PWR_POLICY_DRAM ||
                limit == LW2080_CTRL_PERF_PERF_LIMIT_ID_THERM_POLICY_DRAM)
            {
                unit = "PState";
            }
            const char* limitStr = LimitToStr(limit);
            Printf(GetStatsPri(), "%s capping stats for %s:\n",
                limitStr, pSubdev->GpuIdentStr().c_str());
            Printf(GetStatsPri(), "  Lowest %s cap (%s): %u (count=%u)\n",
                limitStr, unit, minFreqMHz, minCount);
            Printf(GetStatsPri(), "  Most %s frequent cap (%s): %u (count=%u)\n",
                limitStr, unit, modeFreqMHz, modeCount);
            Printf(GetStatsPri(), "  Mean %s cap (%s): %0.3f\n",
                limitStr, unit, averageMHz);
        }
    }

    return RC::OK;
}
