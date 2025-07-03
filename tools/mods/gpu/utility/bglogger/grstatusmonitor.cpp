/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "gpu/include/gpusbdev.h"

class GrStatusMonitor : public PerGpuMonitor
{
public:
    explicit GrStatusMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Graphics Status")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;
        descs.push_back({ "", "", false, INT });
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        const UINT32 status = pSubdev->GetEngineStatus(GpuSubdevice::GR_ENGINE);

        pSample->Push(status);

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        for (const Sample::SampleItem& item : sample)
        {
            MASSERT(item.type == INT);
            MASSERT(item.numElems == 1);
            pStrings->push_back(Utility::StrPrintf("0x%08llx", item.value.intValue[0]));
        }

        return RC::OK;
    }
};

static BgMonFactoryRegistrator<GrStatusMonitor> RegisterGrStatusMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::GR_STATUS
);
