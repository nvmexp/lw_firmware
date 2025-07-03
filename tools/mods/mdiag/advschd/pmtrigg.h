/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2006-2019 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

//! \file
//! \brief Defines the triggers

#ifndef INCLUDED_PMTRIGG_H
#define INCLUDED_PMTRIGG_H

#ifndef INCLUDED_PMEVNTMN_H
#include "pmevntmn.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

#ifndef INCLUDED_PMUTILS_H
#include "pmutils.h"
#endif

#include <boost/smart_ptr.hpp>

//--------------------------------------------------------------------
//! \brief Abstract base class for PolicyManager triggers
//!
//! A trigger fires when a PmEvent oclwrs that matches the trigger.
//! Each trigger subclass matches a different type of event.
//!
//! Note: Each trigger contains a pointer to the EventManager that
//! owns it.  To simplify the constructor a bit, this pointer is set
//! when the parser adds the trigger to the EventManager.
//!
class PmTrigger
{
public:
    PmTrigger();
    virtual ~PmTrigger();
    PmEventManager *GetEventManager() const { return m_pEventManager; }
    PolicyManager *GetPolicyManager() const;

    virtual RC                   IsSupported()                     const = 0;
    virtual bool                 CouldMatch(const PmEvent *pEvent) const = 0;
    virtual bool                 DoMatch(const PmEvent *pEvent)          = 0;
    virtual const PmSurfaceDesc *GetSurfaceDesc()                const;
    virtual const PmPageDesc    *GetPageDesc()                   const;

    bool HasMemRange()      const { return (GetCaps() & HAS_MEM_RANGE)      != 0; }
    bool HasChannel()       const { return (GetCaps() & HAS_CHANNEL)        != 0; }
    bool HasGpuSubdevice()  const { return (GetCaps() & HAS_GPU_SUBDEV)     != 0; }
    bool HasGpuDevice()     const { return (GetCaps() & HAS_GPU_DEVICE)     != 0; }
    bool HasMethodCount()   const { return (GetCaps() & HAS_METHOD_COUNT)   != 0; }
    bool HasReplayFault()   const { return (GetCaps() & HAS_REPLAY_FAULT)   != 0; }
protected:
    enum {
        HAS_MEM_RANGE      = 0x01,
        HAS_CHANNEL        = 0x02,
        HAS_GPU_SUBDEV     = 0x04,
        HAS_GPU_DEVICE     = 0x08,
        HAS_METHOD_COUNT   = 0x10,
        HAS_REPLAY_FAULT   = 0x20,
        HAS_ACCESS_COUNTER = 0x40
    };
    virtual UINT32 GetCaps() const = 0;
    virtual RC StartTest();

private:
    friend RC PmEventManager::AddTrigger(PmTrigger*, PmActionBlock*);
    friend RC PmEventManager::StartTest();
    void SetEventManager(PmEventManager *ptr) { m_pEventManager = ptr; }

    PmEventManager *m_pEventManager; //!< EventManager that owns this trigger.
                                     //!< Set by PmEventManager::SetTrigger().
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when testing callback mods
//!
class PmTrigger_OnChannelReset : public PmTrigger
{
public:
    PmTrigger_OnChannelReset();
    virtual RC     IsSupported()                     const;
    virtual bool   CouldMatch(const PmEvent *pEvent) const;
    virtual bool   DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL; }
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when testing starts
//!
class PmTrigger_Start : public PmTrigger
{
public:
    PmTrigger_Start();
    virtual RC     IsSupported()                     const;
    virtual bool   CouldMatch(const PmEvent *pEvent) const;
    virtual bool   DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return 0; }
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when testing ends
//!
class PmTrigger_End : public PmTrigger
{
public:
    PmTrigger_End();
    virtual RC     IsSupported()                     const;
    virtual bool   CouldMatch(const PmEvent *pEvent) const;
    virtual bool   DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return 0; }
};

//--------------------------------------------------------------------
//! Trigger that fires when a robust-channel error oclwrs on a channel
//!
//! We don't expose this trigger directly to the parser, instead using
//! wrappers like "OnPageFault" or "OnResetChannel", because the
//! mechanism for page-fault reporting and reset-channel reporting has
//! changed in the past, and is likely to change again in the future.
//! They're robust-channel errors at the moment.
//!
class PmTrigger_RobustChannel : public PmTrigger
{
public:
    PmTrigger_RobustChannel(const PmChannelDesc *pChannelDesc, RC rc);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL | HAS_GPU_DEVICE; }

