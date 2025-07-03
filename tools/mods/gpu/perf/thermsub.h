/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  thermsub.h
 * @brief Information shared between thermal.cpp and therm_gl.cpp
 *
 *
 */

#pragma once
#ifndef INCLUDED_THERMSUB_H
#define INCLUDED_THERMSUB_H

#include "core/include/types.h"
#include "core/include/rc.h"
#include "Lwcm.h"
#include "core/include/gpu.h"
#include "ctrl/ctrl2080.h"
#include "gpu/utility/gpuerrcount.h"
#include "core/include/lwrm.h"
#include "device/utility/genericipmi.h"
#include "gpu/include/pmgrmutex.h"
#include <string>
#include <vector>
#include <set>
#include <map>

class GpuSubdevice;
class TestDevice;
class SmbPort;
class SmbManager;

//--------------------------------------------------------------------
//! \brief Class to keep track of the thermal OverTemp event counts.
//!
//! Error limits are controlled by JS properties:
//!   g_OverTempCountVerbose
//!   g_OverTempCountAllowed (per test limit)
//!   g_OverTempCountAllowedTotal
//!   g_OverTempCountDisabled
//!
class OverTempCounter : public GpuErrCounter
{
public:
    enum TempType
    {
        INTERNAL_TEMP,
        EXTERNAL_TEMP,
        MEMORY_TEMP,
        RM_EVENT,
        VOLTERRA_TEMP_1,   // Keep all the volterra temperatue enums together
        VOLTERRA_TEMP_2,   // as well as the last set of enums.  Since this
        VOLTERRA_TEMP_3,   // type is callwlated, keeping them last ensures an
        VOLTERRA_TEMP_4,   // error if the callwlation overruns
        VOLTERRA_TEMP_LAST = VOLTERRA_TEMP_4,
        SMBUS_TEMP,
        IPMI_TEMP,
        TSENSE_OFFSET_MAX_TEMP,
        NUM_TEMPS
    };
    OverTempCounter(TestDevice *pDev);
    RC Initialize();
    RC Update(TempType type, INT32 Temp);
    RC SetTempRange(TempType type, INT32 Min, INT32 Max);
    void EnableRangeMonitoring(bool bEnable);
    void BeginExpectingErrors(int idx);
    void EndExpectingErrors(int idx);
    void EndExpectingAllErrors();
    bool IsExpecting(int idx);
    RC ReadExpectedErrorCount(UINT64 * pCount);
    RC GetTempRange(TempType type, INT32 * pMin, INT32 * pMax);
protected:
    virtual RC ReadErrorCount(UINT64 * pCount);
    virtual void OnCheckpoint(const CheckpointData *pData) const;
    virtual RC OnError(const ErrorData *pData);
private:
    using TempLimits = pair<INT32, INT32>;
    struct OverTempData
    {
        bool  bActive;
        INT32 Min;
        INT32 Max;
        map<TempLimits, vector<INT32>> TempData;
        UINT64 OverTempCount;
        bool   IsExpecting; // if true we are expecting errors
        vector<INT32> ExpectedTempData;
        UINT64 ExpectedOverTempCount;
    };
    vector<OverTempData> m_OverTempData;
};

//--------------------------------------------------------------------
// RM has provided two different thermal sensors models:
//
// Execute:
//   Has "targets" (physical heat sources), "providers" (physical sensors), and
//   "sensors" (logical pairings of targets and providers). API is designed to
//   handle many targets, providers, and sensors, but RM implementation supports
//   only 1 target. Reports temperature in INT32 degrees Celsius.
//
//   Control calls:
//     LW2080_CTRL_CMD_THERMAL_SYSTEM_EXELWTE_V2
//
// TSM 2.0:
//   The newest, available and preferred starting with Maxwell. Based on
//   "channels", each with an underlying "device" (physical sensor), with
//   different channel "classes" targeting different physical heat sources.
//   Supports multiple sensors. Reports temperature in LwTemp (256ths of
//   degrees Celsius).
//
//   Control calls:
//     LW2080_CTRL_CMD_THERMAL_DEVICE_GET_INFO
//     LW2080_CTRL_CMD_THERMAL_CHANNEL_GET_INFO
//     LW2080_CTRL_CMD_THERMAL_CHANNEL_GET_STATUS
//     LW2080_CTRL_CMD_THERMAL_CHANNEL_GET_CONTROL
//     LW2080_CTRL_CMD_THERMAL_CHANNEL_SET_CONTROL
//
// In MODS, ExelwteSensors interfaces with the Execute model. TSM20Sensors
// interfaces with the TSM 2.0 model. ThermalSensors provides an abstraction
// over both.
//
// For CheetAh, the thermal APIs are completely different.
// There are 2 different methods for obtaining thermal sensor info
//
// It should be noted that the following APIs allow mods to read from a
// different set of physical heat sources from dGPU. On CheetAh, you can
// read from the CPU, GPU, PLL, or memory (actually the on-chip memory
// I/O ports).
//
// Sys FS file reads:
//   This isn't really an API per-se, instead, the OS is providing linux style
//   device files which a user-land application (like mods) can then open,
//   read, and close. These files can be found in the following location:
//   /sys/kernel/debug/tegra_soctherm/. These files are safe to use in kernel
//   mode.
//
// TegraSensors is a abstract subclass of ThermalSensors. This is done because
// the CheetAh APIs only differ in how they access individual sensors.
// SocThermControllerSensors interacts with the SocThermController interface
// and SocSysFsSensors interacts with the sys fs file reads; they are both
// Subclasses of TegraSensors

