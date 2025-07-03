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
#include "thermressoc.h"

// Exposed JS properties
JS_CLASS_INHERIT(ThermalResSoc, ThermalRes, "Thermal resistance SoC test");

RC ThermalResSoc::GetSocPlatform(CheetAh::TegraBoardId* pPlatform)
{
    RC rc;
    Xp::BoardInfo boardInfo;
    CHECK_RC(Xp::GetBoardInfo(&boardInfo));
    switch (boardInfo.boardId)
    {
        case CheetAh::TegraBoardId::T234P3737:
        case CheetAh::TegraBoardId::T234E2425:
            *pPlatform = static_cast<CheetAh::TegraBoardId>(boardInfo.boardId);
            break;
        default:
            *pPlatform = CheetAh::TegraBoardId::Unknown;
            break;
    }

    return rc;
}

UINT32 ThermalResSoc::GetMaxPwm() const
{
    //TODO: check if the pwm polarity is ilwerted first
    return 255;
}

RC ThermalResSoc::SocThermalInit()
{
    UINT32 numOfFans = 0;
    switch (m_Platform)
    {
        case CheetAh::TegraBoardId::T234P3737:
            numOfFans = 1;
            m_pSocThermal = make_unique<SocThermalConcord>(numOfFans);
            break;
        case CheetAh::TegraBoardId::T234E2425:
            m_pSocThermal = make_unique<SocThermalConcord>(numOfFans);
            break;
        default:
            Printf(Tee::PriError,
                "The platform is not lwrrently supported to read "
                "power / thermal sensors in the test\n");
            return RC::UNSUPPORTED_DEVICE;
            break;
    }

    return RC::OK;
}

bool ThermalResSoc::IsSupported()
{
    GpuSubdevice *pGpuSubdevice = GetBoundGpuSubdevice();

    if (!pGpuSubdevice->IsSOC())
    {
        Printf(Tee::PriLow,
               "Skipping %s because it is only supported on CheetAh SoC\n",
               GetName().c_str());
        return false;
    }

    if (GetSocPlatform(&m_Platform) != RC::OK)
    {
        Printf(Tee::PriError, "Error oclwrs when determining the SoC platform\n");
        return false;
    }

    if (m_Platform == CheetAh::TegraBoardId::Unknown)
    {
        Printf(Tee::PriLow,
               "Skipping %s because current platform cannot be identified\n",
               GetName().c_str());
        return false;
    }

    return true;
}

RC ThermalResSoc::Setup()
{
    RC rc;

    CHECK_RC(SocThermalInit());
    CHECK_RC(ThermalRes::Setup());

    return rc;
}

RC ThermalResSoc::Cleanup()
{
    StickyRC rc;

    UINT32 numOfFans = m_pSocThermal->GetNumOfFans();
    if (m_OverrideFan || m_StabilityCriteria)
    {
        for (UINT32 idx = 0; idx < numOfFans; idx++)
        {
            rc = m_pSocThermal->SetFanPwm(m_OrigPwmList[idx], idx);
        }
    }

    rc = ThermalRes::Cleanup();
    return rc;
}

