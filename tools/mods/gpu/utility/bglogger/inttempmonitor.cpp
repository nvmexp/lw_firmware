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
 * @file   inttempmonitor.cpp
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
#include "gpu/perf/thermsub.h"

class IntTempMonitor : public PerGpuMonitor
{
public:
    explicit IntTempMonitor(BgMonitorType type)
        : PerGpuMonitor(type, "IntTemp")
    {
    }

    vector<SampleDesc> GetSampleDescs(GpuSubdevice* pSubdev) override
    {
        vector<SampleDesc> descs;

        Thermal* const pTherm = pSubdev->GetThermal();
        bool forceInt = false;
        pTherm->GetForceInt(&forceInt);
        // Unless user forces internal to be present, check with RM to see if we support it
        if (!forceInt)
        {
            bool intPresent = false;
            if (pTherm->GetInternalPresent(&intPresent) != RC::OK || !intPresent)
            {
                return descs;
            }
        }

        vector<ThermalSensors::ChannelTempData> channelTemps;
        if (m_ChannelIdx != (std::numeric_limits<UINT32>::max)())
        {
            // Monitor a single channel
            ThermalSensors::ChannelTempData tempData;
            if (pSubdev->GetThermal()->GetChipTempViaChannel(m_ChannelIdx, &tempData) != RC::OK)
            {
                return descs;
            }
            channelTemps.push_back(tempData);
        }
        else
        {
            // Monitor all channels
            if (pTherm->GetChipTempsViaChannels(&channelTemps) != RC::OK)
            {
                return descs;
            }
        }

        const UINT32 gpuInst = pSubdev->GetGpuInst();
        UINT32 descIdx = 0;

        for (const auto& temp : channelTemps)
        {
            string chanStr = TSM20Sensors::ProviderIndexToString(temp.devClass,
                                                                 temp.provIdx);
            // Append class specific data
            switch (temp.devClass)
            {
                case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADM1032:
                case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_MAX6649:
                case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP411:
                case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_ADT7461:
                case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_I2C_TMP451:
                    chanStr += Utility::StrPrintf(" (i2cDevIdx = %u)", temp.i2cDevIdx);
                    break;
                case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC:
                    chanStr += Utility::StrPrintf(" (tsoscIdx = %u)", temp.tsoscIdx);
                    break;
                case LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE:
                    chanStr += Utility::StrPrintf(" (siteIdx = %u)", temp.siteIdx);
                    break;
            }

            descs.push_back({ chanStr, "C", true, FLOAT });

            m_ChIdxToDescIdx[gpuInst][temp.chIdx] = descIdx++;
        }

        return descs;
    }

    RC HandleParamsPreInit(const vector<UINT32> &Param,
                           const set<UINT32>    &MonitorDevices) override
    {
        // Check if a specific channel index needs to be monitored
        // Need to handle this before GPU init
        m_ChannelIdx = Param[1];
        return OK;
    }

    RC HandleParamsPostInit(const vector<UINT32>& params, const set<UINT32>&) override
    {
        // Param[0]=print mask, Param[1]=should monitor hotspots
        // rest of the following elements are max, min value for each
        // gpu.  Min/Max are optional, but if present must be specified
        // for all GPUs and print mask must have been specified
        if ((params.size() > 2) &&
            (params.size() != (GetNumDevices() * 2 + 2)))
        {
            Printf(Tee::PriHigh, "Invalid Params passed to IntTemp bglogger\n");
            return RC::SOFTWARE_ERROR;
        }

        const auto& monitored = MonitoredGpus();
        UINT32 pos = 2;
        for (UINT32 gpuInst = 0; gpuInst < monitored.size() && pos + 1 < params.size(); gpuInst++)
        {
            if (monitored[gpuInst])
            {
                m_ExpectedMinMax[gpuInst].max = static_cast<FLOAT32>(params[pos++]);
                m_ExpectedMinMax[gpuInst].min = static_cast<FLOAT32>(params[pos++]);
            }
        }

        return RC::OK;
    }

    RC InitPerGpu(GpuSubdevice* pSubdev) override
    {
        RC rc;
        Thermal *pTherm = pSubdev->GetThermal();
        bool ForceInt = false;
        bool IntPresent = false;
        pTherm->GetForceInt(&ForceInt);
        //  unless user forces internal to be present, check with RM to see if we support it
        if (!ForceInt)
        {
            if ((OK != pTherm->GetInternalPresent(&IntPresent)) || !IntPresent)
            {
                Printf(Tee::PriHigh,
                       "IntTempMonitor : Failed to initialize on %s\n",
                       pSubdev->GpuIdentStr().c_str());
                return RC::THERMAL_SENSOR_ERROR;
            }
        }

        return RC::OK;
    }

