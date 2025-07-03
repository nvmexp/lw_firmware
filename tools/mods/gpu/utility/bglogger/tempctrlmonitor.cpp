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

/**
 * @file   tempctrlmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "inc/bytestream.h"
#include "core/include/mgrmgr.h"
#include "core/include/utility.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/tempctrl/tempctrl.h"

#include <numeric>                  // for std::accumulate()

class TempCtrlMonitor : public PerGpuMonitor
{
public:
    explicit TempCtrlMonitor(BgMonitorType type) :
        PerGpuMonitor(type, "TempCtrl"),
        m_CheckStats(false),
        m_MovingAvgWindowSize(0)
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        // For every controller ID
        for (UINT32 id : m_TempCtrlIds)
        {
            const TempCtrlPtr ctrl = pSubdev->GetTempCtrlViaId(id);
            if (ctrl == nullptr)
            {
                return descs;
            }
            const UINT32 numInsts = ctrl->GetNumInst();

            // For every instance of the given ID
            for (UINT32 i = 0; i < numInsts; i++)
            {
                // Appending the index of the instance to differentiate between them
                string idxStr = Utility::StrPrintf("%u-%s%d",
                                                   id,
                                                   ctrl->GetName().c_str(),
                                                   i);
                descs.push_back({ move(idxStr), ctrl->GetUnits(), true, INT });
            }
        }

        return descs;
    }

    RC HandleParamsPreInit(const vector<UINT32> &Param,
                           const set<UINT32>    &MonitorDevices) override
    {
        MASSERT(Param.size() > 2);

        m_CheckStats          = !Param[1];
        m_MovingAvgWindowSize = Param[2];
        for (UINT32 i = 3; i < Param.size(); i++)
        {
            // Add controller ids for logging
            m_TempCtrlIds.insert(Param[i]);
        }
        return OK;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        // For every controller ID
        for (UINT32 id : m_TempCtrlIds)
        {
            TempCtrlPtr ctrl = pSubdev->GetTempCtrlViaId(id);
            if (ctrl == nullptr)
            {
                Printf(Tee::PriError, "Temp controller id %u not valid on %s\n",
                                       id, pSubdev->GpuIdentStr().c_str());
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }

            const RC rc = ctrl->Initialize();
            if (rc != RC::OK)
            {
                Printf(Tee::PriError, "Failed to initialize temp ctrl %s on %s\n",
                                       ctrl->GetName().c_str(), pSubdev->GpuIdentStr().c_str());
                return rc;
            }
        }

        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        for (UINT32 id : m_TempCtrlIds)
        {
            TempCtrlPtr ctrl = pSubdev->GetTempCtrlViaId(id);
            for (UINT32 i = 0; i < ctrl->GetNumInst(); i++)
            {
                UINT32 data = 0;
                RC rc;
                CHECK_RC(ctrl->GetTempCtrlVal(&data, i));

                pSample->Push(data);

                if (m_CheckStats)
                {
                    const UINT32 key = GetStatsDataMapKey(gpuInst, id, i);
                    // TODO Don't accumulate this forever!!!
                    m_StatsData[key].push_back(data);
                }
            }
        }

        return RC::OK;
    }

    RC StatsChecker() override
    {
        if (!m_CheckStats)
            return OK;

        GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
        for (GpuSubdevice *pSubdev = pGpuDevMgr->GetFirstGpu(); pSubdev != NULL;
                pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
        {
            UINT32 devInst = pSubdev->GetGpuInst();
            if (!IsMonitoringGpu(devInst))
                continue;

            for (UINT32 id : m_TempCtrlIds)
            {
                TempCtrlPtr ctrl = pSubdev->GetTempCtrlViaId(id);
                UINT32 allowedMin = ctrl->GetMin();
                UINT32 allowedMax = ctrl->GetMax();

                for (UINT32 i = 0; i < ctrl->GetNumInst(); i++)
                {
                    UINT32 key = GetStatsDataMapKey(devInst, id, i);
                    const auto &samples = m_StatsData[key];
                    UINT32 size = UINT32(samples.size());
                    if (size == 0)
                    {
                        Printf(Tee::PriWarn, "No readings found for Tempctrl %s (ID %u)\n",
                                              ctrl->GetName().c_str(), id);
                        continue;
                    }

                    UINT32 endIdx = size - m_MovingAvgWindowSize;
                    // Run atleast once if total samples are insufficient
                    if (endIdx <= 0)
                        endIdx = 1;

                    for (UINT32 sampleIdx = 0; sampleIdx < endIdx; sampleIdx++)
                    {
                        // Averages samples[i] to samples[i + movingAvgWindow]
                        UINT32 avgVal = GetAvg(samples, size, sampleIdx, m_MovingAvgWindowSize);
                        if ((avgVal < allowedMin) || (avgVal > allowedMax))
                        {
                            Printf(Tee::PriError, "Tempctrl %s (ID %u) readings out of range\n"
                                                  "Moving avg %u out of allowed range [%u, %u]\n",
                                                   ctrl->GetName().c_str(), id,
                                                   avgVal, allowedMin, allowedMax);
                            return RC::EXCEEDED_EXPECTED_THRESHOLD;
                        }
                    }
                }
            }
        }
        return OK;
    }

private:
    static UINT32 GetAvg
    (
        const vector<UINT32> &samples,
        UINT32 size,
        UINT32 startIndex,
        UINT32 windowSize
    )
    {
        UINT32 endIndex = startIndex + windowSize;
        UINT32 numSamples = windowSize;
        // In case there are less samples available than needed, just avg over all available
        if (endIndex >= size)
        {
            endIndex = size;
            numSamples = endIndex - startIndex + 1;
        }

        UINT32 sum = std::accumulate(samples.begin() + startIndex, samples.begin() + endIndex, 0);
        return sum / numSamples;
    }

    static UINT32 GetStatsDataMapKey(UINT32 devInst, UINT32 ctrlId, UINT32 ctrlInst)
    {
        UINT32 key = (ctrlId * 100) + (ctrlInst * 10) + devInst;
        return key;
    }

    set<UINT32> m_TempCtrlIds;

    bool m_CheckStats;
    UINT32 m_MovingAvgWindowSize;

    // Key : (ControllerID * 100) + (ControllerInst * 10) + (Device Inst)
    // A nested structure with the above mapping would become too deep, which
    // is why the key is a combination of the identifying ids.
    map<UINT32, vector<UINT32>> m_StatsData;
};

BgMonFactoryRegistrator<TempCtrlMonitor> RegisterTempCtrlMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::TEMP_CTRL
);
