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
 * @file   powermonitor.cpp
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
#include "cheetah/include/tegrasocdeviceptr.h"
#include "cheetah/include/tegrasocdevice.h"

class PowerMonitor : public PerGpuMonitor
{
public:
    explicit PowerMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "Power")
    {
    }

    ~PowerMonitor() override
    {
        if (m_bSocPowerInit)
        {
            CheetAh::SocPtr()->EndPowerMeas();
        }
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        if (pSubdev->IsSOC())
        {
            vector<string> rails;
            if (CheetAh::SocPtr()->GetPowerMeasRailNames(&rails) != RC::OK)
            {
                return descs;
            }

            for (const auto& rail : rails)
            {
                descs.push_back({ rail, "mW", true, INT });
            }
        }
        else
        {
            bool hasTotalPwrChannel = false;

            Power* const pPower = pSubdev->GetPower();
            MASSERT(pPower);

            UINT32 channelMask = pPower->GetPowerChannelMask();

            while (channelMask)
            {
                const UINT32 sensorMask = channelMask & ~(channelMask - 1u);
                channelMask ^= sensorMask;

                const UINT32 powerRail  = pPower->GetPowerRail(sensorMask);
                const char*  railName   = pPower->PowerRailToStr(powerRail);

                if (powerRail == LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD)
                {
                    hasTotalPwrChannel = true;
                }

                if (powerRail == LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD
                    || m_PrintAllChannels)
                {
                    descs.push_back({ railName, "mW", true, INT });
                }
            }

            // Add a pseudo-sensor for whole-gpu sum if needed.
            if (!hasTotalPwrChannel)
            {
                const char* railName = pPower->PowerRailToStr(
                        LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD);
                descs.push_back({ railName, "mW", true, INT });
            }
        }

        return descs;
    }

    RC HandleParamsPreInit
    (
        const vector<UINT32> &param,
        const set<UINT32>    &monitorDevices
    ) override
    {
        const UINT32 requiredParams = 2;
        if (param.size() < requiredParams)
        {
            Printf(Tee::PriError, "Invalid Params passed to Power bglogger\n");
            return RC::SOFTWARE_ERROR;
        }

        m_PrintAllChannels = param[1] != 0;
        return OK;
    }

    RC HandleParamsPostInit(const vector<UINT32>& params, const set<UINT32>&) override
    {
        const auto& monitored = MonitoredGpus();
        UINT32 pos = 2;
        for (UINT32 gpuInst = 0; gpuInst < monitored.size() && pos < params.size(); gpuInst++)
        {
            if (!monitored[gpuInst])
            {
                continue;
            }

            UINT32 pwrMask = params[pos++];
            const INT32 numPwrBits = Utility::CountBits(pwrMask);
            if (params.size() < pos + numPwrBits * 2)
            {
                Printf(Tee::PriError,
                       "Too few parameters passed to power monitor for bit mask 0x%x.\n"
                       "Need at least %u parameters, but only %u provides\n",
                       pwrMask, pos + numPwrBits * 2, static_cast<unsigned>(params.size()));
                return RC::BAD_COMMAND_LINE_ARGUMENT;
            }
            while (pwrMask)
            {
                const UINT32 chId = pwrMask & ~(pwrMask - 1);
                pwrMask ^= chId;

                // Param vector has channelMask followed by min-max entry of
                // each channel, also channel mask was passed from JS with
                // bit 0 containing maxPwrRange of total board if present,
                // and as a result rest of channelId was left shifted by 1.
                // So here we right shift it by 1 to get actual channelid,
                // it will become 0 for total board if present.
                m_MaxPwrRange[gpuInst][chId >> 1].min = params[pos++];
                m_MaxPwrRange[gpuInst][chId >> 1].max = params[pos++];
            }
        }

        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        MASSERT(pSubdev);

        if (pSubdev->IsSOC())
        {
            vector<UINT32> measurements;
            RC rc;
            CHECK_RC(CheetAh::SocPtr()->GetPowerMeas(&measurements));
            for (auto meas : measurements)
            {
                pSample->Push(meas);
            }
        }
        else
        {
            LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS sample = { };
            if (pSubdev->GetPower()->GetPowerSensorSample(&sample) != RC::OK)
            {
                return RC::OK;
            }

            const UINT32 gpuInst = pSubdev->GetGpuInst();

            for (UINT32 channelIdx = 0;
                 channelIdx < LW2080_CTRL_PMGR_PWR_DEVICES_MAX_DEVICES; channelIdx++)
            {
                const UINT32 mask = 1U << channelIdx;
                if ((mask & sample.super.objMask.super.pData[0]) == 0 ||
                    // Skip channels that we're not printing
                    (!m_PrintAllChannels && (mask & m_TotalPowerMask[gpuInst]) == 0))
                {
                    continue;
                }

                const UINT32 value = sample.channelStatus[channelIdx].tuple.pwrmW;

                if (mask == m_TotalPwrChannel[gpuInst])
                {
                    PROFILE_PLOT("Power", static_cast<int64_t>(value) / 1000)
                }

                pSample->Push(value);
                m_PowerStatsList[gpuInst][mask].Update(value);
            }

            if (m_TotalPwrChannel[gpuInst] == NO_CHANNEL)
            {
                pSample->Push(static_cast<UINT32>(sample.totalGpuPowermW));
                m_PowerStatsList[gpuInst][DEFAULT_TOTAL_PWR_MASK].Update(sample.totalGpuPowermW);
                PROFILE_PLOT("Power", static_cast<int64_t>(sample.totalGpuPowermW) / 1000)
            }
        }

        return RC::OK;
    }

