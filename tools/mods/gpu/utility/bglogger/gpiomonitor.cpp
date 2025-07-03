/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/include/gpusbdev.h"

class GpioMonitor : public PerGpuMonitor
{
public:
    explicit GpioMonitor(BgMonitorType type) : PerGpuMonitor(type, "Gpio") {}

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        for (const auto gpio : m_Gpios)
        {
            const string monName = Utility::StrPrintf("GPIO%u", gpio);
            descs.push_back({ monName, "", false, INT });
        }

        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        for (const auto gpio : m_Gpios)
        {
            // TODO: Get RM to fix their define for LW2080_CTRL_GPIO_ACCESS_PORT_MAX,
            //       or better yet: use a RMCTRL from ctrl2080gpio.h to query this.
            if (gpio > 63)
            {
                Printf(Tee::PriError, "Invalid gpio specified: %u\n", gpio);
                return RC::ILWALID_ARGUMENT;
            }
            const GpuSubdevice::GpioDirection dir = pSubdev->GpioIsOutput(gpio) ?
                GpuSubdevice::GpioDirection::OUTPUT : GpuSubdevice::GpioDirection::INPUT;
            m_GpioInfos.emplace_back(gpuInst, gpio, dir);
        }
        return RC::OK;
    }

    RC HandleParamsPreInit(const vector<UINT32>& params, const set<UINT32>& monDevs) override
    {
        if (params.size() < 2)
        {
            Printf(Tee::PriError, "-bg_gpio requires at least one GPIO to monitor\n");
            return RC::ILWALID_ARGUMENT;
        }
        set<UINT32> gpios(params.begin() + 1, params.end());
        m_Gpios.assign(gpios.begin(), gpios.end());
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        for (const auto& gpioInfo : m_GpioInfos)
        {
            if (gpioInfo.GpuInst != gpuInst)
                continue;

            bool gpioVal;
            if (gpioInfo.Direction == GpuSubdevice::GpioDirection::INPUT)
            {
                gpioVal = pSubdev->GpioGetInput(gpioInfo.Num);
            }
            else
            {
                gpioVal = pSubdev->GpioGetOutput(gpioInfo.Num);
            }
            pSample->Push(static_cast<int>(gpioVal));
        }
        return RC::OK;
    }

private:
    struct GpioInfo
    {
        UINT32 GpuInst;
        UINT32 Num;
        GpuSubdevice::GpioDirection Direction;

        GpioInfo() = default;
        GpioInfo(UINT32 gpuInst, UINT32 gpioNum, GpuSubdevice::GpioDirection gpioDir) :
            GpuInst(gpuInst), Num(gpioNum), Direction(gpioDir) {}
    };
    vector<GpioInfo> m_GpioInfos = {};
    vector<UINT32> m_Gpios = {};
};

BgMonFactoryRegistrator<GpioMonitor> RegisterGpioMonitorFactory(
    BgMonitorFactories::GetInstance(), BgMonitorType::GPIO
);
