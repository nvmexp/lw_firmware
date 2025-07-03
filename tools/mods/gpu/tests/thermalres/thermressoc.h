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

#define THERMALRES_SOC_MIN_ALLOWED_R                (0.000001f)
#define THERMALRES_SOC_MAX_ALLOWED_R                (0.001f)
#define THERMALRES_SOC_CHIP_OFFSET_MAX_TEMP_CH_NAME "CHIP_MAX"
#define THERMALRES_SOC_CHIP_TOTAL_POWER_CH_NAME     "TOTAL_CHIP_POWER"

class SocThermal
{
public:
    SocThermal(UINT32 numOfFan) : m_NumOfFan(numOfFan) {};;
    virtual ~SocThermal() = default;

    UINT32 GetNumOfFans() const;
    RC SetAllFanPwm(UINT32 pwm);
    RC SetAllFanRpm(UINT32 rpm);
    RC GetAllFanRpm(vector<UINT32>* pRpmList);
    RC GetAllFanPwm(vector<UINT32>* pPwmList);
    virtual RC SetFanPwm(UINT32 pwm, UINT32 idx) = 0; // the range [0, 255]
    virtual RC SetFanRpm(UINT32 rpm, UINT32 idx) = 0;
    virtual RC GetFanPwm(UINT32* pPwm, UINT32 idx) = 0;
    virtual RC GetFanRpm(UINT32* pRpm, UINT32 idx) = 0;
    virtual RC GetPowerSample(const string& chName, FLOAT32* pPower) = 0;  //Unit: mW,
    virtual RC GetThermalSample(const string& chName, FLOAT32* pTemp) = 0; //Unit: degreeC

protected:
    UINT32 m_NumOfFan = 0;
};

class SocThermalConcord : public SocThermal
{
public:
    SocThermalConcord(UINT32 numOfFan) : SocThermal(numOfFan) {};
    ~SocThermalConcord() = default;

    //implement Concord specific fan control and sensor reading methods
    RC SetFanPwm(UINT32 pwm, UINT32 idx) override;
    RC SetFanRpm(UINT32 rpm, UINT32 idx) override;
    RC GetFanPwm(UINT32* pPwm, UINT32 idx) override;
    RC GetFanRpm(UINT32* pRpm, UINT32 idx) override;
    RC GetPowerSample(const string& chName, FLOAT32* pPower) override;
    RC GetThermalSample(const string& chName, FLOAT32* pTemp) override;
};

class ThermalResSoc : public ThermalRes
{
public:
    ThermalResSoc() = default;
    ~ThermalResSoc() = default;

    RC Setup() override;
    RC Cleanup() override;
    bool IsSupported() override;

private:
    unique_ptr<SocThermal>  m_pSocThermal;
    CheetAh::TegraBoardId     m_Platform       = CheetAh::TegraBoardId::Unknown;
    vector<UINT32>          m_OrigPwmList;
    const vector<string>    m_AllValidTempCh = { "GPU_MAX", "CPU_MAX", "SOC0_MAX",
                                                 "SOC1_MAX", "SOC2_MAX", "CHIP_MAX" };

    RC SocThermalInit();
    RC ConstructRConfigAndSensorInfo() override;
    RC ConstructRConfig
    (
        map<string, UINT32>& chNameToMaxIdleValMap,
        float allow_R_min,
        float allow_R_max
    );
    RC ConstructRConfigForAllTempCh();
    RC CheckFanOverrideConfig() override;
    RC ApplyFanOverrides() override;
    RC CollectPowerSamples() override;
    RC CollectThermalSamples() override;
    RC GetChipAvgTemp(FLOAT32* pLwrTemp) override;
    RC StartBgTest() override;
    RC StopBgTest() override;
    RC SetIdlePerfPoint() override;
    RC SetLoadPerfPoint() override;
    RC AcceleratePowerColw() override;
    RC SetAllFanPwm(UINT32 pwm) override; // the range [0, 255]
    RC SetAllFanRpm(UINT32 rpm) override;
    RC GetAllFanRpm(vector<UINT32>* pRpmList) override;
    RC GetAllFanPwm(vector<UINT32>* pPwmList) override;
    RC GetSocPlatform(CheetAh::TegraBoardId* pPlatform);
    bool IsThermalChValid(const string& chName) const;
    UINT32 GetMaxPwm() const;
};
