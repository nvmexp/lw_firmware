/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_PWMUTIL_H
#define INCLUDED_PWMUTIL_H

#include "core/include/rc.h"
#include "core/include/setget.h"
#include "gpu/perf/perfsub.h"
#include <atomic>

class Power;
class Thermal;
class ClockThrottle;
class Avfs;

enum ActorType
{
    FREQ_ACTOR,
    SLOWDOWN_ACTOR
};

static constexpr FLOAT32 DEFAULT_DUTY_CYCLE = 50.0f;

//-----------------------------------------------------------------------------

class PwmActor
{
public:
    PwmActor() = default;
    virtual ~PwmActor() = default;

    RC Start();
    void RunThread();
    virtual void SamplerThread();
    RC JoinThreads(UINT64 timeoutMs);
    virtual RC Stop();

    virtual RC Setup() = 0;
    virtual RC Cleanup() = 0;
    virtual RC ParseParams(JSObject *pParamsObj);
    virtual void PrintParams();

    void SetIsRunning(bool value);
    bool GetIsRunning() const;
    GpuSubdevice* GetGpuSubdev();

    virtual void PrintStats() const;

    GET_PROP(TotalTargetTimeUs, UINT64);
    GET_PROP(TotalLowTimeUs, UINT64);
    SETGET_PROP(ForceMinDutyCycle, bool);

protected:
    virtual bool RequiresSamplerThread() const;

    virtual RC ExelwtePwm() = 0;

    Tee::Priority           m_PrintPri = Tee::PriLow;

    FLOAT32                 m_DutyCyclePct = 0.0f;
    UINT32                  m_DutyCyclePeriodUs = 100000;

    // The amount of time to set and hold the target point
    UINT32                  m_TargetActionDurationUs = 0;

    // The amount of time to set and hold the lower point
    UINT32                  m_LowerActionDurationUs = 0;

    UINT32                  m_SamplerPeriodUs = 5000;

    // Whether or not we will automatically adjust the duty cycle % or use a
    // fixed value provided by the user.
    bool                    m_AdjustDutyCyle = false;
    StickyRC                m_SamplerRc = OK;

    // The initial duty cycle value when using the PID controller
    FLOAT32                 m_DutyHint = DEFAULT_DUTY_CYCLE;
    Tee::Priority           m_StatsPrintPri = Tee::PriLow;
    UINT64                  m_NumDutyCycles = 0;
    FLOAT32                 m_DutyCycleFailThreshold = 5.0f;
    UINT64                  m_TotalTargetTimeUs = 0;
    UINT64                  m_TotalLowTimeUs = 0;
    UINT32                  m_DutyControllerSkipCount = 2;
    FLOAT32                 m_AvgDutyCycle = 0.0f;
    UINT32                  m_CyclesUntilFail = 5;
    string                  m_ActorName = "";

private:
    GpuSubdevice*           m_pGpuSubdev = nullptr;
    Tasker::ThreadID        m_ActorThreadId = Tasker::NULL_THREAD;
    Tasker::ThreadID        m_SamplerThreadId = Tasker::NULL_THREAD;
    volatile INT32          m_IsRunning = 0;
    StickyRC                m_RunRc = OK;
    bool                    m_ForceMinDutyCycle = false;
};

//-----------------------------------------------------------------------------

// Measures something from the GPU that can be represented by a single FLOAT32
// such as power, temperature, etc. This base class provides some thread safety
// around reading the samples.
class GpuSampler
{
public:
    GpuSampler();
    GpuSampler(const GpuSampler&) = delete;
    GpuSampler& operator=(const GpuSampler&) = delete;
    GpuSampler(GpuSampler&&);
    GpuSampler& operator=(GpuSampler&&);
    virtual ~GpuSampler();

    // NOTE: MeasureSample() is assumed to be thread-safe across multiple
    // GpuSamplers. This is true if RM is taking the samples due to their
    // RMCTRL locking model.
    virtual RC MeasureSample() = 0;

