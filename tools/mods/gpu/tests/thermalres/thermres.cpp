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

#include <math.h>
#include "core/include/abort.h"
#include "core/include/mle_protobuf.h"
#include "core/include/fileholder.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/gpudev.h"
#include "thermres.h"

// Exposed JS properties //
JS_CLASS_VIRTUAL_INHERIT(ThermalRes, GpuTest, "Thermal resistance base class");

CLASS_PROP_READWRITE(ThermalRes, Number, UINT32,
                     "Thermal Resistance Test Number");

CLASS_PROP_READWRITE(ThermalRes, BgTestNum, UINT32,
                     "Background Test Number in the Load Phase");

CLASS_PROP_READWRITE(ThermalRes, BgTestName, string,
                     "Background Test Name in the Load Phase");

CLASS_PROP_READWRITE(ThermalRes, LoadPhaseFirst, bool,
                     "Determine if Load starts first");

CLASS_PROP_READWRITE(ThermalRes, MaxHotSpotOffsetTemp, UINT32,
                     "Allowed Max HotSpot Offset Temperature");

CLASS_PROP_READWRITE(ThermalRes, IdleTimeMs, UINT32,
                     "How long to stay in the steady phase in milliseconds");

CLASS_PROP_READWRITE(ThermalRes, LoadTimeMs, UINT32,
                     "How long to stay in the heat phase in milliseconds");

CLASS_PROP_READWRITE(ThermalRes, PrintSamples, bool,
                     "Print the power, temperature, and R values to the log (not CSV)");

CLASS_PROP_READWRITE(ThermalRes, PrintSummary, bool,
                     "Prints a summary after each time the R values are checked");

CLASS_PROP_READWRITE(ThermalRes, SampleDelayMs, UINT32,
                     "How long to sleep between collecting samples in milliseconds");

CLASS_PROP_READWRITE(ThermalRes, LogCsv, bool,
                     "Output the samples to a CSV file");

CLASS_PROP_READWRITE(ThermalRes, IdleCsv, string,
                     "The name of the CSV file to output for the idle phase");

CLASS_PROP_READWRITE(ThermalRes, LoadCsv, string,
                     "The name of the CSV file to output for the load phase");

CLASS_PROP_READWRITE_JSARRAY(ThermalRes, Monitors, JsArray,
                    "The list of thermal/power channels to monitor");

CLASS_PROP_READWRITE_JSARRAY(ThermalRes, PowerLimits, JsArray,
                    "Limits for the power readings");

CLASS_PROP_READWRITE(ThermalRes, PowerIirGain, FLOAT32,
                     "Control the gain value of the power IIR filter");

CLASS_PROP_READWRITE(ThermalRes, TempIirGain, FLOAT32,
                     "Control the gain value of the thermal IIR filter");

CLASS_PROP_READWRITE(ThermalRes, IdleDelayMs, UINT32,
                     "How long to wait before collecting "
                     "power/thermal samples during idle");

CLASS_PROP_READWRITE_JSARRAY(ThermalRes, CheckpointTimesMs, JsArray,
                    "Specifies when to check the R values");

CLASS_PROP_READWRITE(ThermalRes, OverrideFan, bool,
                     "Overrides the fan(s) speed if true");

CLASS_PROP_READWRITE(ThermalRes, FanSettleTimeMs, UINT32,
                     "How long to wait for the fan(s) to colwerge to the target RPM/PWM");

CLASS_PROP_READWRITE_JSARRAY(ThermalRes, FanSettings, JsArray,
                     "RPM/PWM values for the fan(s) speed");

CLASS_PROP_READWRITE(ThermalRes, UseRpm, bool,
                     "Determines whether FanSettings is interpreted as RPM vs. PWM");

CLASS_PROP_READWRITE(ThermalRes, StabilityCriteria, bool,
                     "Controls whether or not the stability criteria phase is active."
                     "This option is mutually exclusive with FanOverrides");

CLASS_PROP_READWRITE(ThermalRes, StabilityTimeoutMs, UINT32,
                     "Controls the timeout interval for both the FastStabilize "
                     "and TempStability parts");

CLASS_PROP_READWRITE(ThermalRes, StabilityLoopMs, UINT32,
                     "Sets the time between temperature samples for both the"
                     "FastStabilize and TempStability parts");

CLASS_PROP_READWRITE(ThermalRes, TempFilter, FLOAT32,
                     "Sets the filter gain value used for filtering the "
                     "raw data collected by this phase");

CLASS_PROP_READWRITE(ThermalRes, TempStability, FLOAT32,
                     "The TempStability section finishes when dT falls below this"
                     "TempStability threshold (in degrees Celsius)");

CLASS_PROP_READWRITE(ThermalRes, StartPwm, FLOAT32,
                     "PWM value used for fan control during the FastStabilize part");

CLASS_PROP_READWRITE(ThermalRes, StablePwm, FLOAT32,
                     "PWM value used for fan control during TempStability "
                     "and the rest of the test");