class ThermalSensors
{
public:
    enum Sensor {
        GPU     // Keep for legacy reasons
       ,GPU_AVG = GPU
       ,GPU_MAX
       ,BOARD
       ,MEMORY
       ,POWER_SUPPLY
       ,VOLTERRA_1
       ,VOLTERRA_2
       ,VOLTERRA_3
       ,VOLTERRA_4
       ,GPU_BJT
       ,LWL_BJT
       ,DRAM_TEMP
       ,SYS_BJT
       ,SCI_BJT
    };

    // This struct is used to communicate with bglogger
    // TODO: remove some fields since they are now redundant to ChannelInfo
    struct ChannelTempData
    {
        UINT32 chIdx = ~0;      // Channel index
        FLOAT32 temp = 0;       // Temp reading
        UINT32 devIdx = ~0;     // Thermal device index
        UINT32 devClass = ~0;   // Thermal device class
        UINT32 provIdx = ~0;    // Thermal provider index
        union
        {
            UINT32 tsoscIdx = ~0;   // TSOSC GPC index, only applies for TSOSC devices
            UINT32 i2cDevIdx;       // I2C device index, only applies for I2C devices
            UINT32 siteIdx;         // site index, only applies for HBM devices
        };
    };

    // This map is used to communicate with BJT temperature logger
    // Map of Pair <LWRRRENT_TEMP, LWRRENT_OFFSET_TEMP> with gpcIdx as key
    // Each Pair is recorded per BJT and maintained in a Vector
    using TsenseBJTTempMap = map<UINT32, vector<pair<FLOAT32, FLOAT32>>>;

    // This map is used to communicate with DRAM temperature logger
    // Key is tuple of <FBPA_VIRT, SUBP, CHIP (for x8 clamshell operation), IC (independent channel aka pseudochannel)>
    using DramTempMap = map<tuple<UINT08, UINT08, UINT08, UINT08>, INT32>;

    //! Thin wrapper around a RM THERMAL_CHANNEL_INFO object
    struct ChannelInfo
    {
        LW2080_CTRL_THERMAL_CHANNEL_INFO RmInfo = {};
        string Name = "";
        bool IsFloorswept = false;

        ChannelInfo() = default;
        ChannelInfo(const LW2080_CTRL_THERMAL_CHANNEL_INFO& rmInfo, const string& name) :
            RmInfo(rmInfo), Name(name) {}
    };
    using ThermChIdx = UINT32;
    using ChannelInfos = map<ThermChIdx, ChannelInfo>;

    static RC TranslateLw2080TargetTypeToThermalSensor(UINT32 lw2080TargetType,
                                                       Sensor *outSensor);

    virtual RC ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent) = 0;
    virtual RC IsSensorPresent(Sensor sensor, bool *pIsPresent) = 0;
    virtual RC GetTemp(Sensor sensor, FLOAT32 *ptemp) = 0;
    virtual RC GetTempsViaChannels(vector<ChannelTempData> *pTemp) = 0;
    virtual RC GetTempViaChannel(UINT32 chIdx, ChannelTempData *pTemp) = 0;
    virtual RC SetSimulatedTemp(Sensor sensor, FLOAT32 temp) = 0;
    virtual RC ClearSimulatedTemp(Sensor sensor) = 0;
    virtual RC PrintSensors(Tee::Priority pri = Tee::PriNormal) = 0;
    virtual RC GetChannelIdx(Sensor sensor, UINT32* idx) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetTempSnapshot
    (
        map<string, ThermalSensors::TsenseBJTTempMap> *pBjtTemps,
        const vector<ThermalSensors::Sensor> &sensors
    ) { return RC::UNSUPPORTED_FUNCTION; }
    virtual RC GetChannelInfos(ChannelInfos* pChInfos) const { return RC::UNSUPPORTED_FUNCTION; }

    virtual ~ThermalSensors() = 0;
    virtual RC SetTempViaChannel
    (
        UINT32 chIdx,
        FLOAT32 temp
    ) { return RC::UNSUPPORTED_FUNCTION; }

protected:
    ThermalSensors(GpuSubdevice *sbDev);

    static map<UINT32, Sensor> CreateLw2080TargetTypeToThermalSensorsMap();
    static map<UINT32, Sensor> lw2080TargetTypeToThermalSensorsMap;

    GpuSubdevice *m_pSubdev;
};

class TSM20Sensors : public ThermalSensors
{
public:
    TSM20Sensors(RC *rc, GpuSubdevice *sbDev);

