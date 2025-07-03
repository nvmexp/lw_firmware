/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef INCLUDED_DISPATCHER_H
#define INCLUDED_DISPATCHER_H

#include "jsapi.h"
#include <vector>
#include <map>
#include <set>
#include "random.h"
#include "core/include/log.h"
#include "core/include/cirlwlarbuffer.h"
#include "core/include/rc.h"
#include "core/include/fpicker.h"
#include "core/include/tasker.h"
#include "core/include/jscript.h"
#include "core/include/tee.h"
#include "core/include/pci.h"
#include "device/cpu/tests/cpustress/cpustress.h"
#include "gpu/perf/perfprofiler.h"
#include "gpu/perf/perfsub.h"
#include "gpu/perf/pwrsub.h"

class Perf;
class Thermal;
class LwpActor;
class JsonItem;
class Pcie;
class ClockThrottle;
class Volt3x;

class ActionLogs
{
public:
    ActionLogs();
    ~ActionLogs();

    struct LogEntry
    {
        LwpActor  *pActor;
        UINT32     Actiolwal;
        UINT32     ActionIdx;
        UINT64     TimeStamp;
    };

    void InsertEntry(LwpActor *pActor, UINT32 Actiolwal, UINT32 ActionId);

    const LogEntry* SearchErrors(UINT64 TimeOfError);

    // debug only
    void DumpEntries();
    void SetSize(UINT32 Size);

private:
    void*               m_Mutex;
    CirBuffer<LogEntry> m_CirBuffer;
};

// put this in a namespace?
enum ActorTypes
{
    JS_ACTOR,
    PSTATE_ACTOR,
    SIMTEMP_ACTOR,
    PCIECHANGE_ACTOR,
    THERMALLIMIT_ACTOR,
    LWVDD_ACTOR,    // not implemented
    DROOPY_ACTOR,
    CPUPOWER_ACTOR
};

//! Brief: Base class of all LwpActor
//
class LwpActor
{
public:
    LwpActor(ActorTypes Type, Tee::Priority PrintPri);
    virtual ~LwpActor();

    virtual RC Cleanup();
    virtual RC ParseParams(const vector<jsval>& jsPickers,
                           const string& Params);
    virtual RC ExtraParamFromJs(jsval jsv);
    virtual RC CheckParams() = 0;
    virtual void Reset();
    void FastForwardToSquenceNum(UINT32 SeqNum);

    //! Brief: given the time since last change, decrement the time
    // for this actor. If it is time to execute, this will call the RunIndex
    // function
    RC ExelwteIfTime(INT64 TimeUsSinceLastSleep,
                     INT64 TimeUsSinceLastChange,
                     bool *pExelwted);

    //! Brief: return the time before next Action to be exelwted
    INT64 GetTimeUsToNextChange() const { return m_TimeUsRemain; }
    UINT32 GetNumSteps() const { return m_NumSteps; }
    string GetActorName() const { return m_ActorName; }
    bool GetUseMsAsTime() const { return m_UseMsAsTime; }
    UINT32 GetSeed() const { return m_Seed; }

    RC CreateJsonItem(JSContext *pCtx, JSObject *pObj);
    virtual RC SetActorPropertiesByJsObj(JSObject *pObj);
    RC CreateNewActionFromJs(JSObject *pObj, UINT32 ActionId);

    //! Brief: Execute one action
    virtual RC RunIndex(UINT32 Index) = 0;
    virtual RC PrintCoverage(Tee::Priority PrintPri)
    {return RC::UNSUPPORTED_FUNCTION;}
    virtual UINT32 CallwlateCoverage() {return 0;}

    void DumpLastActions();

    ActionLogs* GetLog() { return &m_Log; }

    bool GetEnableJson() const { return m_EnableJson; }

    bool IsThreadSafe() const { return m_IsThreadSafe; }

protected:
    struct Action
    {
        UINT32 Actiolwal;            // const value after init
        UINT32 TimeUsToNext;         // const value after init
    };

    UINT32 GetActiolwalById(UINT32 Id) { return m_Actions[Id].Actiolwal; }
    UINT32 GetPreviousActionIndex() { return m_PrevActionIdx; }
    RC ParseStrParams(string InputStr);
    virtual RC SetParameterByName(const string& Name, const string& Val);
    JsonItem* GetJsonItem() { return m_pJsonItem; }
    virtual void AddJsonInfo() = 0;

