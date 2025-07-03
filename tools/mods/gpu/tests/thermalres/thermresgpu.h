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

#include "thermreslib.h"
#include "thermres.h"

#define THERMALRES_GPU_MIN_ALLOWED_R (0.000001f)
#define THERMALRES_GPU_MAX_ALLOWED_R (0.001f)

class ThermalResGpu : public ThermalRes
{
public:
    ThermalResGpu() = default;
    ~ThermalResGpu() = default;

    virtual RC   Setup();
    virtual RC   Cleanup();
    virtual bool IsSupported();

    SETGET_PROP(IdlePerfPoint, JSObject*);
    SETGET_PROP(LoadPerfPoint, JSObject*);

private:
    struct CoolerTableInfo
    {
        LW2080_CTRL_FAN_COOLER_INFO_PARAMS      infoParams;
        LW2080_CTRL_FAN_COOLER_STATUS_PARAMS    statusParams;
        LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS   ctrlParams;
    };

    struct PwrChannelInfo
    {
        UINT32 pwrAvgmW;
        UINT32 pwrMinmW;
        UINT32 pwrMaxmW;
        UINT32 voltuV;
        UINT32 lwrrmA;
        bool   bValid;
    };

    struct PowerSensorSample
    {
        UINT32 totalGpuPowermW;
        vector<PwrChannelInfo> channels;
    };

    struct PerfPointConfig
    {
        UINT32 PStateNum;
        string locationStr;
    };

    RC GetPerfPointConfig();
    RC SetPerfPoint(UINT32 pStateNum, const string& locationStr);
    RC SetIdlePerfPoint() override;
    RC SetLoadPerfPoint() override;
    RC GetTGPChannelInfo(string& tgpChName, UINT32* pTgpChIdx);
    RC BuildTempReadIdMap();
    RC GetFanCoolerTable
    (
        bool defaultValues,
        CoolerTableInfo& fanCoolerTable
    );
    RC SetFanCoolerTable
    (
        const UINT32 mask,
        const CoolerTableInfo& fanCoolerTable
    );
    RC GetPowerSensorSample(PowerSensorSample& pwrSample);
    bool IsFloorSwept
    (
        const ThermalSensors::ChannelInfos& chInfos,
        const string& chName,
        UINT32 chIdx
    );
    RC AcceleratePowerColw() override;
    RC ConstructRConfigAndSensorInfo() override;
    RC ConstructRConfig(map<string, UINT32>& chNameToMaxIdleValMap,
        float allow_R_min,
        float allow_R_max);
    RC ConstructRConfigForAllTempCh();
    RC CheckFanOverrideConfig() override;
    RC SetAllFanPwm(UINT32 dutyPct) override; // the range (0, 100]
    RC SetAllFanRpm(UINT32 rpm) override;
    RC GetAllFanRpm(vector<UINT32>* pRpmList) override;
    RC GetAllFanPwm(vector<UINT32>* pPwmList) override;
    RC CollectPowerSamples() override;
    RC CollectThermalSamples() override;
    RC ApplyFanOverrides() override;
    RC GetChipAvgTemp(FLOAT32* pLwrTemp) override;
    RC StartBgTest() override;
    RC StopBgTest() override;

    map<string, UINT32>      m_PwrChNameToIdxMap;
    map<string, UINT32>      m_TempChNameToIdxMap;
    map<string, UINT32>      m_TempChNameToReadIdxMap;
    Perf::PerfPoint          m_OrigPerfPoint;
    CoolerTableInfo          m_OrigCoolerTable;
    CoolerTableInfo          m_LwrrCoolerTable;
    UINT32                   m_CoolerMask;
    bool                     m_SupportsMultiFan;
    PerfPointConfig          m_IdlePerfPointConfig = { ILWALID_THERMRES_VAL_UINT32, "INVALID" };
    PerfPointConfig          m_LoadPerfPointConfig = { ILWALID_THERMRES_VAL_UINT32, "INVALID" };
    /* User specified test Arguments */
    JSObject*                m_IdlePerfPoint;
    JSObject*                m_LoadPerfPoint;
};
