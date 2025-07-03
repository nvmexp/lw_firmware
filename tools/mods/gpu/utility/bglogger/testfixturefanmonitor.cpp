/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2020 by LWPU Corporation. All rights reserved. All information
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

#include <algorithm>
#include <functional>
#include <map>

using Utility::StrPrintf;

class TestFixtureFanMonitor : public BgMonitor
{
public:
    explicit TestFixtureFanMonitor(BgMonitorType type)
        : BgMonitor(type, "Raiser RPM")
    {
        m_GP100TestFixture.GetIntakeFans(&m_IntakeFans);
        m_GP100TestFixture.GetOuttakeFans(&m_OuttakeFans);
    };

    vector<SampleDesc> GetSampleDescs(UINT32) override
    {
        vector<SampleDesc> descs;

        map<size_t, UINT32> fans;
        if (m_GP100TestFixture.GetTachometer(&fans) != RC::OK)
        {
            return descs;
        }

        for (const auto& fan : fans)
        {
            string name = Utility::StrPrintf("Fan %u", static_cast<unsigned>(fan.first));
            descs.push_back({ move(name), "rpm", false, INT });
        }

        return descs;
    }

    // Note: This function communicates over serial port, perf is unknown.
    // Secondly, it interferes with TestFixtureTemperatureMonitor.
    RC GatherData(UINT32 devIdx, Sample* pSample) override
    {
        RC rc;

        map<size_t, UINT32> fans;
        CHECK_RC(m_GP100TestFixture.GetTachometer(&fans));

        for (const auto& fan : fans)
        {
            pSample->Push(fan.second);
        }

        return RC::OK;
    }

    bool IsLwSwitchBased() override
    {
        return false;
    }

    bool IsSubdevBased() override
    {
        return false;
    }

    UINT32 GetNumDevices() override
    {
        return 1;
    }

    bool IsMonitoring(UINT32) override
    {
        return m_GP100TestFixture;
    }

private:
    RC InitializeImpl(const vector<UINT32> &, const set<UINT32> &) override
    {
        return RC::OK;
    }

    GP100TestFixture m_GP100TestFixture;

    vector<size_t> m_IntakeFans;
    vector<size_t> m_OuttakeFans;
};

BgMonFactoryRegistrator<TestFixtureFanMonitor> RegisterTestFixtureFanMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::TEST_FIXTURE_RPM
);