    RC ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent) override;
    RC IsSensorPresent(Sensor sensor, bool *pIsPresent) override;
    RC GetTemp(Sensor sensor, FLOAT32 *temp) override;
    RC GetTempsViaChannels(vector<ChannelTempData> *pTemp) override;
    RC GetTempViaChannel(UINT32 chIdx, ChannelTempData *pTemp) override;
    RC SetSimulatedTemp(Sensor sensor, FLOAT32 temp) override;
    RC ClearSimulatedTemp(Sensor sensor) override;
    RC PrintSensors(Tee::Priority pri = Tee::PriNormal) override;
    RC GetChannelIdx(Sensor sensor, UINT32* idx) override;
    RC GetChannelInfos(ChannelInfos* pChInfos) const override;
    static const char *ProviderIndexToString
    (
        UINT32 devClass,
        UINT32 thermDevProvIdx
    );
    RC SetTempViaChannel(UINT32 chIdx, FLOAT32 temp) override;

private:
    RC ReadChannel(UINT32 channelIdx, FLOAT32 *temp);
    RC ReadChannels(UINT32 chMask, vector<FLOAT32> *temp);

    static const char *DeviceClassStr(UINT32 devClass);
    static const char *ChanTypeStr(UINT32 chanType);

    void FillTempDataMiscInfo(UINT32 chIdx, ChannelTempData* pTemp);

    RC HandleFloorsweptChannels();

    // Lists the LW2080_CTRL_THERMAL_DEVICE_INFO values
    // returned by LW2080_CTRL_CMD_THERMAL_DEVICE_GET_INFO
    // with devIdx as a key
    map<UINT32, LW2080_CTRL_THERMAL_DEVICE_INFO> m_Devices;

    // Lists the channels (each with a LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_*
    // value) returned by LW2080_CTRL_CMD_THERMAL_CHANNEL_GET_INFO with channelIdx
    // as a key
    ChannelInfos m_Channels;

    // Stores primary channel index for each channel type from
    // LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_<xyz>
    UINT08 m_PriChIdx[LW2080_CTRL_THERMAL_THERM_CHANNEL_TYPE_MAX_COUNT];
};

class ExelwteSensors : public ThermalSensors
{
public:
    ExelwteSensors(RC *rc, GpuSubdevice *sbDev);

    virtual RC ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent);
    virtual RC IsSensorPresent(Sensor sensor, bool *pIsPresent);
    virtual RC GetTemp(Sensor sensor, FLOAT32 *temp);
    virtual RC GetTempsViaChannels(vector<ChannelTempData> *pTemp);
    virtual RC GetTempViaChannel(UINT32 chIdx, ChannelTempData *pTemp);
    virtual RC SetSimulatedTemp(Sensor sensor, FLOAT32 temp);
    virtual RC ClearSimulatedTemp(Sensor sensor);
    virtual RC PrintSensors(Tee::Priority pri = Tee::PriNormal);

private:
    RC ReadSensor(FLOAT32 *temp);
    RC ConstructExelwteSensors();
    map<Sensor, UINT32> m_presentSensors;
};

class TegraSensors : public ThermalSensors
{
public:
    TegraSensors(RC *rc, GpuSubdevice *sbDev);

    virtual RC ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent);
    virtual RC IsSensorPresent(Sensor sensor, bool *pIsPresent);
    virtual RC GetTemp(Sensor sensor, FLOAT32 *temp);
    virtual RC GetTempsViaChannels(vector<ChannelTempData> *pTemp);
    virtual RC GetTempViaChannel(UINT32 chIdx, ChannelTempData *pTemp);
    virtual RC SetSimulatedTemp(Sensor sensor, FLOAT32 temp);
    virtual RC ClearSimulatedTemp(Sensor sensor);
    virtual RC PrintSensors(Tee::Priority pri = Tee::PriNormal);
    static RC GetTegraGpuTemp(FLOAT32 *temp);
};

class GpuBJTSensors : public ThermalSensors
{
public:
    GpuBJTSensors(RC *rc, GpuSubdevice *sbDev);

    static constexpr FLOAT32 ILWALID_TEMP = -1000.0f;

    virtual RC ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent);
    virtual RC IsSensorPresent(Sensor sensor, bool *pIsPresent);
    virtual RC GetTemp(Sensor sensor, FLOAT32 *ptemp);
    virtual RC GetTempsViaChannels(vector<ChannelTempData> *pTemp);
    virtual RC GetTempViaChannel(UINT32 chIdx, ChannelTempData *pTemp);
    virtual RC SetSimulatedTemp(Sensor sensor, FLOAT32 temp);
    virtual RC ClearSimulatedTemp(Sensor sensor);
    virtual RC PrintSensors(Tee::Priority pri = Tee::PriNormal);
    virtual RC GetTempSnapshot
    (
        map<string, ThermalSensors::TsenseBJTTempMap> *pBjtTemps,
        const vector<ThermalSensors::Sensor> &sensors
    );
    virtual RC SetTempViaChannel
    (
        UINT32 chIdx,
        FLOAT32 temp
    ) { return RC::UNSUPPORTED_FUNCTION; }