CLASS_PROP_READWRITE(ThermalRes, GradientTrigger, FLOAT32,
                     "The threshold at which the FastStabilize part finishes. "
                     "The units are degC/s");

CLASS_PROP_READWRITE(ThermalRes, HasPowerCapping, bool,
                     "indicate whether the power capping configuration can be applied");

RC ThermalRes::GetMonitors(JsArray* val) const
{
     MASSERT(val != NULL);
     *val = m_Monitors;
      return RC::OK;
}

RC ThermalRes::SetMonitors(JsArray* val)
{
      MASSERT(val != NULL);
      m_Monitors = *val;
      return RC::OK;
}

RC ThermalRes::GetPowerLimits(JsArray* val) const
{
     MASSERT(val != NULL);
     *val = m_PowerLimits;
      return RC::OK;
}

RC ThermalRes::SetPowerLimits(JsArray* val)
{
      MASSERT(val != NULL);
      m_PowerLimits = *val;
      return RC::OK;
}

RC ThermalRes::GetFanSettings(JsArray* val) const
{
     MASSERT(val != NULL);
     *val = m_FanSettings;
      return RC::OK;
}

RC ThermalRes::SetFanSettings(JsArray* val)
{
      MASSERT(val != NULL);
      m_FanSettings = *val;
      return RC::OK;
}

RC ThermalRes::GetCheckpointTimesMs(JsArray* val) const
{
     MASSERT(val != NULL);
     *val = m_CheckpointTimesMs;
      return RC::OK;
}

RC ThermalRes::SetCheckpointTimesMs(JsArray* val)
{
      MASSERT(val != NULL);
      m_CheckpointTimesMs = *val;
      return RC::OK;
}
//////////////////////////////////////////////////////////////////////////////////////////

bool ThermalRes::IsSupported()
{
    return false;
}

RC ThermalRes::Setup()
{
    RC rc;
    CHECK_RC(GpuTest::Setup());

    if (m_PowerIirGain <= 0.0 || m_PowerIirGain > 1.0)
    {
        Printf(Tee::PriError, "%s (%.3f) must be in the range (0.0, 1.0]\n",
           "PowerIirGain", m_PowerIirGain);
        return RC::ILWALID_ARGUMENT;
    }

    if (m_TempIirGain <= 0.0 || m_TempIirGain > 1.0)
    {
        Printf(Tee::PriError, "%s (%.3f) must be in the range (0.0, 1.0]\n",
           "TempIirGain", m_TempIirGain);
        return RC::ILWALID_ARGUMENT;
    }

    CHECK_RC(GetCheckpointsAndFanSettings());
    CHECK_RC(InitThermResLibAndMonitors());
    CHECK_RC(CheckFanOverrideConfig());
    CHECK_RC(CreateHeader());

    return rc;
}

RC ThermalRes::Run()
{
    RC rc = RC::OK;
    if (m_StabilityCriteria)
    {
        CHECK_RC(EnterStability());
    }

    if (m_OverrideFan)
    {
        CHECK_RC(ApplyFanOverrides());
    }

    if (!m_LoadPhaseFirst)
    {
        CHECK_RC(EnterIdle());
        CHECK_RC(UpdateBaselineValue());
        CHECK_RC(AcceleratePowerColw());
        CHECK_RC(EnterLoad());
    }
    else
    {
        CHECK_RC(EnterLoad());
        CHECK_RC(UpdateBaselineValue());
        CHECK_RC(EnterIdle());
    }

    // If there are no checkpoints, then the test is in a "data collection"
    // mode, and we want to print the summary once at the end.
    if (m_CheckpointTimesMsInternal.size() == 0 || m_RconfigList[0].rLimits.size() == 0)
    {
        PrintSummary();
    }
    return rc;
}

RC ThermalRes::Cleanup()
{
    StickyRC rc;

    rc = GpuTest::Cleanup();
    return rc;
}

RC ThermalRes::GetCheckpointsAndFanSettings()
{
    RC rc;
    JavaScriptPtr pJavaScript;
    size_t checkPtLen =  m_CheckpointTimesMs.size();

    for (UINT32 i = 0; i < checkPtLen; i++)
    {
        UINT64 time;
        CHECK_RC(pJavaScript->FromJsval(m_CheckpointTimesMs[i], &time));
        m_CheckpointTimesMsInternal.push_back(time);
    }

    for (UINT32 i = 0; i < m_FanSettings.size(); i++)
    {
        UINT32 setting;
        CHECK_RC(pJavaScript->FromJsval(m_FanSettings[i], &setting));
        m_FanSettingsInternal.push_back(setting);
    }

    UINT32 prevCpTimeMs = 0;
    for (UINT32 cpIdx = 0; cpIdx < checkPtLen; cpIdx++)
    {
        UINT32 cpTimeMs = static_cast<UINT32>(m_CheckpointTimesMsInternal[cpIdx]);
        if (cpTimeMs <= prevCpTimeMs)
        {
            Printf(Tee::PriError, "CheckpointTimesMs entry %u is out of order\n", cpIdx);
            return RC::ILWALID_ARGUMENT;
        }
        if (cpTimeMs > m_LoadTimeMs)
        {
            Printf(Tee::PriError,
                "CheckpointTimesMs entry %u (%u) is greater than LoadTimeMs (%u)\n",
                cpIdx, cpTimeMs, m_LoadTimeMs);
            return RC::ILWALID_ARGUMENT;
        }
        prevCpTimeMs = cpTimeMs;
    }

    return rc;
}

