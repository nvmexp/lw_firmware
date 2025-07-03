/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2022 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/utility.h"
#include "core/include/abort.h"
#include "ctrl/ctrl2080/ctrl2080thermal.h"
#include "ctrl/ctrl2080.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/pwrsub.h"
#include "gpu/perf/thermsub.h"
#include "thermresgpu.h"

// Exposed JS properties
JS_CLASS_INHERIT(ThermalResGpu, ThermalRes, "Thermal resistance GPU test");

CLASS_PROP_READWRITE(ThermalResGpu, IdlePerfPoint, JSObject*,
                    "JSObject: The PerfPoint during the idle phase");

CLASS_PROP_READWRITE(ThermalResGpu, LoadPerfPoint, JSObject*,
                    "JSObject: The PerfPoint during the load phase");

////////////////////////////////////////////////////////////////////////////////////////////////

bool ThermalResGpu::IsSupported()
{
    GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
    vector<UINT32> pStates;

    // We rely on the RM APIs to return power/thermal samples.
    // So this test is not supported on CheetAh
    if (pGpuSubdevice->IsSOC())
    {
        Printf(Tee::PriLow, "Skipping %s because it is not supported on CheetAh\n",
            GetName().c_str());
        return false;
    }

    if (RC::OK != pGpuSubdevice->GetPerf()->GetAvailablePStates(&pStates) ||
        pStates.size() == 0)
    {
        Printf(Tee::PriLow, "Skipping %s because there are no pstates\n",
            GetName().c_str());
        return false;
    }
    if (!pGpuSubdevice->GetPerf()->IsPState30Supported())
    {
        Printf(Tee::PriLow, "Skipping %s because PStates 3.0+ is not available\n",
            GetName().c_str());
        return false;
    }
    if (!pGpuSubdevice->GetPower()->GetNumPowerSensors())
    {
        Printf(Tee::PriLow, "Skipping %s because there are no power channels\n",
            GetName().c_str());
        return false;
    }
    vector<ThermalSensors::ChannelTempData> dummy;
    if (pGpuSubdevice->GetThermal()->GetChipTempsViaChannels(&dummy) != RC::OK)
    {
        Printf(Tee::PriLow, "Skipping %s because MODS cannot read VBIOS/RM thermal channels\n",
            GetName().c_str());
        return false;
    }

    return true;
}

RC ThermalResGpu::Setup()
{
    RC rc;
    CHECK_RC(ThermalRes::Setup());

    GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
    CHECK_RC(pGpuSubdevice->GetPerf()->GetLwrrentPerfPoint(&m_OrigPerfPoint));
    CHECK_RC(GetPerfPointConfig());
    CHECK_RC(BuildTempReadIdMap());

    if (m_IdlePerfPointConfig.locationStr == "INVALID")
    {
        // Pick the lowest power PerfPoint possible. This is always PX.min
        // where X is the highest pstate number.
        vector<UINT32> PStates;
        CHECK_RC(pGpuSubdevice->GetPerf()->GetAvailablePStates(&PStates));
        auto pLowPowerPState = std::max_element(PStates.begin(), PStates.end());
        if (pLowPowerPState != PStates.end())
        {
            m_IdlePerfPointConfig.PStateNum = *pLowPowerPState;
            m_IdlePerfPointConfig.locationStr = "min";

        }
    }
    return rc;
}

RC ThermalResGpu::Cleanup()
{
    StickyRC rc;
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    string locationStr = pPerf->SpecialPtToString(m_OrigPerfPoint.SpecialPt);
    VerbosePrintf("Restoring original PerfPoint %u.%s\n",
        m_OrigPerfPoint.PStateNum,
        locationStr.c_str());
    rc = GetBoundGpuSubdevice()->GetPerf()->SetPerfPoint(m_OrigPerfPoint);
    if (m_OverrideFan || m_StabilityCriteria)
    {
        rc = ThermalResGpu::SetFanCoolerTable(m_CoolerMask, m_OrigCoolerTable);
    }

    rc = ThermalRes::Cleanup();
    return rc;
}

RC ThermalResGpu::SetIdlePerfPoint()
{
    RC rc;
    VerbosePrintf("Setting Idle PerfPoint, PstateNum = %u,  LocationStr = %s\n",
        m_IdlePerfPointConfig.PStateNum,
        m_IdlePerfPointConfig.locationStr.c_str());
    CHECK_RC(SetPerfPoint(m_IdlePerfPointConfig.PStateNum,
        m_IdlePerfPointConfig.locationStr.c_str()));

    return rc;
}

RC ThermalResGpu::SetLoadPerfPoint()
{
    RC rc;
    VerbosePrintf("Setting Load PerfPoint, PstateNum = %u,  LocationStr = %s\n",
        m_LoadPerfPointConfig.PStateNum,
        m_LoadPerfPointConfig.locationStr.c_str());
    CHECK_RC(SetPerfPoint(m_LoadPerfPointConfig.PStateNum,
        m_LoadPerfPointConfig.locationStr));

    return rc;
}

bool ThermalResGpu::IsFloorSwept
(
    const ThermalSensors::ChannelInfos& chInfos,
    const string& chName,
    UINT32 chIdx
)
{
    for (const auto& chInfo : chInfos)
    {
        if (chName != ILWALID_THERMRES_CHANNEL_NAME &&
            chName == chInfo.second.Name)
        {
            return chInfo.second.IsFloorswept;
        }

        if (chIdx != ILWALID_THERMRES_VAL_UINT32 &&
            chIdx == chInfo.first)
        {
            return chInfo.second.IsFloorswept;
        }
    }

    return true;
}

