/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2022 by LWPU Corporation.
 * All rights reserved. All information contained herein is proprietary and
 * confidential to LWPU Corporation. Any use, reproduction, or disclosure
 * without the written permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/abort.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/perfutil.h"
#include "gpu/tests/gputest.h"

class ClockMonitor final : public GpuTest
{
public:
    ClockMonitor();

    bool IsSupported() override;
    RC Setup() override;
    RC Run() override;
    RC Cleanup() override;
    void PrintJsProperties(Tee::Priority pri) override;
    RC InitFromJs() override;

    SETGET_PROP(KeepRunning, bool);
    SETGET_PROP(IsBgTest, bool);
    SETGET_PROP(SampleIntervalUs, UINT32);
    SETGET_PROP(ClearFaults, bool);

private:
    // Test arguments
    bool m_IsBgTest = false;
    bool m_KeepRunning = false;
    UINT32 m_SampleIntervalUs = 10000;
    bool m_ClearFaults = false;
    vector<Perf::ClkMonOverride> m_ClkMonOverrides;

    // Test state
    vector<Gpu::ClkDomain> m_ClkDomsToMonitor = {};
    Perf::ClkMonStatuses m_ClkMonStatuses = {};
    map<Perf::ClkMonStatus, UINT64> m_FaultCounts = {};
    Perf* m_pPerf = nullptr;
};

JS_CLASS_INHERIT(ClockMonitor, GpuTest, "Check all clock monitors for faults");

CLASS_PROP_READWRITE_FULL(ClockMonitor, KeepRunning, bool,
    "Keep this test running until false", 0, 0);
CLASS_PROP_READWRITE_FULL(ClockMonitor, IsBgTest, bool,
    "True if this test is running as a background test", 0, 0);
CLASS_PROP_READWRITE(ClockMonitor, SampleIntervalUs, UINT32,
    "How long to sleep between sampling clock monitor faults");
CLASS_PROP_READWRITE(ClockMonitor, ClearFaults, bool,
    "Clear clock monitor faults when they are seen");

ClockMonitor::ClockMonitor()
{
    SetName("ClockMonitor");
}

bool ClockMonitor::IsSupported()
{
    return !GetBoundGpuSubdevice()->GetPerf()->GetClkDomainsWithMonitors().empty();
}

RC ClockMonitor::Setup()
{
    RC rc;

    CHECK_RC(GpuTest::Setup());

    m_pPerf = GetBoundGpuSubdevice()->GetPerf();
    m_ClkDomsToMonitor = m_pPerf->GetClkDomainsWithMonitors();
    for (const auto& clkDom : m_ClkDomsToMonitor)
    {
        VerbosePrintf("Check clock monitoring for Clk = %s \n",
               PerfUtil::ClkDomainToStr(clkDom));
        if (clkDom == Gpu::ClkGpc)
        {
            const UINT32 numGpcs = GetBoundGpuSubdevice()->GetMaxGpcCount();
            for (UINT32 i = 0; i < numGpcs; i++)
            {
                m_ClkMonStatuses.emplace_back(clkDom, i);
            }
        }
        else if (clkDom == Gpu::ClkM)
        {
            const UINT32 numFbs = GetBoundGpuSubdevice()->GetMaxFbpCount();
            for (UINT32 i = 0; i < numFbs; i++)
            {
                m_ClkMonStatuses.emplace_back(clkDom, i);
            }
        }
        else
        {
            m_ClkMonStatuses.emplace_back(clkDom, 0);
        }
    }

    VerbosePrintf("Querying initial clock monitor statuses\n");
    CHECK_RC(m_pPerf->GetClkMonFaults(m_ClkDomsToMonitor, &m_ClkMonStatuses));
    for (auto& clkMonStatus : m_ClkMonStatuses)
    {
        clkMonStatus.PrintFaults(clkMonStatus.faultMask ? Tee::PriWarn : GetVerbosePrintPri());
        if (clkMonStatus.faultMask && m_ClearFaults)
        {
            VerbosePrintf("Clearing clock monitor faults for %s\n",
                PerfUtil::ClkDomainToStr(clkMonStatus.clkDom));
            clkMonStatus.faultMask = 0;
        }
    }

    if (!m_ClkMonOverrides.empty())
    {
        CHECK_RC(m_pPerf->SetClkMonThresholds(m_ClkMonOverrides));
    }

    if (m_ClearFaults)
    {
        if (!m_IsBgTest)
        {
            Printf(Tee::PriError, "Cannot use ClearFaults unless running as a BgTest\n");
            return RC::ILWALID_ARGUMENT;
        }

        // Clear any faults that were triggered by a previous test.
        // Also, we need a HULK license for the chip SKUs where the
        // FMON_STATUS_CLEAR register's priv level mask is level 3 write-protected.
        // Let's fail early if we do not have the correct permissions.
        CHECK_RC(m_pPerf->ClearClkMonFaults(m_ClkDomsToMonitor));
    }

    return rc;
}

