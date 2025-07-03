/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpu/perf/perfprofiler.h"

#include "gpu/include/gpusbdev.h"
#include "gpu/perf/avfssub.h"
#include "gpu/perf/perfsub_30.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/voltsub.h"

RC PerfProfiler::Setup(GpuSubdevice* pGpuSub)
{
    MASSERT(pGpuSub);
    RC rc;

    m_pGpuSub = pGpuSub;
    m_pPerf = pGpuSub->GetPerf();
    m_pVolt3x = pGpuSub->GetVolt3x();

    for (UINT32 ii = 0; ii < Gpu::ClkDomain_NUM; ++ii)
    {
        Gpu::ClkDomain clkDom = static_cast<Gpu::ClkDomain>(ii);
        if (m_pGpuSub->HasDomain(clkDom) &&
            !PerfUtil::Is2xClockDomain(clkDom) &&
            m_pPerf->ClkDomainType(clkDom) != Perf::FIXED)
        {
            m_ClkSamples.emplace_back(clkDom);
        }
    }
    CHECK_RC(m_pPerf->GetClockSamples(&m_ClkSamples));
    for (const auto clkSamp : m_ClkSamples)
    {
        m_ClkProfile[clkSamp.Domain].lwrrFreqkHz = clkSamp.ActualkHz;
    }

    if (m_pVolt3x && m_pVolt3x->IsInitialized())
    {
        map<Gpu::SplitVoltageDomain, UINT32> voltagesuV;
        CHECK_RC(m_pVolt3x->GetLwrrentVoltagesuV(voltagesuV));
        for (const auto& voltInfo : voltagesuV)
        {
            m_VoltProfile[voltInfo.first].lwrrVoltuV = voltInfo.second;
        }
    }
    else
    {
        UINT32 voltmV;
        CHECK_RC(m_pPerf->GetCoreVoltageMv(&voltmV));
        m_VoltProfile[Gpu::VOLTAGE_LOGIC].lwrrVoltuV = voltmV * 1000;
    }

    UINT32 samplePState;
    CHECK_RC(m_pPerf->GetLwrrentPState(&samplePState));
    m_PrevPStateNum = samplePState;

    if (m_pPerf->IsPState30Supported())
    {
        CHECK_RC(FindAvailableFreqs());
    }

    return rc;
}

RC PerfProfiler::Cleanup()
{
    m_pGpuSub = nullptr;
    m_pPerf = nullptr;
    m_pVolt3x = nullptr;
    m_NumPerfSwitches = 0;
    m_PStateSwitchesCount.clear();
    m_PrevPStateNum = Perf::ILWALID_PSTATE;
    m_VoltProfile.clear();
    m_ClkProfile.clear();
    m_ClkSamples.clear();
    m_StartTickCount = 0;
    m_LastTickCount = 0;
    m_AnySwitchDetected = false;
    m_Freqs.clear();
    return OK;
}

RC PerfProfiler::Sample()
{
    RC rc;

    if (!m_StartTickCount)
    {
        m_StartTickCount = Xp::QueryPerformanceCounter();
        m_LastTickCount = m_StartTickCount;
    }
    else
    {
        m_LastTickCount = Xp::QueryPerformanceCounter();
    }

    // Sample clocks first as power-capping kicks in fairly often and can
    // change gpcclk readings
    CHECK_RC(SampleClocks());
    CHECK_RC(SampleVoltages());
    CHECK_RC(SamplePState());
    if (m_AnySwitchDetected)
    {
        m_NumPerfSwitches++;
        m_AnySwitchDetected = false;
    }

    return rc;
}

RC PerfProfiler::SamplePState()
{
    RC rc;

    UINT32 samplePState;
    CHECK_RC(m_pPerf->GetLwrrentPState(&samplePState));
    if (samplePState != m_PrevPStateNum)
    {
        m_AnySwitchDetected = true;
        pair<UINT32, UINT32> lwrrPStateSwitch(m_PrevPStateNum, samplePState);
        m_PStateSwitchesCount[lwrrPStateSwitch]++;
        m_PrevPStateNum = samplePState;
    }

    return rc;
}