RC ThermalResGpu::ConstructRConfig
(
    map<string, UINT32>& chNameToMaxIdleValMap,
    float allow_R_min,
    float allow_R_max
)
{
    RC rc = RC::OK;
    JavaScriptPtr pJavaScript;
    UINT32 prevRLimitsLen = ILWALID_THERMRES_VAL_UINT32;
    ThermalSensors::ChannelInfos chInfos;
    Thermal* pThermal = GetBoundGpuSubdevice()->GetThermal();
    CHECK_RC(pThermal->GetThermalSensors()->GetChannelInfos(&chInfos));

    for (UINT32 monIdx = 0; monIdx < m_Monitors.size(); monIdx++)
    {
        JSObject*   pOneEntry;
        JsArray     rLimitsInfoArr;
        ThermalResLib::RConfig rConfig;

        UINT32 pwrChIdx, tempChIdx, maxIdleTemp;
        CHECK_RC(pJavaScript->FromJsval(m_Monitors[monIdx], &pOneEntry));
        CHECK_RC(pJavaScript->UnpackFields(pOneEntry, "IIssIa",
                                        "ThermChannelIdx", &tempChIdx,
                                        "PwrChannelIdx", &pwrChIdx,
                                        "ThermChannelName", &rConfig.tempChName,
                                        "PwrChannelName", &rConfig.pwrChName,
                                        "MaxIdleTempDegC", &maxIdleTemp,
                                        "RLimits", &rLimitsInfoArr));

        if ((maxIdleTemp != ILWALID_THERMRES_VAL_UINT32) &&
            (maxIdleTemp < 0 || maxIdleTemp > 125))
        {
            Printf(Tee::PriError, "MaxIdleTempDegC (%.2f) must be between 0 and 125\n",
                static_cast<FLOAT32>(maxIdleTemp));
            return RC::ILWALID_ARGUMENT;
        }

        UINT32 rLimitsLen = static_cast<UINT32>(rLimitsInfoArr.size());
        if (prevRLimitsLen != ILWALID_THERMRES_VAL_UINT32)
        {
            if (rLimitsLen != prevRLimitsLen)
            {
                Printf(Tee::PriError,
                    "Monitor %u (%s, %s) has %u RLimits which is mismatched "
                    "with previous Monitor which has %u\n",
                    monIdx, rConfig.tempChName.c_str(), rConfig.pwrChName.c_str(),
                    static_cast<UINT32>(rLimitsInfoArr.size()), prevRLimitsLen);
                return RC::ILWALID_ARGUMENT;
            }
        }
        else
        {
            prevRLimitsLen = rLimitsLen;
        }

        if (rConfig.tempChName == ILWALID_THERMRES_CHANNEL_NAME ||
            tempChIdx == ILWALID_THERMRES_VAL_UINT32)
        {
            // If we cannot find the RM thermal channel index or name, the user
            // either passed an invalid channel, or they passed a valid channel but
            // it has been floorswept. We will ignore channels that are floorswept
            // so that users can use a single spec file that works across parts of
            // the same SKU with different floorsweeping configs.
            if (IsFloorSwept(chInfos, rConfig.tempChName, tempChIdx))
            {
                VerbosePrintf("Thermal channel %s (idx=%u) is floorswept\n",
                    rConfig.tempChName.c_str(), tempChIdx);
                continue;
            }
            else
            {
                Printf(Tee::PriError, "Invalid thermal channel given for monitor %u\n", monIdx);
                // TODO : Print all available thermal channels
                return RC::ILWALID_ARGUMENT;
            }
        }

        m_PwrChNameToIdxMap[rConfig.pwrChName] = pwrChIdx;
        m_TempChNameToIdxMap[rConfig.tempChName] = tempChIdx;
        chNameToMaxIdleValMap[rConfig.tempChName] = maxIdleTemp;

        for (UINT32 rIdx = 0; rIdx < rLimitsLen; rIdx++)
        {
            JSObject* pOneEntry;
            FLOAT32   minR, maxR;
            CHECK_RC(pJavaScript->FromJsval(rLimitsInfoArr[rIdx], &pOneEntry));
            CHECK_RC(pJavaScript->UnpackFields(pOneEntry, "ff",
                                            "MinR", &minR,
                                            "MaxR", &maxR));

            if ((maxR != ILWALID_THERMRES_R_VALUE) &&
                (maxR > allow_R_max || maxR < allow_R_min))
            {
                Printf(Tee::PriError, "MaxR (%.3e) is outside the allowed range [%.3e, %.3e]\n",
                    maxR, allow_R_min, allow_R_max);
                return RC::ILWALID_ARGUMENT;
            }

            if ((minR != ILWALID_THERMRES_R_VALUE) &&
                (minR > allow_R_max || minR < allow_R_min))
            {
                Printf(Tee::PriError, "Monitor %u (%s, %s) MinR (%.3e) is outside "
                    "the allowed range [%.3e, %.3e]\n",
                    monIdx, rConfig.tempChName.c_str(), rConfig.pwrChName.c_str(),
                    minR, allow_R_min, allow_R_max);
                return RC::ILWALID_ARGUMENT;
            }

            if (minR != ILWALID_THERMRES_R_VALUE &&
                maxR != ILWALID_THERMRES_R_VALUE &&
                minR >= maxR)
            {
                Printf(Tee::PriError,
                    "Monitor %u (%s, %s) RLimits[%u] MinR (%.3e) must be less than MaxR (%.3e)\n",
                    monIdx, rConfig.tempChName.c_str(), rConfig.pwrChName.c_str(),
                    rIdx, minR, maxR);
                return RC::ILWALID_ARGUMENT;
            }

            rConfig.rLimits.push_back(std::make_pair(minR, maxR));
        }

        m_RconfigList.push_back(std::move(rConfig));
    }

    UINT32 checkPtLen = static_cast<UINT32>(m_CheckpointTimesMsInternal.size());
    if (checkPtLen != prevRLimitsLen &&
        checkPtLen != prevRLimitsLen-1)
    {
        Printf(Tee::PriError,
            "CheckpointTimeMs size (%u) mismatches the number of expected checkpoints (%u)\n",
            checkPtLen, prevRLimitsLen);
        return RC::ILWALID_ARGUMENT;
    }

    if (checkPtLen == prevRLimitsLen-1)
    {
        // If the user only wants a single checkpoint, they do not need to
        // use CheckpointTimesMs. We will add a final checkpoint for them.
        m_CheckpointTimesMsInternal.push_back(m_LoadTimeMs);
    }

    VerbosePrintf("%u checkpoint time(s) specified: ", checkPtLen);
    for (const auto& checkpt : m_CheckpointTimesMsInternal)
    {
        VerbosePrintf("%u ", static_cast<UINT32>(checkpt));
    }
    VerbosePrintf("\n");

    return rc;
}

RC ThermalResGpu::ConstructRConfigForAllTempCh()
{
    RC rc;
    string tgpChName;
    UINT32 tgpChIdx;
    vector<ThermalSensors::ChannelTempData> channelTemps;
    CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->GetChipTempsViaChannels(&channelTemps));
    CHECK_RC(GetTGPChannelInfo(tgpChName, &tgpChIdx));

    for (const auto& lwrrTemp : channelTemps)
    {
        ThermalResLib::RConfig rConfig;
        string chName = TSM20Sensors::ProviderIndexToString(lwrrTemp.devClass, lwrrTemp.provIdx);
        if (lwrrTemp.devClass == LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU_GPC_TSOSC)
        {
            chName += Utility::StrPrintf("_%u", lwrrTemp.tsoscIdx);
        }
        else if (lwrrTemp.devClass == LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_HBM2_SITE)
        {
            chName += Utility::StrPrintf("_%u", lwrrTemp.siteIdx);
        }
        rConfig.tempChName = chName;
        rConfig.pwrChName = tgpChName;
        m_TempChNameToIdxMap[rConfig.tempChName] = lwrrTemp.chIdx;
        m_RconfigList.push_back(std::move(rConfig));
    }
    m_PwrChNameToIdxMap[tgpChName] = tgpChIdx;

    return rc;
}

