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
 * @file   smbustempmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/thermsub.h"

class SmbusTempMonitor : public PerGpuMonitor
{
public:
    explicit SmbusTempMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "SmbusTemp")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "SmbusTemp", "C", true, INT });
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        Thermal *pTherm = pSubdev->GetThermal();
        if (pTherm->IsSmbusTempOffsetSet())
        {
            if (OK != pTherm->InitializeSmbusDevice())
            {
                Printf(Tee::PriHigh,
                       "SmbusTempMonitor : Failed to initialize on %s\n",
                       pSubdev->GpuIdentStr().c_str());
                return RC::THERMAL_SENSOR_ERROR;
            }
        }
        else
        {
            Printf(Tee::PriHigh,
                   "SmbusTempMonitor : SMBus offset not set on %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return RC::THERMAL_SENSOR_ERROR;
        }

        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        INT32 temp = -273;
        RC rc;
        CHECK_RC(pSubdev->GetThermal()->GetChipTempViaSmbus(&temp));

        pSample->Push(temp);

        return RC::OK;
    }
};

BgMonFactoryRegistrator<SmbusTempMonitor> RegisterSmbusTempMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::SMBUS_TEMP_SENSOR
);
