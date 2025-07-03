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
 * @file   testnummonitor.cpp
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
#include "core/include/errormap.h"
#include <set>

//-----------------------------------------------------------------------------
class TestNumMonitor : public PerGpuMonitor
{
public:
    explicit TestNumMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "LwrrentTest(s)")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "LwrrentTest(s)", "", false, INT_ARRAY });
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        // TODO perhaps change GetRunningTests to return vector<INT64> in the first place
        set<INT32> runningTests;
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        ErrorMap::GetRunningTests(gpuInst, &runningTests);

        vector<INT64> testIds;
        for (const INT32 testId : runningTests)
        {
            testIds.push_back(testId);
        }

        pSample->Push(&testIds[0], static_cast<UINT32>(testIds.size()));

        return RC::OK;
    }
};

BgMonFactoryRegistrator<TestNumMonitor> RegisterTestNumMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::TEST_NUM
);
