/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   lwrrentmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */
#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/include/gpusbdev.h"

#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "core/include/utility.h"
#include "gpu/perf/pwrsub.h"

class LwrrentMonitor : public PerGpuMonitor
{
public:
    explicit LwrrentMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Current")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        Power* const pPower = pSubdev->GetPower();
        UINT32 channelMask = pPower->GetPowerChannelMask();
        while (channelMask)
        {
            const UINT32 sensorMask = channelMask & ~(channelMask - 1u);
            channelMask ^= sensorMask;

            const char* powerRailStr = pPower->PowerRailToStr(pPower->GetPowerRail(sensorMask));

            descs.push_back({ powerRailStr, "mA", true, INT });
        }
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        Power* pPower = pSubdev->GetPower();
        MASSERT(pPower);

        const UINT32 channelMask = pPower->GetPowerChannelMask();
        if (0 == channelMask)
        {
            Printf(Tee::PriNormal, "No sensors detected\n");
            return RC::SOFTWARE_ERROR;
        }

        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS sample = { };
        RC rc;
        CHECK_RC(pSubdev->GetPower()->GetPowerSensorSample(&sample));

        for (UINT32 channelIdx = 0;
             channelIdx < LW2080_CTRL_PMGR_PWR_DEVICES_MAX_DEVICES;
             channelIdx++)
        {
            const UINT32 channelBit = (1 << channelIdx);
            if (channelBit & sample.super.objMask.super.pData[0])
            {
                pSample->Push(sample.channelStatus[channelIdx].tuplePolled.lwrrmA);
            }
        }

        return RC::OK;
    }
};

BgMonFactoryRegistrator<LwrrentMonitor> RegisterLwrrentMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::LWRRENT_SENSOR
);