RC PerfProfiler::SampleVoltages()
{
    RC rc;

    if (m_pVolt3x && m_pVolt3x->IsInitialized())
    {
        map<Gpu::SplitVoltageDomain, UINT32> voltagesuV;
        m_pVolt3x->GetLwrrentVoltagesuV(voltagesuV);
        for (const auto& sampleuV : voltagesuV)
        {
            VoltProfile& voltProfile = m_VoltProfile[sampleuV.first];
            if (sampleuV.second != voltProfile.lwrrVoltuV)
            {
                m_AnySwitchDetected = true;
                voltProfile.numSwitches++;
                if (sampleuV.second > voltProfile.lwrrVoltuV)
                {
                    voltProfile.sumDeltauV += sampleuV.second - voltProfile.lwrrVoltuV;
                }
                else
                {
                    voltProfile.sumDeltauV += voltProfile.lwrrVoltuV - sampleuV.second;
                }
                voltProfile.lwrrVoltuV = sampleuV.second;
            }
        }
    }
    else
    {
        VoltProfile& voltProfile = m_VoltProfile[Gpu::VOLTAGE_LOGIC];
        UINT32 voltmV;
        CHECK_RC(m_pPerf->GetCoreVoltageMv(&voltmV));
        const UINT32 voltuV = 1000 * voltmV;
        if (voltuV != voltProfile.lwrrVoltuV)
        {
            m_AnySwitchDetected = true;
            voltProfile.numSwitches++;
            if (voltuV > voltProfile.lwrrVoltuV)
            {
                voltProfile.sumDeltauV += voltuV - voltProfile.lwrrVoltuV;
            }
            else
            {
                voltProfile.sumDeltauV += voltProfile.lwrrVoltuV - voltuV;
            }
            voltProfile.lwrrVoltuV = voltuV;
        }
    }

    return rc;
}

RC PerfProfiler::SampleClocks()
{
    RC rc;

    CHECK_RC(m_pPerf->GetClockSamples(&m_ClkSamples));

    for (const auto& clkSamp : m_ClkSamples)
    {
        auto& clkProfile = m_ClkProfile[clkSamp.Domain];
        if (clkSamp.ActualkHz != clkProfile.lwrrFreqkHz)
        {
            m_AnySwitchDetected = true;
            clkProfile.numSwitches++;
            clkProfile.sumDeltakHz += (clkSamp.ActualkHz > clkProfile.lwrrFreqkHz) ?
                clkSamp.ActualkHz - clkProfile.lwrrFreqkHz :
                clkProfile.lwrrFreqkHz - clkSamp.ActualkHz;
            clkProfile.lwrrFreqkHz = clkSamp.ActualkHz;
            const UINT32 freqMHz = (clkSamp.Domain == Gpu::ClkPexGen) ?
                clkSamp.ActualkHz : ColwertKiloToMega(clkSamp.ActualkHz);
            auto freq = std::find_if(m_Freqs.begin(), m_Freqs.end(), [&](const Freq& freq) -> bool
            {
                return freq.clkDom == clkSamp.Domain && freq.freqMHz == freqMHz;
            });
            if (freq != m_Freqs.end())
            {
                freq->count++;
            }
        }
        if (clkSamp.Domain == Gpu::ClkGpc && m_GpcInRangeMinMHz)
        {
            if (clkSamp.ActualkHz >= m_GpcInRangeMinMHz*1e3 &&
                clkSamp.ActualkHz <= m_GpcInRangeMaxMHz*1e3)
            {
                m_GpcInRangeFreq++;
            }
            else
            {
                m_GpcOutRangeFreq++;
            }
        }
    }

    return rc;
}

void PerfProfiler::PrintSample(Tee::Priority pri) const
{
    if (m_pVolt3x && m_pVolt3x->IsInitialized())
    {
        bool firstRail = true;
        for (const auto& voltProfile : m_VoltProfile)
        {
            Printf(pri, "%s%s=%uuV",
                firstRail ? "" : ",",
                PerfUtil::SplitVoltDomainToStr(voltProfile.first),
                voltProfile.second.lwrrVoltuV);
            firstRail = false;
        }
    }
    else
    {
        const auto& voltProfile = m_VoltProfile.find(Gpu::VOLTAGE_LOGIC);
        Printf(pri, "lwvdd=%uuV", voltProfile->second.lwrrVoltuV);
    }

    for (const auto& clkProfile : m_ClkProfile)
    {
        Printf(pri, ",%s=%u%s",
            PerfUtil::ClkDomainToStr(clkProfile.first),
            clkProfile.second.lwrrFreqkHz,
            (clkProfile.first == Gpu::ClkPexGen ? "" : "kHz"));
    }

    Printf(pri, ",pstate=%u\n", m_PrevPStateNum);
}