private:
    const PmChannelDesc *m_pChannelDesc; //!< Matches the faulting channel
    RC m_rc;                             //!< The robust-channel error
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when an non-stalling interrupt oclwrs
//!
class PmTrigger_NonStallInt : public PmTrigger
{
public:
    PmTrigger_NonStallInt(const RegExp &intNames,
                          const PmChannelDesc *pChannelDesc);
    virtual RC   IsSupported()                     const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL | HAS_GPU_DEVICE; }

private:
    RegExp m_IntNames; //!< Matches the interrupt name in Action.NonStallInt
    const PmChannelDesc *m_pChannelDesc; //!< Matches the channel
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when a semaphore is released
//!
class PmTrigger_SemaphoreRelease : public PmTrigger
{
public:
    PmTrigger_SemaphoreRelease(const PmSurfaceDesc *pSurfaceDesc,
                               const FancyPicker& offsetPicker,
                               const FancyPicker& payloadPicker,
                               bool   bUseSeedString,
                               UINT32 randomSeed);
    virtual ~PmTrigger_SemaphoreRelease();
    virtual RC   IsSupported()                     const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);
    virtual const PmPageDesc *GetPageDesc()      const;

protected:
    virtual UINT32 GetCaps() const { return HAS_MEM_RANGE; }
    virtual RC StartTest();

private:
    static  RC SemReleaseCallback(void *ppThis,
                         const PmMemRanges &matchingRanges,
                         PmPageDesc* nextPageDesc,
                         vector<UINT08>* nextData);

    UINT64 GetPayloadSize() const; //!< return payload size in bytes

    PmPageDesc                  m_PageDesc; //!< Matching semaphores
    UINT64                      m_Payload;  //!< Matching payload
    FancyPicker                 m_OffsetPicker;
    FancyPicker                 m_PayloadPicker;
    FancyPicker::FpContext      m_FpContext;
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when the entire chip (all channels)
//! are idled
//!
class PmTrigger_OnWaitForChipIdle : public PmTrigger
{
public:
    PmTrigger_OnWaitForChipIdle();
    virtual RC   IsSupported()                     const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return 0; }
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when a wait for idle oclwrs on channels
//! matching the regular expression
//!
class PmTrigger_OnWaitForIdle : public PmTrigger
{
public:
    PmTrigger_OnWaitForIdle(const PmChannelDesc *pChannelDesc);
    virtual RC   IsSupported()                     const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL | HAS_GPU_DEVICE; }

private:
    const PmChannelDesc *m_pChannelDesc;  //!< Matches the channel
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when the method write count on channels
//! matching the regular expression matches the provided fancy picker
//!
class PmTrigger_OnMethodWrite : public PmTrigger
{
public:
    PmTrigger_OnMethodWrite(const PmChannelDesc *pChannelDesc,
                            const FancyPicker&   fancyPicker,
                            bool                 bUseSeedString,
                            UINT32               randomSeed);
    virtual ~PmTrigger_OnMethodWrite();
    virtual RC   IsSupported()                     const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);
protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL |
                                            HAS_GPU_DEVICE |
                                            HAS_METHOD_COUNT; }
private:
    const PmChannelDesc *m_pChannelDesc;  //!< Matches the channel
    FancyPicker  m_FancyPicker;   //!< Pick Counter for the method count
    bool            m_bUseSeedString;//!< true if the seed should come from
                                     //!< a string instead of m_RandomSeed
    UINT32          m_RandomSeed;    //!< Random seed for the pick counters
    typedef map<const PmChannel*, PmPickCounter *> PickCountMap;
    PickCountMap  m_PickCountMap;     //!< Per-channel method count data
};

#define ILWALID_METHOD_PERCENT 101.0
#define MAX_VALID_METHOD_PERCENT 100.0
//--------------------------------------------------------------------
//! \brief Base class for triggers that fire when a percentage of methods
//! has been written or exelwted on channels matching the regular
//! expression
//!
class PmTrigger_OnPercentMethods : public PmTrigger
{
public:
    PmTrigger_OnPercentMethods(const PmChannelDesc *pChannelDesc,
                               const FancyPicker&   fancyPicker,
                               bool                 bUseSeedString,
                               UINT32               randomSeed);
    virtual ~PmTrigger_OnPercentMethods();
    virtual RC   IsSupported()                  const;
protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL |
                                            HAS_GPU_DEVICE |
                                            HAS_METHOD_COUNT; }
    bool              ChannelMatches(const PmEvent *pEvent);
    bool              MethodCountMatches(const PmEvent *pEvent);
    void              UpdateNextMethodCount(const PmEvent *pEvent);
    UINT32            GetNextMethodCount(const PmEvent *pEvent);
    FLOAT32           GetNextMethodPercent(const PmEvent *pEvent);