    RC GatherData(GpuSubdevice* pSubdev, Sample* pSample) override
    {
        const UINT32 gpuInst = pSubdev->GetGpuInst();

        if (m_ChannelIdx != (std::numeric_limits<UINT32>::max)())
        {
            // Monitor a single channel
            ThermalSensors::ChannelTempData tempData;
            RC rc;
            CHECK_RC(pSubdev->GetThermal()->GetChipTempViaChannel(m_ChannelIdx, &tempData));

            pSample->Push(tempData.temp);

            m_SensorStatsList[gpuInst][tempData.chIdx].Update(tempData.temp);

            // If a channel has been specified, let statschecker
            // use the reading from that channel
            m_IntTempStatsList[gpuInst].Update(tempData.temp);
        }
        else
        {
            // Monitor all channels
            vector<ThermalSensors::ChannelTempData> channelTemps;
            RC rc;
            CHECK_RC(pSubdev->GetThermal()->GetChipTempsViaChannels(&channelTemps));

            for (const auto& lwrTemp : channelTemps)
            {
                pSample->Push(lwrTemp.temp);

                m_SensorStatsList[gpuInst][lwrTemp.chIdx].Update(lwrTemp.temp);
            }

            // If no specific channel has been specified, let statschecker
            // use the AVG temp (legacy behavior)
            FLOAT32 intTemp;
            CHECK_RC(pSubdev->GetThermal()->GetChipTempViaInt(&intTemp));

            m_IntTempStatsList[gpuInst].Update(intTemp);
        }

        return RC::OK;
    }

    RC GetPrintable(const Sample& sample, vector<string>* pStrings) override
    {
        return sample.FormatAsStrings(pStrings, FLOAT, 2);
    }

    RC StatsChecker() override
    {
        for (auto* pSubdev : *DevMgrMgr::d_GraphDevMgr)
        {
            const UINT32 gpuInst = pSubdev->GetGpuInst();
            if (!IsMonitoringGpu(gpuInst))
            {
                continue;
            }

            string statsStr =
                Utility::StrPrintf("GPU temperature by internal sensor [%s]\n",
                                   pSubdev->GpuIdentStr().c_str());
            statsStr += Utility::StrPrintf("  Current temp : %.1f\n"
                                           "  Maximum temp : %.1f",
                                           m_IntTempStatsList[gpuInst].last,
                                           m_IntTempStatsList[gpuInst].max);
            if (m_ExpectedMinMax.find(gpuInst) != m_ExpectedMinMax.end())
                statsStr += Utility::StrPrintf(" (Allowed: %.1f)",
                                               m_ExpectedMinMax[gpuInst].max);

            statsStr += Utility::StrPrintf("\n  Minimum temp : %.1f",
                                           m_IntTempStatsList[gpuInst].min);
            if (m_ExpectedMinMax.find(gpuInst) != m_ExpectedMinMax.end())
                statsStr += Utility::StrPrintf(" (Allowed: %.1f)",
                                               m_ExpectedMinMax[gpuInst].min);
            Printf(GetStatsPri(), "%s\n", statsStr.c_str());

            Printf(GetStatsPri(), " Internal sensor summary:\n");

            const auto descs = GetSampleDescs(pSubdev);

            for (const auto& chRangePair: m_SensorStatsList[gpuInst])
            {
                const UINT32 chIdx = chRangePair.first;
                const auto& range = chRangePair.second;

                const auto& chIdxMap = m_ChIdxToDescIdx[gpuInst];
                const auto idxIter = chIdxMap.find(chIdx);
                if (idxIter == chIdxMap.end())
                {
                    continue;
                }
                const UINT32 descIdx = idxIter->second;
                if (descIdx < descs.size())
                {
                    Printf(GetStatsPri(), "  %s - Current: %.1f;  Min/Max: %.1f/%.1f\n",
                        descs[descIdx].columnName.c_str(),
                        range.last, range.min, range.max);
                }
            }

            Printf(GetStatsPri(), "\n");
        }
        return RC::OK;
    }

private:
    map<UINT32, map<UINT32, Range<FLOAT32>>> m_SensorStatsList;
    map<UINT32, Range<FLOAT32>> m_IntTempStatsList;
    map<UINT32, Range<FLOAT32>> m_ExpectedMinMax;
    map<UINT32, map<UINT32, UINT32>> m_ChIdxToDescIdx;
    // Used when a particular channel needs to be monitored
    UINT32 m_ChannelIdx = (std::numeric_limits<UINT32>::max)();
};

BgMonFactoryRegistrator<IntTempMonitor> RegisterIntTempMonitorFactory(
    BgMonitorFactories::GetInstance()
  , BgMonitorType::INT_TEMP_SENSOR
);
