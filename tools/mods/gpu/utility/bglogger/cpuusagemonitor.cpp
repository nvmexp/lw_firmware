/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation. All rights reserved. All information
 * contained herein is proprietary and confidential to LWPU Corporation. Any
 * use, reproduction, or disclosure without the written permission of LWPU
 * Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "bgmonitor.h"
#include "bgmonitorfactory.h"
#include "device/include/gp100testfixture.h"
#include "core/include/rc.h"
#include "core/include/types.h"
#include "core/include/utility.h"

class CpuUsageMonitor final: public BgMonitor
{
    public:
        explicit CpuUsageMonitor(BgMonitorType type)
            : BgMonitor(CPU_USAGE, "CPU usage")
        {
            MASSERT(type == CPU_USAGE);
        }

        vector<SampleDesc> GetSampleDescs(UINT32) override
        {
            vector<SampleDesc> descs;
            descs.push_back({ "CPU usage", "%", false, FLOAT });
            return descs;
        }

        RC GatherData(UINT32 devIdx, Sample* pSample) override
        {
            // It's not possible for this to be 0 under normal cirlwmstances
            MASSERT(pSample->timeDeltaNs);

            const double newValue = Xp::GetCpuUsageSec();

            // The number of seconds of CPU time that MODS process
            // has used in both user space and kernel space since the previous
            // sample.  This is a sum across all CPU cores, so this can be
            // more than the wall clock time that has passed.
            const double usageInSec = newValue - m_LastValue;

            // Colwert seconds to nanoseconds by multiplying by one billion,
            // then covert to percent by multiplying by 100 and dividing by
            // wall clock time that has passed since the last sample.
            const double percent = usageInSec * 100'000'000'000.0 / pSample->timeDeltaNs;

            pSample->Push(static_cast<FLOAT32>(percent));

            m_LastValue = newValue;

            return RC::OK;
        }

        RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
        {
            return sample.FormatAsStrings(pStrings, FLOAT, 1);
        }

        bool IsSubdevBased() override
        {
            return false;
        }

        bool IsLwSwitchBased() override
        {
            return false;
        }

        // This logger does not touch any GPUs, so it does not need to
        // be paused.
        bool IsPauseable() override
        {
            return false;
        }

        UINT32 GetNumDevices() override
        {
            return 1;
        }

        bool IsMonitoring(UINT32) override
        {
            return true;
        }

    private:
        RC InitializeImpl(const vector<UINT32>&, const set<UINT32>&) override
        {
            m_LastValue = Xp::GetCpuUsageSec();
            return RC::OK;
        }

        FLOAT64 m_LastValue = 0;
};

static BgMonFactoryRegistrator<CpuUsageMonitor> registerCpuUsageMonitor
(
    BgMonitorFactories::GetInstance(),
    BgMonitorType::CPU_USAGE
);