RC ThermalResGpu::ConstructRConfigAndSensorInfo()
{
    RC rc;
    const string& device = GetBoundGpuSubdevice()->GetName();
    JavaScriptPtr pJavaScript;
    map<string, UINT32> chNameToMaxIdleValMap;

    if (m_Monitors.size())
    {
        CHECK_RC(ConstructRConfig(chNameToMaxIdleValMap,
            THERMALRES_GPU_MIN_ALLOWED_R,
            THERMALRES_GPU_MAX_ALLOWED_R));
    }
    else
    {
        if (m_CheckpointTimesMsInternal.size() != 0)
        {
            Printf(Tee::PriError,
                "CheckpointTimeMs size (%u) mismatches the number of expected checkpoints (%u)\n",
                static_cast<UINT32>(m_CheckpointTimesMsInternal.size()), 0);
            return RC::ILWALID_ARGUMENT;
        }
        CHECK_RC(ConstructRConfigForAllTempCh());
    }

    if (m_RconfigList.size() == 0)
    {
        Printf(Tee::PriError, "No monitors were created\n");
        return RC::ILWALID_ARGUMENT;
    }

    for (UINT32 i = 0; i < m_RconfigList.size(); i++)
    {
        const string& tempChName = m_RconfigList[i].tempChName;
        const string& pwrChName = m_RconfigList[i].pwrChName;
        VerbosePrintf("Created monitor %u: %s (ch=%u), %s (ch=%u)\n",
            i,
            tempChName.c_str(),
            m_TempChNameToIdxMap[tempChName],
            pwrChName.c_str(),
            m_PwrChNameToIdxMap[pwrChName]);
    }

    UINT32 numOfCheckPoints = static_cast<UINT32>(m_CheckpointTimesMsInternal.size());
    for (UINT32 i = 0; i < m_PowerLimits.size(); i++)
    {
        JSObject* pOneEntry;
        UINT32 pwrChIdx, maxIdlePwr, minAvgMw, maxAvgMw;
        CHECK_RC(pJavaScript->FromJsval(m_PowerLimits[i], &pOneEntry));
        CHECK_RC(pJavaScript->UnpackFields(pOneEntry, "IIII",
                                "ChannelIdx", &pwrChIdx,
                                "MaxIdleMw", &maxIdlePwr,
                                "MinAvgMw", &minAvgMw,
                                "MaxAvgMw", &maxAvgMw));

        for (auto& rConfig : m_RconfigList)
        {
            if (m_PwrChNameToIdxMap[rConfig.pwrChName] == pwrChIdx)
            {
                chNameToMaxIdleValMap[rConfig.pwrChName] = maxIdlePwr;
                for (UINT32 j = 0; j < numOfCheckPoints; j++)
                {
                    rConfig.pwrLimits.push_back(std::make_pair(minAvgMw, maxAvgMw));
                }
            }
        }
    }

    for (auto& rConfig : m_RconfigList)
    {
        for (const auto& checkpt : m_CheckpointTimesMsInternal)
        {
            rConfig.checkTimeMs.push_back(static_cast<uint64_t>(checkpt));
        }
        if (rConfig.pwrLimits.size() == 0)
        {
            for (UINT32 j = 0; j < numOfCheckPoints; j++)
            {
                rConfig.pwrLimits.push_back(std::make_pair(ILWALID_THERMRES_VAL_UINT32,
                    ILWALID_THERMRES_VAL_UINT32));
            }
        }

        if (QuerySensorInfoIdxByName(rConfig.pwrChName) == ILWALID_THERMRES_VAL_UINT32)
        {
            m_SensorInfoList.emplace_back(device,
                                        rConfig.pwrChName,
                                        ThermalResLib::DeviceType::GPU,
                                        ThermalResLib::ChannelType::POWER,
                                        m_PowerIirGain,
                                        ThermalResLib::Unit::MILLIWATT);
            if (chNameToMaxIdleValMap.find(rConfig.pwrChName) != chNameToMaxIdleValMap.end())
            {
                m_SensorInfoList[m_SensorInfoList.size()-1].maxIdleValue = \
                    chNameToMaxIdleValMap[rConfig.pwrChName];
            }
            else
            {
                m_SensorInfoList[m_SensorInfoList.size()-1].maxIdleValue = \
                    ILWALID_THERMRES_VAL_UINT32;
            }
        }

        if (QuerySensorInfoIdxByName(rConfig.tempChName) == ILWALID_THERMRES_VAL_UINT32)
        {
            m_SensorInfoList.emplace_back(device,
                                        rConfig.tempChName,
                                        ThermalResLib::DeviceType::GPU,
                                        ThermalResLib::ChannelType::TEMP,
                                        m_TempIirGain,
                                        ThermalResLib::Unit::DEGREE_C);
            if (chNameToMaxIdleValMap.find(rConfig.tempChName) != chNameToMaxIdleValMap.end())
            {
                m_SensorInfoList[m_SensorInfoList.size()-1].maxIdleValue = \
                    chNameToMaxIdleValMap[rConfig.tempChName];
            }
            else
            {
                m_SensorInfoList[m_SensorInfoList.size()-1].maxIdleValue = \
                    ILWALID_THERMRES_VAL_UINT32;
            }
        }
    }
    return rc;
}

RC ThermalResGpu::GetPerfPointConfig()
{
    RC rc;
    JavaScriptPtr pJavaScript;

    CHECK_RC(pJavaScript->UnpackFields(m_IdlePerfPoint, "Is",
                            "PStateNum", &m_IdlePerfPointConfig.PStateNum,
                            "LocationStr", &m_IdlePerfPointConfig.locationStr));

    CHECK_RC(pJavaScript->UnpackFields(m_LoadPerfPoint, "Is",
                            "PStateNum", &m_LoadPerfPointConfig.PStateNum,
                            "LocationStr", &m_LoadPerfPointConfig.locationStr));

    return rc;
}

