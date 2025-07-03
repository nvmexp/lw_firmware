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
 * @file   soctempmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/thermsub.h"
#include "math.h"
#include "core/include/platform.h"

// This monitors all available SOC temperatures, lwrrently CPU, GPU,
// PLLX, & MEM.
class SocTempMonitor : public PerGpuMonitor
{
public:
    explicit SocTempMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "SocTemp")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "SocTemp", "C", true, FLOAT });
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        if (pSubdev->IsSOC())
        {
            FLOAT32 dummy;
            if (pSubdev->GetThermal()->GetSocGpuTemp(&dummy) != OK)
            {
                m_SupportsGpuTemp = false;
                Printf(Tee::PriHigh,
                       "WARNING: Cannot read GPU temperature\n");
            }
        }
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        if (!m_SupportsGpuTemp)
        {
            return RC::UNSUPPORTED_SYSTEM_CONFIG;
        }

        FLOAT32 gpuTemp = -273.15f;
        RC rc;
        CHECK_RC(pSubdev->GetThermal()->GetSocGpuTemp(&gpuTemp));

        pSample->Push(gpuTemp);

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, FLOAT, 1);
    }

private:
    bool m_SupportsGpuTemp = true;
};

BgMonFactoryRegistrator<SocTempMonitor> RegisterSocTempMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::SOC_TEMP_SENSOR
);