    struct Sample
    {
        FLOAT32 value = 0;
        UINT64 duration_us = 0;
        Sample() = default;
        explicit Sample(FLOAT32 val) : value(val), duration_us(0) {}
        Sample(FLOAT32 val, UINT64 dur_us) : value(val), duration_us(dur_us) {}
    };

    // Returns a copy of the latest measured samples (thread-safe)
    vector<Sample> AcquireSamples();

    virtual string GetName() const = 0;
    virtual const char* GetUnits() const = 0;
    SETGET_PROP(SkipCount, UINT32);
    SETGET_PROP(ExpectedAvg, FLOAT32);

    virtual void PrintStats(Tee::Priority pri);
    virtual void ResetStats();
    // Adds a sample to m_Samples (thread-safe)
    void AddSample(FLOAT32 sample);
private:
    void* m_Mutex = nullptr;
    vector<Sample> m_Samples = {};
    FLOAT32 m_ExpectedAvg = 0.0f;
    UINT32 m_SkipCount = 1ULL;

    struct Stats
    {
        FLOAT32 min;
        FLOAT32 max;
        FLOAT32 avg;
        UINT64 numSamples;

        Stats() { Reset(); }
        void Reset()
        {
            min = (std::numeric_limits<FLOAT32>::max)();
            max = (std::numeric_limits<FLOAT32>::lowest)();
            avg = 0.0f;
            numSamples = 0;
        }
    };
    Stats m_Stats = {};
    UINT64 m_LastSampleTickCount = 0;
};

class LwrrentSampler final : public GpuSampler
{
public:
    virtual RC MeasureSample() final { return RC::OK; }
    virtual string GetName() const final;
    virtual const char* GetUnits() const final;
};

// Logs the Total GPU Power & Total GPU Current
// The total GPU power & total GPU current come from the 
// RM channel LW2080_CTRL_PMGR_PWR_CHANNEL_POWER_RAIL_INPUT_TOTAL_BOARD.
class PowerSampler final : public GpuSampler
{
public:
    explicit PowerSampler(Power* pPower);
    PowerSampler(const PowerSampler&) = delete;
    PowerSampler& operator=(const PowerSampler&) = delete;
    PowerSampler(PowerSampler&&);
    PowerSampler& operator=(PowerSampler&&);

    RC MeasureSample() override final;
    string GetName() const override final;
    const char* GetUnits() const override final;
    void PrintStats(Tee::Priority pri) override final;
    void ResetStats() override final;

private:
    Power* m_pPower = nullptr;
    unique_ptr<GpuSampler> m_pLwrSampler = nullptr;
};

// Logs the Temperature corresponding to the channel index
class ThermalSampler final : public GpuSampler
{
public:
    ThermalSampler(Thermal* pTherm, UINT32 chIdx);
    ThermalSampler(const ThermalSampler&) = delete;
    ThermalSampler& operator=(const ThermalSampler&) = delete;
    ThermalSampler(ThermalSampler&&);
    ThermalSampler& operator=(ThermalSampler&&);

    virtual RC MeasureSample() final;
    virtual string GetName() const final;
    virtual const char* GetUnits() const final;

private:
    Thermal* m_pTherm = nullptr;
    UINT32 m_ChIdx = 0;
};

// Uses values read by one or more GpuSamplers to predict the optimal
// duty cycle for the next period of PwmFreqActor.
class DutyController
{
public:
    explicit DutyController(Tee::Priority pri);
    DutyController() = default;
    virtual ~DutyController() = default;
    DutyController(DutyController&&) = default;
    DutyController& operator=(DutyController&&) = default;
    DutyController(DutyController&) = delete;
    DutyController& operator=(DutyController&) = delete;

    // Must return a duty cycle percentage between 1.0 and 99.0
    virtual FLOAT32 CallwlateDutyCycle() = 0;

    // Transfers ownership of a GpuSampler to this DutyController
    void AddSampler(GpuSampler* pGpuSampler);

