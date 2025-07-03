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
//! \brief Define a PmEvent hierarchy of events

#ifndef INCLUDED_PMEVENT_H
#define INCLUDED_PMEVENT_H

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

#ifndef INCLUDED_PMSURF_H
#include "pmsurf.h"
#endif

#ifndef INCLUDED_PMU_H
#include "gpu/perf/pmusub.h"
#endif

//--------------------------------------------------------------------
//! \brief Abstract class representing an advanced-scheduling event
//!
//! A PmEvent (such as page fault) usually has a very short life:
//!
//! - Created
//! - Passed to PmEventManager::HandleEvent(), which
//!   - Calls the event's NotifyGilder() method
//!   - Calls Execute() on all actionBlocks triggered by the event
//! - Destroyed
//!
//! A copy of the event is often cloned for the gilder, which then
//! owns the clone (i.e. the gilder is in charge of deleting the
//! clone).  The original event is usually created on the stack, and
//! goes out-of-scope as soon as HandleEvent() returns.
//!
class PmEvent
{
public:
    enum Type
    {
        START,
        ROBUST_CHANNEL,
        NON_STALL_INT,
        SEMAPHORE_RELEASE,
        WAIT_FOR_IDLE,
        METHOD_WRITE,
        METHOD_EXELWTE,
        METHOD_ID_WRITE,
        START_TIMER,
        TIMER,
        GILD_STRING,
        PMU_EVENT,
        RM_EVENT,
        TRACE_EVENT_CPU,
        REPLAYABLE_FAULT,
        FAULT_BUFFER_OVERFLOW,
        ACCESS_COUNTER_NOTIFICATION,
        CHANNEL_RESET,
        PLUGIN,
        PHYSICAL_PAGE_FAULT,
        CE_RECOVERABLE_FAULT,
        NON_REPLAYABLE_FAULT,
        ERROR_LOGGER_INTERRUPT,
        T3D_PLUGIN_EVENT,
        NONE_FATAL_POISON_ERROR,
        FATAL_POISON_ERROR,
        END,
    };

    PmEvent(Type type);
    virtual ~PmEvent();
    Type                  GetType()       const { return m_Type; }
    virtual PmEvent      *Clone()                           const = 0;
    virtual void          NotifyGilder(PmGilder *pGilder)   const;
    virtual string        GetGildString()                   const = 0;
    virtual bool          Matches(const string &gildString) const;
    virtual bool          MatchesRequiredEvent(const string &gildString) const;
    virtual bool          RequiresPotentialEvent()          const;
    virtual PmChannels    GetChannels()                     const;
    virtual const PmMemRange *GetMemRange()                 const;
    virtual PmSubsurfaces GetSubsurfaces()                  const;
    virtual GpuSubdevice *GetGpuSubdevice()                 const;
    virtual GpuDevice    *GetGpuDevice()                    const;
    virtual UINT32        GetMethodCount()                  const;
    virtual PmTest       *GetTest()                         const;
    virtual string        GetTraceEventName()               const;
    virtual PmSmcEngine        *GetSmcEngine()                    const;
    virtual const UINT64        GetTimeStamp()                 const;
    virtual const string        GetTsgName()                      const;
    virtual const UINT32        GetVEID()                         const;
    virtual const UINT32        GetGPCID()                        const;
    virtual const UINT32        GetClientID()                     const;
    virtual const UINT32        GetFaultType()                    const;
    virtual const UINT32        GetAccessType()                   const;
    virtual const UINT32        GetAddressType()                  const;
    virtual const UINT32        GetMMUEngineID()                  const;
    virtual const UINT16        GetInfo16()                       const;
protected:
    static string GetGildStringForMemRange(const PmMemRange *pMemRange);
    RC DoFieldsMatch(const vector<string> &MyGildFields,
                     const vector<string> &TheirGildFields,
                     UINT32 FieldStart,
                     UINT32 FieldCount,
                     bool bAllowRegex,
                     bool *pbMatches) const;
    virtual RC InnerMatches(const string &gildString, bool *pbMatches) const;

private:
    Type m_Type;        //!< The type of event
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when the rm call back to mods
//!
class PmEvent_OnChannelReset : public PmEvent
{
public:
    PmEvent_OnChannelReset(LwRm::Handle hObject, LwRm* pLwRm);
    virtual PmEvent *Clone()                           const;
    virtual string   GetGildString()                   const;
    virtual PmChannels GetChannels()                   const;

private:
    LwRm::Handle m_hChannel;
    LwRm* m_pLwRm;
    PmChannels  m_pChannels; //!< The channels on which the error oclwrred
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when the test starts
//!
class PmEvent_Start : public PmEvent
{
public:
    PmEvent_Start(PmTest * pTest) : PmEvent(START) { m_pTest = pTest; }
    virtual PmEvent *Clone()                           const;
    virtual void     NotifyGilder(PmGilder *pGilder)   const;
    virtual string   GetGildString()                   const;
    virtual PmTest * GetTest()                         const;
private:
    PmTest * m_pTest;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when the test end or abort
//!
class PmEvent_End : public PmEvent
{
public:
    PmEvent_End(PmTest * pTest) : PmEvent(END) { m_pTest = pTest; }
    virtual PmEvent *Clone()                           const;
    virtual void     NotifyGilder(PmGilder *pGilder)   const;
    virtual string   GetGildString()                   const;
    virtual PmTest * GetTest()                         const;
private:
    PmTest * m_pTest;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a robust-channel error oclwrs
//!
//! This event is triggered when the RM writes a robust-channel error
//! code to the channel's error notifier.
//! /sa Channel::CheckError
//! /sa Channel::ClearNotifierErrorStatus
//!
class PmEvent_RobustChannel : public PmEvent
{
public:
    PmEvent_RobustChannel(PmChannel *pChannel, RC rc);
    virtual PmEvent      *Clone()                           const;
    virtual string        GetGildString()                   const;
    virtual PmChannels    GetChannels()                     const;

