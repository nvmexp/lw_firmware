/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/*
 * @file    power.h
 * @brief   Provides access to power monitoring
 *
 */

#pragma once
#ifndef INCLUDED_PWRSUB_H
#define INCLUDED_PWRSUB_H

#include "core/include/types.h"
#include "core/include/rc.h"
#include "core/include/callback.h"
#include "core/include/tasker.h"
#include "gpu/utility/gpuerrcount.h"
#include "ctrl/ctrl2080.h"
#include "ctrl/ctrl2080/ctrl2080pmgr.h"
#include <vector>
#include <map>
#include <queue>

class LwstPowerRailLeakageChecker : public GpuErrCounter
{
public:
    LwstPowerRailLeakageChecker(GpuSubdevice * pGpuSubdevice)
    : GpuErrCounter(pGpuSubdevice),
      m_Min(0),
      m_Max(0),
      m_Count(0)
    {
    }
    RC InitializeCounter(UINT32 Min, UINT32 Max);
    UINT32 GetMin(){return m_Min;}
    UINT32 GetMax(){return m_Max;}

protected:
    virtual RC ReadErrorCount(UINT64 * pCount);
    virtual RC OnError(const ErrorData *pData);
private:
    UINT32        m_Min;
    UINT32        m_Max;
    UINT32        m_Count;
};

// if there are more of these, consider the OverTemp design used in thermsub.h
class PowerRangeChecker : public GpuErrCounter
{
public:
    struct PowerRange
    {
        UINT32 SensorMask;
        UINT32 MinMw;
        UINT32 MaxMw;
    };
    PowerRangeChecker(GpuSubdevice * pGpuSubdevice)
    : GpuErrCounter(pGpuSubdevice),
      m_Active(false)
    {
    }

    RC InitializeCounter(const vector<PowerRange> & RangePerMask);
    RC Update(UINT32 SensorMask, UINT32 PowerMw);

protected:
    virtual RC OnError(const ErrorData *pData);
    virtual RC ReadErrorCount(UINT64 *pCount);
private:
    bool                m_Active;
    map<UINT32, UINT32> m_SensorMaskToIdx;
    vector<UINT32>      m_MinMw;
    vector<UINT32>      m_MaxMw;
    vector<UINT32>      m_Count;
};

class Power
{
public:
    explicit Power(GpuSubdevice* pSubdevice);
    ~Power();
    Power(const Power&) = delete;
    Power& operator=(const Power&) = delete;

    struct PowerSensorReading
    {
        UINT32 PowerMw;
        UINT32 VoltageMv;
        UINT32 LwrrentMa;
    };

    struct PowerSensorI2cCfg
    {
        UINT08 I2cIndex;
        UINT16 Configuration;
    };

    using PwrSensorCfg = vector<PowerSensorI2cCfg>;

    enum PowerSensorIdx
    {
        VOLTAGE_IDX = 0,
        LWRRENT_IDX,
        POWER_IDX,
    };

    enum PowerCallback
    {
        SET_POWER_RAIL_VOLTAGE,
        CHECK_POWER_RAIL_LEAKAGE,
        NUM_POWER_CALLBACKS
    };
    RC SetUseCallbacks(PowerCallback CallbackType, string JSFuncName);

    RC SetPowerSupplyType(UINT32 PowerSupply);
    RC SetLwstPowerRailVoltage(UINT32 Voltage);
    RC CheckLwstomPowerRailLeakage();
    RC RegisterLwstPowerRailLeakageCheck(UINT32 Min, UINT32 Max);

    //! A sensor is a physical measurement device
    UINT32 GetPowerSensorMask();
    //! A channel is a sensor or an estimation summary of other channels
    UINT32 GetPowerChannelMask();
    UINT32 GetNumPowerSensors();
    UINT32 GetPowerSensorType(UINT32 sensorMask);
    UINT32 GetPowerRail(UINT32 channelMask);