    void AddAction(Action action);
    void AdvanceToNextAction();
    void AdvanceToNextAction(bool IsSkipped);
    Tee::Priority  m_PrintPri;    // Print priority for dispatcher/actors
    string         m_ActorName;
    bool           m_IsThreadSafe; // true if we can run this actor in a detached thread
    bool           m_EnableJson;  // enable JSON logging

private:
    static UINT32  s_SequenceNum;  // action sequence number - this increase (gives us a sense of the order of actions)
    bool           m_UseMsAsTime;  // this is how user defines time in Fancy Picker
    UINT32         m_Seed;
    UINT32         m_NumSteps;     // number of actions before restarting
    INT64          m_TimeUsRemain; // time before the next
    UINT32         m_ActionIdx;    // index into m_Actions
    UINT32         m_PrevActionIdx;  // index into previous m_Actions performed
    ActorTypes     m_Type;         // Type of actor: eg. JS_ACTOR, PSTATE_ACTOR, etc.
    bool           m_Verbose;
    FancyPicker    m_ValPicker;   // fancy picker that describes the value of the action
    FancyPicker    m_TimePicker;  // fancy picker that describes how often the actor should kick off an action
    vector<Action> m_Actions;     // vector of actions generated at initialization
    set<UINT32>    m_ActionSet;   // a set of ActionId's. This is used in trace mode to avoid recreate dupe actions
    JsonItem      *m_pJsonItem;
    ActionLogs     m_Log;         // cirlwlar buffer of all the actions that happened
    UINT32         m_CirBufferSize;          // default = 64
};

//-----------------------------------------------------------------------------
// Types of Actors
//
// 1) Generic JS function
class JsActor : public LwpActor
{
public:
    JsActor(JSObject *pObj, Tee::Priority PrintPri);
    virtual ~JsActor();
    RC CheckParams() override;
    RC SetActorPropertiesByJsObj(JSObject *pObj) override;

protected:
    RC SetParameterByName(const string& ParamName, const string& Value) override;
    RC RunIndex(UINT32 Index) override;
    void AddJsonInfo() override;
    RC PrintCoverage(Tee::Priority PrintPri) override
    {return RC::UNSUPPORTED_FUNCTION;}
    UINT32 CallwlateCoverage() override {return 0;}

private:
    string      m_JsFuncName;
    JSObject   *m_pObj;
    JSFunction *m_pJsFunc;
};

// 2) Simple PState Switch
class PStateActor : public LwpActor
{
public:
    PStateActor(Tee::Priority PrintPri);
    virtual ~PStateActor();
    RC Cleanup() override;
    void Reset() override;
    RC CheckParams() override;
    RC SetActorPropertiesByJsObj(JSObject *pObj) override;
    RC ExtraParamFromJs(jsval jsvPerfPoints) override;
    RC PrintCoverage(Tee::Priority PrintPri) override;
    UINT32 CallwlateCoverage() override;
    bool IsDispClkTransitionSkipped(UINT64 PrevDispClkKHz, UINT64 NextDispClkKHz);

protected:
    RC SetParameterByName(const string& ParamName, const string& Value) override;
    RC RunIndex(UINT32 Index) override;
    void AddJsonInfo() override;

private:
    RC SetupActor();

    Perf*  m_pPerf;
    Perf::PerfPoint m_InitialPerfPoint;
    vector<Perf::PerfPoint> m_PerfPoints;
    // List of perf points and their usage (has duplicates)
    vector<bool> m_WasPerfPointUsed;
    // Unique list of GpcClk values for evaluating coverage
    map<UINT64,bool> m_UniqueGpcClkList;
    string m_UserPerfPointTableName;
    string m_PrePStateCallback;
    string m_PostPStateCallback;
    string m_PreCleanupCallback;
    bool   m_PrintRmPerfLimits;
    bool   m_PrintClockChanges;
    bool   m_PrintDispClkTransitions;
    bool   m_SendDramLimitStrict;
    map<pair<UINT32, UINT32>, UINT32> m_PStateSwitchesCount;
    bool   m_PerfSweep;
    UINT32 m_CoveragePct;
    UINT32 m_GpuTestNum;
    struct DispClkTransition
    {
        UINT64 FromClockValMHz;
        UINT64 ToClockValMHz;
    };
    vector<DispClkTransition> m_DispClkSwitchSkipList;
    bool m_OrigCallbackUsage;