    RC GetRC() const { return m_RC; }

private:
    PmChannel *m_pChannel; //!< The channel on which the error oclwrred
    RC         m_RC;       //!< The robust-channel error that oclwrred
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a non-stalling int oclwrs
//!
//! This event is detected by a thread that listens for
//! LW906F_NOTIFIERS_NONSTALL or LW2080_NOTIFIERS_GRAPHICS events.
//!
class PmEvent_NonStallInt : public PmEvent
{
public:
    PmEvent_NonStallInt(PmNonStallInt *pNonStallInt);
    virtual PmEvent   *Clone()                           const;
    virtual void       NotifyGilder(PmGilder *pGilder)   const;
    virtual string     GetGildString()                   const;
    virtual bool       RequiresPotentialEvent()          const;
    virtual PmChannels GetChannels()                     const;
    PmNonStallInt     *GetNonStallInt() const { return m_pNonStallInt; }

private:
    PmNonStallInt *m_pNonStallInt;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a semaphore-release oclwrs
//!
//! This event is detected by a thread that checks for changes in
//! surfaces with SemaphoreRelease triggers waiting on them.
//!
class PmEvent_SemaphoreRelease : public PmEvent
{
public:
    PmEvent_SemaphoreRelease(const PmMemRange &MemRange, UINT64 payload);
    virtual PmEvent    *Clone()                           const;
    virtual string      GetGildString()                   const;
    virtual const PmMemRange *GetMemRange()               const;
    UINT64              GetPayload() const { return m_Payload; }

protected:
    virtual RC InnerMatches(const string &gildString, bool *pbMatches) const;

private:
    PmMemRange m_MemRange;
    UINT64 m_Payload;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a wait for idle oclwrs
//! (either full chip or on a particular channel)
class PmEvent_WaitForIdle : public PmEvent
{
public:
    PmEvent_WaitForIdle(PmChannel *pChannel);
    virtual PmEvent   *Clone()                           const;
    virtual void       NotifyGilder(PmGilder *pGilder)   const;
    virtual string     GetGildString()                   const;
    virtual PmChannels GetChannels()                     const;
private:
    PmChannel *m_pChannel; //!< The channel on which the WFI was received
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a method is written
//!
class PmEvent_MethodWrite : public PmEvent
{
public:
    PmEvent_MethodWrite(PmChannel        *pChannel,
                        PmChannelWrapper *pPmChannelWrapper,
                        UINT32            methodNum);
    virtual PmEvent   *Clone()                           const;
    virtual string     GetGildString()                   const;
    virtual PmChannels GetChannels()                     const;
    virtual UINT32     GetMethodCount()                  const;
    PmChannelWrapper  *GetPmChannelWrapper()             const;
private:
    PmChannel        *m_pChannel;          //!< The channel on which the method
                                           //! was written
    PmChannelWrapper *m_pPmChannelWrapper; //!< The channel on which the method
                                           //! was written
    UINT32            m_MethodNum;         //!< The method number where the
                                           //! event oclwrred
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a method is exelwted
//!
class PmEvent_MethodExelwte : public PmEvent
{
public:
    PmEvent_MethodExelwte(PmChannel *pChannel, UINT32 methodNum);
    virtual PmEvent   *Clone()                           const;
    virtual string     GetGildString()                   const;
    virtual PmChannels GetChannels()                     const;
    virtual UINT32     GetMethodCount()                  const;
private:
    PmChannel *m_pChannel; //!< The channel on which the method was exelwted
    UINT32     m_MethodNum; //!< The method number where the event oclwrred
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a class/method is written
//!
class PmEvent_MethodIdWrite : public PmEvent
{
public:
    PmEvent_MethodIdWrite(PmChannel *pChannel, UINT32 ClassId,
                          UINT32 Method, bool AfterWrite);
    virtual PmEvent   *Clone()                           const;
    virtual string     GetGildString()                   const;
    virtual PmChannels GetChannels()                     const;
    UINT32 GetClassId()    const { return m_ClassId; }
    UINT32 GetMethod()     const { return m_Method; }
    bool GetAfterWrite()   const { return m_AfterWrite; }
private:
    PmChannel *m_pChannel;   //!< The channel on which the method was exelwted
    UINT32     m_ClassId;    //!< The class that was written to
    UINT32     m_Method;     //!< The method that was written to the class
    bool       m_AfterWrite; //!< Tells whether event is before/after the write
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a timer is started.
//!
class PmEvent_StartTimer : public PmEvent
{
public:
    PmEvent_StartTimer(const string &TimerName);
    virtual PmEvent *Clone()                           const;
    virtual string   GetGildString()                   const;
    const string    &GetTimerName() const { return m_TimerName; }

private:
    string m_TimerName;   //!< Name of the timer
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a specified time has
//! elapsed since a timer started
//!
class PmEvent_Timer : public PmEvent
{
public:
    PmEvent_Timer(const string &TimerName, UINT64 TimeUS);
    virtual PmEvent *Clone()                           const;
    virtual string   GetGildString()                   const;
    const string    &GetTimerName() const { return m_TimerName; }
    UINT64           GetTimeUS() const { return m_TimeUS; }

private:
    string m_TimerName;   //!< Name of the timer
    UINT64 m_TimeUS;      //!< Time in us since the timer started
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that represents a raw string in the gild file
//!
//! This is used to store hex dumps, crcs, etc in the gild file.
//!
class PmEvent_GildString : public PmEvent
{
public:
    PmEvent_GildString(const string &str);
    virtual PmEvent *Clone()                           const;
    virtual string   GetGildString()                   const;
private:
    string m_String; //!< The string to dump to the gild file
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when the RM sends a PMU interrupt
//!
//! This event is detected by an PMU Interrupt listening thread launched by
//! the eventManager.
//!
class PmEvent_PmuEvent : public PmEvent
{
public:
    typedef PMU::Message PmuEvent;