private:
    RC SetSnapShotTrigger();
    RC GetGpcBjtTempSnapshot(ThermalSensors::TsenseBJTTempMap *pTemp);
    RC GetLwlBjtTempSnapshot(ThermalSensors::TsenseBJTTempMap *pTemp);
    RC GetSysBjtTempSnapshot(ThermalSensors::TsenseBJTTempMap *pTemp);
    RC GetSciBjtTemp(ThermalSensors::TsenseBJTTempMap *pTemp);

    RC AcquireSnapshotMutex();
    RC ReleaseSnapShotMutex();

    unique_ptr<PmgrMutexHolder> m_pPmgrMutex;
};

class DramTempSensors : public ThermalSensors
{
public:
    DramTempSensors(RC* rc, GpuSubdevice* sbDev);

    RC ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent) override;
    RC IsSensorPresent(Sensor sensor, bool *pIsPresent) override;
    RC GetTemp(Sensor sensor, FLOAT32 *ptemp) override;
    RC GetTempsViaChannels(vector<ChannelTempData> *pTemp) override;
    RC GetTempViaChannel(UINT32 chIdx, ChannelTempData *pTemp) override;
    RC SetSimulatedTemp(Sensor sensor, FLOAT32 temp) override;
    RC ClearSimulatedTemp(Sensor sensor) override;
    RC PrintSensors(Tee::Priority pri = Tee::PriNormal) override;

    INT32 DecodeTemp(UINT32 raw, bool clamshell);
    RC GetDramTempSnapshot(ThermalSensors::DramTempMap *pTemp);
};

class StubSensors : public ThermalSensors
{
//! A class with functions that effectively do nothing
//! This allows support for platforms which don't support thermal opcodes or TSM2.0
public:
    StubSensors(RC *rc, GpuSubdevice *sbDev);

    virtual RC ShouldSensorBePresent(Sensor sensor, bool* pShouldBePresent);
    virtual RC IsSensorPresent(Sensor sensor, bool *pIsPresent);
    virtual RC GetTemp(Sensor sensor, FLOAT32 *temp);
    virtual RC GetTempsViaChannels(vector<ChannelTempData> *pTemp);
    virtual RC GetTempViaChannel(UINT32 chIdx, ChannelTempData *pTemp);
    virtual RC SetSimulatedTemp(Sensor sensor, FLOAT32 temp);
    virtual RC ClearSimulatedTemp(Sensor sensor);
    virtual RC PrintSensors(Tee::Priority pri = Tee::PriNormal);

    static const FLOAT32 STUB_TEMPERATURE;
};

//--------------------------------------------------------------------
class Thermal
{
public:
    Thermal(GpuSubdevice *pSubdev);
    ~Thermal();

    RC Initialize();
    RC Cleanup();

    //! this is the 'local temp' in RM term
    //! this is detected by external sensor
    RC GetAmbientTemp(FLOAT32 *pTemp);

    //! This is the temperature on die - detected by internal sensor
    RC GetChipTempViaInt(FLOAT32 *pTemp);

    //! This is the temperature on die - detected by external sensor
    RC GetChipTempViaExt(FLOAT32 *pTemp);

    //! This is the memory temperature
    RC GetChipMemoryTemp(FLOAT32 *pTemp);

    RC GetShouldExtBePresent(bool *pPresent);
    RC GetShouldIntBePresent(bool *pPresent);

    RC GetExternalPresent(bool *pPresent);
    RC GetInternalPresent(bool *pPresent);
    RC GetMemorySensorPresent(bool *pPresent);

    // TODO: create a enum for this?
    RC GetPrimarySensorType(UINT32 *pType);
    //! check if there is a primary sensor
    RC GetPrimaryPresent(bool *pPresent);
    //! get die temperature from primary sensor
    RC GetChipTempViaPrimary(FLOAT32 *pTemp);

    //! Note that these limits are associated with the primary sensor
    RC GetChipHighLimit(FLOAT32 *pTemp);
    RC GetHwfsEventInfo(LW2080_CTRL_THERMAL_HWFS_EVENT_SETTINGS_PARAMS *pInfo, UINT32 eventId);
    RC GetHasChipHighLimit(bool *pHighLimitSupported);
    RC SetChipHighLimit(FLOAT32 Temp);
    RC GetChipShutdownLimit(FLOAT32 *pTemp);
    RC SetChipShutdownLimit(FLOAT32 Temp);

    // todo cleanups: merge the interface for Ext/Int/Smbus TempOffset
    RC SetExtTempOffset(INT32 Offset);
    RC GetForceInt(bool *pForced);
    RC GetForceExt(bool *pForced);
    RC GetForceSmbus(bool *pForced);
    RC GetExtTempOffset(INT32 *pOffset);
    RC GetIntTempOffset(INT32 *pOffset);
    RC SetIntTempOffset(INT32 Offset);

