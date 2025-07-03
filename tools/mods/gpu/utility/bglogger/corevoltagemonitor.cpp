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
 * @file   corevoltagemonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "inc/bytestream.h"
#include "core/include/utility.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/perfutil.h"
#include "gpu/perf/voltsub.h"

//-----------------------------------------------------------------------------
class VoltageMonitor : public PerGpuMonitor
{
public:
    explicit VoltageMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Voltage")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        if (m_SplitVoltageDomains[gpuInst].empty())
        {
            descs.push_back(
            {
                PerfUtil::SplitVoltDomainToStr(Gpu::VOLTAGE_LOGIC),
                "mV", true, FLOAT
            });
        }
        else
        {
            for (const auto& voltDomain : m_SplitVoltageDomains[gpuInst])
            {
                if (voltDomain == Gpu::VOLTAGE_LOGIC ||
                    voltDomain == Gpu::VOLTAGE_SRAM ||
                    voltDomain == Gpu::VOLTAGE_MSVDD)
                {
                    descs.push_back(
                    {
                        PerfUtil::SplitVoltDomainToStr(voltDomain),
                        "mV", true, FLOAT
                    });
                }
            }
        }
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        // Check for a spilt-rail support
        if (pSubdev->GetVolt3x()->IsInitialized())
        {
            m_SplitVoltageDomains[gpuInst] = pSubdev->GetVolt3x()->GetAvailableDomains();
        }

        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        if (m_SplitVoltageDomains[gpuInst].empty())
        {
            // Legacy API
            UINT32 coreVoltage = 0;
            RC rc;
            CHECK_RC(pSubdev->GetPerf()->GetCoreVoltageMv(&coreVoltage));

            pSample->Push(static_cast<FLOAT32>(coreVoltage));
        }
        else
        {
            // Volt3X API
            map<Gpu::SplitVoltageDomain, UINT32> voltages;
            RC rc;
            CHECK_RC(pSubdev->GetVolt3x()->GetLwrrentVoltagesuV(voltages));

            const auto& domainSet = m_SplitVoltageDomains[gpuInst];

            for (auto voltDom : domainSet)
            {
                if (voltDom == Gpu::VOLTAGE_LOGIC ||
                    voltDom == Gpu::VOLTAGE_SRAM ||
                    voltDom == Gpu::VOLTAGE_MSVDD)
                {
                    pSample->Push(voltages[voltDom] / 1000.0f);
                }
            }
        }

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        const int precision = m_SplitVoltageDomains[sample.devIdx].empty() ? 0 : 3;
        return sample.FormatAsStrings(pStrings, FLOAT, precision);
    }

private:
    map<UINT32, set<Gpu::SplitVoltageDomain>> m_SplitVoltageDomains;
};

BgMonFactoryRegistrator<VoltageMonitor> RegisterVoltageMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::GPU_VOLTAGE
);