    PerfProfiler m_PerfProfiler;
};

class SimTempActor : public LwpActor
{
public:
    SimTempActor(Tee::Priority printPri);
    virtual ~SimTempActor();
    RC Cleanup() override;
    RC CheckParams() override;

protected:
    RC SetParameterByName(const string& paramName, const string& value) override;
    RC RunIndex(UINT32 index) override;
    void AddJsonInfo() override;

private:
    Thermal* m_pThermal;
};

class PcieSpeedChangeActor : public LwpActor
{
public:
    PcieSpeedChangeActor(Tee::Priority printPri);
    virtual ~PcieSpeedChangeActor();
    RC Cleanup() override;
    RC CheckParams() override;

protected:
    RC SetParameterByName(const string& paramName, const string& value) override;
    RC RunIndex(UINT32 index) override;
    void AddJsonInfo() override;

private:
    GpuSubdevice* m_pGpuSubdev;
    Pcie*         m_pGpuPcie;
    Perf*         m_pPerf;
    Pci::PcieLinkSpeed m_InitialSpeed;
    Perf::PerfPoint    m_InitialPerfPoint;
};

class ThermalLimitActor : public LwpActor
{
public:
    ThermalLimitActor(Tee::Priority printPri);
    virtual ~ThermalLimitActor();
    RC Cleanup() override;
    RC ParseParams(const vector<jsval>& jsPickers,
                   const string& Params) override;
    RC CheckParams() override;

    // -1 is special value in ThermalLimit. It will restore the event to its original value.
    const static INT32 s_RestoreLimitValue = -1;

protected:
    RC SetParameterByName(const string& paramName, const string& value) override;
    RC RunIndex(UINT32 index) override;
    void AddJsonInfo() override;

private:
    GpuSubdevice* m_pGpuSubdev;
    Thermal* m_pThermal;

    struct ThermalLimit
    {
        string eventStr;
        UINT32 eventId;
        INT32 value;
    };
    vector<ThermalLimit> m_ThermalLimitList;
    map<UINT32,INT32> m_SavedThermalLimitValues;
    RC SaveThermalLimit(UINT32 eventId);
    RC RestoreThermalLimit(UINT32 eventId);

    static UINT32 TranslateThermalEventString(string str);
    static map<string,UINT32> s_ThermalEventStringMap;
    const static UINT32 s_SlowdownPwmEventId; // 0xFFFFFFFE
    const static UINT32 s_IlwalidEventId = 0xFFFFFFFF;
};

class DroopyActor : public LwpActor
{
public:
    DroopyActor(Tee::Priority pri);
    RC Cleanup() override;
    RC CheckParams() override;

protected:
    RC SetParameterByName(const string& paramName, const string& value) override;
    RC RunIndex(UINT32 index) override;
    void AddJsonInfo() override {}

private:
    Power* m_pPower = nullptr;
    UINT32 m_OrigLimitmA = 0;
};

class CpuPowerActor : public LwpActor
{
public:
    CpuPowerActor(Tee::Priority printPri);
    RC Cleanup() override;
    RC CheckParams() override;

protected:
    RC SetParameterByName(const string& paramName, const string& value) override;
    RC RunIndex(UINT32 index) override;
    void AddJsonInfo() override {}

private:
    volatile static UINT32 s_DevInstOwner; // Only one CpuPowerActor should be running at a time

    struct ThreadInfo
    {
        unique_ptr<CpuStress> test;
        UINT32 idx = _UINT32_MAX;
        Tasker::ThreadID id = Tasker::NULL_THREAD;
        StickyRC threadRC;
        ThreadInfo(UINT32 i) : idx(i)
        {
            test = MakeCpuStress();
        }
    };
    using ThreadList = vector<unique_ptr<ThreadInfo>>;