    RC InitializeSmbusDevice();
    RC GetSmbusTempOffset(INT32 *pOffset);
    RC SetSmbusTempOffset(INT32 Offset);
    RC GetSmbusTempSensor(INT32 *pSensor);
    RC SetSmbusTempSensor(INT32 Sensor);
    RC GetSmbusTempDevice(string *pName);
    RC SetSmbusTempDevice(string Name);
    bool IsSmbusTempOffsetSet(){return m_SmbusOffsetSet;};
    //! This is the temperature in the system - detected by Smbus sensor
    RC GetChipTempViaSmbus(INT32 *pTemp);

    RC InitializeIpmiDevice();
    RC SetIpmiTempOffset(INT32 Offset);
    RC GetIpmiTempOffset(INT32 *pOffset);
    RC GetIpmiTempSensor(INT32 *pSensor);
    RC SetIpmiTempSensor(INT32 Sensor);
    RC SetIpmiTempDevice(string Name);
    RC GetIpmiTempDevice(string *pName);
    bool IsIpmiTempOffsetSet(){return m_IpmiOffsetSet;};
    RC GetChipTempViaIpmi(INT32 *pTemp);

    RC InitializeVolterra();
    RC GetVolterraSlaves(set<UINT32> *pVolterraSlaves);
    RC GetVolterraPresent(UINT32 SlaveId, bool *pPresent);
    RC GetIsVolterraPresent(bool *pPresent);
    RC GetVolterraSlaveTemp(UINT32 SlaveId, UINT32 *pTemp);
    RC GetVolterraMaxTempDelta(UINT32 *pMaxDelta);
    RC SetVolterraMaxTempDelta(UINT32 MaxDelta);

    bool IsExtTempOffsetSet(){return m_ExtOffsetSet;};
    bool IsIntTempOffsetSet(){return m_IntOffsetSet;};

    RC GetIsSupported(bool *pIsSupported);
    RC CheckIsSupported();

    RC GetVbiosExtTempOffset(INT32 *pOffset);
    RC GetVbiosIntTempOffset(INT32 *pOffset);

    RC DumpThermalTable(UINT32 TableNum);
    RC ResetThermalTable();
    RC ReadThermalTable(UINT32 TableNum,
                         UINT32 *pTable,
                         UINT32 *pTableSize,
                         UINT32 *pFlag,
                         UINT32 *pVersion);
    RC WriteThermalTable(UINT32 *pTable, UINT32 Size, UINT32 Version);

    RC InitOverTempCounter();
    RC ShutdownOverTempCounter();
    RC GetOverTempCounter(UINT32 *pCount);
    RC GetExpectedOverTempCounter(UINT32 *pCount);
    void IncrementOverTempCounter();
    void IncrementHwfsEventCounter(UINT32 eventId);
    RC GetClockThermalSlowdownAllowed(bool* pAllowed);
    RC SetClockThermalSlowdownAllowed(bool allowed);

    // Pmu Fan Control Block
    RC GetPmuFanCtrlBlk(LW2080_CTRL_PMU_FAN_CONTROL_BLOCK *pFCB);
    RC SetPmuFanCtrlBlk(const LW2080_CTRL_PMU_FAN_CONTROL_BLOCK &FCB);

    enum CoolerPolicy
    {
        CoolerPolicyNone
                = LW2080_CTRL_THERMAL_COOLER_POLICY_NONE,
        CoolerPolicyManual      //!< set this to make RM leave fan speed alone
                = LW2080_CTRL_THERMAL_COOLER_POLICY_MANUAL,
        CoolerPolicyPerf        //!< fan speed is set by perf-table (PState)
                = LW2080_CTRL_THERMAL_COOLER_POLICY_PERF,
        CoolerPolicyDiscrete    //!< RM sets one of N fan speeds
                = LW2080_CTRL_THERMAL_COOLER_POLICY_TEMPERATURE_DISCRETE,
        CoolerPolicyContinuous  //!< ADT7473 smoothly varies fan speed
                = LW2080_CTRL_THERMAL_COOLER_POLICY_TEMPERATURE_CONTINUOUS,
        CoolerPolicyContinuousSw//!< RM smoothly varies fan speed
                = LW2080_CTRL_THERMAL_COOLER_POLICY_TEMPERATURE_CONTINUOUS_SW,
    };
    //! Get the current CoolerPolicy values from RM.
    RC GetCoolerPolicy(UINT32 * pPolicy);
    //! Set the cooler policy.
    RC SetCoolerPolicy(UINT32 policy);
    //! Get the default minimum fan speed.
    RC GetDefaultMinimumFanSpeed(UINT32 * pPct);
    //! Get the default maximum fan speed.
    RC GetDefaultMaximumFanSpeed(UINT32 * pPct);
    //! Get the current minimum fan speed.
    RC GetMinimumFanSpeed(UINT32 * pPct);
    //! Get the current maximum fan speed.
    RC GetMaximumFanSpeed(UINT32 * pPct);
    //! Get the current fan speed.
    RC GetFanSpeed(UINT32 * pPct);
    //! Report whether or not fan activity sense is supported.
    RC GetIsFanActivitySenseSupported(bool * isFanActivitySenseSupported);
    //! Report whether or not the fan is lwrrently spinning.
    RC GetIsFanSpinning(bool * isFanSpinning);