RC ThermalResGpu::GetFanCoolerTable
(
    bool defaultValues,
    CoolerTableInfo& fanCoolerTable
)
{
    RC rc;

    fanCoolerTable.infoParams = { 0 };
    fanCoolerTable.statusParams = { };
    fanCoolerTable.ctrlParams = { };
    fanCoolerTable.ctrlParams.bDefault = defaultValues;
    CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->GetFanCoolerTable(
            &fanCoolerTable.infoParams,
            &fanCoolerTable.statusParams,
            &fanCoolerTable.ctrlParams));

    return rc;
}

 RC ThermalResGpu::SetFanCoolerTable(const UINT32 mask, const CoolerTableInfo& fanCoolerTable)
 {
     RC rc;
     LW2080_CTRL_FAN_COOLER_INFO_PARAMS infoParams = { 0 };
     LW2080_CTRL_FAN_COOLER_STATUS_PARAMS statusParams = { };
     LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS ctrlParams = { 0 };

     ctrlParams.bDefault = false;
     CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->GetFanCoolerTable(
            &infoParams,
            &statusParams,
            &ctrlParams));
     ctrlParams.coolerMask = mask;
     const LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS& fanCtrlParams = fanCoolerTable.ctrlParams;
     for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
     {
         if ((1 << index) & mask)
         {
             const LW2080_CTRL_FAN_COOLER_CONTROL_DATA& fanCtrlData = \
                fanCtrlParams.coolers[index].data;
             LW2080_CTRL_FAN_COOLER_CONTROL_DATA& ctrlData = ctrlParams.coolers[index].data;
             if (ctrlParams.coolers[index].type == LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE)
             {
                 ctrlData.active.rpmMin = fanCtrlData.active.rpmMin;
                 ctrlData.active.rpmMax = fanCtrlData.active.rpmMax;
                 ctrlData.active.bLevelSimActive = fanCtrlData.active.bLevelSimActive;
                 ctrlData.active.levelSim = fanCtrlData.active.levelSim;
             }
             else if (ctrlParams.coolers[index].type == LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM)
             {
                 ctrlData.activePwm.pwmMin = fanCtrlData.activePwm.pwmMin;
                 ctrlData.activePwm.pwmMax = fanCtrlData.activePwm.pwmMax;
                 ctrlData.activePwm.bPwmSimActive = fanCtrlData.activePwm.bPwmSimActive;
                 ctrlData.activePwm.pwmSim = fanCtrlData.activePwm.pwmSim;
                 ctrlData.activePwm.active.rpmMin = fanCtrlData.activePwm.active.rpmMin;
                 ctrlData.activePwm.active.rpmMax = fanCtrlData.activePwm.active.rpmMax;
                 ctrlData.activePwm.active.bLevelSimActive = \
                    fanCtrlData.activePwm.active.bLevelSimActive;
                 ctrlData.activePwm.active.levelSim = fanCtrlData.activePwm.active.levelSim;
                 ctrlData.activePwm.maxFanPwm = fanCtrlData.activePwm.maxFanPwm;
             }
             else if (ctrlParams.coolers[index].type == \
                      LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR)
             {
                 ctrlData.activePwmTachCorr.propGain = \
                    fanCtrlData.activePwmTachCorr.propGain;
                 ctrlData.activePwmTachCorr.bRpmSimActive = \
                    fanCtrlData.activePwmTachCorr.bRpmSimActive;
                 ctrlData.activePwmTachCorr.rpmSim = \
                    fanCtrlData.activePwmTachCorr.rpmSim;
                 ctrlData.activePwmTachCorr.activePwm.pwmMin = \
                    fanCtrlData.activePwmTachCorr.activePwm.pwmMin;
                 ctrlData.activePwmTachCorr.activePwm.pwmMax = \
                    fanCtrlData.activePwmTachCorr.activePwm.pwmMax;
                 ctrlData.activePwmTachCorr.activePwm.bPwmSimActive = \
                    fanCtrlData.activePwmTachCorr.activePwm.bPwmSimActive;
                 ctrlData.activePwmTachCorr.activePwm.pwmSim = \
                    fanCtrlData.activePwmTachCorr.activePwm.pwmSim;
                 ctrlData.activePwmTachCorr.activePwm.active.rpmMin = \
                    fanCtrlData.activePwmTachCorr.activePwm.active.rpmMin;
                 ctrlData.activePwmTachCorr.activePwm.active.rpmMax = \
                    fanCtrlData.activePwmTachCorr.activePwm.active.rpmMax;
                 ctrlData.activePwmTachCorr.activePwm.active.bLevelSimActive = \
                    fanCtrlData.activePwmTachCorr.activePwm.active.bLevelSimActive;
                 ctrlData.activePwmTachCorr.activePwm.active.levelSim = \
                    fanCtrlData.activePwmTachCorr.activePwm.active.levelSim;
                 ctrlData.activePwmTachCorr.pwmFloorLimitOffset = \
                    fanCtrlData.activePwmTachCorr.pwmFloorLimitOffset;
             }
         }

     }
     CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->SetFanCoolerTable(ctrlParams));
     return rc;
}