RC ThermalRes::CreateHeader()
{
    // This is the header used in the CSV file and in the verbose output
    RC rc;
    vector<UINT32> fanRpmList;
    CHECK_RC(GetAllFanRpm(&fanRpmList));

    m_Header = "elapsedMs";
    string comma = ",";
    for (UINT32 i = 0; i < fanRpmList.size(); i++)
    {
        if (fanRpmList[i] != ILWALID_THERMRES_VAL_UINT32)
        {
            m_Header += comma + std::string("Fan") + std::to_string(i) + std::string("_RPM");
        }
    }
    for (UINT32 i = 0; i < m_RconfigList.size(); i++)
    {
        m_Header += comma + m_RconfigList[i].tempChName + std::string("(FILTERED)");
        m_Header += comma + m_RconfigList[i].pwrChName + std::string("(FILTERED)");
        m_Header += comma + m_RconfigList[i].tempChName + \
            std::string("_R") + std::string("(FILTERED)");
        m_Header += comma + m_RconfigList[i].tempChName + std::string("(RAW)");
        m_Header += comma + m_RconfigList[i].pwrChName + std::string("(RAW)");
        m_Header += comma + m_RconfigList[i].tempChName + \
            std::string("_R") + std::string("(RAW)");
    }
    if (!GetBoundGpuSubdevice()->IsSOC())
    {
        m_Header += std::string(", CALLWLATED_HOTSPOT_OFFSET");
    }

    if (m_LogCsv)
    {
        string ecidStr;
        CHECK_RC(GetBoundGpuSubdevice()->GetEcidString(&ecidStr));

        string suffix = ecidStr + std::string(".csv");
        if (m_IdleCsv == "")
        {
            m_IdleCsv = std::string("test") + std::to_string(m_Number) + \
                std::string("_idle_") + suffix;
        }
        if (m_LoadCsv == "")
        {
            m_LoadCsv = std::string("test") + std::to_string(m_Number) + \
                std::string("_load_") + suffix;
        }
    }

    return rc;
}

UINT32 ThermalRes::QuerySensorInfoIdxByName(const std::string& chName) const
{
    UINT32 index = ILWALID_THERMRES_VAL_UINT32;
    for (UINT32 i = 0; i < m_SensorInfoList.size(); i++)
    {
        if (m_SensorInfoList[i].chName == chName)
        {
            index = i;
            break;
        }
    }
    return index;
}

RC ThermalRes::InitThermResLibAndMonitors()
{
    RC rc;
    CHECK_RC(ConstructRConfigAndSensorInfo());
    m_pLib = make_unique<ThermalResLib>(m_RconfigList, m_SensorInfoList);
    const string& device = GetBoundGpuSubdevice()->GetName();
    for (const auto& rConfig : m_RconfigList)
    {
        UINT32 id;
        CHECK_RC(HandleThermResLibRC(m_pLib->CreateMonitor(device, rConfig, &id)));
    }

    return rc;
}

RC ThermalRes::HandleThermResLibRC(ThermalResLib::ThermResRC thermRC)
{
    RC rc;
    switch (thermRC)
    {
        case ThermalResLib::ThermResRC::OK:
            rc = RC::OK;
            break;
        case ThermalResLib::ThermResRC::POWER_TOO_LOW:
            rc = RC::POWER_TOO_LOW;
            break;
        case ThermalResLib::ThermResRC::POWER_TOO_HIGH:
            rc = RC::POWER_TOO_HIGH;
            break;
        case ThermalResLib::ThermResRC::TEMP_TOO_HIGH:
            rc = RC::TEMP_TOO_HIGH;
            break;
        case ThermalResLib::ThermResRC::THERMAL_RESISTANCE_TOO_LOW:
            rc = RC::THERMAL_RESISTANCE_TOO_LOW;
            break;
        case ThermalResLib::ThermResRC::THERMAL_RESISTANCE_TOO_HIGH:
            rc = RC::THERMAL_RESISTANCE_TOO_HIGH;
            break;
        case ThermalResLib::ThermResRC::BAD_PARAMETER:
            rc = RC::BAD_PARAMETER;
            break;
        default:
            Printf(Tee::PriNormal, "ThermalRes Library return an Error Code: %d\n", thermRC);
            rc = RC::SOFTWARE_ERROR;
            break;
    }
    return rc;
}