    // Returns a reference to all the lwrrently owned GpuSamplers
    vector<unique_ptr<GpuSampler>>& GetSamplers();

    // Resets the controller to its init state
    virtual void Reset();

    void PrintSamplerStats(Tee::Priority pri) const;

    GET_PROP(DutyCyclePct, FLOAT32);
    SETGET_PROP(DutyHint, FLOAT32);

protected:
    vector<unique_ptr<GpuSampler>> m_Samplers;
    UINT64 m_PeriodCycles = 0;

    Tee::Priority m_PrintPri = Tee::PriSecret;

    FLOAT32 m_DutyCyclePct = DEFAULT_DUTY_CYCLE;
    FLOAT32 m_DutyHint = DEFAULT_DUTY_CYCLE;
};

// Gain knobs of a PID controller
struct PIDGains
{
    PIDGains(FLOAT32 propGain, FLOAT32 intGain, FLOAT32 dvGain);
    PIDGains() = default;
    FLOAT32 PropGain = 0.0f;
    FLOAT32 IntGain = 0.0f;
    FLOAT32 DvGain = 0.0f;
};

class PIDDutyController : public DutyController
{
public:
    PIDDutyController(PIDGains gains, Tee::Priority pri);
    PIDDutyController() = default;
    virtual FLOAT32 CallwlateDutyCycle() final;

    void Reset() override;

    SETGET_PROP(MaxDutyPct, FLOAT32);

protected:
    // Returns an error value which is used by the PID controller
    // to regulate the duty cycle
    virtual FLOAT32 CallwlateError() = 0;
    virtual const char* GetUnits() const = 0;

private:
    FLOAT32 m_LastError = 0.0f;
    FLOAT32 m_IntegralError = 0.0f;
    PIDGains m_PIDGains = {};
    FLOAT32 m_MaxDutyPct = 99.0f;
};

class PowerTempController final : public PIDDutyController
{
public:
    PowerTempController(PIDGains gains, Tee::Priority pri);
    PowerTempController() = default;

    void Reset() override;
    void PrintParams();

    SETGET_PROP(ThermalSkipCount, UINT32);
    SETGET_PROP(ThermalMarginDegC, FLOAT32);
    SETGET_PROP(MaxThermalCompensationmW, FLOAT32);
    GET_PROP(ThermalErrDegC, FLOAT32);

private:
    virtual FLOAT32 CallwlateError() final;
    virtual const char* GetUnits() const final;

    // How many cycles to skip between evaluation of the thermal portion of
    // the controller. Temperature changes less quickly.
    UINT32 m_ThermalSkipCount = 10;

    FLOAT32 m_ThermalErrDegC = 0.0f;

    FLOAT32 m_ThermalMarginDegC = 5.0f;
    FLOAT32 m_ThermCompensationmW = 0.0f;
    FLOAT32 m_MaxThermalCompensationmW = 50.0f;
};

//! \brief Switches between two perfpoints rapidly
//
// It switches between the two points at either a fixed duty cycle or via a
// dynamically controlled duty cycle.
class PwmFreqActor final : public PwmActor
{
public:
    PwmFreqActor() = default;
    void SamplerThread() override;
    virtual ~PwmFreqActor() = default;
    void PrintStats() const override;
    bool IsOverTempLimit() const;
    RC Stop() override;

private:
    RC Setup() override;
    RC Cleanup() override;
    RC ParseParams(JSObject *pParamsObj) override;
    void PrintParams() override;
    RC ParseControllerParams(JSObject *pParamsObj);
    RC PerformTargetAction();
    RC PerformLowerAction();
    void CallwlateDutyCycle();
    bool RequiresSamplerThread() const override;

    enum class DutyCycleLimiter : UINT08
    {
        POWER,
        THERMAL
    };

    //! \brief Returns which aspect of GPU performance is limiting the duty cycle
    DutyCycleLimiter GetDutyCycleLimiter() const;
    RC ExelwtePwm() final;