    static const char * PowerRailToStr(UINT32 Rail);
    static const char * PowerSensorTypeToStr(UINT32 DevType);
    static const char * PowerChannelTypeToStr(UINT32 ChannelType);
    static const char * PowerCapPolicyIndexToStr(UINT32 policyIndex);
    static const char * PowerPolicyTypeToStr(UINT32 policyType);
    RC GetPowerSensorReading(UINT32 SensorMask, PowerSensorReading *pReadings);
    UINT32 GetWeightedPolicyRelationshipMask();
    UINT32 GetBalanceablePolicyRelationshipMask();

    // Sample all power channels and total gpu power.
    // If possible, query recent measurements already held in RM/PMU
    // rather than doing new measurements (faster).
    RC GetPowerSensorSample(LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS *pSample);
    RC GetTegraPowerSensorSample(LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS* pSample);
    RC GetMaxPowerSensorReading(UINT32 SensorMask, UINT32 *pPowerMw);
    void ResetMaxPowerSensorReading(UINT32 SensorMask);

    RC InitPowerRangeChecker(const vector<PowerRangeChecker::PowerRange> &Ranges);
    RC InitRunningAvg(UINT32 sensorMask, UINT32 runningAvgSize);

    RC PrintChannelsInfo(UINT32 channelMask) const;
    RC PrintPoliciesInfo(UINT32 policyMask);
    RC PrintPoliciesStatus(UINT32 policyMask);
    RC PrintViolationsInfo(UINT32 policyMask);
    RC PrintViolationsStatus(UINT32 policyMask);

    enum PowerCapIdx
    {
        // 0 to LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES - 1
        // mean literal policy-table indexes.
        // See ctrl2080pmgr.h for where this constant is defined

        // Symbolic values below, will be colwerted to actual
        // indexes at runtime:
        RTP = 100, // Room Temperature Power (soft limit for boost)
        TGP = 101, // Total Gpu Power (hard limit for hardware safety)
        DROOPY = 102 // HW threshold policy that caps current
    };

    RC GetLwrrPowerCapLimFreqkHzToJs(JSObject *pJsObj);
    RC GetPowerCapRange(PowerCapIdx i, UINT32 *pMinMw, UINT32 * pRatedMw, UINT32 * pMaxMw);
    RC GetPowerCap(PowerCapIdx i, UINT32 *pLimitMw);
    RC SetPowerCap(PowerCapIdx i, UINT32 newLimitMw);
    RC PrintPowerCapPolicyTable(Tee::Priority pri);

    //! \brief Determines if there are any power policy table entries marked as balancing
    bool IsPowerBalancingSupported();

    //! \brief Get indices into power policy relationship table which support balancing
    //! See LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS::policyRelMask in ctrl2080pmgr.h
    RC GetBalanceablePowerPolicyRelations(vector<UINT32> *policyRelIndices);

    //! \brief Get current PWM% for given power policy relationship index
    //! See LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS::policyRels for details
    RC GetPowerBalancePwm(UINT32 policyRelationshipIndex, FLOAT64 *pct);

    //! \brief Set specified power balancing relationship PWM%
    //! Uses LW2080_CTRL_CMD_PMGR_PWR_POLICY_SET_CONTROL to take control over
    //! and set the specified power balancing relationship to the specified PWM
    RC SetPowerBalancePwm(UINT32 policyRelationshipIndex, FLOAT64 pct);

    //! \brief Return control over power policy relationship PWM% back to RM
    RC ResetPowerBalance(UINT32 policyRelationshipIndex);

    //! Given a power policy relationsip, get the two power channels that are
    //! balanced using that relationship, and the phaseEstimate if available.
    //! phaseEstimateChannel will be set to
    //! LW2080_CTRL_PMGR_PWR_CHANNEL_INDEX_ILWALID if this is the case
    RC GetPowerChannelsFromPolicyRelationshipIndex(UINT32 policyRelationshipIndex,
                                                   UINT32 *primaryChannel,
                                                   UINT32 *secondaryChannel,
                                                   UINT32 *phaseEstimateChannel);