RC ThermalRes::CollectSamples()
{
    RC rc;
    CHECK_RC(CollectThermalSamples());
    CHECK_RC(CollectPowerSamples());

    return rc;
}

// Run at the max fan speed until the cooling rate slows, then change
// to a lower fan speed. The cooling rate is callwlated based on the
// temperature gradient (change) over time.
RC ThermalRes::FastStabilize()
{
    RC rc;

    UINT32  elapsedMs = 0;
    UINT32  elapsedMsOld = 0;
    UINT64  startTime = Xp::GetWallTimeMS();
    FLOAT32 lwrTemp;
    CHECK_RC(GetChipAvgTemp(&lwrTemp));
    FLOAT32 tempGradient = m_GradientTrigger;

    // Initialize the filtered values to be slightly higher than
    // the lwrTemp so that tempGradient will start at a slightly
    // higher value and avoid spuriously exiting early.
    FLOAT32 tempFiltered = lwrTemp + 1;
    FLOAT32 tempFilteredOld = lwrTemp + 1;
    FLOAT32 maxPwm = 100;
    if (GetBoundGpuSubdevice()->IsSOC())
    {
        maxPwm = 255;
    }

    VerbosePrintf("Starting Fast Stabilization at %.2f%% pwm\n", (m_StartPwm / maxPwm)*100);
    CHECK_RC(SetAllFanPwm(static_cast<UINT32>(m_StartPwm)));

    // Must be at the same PerfPoint as the idle portion of the test
    SetIdlePerfPoint();

    while (elapsedMs < m_StabilityTimeoutMs)
    {
        Tasker::Sleep(static_cast<FLOAT64>(m_StabilityLoopMs));
        elapsedMsOld = elapsedMs;
        elapsedMs = static_cast<UINT32>(Xp::GetWallTimeMS() - startTime);
        // Avoid division by zero and other bad behavior
        if (elapsedMs - elapsedMsOld < 1)
        {
            continue;
        }
        CHECK_RC(GetChipAvgTemp(&lwrTemp));
        tempFilteredOld = tempFiltered;
        tempFiltered += m_TempFilter * (lwrTemp - tempFiltered);
        tempGradient = 1000 * (tempFiltered - tempFilteredOld) / (elapsedMs - elapsedMsOld);
        VerbosePrintf("lwrTemp=%.3f, tempFiltered=%.3f, tempFilteredOld=%.3f, tempGradient=%.6f\n",
            lwrTemp, tempFiltered, tempFilteredOld, tempGradient);
        if (std::abs(tempGradient) < m_GradientTrigger)
        {
            VerbosePrintf("Changing fan speed to %.2f%% pwm\n", (m_StablePwm / maxPwm)*100);
            CHECK_RC(SetAllFanPwm(static_cast<UINT32>(m_StablePwm)));
            return rc;
        }
    }

    Printf(Tee::PriError, "FastStabilize() timed out\n");
    return RC::TIMEOUT_ERROR;
}

RC ThermalRes::TempStability()
{
    RC rc = RC::OK;

    VerbosePrintf("Starting temp stability\n");
    FLOAT32 lwrTemp;
    CHECK_RC(GetChipAvgTemp(&lwrTemp));

    FLOAT32 tempFiltered = lwrTemp;
    FLOAT32 tempFilteredOld = lwrTemp;
    FLOAT32 dT = 0;

    UINT32 elapsedMs = 0;
    UINT64 startTime = Xp::GetWallTimeMS();
    while (elapsedMs < m_StabilityTimeoutMs)
    {
        Tasker::Sleep(m_StabilityLoopMs);
        elapsedMs = static_cast<UINT32>(Xp::GetWallTimeMS() - startTime);

        CHECK_RC(GetChipAvgTemp(&lwrTemp));
        tempFilteredOld = tempFiltered;
        tempFiltered += m_TempFilter * (lwrTemp - tempFiltered);
        dT = tempFiltered - tempFilteredOld;
        VerbosePrintf("lwrTemp=%.3f, tempFiltered=%.3f, dT=%.4f\n", lwrTemp, tempFiltered, dT);
        if (std::abs(dT) < m_TempStability)
        {
            VerbosePrintf("Final thermal stability reached in %dms\n", elapsedMs);
            return rc;
        }
    }

    Printf(Tee::PriError, "TempStability() timed out\n");
    return RC::TIMEOUT_ERROR;
}

RC ThermalRes::EnterStability()
{
    RC rc;
    CHECK_RC(FastStabilize());
    CHECK_RC(TempStability());
    return rc;
}