RC ClockMonitor::Run()
{
    RC rc;

    do
    {
        VerbosePrintf("Querying for clock monitor faults\n");
        CHECK_RC(m_pPerf->GetClkMonFaults(m_ClkDomsToMonitor, &m_ClkMonStatuses));
        vector<Gpu::ClkDomain> clkDomsToClear;
        for (auto& clkMonStatus : m_ClkMonStatuses)
        {
            clkMonStatus.PrintFaults(clkMonStatus.faultMask ?
                Tee::PriError : GetVerbosePrintPri());

            if (clkMonStatus.faultMask)
            {
                clkDomsToClear.push_back(clkMonStatus.clkDom);
            }

            // For each fault in the mask, create a ClkMonStatus that contains
            // only one fault type, then increment m_FaultCounts. This way, we
            // can keep track of fault count per clock domain per GPC/FB partition
            // if GPC Clk or DRAM clk and fault type.
            INT32 bitIdx = 0;
            while ((bitIdx = Utility::BitScanForward(clkMonStatus.faultMask)) >= 0)
            {
                const UINT32 faultType = 1 << bitIdx;
                clkMonStatus.faultMask ^= faultType;
                Perf::ClkMonStatus tmpStatus(clkMonStatus.clkDom, faultType, clkMonStatus.index);
                m_FaultCounts[tmpStatus]++;
            }
        }

        if (!clkDomsToClear.empty())
        {
            if (m_ClearFaults)
            {
                VerbosePrintf("Clearing clock monitor faults\n");
                CHECK_RC(m_pPerf->ClearClkMonFaults(clkDomsToClear));
            }
            else
            {
                // We can't clear the faults to get useful fault counts, so bail early.
                return RC::CLOCK_MONITOR_FAULT;
            }
        }

        if (m_KeepRunning)
        {
            CHECK_RC(Abort::Check());
            Utility::SleepUS(m_SampleIntervalUs);
        }
    }
    while (m_KeepRunning);

    if (!m_FaultCounts.empty())
    {
        return RC::CLOCK_MONITOR_FAULT;
    }

    return rc;
}

RC ClockMonitor::Cleanup()
{
    StickyRC sticky;
    const Tee::Priority summaryPri = !m_FaultCounts.empty() ?
        Tee::PriNormal : GetVerbosePrintPri();
    Printf(summaryPri, "Clock monitor fault summary:\n");
    for (const auto& faultEntry: m_FaultCounts)
    {
       if (faultEntry.first.clkDom == Gpu::ClkGpc || faultEntry.first.clkDom == Gpu::ClkM)
       {
            Printf(summaryPri, " %s Partition%d %-10s fault count: %llu\n",
                PerfUtil::ClkDomainToStr(faultEntry.first.clkDom),
                faultEntry.first.index,
                Perf::ClkMonStatus::ClkMonFaultTypeToStr(faultEntry.first.faultMask),
                faultEntry.second);
       }
       else
       {
            Printf(summaryPri, " %-17s %-16s fault count: %llu\n",
                PerfUtil::ClkDomainToStr(faultEntry.first.clkDom),
                Perf::ClkMonStatus::ClkMonFaultTypeToStr(faultEntry.first.faultMask),
                faultEntry.second);
       }
    }

    if (!m_ClkMonOverrides.empty())
    {
        vector<Gpu::ClkDomain> clkDoms;
        clkDoms.reserve(m_ClkMonOverrides.size());
        for (const auto& ov : m_ClkMonOverrides)
        {
            clkDoms.push_back(ov.clkDom);
        }
        sticky = m_pPerf->ClearClkMonThresholds(clkDoms);
    }

    m_ClkDomsToMonitor.clear();
    m_ClkMonStatuses.clear();
    m_FaultCounts.clear();
    m_ClkMonOverrides.clear();

    return sticky;
}

void ClockMonitor::PrintJsProperties(const Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "ClockMonitor Js Properties:\n");
    Printf(pri, "\tSampleIntervalUs:\t\t\t%u\n", m_SampleIntervalUs);
    Printf(pri, "\tClearFaults:\t\t\t%s\n", m_ClearFaults ? "true" : "false");
}

RC ClockMonitor::InitFromJs()
{
    RC rc;

    CHECK_RC(GpuTest::InitFromJs());

    JavaScriptPtr pJs;
    for (const auto overrideType : { Perf::ClkMonThreshold::LOW, Perf::ClkMonThreshold::HIGH })
    {
        const char* argName = (overrideType == Perf::ClkMonThreshold::LOW ?
            "LowThresholdOverrides" : "HighThresholdOverrides");
        JsArray jsOverrides;
        if (pJs->GetProperty(GetJSObject(), argName, &jsOverrides) != OK)
            continue;

        JSObject* pClkMonOvObj;
        UINT32 domain;
        FLOAT32 overrideVal;
        for (const auto& ov : jsOverrides)
        {
            CHECK_RC(pJs->FromJsval(ov, &pClkMonOvObj));
            CHECK_RC(pJs->GetProperty(pClkMonOvObj, "Domain", &domain));
            CHECK_RC(pJs->GetProperty(pClkMonOvObj, "Value", &overrideVal));
            if (overrideVal < 0.0f || overrideVal > 1.0f)
            {
                Printf(Tee::PriError,
                    "ClockMonitor threshold overrides must be between 0.0 and 1.0\n");
                return RC::ILWALID_ARGUMENT;
            }
            Perf::ClkMonOverride clkMonOverride;
            clkMonOverride.clkDom = static_cast<Gpu::ClkDomain>(domain);
            clkMonOverride.val = overrideVal;
            clkMonOverride.thresh = overrideType;
            m_ClkMonOverrides.push_back(clkMonOverride);
        }
    }

    return rc;
}