    RC GetPowerEquationInfoToJs(JSContext *pCtx, JSObject *pJsObj);

    RC GetPowerCapPollingPeriod(UINT16* periodms);

    //! \brief Controls the clkUpScale factor for the workload policies.
    //! This controls how quickly RM power-capping raises gpcclk in response to
    //! workload decreases. Setting clkUpScale to 1 means that RM will immediately
    //! raise gpcclk if workload decreases. clkUpScale of 1/2 means that RM will
    //! only increase gpcclk 1/2 way to what it thinks is the optimal value.
    //! gpcclk will asymptotically approach the optimal target frequency if
    //! clkUpScale < 1. RM does this conservative behavior to prevent raising
    //! gpcclk too quickly if the workload is bursty.
    RC GetClkUpScaleFactor(LwUFXP4_12* pClkUpScale);
    RC SetClkUpScaleFactor(LwUFXP4_12 clkUpScale);

    //! \brief Returns whether or not at least one RM power policy supports clkUpScale.
    //! Assumes that only LW2080_CTRL_PMGR_PWR_POLICY_TYPE_WORKLOAD_MULTIRAIL supports
    //! clkUpScale.
    RC IsClkUpScaleSupported(bool* pSupported);

    //! \brief Returns a mask corresponding to the first channel matching "rail".
    //! Returns 0 if no match is found.
    UINT32 FindChannelMask(UINT32 rail);

    //! \brief Returns a channel index corresponding to the "rail"
    //! Returns -1 if no match is found.
    INT32 FindChannelIdx(UINT32 rail);

    //! \brief Given a Power Sensor type, this will fetch I2C configuration for
    //! that device type if present in Power Sensor Table
    RC GetPowerSensorCfgVals
    (
        const UINT08 pwrSensorType,
        PwrSensorCfg *pPwrSensorCfg
    );

    bool HasAdcPwrSensor();
    bool IsAdcPwrDevice(UINT32 devType);

    RC GetPowerScalingTgp1XEstimates
    (
        FLOAT32 temp,
        UINT32 gpc2clkMhz,
        UINT32 workLoad
    );

    // Prepare Power Policy Static status structure
    RC PrepPmgrPowerPolicyInfoParam(LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS* pInfoParam);

    // Prepare Power Policy Dynamic Status structure
    RC PrepPmgrPowerPolicyStatusParam(UINT32 policyMask,
                                    LW2080_CTRL_PMGR_PWR_POLICY_STATUS_PARAMS* pStatusParam);

    RC GetPolicyLimitUnitStr(LwU8 limitUnit, string* unitSuffix);

private:
    Tasker::Mutex  m_Mutex = Tasker::CreateMutex("Power", Tasker::mtxUnchecked);
    GpuSubdevice  *m_pSubdevice;
    LwstPowerRailLeakageChecker m_LwstPowerLeakageChecker;
    PowerRangeChecker m_PowerRangeChecker;

    bool           m_PowerSensorInfoRetrieved;
    bool           m_PowerMonitorIsEnabled;
    UINT32         m_PowerChannelMask;
    UINT32         m_PowerSensorMask;
    UINT32         m_PowerMonitorSampleCount;
    UINT32         m_PowerMonitorSamplePeriodMs;
    UINT32         m_TotalPowerMask;
    UINT32         m_CachedDroopyPolicyIdx = LW2080_CTRL_PMGR_PWR_POLICY_MAX_POLICIES;

    map<UINT32, UINT32> m_RunningAvgSizes;
    map<UINT32, UINT64> m_RunningSums;
    map<UINT32, queue<UINT32> > m_SamplesToAvg;

    //! Cached values from LW2080_CTRL_PMGR_PWR_POLICY_GET_INFO
    LW2080_CTRL_PMGR_PWR_POLICY_INFO_PARAMS m_CachedPowerPolicyInfo;
    //! Cached values from LW2080_CTRL_CMD_PMGR_PWR_MONITOR_GET_STATUS
    LW2080_CTRL_PMGR_PWR_MONITOR_GET_STATUS_PARAMS m_CachedPwrStatus;
    //! Mask of policyRels that have type *_POLICY_RELATIONSHIP_TYPE_WEIGHT
    UINT32         m_WeightedPolicyRelationshipMask;
    //! Mask of policyRels that have type *_POLICY_RELATIONSHIP_TYPE_BALANCE
    UINT32         m_BalanceablePolicyRelationshipMask;