    //! Check whether or not Fan 2.0+ is enabled
    RC SupportsMultiFan(bool * supportsMultiFan);

    //! Report whether the fan's RPM sense is supported or not
    RC GetIsFanRPMSenseSupported(bool * isRPMSenseSupported);
    //! Reset cooler settings
    RC ResetCoolerSettings();

    RC GetTsenseHotspotOffset(FLOAT32 *pOffset);

    RC GetThermalSlowdownRatio(Gpu::ClkDomain domain, FLOAT32 *pRatio) const;

    RC SetSimulatedTemperature(UINT32 Sensor, INT32 Temp);
    RC ClearSimulatedTemperature(UINT32 Sensor);

    // Fan 2.0+ controls
    RC GetFanPolicyMask(UINT32 *);
    RC DumpFanPolicyTable(bool);
    RC GetFanPolicyStatus(LW2080_CTRL_FAN_POLICY_STATUS_PARAMS *pFanPolicyStatusParams);
    RC GetFanPolicyTable(LW2080_CTRL_FAN_POLICY_INFO_PARAMS *,
                         LW2080_CTRL_FAN_POLICY_STATUS_PARAMS *,
                         LW2080_CTRL_FAN_POLICY_CONTROL_PARAMS *);
    RC SetFanPolicyTable(LW2080_CTRL_FAN_POLICY_CONTROL_PARAMS  &);

    RC GetFanCoolerMask(UINT32 *);
    RC DumpFanCoolerTable(bool);
    RC GetFanCoolerStatus(LW2080_CTRL_FAN_COOLER_STATUS_PARAMS *pFanCoolerStatusParams);
    RC GetFanCoolerTable(LW2080_CTRL_FAN_COOLER_INFO_PARAMS *,
                         LW2080_CTRL_FAN_COOLER_STATUS_PARAMS *,
                         LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS *);
    RC SetFanCoolerTable(LW2080_CTRL_FAN_COOLER_CONTROL_PARAMS &);

    struct FanTestParams
    {
        UINT32 coolerIdx;
        UINT32 measTolPct;
        UINT32 colwTimeMs;
    };

    RC GetFanTestParams(vector<FanTestParams> *testParams);

    enum FanCtrlSignal
    {
        FAN_SIGNAL_NONE = LW2080_CTRL_THERMAL_COOLER_SIGNAL_NONE,
        FAN_SIGNAL_TOGGLE = LW2080_CTRL_THERMAL_COOLER_SIGNAL_TOGGLE,
        FAN_SIGNAL_VARIABLE = LW2080_CTRL_THERMAL_COOLER_SIGNAL_VARIABLE
    };
    //! Get the fan control signal type: (none, toggle, or variable)
    RC GetFanControlSignal(UINT32 *pSignalType);

    //! Set the minimum fan speed.
    RC SetMinimumFanSpeed(UINT32 pct);
    //! Set the maximum fan speed.
    RC SetMaximumFanSpeed(UINT32 pct);
    //! Set the fan speed.
    RC SetFanSpeed(UINT32 pct);

    //! Get the number of coolers in the system
    RC GetCoolerCount(UINT32 *pCoolerCount);

    //! Get the RPM of fan
    RC GetFanTachRPM(UINT32 * rpm);
    //! Returns the support for aclwrately callwlating the expected RPMs
    RC GetFanTachExpectedRPMSupported(bool *pbSupported);
    //! Tachometer mininum fan RPM specified in VBIOS - that fan should reach at cooler's min fan speed level
    RC GetFanTachMinRPM(UINT32 *pMinRPM);
    //! Tachometer minimum fan level specified in VBIOS - should correspond to min fan RPM
    RC GetFanTachMinPct(UINT32 *pMinPct);
    //! Tachometer maximum fan RPM specified in VBIOS - that fan should reach at cooler's max fan speed pct
    RC GetFanTachMaxRPM(UINT32 *pMaxRPM);
    //! Tachometer maximum fan level specified in VBIOS - should correspond to max fan RPM
    RC GetFanTachMaxPct(UINT32 *pMaxPct);
    //! Expected error for endpoint fan speed pct -> RPM mappings
    RC GetFanTachEndpointError(JSObject *pEndpointError);
    //! Expected error for interpolating between endpoint fan speed pct -> RPM mappings
    RC GetFanTachInterpolationError(UINT32 *pInterpolationError);

    // Fan 3.0 controls
    RC GetFanArbiterMask(UINT32 *pArbiterMask);
    RC GetFanArbiterInfo(LW2080_CTRL_FAN_ARBITER_INFO_PARAMS *pFanArbiterInfoParams);
    RC GetFanArbiterStatus(LW2080_CTRL_FAN_ARBITER_STATUS_PARAMS *pFanArbiterStatusParams);