private:
    UINT32            CalcMethods(const PmChannel *pChannel);

    const PmChannelDesc *m_pChannelDesc;  //!< Matches the channel
    FancyPicker          m_FancyPicker;   //!< Pick Counter for the method
                                          //!< count
    bool            m_bUseSeedString;//!< true if the seed should come from
                                     //!< a string instead of m_RandomSeed
    UINT32          m_RandomSeed;    //!< Random seed for the FancyPicker

    struct PmPercentMethods
    {
        FancyPicker::FpContext  fpContext;
        FLOAT32                 nextTriggerPercent;
    } ;
    typedef map<const PmChannel*, PmPercentMethods> PercentMethodsMap;
    PercentMethodsMap  m_PercentMap;     //!< Per-channel method percentage data
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when the percentage of method written
//! on channels matching the regular expression matches the desired
//! percentages from the fancy picker
//!
class PmTrigger_OnPercentMethodsWritten : public PmTrigger_OnPercentMethods
{
public:
    PmTrigger_OnPercentMethodsWritten(const PmChannelDesc *pChannelDesc,
                                      const FancyPicker&   fancyPicker,
                                      bool                 bUseSeedString,
                                      UINT32               randomSeed);
    virtual ~PmTrigger_OnPercentMethodsWritten() { ; }
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when the method exelwtion count on channels
//! matching the regular expression matches the provided fancy picker
//!
class PmTrigger_OnMethodExelwte : public PmTrigger
{
public:
    PmTrigger_OnMethodExelwte(const PmChannelDesc *pChannelDesc,
                              const FancyPicker&   fancyPicker,
                              bool                 bUseSeedString,
                              UINT32               randomSeed);
    virtual ~PmTrigger_OnMethodExelwte();
    virtual RC   IsSupported()                     const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);
protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL |
                                            HAS_GPU_DEVICE |
                                            HAS_METHOD_COUNT; }
private:
    const PmChannelDesc *m_pChannelDesc;  //!< Matches the channel
    FancyPicker  m_FancyPicker;   //!< Pick Counter for the method count
    bool            m_bUseSeedString;//!< true if the seed should come from
                                     //!< a string instead of m_RandomSeed
    UINT32          m_RandomSeed;    //!< Random seed for the pick counters
    typedef map<const PmChannel*, PmPickCounter *> PickCounterMap;
    PickCounterMap  m_PickCountMap;//!< Per-channel pick counters
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when the percentage of method exelwted
//! on channels matching the regular expression matches the desired
//! percentages from the fancy picker
//!
class PmTrigger_OnPercentMethodsExelwted : public PmTrigger_OnPercentMethods
{
public:
    PmTrigger_OnPercentMethodsExelwted(const PmChannelDesc *pChannelDesc,
                                       const FancyPicker&   fancyPicker,
                                       bool                 bUseSeedString,
                                       UINT32               randomSeed);
    virtual ~PmTrigger_OnPercentMethodsExelwted() { ; }
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when the class/method is written to the
//! channel's pushbuffer.
//!
class PmTrigger_OnMethodIdWrite : public PmTrigger
{
public:
    PmTrigger_OnMethodIdWrite(const PmChannelDesc *pChannelDesc,
                              vector<UINT32> &ClassIds, string classType,
                              vector<UINT32> &Method, bool AfterWrite);
    virtual RC   IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL | HAS_GPU_DEVICE; }
    virtual RC StartTest();

private:
    const PmChannelDesc *m_pChannelDesc;  //!< Channels to trigger on
    set<UINT32> m_ClassIds;  //!< Trigger when we write to one of these classes
    string m_ClassType;      //!< or first supported class of this type
    set<UINT32> m_Methods;   //!< ...with one of these methods
    bool m_AfterWrite;       //!< Trigger before or after the write
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when the specified time has elapsed
//! since Action.StartTimer.
//!
class PmTrigger_OnTimeUs : public PmTrigger
{
public:
    PmTrigger_OnTimeUs(const string &TimerName,
                       const FancyPicker& hwPicker,
                       const FancyPicker& modelPicker,
                       const FancyPicker& rtlPicker,
                       bool   bUseSeedString,
                       UINT32 RandomSeed);
    virtual ~PmTrigger_OnTimeUs() { delete m_pPickCounter; }
    virtual RC   IsSupported() const { return OK; }
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return 0; }

private:
    string         m_TimerName;
    PmPickCounter *m_pPickCounter;
    FancyPicker    m_HwPicker;
    FancyPicker    m_ModelPicker;
    FancyPicker    m_RtlPicker;
    bool           m_bUseSeedString;//!< true if the seed should come from
                                    //!< a string instead of m_RandomSeed
    UINT32         m_RandomSeed;    //!< Random seed for the pick counters

