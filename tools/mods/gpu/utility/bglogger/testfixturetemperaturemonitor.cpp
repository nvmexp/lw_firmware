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

#include <set>
#include <vector>

#include "bgmonitor.h"
#include "bgmonitorfactory.h"
#include "device/include/gp100testfixture.h"
#include "core/include/rc.h"
#include "core/include/types.h"

#include "core/include/xp.h"

class TestFixtureTemperatureMonitor : public BgMonitor
{
public:
    explicit TestFixtureTemperatureMonitor(BgMonitorType type)
        : BgMonitor(type, "Air Temp")
    {
    }

    vector<SampleDesc> GetSampleDescs(UINT32) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "Air Temp", "C", false, INT });
        return descs;
    }

    RC GatherData(UINT32 devIdx, Sample* pSample) override
    {
        // UINT32 is a bug in GP100TestFixture::GetTemperature() interface,
        // it should be a signed integer instead!
        UINT32 temperature = 0;

        RC rc;
        CHECK_RC(m_GP100TestFixture.GetTemperature(&temperature));

        pSample->Push(temperature);

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
};

BgMonFactoryRegistrator<TestFixtureTemperatureMonitor> RegisterUsbTemperatureMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::TEST_FIXTURE_TEMPERATURE
);