    vector<Callbacks> m_Callbacks;

    //! Describes one power-sensor "device" readable via RM.
    struct PowerSensor
    {
        UINT32 DevType;             //!< LW2080_CTRL_PMGR_PWR_DEVICE_TYPE_XXX
        UINT32 PowerRail;             //!< LW2080_CTRL_PMGR_PWR_CHANNEL_RAIL_XXX
        UINT32 ChannelType;         //!< LW2080_CTRL_PMGR_PWR_CHANNEL_TYPE_XXX
        UINT32 MonitorMask;         //!< LW2080_CTRL_PMGR_PWR_MONITOR
    };
    map<UINT32, PowerSensor> m_PowerSensors;

    //! Per sensor readings of the maximum power
    map<UINT32, UINT32> m_MaxPowerReadings;

    bool m_TegraSensorsStarted = false;

    RC GetPowerSensorInfo();
    RC GetTegraPowerSensorInfo();
    RC ConstructPowerSensors
    (
        const LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS& devParams,
        const LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS& monParams
    );
    void ConstructLegacyPowerSensors
    (
        const LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS& devParams
    );
    RC ConstructPowerPolicyMasks();
    void DumpPowerTables(Tee::Priority pri,
                         const LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS & devParams,
                         const LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS & monParams);

    // Prepare Perf Power Model Static status structure
    RC PrepPerfPowerModelInfoParam
    (
        LW2080_CTRL_PERF_PERF_CF_PWR_MODELS_INFO* pPowerModelInfoParam
    );

    // Inject Perf Power Model Scaling Parameters
    RC SetPerfPowerModelScaleParam
    (
        LW2080_CTRL_PERF_PERF_CF_PWR_MODEL_SCALE_PARAMS* pPowerModelScaleParam
    );

    RC ColwertPowerPolicyIndex(PowerCapIdx * pIdx);

    //! Calls LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_CONTROL with the given parameters
    RC PrepPmgrPowerPolicyControlParam
    (
        LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS * pParam,
        UINT32 powerCapMask,
        UINT32 policyRelMask
    );

    //! Verifies that policyRelationshipIndex'th bit is set in
    //! m_BalanceablePolicyRelationshipMask
    RC CheckForValidPolicyRelationshipIndex(UINT32 policyRelationshipIndex);

    //! Prepares LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS by calling
    //! LW2080_CTRL_CMD_PMGR_PWR_POLICY_GET_CONTROL and sanity checks
    //! policyRelationshipIndex
    RC PrepPowerBalanceCtrlParams
    (
        LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS *pParam,
        UINT32 policyRelationshipIndex
    );

    RC PrepPmgrPowerEquationInfo
    (
        LW2080_CTRL_PMGR_PWR_EQUATION_GET_INFO_PARAMS *pParam
    );

    RC CheckPowerPolicyMask(UINT32* policyMask);

    void GetDroopyInfo
    (
        const LW2080_CTRL_PMGR_PWR_DEVICES_GET_INFO_PARAMS& devParams,
        const LW2080_CTRL_PMGR_PWR_MONITOR_GET_INFO_PARAMS& monParams
    );

    RC GetWorkloadMultiRailPolicies(LW2080_CTRL_PMGR_PWR_POLICY_CONTROL_PARAMS* pParams);

    void DumpTgp1xEstimatedMetrics
    (
        Tee::Priority pri,
        LW2080_CTRL_PMGR_PWR_POLICY_PERF_CF_PWR_MODEL_ESTIMATED_METRICS_WORKLOAD_COMBINED_1X *pwrTgp1XEstimates
    );
};

#endif