    Perf*                   m_pPerf = nullptr;
    Perf::PerfPoint         m_TargetPerfPoint = {};
    Perf::PerfPoint         m_LowerPerfPoint = {};
    Perf::PerfPoint         m_OriPerfPoint = {};

    Power*                  m_pPower = nullptr;
    bool                    m_HasClkUpScale = false;
    LwUFXP4_12              m_OriClkUpScale = 0;

    UINT32                  m_PowerTargetmW = 0;
    UINT32                  m_OrigPowerTargetmW = 0;
    bool                    m_DisablePowerCapping = true;
    Perf::PStateLockType    m_OrigPerfLock = Perf::NotLocked;
    PowerTempController     m_PowerTempController = {};

    UINT32                  m_TargetActionTimeTakenUs = 0;
    UINT32                  m_LowerActionTimeTakenUs = 0;
    bool                    m_IgnorePowerErrors = false;
    bool                    m_IgnoreThermalErrors = false;

    PIDGains                m_Gains = { 0.0000270f, 0.0000325f, -0.0000175f };

    Avfs*                   m_pAvfs = nullptr;
    vector<Gpu::ClkDomain>  m_ClfcClkDoms = {};
    set<Gpu::SplitVoltageDomain> m_ClvcVoltDoms = {};
};

// PwmSlowdownActor uses the HW slowdown PWM registers (same as test 117)
// to create very fast transitions from the target gpcclk frequency (e.g. 1GHz)
// to some divided value (e.g 1/8) the original frequency. Its goal is to PWM
// fast enough to defeat certain board protection mechanisms like on-die-droopy.
// PwmSlowdownActor monitors RM for EDPp events, which are basically short,
// high-power events (power spikes). When PwmSlowdownActor detects EDPp events,
// it increases the ClockThrottle frequency in hopes that the power spikes will
// be too fast for HW to detect.
class PwmSlowdownActor final : public PwmActor
{
public:
    PwmSlowdownActor() = default;
    virtual ~PwmSlowdownActor() = default;
    void SamplerThread() override;
    enum class GpioLogicLevel : UINT32
    {
        ACTIVE_LOW,
        ACTIVE_HIGH
    };

private:
    RC ParseParams(JSObject *pParamsObj) final;
    void PrintParams() override;
    RC Setup() final;
    RC Cleanup() final;
    RC ExelwtePwm() final;
    RC DetectPowerSpikes(bool* pSpikes);
    bool RequiresSamplerThread() const override;

    // User-configurable args
    FLOAT32                 m_FreqHz = 20000.0f;
    UINT32                  m_SlowFactor = 8;
    UINT32                  m_SlewRate = 8;
    FLOAT32                 m_FreqAdjustmentPct = 12.5f;
    UINT32                  m_ControllerPeriodMs = 50;

    static constexpr UINT32 ILWALID_GPIO = ~0U;
    UINT32                  m_OclGpio = ILWALID_GPIO;
    GpioLogicLevel          m_GpioLogicLevel = GpioLogicLevel::ACTIVE_LOW;
    atomic<UINT32>          m_OclEvents { 0U };

    // Internal state
    ClockThrottle*          m_pClockThrottle = nullptr;

    UINT32                  m_EdppMonitorMask = 0;
    map<UINT32, LwU64>      m_EdppCounters = {};
};

//-----------------------------------------------------------------------------

class PwmDispatcher
{
public:
    RC AddActor(ActorType Type, JSObject *pParamsObj);
    RC Start();
    RC Stop();
    UINT64 GetTotalTargetTimeMs() const;
    UINT64 GetTotalLowTimeMs() const;
    bool IsRunning() const;
    void ForceMinDutyCycle();

private:
    bool HasActors() const;

    enum class TimeQuery : UINT08
    {
        TARGET,
        LOW
    };
    UINT64 GetTotalTimeMs(TimeQuery timeQueryType) const;

    vector<unique_ptr<PwmActor>>    m_Actors;
};

#endif