RC ThermalResGpu::CheckFanOverrideConfig()
{
    RC rc;
    GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
    UINT32 PolicyMask = 0;
    UINT32 numOfFans = 0;
    CHECK_RC(pGpuSubdevice->GetThermal()->GetFanPolicyMask(&PolicyMask));
    CHECK_RC(pGpuSubdevice->GetThermal()->GetFanCoolerMask(&m_CoolerMask));

    if (PolicyMask > 0 && m_CoolerMask > 0)
    {
        m_SupportsMultiFan = true;
    }

    if (m_SupportsMultiFan)
    {
        // Fan 2.0+
        CHECK_RC(ThermalResGpu::GetFanCoolerTable(false, m_OrigCoolerTable));

        UINT32 fanIdx = 0;
        for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
        {
            if ((1 << index) & m_CoolerMask)
            {
                bool tachSupported;
                const LW2080_CTRL_FAN_COOLER_INFO_DATA& infoData = \
                    m_OrigCoolerTable.infoParams.coolers[index].data;
                switch (m_OrigCoolerTable.infoParams.coolers[index].type)
                {
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                        tachSupported = infoData.active.bTachSupported;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                        tachSupported = infoData.activePwm.active.bTachSupported;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                        tachSupported = infoData.activePwmTachCorr.activePwm.active.bTachSupported;
                        break;
                    default:
                        Printf(Tee::PriError, "No matched cooler type found!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                }

                if (!tachSupported)
                {
                    VerbosePrintf("Fan%u does not have a tachometer\n", fanIdx);
                    m_OverrideFan = false;
                }
                fanIdx += 1;
                numOfFans += 1;
            }
        }
    }
    else
    {
        // Fan 1.0
        VerbosePrintf("Fan overrides are not supported\n");
        m_OverrideFan = false;
    }

    if (m_StabilityCriteria && m_OverrideFan)
    {
        Printf(Tee::PriWarn, "Disabling legacy OverrideFan behavior for StabilityCriteria\n");
        m_OverrideFan = false;
    }

    if (m_OverrideFan)
    {
        // If the user didn't explicitly override the fan settings, we will
        // lock to the acoustic max RPM.
        if (m_FanSettingsInternal.size() == 0)
        {
            m_UseRpm = true;
            for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
            {
                if ((1 << index) & m_CoolerMask)
                {
                    const LW2080_CTRL_FAN_COOLER_CONTROL_DATA& ctrlData = \
                        m_OrigCoolerTable.ctrlParams.coolers[index].data;
                    UINT32 rpmMax;
                    switch (m_OrigCoolerTable.infoParams.coolers[index].type)
                    {
                        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                            rpmMax = ctrlData.active.rpmMax;
                            break;
                        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                            rpmMax = ctrlData.activePwm.active.rpmMax;
                            break;
                        case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                            rpmMax = ctrlData.activePwmTachCorr.activePwm.active.rpmMax;
                            break;
                        default:
                            Printf(Tee::PriError, "No matched cooler type found!\n");
                            return RC::SOFTWARE_ERROR;
                            break;
                    }
                    m_FanSettingsInternal.push_back(rpmMax);
                }
            }
        }

        if (m_FanSettingsInternal.size() != numOfFans)
        {
            Printf(Tee::PriError, "FanSettings does not override all %u available fans\n",
                numOfFans);
            return RC::ILWALID_ARGUMENT;
        }
    }
    else
    {
        VerbosePrintf("Fan overrides are disabled\n");
    }
    return rc;
}

RC ThermalResGpu::SetAllFanPwm(UINT32 dutyPct)
{
    RC rc = RC::OK;
    CoolerTableInfo coolerTable;
    GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
    CHECK_RC(ThermalResGpu::GetFanCoolerTable(false, coolerTable));
    CHECK_RC(pGpuSubdevice->GetThermal()->GetFanCoolerMask(&m_CoolerMask));
    UINT32 targetPwm = static_cast<UINT32>(Utility::ColwertFloatToFXP(dutyPct / 100, 16, 16));
    for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
    {
        if ((1 << index) & m_CoolerMask)
        {
            LW2080_CTRL_FAN_COOLER_CONTROL_DATA& ctrlData = \
                coolerTable.ctrlParams.coolers[index].data;
            switch (coolerTable.infoParams.coolers[index].type)
            {
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                    Printf(Tee::PriError, "No support to set fan PWM!\n");
                    return RC::SOFTWARE_ERROR;
                    break;
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                    ctrlData.activePwm.bPwmSimActive = true;
                    ctrlData.activePwm.pwmSim = targetPwm;
                    break;
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                    ctrlData.activePwmTachCorr.activePwm.bPwmSimActive = true;
                    ctrlData.activePwmTachCorr.activePwm.pwmSim = targetPwm;
                    break;
                default:
                    Printf(Tee::PriError, "No matched cooler type found!\n");
                    return RC::SOFTWARE_ERROR;
                    break;
            }
        }
    }

    CHECK_RC(ThermalResGpu::SetFanCoolerTable(m_CoolerMask, coolerTable));
    return rc;
}

RC ThermalResGpu::SetAllFanRpm(UINT32 rpm)
{
    RC rc = RC::OK;
    CoolerTableInfo coolerTable;
    GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
    CHECK_RC(ThermalResGpu::GetFanCoolerTable(false, coolerTable));
    CHECK_RC(pGpuSubdevice->GetThermal()->GetFanCoolerMask(&m_CoolerMask));

    for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
    {
        if ((1 << index) & m_CoolerMask)
        {
            LW2080_CTRL_FAN_COOLER_CONTROL_DATA& ctrlData = \
                coolerTable.ctrlParams.coolers[index].data;
            switch (coolerTable.infoParams.coolers[index].type)
            {
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                    Printf(Tee::PriError, "No support to set fan RPM!\n");
                    return RC::SOFTWARE_ERROR;
                    break;
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                    Printf(Tee::PriError, "No support to set fan RPM!\n");
                    return RC::SOFTWARE_ERROR;
                    break;
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                    ctrlData.activePwmTachCorr.bRpmSimActive = true;
                    ctrlData.activePwmTachCorr.rpmSim = rpm;
                    break;
                default:
                    Printf(Tee::PriError, "No matched cooler type found!\n");
                    return RC::SOFTWARE_ERROR;
                    break;
            }
        }
    }

    CHECK_RC(ThermalResGpu::SetFanCoolerTable(m_CoolerMask, coolerTable));
    return rc;
}

RC ThermalResGpu::GetAllFanRpm(vector<UINT32>* pRpmList)
{
    RC rc;
    MASSERT(pRpmList);
    MASSERT(pRpmList->size() == 0);

    if (m_SupportsMultiFan)
    {
        GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
        CHECK_RC(pGpuSubdevice->GetThermal()->GetFanCoolerMask(&m_CoolerMask));
        CHECK_RC(ThermalResGpu::GetFanCoolerTable(false, m_LwrrCoolerTable));

        for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
        {
            if ((1 << index) & m_CoolerMask)
            {
                const LW2080_CTRL_FAN_COOLER_INFO_DATA& infoData = \
                    m_LwrrCoolerTable.infoParams.coolers[index].data;
                const LW2080_CTRL_FAN_COOLER_STATUS_DATA& statusData = \
                    m_LwrrCoolerTable.statusParams.coolers[index].data;
                UINT32 rpmLwrr;
                bool tachSupported;
                switch (m_LwrrCoolerTable.infoParams.coolers[index].type)
                {
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                        tachSupported = infoData.active.bTachSupported;
                        rpmLwrr = statusData.active.rpmLwrr;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                        tachSupported = infoData.activePwm.active.bTachSupported;
                        rpmLwrr = statusData.activePwm.active.rpmLwrr;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                        tachSupported = infoData.activePwmTachCorr.activePwm.active.bTachSupported;
                        rpmLwrr = statusData.activePwmTachCorr.activePwm.active.rpmLwrr;
                        break;
                    default:
                        Printf(Tee::PriError, "No matched cooler type found!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                }

                if (tachSupported)
                {
                    pRpmList->push_back(rpmLwrr);
                }
                else
                {
                    pRpmList->push_back(ILWALID_THERMRES_VAL_UINT32);
                }
            }
        }
    }

    return rc;
}

RC ThermalResGpu::GetAllFanPwm(vector<UINT32>* pPwmList)
{
    RC rc;
    MASSERT(pPwmList);
    MASSERT(pPwmList->size() == 0);

    if (m_SupportsMultiFan)
    {
        GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
        CHECK_RC(pGpuSubdevice->GetThermal()->GetFanCoolerMask(&m_CoolerMask));
        CHECK_RC(ThermalResGpu::GetFanCoolerTable(false, m_LwrrCoolerTable));

        for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
        {
            if ((1 << index) & m_CoolerMask)
            {
                const LW2080_CTRL_FAN_COOLER_INFO_DATA& infoData = \
                    m_LwrrCoolerTable.infoParams.coolers[index].data;
                const LW2080_CTRL_FAN_COOLER_STATUS_DATA& statusData = \
                    m_LwrrCoolerTable.statusParams.coolers[index].data;
                UINT32 pwmLwrr;
                bool tachSupported;
                switch (m_LwrrCoolerTable.infoParams.coolers[index].type)
                {
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                        Printf(Tee::PriError, "No support to get fan PWM!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                        tachSupported = infoData.activePwm.active.bTachSupported;
                        pwmLwrr = statusData.activePwm.pwmLwrr;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                        tachSupported = infoData.activePwmTachCorr.activePwm.active.bTachSupported;
                        pwmLwrr = statusData.activePwmTachCorr.activePwm.pwmLwrr;
                        break;
                    default:
                        Printf(Tee::PriError, "No matched cooler type found!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                }

                if (tachSupported)
                {
                    pPwmList->push_back(pwmLwrr);
                }
                else
                {
                    pPwmList->push_back(ILWALID_THERMRES_VAL_UINT32);
                }
            }
        }
    }

    return rc;
}

RC ThermalResGpu::ApplyFanOverrides()
{
    RC rc;
    CoolerTableInfo coolerTable;
    CHECK_RC(ThermalResGpu::GetFanCoolerTable(false, coolerTable));

    UINT32 fanIdx = 0;
    for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
    {
        if ((1 << index) & m_CoolerMask)
        {
            VerbosePrintf("Overriding fan%u %s to %u%s\n",
                fanIdx,
                m_UseRpm ? "RPM" : "PWM",
                m_FanSettingsInternal[fanIdx],
                m_UseRpm ? "" : "%");

            LW2080_CTRL_FAN_COOLER_CONTROL_DATA& ctrlData = \
                coolerTable.ctrlParams.coolers[index].data;
            if (m_UseRpm)
            {
                switch (coolerTable.infoParams.coolers[index].type)
                {
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                        Printf(Tee::PriError, "No support to set fan RPM!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                        Printf(Tee::PriError, "No support to set fan RPM!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                        ctrlData.activePwmTachCorr.bRpmSimActive = true;
                        ctrlData.activePwmTachCorr.rpmSim = m_FanSettingsInternal[fanIdx];
                        break;
                    default:
                        Printf(Tee::PriError, "No matched cooler type found!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                }
            }
            else
            {
                UINT32 targetPwm = static_cast<UINT32>(
                        Utility::ColwertFloatToFXP(
                            static_cast<FLOAT64>(m_FanSettingsInternal[fanIdx]) / 100, 16, 16));
                switch (coolerTable.infoParams.coolers[index].type)
                {
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                        Printf(Tee::PriError, "No support to set fan PWM!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                        ctrlData.activePwm.bPwmSimActive = true;
                        ctrlData.activePwm.pwmSim = targetPwm;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                        ctrlData.activePwmTachCorr.activePwm.bPwmSimActive = true;
                        ctrlData.activePwmTachCorr.activePwm.pwmSim = targetPwm;
                        break;
                    default:
                        Printf(Tee::PriError, "No matched cooler type found!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                }
            }
            fanIdx += 1;
        }
    }

    CHECK_RC(ThermalResGpu::SetFanCoolerTable(m_CoolerMask, coolerTable));

    VerbosePrintf("Letting fans colwerge for %ums\n", m_FanSettleTimeMs);

    UINT32 iterTimeMs = std::min(static_cast<UINT32>(500), m_FanSettleTimeMs);
    UINT32 elapsedMs = 0;
    UINT64 startTime = Xp::GetWallTimeMS();
    while (elapsedMs < m_FanSettleTimeMs)
    {
        Tasker::Sleep(static_cast<FLOAT64>(iterTimeMs));
        CHECK_RC(Abort::Check());
        CHECK_RC(ThermalResGpu::GetFanCoolerTable(false, m_LwrrCoolerTable));
        string line = "RPM:";

        for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
        {
            if ((1 << index) & m_CoolerMask)
            {
                const LW2080_CTRL_FAN_COOLER_INFO_DATA& infoData = \
                    m_LwrrCoolerTable.infoParams.coolers[index].data;
                const LW2080_CTRL_FAN_COOLER_STATUS_DATA& statusData = \
                    m_LwrrCoolerTable.statusParams.coolers[index].data;
                UINT32 rpmLwrr;
                bool tachSupported;
                switch (m_LwrrCoolerTable.infoParams.coolers[index].type)
                {
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                        tachSupported = infoData.active.bTachSupported;
                        rpmLwrr = statusData.active.rpmLwrr;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                        tachSupported = infoData.activePwm.active.bTachSupported;
                        rpmLwrr = statusData.activePwm.active.rpmLwrr;
                        break;
                    case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                        tachSupported = infoData.activePwmTachCorr.activePwm.active.bTachSupported;
                        rpmLwrr = statusData.activePwmTachCorr.activePwm.active.rpmLwrr;
                        break;
                    default:
                        Printf(Tee::PriError, "No matched cooler type found!\n");
                        return RC::SOFTWARE_ERROR;
                        break;
                }

                if (tachSupported)
                {
                    line += Utility::StrPrintf(" %-4u", rpmLwrr);
                }
            }
        }

        VerbosePrintf("%s\n", line.c_str());
        elapsedMs = static_cast<UINT32>(Xp::GetWallTimeMS() - startTime);
    }

    CHECK_RC(ThermalResGpu::GetFanCoolerTable(false, coolerTable));
    fanIdx = 0;
    for (UINT32 index = 0; index < LW2080_CTRL_FAN_COOLER_MAX_COOLERS; index++)
    {
        if ((1 << index) & m_CoolerMask)
        {
            const LW2080_CTRL_FAN_COOLER_STATUS_DATA& statusData = \
                coolerTable.statusParams.coolers[index].data;
            UINT32 rpmLwrr, pwmLwrr;
            switch (coolerTable.infoParams.coolers[index].type)
            {
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE:
                    Printf(Tee::PriError, "No support to get fan PWM!\n");
                    return RC::SOFTWARE_ERROR;
                    break;
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM:
                    rpmLwrr = statusData.activePwm.active.rpmLwrr;
                    pwmLwrr = statusData.activePwm.pwmLwrr;
                    break;
                case LW2080_CTRL_FAN_COOLER_TYPE_ACTIVE_PWM_TACH_CORR:
                    rpmLwrr = statusData.activePwmTachCorr.activePwm.active.rpmLwrr;
                    pwmLwrr = statusData.activePwmTachCorr.activePwm.pwmLwrr;
                    break;
                default:
                    Printf(Tee::PriError, "No matched cooler type found!\n");
                    return RC::SOFTWARE_ERROR;
                    break;
            }

            VerbosePrintf("Fan%u: RPM=%u, PWM=%.1f%%\n", fanIdx, rpmLwrr,
                Utility::ColwertFXPToFloat(static_cast<INT32>(pwmLwrr), 16, 16) * 100);
            fanIdx += 1;
        }
    }

    return rc;
}

RC ThermalResGpu::BuildTempReadIdMap()
{
    RC rc;
    vector<ThermalSensors::ChannelTempData> channelTemps;
    CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->GetChipTempsViaChannels(&channelTemps));

    for (const auto & sensorInfo : m_SensorInfoList)
    {
        if (sensorInfo.chType == ThermalResLib::ChannelType::TEMP &&
            m_TempChNameToReadIdxMap.find(sensorInfo.chName) == m_TempChNameToReadIdxMap.end())
        {
            MASSERT(m_TempChNameToIdxMap.find(sensorInfo.chName) != m_TempChNameToIdxMap.end());
            UINT32 chIdx = m_TempChNameToIdxMap[sensorInfo.chName];
            for (UINT32 idx = 0; idx < channelTemps.size(); idx++)
            {
                if (channelTemps[idx].chIdx == chIdx)
                {
                    m_TempChNameToReadIdxMap[sensorInfo.chName] = idx;
                    break;
                }
            }
        }
    }
    return rc;
}

RC ThermalResGpu::GetChipAvgTemp(FLOAT32* pLwrTemp)
{
    RC rc;
    CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->GetChipTempViaPrimary(pLwrTemp));
    return rc;
}

RC ThermalResGpu::SetPerfPoint(UINT32 pStateNum, const string& locationStr)
{
    RC rc;
    Perf* pPerf = GetBoundGpuSubdevice()->GetPerf();
    UINT32 gpcPerfMode = pPerf->StringToSpecialPt(locationStr);
    if (static_cast<Perf::GpcPerfMode>(gpcPerfMode) == Perf::GpcPerfMode::GpcPerf_EXPLICIT)
    {
        Printf(Tee::PriNormal, "This test does not support locationStr = explicit\n");
        return RC::SOFTWARE_ERROR;
    }

    Perf::PerfPoint newPerfPt(pStateNum, static_cast<Perf::GpcPerfMode>(gpcPerfMode));
    Perf::PerfPoint* pNewPerfPt = &newPerfPt;
    if (pNewPerfPt->SpecialPt != Perf::GpcPerf_EXPLICIT &&
        !(pNewPerfPt->SpecialPt == Perf::GpcPerf_INTERSECT &&
        !pNewPerfPt->IntersectSettings.empty()))
    {
        // Get a matching standard perfpoint
        const Perf::PerfPoint &perfPoint = *pNewPerfPt;
        const Perf::PerfPoint stdPerfPoint = pPerf->GetMatchingStdPerfPoint(perfPoint);
        if (stdPerfPoint.SpecialPt == Perf::NUM_GpcPerfMode)
        {
            return RC::SOFTWARE_ERROR;
        }
        *pNewPerfPt = stdPerfPoint;
    }

    CHECK_RC(pPerf->SetPerfPoint(newPerfPt));
    return rc;
}

RC ThermalResGpu::GetPowerSensorSample(PowerSensorSample& pwrSample)
{
    RC rc;
    Power* pPower = GetBoundGpuSubdevice()->GetPower();
    LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS Samp;
    memset(&Samp, 0, sizeof(LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS));
    CHECK_RC(pPower->GetPowerSensorSample(&Samp));

    pwrSample.totalGpuPowermW = Samp.totalGpuPowermW;
    MASSERT(pwrSample.channels.size() == 0);
    UINT32 SensorMask = Samp.super.objMask.super.pData[0];
    for (UINT32 i = 0; i < LW2080_CTRL_PMGR_PWR_CHANNEL_MAX_CHANNELS; i++)
    {
        UINT32 m = (1<<i);
        if (m > SensorMask)
        {
            break;
        }

        PwrChannelInfo chInfo;
        if (0 == (SensorMask & m))
        {
            //placeholder to avoid gaps in array
            chInfo.bValid = false;
            pwrSample.channels.push_back(std::move(chInfo));
            continue;
        }
        chInfo.bValid   = true;
        chInfo.pwrAvgmW = Samp.channelStatus[i].tuplePolled.pwrmW;
        chInfo.pwrMinmW = Samp.channelStatus[i].tuplePolled.pwrmW;
        chInfo.pwrMaxmW = Samp.channelStatus[i].tuplePolled.pwrmW;
        chInfo.voltuV   = Samp.channelStatus[i].tuplePolled.voltuV;
        chInfo.lwrrmA   = Samp.channelStatus[i].tuplePolled.lwrrmA;
        pwrSample.channels.push_back(std::move(chInfo));
    }
    return rc;
}

RC ThermalResGpu::GetTGPChannelInfo(string& tgpChName, UINT32* pTgpChIdx)
{
    RC rc;
    PowerSensorSample pwrSample;
    CHECK_RC(ThermalResGpu::GetPowerSensorSample(pwrSample));

    Power* pPower = GetBoundGpuSubdevice()->GetPower();
    for (UINT32 i = 0; i < pwrSample.channels.size(); i++)
    {
        if (pwrSample.channels[i].bValid)
        {
            UINT32 rail = pPower->GetPowerRail(1 << i);
            if (strcmp(pPower->PowerRailToStr(rail), "INPUT_TOTAL_BOARD2") == 0)
            {
                tgpChName  = pPower->PowerRailToStr(rail);
                *pTgpChIdx = i;
                break;
            }
            else if (strcmp(pPower->PowerRailToStr(rail), "INPUT_TOTAL_BOARD") == 0)
            {
                tgpChName  = pPower->PowerRailToStr(rail);
                *pTgpChIdx = i;
            }
        }
    }
    return rc;
}

RC ThermalResGpu::CollectPowerSamples()
{
    RC rc;
    string device = GetBoundGpuSubdevice()->GetName();
    PowerSensorSample pwrSample;
    CHECK_RC(ThermalResGpu::GetPowerSensorSample(pwrSample));

    const vector<ThermalResLib::Monitor>&          monList  = m_pLib->GetAllMonitor();
    const vector<ThermalResLib::SensorDataStat>& sensorList = m_pLib->GetAllSensorDataStat();

    for (const auto& mon : monList)
    {
        UINT32 pwrSensorInfoIdx = sensorList[mon.pwrSensorIndex].sensorInfoIdx;
        MASSERT(m_PwrChNameToIdxMap.find(m_SensorInfoList[pwrSensorInfoIdx].chName) != \
            m_PwrChNameToIdxMap.end());
        UINT32 pwrChIdx = m_PwrChNameToIdxMap[m_SensorInfoList[pwrSensorInfoIdx].chName];
        if (pwrSample.channels[pwrChIdx].bValid)
        {
            UINT32 id;
            ThermalResLib::SensorData pwrSensor;
            pwrSensor.sensorInfoIdx = pwrSensorInfoIdx;
            pwrSensor.value         = static_cast<FLOAT32>(pwrSample.channels[pwrChIdx].pwrAvgmW);
            pwrSensor.timestamp     = Xp::GetWallTimeMS();
            CHECK_RC(HandleThermResLibRC(m_pLib->StoreSensorData(pwrSensor, &id)));
        }
    }

    return rc;
}

RC ThermalResGpu::CollectThermalSamples()
{
    RC rc;
    string device = GetBoundGpuSubdevice()->GetName();
    vector<ThermalSensors::ChannelTempData> channelTemps;
    CHECK_RC(GetBoundGpuSubdevice()->GetThermal()->GetChipTempsViaChannels(&channelTemps));

    string gpuAvgTempChName = TSM20Sensors::ProviderIndexToString(
            LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU,
            LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_AVG);

    string gpuOffsetMaxTempChName = TSM20Sensors::ProviderIndexToString(
            LW2080_CTRL_THERMAL_THERM_DEVICE_CLASS_GPU,
            LW2080_CTRL_THERMAL_THERM_DEVICE_GPU_PROV_GPU_OFFSET_MAX);

    const vector<ThermalResLib::Monitor>&          monList  = m_pLib->GetAllMonitor();
    const vector<ThermalResLib::SensorDataStat>& sensorList = m_pLib->GetAllSensorDataStat();
    FLOAT32 gpuAvgTemp = 0.0, gpuOffsetMaxTemp = 0.0;
    for (const auto& mon : monList)
    {
        UINT32 tempSensorInfoIdx = sensorList[mon.tempSensorIndex].sensorInfoIdx;
        MASSERT(m_TempChNameToReadIdxMap.find(m_SensorInfoList[tempSensorInfoIdx].chName) != \
            m_TempChNameToReadIdxMap.end());
        UINT32 readIdx = m_TempChNameToReadIdxMap[m_SensorInfoList[tempSensorInfoIdx].chName];
        UINT32 id;
        ThermalResLib::SensorData tempSensor;

        tempSensor.sensorInfoIdx = tempSensorInfoIdx;
        tempSensor.value         = channelTemps[readIdx].temp;
        tempSensor.timestamp     = Xp::GetWallTimeMS();
        CHECK_RC(HandleThermResLibRC(m_pLib->StoreSensorData(tempSensor, &id)));

        MASSERT(m_RconfigList[mon.rIndex].tempChName == \
            m_SensorInfoList[tempSensorInfoIdx].chName);
        if (gpuAvgTempChName == m_SensorInfoList[tempSensorInfoIdx].chName)
        {
            gpuAvgTemp = m_pLib->GetSensorDataStat(id).filteredValue;
        }
        if (gpuOffsetMaxTempChName == m_SensorInfoList[tempSensorInfoIdx].chName)
        {
            gpuOffsetMaxTemp = m_pLib->GetSensorDataStat(id).filteredValue;
        }
    }
    m_CallwlatedHotSpotOffsetTemp = gpuOffsetMaxTemp - gpuAvgTemp;

    return rc;
}

RC ThermalResGpu::AcceleratePowerColw()
{
    RC rc = RC::OK;
    string tgpChName;
    UINT32 tgpChIdx;
    CHECK_RC(GetTGPChannelInfo(tgpChName, &tgpChIdx));
    Power* pPower = GetBoundGpuSubdevice()->GetPower();
    const vector<ThermalResLib::Monitor>& monList  = m_pLib->GetAllMonitor();
    for (const auto& mon : monList)
    {
        const auto& rConfig = m_RconfigList[mon.rIndex];
        if (rConfig.pwrChName == tgpChName && m_HasPowerCapping)
        {
            UINT32 capMw;
            CHECK_RC(pPower->GetPowerCap(Power::TGP, &capMw));
            CHECK_RC(HandleThermResLibRC(
                    m_pLib->ForceSetPowerSensorDataStatValue(
                            mon.pwrSensorIndex,
                            static_cast<float>(capMw))));
        }
        else if (rConfig.pwrLimits.size())
        {
            UINT32 minPwr = rConfig.pwrLimits[0].first;
            UINT32 maxPwr = rConfig.pwrLimits[0].second;

            if (minPwr != ILWALID_THERMRES_VAL_UINT32 &&
                maxPwr != ILWALID_THERMRES_VAL_UINT32)
            {
                float newFilteredVal = (static_cast<float>(minPwr) +
                                        static_cast<float>(maxPwr)) / 2;
                CHECK_RC(HandleThermResLibRC(
                        m_pLib->ForceSetPowerSensorDataStatValue(
                            mon.pwrSensorIndex,
                            newFilteredVal)));
            }
            else if (maxPwr != ILWALID_THERMRES_VAL_UINT32)
            {
                float newFilteredVal = static_cast<float>(maxPwr);
                CHECK_RC(HandleThermResLibRC(
                    m_pLib->ForceSetPowerSensorDataStatValue(
                        mon.pwrSensorIndex,
                        newFilteredVal)));
            }
            else if (minPwr != ILWALID_THERMRES_VAL_UINT32)
            {
                float newFilteredVal = static_cast<float>(minPwr);
                CHECK_RC(HandleThermResLibRC(
                    m_pLib->ForceSetPowerSensorDataStatValue(
                        mon.pwrSensorIndex,
                        newFilteredVal)));
            }
        }
    }

    return rc;
}

RC ThermalResGpu::StartBgTest()
{
    RC rc;
    // Starting the BgTest (defined in gpudecls.js)
    VerbosePrintf("Starting background test %s (test %u)\n",
        m_BgTestName.c_str(), m_BgTestNum);
    m_pCallbackArgs = make_unique<CallbackArguments>();
    CHECK_RC(FireCallbacks(ModsTest::MISC_A,
        Callbacks::STOP_ON_ERROR,
        *m_pCallbackArgs,
        "Bgtest Start"));
    return rc;
}

RC ThermalResGpu::StopBgTest()
{
    RC rc;
    VerbosePrintf("Stopping background test %s (test %u)\n",
        m_BgTestName.c_str(), m_BgTestNum);
    CHECK_RC(FireCallbacks(ModsTest::MISC_B,
        Callbacks::STOP_ON_ERROR,
        *m_pCallbackArgs,
        "Bgtest End"));
    return rc;
}