RC ThermalRes::EnterIdle()
{
    StickyRC rc;
    VerbosePrintf("Running ThermRes Idle Phase for %ums\n", m_IdleDelayMs + m_IdleTimeMs);
    SetIdlePerfPoint();

    if (m_IdleDelayMs)
    {
        VerbosePrintf("Letting power settle for %ums\n", m_IdleDelayMs);
        Tasker::Sleep(static_cast<FLOAT64>(m_IdleDelayMs));
    }

    if (m_LogCsv)
    {
        VerbosePrintf("Writing CSV output to %s\n", m_IdleCsv.c_str());
        FileHolder fh;
        CHECK_RC(fh.Open(m_IdleCsv, "w"));
        fprintf(fh.GetFile(), "%s\n", m_Header.c_str());
    }

    UINT32 elapsedMs = 0;
    UINT64 startTime = Xp::GetWallTimeMS();
    const string& device = GetBoundGpuSubdevice()->GetName();
    Tee::Priority pri = m_PrintSamples ? Tee::PriNormal : GetVerbosePrintPri();
    VerbosePrintf("Collecting power/thermal samples for %ums\n", m_IdleTimeMs);
    Printf(pri, "%s\n", m_Header.c_str());
    do
    {
        elapsedMs = static_cast<UINT32>(Xp::GetWallTimeMS() - startTime);
        CHECK_RC(CollectSamples());
        bool isChecked = false;
        if (m_LoadPhaseFirst)
        {
            CHECK_RC(HandleThermResLibRC(m_pLib->ComputeRValue(device)));
            rc = HandleThermResLibRC(m_pLib->ValidateRAndPowerLimits(device,
                elapsedMs,
                &isChecked));
        }
        PrintLwrrentSample(elapsedMs, m_LoadPhaseFirst, false);
        if (isChecked)
        {
            PrintSummary();
        }
        CHECK_RC(Abort::Check());
        CHECK_RC(Tasker::Yield());
        Tasker::Sleep(static_cast<FLOAT64>(m_SampleDelayMs));
    }
    while (elapsedMs < m_IdleTimeMs);

    const vector<ThermalResLib::Monitor>& monList  = m_pLib->GetAllMonitor();
    const vector<ThermalResLib::SensorDataStat>& sensorList = m_pLib->GetAllSensorDataStat();
    for (UINT32 i = 0; i < monList.size(); i++)
    {
        const auto& mon = monList[i];
        MASSERT(mon.rIndex < m_RconfigList.size());
        const auto& rConfig = m_RconfigList[mon.rIndex];
        const string& tempChName = rConfig.tempChName;

        MASSERT(mon.pwrSensorIndex < sensorList.size());
        MASSERT(mon.tempSensorIndex < sensorList.size());
        const auto& pwrSensorStat = sensorList[mon.pwrSensorIndex];
        const auto& tempSensorStat = sensorList[mon.tempSensorIndex];
        VerbosePrintf("Monitor %s idle: Temp=%.2fdegC Power=%.0fmW\n",
            tempChName.c_str(), tempSensorStat.filteredValue, pwrSensorStat.filteredValue);
    }

    CHECK_RC(HandleThermResLibRC(m_pLib->ValidateIdleLimits(device)));
    CHECK_RC(ResetAllMonitorPower());
    return rc;
}

RC ThermalRes::ResetAllMonitorPower()
{
    RC rc = RC::OK;
    const vector<ThermalResLib::Monitor>& monList  = m_pLib->GetAllMonitor();

    for (UINT32 i = 0; i < monList.size(); i++)
    {
        CHECK_RC(HandleThermResLibRC(m_pLib->ResetMonitorPower(i)));
    }
    return rc;
}

RC ThermalRes::EnterLoad()
{
    StickyRC rc;
    VerbosePrintf("Running ThermRes Load Phase for %ums\n", m_LoadTimeMs);

    GpuSubdevice* pGpuSubdevice = GetBoundGpuSubdevice();
    SetLoadPerfPoint();

    CHECK_RC(StartBgTest());
    DEFERNAME(releaseBgTest)
    {
        StopBgTest();
    };

    if (m_LogCsv)
    {
        VerbosePrintf("Writing CSV output to %s\n", m_LoadCsv.c_str());
        FileHolder fh;
        CHECK_RC(fh.Open(m_LoadCsv, "w"));
        fprintf(fh.GetFile(), "%s\n", m_Header.c_str());
    }

    UINT32 elapsedMs = 0;
    UINT64 startTime = Xp::GetWallTimeMS();
    const string& device = pGpuSubdevice->GetName();
    VerbosePrintf("Collecting power/thermal samples for %ums\n", m_LoadTimeMs);
    do
    {
        elapsedMs = static_cast<UINT32>(Xp::GetWallTimeMS() - startTime);
        CHECK_RC(CollectSamples());
        bool isChecked = false;
        if (!m_LoadPhaseFirst)
        {
            CHECK_RC(HandleThermResLibRC(m_pLib->ComputeRValue(device)));
            rc = HandleThermResLibRC(m_pLib->ValidateRAndPowerLimits(device,
                elapsedMs,
                &isChecked));
        }
        PrintLwrrentSample(elapsedMs, !m_LoadPhaseFirst, true);
        if (isChecked)
        {
            if (!pGpuSubdevice->IsSOC())
            {
                rc = ValidateHotspotOffset();
            }
            PrintSummary();
        }
        CHECK_RC(Abort::Check());
        CHECK_RC(Tasker::Yield());
        Tasker::Sleep(static_cast<FLOAT64>(m_SampleDelayMs));
    }
    while (elapsedMs < m_LoadTimeMs);

    releaseBgTest.Cancel();
    CHECK_RC(StopBgTest());
    CHECK_RC(CheckLoadAvgPower());
    return rc;
}