    struct TimerData
    {
        PmTrigger_OnTimeUs *pTrigger;
        UINT64 ElapsedTimeUs;
    };
    static RC TimerHandler(void *pTimerData);
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when a PMU ELPG event oclwrs
//!
class PmTrigger_OnPmuElpgEvent : public PmTrigger
{
public:
    PmTrigger_OnPmuElpgEvent(const PmGpuDesc *pGpuDesc,
                             UINT32           engineId,
                             UINT32           interruptStatus);
    virtual RC   IsSupported()                     const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);
protected:
    virtual UINT32 GetCaps() const { return HAS_GPU_DEVICE |
                                            HAS_GPU_SUBDEV; }
private:
    const PmGpuDesc *m_pGpuDesc;  //!< Matches the gpu
    UINT32 m_EngineId;            //!< EngineId of the ELPG event
    UINT32 m_InterruptStatus;     //!< Interrupt status for the ELPG event
};

//--------------------------------------------------------------------
//! \brief Trigger that fires when a RM Event (Probe) oclwrs
// see resman/arch/lwalloc/mods/inc/rmpolicy.h
class PmTrigger_OnRmEvent : public PmTrigger
{
public:
    PmTrigger_OnRmEvent(const PmGpuDesc *pGpuDesc, UINT32 EventId);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);
protected:
    virtual UINT32 GetCaps() const { return HAS_GPU_DEVICE |
                                            HAS_GPU_SUBDEV; }
private:
    const PmGpuDesc *m_pGpuDesc;
    UINT32           m_EventId;     // RM EVENT ID -> defined in resman/arch/lwalloc/mods/inc/rmpolicy.h
};

//--------------------------------------------------------------------
//! \brief Trigger that fires at trace command EVENT
//!
class PmTrigger_OnTraceEventCpu: public PmTrigger
{
public:
    PmTrigger_OnTraceEventCpu(const string &traceEventName, bool afterTraceEvent);
    virtual RC   IsSupported()                     const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);
protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL; }
private:
    string m_TraceEventName;
    bool m_AfterTraceEvent;
};

//--------------------------------------------------------------------
//! Trigger that fires when a replayable fault oclwrs on a GPU
//!
class PmTrigger_ReplayablePageFault : public PmTrigger
{
public:
    PmTrigger_ReplayablePageFault(const PmGpuDesc *pGpuDesc);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const
    {
        return HAS_MEM_RANGE | HAS_GPU_DEVICE | HAS_REPLAY_FAULT | HAS_CHANNEL;
    }

private:
    const PmGpuDesc *m_pGpuDesc; //!< Matches the faulting GPU
};

//--------------------------------------------------------------------
//! Trigger that fires when a recoverable CE fault oclwrs on a GPU
//!
class PmTrigger_CeRecoverableFault : public PmTrigger
{
public:
    PmTrigger_CeRecoverableFault(const PmGpuDesc *pGpuDesc);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    //! Reuse HAS_REPLAY_FAULT.
    virtual UINT32 GetCaps() const
    {
        return HAS_MEM_RANGE | HAS_GPU_DEVICE | HAS_REPLAY_FAULT | HAS_CHANNEL;
    }

private:
    const PmGpuDesc *m_pGpuDesc; //!< Matches the faulting GPU
};

//--------------------------------------------------------------------
//! Trigger that fires when a non-replayable fatal fault oclwrs on a GPU
//!
class PmTrigger_NonReplayableFault : public PmTrigger
{
public:
    PmTrigger_NonReplayableFault(const PmChannelDesc *pChannelDesc);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL | HAS_GPU_DEVICE; }

private:
    const PmChannelDesc *m_pChannelDesc; //!< Matches the faulting channel
};

//--------------------------------------------------------------------
//! \brief  Trigger that fires when fatal fault oclwrs
//! Composite trigger including both PmTrigger_RobustChannel and
//! PmTrigger_NonReplayableFault if chip is VOLTA+.
//! PmTrigger_RobustChannel is for fatal fault
//! PmTrigger_NonReplayableFault is for fault in register. Those faults
//! are usually caused by CPU engines or fault buffer disabled tests.
class PmTrigger_PageFault : public PmTrigger
{
public:
    PmTrigger_PageFault(const PmChannelDesc *pChannelDesc);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL | HAS_GPU_DEVICE; }
    virtual RC StartTest();