    static const char *FanCoolerActiveControlUnitToString(UINT32 controlUnit);
    static const char *FanArbiterCtrlActionToString(UINT32 fanCtrlAction);
    static const char *FanArbiterModeToString(UINT32 mode);

    RC GetPhaseLwrrent(UINT32 PhaseIdx, INT32 *PhaseMiliAmps);

    RC InitOverTempErrCounter();
    RC ShutdownOverTempErrCounter();
    RC SetOverTempRange(OverTempCounter::TempType OverTempType,
                        INT32 Min, INT32 Max);
    void EnableOverTempRangeMonitoring(bool bEnable);

    RC SetADT7461Config(UINT32 Config,   UINT32 LocLimit,
                        UINT32 ExtLimit, UINT32 Hysteresis);

    void BeginExpectingErrors(OverTempCounter::TempType idx);
    void EndExpectingErrors(OverTempCounter::TempType idx);
    void EndExpectingAllErrors();
    RC ReadExpectedErrorCount(UINT64 * pCount);

    static RC GetSocGpuTemp(FLOAT32 *pTemp);

    RC GetIsTempControllerPresent(bool *pPresent);
    RC GetIsAcousticTempControllerPresent(bool *pPresent);

    enum ThermalCapIdx
    {
        // 0 to 15 mean literal policy-table indexes.
        // LW2080_CTRL_THERMAL_POLICY_MAX_POLICIES is 16.

        // Symbolic values below, will be colwerted to actual
        // indexes at runtime:
        GPS = 100,        // ??? something laptop/mobile related
        Acoustic = 101,   // desktop
        MEMORY = 102,     // Limits memory temperature
        SW_SLOWDOWN = 103 // GPU SW Slowdown policy
    };
    RC GetCapRange(ThermalCapIdx i, float *pMinDegC, float * pRatedDegC, float * pMaxDegC);
    RC GetCap(ThermalCapIdx i, float *pLimitDegC);
    RC SetCap(ThermalCapIdx i, float newLimitDegC);
    RC PrintCapPolicyTable(Tee::Priority pri);

    RC GetThermalLimit(UINT32 eventId, INT32 *temperature);
    RC SetThermalLimit(UINT32 eventId, INT32 temperature);

    // Enum for HWFS event categories
    enum class HwfsEventCategory
    {
        EXT_GPIO,
        TEMP_BASED,
        BA,
        EDPP,
        UNKNOWN
    };

    static const char *ThermalCapPolicyIndexToStr(UINT32 policyIndex);
    static const char *HwfsEventTypeStr(UINT32 HwfsEventType);
    static const char *HwfsEventCategoryToStr(HwfsEventCategory eventCat);

    ThermalSensors *GetThermalSensors() { return m_SbdevSensors; };

    RC GetChipTempsViaChannels(vector<ThermalSensors::ChannelTempData> *pTemp);
    RC GetChipTempViaChannel(UINT32 chIdx, ThermalSensors::ChannelTempData *pTemp);
    RC GetOverTempRange(OverTempCounter::TempType OverTempType,
                        INT32 * pMin, INT32 * pMax);
    void PrintHwfsEventData();

    RC GetHwfsEventIgnoreMask(UINT32 *mask);
    RC SetHwfsEventIgnoreMask(UINT32 mask);
    RC GetChipTempsPerBJT(map<string, ThermalSensors::TsenseBJTTempMap> *pBjtTemps);
    RC GetDramTemps(ThermalSensors::DramTempMap *pTemp);
    RC SetChipTempViaChannel(UINT32 chIdx, FLOAT32 temp);

private:

    enum
    {
        UNINTIALIZED_TEMP = 0xFFFF,

        ADT7461_SMBUS_ADDR           = 0x4C,
        ADT7461_LOCAL_TEMP_REG       = 0x00,
        ADT7461_REMOTE_TEMP_REG      = 0x01,
        ADT7461_CONFIG_RD_REG        = 0x03,
        ADT7461_COLW_RATE_RD_REG     = 0x04,
        ADT7461_CONFIG_WR_REG        = 0x09,
        ADT7461_LOC_LIMIT_RD_REG     = 0x05,
        ADT7461_LOC_LIMIT_WR_REG     = 0x0B,
        ADT7461_EXT_LIMIT_RD_REG     = 0x07,
        ADT7461_EXT_LIMIT_WR_REG     = 0x0D,
        ADT7461_EXT_OFFSET_REG       = 0x11,
        ADT7461_EXT_THRM_LIMIT_REG   = 0x19,
        ADT7461_LOC_THRM_LIMIT_REG   = 0x20,
        ADT7461_HYSTERESIS_REG       = 0x21,

        ADT7461_CONFIG_TEMP_OFFSET_MASK = 0x04,

        ADT7473_SMBUS_ADDR       = 0x2E,
        ADT7473_REMOTE_TEMP1_REG = 0x25,
        ADT7473_LOCAL_TEMP_REG   = 0x26,
        ADT7473_REMOTE_TEMP2_REG = 0x27,