void PerfProfiler::PrintSummary(Tee::Priority pri) const
{
    const FLOAT64 elapsedSec = GetElapsedSeconds();

    Printf(pri, "Total PerfPoint switches: %u\n", m_NumPerfSwitches);
    Printf(pri, "PerfPoint switches/sec:   %.3f\n", GetSwitchesPerSecond());

    for (const auto& clkProfile : m_ClkProfile)
    {
        const Gpu::ClkDomain clkDom = clkProfile.first;
        Printf(pri, "%s stats:\n", PerfUtil::ClkDomainToStr(clkDom));
        Printf(pri, "  total switches:      %u\n", clkProfile.second.numSwitches);
        Printf(pri, "  switches/sec:        %.3f\n",
            elapsedSec ? clkProfile.second.numSwitches / elapsedSec : 0.0);
        if (clkDom != Gpu::ClkPexGen)
        {
            Printf(pri, "  average switch size: %llu kHz\n",
                clkProfile.second.numSwitches ?
                clkProfile.second.sumDeltakHz / clkProfile.second.numSwitches : 0);
        }

        if (m_Freqs.empty()) // PStates 2.X
            continue;

        UINT64 totalFreqs = 0;
        for (const auto& freq : m_Freqs)
        {
            if (freq.clkDom == clkDom)
            {
                totalFreqs++;
            }
        }
        Printf(pri, "  total POR freqs:     %llu\n", totalFreqs);

        UINT64 numUntestedFreqs = 0;
        string msg = "  untested POR freqs (MHz): [ ";
        for (const auto freq : m_Freqs)
        {
            if (freq.clkDom == clkDom && !freq.count)
            {
                msg += Utility::StrPrintf("%u ", freq.freqMHz);
                numUntestedFreqs++;
            }
        }
        Printf(pri, "%s]\n", msg.c_str());
        
        msg = "  tested freqs (MHz):  [ ";
        for (const auto freq : m_Freqs)
        {
            if (freq.clkDom == clkDom && freq.count)
            {
                msg += Utility::StrPrintf("%u(%u) ", freq.freqMHz, freq.count);
            }
        }
        Printf(pri, "%s]\n", msg.c_str());

        const UINT64 numTestedPOR = totalFreqs - numUntestedFreqs;
        Printf(pri, "  tested POR freqs:    %llu (%.1f%%)\n", numTestedPOR,
            totalFreqs ? static_cast<FLOAT32>(numTestedPOR) / totalFreqs * 100.0f : 0.0f);
    }

    for (const auto& voltProfile : m_VoltProfile)
    {
        Printf(pri, "%s stats:\n", PerfUtil::SplitVoltDomainToStr(voltProfile.first));
        Printf(pri, "  total switches:      %u\n", voltProfile.second.numSwitches);
        Printf(pri, "  switches/sec:        %.3f\n",
            elapsedSec ? voltProfile.second.numSwitches / elapsedSec : 0.0);
        Printf(pri, "  average switch size: %llu uV\n",
            voltProfile.second.numSwitches ?
            voltProfile.second.sumDeltauV / voltProfile.second.numSwitches : 0);
    }

    if (!m_PStateSwitchesCount.empty())
    {
        Printf(pri, "PState switches:\n");
        {
            Printf(pri, " From |  To  |  Count  \n");
            Printf(pri, "-----------------------\n");
        }
        UINT32 numPStateSwitches = 0;
        for (const auto& pstateSwitch : m_PStateSwitchesCount)
        {
            Printf(pri, "%4u  |%4u  |%6u\n",
                   pstateSwitch.first.first,
                   pstateSwitch.first.second,
                   pstateSwitch.second);
            numPStateSwitches += pstateSwitch.second;
        }
        Printf(pri, "Total pstate switches: %u\n", numPStateSwitches);
        Printf(pri, "PState switches/sec:   %.3f\n",
            elapsedSec ? numPStateSwitches / elapsedSec : 0.0);
    }
    if (m_GpcInRangeMinMHz != 0)
    {
        Printf(pri, "Total GPC frequencies in range: %llu\n", m_GpcInRangeFreq);
        Printf(pri, "Total GPC frequencies out range: %llu\n", m_GpcOutRangeFreq);
    }
}

FLOAT64 PerfProfiler::GetSwitchesPerSecond() const
{
    const FLOAT64 elapsedSec = GetElapsedSeconds();
    return elapsedSec ? m_NumPerfSwitches / elapsedSec : 0.0;
}

FLOAT64 PerfProfiler::GetElapsedSeconds() const
{
    return static_cast<FLOAT64>(m_LastTickCount - m_StartTickCount) /
        Xp::QueryPerformanceFrequency();
}