private:
    const PmChannelDesc *m_pChannelDesc; //!< Matches the faulting channel
    vector< shared_ptr<PmTrigger> > m_Triggers;
};

//--------------------------------------------------------------------
//! Trigger that fires when fault buffer overflow oclwrs
//!
class PmTrigger_FaultBufferOverflow : public PmTrigger
{
public:
    PmTrigger_FaultBufferOverflow(const PmGpuDesc *pGpuDesc);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_GPU_DEVICE; }
    virtual RC StartTest();

private:
    const PmGpuDesc *m_pGpuDesc; //!< Matches the fault buffer's GPU
};

//--------------------------------------------------------------------
//! Trigger that fires when an access counter notification oclwrs on a GPU
//!
class PmTrigger_AccessCounterNotification : public PmTrigger
{
public:
    PmTrigger_AccessCounterNotification(const PmGpuDesc *pGpuDesc);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_MEM_RANGE | HAS_GPU_DEVICE | HAS_ACCESS_COUNTER; }

private:
    const PmGpuDesc *m_pGpuDesc; //!< Matches the faulting GPU
};

//--------------------------------------------------------------------
//! Trigger that triggered by trace_3d plugin
//!
class PmTrigger_PluginEventTrigger : public PmTrigger
{
public:
    PmTrigger_PluginEventTrigger(const string &eventName);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return 0; }
private:
    string m_EventName;
};

//--------------------------------------------------------------------
//! Trigger that fires when an interrupt is reported to the error logger
//!
class PmTrigger_OnErrorLoggerInterrupt : public PmTrigger
{
public:
    PmTrigger_OnErrorLoggerInterrupt(const PmGpuDesc *pGpuDesc,
        const std::string &interruptString);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_GPU_DEVICE |
                                            HAS_GPU_SUBDEV; }
    virtual RC StartTest() override;

private:
    const PmGpuDesc *m_pGpuDesc;  //!< Matches the GPU
    const std::string m_InterruptString;  //!< Matches the interrupt
};

//--------------------------------------------------------------------
//! \brief Trigger that fires at a trace3d plugin trace event
//!
class PmTrigger_OnT3dPluginEvent: public PmTrigger
{
public:
    PmTrigger_OnT3dPluginEvent(const string &traceEventName);
    virtual RC   IsSupported()                     const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);
protected:
    virtual UINT32 GetCaps() const { return HAS_GPU_DEVICE |
                                            HAS_GPU_SUBDEV; }
private:
    string m_TraceEventName;
};
//! \brief  Trigger that fires when non fatal poison error oclwrs
//! Composite trigger including both PmTrigger_RobustChannel and
//! PmTrigger_NonReplayableFault if chip is VOLTA+.
//! PmTrigger_RobustChannel is for fatal fault
//! PmTrigger_NonReplayableFault is for fault in register. Those faults
//! are usually caused by CPU engines or fault buffer disabled tests.
class PmTrigger_NonfatalPoisonError : public PmTrigger
{
public:
    PmTrigger_NonfatalPoisonError(const PmChannelDesc *pChannelDesc, string type);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL | HAS_GPU_DEVICE; }
    virtual RC StartTest();

private:
    const PmChannelDesc *m_pChannelDesc; //!< Matches the faulting channel
    string m_poisonType;
    vector< shared_ptr<PmTrigger> > m_Triggers;
};

//--------------------------------------------------------------------
//! \brief  Trigger that fires when fatal poison error oclwrs
//! Composite trigger including both PmTrigger_RobustChannel and
//! PmTrigger_NonReplayableFault if chip is VOLTA+.
//! PmTrigger_RobustChannel is for fatal fault
//! PmTrigger_NonReplayableFault is for fault in register. Those faults
//! are usually caused by CPU engines or fault buffer disabled tests.
class PmTrigger_FatalPoisonError : public PmTrigger
{
public:
    PmTrigger_FatalPoisonError(const PmChannelDesc *pChannelDesc, string type);
    virtual RC IsSupported() const;
    virtual bool CouldMatch(const PmEvent *pEvent) const;
    virtual bool DoMatch(const PmEvent *pEvent);

protected:
    virtual UINT32 GetCaps() const { return HAS_CHANNEL | HAS_GPU_DEVICE; }
    virtual RC StartTest();

private:
    const PmChannelDesc *m_pChannelDesc; //!< Matches the faulting channel
    string m_poisonType;
    vector< shared_ptr<PmTrigger> > m_Triggers;
};

#endif // INCLUDED_PMTRIGG_H