RC ThermalRes::ValidateHotspotOffset()
{
    RC rc = RC::OK;
    if (m_MaxHotSpotOffsetTemp)
    {
        VerbosePrintf("Checking callwlated hotspot offset \n");
        if (m_CallwlatedHotSpotOffsetTemp > m_MaxHotSpotOffsetTemp)
        {
            Printf(Tee::PriError,
                "Callwlated hotspot offset Temp (%.2fdegC) is greater "
                "than allowed (%.2fdegC) \n",
                m_CallwlatedHotSpotOffsetTemp,
                static_cast<FLOAT32>(m_MaxHotSpotOffsetTemp));
            rc = RC::TEMP_TOO_HIGH;
        }
    }
    return rc;
}

RC ThermalRes::CheckLoadAvgPower()
{
    StickyRC rc;
    const vector<ThermalResLib::Monitor>&          monList  = m_pLib->GetAllMonitor();

    for (UINT32 monIdx = 0; monIdx < monList.size(); monIdx++)
    {
        const auto& mon = monList[monIdx];
        const auto& rConfig = m_RconfigList[mon.rIndex];

        if (rConfig.pwrLimits.size() > 0 &&
            rConfig.pwrLimits[0].first != ILWALID_THERMRES_VAL_UINT32)
        {
            UINT32 minPwr = rConfig.pwrLimits[0].first;
            UINT32 avgPower = static_cast<UINT32>(round(mon.avgPower_mW));
            if (avgPower < minPwr)
            {
                Printf(Tee::PriError,
                    "Monitor %u: %s power (%umW) is lower than allowed (%umW)\n",
                    monIdx,
                    rConfig.pwrChName.c_str(),
                    avgPower,
                    minPwr);
                rc = RC::POWER_TOO_LOW;
            }
        }

        if (rConfig.pwrLimits.size() > 0 &&
            rConfig.pwrLimits[0].second != ILWALID_THERMRES_VAL_UINT32)
        {
            UINT32 maxPwr = rConfig.pwrLimits[0].second;
            UINT32 avgPower = static_cast<UINT32>(round(mon.avgPower_mW));
            if (avgPower > maxPwr)
            {
                Printf(Tee::PriError,
                    "Monitor %u: %s power (%umW) is higher than allowed (%umW)\n",
                    monIdx,
                    rConfig.pwrChName.c_str(),
                    avgPower,
                    maxPwr);
                rc = RC::POWER_TOO_HIGH;
            }
        }
    }

    return rc;
}

RC ThermalRes::UpdateBaselineValue()
{
    RC rc;
    const string& device = GetBoundGpuSubdevice()->GetName();
    CHECK_RC(HandleThermResLibRC(m_pLib->UpdateMonitorBaselineValue(device)));

    return rc;
}

