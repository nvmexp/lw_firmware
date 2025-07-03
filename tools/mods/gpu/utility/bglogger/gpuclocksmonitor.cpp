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
 * @file   gpuclocksmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "inc/bytestream.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/perfsub.h"

class GpuClocksMonitor : public PerGpuMonitor
{
public:
    explicit GpuClocksMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Clocks")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        for (const auto& clk : m_Clocks)
        {
            const string& name  = clk.ClkName;
            const bool isPexGen = clk.ClkType == Gpu::ClkPexGen;
            const string unit   = isPexGen ? "" : "MHz";

            descs.push_back({ name,                unit, !isPexGen, FLOAT });
            descs.push_back({ name + "_ClkSrc",    "",   false,     STR });
            descs.push_back({ name + "_TargetPLL", unit, false,     FLOAT });
            descs.push_back({ name + "_ActualPLL", unit, false,     FLOAT });
        }

        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        PROFILE_PLOT_INIT("GpcClk", Number)
        return RC::OK;
    }

    RC HandleParamsPreInit(const vector<UINT32> &Param,
                           const set<UINT32>    &MonitorDevices) override
    {
        RC rc;
        set<UINT32> clkSet;
        pair<set<UINT32>::iterator, bool> IsUnique;
        for (UINT32 i = 1; i < Param.size(); i++)
        {
            // add unique clocks for logging
            IsUnique = clkSet.insert(Param[i]);
            if (IsUnique.second)
            {
                Gpu::ClkDomain Clk = static_cast<Gpu::ClkDomain>(Param[i]);
                m_Clocks.push_back(ClockInfo(Clk, PerfUtil::ClkDomainToStr(Clk)));
                m_ClkSamples.push_back(Perf::ClkSample(Clk));
            }
        }
        return rc;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;
        CHECK_RC(pSubdev->GetPerf()->GetClockSamples(&m_ClkSamples));

        for (const auto& clkSamp : m_ClkSamples)
        {
            // Unless it is PEX gen, divide the clock value in kHz by 1000
            // to obtain a value in MHz.
            const float toMHz = (clkSamp.Domain == Gpu::ClkPexGen) ? 1.0f : 0.001f;

            // Colwert to MHz
            pSample->Push(clkSamp.EffectivekHz * toMHz);
            pSample->Push(Gpu::ClkSrcToString(clkSamp.Src));
            pSample->Push(clkSamp.TargetkHz * toMHz);
            pSample->Push(clkSamp.ActualkHz * toMHz);

            if (clkSamp.Domain == Gpu::ClkGpc)
            {
                PROFILE_PLOT("GpcClk", static_cast<int64_t>(clkSamp.EffectivekHz) / 1000)
            }
        }

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, ANY, 2);
    }

protected:
    struct ClockInfo
    {
        string ClkName;
        Gpu::ClkDomain ClkType;

        ClockInfo() : ClkType(Gpu::ClkUnkUnsp) {}
        ClockInfo(Gpu::ClkDomain Type, string Name)
        {
            ClkType = Type;
            ClkName = Name;
        }
    };

    vector<ClockInfo> m_Clocks;

private:
    vector<Perf::ClkSample> m_ClkSamples;
};

BgMonFactoryRegistrator<GpuClocksMonitor> RegisterGpuClocksMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::GPU_CLOCKS
);

// Logs the effective clock frequencies per partition per domain
class GpuPartClocksMonitor : public GpuClocksMonitor
{
public:
    using GpuClocksMonitor::GpuClocksMonitor;

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        for (const auto& clk : m_Clocks)
        {
            const string&        name      = clk.ClkName;
            const Gpu::ClkDomain clkDomain = clk.ClkType;
            const bool           isPexGen  = clkDomain == Gpu::ClkPexGen;
            const string         unit      = isPexGen ? "" : "MHz";
            const UINT32         partMask  = pSubdev->GetPerf()->GetClockPartitionMask(clkDomain);

            if (partMask)
            {
                for (UINT32 partIdx = 0; partIdx < LW2080_CTRL_CLK_DOMAIN_PART_IDX_MAX; partIdx++)
                {
                    const UINT32 subMask = 1 << partIdx;
                    if (subMask & partMask)
                    {
                        descs.push_back(
                        {
                            name + Utility::StrPrintf(" partition %u", partIdx),
                            unit,
                            !isPexGen,
                            FLOAT
                        });
                    }
                }
            }
            else
            {
                descs.push_back({ name, unit, !isPexGen, FLOAT });
            }
        }

        return descs;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        for (const auto& clk : m_Clocks)
        {
            const Gpu::ClkDomain clkDomain = clk.ClkType;

            // Unless it is PEX gen, divide the clock value in Hz by one million
            // to obtain a value in MHz.
            const float toMHz = (clkDomain == Gpu::ClkPexGen) ? 1.0f : 0.000'001f;

            const UINT32 partitionMask = pSubdev->GetPerf()->GetClockPartitionMask(clkDomain);
            if (partitionMask)
            {
                vector<UINT64> clockVals;
                RC rc;
                CHECK_RC(pSubdev->GetPerf()->MeasureClockPartitions(clkDomain,
                                                                    partitionMask,
                                                                    &clockVals));
                for (auto clockVal : clockVals)
                {
                    const FLOAT32 sample = clockVal * toMHz;
                    pSample->Push(sample);
                }
            }
            else
            {
                UINT64 clockVal = 0;
                RC rc;
                CHECK_RC(pSubdev->GetPerf()->MeasureClock(clkDomain, &clockVal));

                const FLOAT32 sample = clockVal * toMHz;
                pSample->Push(sample);
            }
        }

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, FLOAT, 2);
    }
};

BgMonFactoryRegistrator<GpuPartClocksMonitor> RegisterGpuPartClocksMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::GPU_PART_CLOCKS
);