    PmEvent_PmuEvent(GpuSubdevice *pGpuSubdevice, const PmuEvent &pmuEvent);
    virtual PmEvent      *Clone()                           const;
    virtual string        GetGildString()                   const;
    virtual bool          MatchesRequiredEvent(const string &gildString) const;
    virtual GpuDevice    *GetGpuDevice()                    const;
    virtual GpuSubdevice *GetGpuSubdevice()                 const;
    const   PmuEvent     *GetPmuEvent()                     const;

protected:
    virtual RC InnerMatches(const string &gildString, bool *pbMatches) const;

private:
    virtual RC InnerMatchesRequiredEvent(const string &gildString, bool *pbMatches) const;
    string PmuEventDescription()                            const;

    GpuSubdevice *m_pGpuSubdevice; //!< The subdevice that sent the Pmu event
    PmuEvent      m_PmuEvent;      //!< The Pmu Event
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs on RM Events
//!
class PmEvent_RmEvent : public PmEvent
{
public:
    PmEvent_RmEvent(GpuSubdevice *pSubdev, UINT32 EventId);
    virtual PmEvent * Clone() const;
    virtual string GetGildString() const;
    virtual GpuDevice    *GetGpuDevice()           const;
    virtual GpuSubdevice *GetGpuSubdevice()        const;
    UINT32 GetRmEventId() const;
private:
    GpuSubdevice *m_pSubdev;
    UINT32        m_EventId;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs at trace command EVENT
//!
class PmEvent_TraceEventCpu : public PmEvent
{
public:
    PmEvent_TraceEventCpu(
        const string &traceEventName,
        PmTest *pTest,
        LwRm::Handle chHandle,
        bool afterTraceEvent);
    virtual PmEvent *Clone()                           const;
    virtual string  GetGildString()                    const;
    virtual string  GetTraceEventName()                const;
    virtual PmTest  *GetTest()                         const { return m_pTest; }
    virtual PmChannels GetChannels()                   const;
    bool IsAfterTraceEvent()                           const { return m_AfterTraceEvent; }
private:
    string m_TraceEventName;
    PmTest *m_pTest;
    LwRm::Handle m_ChannelHandle;
    bool m_AfterTraceEvent;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a GMMU fault
//! including replayable/non-replayable oclwrs
//!
class PmEvent_GMMUFault : public PmEvent
{
public:
    PmEvent_GMMUFault(
        Type type,
        GpuSubdevice *pGpuSubdevice,
        const PmMemRange &memRange,
        UINT32 GPCID,
        UINT32 clientID,
        UINT32 faultType,
        UINT64 timestamp,
        const PmChannels &channels);
    virtual GpuSubdevice *GetGpuSubdevice()              const;
    virtual const PmMemRange *GetMemRange()              const;
    virtual const UINT64 GetTimeStamp()                  const;
    virtual const string GetTsgName()                    const;
    virtual const UINT32 GetGPCID()                      const;
    virtual const UINT32 GetClientID()                   const;
    virtual const UINT32 GetFaultType()                  const;
    virtual PmChannels GetChannels()                  const;
protected:
    GpuSubdevice *m_pGpuSubdevice;
    PmMemRange m_MemRange;
    UINT32 m_GPCID;
    UINT32 m_ClientID;
    UINT32 m_FaultType;
    UINT64 m_TimeStamp;
    PmChannels m_channels;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when a replayable fault oclwrs
//!
//! This event is detected by a thread that listens for
//! LWC369_NOTIFIER_MMU_FAULT_REPLAYABLE
//!
class PmEvent_ReplayableFault : public PmEvent_GMMUFault
{
public:
    PmEvent_ReplayableFault(
        GpuSubdevice *pGpuSubdevice,
        PmReplayableInt *pReplayableInt,
        const PmMemRange &memRange,
        UINT32 GPCID,
        UINT32 clientID,
        UINT32 faultType,
        UINT64 timestamp,
        UINT32 veid,
        const PmChannels &channels);
    virtual PmEvent   *Clone()                           const;
    virtual string     GetGildString()                   const;
    virtual const UINT32 GetVEID()                       const;
private:
    PmReplayableInt *m_pReplayableInt;
    UINT32 m_VEID;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs
//!  when a non-replayable recoverable fault oclwrs
//!
//! This event is detected by a thread that listens for
//! LWC369_NOTIFIER_MMU_FAULT_NON_REPLAYABLE and faulting engine is CE
//!
class PmEvent_CeRecoverableFault : public PmEvent_GMMUFault
{
public:
    PmEvent_CeRecoverableFault(
        GpuSubdevice *pGpuSubdevice,
        PmNonReplayableInt *pNonReplayableInt,
        const PmMemRange &memRange,
        UINT32 GPCID,
        UINT32 clientID,
        UINT32 faultType,
        UINT64 timestamp,
        const PmChannels &channels);
    virtual PmEvent   *Clone()                           const;
    virtual string     GetGildString()                   const;
private:
    PmNonReplayableInt *m_pNonReplayableInt;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs
//!  when a non-replayable recoverable fault oclwrs
//!
//! This event is detected by a thread that listens for
//! LWC369_NOTIFIER_MMU_FAULT_NON_REPLAYABLE and faulting engine is others not CE
//!
class PmEvent_NonReplayableFault : public PmEvent_GMMUFault
{
public:
    PmEvent_NonReplayableFault(
        GpuSubdevice *pGpuSubdevice,
        PmNonReplayableInt *pNonReplayableInt,
        const PmMemRange &memRange,
        UINT32 GPCID,
        UINT32 clientID,
        UINT32 faultType,
        UINT64 timestamp,
        const PmChannels &channels);
    virtual PmEvent   *Clone()                           const;
    virtual string     GetGildString()                   const;
private:
    PmNonReplayableInt *m_pNonReplayableInt;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when the fault event buffer
//!        overflows
//!
class PmEvent_FaultBufferOverflow : public PmEvent
{
public:
    PmEvent_FaultBufferOverflow(
        GpuSubdevice *pGpuSubdevice);
    virtual PmEvent   *Clone()                           const;
    virtual GpuSubdevice *GetGpuSubdevice()              const;
    virtual string     GetGildString()                   const;

private:
    GpuSubdevice *m_pGpuSubdevice;
};

//--------------------------------------------------------------------
//! \brief PmEvent subclass that oclwrs when by an access counter notification
//!
//! This event is detected by a thread that listens for
//! LWC365_NOTIFIERS_ACCESS_COUNTER.
//!
class PmEvent_AccessCounterNotification : public PmEvent
{
public:
    PmEvent_AccessCounterNotification(
        GpuSubdevice *pGpuSubdevice,
        PmAccessCounterInt *pAccessCounterInt,
        const PmMemRange &memRange,
        const UINT64 instanceBlockPointer,
        const UINT32 aperture,
        const UINT32 accessType,
        const UINT32 addressType,
        const UINT32 counterValue,
        const UINT32 peerID,
        const UINT32 mmuEngineID,
        const UINT32 bank,
        const UINT32 notifyTag,
        const UINT32 subGranularity,
        const UINT32 instAperture,
        const bool m_IsVirtualAccess,
        const PmChannels &channels);
    virtual PmEvent *Clone()                             const;
    virtual string   GetGildString()                     const;
    virtual GpuSubdevice *GetGpuSubdevice()              const;
    virtual const PmMemRange *GetMemRange()              const;
    virtual const UINT32 GetAperture()                   const;
    virtual const UINT32 GetAccessType()                 const;
    virtual const UINT32 GetAddressType()                const;
    virtual const UINT32 GetPeerID()                     const;
    virtual const UINT32 GetMMUEngineID()                const;
    virtual const UINT32 GetBank()                       const;
    virtual const UINT32 GetNotifyTag()                  const;
    virtual const UINT32 GetSubGranularity()             const;
    virtual bool GetIsVirtualAccess()                    const;
    virtual PmChannels GetChannels()                     const;
private:
    GpuSubdevice *m_pGpuSubdevice;
    PmAccessCounterInt *m_pAccessCounterInt;
    PmMemRange m_MemRange;
    UINT64 m_InstanceBlockPointer;
    UINT32 m_Aperture;
    UINT32 m_AccessType;
    UINT32 m_AddressType;
    UINT32 m_CounterValue;
    UINT32 m_PeerID;
    UINT32 m_MmuEngineID;
    UINT32 m_Bank;
    UINT32 m_NotifyTag;
    UINT32 m_SubGranularity;
    UINT32 m_InstAperture;
    bool m_IsVirtualAccess;
    PmChannels m_channels;
};

//--------------------------------------------------------------------
//! \brief PluginEvent subclass that oclwrs when t3d plugin needs PM actions
//!
class PmEvent_Plugin : public PmEvent
{
public:
    PmEvent_Plugin(const string eventName) : PmEvent(PLUGIN), m_EventName(eventName) {}
    virtual PmEvent *Clone()                           const;
    virtual string  GetGildString()                    const;
    virtual string  GetEventName()                     const;

private:
    string m_EventName;
};

//--------------------------------------------------------------------
//! \brief OnPhyscialPageFault subclass that oclwrs when gpu device has
//! physcial page fault
//!
class PmEvent_OnPhysicalPageFault : public PmEvent
{
public:
    PmEvent_OnPhysicalPageFault(GpuSubdevice * pGpuSubdevice);
    virtual PmEvent *Clone()                           const;
    virtual string  GetGildString()                    const;
    virtual GpuSubdevice * GetGpuSubdevice()           const;

private:
    GpuSubdevice * m_pGpuSubdevice;
};

//--------------------------------------------------------------------
//! \brief Oninterrupt subclass that oclwrs when an interrupt is logged
//!
class PmEvent_OnErrorLoggerInterrupt : public PmEvent
{
public:
    PmEvent_OnErrorLoggerInterrupt(GpuSubdevice * pGpuSubdevice,
        const std::string &interruptString);
    virtual PmEvent *Clone()                           const;
    virtual string  GetGildString()                    const;
    virtual GpuSubdevice * GetGpuSubdevice()           const;
    virtual std::string GetInterruptString()           const;

private:
    GpuSubdevice * m_pGpuSubdevice;
    const std::string m_InterruptString;
};

//--------------------------------------------------------------------
//! \brief OnT3dPluginEvent subclass that oclwrs at a trace3d plugin trace event
//!
class PmEvent_OnT3dPluginEvent : public PmEvent
{
public:
    PmEvent_OnT3dPluginEvent(PmTest * pTest, const string& traceEventName);
    virtual PmEvent *Clone()                           const;
    virtual string  GetGildString()                    const;
    virtual string  GetTraceEventName()                const;
    virtual PmTest * GetTest()                         const;

private:
    PmTest * m_pTest;
    string m_TraceEventName;
};

//! \brief OnNonfatalPoisonError subclass that oclwrs when gpu device has
//! a none fatal poison error
//!
class PmEvent_OnNonfatalPoisonError : public PmEvent
{
public:
    PmEvent_OnNonfatalPoisonError(GpuSubdevice * pGpuSubdevice, UINT16 info16);
    virtual PmEvent *Clone()                           const;
    virtual string  GetGildString()                    const;
    virtual GpuSubdevice * GetGpuSubdevice()           const;
    virtual const UINT16 GetInfo16()                   const;

private:
    GpuSubdevice * m_pGpuSubdevice;

    // Lwrrently, the info16 in notification data returned from
    // LW2080_NOTIFIERS_POISON_ERROR_NON_FATAL represents different poison error
    UINT16 m_info16;
};

//--------------------------------------------------------------------
//! \brief OnFatalPoisonError subclass that oclwrs when gpu device has
//! a fatal poison error
//!
class PmEvent_OnFatalPoisonError : public PmEvent
{
public:
    PmEvent_OnFatalPoisonError(GpuSubdevice * pGpuSubdevice, UINT16 info16);
    virtual PmEvent *Clone()                           const;
    virtual string  GetGildString()                    const;
    virtual GpuSubdevice * GetGpuSubdevice()           const;
    virtual const UINT16 GetInfo16()                   const;

private:
    GpuSubdevice * m_pGpuSubdevice;

    // Lwrrently, the info16 in notification data returned from
    // LW2080_NOTIFIERS_POISON_ERROR_FATAL represents different poison error
    UINT16 m_info16;
};

#endif // INCLUDED_PMEVENT_H
