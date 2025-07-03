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

#pragma once

#include <algorithm>
#include <cmath>
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "core/include/utility.h"
#include "core/include/tasker.h"
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "gpu/tests/gputest.h"
#include "mods_reg_hal.h"
#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"
#include "thermreslib.h"

class ThermalRes : public GpuTest
{
public:
    ThermalRes() = default;
    virtual ~ThermalRes() = default;

    virtual RC   Setup();
    virtual RC   Run();
    virtual RC   Cleanup();
    virtual bool IsSupported();
    virtual void PrintJsProperties(Tee::Priority pri);

    SETGET_PROP(Number, UINT32);
    SETGET_PROP(BgTestNum, UINT32);
    SETGET_PROP(BgTestName, string);
    SETGET_PROP(LoadPhaseFirst, bool);
    SETGET_PROP(MaxHotSpotOffsetTemp, UINT32)
    SETGET_PROP(IdleTimeMs, UINT32);
    SETGET_PROP(LoadTimeMs, UINT32);
    SETGET_PROP(PrintSamples, bool);
    SETGET_PROP(PrintSummary, bool);
    SETGET_PROP(SampleDelayMs, UINT32);
    SETGET_PROP(LogCsv, bool);
    SETGET_PROP(IdleCsv, string);
    SETGET_PROP(LoadCsv, string);
    SETGET_JSARRAY_PROP_LWSTOM(Monitors);
    SETGET_JSARRAY_PROP_LWSTOM(PowerLimits);
    SETGET_PROP(PowerIirGain, FLOAT32);
    SETGET_PROP(TempIirGain, FLOAT32);
    SETGET_PROP(IdleDelayMs, UINT32);
    SETGET_JSARRAY_PROP_LWSTOM(CheckpointTimesMs);
    SETGET_PROP(OverrideFan, bool);
    SETGET_PROP(FanSettleTimeMs, UINT32);
    SETGET_JSARRAY_PROP_LWSTOM(FanSettings);
    SETGET_PROP(UseRpm, bool);
    SETGET_PROP(StabilityCriteria, bool);
    SETGET_PROP(StabilityTimeoutMs, UINT32);
    SETGET_PROP(StabilityLoopMs, UINT32);
    SETGET_PROP(TempFilter, FLOAT32);
    SETGET_PROP(TempStability, FLOAT32);
    SETGET_PROP(StartPwm, FLOAT32);
    SETGET_PROP(StablePwm, FLOAT32);
    SETGET_PROP(GradientTrigger, FLOAT32);
    SETGET_PROP(HasPowerCapping, bool);

protected:

    RC EnterStability();
    RC EnterIdle();
    RC EnterLoad();
    RC FastStabilize();
    RC TempStability();
    UINT32 QuerySensorInfoIdxByName(const string& chName) const;
    static RC HandleThermResLibRC(ThermalResLib::ThermResRC thermRC);
    RC InitThermResLibAndMonitors();
    RC CollectSamples();
    RC PrintLwrrentSample
    (
        UINT32 elaspedTime,
        bool bGetRValue,
        bool bIsLoadPhase
    );
    RC CreateHeader();
    RC GetCheckpointsAndFanSettings();
    RC ValidateHotspotOffset();
    void PrintSummary();
    RC ResetAllMonitorPower();
    RC CheckLoadAvgPower();
    virtual RC UpdateBaselineValue();
    virtual RC ConstructRConfigAndSensorInfo()          = 0;
    virtual RC CheckFanOverrideConfig()                 = 0;
    virtual RC ApplyFanOverrides()                      = 0;
    virtual RC GetChipAvgTemp(FLOAT32* pLwrTemp)        = 0;
    virtual RC StartBgTest()                            = 0;
    virtual RC StopBgTest()                             = 0;
    virtual RC SetIdlePerfPoint()                       = 0;
    virtual RC SetLoadPerfPoint()                       = 0;
    virtual RC SetAllFanPwm(UINT32 dutyPct)             = 0; // the range (0, 100]
    virtual RC SetAllFanRpm(UINT32 rpm)                 = 0;
    virtual RC GetAllFanPwm(vector<UINT32>* pPwmList)   = 0;
    virtual RC GetAllFanRpm(vector<UINT32>* pRpmList)   = 0;
    virtual RC CollectPowerSamples()                    = 0;
    virtual RC CollectThermalSamples()                  = 0;
    virtual RC AcceleratePowerColw()                    = 0;

    unique_ptr<ThermalResLib>               m_pLib;
    vector<ThermalResLib::RConfig>          m_RconfigList;
    vector<ThermalResLib::SensorInfo>       m_SensorInfoList;
    bool                                    m_HasPowerCapping = false;
    unique_ptr<CallbackArguments>           m_pCallbackArgs;
    string                                  m_Header;
    FLOAT32                                 m_CallwlatedHotSpotOffsetTemp = 0;
    vector<UINT64>                          m_CheckpointTimesMsInternal;
    vector<UINT32>                          m_FanSettingsInternal;
    string                                  m_BgTestName;
    UINT32                                  m_Number; // test number

    /* User specified test Arguments */
    UINT32                                  m_BgTestNum;
    bool                                    m_LoadPhaseFirst       = false;
    UINT32                                  m_MaxHotSpotOffsetTemp = 0;
    UINT32                                  m_IdleTimeMs           = 1000;
    UINT32                                  m_LoadTimeMs           = 7000;
    bool                                    m_PrintSamples         = false;
    bool                                    m_PrintSummary         = true;
    UINT32                                  m_SampleDelayMs        = 20;
    bool                                    m_LogCsv               = false;
    string                                  m_IdleCsv;
    string                                  m_LoadCsv;
    JsArray                                 m_Monitors;
    JsArray                                 m_PowerLimits;
    FLOAT32                                 m_PowerIirGain         = 0.085f;
    FLOAT32                                 m_TempIirGain          = 0.15f;
    UINT32                                  m_IdleDelayMs          = 300;
    JsArray                                 m_CheckpointTimesMs;
    bool                                    m_OverrideFan          = false;
    UINT32                                  m_FanSettleTimeMs      = 7000;
    bool                                    m_UseRpm               = false;
    JsArray                                 m_FanSettings;
    bool                                    m_StabilityCriteria    = false;
    UINT32                                  m_StabilityTimeoutMs   = 30000;
    UINT32                                  m_StabilityLoopMs      = 200;
    FLOAT32                                 m_TempFilter           = 0.1f;
    FLOAT32                                 m_TempStability        = 0.01f;
    FLOAT32                                 m_StartPwm             = 100;
    FLOAT32                                 m_StablePwm            = 30;
    FLOAT32                                 m_GradientTrigger      = 0.1f;
};

extern SObject ThermalRes_Object;