        HS22_ADT7473_TEMP_MEAS_OFFSET = -128,

        TEMP_MEAS_OFFSET         = -64
    };

    RC IntGetVbiosTempOffset(INT32 *pOffset, bool IsExternal);

    struct CoolerInstrs
    {
        vector<LW2080_CTRL_THERMAL_COOLER_INSTRUCTION> vec;
        CoolerInstrs(UINT32 num) : vec(num) { memset(&vec[0], 0, sizeof(vec[0]) * num);};
    };
    RC ExelwteCoolerInstruction
    (
        vector<LW2080_CTRL_THERMAL_COOLER_INSTRUCTION> * pInstruction
    );

    struct SysInstrs
    {
        vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> vec;
        SysInstrs(UINT32 num) : vec(num) { memset(&vec[0], 0, sizeof(vec[0]) * num);};
    };
    RC ExelwteSystemInstructions
    (
        vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> * pInstructions
    );
    RC ExelwteSystemInstructions
    (
        vector<LW2080_CTRL_THERMAL_SYSTEM_INSTRUCTION> * pInstructions,
        bool *pbExelwteFailed
    );

    RC EnablePrimarionWrites(bool enable);

    RC InitLastVolterraTemp(UINT32 SlaveId);
    RC DoGetVolterraTemp(UINT32 SensorIndex, UINT32 *pTemp);
    bool IsVolterraTempValid(UINT32 SlaveId, UINT32 Temp);

    RC InitializeADT7461();
    RC PrintADT7461Config();

    RC SaveInitialThermalLimit(UINT32 eventId);
    RC RestoreInitialThermalLimit(UINT32 eventId);

    static RC CreateSensors
    (
        GpuSubdevice *pSbdev,
        ThermalSensors  **pSensors,
        GpuBJTSensors   **pSensorsBjt,
        DramTempSensors **pSensorsDramTemp
    );

    RC GetTempViaSensor(ThermalSensors::Sensor sensor, FLOAT32 *pTemp);

    RC PrepPolicyInfoParam(LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS* pParam);
    RC GetPolicyControlParam(LW2080_CTRL_THERMAL_POLICY_CONTROL_PARAMS* pParam);
    RC ColwertPolicyIndex(ThermalCapIdx* pIdx // In/Out
                                ,const LW2080_CTRL_THERMAL_POLICY_INFO_PARAMS& infoParams);

    // MODS needs to reconfigure the ADT7461, so save off the registers that it
    // will be changing
    struct ADT7461Regs
    {
        bool   bSet;
        UINT08 Config;
        UINT08 LocHighLimit;
        UINT08 ExtHighLimit;
        UINT08 ExtOffset;
        UINT08 ExtThermLimit;
        UINT08 LocThermLimit;
        UINT08 Hysteresis;
    };

    GpuSubdevice    *m_pSubdevice;
    ThermalSensors  *m_SbdevSensors;
    GpuBJTSensors   *m_pSbdevSensorsBjt;
    DramTempSensors *m_pSbdevSensorsDramTemp;
    UINT32          m_OverTempCount;
    UINT32          m_ExpectedOverTempCount;
    bool            m_Hooked;
    INT32           m_IntOffset;
    INT32           m_ExtOffset;
    bool            m_IntOffsetSet;
    bool            m_ExtOffsetSet;
    bool            m_PX3584Detected;
    UINT32          m_PrimarionAddress;
    bool            m_WritesToPrimarionEnabled;
    bool            m_ForceInternal;
    INT32           m_SmbusOffset;
    INT32           m_SmbusSensor;
    INT32           m_SmbAddr;
    INT32           m_MeasurementOffset;
    bool            m_SmbusOffsetSet;
    bool            m_SmbusInit;
    string          m_SmbDevName;
    ADT7461Regs     m_ADT7461OrigRegs;
    ADT7461Regs     m_ADT7461ModsRegs;
    bool            m_SmbConfigChanged;
    SmbPort        *m_pSmbPort;
    SmbManager     *m_pSmbMgr;

    INT32           m_IpmiOffset;
    INT32           m_IpmiSensor;
    INT32           m_IpmiMeasurementOffset;
    bool            m_IpmiOffsetSet;
    string          m_IpmiDevName;
    IpmiDevice      m_IpmiDevice;

    struct VolterraTempReading
    {
        UINT32 Temp;
        UINT64 TimeMs;
    };
    bool                m_VolterraInit;
    map<UINT32, UINT32> m_VolterraSensors;
    map<UINT32, VolterraTempReading> m_LastVolterraReading;
    UINT32              m_VolterraMaxTempDelta;
    OverTempCounter     m_OverTempCounter;

    map<UINT32, LwTemp> m_InitialThermalLimits;

    map<UINT32, UINT32> m_HwfsEventCount;
    UINT32              m_HwfsIgnoreMask = 0;
};

#endif // INCLUDED_THERMUTL_H
