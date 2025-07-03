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

#pragma once

#include "gpu/perf/perfsub.h"

class Volt3x;

class PerfProfiler
{
public:
    RC Setup(GpuSubdevice* pGpuSub);
    RC Cleanup();
    RC Sample();
    void PrintSample(Tee::Priority pri) const;
    void PrintSummary(Tee::Priority pri) const;
    FLOAT64 GetSwitchesPerSecond() const;
    void SetGPCRange(UINT64 gpcMinMHz, UINT64 gpcMaxMHz) {
        m_GpcInRangeMinMHz = gpcMinMHz; m_GpcInRangeMaxMHz = gpcMaxMHz; }
    // Record VF switch latency times and populate it
    void RecordVfSwitchLatencyTime(UINT64 modsVfSwitchTimeUs, UINT64 pmuVfSwitchTimeUs, 
                                   UINT64 pmuBuildTimeUs, UINT64 pmuExecTimeUs);
    // Set the maximum VF switch latency time for MODS and PMU 
    void SetMaxVfSwitchLatencyTime(UINT64 maxModsVfSwitchTimeUs, UINT64 maxPmuVfSwitchTimeUs);
    RC CheckVfSwitchLatencyTime(Tee::Priority pri);
    void SetVFSwitchLatencyPercentile(UINT64 percentile) {m_ChoselwfSwitchPercentile = percentile; }
private:
    RC SamplePState();
    RC SampleVoltages();
    RC SampleClocks();
    FLOAT64 GetElapsedSeconds() const;
    RC FindAvailableFreqs();
    static UINT32 ColwertKiloToMega(UINT32 kilo);

    GpuSubdevice* m_pGpuSub = nullptr;
    Perf* m_pPerf = nullptr;
    Volt3x* m_pVolt3x = nullptr;

    // Cumulative number of perf switches
    UINT32 m_NumPerfSwitches = 0;

    // Number of transitions between various pstates
    map<pair<UINT32, UINT32>, UINT32> m_PStateSwitchesCount;
    UINT32 m_PrevPStateNum = Perf::ILWALID_PSTATE;

    struct VoltProfile
    {
        UINT64 sumDeltauV = 0;  // sum of the absolute value of all voltage switches
        UINT32 lwrrVoltuV = 0;  // current voltage for this rail
        UINT32 numSwitches = 0; // total number of voltage switches
    };
    map<Gpu::SplitVoltageDomain, VoltProfile> m_VoltProfile;

    struct ClkProfile
    {
        UINT64 sumDeltakHz = 0; // sum of the absolute value of all frequency switches
        UINT32 lwrrFreqkHz = 0; // current frequency for this clock domain
        UINT32 numSwitches = 0; // total number of clock switches
    };
    map<Gpu::ClkDomain, ClkProfile> m_ClkProfile;
    vector<Perf::ClkSample> m_ClkSamples;

    UINT64 m_StartTickCount = 0;
    UINT64 m_LastTickCount = 0;

    bool m_AnySwitchDetected = false;
    struct Freq
    {
        Gpu::ClkDomain clkDom = Gpu::ClkUnkUnsp;
        UINT32 freqMHz = 0;
        UINT32 count = 0;
        Freq() = default;
        Freq(Gpu::ClkDomain c, UINT32 mhz) : clkDom(c), freqMHz(mhz) {}
    };
    vector<Freq> m_Freqs;

    struct PmuVfSwitchTimeBreakDown
    {
        UINT64 totalPmuBuildTimeUs = 0;
        UINT64 totalPmuExecTimeUs = 0;
        UINT64 totalPmuTimeUs = 0;

        bool operator<(const PmuVfSwitchTimeBreakDown& rhs) const;
    };
    UINT64 m_GpcInRangeMinMHz = 0;
    UINT64 m_GpcInRangeMaxMHz = 0;

    UINT64 m_GpcInRangeFreq = 0;
    UINT64 m_GpcOutRangeFreq = 0;

    set<UINT64> m_ModsVfSwitchTimeUs;
    set<PmuVfSwitchTimeBreakDown> m_PmuVfSwitchTimeUs;
    UINT64 m_MaxModsVfSwitchTimeUs = 0;
    UINT64 m_MaxPmuVfSwitchTimeUs = 0;
    UINT64 m_ChoselwfSwitchPercentile = 90;
};