RC PerfProfiler::FindAvailableFreqs()
{
    RC rc;

    vector<UINT32> pstates;
    CHECK_RC(m_pPerf->GetAvailablePStates(&pstates));

    for (const auto& clkSample : m_ClkSamples)
    {
        const Gpu::ClkDomain clkDom = clkSample.Domain;
        vector<UINT32> freqskHz;
        CHECK_RC(m_pPerf->GetFrequencieskHz(clkDom, &freqskHz));
        for (const auto freqkHz : freqskHz)
        {
            for (const auto pstate : pstates)
            {
                const UINT32 rmDomain = PerfUtil::GpuClkDomainToCtrl2080Bit(clkDom);
                if (m_pPerf->IsClockInRange(pstate, rmDomain, freqkHz))
                {
                    m_Freqs.emplace_back(clkDom,
                        clkDom == Gpu::ClkPexGen ? freqkHz : ColwertKiloToMega(freqkHz));
                    break;
                }
            }
        }
    }

    return rc;
}

UINT32 PerfProfiler::ColwertKiloToMega(UINT32 kilo)
{
    // Round to the nearest MHz
    return (kilo + 500) / 1000;
}

void PerfProfiler::RecordVfSwitchLatencyTime(UINT64 modsVfSwitchTimeUs, UINT64 pmuVfSwitchTimeUs,
                                             UINT64 pmuBuildTimeUs, UINT64 pmuExecTimeUs)
{
    m_ModsVfSwitchTimeUs.insert(modsVfSwitchTimeUs);
    PmuVfSwitchTimeBreakDown pmuVfSwitchTime;
    pmuVfSwitchTime.totalPmuBuildTimeUs = pmuBuildTimeUs;
    pmuVfSwitchTime.totalPmuExecTimeUs = pmuExecTimeUs;
    pmuVfSwitchTime.totalPmuTimeUs = pmuVfSwitchTimeUs;
    m_PmuVfSwitchTimeUs.insert(pmuVfSwitchTime);
}

void PerfProfiler::SetMaxVfSwitchLatencyTime(UINT64 maxModsVfSwitchTimeUs, UINT64 maxPmuVfSwitchTimeUs)
{
    m_MaxModsVfSwitchTimeUs = maxModsVfSwitchTimeUs;
    m_MaxPmuVfSwitchTimeUs = maxPmuVfSwitchTimeUs;
}

bool PerfProfiler::PmuVfSwitchTimeBreakDown::operator<(const PmuVfSwitchTimeBreakDown& rhs) const
{
    return totalPmuTimeUs < rhs.totalPmuTimeUs;
}

RC PerfProfiler::CheckVfSwitchLatencyTime(Tee::Priority pri)
{
    RC rc;
    if (m_ModsVfSwitchTimeUs.size() == 0)
    {
        return RC::OK;
    }
    // 90th percentile latency of the MODS and PMU VF Switch Time Array
    UINT64 index = static_cast<UINT64>(max((m_ChoselwfSwitchPercentile/100.0f) * m_ModsVfSwitchTimeUs.size() - 1, 0.0f));
    UINT64 index1 = static_cast<UINT64>(max((m_ChoselwfSwitchPercentile/100.0f) * m_PmuVfSwitchTimeUs.size() - 1, 0.0f));
    auto iter = m_ModsVfSwitchTimeUs.begin();
    auto iter2 = m_PmuVfSwitchTimeUs.begin();
    std::advance(iter,  index);
    std::advance(iter2, index1);
    UINT64 modsVfSwitchLatencyTimeUs = *iter;
    UINT64 pmuVfSwitchLatencyTimeUs = iter2->totalPmuTimeUs;
    Printf(pri, "90th%c of our MODS and PMU VF Switch latency dataset are "
        "= {%lluUs, %lluUs}\n", '%', modsVfSwitchLatencyTimeUs, pmuVfSwitchLatencyTimeUs);
    Printf(pri, "90th%c  of PMU BuildTimeUs and PMU ExecTimeUs are "
        "= {%lluUs, %lluUs}\n", '%', iter2->totalPmuBuildTimeUs, iter2->totalPmuExecTimeUs);
    if (modsVfSwitchLatencyTimeUs > m_MaxModsVfSwitchTimeUs && m_MaxModsVfSwitchTimeUs != 0)
    {
        Printf(Tee::PriError,
            "MODS VF Switch took %lluUs (limit %lluUs)\n",
            modsVfSwitchLatencyTimeUs,
            m_MaxModsVfSwitchTimeUs);
        rc = RC::VF_SWITCH_TOO_SLOW;
    }

    if (pmuVfSwitchLatencyTimeUs > m_MaxPmuVfSwitchTimeUs && m_MaxPmuVfSwitchTimeUs != 0)
    {
        Printf(Tee::PriError,
            "PMU VF Switch took %lluUs (limit %lluUs)\n",
            pmuVfSwitchLatencyTimeUs,
            m_MaxPmuVfSwitchTimeUs);
        rc = RC::VF_SWITCH_TOO_SLOW;
    }
    return rc;
}