RC ThermalRes::PrintLwrrentSample(UINT32 elaspedTime, bool bGetRValue, bool bIsLoadPhase)
{
    RC rc;
    Tee::Priority pri = m_PrintSamples ? Tee::PriNormal : GetVerbosePrintPri();
    string line = Utility::StrPrintf("DATA: %7u ", elaspedTime);

    vector<UINT32> fanRpmList;
    CHECK_RC(GetAllFanRpm(&fanRpmList));

    for (UINT32 i = 0; i < fanRpmList.size(); i++)
    {
        if (fanRpmList[i] != ILWALID_THERMRES_VAL_UINT32)
        {
            line += Utility::StrPrintf("%-4u ", fanRpmList[i]);
        }
    }

    const vector<ThermalResLib::Monitor>&          monList  = m_pLib->GetAllMonitor();
    const vector<ThermalResLib::SensorDataStat>& sensorList = m_pLib->GetAllSensorDataStat();

    for (UINT32 j = 0; j < monList.size(); j++)
    {
        UINT32 tempIdx = monList[j].tempSensorIndex;
        UINT32 pwrIdx = monList[j].pwrSensorIndex;
        line += Utility::StrPrintf
        (
            "%5.2f %-6u %.3e %5.2f %-6u %.3e ",
            sensorList[tempIdx].filteredValue,
            static_cast<UINT32>(sensorList[pwrIdx].filteredValue),
            bGetRValue ? monList[j].filteredR : -1.0f,
            sensorList[tempIdx].value,
            static_cast<UINT32>(sensorList[pwrIdx].value),
            bGetRValue ? monList[j].r : -1.0f
        );
    }
    Printf(pri, "%s\n", line.c_str());

    if (!m_LogCsv)
    {
        return RC::OK;
    }

    string csvFile = bIsLoadPhase ? m_LoadCsv : m_IdleCsv;
    line = Utility::StrPrintf("%u", elaspedTime);
    for (UINT32 i = 0; i < fanRpmList.size(); i++)
    {
        if (fanRpmList[i] != ILWALID_THERMRES_VAL_UINT32)
        {
            line += Utility::StrPrintf(",%u", fanRpmList[i]);
        }
    }

    for (UINT32 j = 0; j < monList.size(); j++)
    {
        UINT32 tempIdx = monList[j].tempSensorIndex;
        UINT32 pwrIdx = monList[j].pwrSensorIndex;

        line += Utility::StrPrintf(",%.2f,%u,%.3e,%.2f,%u,%.3e",
            sensorList[tempIdx].filteredValue,
            static_cast<UINT32>(sensorList[pwrIdx].filteredValue),
            bGetRValue ? monList[j].filteredR : -1.0f,
            sensorList[tempIdx].value,
            static_cast<UINT32>(sensorList[pwrIdx].value),
            bGetRValue ? monList[j].r : -1.0f);
    }
    if (!GetBoundGpuSubdevice()->IsSOC())
    {
        line += Utility::StrPrintf(", %.2f",m_CallwlatedHotSpotOffsetTemp);
    }

    FileHolder fh;
    CHECK_RC(fh.Open(csvFile, "a"));
    fprintf(fh.GetFile(), "%s\n", line.c_str());
    return RC::OK;
}

void ThermalRes::PrintSummary()
{
    Tee::Priority pri = m_PrintSummary ? Tee::PriNormal : GetVerbosePrintPri();
    const vector<ThermalResLib::Monitor>&          monList  = m_pLib->GetAllMonitor();
    const vector<ThermalResLib::SensorDataStat>& sensorList = m_pLib->GetAllSensorDataStat();

    UINT32 lastCpIdx = m_CheckpointTimesMsInternal.size() ? \
        (monList[0].checkTimeIndex - 1) : ILWALID_THERMRES_VAL_UINT32;
    UINT32 lastCpTimeMs = m_LoadPhaseFirst ? m_IdleTimeMs : m_LoadTimeMs;
    if (lastCpIdx == ILWALID_THERMRES_VAL_UINT32 ||
        lastCpIdx >= m_CheckpointTimesMsInternal.size())
    {
        Printf(pri, "%s final summary:\n", GetName().c_str());
    }
    else
    {
        lastCpTimeMs = static_cast<UINT32>(m_CheckpointTimesMsInternal[lastCpIdx]);
        Printf(pri, "%s summary at checkpoint %u (%ums):\n",
            GetName().c_str(), lastCpIdx, lastCpTimeMs);
    }
    Printf(pri,
        (
            std::string("+------------------------------------------------------------------------------------------------------------------+\n")+
            std::string("| Name                    | Final R   | Max R     | Min R     | Final  | Idle  | Temp   | Final  | Idle  | Power   |\n")+
            std::string("|                         |           | Spec      | Spec      | Temp   | Temp  | Delta  | Power  | Power | Delta   |\n")+
            std::string("|                         | (C/mW)    | (C/mW)    | (C/mW)    | (C)    | (C)   | (C)    | (mW)   | (mW)  | (mW)    |\n")+
            std::string("|=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-|\n")
        ).c_str()
    );

    for (UINT32 j = 0; j < monList.size(); j++)
    {
        const auto& mon = monList[j];
        const UINT32 tempIdx = mon.tempSensorIndex;
        const UINT32 pwrIdx = mon.pwrSensorIndex;
        const UINT32 tempInfoIdx = sensorList[tempIdx].sensorInfoIdx;
        const ThermalResLib::RConfig& rConfig = m_RconfigList[mon.rIndex];
        Printf(pri, "| %-24s", m_SensorInfoList[tempInfoIdx].chName.c_str());
        Printf(pri, "| %-10.3e", mon.filteredR);

        FLOAT32 minR = ILWALID_THERMRES_R_VALUE;
        FLOAT32 maxR = ILWALID_THERMRES_R_VALUE;
        if (lastCpIdx != ILWALID_THERMRES_VAL_UINT32 &&
            rConfig.rLimits.size() &&
            lastCpIdx < rConfig.rLimits.size())
        {
            minR = rConfig.rLimits[lastCpIdx].first;
            maxR = rConfig.rLimits[lastCpIdx].second;
        }
        if (maxR != ILWALID_THERMRES_R_VALUE)
        {
            Printf(pri, "| %-10.3e", maxR);
        }
        else
        {
            Printf(pri, "| %-10s", "NOT SET");
        }
        if (minR != ILWALID_THERMRES_R_VALUE)
        {
            Printf(pri, "| %-10.3e", minR);
        }
        else
        {
            Printf(pri, "| %-10s", "NOT SET");
        }
        Printf(pri, "| %-7.2f", sensorList[tempIdx].filteredValue);
        Printf(pri, "| %-6.2f", mon.baseTemp_degC);
        Printf(pri, "| %+06.2f ", sensorList[tempIdx].filteredValue - mon.baseTemp_degC);
        Printf(pri, "| %-7u", static_cast<UINT32>(round(sensorList[pwrIdx].filteredValue)));
        Printf(pri, "| %-6u", static_cast<UINT32>(mon.basePower_mW));
        Printf(pri, "| %+-8d",
            static_cast<INT32>(sensorList[pwrIdx].filteredValue - mon.basePower_mW));
        Printf(pri, "|\n");

        Mle::Print(Mle::Entry::thermal_resistance)
            .therm_channel(m_SensorInfoList[tempInfoIdx].chName)
            .final_r(mon.filteredR)
            .max_r(maxR)
            .min_r(minR)
            .final_temp(sensorList[tempIdx].filteredValue)
            .idle_temp(mon.baseTemp_degC)
            .temp_delta(sensorList[tempIdx].filteredValue - mon.baseTemp_degC)
            .final_power(static_cast<UINT32>(round(sensorList[pwrIdx].filteredValue)))
            .idle_power(static_cast<UINT32>(mon.basePower_mW))
            .power_delta(static_cast<INT32>(sensorList[pwrIdx].filteredValue - mon.basePower_mW))
            .checkpt_time_ms(lastCpTimeMs)
            .checkpt_idx(lastCpIdx);
    }
    Printf(pri, "+------------------------------------------------------------------------------------------------------------------+\n"); //$
    if (!GetBoundGpuSubdevice()->IsSOC())
    {
        Printf(pri, "Callwlated HotSpot Offset = %.2fdegC \n", m_CallwlatedHotSpotOffsetTemp);
        if (m_MaxHotSpotOffsetTemp)
        {
            Printf(pri, "Maximum HotSpot Offset = %.2fdegC \n",
                static_cast<FLOAT32>(m_MaxHotSpotOffsetTemp));
        }
    }
}

