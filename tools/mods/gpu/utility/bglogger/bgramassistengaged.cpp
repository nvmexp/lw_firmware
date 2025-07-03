/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   bgramassistengaged.cpp
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
#include "gpu/include/gpumgr.h"

//-----------------------------------------------------------------------------
class RamAssistStatusMonitor : public PerGpuMonitor
{
public:
    explicit RamAssistStatusMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "RamAssistEngaged")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        set<Gpu::SplitVoltageDomain> domainSet;
        if (m_SplitVoltageDomains[gpuInst].empty())
        {
            domainSet.insert(Gpu::VOLTAGE_LOGIC);
        }
        else
        {
            domainSet = m_SplitVoltageDomains[gpuInst];
        }

        for (const auto& voltDomain : domainSet)
        {
            UINT32 adcDevMask = m_pAvfs->GetAdcDevMask(voltDomain);
            LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS adcStatuses = {};
            m_pAvfs->GetAdcDeviceStatusViaMask(adcDevMask, &adcStatuses);
            INT32 adcIdx = 0;
            while ((adcIdx = Utility::BitScanForward(adcDevMask)) >= 0)
            {
                adcDevMask ^= 1 << adcIdx;
                Avfs::AdcId adcId = m_pAvfs->TranslateDevIndexToAdcId(adcIdx);
                descs.push_back(
                {
                    PerfUtil::AdcIdToStr(adcId),
                    "", true, INT
                });
            }
        }
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        m_pPerf = pSubdev->GetPerf();
        m_pAvfs = pSubdev->GetAvfs();
        m_pVolt3x = pSubdev->GetVolt3x();

        if (!m_pPerf || !m_pPerf->IsPState30Supported() || !m_pPerf->HasPStates())
            return RC::UNSUPPORTED_FUNCTION;

        if (!m_pVolt3x || !m_pVolt3x->IsInitialized())
            return RC::UNSUPPORTED_FUNCTION;

        if (!m_pAvfs || !m_pAvfs->GetAdcDevMask())
            return RC::UNSUPPORTED_FUNCTION;

        m_SplitVoltageDomains[gpuInst] = m_pVolt3x->GetAvailableDomains();

        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        RC rc;
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        set<Gpu::SplitVoltageDomain> domainSet;
        if (m_SplitVoltageDomains[gpuInst].empty())
        {
            domainSet.insert(Gpu::VOLTAGE_LOGIC);
        }
        else
        {
            domainSet = m_SplitVoltageDomains[gpuInst];
        }

        for (auto voltDom : domainSet)
        {
            UINT32 adcDevMask = m_pAvfs->GetAdcDevMask(voltDom);
            LW2080_CTRL_CLK_ADC_DEVICES_STATUS_PARAMS adcStatuses = {};
            CHECK_RC(m_pAvfs->GetAdcDeviceStatusViaMask(adcDevMask, &adcStatuses));
            INT32 adcIdx = 0;
            while ((adcIdx = Utility::BitScanForward(adcDevMask)) >= 0)
            {
                adcDevMask ^= 1 << adcIdx;
                pSample->Push(adcStatuses.devices[adcIdx].statusData.adcV30.bRamAssistEngaged);
            }
        }
        return RC::OK;
    }

private:
    map<UINT32, set<Gpu::SplitVoltageDomain>> m_SplitVoltageDomains;
    Perf* m_pPerf = nullptr;
    Avfs* m_pAvfs = nullptr;
    Volt3x* m_pVolt3x = nullptr;
};

BgMonFactoryRegistrator<RamAssistStatusMonitor> RegisterRamAssistStatusMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::RAM_ASSIST_ENGAGED
);
