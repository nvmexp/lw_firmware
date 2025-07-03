/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   pciespeedmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"

#include "core/include/mgrmgr.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/utility/bglogger/bgmonitorfactory.h"

class PcieSpeedMonitor : public PerGpuMonitor
{
public:
    explicit PcieSpeedMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "PcieSpeed")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "PcieSpeed", "Mbps", false, INT_ARRAY });
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        MASSERT(pSubdev);

        auto pPci = pSubdev->GetInterface<Pcie>();
        if (!pPci)
        {
            Printf(Tee::PriError, "This GPU is not on a PCI bus\n");
            return RC::UNSUPPORTED_HARDWARE_FEATURE;
        }

        constexpr UINT32 maxDepth = 16;
        INT64 speeds[maxDepth];

        speeds[0] = static_cast<INT64>(pPci->GetLinkSpeed(Pci::SpeedUnknown));
        UINT32 numSpeeds = 1;

        PexDevice *pPexDev = nullptr;
        RC rc;
        CHECK_RC(pPci->GetUpStreamInfo(&pPexDev));

        // Only monitor from the GPU perspective if no upstream device
        while ((pPexDev != nullptr) && !pPexDev->IsRoot() && (numSpeeds < maxDepth))
        {
            speeds[numSpeeds++] = static_cast<INT64>(pPexDev->GetUpStreamSpeed());
            pPexDev = pPexDev->GetUpStreamDev();
        }

        pSample->Push(&speeds[0], numSpeeds);

        return RC::OK;
    }
};

BgMonFactoryRegistrator<PcieSpeedMonitor> RegisterPcieSpeedMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::PCIE_SPEED
);

