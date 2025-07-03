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
 * @file   fanmonitor.cpp
 *
 * @brief  Background logging utility.
 *
 */

#include "pergpumonitor.h"
#include "bgmonitorfactory.h"
#include "inc/bytestream.h"
#include "core/include/mgrmgr.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/perf/thermsub.h"

class FanMonitor : public PerGpuMonitor
{
public:
    explicit FanMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Fan RPM")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        vector<Range<UINT32>> fanRPMs;
        if (GetFanTachRPM(pSubdev, &fanRPMs) != RC::OK)
        {
            return descs;
        }

        for (UINT32 index = 0; index < fanRPMs.size(); index++)
        {
            string name = Utility::StrPrintf("Fan %u", index);
            descs.push_back({ move(name), "rpm", true, INT });
        }
        return descs;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        RC rc;
        UINT32 inst = pSubdev->GetGpuInst();
        Thermal* pTherm = pSubdev->GetThermal();
        bool RpmSense   = false;
        if ((OK != pTherm->GetIsFanRPMSenseSupported(&RpmSense)) || !RpmSense)
        {
            Printf(Tee::PriHigh,
                   "FanMonitor : Fan RPM Sense not supported on %s\n",
                   pSubdev->GpuIdentStr().c_str());
            return RC::FAN_NOT_WORKING;
        }

        vector<Range<UINT32>> fanRPMs;
        CHECK_RC(GetFanTachRPM(pSubdev, &fanRPMs));
        UINT32 index = 0;
        for (const auto& fanRPM : fanRPMs)
        {
            Range<UINT32> range;
            range.max = fanRPM.max;
            range.min = fanRPM.min;
            m_ExpectedRange[inst].push_back(range);
            m_FanStatsList[inst].push_back(range);
            index++;
        }
        return rc;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        vector<Range<UINT32>> fanRPMs;
        RC rc;
        CHECK_RC(GetFanTachRPM(pSubdev, &fanRPMs));

        const UINT32 gpuInst = pSubdev->GetGpuInst();
        UINT32 index = 0;

        for (const auto& fanRPM : fanRPMs)
        {
            pSample->Push(fanRPM.last);

            m_FanStatsList[gpuInst][index++].Update(fanRPM.last);
        }

        return RC::OK;
    }

    RC StatsChecker() override
    {
        GpuDevMgr* pGpuDevMgr = (GpuDevMgr*)DevMgrMgr::d_GraphDevMgr;
        for (GpuSubdevice *pSubdev = pGpuDevMgr->GetFirstGpu(); pSubdev != NULL;
                pSubdev = pGpuDevMgr->GetNextGpu(pSubdev))
        {
            const UINT32 gpuInst = pSubdev->GetGpuInst();
            if (!IsMonitoringGpu(gpuInst))
            {
                continue;
            }

            for (UINT32 index = 0; index < m_FanStatsList[gpuInst].size(); index++)
            {
                Printf(GetStatsPri(), "Fan %u Info [%s]\n",
                       index + 1, pSubdev->GpuIdentStr().c_str());
                Printf(GetStatsPri(), "  Current fan rpm : %u\n"
                        "  Maximum fan %u rpm : %u (Expected: %u)\n"
                        "  Minimum fan %u rpm : %u (Expected: %u)\n\n",
                        m_FanStatsList[gpuInst][index].last,
                        index + 1,
                        m_FanStatsList[gpuInst][index].max,
                        m_ExpectedRange[gpuInst][index].max,
                        index + 1,
                        m_FanStatsList[gpuInst][index].min,
                        m_ExpectedRange[gpuInst][index].min);
            }
        }
        return OK;
    }

private:
    RC GetFanTachRPM(GpuSubdevice *pSubdev, vector<Range<UINT32>> *pFanStats);

    map<UINT32, vector<Range<UINT32> > > m_FanStatsList;
    map<UINT32, vector<Range<UINT32> > > m_ExpectedRange;
};

RC FanMonitor::GetFanTachRPM(GpuSubdevice *pSubdev, vector<Range<UINT32>> *pFanStats)
{
    RC rc;
    Thermal* pTherm = pSubdev->GetThermal();
    UINT32 policyMask = 0;
    UINT32 coolerMask = 0;
    CHECK_RC(pTherm->GetFanPolicyMask(&policyMask));
    CHECK_RC(pTherm->GetFanCoolerMask(&coolerMask));

    // A non zero mask means Fan 2.0+ is configured
    if (policyMask > 0 && coolerMask > 0)
    {
        LW2080_CTRL_FAN_COOLER_INFO_PARAMS infoParams = {0};
        LW2080_CTRL_FAN_COOLER_STATUS_PARAMS statusParams = {};
        LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS ctrlParams = {0};
        ctrlParams.bDefault = false;
        CHECK_RC(pTherm->GetFanCoolerTable(&infoParams, &statusParams,
                 &ctrlParams));
        for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
        {
            if (!(BIT(index) & coolerMask))
            {
                continue;
            }
            LW2080_CTRL_FAN_COOLER_STATUS_DATA statusData =
                statusParams.coolers[index].data;
            LW2080_CTRL_FAN_COOLER_CONTROL_DATA ctrlData =
                ctrlParams.coolers[index].data;
            Range<UINT32> fanStats;
            switch(infoParams.coolers[index].type)
            {
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                    fanStats.Update(ctrlData.active.rpmMin);
                    fanStats.Update(ctrlData.active.rpmMax);
                    fanStats.Update(statusData.active.rpmLwrr);
                    break;

                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                    fanStats.Update(ctrlData.activePwm.active.rpmMin);
                    fanStats.Update(ctrlData.activePwm.active.rpmMax);
                    fanStats.Update(statusData.activePwm.active.rpmLwrr);
                    break;
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                    fanStats.Update(ctrlData.activePwmTachCorr.activePwm.active.rpmMin);
                    fanStats.Update(ctrlData.activePwmTachCorr.activePwm.active.rpmMax);
                    fanStats.Update(statusData.activePwmTachCorr.activePwm.active.rpmLwrr);
                    break;
            }
            pFanStats->push_back(fanStats);
        }
    }
    else //default Fan1.0 API
    {
        Range<UINT32> fanStats;
        CHECK_RC(pTherm->GetFanTachMinRPM(&fanStats.min));
        CHECK_RC(pTherm->GetFanTachMaxRPM(&fanStats.max));
        CHECK_RC(pTherm->GetFanTachRPM(&fanStats.last));
        pFanStats->push_back(fanStats);
    }
    return rc;
}

BgMonFactoryRegistrator<FanMonitor> RegisterFanMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::FAN_RPM_SENSOR
);
