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

/**
* @file   thermalslowdownmonitor.cpp
*
* @brief  Background logging utility.
*
*/

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/thermsub.h"

class ThermalSlowdownMonitor : public PerGpuMonitor
{
public:
    explicit ThermalSlowdownMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Thermal Slowdown Ratio")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "Thermal Slowdown Ratio", "", false, FLOAT });
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        FLOAT32 slowdownRatio = 0;
        RC rc;
        CHECK_RC(pSubdev->GetThermal()->GetThermalSlowdownRatio(Gpu::ClkGpc, &slowdownRatio));

        pSample->Push(slowdownRatio);

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, FLOAT, 3);
    }
};

BgMonFactoryRegistrator<ThermalSlowdownMonitor> RegisterThermalSlowdownMonitorFactory(
    BgMonitorFactories::GetInstance()
    , BgMonitorType::THERMAL_SLOWDOWN
    );