RC ThermalResSoc::CheckFanOverrideConfig()
{
    RC rc;

    CHECK_RC(m_pSocThermal->GetAllFanPwm(&m_OrigPwmList));
    UINT32 numOfFans = static_cast<UINT32>(m_OrigPwmList.size());

    if (m_StabilityCriteria && m_OverrideFan)
    {
        Printf(Tee::PriWarn, "Disabling legacy OverrideFan behavior for StabilityCriteria\n");
        m_OverrideFan = false;
    }

    if (m_OverrideFan)
    {
        if (m_FanSettingsInternal.size() == 0)
        {
            UINT32 maxPwm = GetMaxPwm();
            for (UINT32 i = 0; i < numOfFans; i++)
            {
                m_FanSettingsInternal.push_back(maxPwm);
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

RC ThermalResSoc::ApplyFanOverrides()
{
    RC rc;
    UINT32 numOfFans = static_cast<UINT32>(m_OrigPwmList.size());

    for (UINT32 fanIdx = 0; fanIdx < numOfFans; fanIdx++)
    {
        VerbosePrintf("Overriding fan%u %s to %u\n",
            fanIdx,
            m_UseRpm ? "RPM" : "PWM",
            m_FanSettingsInternal[fanIdx]);

        if (m_UseRpm)
        {
            CHECK_RC(m_pSocThermal->SetFanRpm(m_FanSettingsInternal[fanIdx], fanIdx));
        }
        else
        {
            CHECK_RC(m_pSocThermal->SetFanPwm(m_FanSettingsInternal[fanIdx], fanIdx));
        }
    }

    VerbosePrintf("Letting fans colwerge for %ums\n", m_FanSettleTimeMs);
    UINT32 iterTimeMs = std::min(static_cast<UINT32>(500), m_FanSettleTimeMs);
    UINT32 elapsedMs = 0;
    UINT64 startTime = Xp::GetWallTimeMS();
    while (elapsedMs < m_FanSettleTimeMs)
    {
        Tasker::Sleep(static_cast<FLOAT64>(iterTimeMs));
        CHECK_RC(Abort::Check());
        string line = "RPM:";
        for (UINT32 fanIdx = 0; fanIdx < numOfFans; fanIdx++)
        {
            UINT32 rpmLwrr;
            CHECK_RC(m_pSocThermal->GetFanRpm(&rpmLwrr, fanIdx));
            line += Utility::StrPrintf(" %-4u", rpmLwrr);
        }

        VerbosePrintf("%s\n", line.c_str());
        elapsedMs = static_cast<UINT32>(Xp::GetWallTimeMS() - startTime);
    }

    for (UINT32 fanIdx = 0; fanIdx < numOfFans; fanIdx++)
    {
        UINT32 rpmLwrr, pwmLwrr;
        CHECK_RC(m_pSocThermal->GetFanRpm(&rpmLwrr, fanIdx));
        CHECK_RC(m_pSocThermal->GetFanPwm(&pwmLwrr, fanIdx));
        VerbosePrintf("Fan%u: RPM=%u, PWM=%.1f%%\n", fanIdx, rpmLwrr, pwmLwrr);
    }

    return rc;
}

RC ThermalResSoc::CollectPowerSamples()
{
    RC rc;
    string device = GetBoundGpuSubdevice()->GetName();

    const vector<ThermalResLib::Monitor> &monList = m_pLib->GetAllMonitor();
    const vector<ThermalResLib::SensorDataStat> &sensorList = m_pLib->GetAllSensorDataStat();

    for (const auto &mon : monList)
    {
        UINT32 pwrSensorInfoIdx = sensorList[mon.pwrSensorIndex].sensorInfoIdx;
        const string &chName = m_SensorInfoList[pwrSensorInfoIdx].chName;
        MASSERT(m_RconfigList[mon.rIndex].pwrChName == chName);

        FLOAT32 lwrrPwr;
        CHECK_RC(m_pSocThermal->GetPowerSample(chName, &lwrrPwr));

        UINT32 id;
        ThermalResLib::SensorData pwrSensor;
        pwrSensor.sensorInfoIdx = pwrSensorInfoIdx;
        pwrSensor.value = lwrrPwr;
        pwrSensor.timestamp = Xp::GetWallTimeMS();
        CHECK_RC(HandleThermResLibRC(m_pLib->StoreSensorData(pwrSensor, &id)));
    }

    return rc;
}

RC ThermalResSoc::CollectThermalSamples()
{
    RC rc;
    string device = GetBoundGpuSubdevice()->GetName();

    const vector<ThermalResLib::Monitor> &monList = m_pLib->GetAllMonitor();
    const vector<ThermalResLib::SensorDataStat> &sensorList = m_pLib->GetAllSensorDataStat();

    for (const auto &mon : monList)
    {
        UINT32 tempSensorInfoIdx = sensorList[mon.tempSensorIndex].sensorInfoIdx;
        const string &chName = m_SensorInfoList[tempSensorInfoIdx].chName;
        MASSERT(m_RconfigList[mon.rIndex].tempChName == chName);

        FLOAT32 lwrrTemp;
        CHECK_RC(m_pSocThermal->GetThermalSample(chName, &lwrrTemp));

        UINT32 id;
        ThermalResLib::SensorData tempSensor;
        tempSensor.sensorInfoIdx = tempSensorInfoIdx;
        tempSensor.value = lwrrTemp;
        tempSensor.timestamp = Xp::GetWallTimeMS();
        CHECK_RC(HandleThermResLibRC(m_pLib->StoreSensorData(tempSensor, &id)));

    }

    return rc;
}

RC ThermalResSoc::GetChipAvgTemp(FLOAT32 *pLwrTemp)
{
    RC rc;
    MASSERT(pLwrTemp);
    CHECK_RC(m_pSocThermal->GetThermalSample(THERMALRES_SOC_CHIP_OFFSET_MAX_TEMP_CH_NAME, pLwrTemp));

    return rc;
}

RC ThermalResSoc::StartBgTest()
{
    RC rc;
    VerbosePrintf("Starting background test %s (test %u)\n", m_BgTestName.c_str(), m_BgTestNum);
    m_pCallbackArgs = make_unique<CallbackArguments>();
    CHECK_RC(FireCallbacks(ModsTest::MISC_A,
        Callbacks::STOP_ON_ERROR,
        *m_pCallbackArgs,
        "Bgtest Start"));

    return rc;
}

RC ThermalResSoc::StopBgTest()
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

RC ThermalResSoc::SetIdlePerfPoint()
{
    //we don't set dedicated perfPoint in the idle phase
    //It relys on power-gated function to enter idle state
    return RC::OK;
}

RC ThermalResSoc::SetLoadPerfPoint()
{
    //we use the current perfpoint to run the thermal resistance test
    return RC::OK;
}

RC ThermalResSoc::AcceleratePowerColw()
{
    // TODO: use the power capping information to accelerate power colwergence in load phase
    return RC::OK;
}

bool ThermalResSoc::IsThermalChValid(const string& chName) const
{
    for (const auto& validCh : m_AllValidTempCh)
    {
        if (chName == validCh)
        {
            return true;
        }
    }
    return false;
}

RC ThermalResSoc::ConstructRConfig(
    map<string, UINT32>& chNameToMaxIdleValMap,
    float allow_R_min,
    float allow_R_max)
{
    RC rc;
    JavaScriptPtr pJavaScript;
    UINT32 prevRLimitsLen = ILWALID_THERMRES_VAL_UINT32;

    for (UINT32 monIdx = 0; monIdx < m_Monitors.size(); monIdx++)
    {
        JSObject *pOneEntry;
        JsArray rLimitsInfoArr;
        ThermalResLib::RConfig rConfig;

        UINT32 maxIdleTemp;
        CHECK_RC(pJavaScript->FromJsval(m_Monitors[monIdx], &pOneEntry));
        CHECK_RC(pJavaScript->UnpackFields(pOneEntry, "sIa",
                                        "ThermChannelName", &rConfig.tempChName,
                                        "MaxIdleTempDegC", &maxIdleTemp,
                                        "RLimits", &rLimitsInfoArr));

        rConfig.pwrChName = THERMALRES_SOC_CHIP_TOTAL_POWER_CH_NAME;

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

        if (!IsThermalChValid(rConfig.tempChName))
        {
            Printf(Tee::PriError, "Invalid thermal channel given for monitor %u\n", monIdx);
            // TODO : Print all available thermal channels
            return RC::ILWALID_ARGUMENT;
        }

        chNameToMaxIdleValMap[rConfig.tempChName] = maxIdleTemp;
        for (UINT32 rIdx = 0; rIdx < rLimitsLen; rIdx++)
        {
            JSObject *pOneEntry;
            FLOAT32 minR, maxR;
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
        checkPtLen != prevRLimitsLen - 1)
    {
        Printf(Tee::PriError,
            "CheckpointTimeMs size (%u) mismatches the number of expected checkpoints (%u)\n",
            checkPtLen, prevRLimitsLen);
        return RC::ILWALID_ARGUMENT;
    }

    if (checkPtLen == prevRLimitsLen - 1)
    {
        // If the user only wants a single checkpoint, they do not need to
        // use CheckpointTimesMs. We will add a final checkpoint for them.
        m_CheckpointTimesMsInternal.push_back(m_LoadTimeMs);
    }

    VerbosePrintf("%u checkpoint time(s) specified: ",
        static_cast<UINT32>(m_CheckpointTimesMsInternal.size()));
    for (const auto &checkpt : m_CheckpointTimesMsInternal)
    {
        VerbosePrintf("%u ", static_cast<UINT32>(checkpt));
    }
    VerbosePrintf("\n");

    return rc;
}

RC ThermalResSoc::ConstructRConfigForAllTempCh()
{
    RC rc;

    for (const auto &tempCh : m_AllValidTempCh)
    {
        ThermalResLib::RConfig rConfig;

        rConfig.tempChName = tempCh;
        rConfig.pwrChName = THERMALRES_SOC_CHIP_TOTAL_POWER_CH_NAME;
        m_RconfigList.push_back(std::move(rConfig));
    }

    return rc;
}

RC ThermalResSoc::ConstructRConfigAndSensorInfo()
{
    RC rc;
    const string &device = GetBoundGpuSubdevice()->GetName();
    JavaScriptPtr pJavaScript;
    map<string, UINT32> chNameToMaxIdleValMap;

    if (m_Monitors.size())
    {
        CHECK_RC(ConstructRConfig(chNameToMaxIdleValMap,
                                THERMALRES_SOC_MIN_ALLOWED_R,
                                THERMALRES_SOC_MAX_ALLOWED_R));
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
        const string &tempChName = m_RconfigList[i].tempChName;
        const string &pwrChName = m_RconfigList[i].pwrChName;
        VerbosePrintf("Created monitor %u: %s, %s\n", i, tempChName.c_str(), pwrChName.c_str());
    }

    UINT32 numOfCheckPoints = static_cast<UINT32>(m_CheckpointTimesMsInternal.size());
    for (UINT32 i = 0; i < m_PowerLimits.size(); i++)
    {
        JSObject *pOneEntry;
        UINT32 maxIdlePwr, minAvgMw, maxAvgMw;
        CHECK_RC(pJavaScript->FromJsval(m_PowerLimits[i], &pOneEntry));
        CHECK_RC(pJavaScript->UnpackFields(pOneEntry, "III",
                                        "MaxIdleMw", &maxIdlePwr,
                                        "MinAvgMw", &minAvgMw,
                                        "MaxAvgMw", &maxAvgMw));

        for (auto &rConfig : m_RconfigList)
        {
            //The power limit is always specifed for the whole chip power
            chNameToMaxIdleValMap[rConfig.pwrChName] = maxIdlePwr;
            for (UINT32 j = 0; j < numOfCheckPoints; j++)
            {
                rConfig.pwrLimits.push_back(std::make_pair(minAvgMw, maxAvgMw));
            }
        }
    }

    for (auto &rConfig : m_RconfigList)
    {
        for (const auto &checkpt : m_CheckpointTimesMsInternal)
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
                                          ThermalResLib::DeviceType::SOC,
                                          ThermalResLib::ChannelType::POWER,
                                          m_PowerIirGain,
                                          ThermalResLib::Unit::MILLIWATT);
            if (chNameToMaxIdleValMap.find(rConfig.pwrChName) != chNameToMaxIdleValMap.end())
            {
                m_SensorInfoList[m_SensorInfoList.size() - 1].maxIdleValue =
                    chNameToMaxIdleValMap[rConfig.pwrChName];
            }
            else
            {
                m_SensorInfoList[m_SensorInfoList.size() - 1].maxIdleValue =
                    ILWALID_THERMRES_VAL_UINT32;
            }
        }

        if (QuerySensorInfoIdxByName(rConfig.tempChName) == ILWALID_THERMRES_VAL_UINT32)
        {
            m_SensorInfoList.emplace_back(device,
                                          rConfig.tempChName,
                                          ThermalResLib::DeviceType::SOC,
                                          ThermalResLib::ChannelType::TEMP,
                                          m_TempIirGain,
                                          ThermalResLib::Unit::DEGREE_C);
            if (chNameToMaxIdleValMap.find(rConfig.tempChName) != chNameToMaxIdleValMap.end())
            {
                m_SensorInfoList[m_SensorInfoList.size() - 1].maxIdleValue =
                    chNameToMaxIdleValMap[rConfig.tempChName];
            }
            else
            {
                m_SensorInfoList[m_SensorInfoList.size() - 1].maxIdleValue =
                    ILWALID_THERMRES_VAL_UINT32;
            }
        }
    }
    return rc;
}

RC ThermalResSoc::SetAllFanPwm(UINT32 pwm)
{
    RC rc;
    CHECK_RC(m_pSocThermal->SetAllFanPwm(pwm));
    return rc;
};

RC ThermalResSoc::SetAllFanRpm(UINT32 rpm)
{
    RC rc;
    CHECK_RC(m_pSocThermal->SetAllFanRpm(rpm));
    return rc;
};

RC ThermalResSoc::GetAllFanRpm(vector<UINT32>* pRpmList)
{
    RC rc;
    CHECK_RC(m_pSocThermal->GetAllFanRpm(pRpmList));
    return rc;
};

RC ThermalResSoc::GetAllFanPwm(vector<UINT32>* pPwmList)
{
    RC rc;
    CHECK_RC(m_pSocThermal->GetAllFanPwm(pPwmList));
    return rc;
};

///////// Common Methods Implementations ///////
RC SocThermal::SetAllFanPwm(UINT32 pwm)
{
    RC rc;
    UINT32 numOfFans = GetNumOfFans();
    for (UINT32 idx = 0; idx < numOfFans; idx++)
    {
        CHECK_RC(SetFanPwm(pwm, idx));
    }

    return rc;
}

RC SocThermal::SetAllFanRpm(UINT32 rpm)
{
    RC rc;
    UINT32 numOfFans = GetNumOfFans();
    for (UINT32 idx = 0; idx < numOfFans; idx++)
    {
        CHECK_RC(SetFanRpm(rpm, idx));
    }

    return rc;
}

RC SocThermal::GetAllFanRpm(vector<UINT32>* pRpmList)
{
    MASSERT(pRpmList);
    MASSERT(pRpmList->size() == 0);

    RC rc;
    UINT32 numOfFans = GetNumOfFans();
    for (UINT32 idx = 0; idx < numOfFans; idx++)
    {
        UINT32 rpm;
        CHECK_RC(GetFanRpm(&rpm, idx));
        pRpmList->push_back(rpm);
    }

    return rc;
}

RC SocThermal::GetAllFanPwm(vector<UINT32>* pPwmList)
{
    MASSERT(pPwmList);
    MASSERT(pPwmList->size() == 0);

    RC rc;
    UINT32 numOfFans = GetNumOfFans();
    for (UINT32 idx = 0; idx < numOfFans; idx++)
    {
        UINT32 pwm;
        CHECK_RC(GetFanPwm(&pwm, idx));
        pPwmList->push_back(pwm);
    }

    return rc;
}

UINT32 SocThermal::GetNumOfFans() const
{
    Printf(Tee::PriLow, "Get number of fans: %u\n", m_NumOfFan);
    return m_NumOfFan;
}

///////// Concord Specific Implementation //////////////
RC SocThermalConcord::SetFanPwm(UINT32 pwm, UINT32 idx)
{
    RC rc;
    Printf(Tee::PriNormal, "Setting fan pwm to %u\n", pwm);
    string pwmPath = "/sys/devices/platform/pwm-fan/hwmon/hwmon0/pwm1";
    CHECK_RC(Xp::InteractiveFileWrite(pwmPath.c_str(), static_cast<INT32>(pwm)));

    return rc;
}

RC SocThermalConcord::SetFanRpm(UINT32 rpm, UINT32 idx)
{
    Printf(Tee::PriWarn, "Setting fan in rpm is not supported for this platform\n");
    return RC::OK;
}

RC SocThermalConcord::GetFanPwm(UINT32 *pPwm, UINT32 idx)
{
    MASSERT(pPwm);
    string pwmPath = "/sys/devices/platform/pwm-fan/hwmon/hwmon0/pwm1";
    INT32 pwm = 0;
    if (Xp::InteractiveFileRead(pwmPath.c_str(), &pwm) == RC::OK)
    {
        *pPwm = static_cast<UINT32>(pwm);
    }
    else
    {
        *pPwm = 0;
    }
    return RC::OK;
}

RC SocThermalConcord::GetFanRpm(UINT32 *pRpm, UINT32 idx)
{
    MASSERT(pRpm);
    string fanPath = "/sys/devices/platform/generic_pwm_tachometer/hwmon/hwmon1/rpm";
    INT32 fanSpeed = 0;
    if (Xp::InteractiveFileRead(fanPath.c_str(), &fanSpeed) == RC::OK)
    {
        Printf(Tee::PriLow, "Current fan speed in rpm: %d\n", fanSpeed);
        *pRpm = static_cast<UINT32>(fanSpeed);
    }
    else
    {
        *pRpm = 0;
    }

    return RC::OK;
}

RC SocThermalConcord::GetPowerSample(const string &chName, FLOAT32 *pPower)
{
    RC rc;
    MASSERT(pPower);
    
    if (chName != THERMALRES_SOC_CHIP_TOTAL_POWER_CH_NAME)
    {
        Printf(Tee::PriError,
            "Read power channel %s is not supported\n",
            chName.c_str());

        return RC::SOFTWARE_ERROR;
    }

    *pPower = 0;
    CHECK_RC(CheetAh::SocPtr()->StartPowerMeas());
    DEFER 
    {
        CheetAh::SocPtr()->EndPowerMeas();
    };

    for (UINT32 railIdx = 0; railIdx < 3; railIdx++)
    {
        UINT32 powerRead;
        CHECK_RC(CheetAh::SocPtr()->GetPowerMeas(railIdx, &powerRead));
        *pPower += static_cast<FLOAT32>(powerRead);
    }

    return rc;
}

RC SocThermalConcord::GetThermalSample(const string &chName, FLOAT32 *pTemp)
{
    MASSERT(pTemp);
    string thermalPath = "/sys/class/thermal/thermal_zone";
    static const std::map<string, UINT32> thermalMap = {
        {"CPU_MAX", 0},
        {"GPU_MAX", 1},
        {"SOC0_MAX", 5},
        {"SOC1_MAX", 6},
        {"SOC2_MAX", 7},
        {"CHIP_MAX", 8}
    };

    if (thermalMap.find(chName) == thermalMap.end())
    {
        return RC::BAD_PARAMETER;
    }

    INT32 temp = 0;
    if (Xp::InteractiveFileRead((
            thermalPath + to_string(thermalMap.at(chName)) + "/temp").c_str(), 
            &temp) == RC::OK)
    {            
        *pTemp = static_cast<FLOAT32>(temp/1000);
    }

    return RC::OK;
}