private:
    static constexpr UINT32 NO_CHANNEL = 0;
    map<UINT32, UINT32> m_TotalPwrChannel;

    RC InitPerGpu(GpuSubdevice* pSubdev) override;
    RC StatsChecker() override;

    static const UINT32 DEFAULT_TOTAL_PWR_MASK = 0;

    struct SensorInfo
    {
        UINT32 SensorMask;
        UINT32 Type;
        string PowerRail;
    };
    map<UINT32, vector<SensorInfo> > m_SensorPerGpu;
    bool m_bSocPowerInit = false;

    map<UINT32, UINT32> m_TotalPowerMask;
    map<UINT32, map<UINT32, Range<UINT32> > > m_PowerStatsList;
    map<UINT32, map<UINT32, Range<UINT32> > > m_MaxPwrRange;
    bool m_PrintAllChannels = false;
};

BgMonFactoryRegistrator<PowerMonitor> RegisterPowerMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::POWER_SENSOR
);

const UINT32 PowerMonitor::DEFAULT_TOTAL_PWR_MASK;

RC PowerMonitor::StatsChecker()
{
    for (auto* pSubdev : *DevMgrMgr::d_GraphDevMgr)
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();
        if (!IsMonitoringGpu(gpuInst))
        {
            continue;
        }

        Power *pPower = pSubdev->GetPower();
        Printf(GetStatsPri(), "Power stats (mW) [%s]\n",
                    pSubdev->GpuIdentStr().c_str());
        Printf(GetStatsPri(),
                "-------------------------------------------------------------------\n");
        Printf(GetStatsPri(),
                "        PowerRail        Current      Min      Max (Allowed range)\n");
        Printf(GetStatsPri(),
                "-------------------------------------------------------------------\n");
        for (map<UINT32, Range<UINT32> >::iterator itr = m_PowerStatsList[gpuInst].begin();
                itr != m_PowerStatsList[gpuInst].end(); itr++)
        {
            const char *pwrRailStr;
            if (itr->first == DEFAULT_TOTAL_PWR_MASK)
            {
                pwrRailStr = pPower->PowerRailToStr(
                        LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD);
            }
            else
            {
                pwrRailStr = pPower->PowerRailToStr(
                        pPower->GetPowerRail(itr->first));
            }
            Printf(GetStatsPri(), "%19s%13u%9u%9u",
                    pwrRailStr, itr->second.last, itr->second.min, itr->second.max);

            if (m_MaxPwrRange[gpuInst].find(itr->first) != m_MaxPwrRange[gpuInst].end())
            {

                Printf(GetStatsPri(), " (%u - %u)",
                        m_MaxPwrRange[gpuInst][itr->first].min,
                        m_MaxPwrRange[gpuInst][itr->first].max);
            }
            Printf(GetStatsPri(), "\n");
        }
        Printf(GetStatsPri(),
                "-------------------------------------------------------------------\n");
    }

    return OK;
}

//-----------------------------------------------------------------------------
RC PowerMonitor::InitPerGpu(GpuSubdevice* pSubdev)
{
    const UINT32 gpuInst = pSubdev->GetGpuInst();
    vector<SensorInfo> sensorList;

    if (pSubdev->IsSOC())
    {
        RC rc;
        CHECK_RC(CheetAh::SocPtr()->StartPowerMeas());
        m_bSocPowerInit = true;

        vector<string> rails;
        CHECK_RC(CheetAh::SocPtr()->GetPowerMeasRailNames(&rails));

        for (UINT32 i = 0; i < rails.size(); i++)
        {
            const UINT32 mask = 1 << i;
            const SensorInfo socSensor =
            {
                mask,
                LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION,
                rails[i]
            };
            sensorList.push_back(socSensor);

            if (i == 0 || rails[i] == "GPU")
            {
                m_TotalPowerMask[gpuInst] = mask;
            }
        }
    }
    else
    {
        PROFILE_PLOT_INIT("Power", Number)

        Power* pPower = pSubdev->GetPower();
        MASSERT(pPower);
        UINT32 channelMask = pPower->GetPowerChannelMask();
        m_TotalPwrChannel[gpuInst] = NO_CHANNEL;

        while (0 != channelMask)
        {
            INT32 channelBit = Utility::BitScanForward(channelMask);
            if (channelBit == -1)
                break;

            SensorInfo NewSensor;
            NewSensor.SensorMask = 1<<channelBit;
            NewSensor.Type = pPower->GetPowerSensorType(NewSensor.SensorMask);
            UINT32 PowerRail = pPower->GetPowerRail(NewSensor.SensorMask);
            NewSensor.PowerRail = pPower->PowerRailToStr(PowerRail);

            if (PowerRail == LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD)
            {
                m_TotalPwrChannel[gpuInst] = NewSensor.SensorMask;
                m_TotalPowerMask[gpuInst] = NewSensor.SensorMask;

                // Prioritize Total Power in the prints
                sensorList.insert(sensorList.begin(), NewSensor);
            }
            else
            {
                sensorList.push_back(NewSensor);
            }

            channelMask ^= NewSensor.SensorMask;
        }

        if (sensorList.empty())
        {
            Printf(Tee::PriError,
                "No power sensors detected (VBIOS Power Topology Table may be empty)\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        // Add a pseudo-sensor for whole-gpu sum if needed.
        if (m_TotalPwrChannel[gpuInst] == NO_CHANNEL)
        {
            SensorInfo sumSensor =
                {
                    DEFAULT_TOTAL_PWR_MASK,
                    LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_SUMMATION,
                    pPower->PowerRailToStr(LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD)
                };
            sensorList.insert(sensorList.begin(), sumSensor);

            m_TotalPowerMask[gpuInst] = DEFAULT_TOTAL_PWR_MASK;
        }
    }

    m_SensorPerGpu[gpuInst] = sensorList;

    return OK;
}