void ThermalRes::PrintJsProperties(Tee::Priority pri)
{
    GpuTest::PrintJsProperties(pri);
    Printf(pri, "\tBgTestNum:\t\t\t%u\n", m_BgTestNum);
    Printf(pri, "\tMaxHotSpotOffsetTemp:\t\t%u\n", m_MaxHotSpotOffsetTemp);
    Printf(pri, "\tIdleTimeMs:\t\t\t%u\n", m_IdleTimeMs);
    Printf(pri, "\tLoadTimeMs:\t\t\t%u\n", m_LoadTimeMs);
    Printf(pri, "\tPrintSamples:\t\t\t%s\n", m_PrintSamples ? "true" : "false");
    Printf(pri, "\tPrintSummary:\t\t\t%s\n", m_PrintSummary ? "true" : "false");
    Printf(pri, "\tSampleDelayMs:\t\t\t%u\n", m_SampleDelayMs);
    Printf(pri, "\tLogCsv:\t\t\t\t%s\n", m_LogCsv ? "true" : "false");
    Printf(pri, "\tIdleCsv:\t\t\t%s\n", m_IdleCsv.c_str());
    Printf(pri, "\tLoadCsv:\t\t\t%s\n", m_LoadCsv.c_str());
    Printf(pri, "\tPowerIirGain:\t\t\t%.2f\n", m_PowerIirGain);
    Printf(pri, "\tTempIirGain:\t\t\t%.2f\n", m_TempIirGain);
    Printf(pri, "\tIdleDelayMs:\t\t\t%u\n", m_IdleDelayMs);
    Printf(pri, "\tOverrideFan:\t\t\t%s\n", m_OverrideFan ? "true" : "false");
    Printf(pri, "\tUseRpm:\t\t\t\t%s\n", m_UseRpm ? "true" : "false");
    Printf(pri, "\tStabilityCriteria:\t\t%s\n", m_StabilityCriteria ? "true" : "false");
    Printf(pri, "\tStabilityTimeoutMs:\t\t%u\n", m_StabilityTimeoutMs);
    Printf(pri, "\tStabilityLoopMs:\t\t%u\n", m_StabilityLoopMs);
    Printf(pri, "\tTempFilter:\t\t\t%.2f\n", m_TempFilter);
    Printf(pri, "\tTempStability:\t\t\t%.2f\n", m_TempStability);
    Printf(pri, "\tStartPwm:\t\t\t%.2f\n", m_StartPwm);
    Printf(pri, "\tStablePwm:\t\t\t%.2f\n", m_StablePwm);
    Printf(pri, "\tGradientTrigger:\t\t%.2f\n", m_GradientTrigger);
}