    struct Action
    {
        FLOAT64                   val;   // The Power value read last iteration
        enum { THREAD, SLOWDOWN } type;  // Whether the thread number or slowdown was changed
        FLOAT64                   delta; // How much the thread number or slowdown was changed by
    };

    RC AddThread();
    RC RemoveThread();
    RC IncreasePower(const FLOAT64 lwrrentmw, const FLOAT64 targetmw);
    RC DecreasePower(const FLOAT64 lwrrentmw, const FLOAT64 targetmw);
    void SleepLoop(UINT64 loopIdx);

    static constexpr FLOAT64 s_MinMargin = 1000.0; // Power values within this margin of each other will
                                                   // be considered "close enough" to be equal due
                                                   // to natural power fluctuations.

    UINT32          m_DevInst = ~0U;         // Device that owns this CpuPowerActor
    ThreadList      m_Threads;               // List of information for lwrrently running threads
    volatile UINT32 m_NumThreads = 0;        // Current number of running threads
    bool            m_TargetPower = true;    // If true then the Action val is the power val to target
    Action          m_LastAction;            // The power value and action taken last iteration
    FLOAT64         m_SlowdownFactor = 0.0;  // Factor to slowdown each thread in order to more closely
                                             // target the desired power value
    UINT32          m_LoopsToSleep = 0;      // How many loops after which to sleep
    UINT32          m_SleepUs = 1000;        // How many microseconds to sleep when requested.
    UINT32          m_NumSleeping = 0;       // The number of threads lwrrently sleeping
};

//-----------------------------------------------------------------------------
//! Brief: core of the Clock bashing tool
// This can potentially go to other
class LwpDispatcher
{
public:
    LwpDispatcher(JSContext *cx, JSObject *obj);
    ~LwpDispatcher();

    RC AddActor
    (
        ActorTypes Type,
        const vector<jsval> &jsPickers,
        const string &Params,
        jsval jsv
   );

    void ClearActors();

    //! debug function
    void DumpLastActions();

    //! creates a new thread and kicks of the actors
    RC Run(UINT32 StartSeq, UINT32 StopSeq);
    //! Stops all actions
    RC Stop();

    //! Brief: the background thread. Protected by m_RunThreadMutex
    RC RunThread();

    // todo: return something other then void?
    void PrintLwlpritAction();

    struct ErrorCausePair
    {
        Log::ErrorEntry             Error;
        const ActionLogs::LogEntry* pCause;
    };

    void GetLwlpritActions(vector<ErrorCausePair> *pLwlprits);

    // for JS interface:
    RC SetStopAtFirstActorError(bool ToStop);
    RC SetCirBufferSize(UINT32 CirBufferSize);

    RC ParseTrace(const JsArray *pJson);

    void SetPrintPriority(Tee::Priority PrintPri) { m_PrintPri = PrintPri; }

    RC PrintCoverage(Tee::Priority PrintPri);
    bool HasAtLeastCoverage(UINT32 Threshold);

    bool HasActors() { return m_Actors.size() > 0; }
    bool IsRunning();

private:
    bool DoesActorExist(const string &ActorName) const;
    RC ParseNewActor(JSObject *pObj);
    RC ParseNewAction(JSObject *pObj);

    struct TraceData
    {
        LwpActor* pActor;
        UINT32    ActionId;
        UINT32    TimeUsBeforeChange;
    };

    bool              m_UseTrace;               // enable trace player
    Tasker::ThreadID  m_ThreadId;
    volatile INT32    m_IsRunning;
    UINT32            m_StartIdx;               // start index of the trace
    UINT32            m_StopIdx;                // stop index of the trace
    void*             m_RunThreadMutex;         // protect run thread -only one per instance
    bool              m_StopAtFirstActorError;  // stop thread when an action errors out
    JSContext        *m_pCtx;
    JSObject         *m_pObj;
    vector<LwpActor*> m_Actors;                 // list of actors
    map<string, size_t> m_ActorNameIdxMap;      // use Actor Name to key off an index for an Actor in m_Actors
    vector<TraceData> m_ActionTrace;            // Trace read in from JSON
    Tee::Priority     m_PrintPri;               // Print priority for dispatcher/actors
    RC                m_RunRc;
};

#endif
