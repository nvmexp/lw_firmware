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

// Channel wrapper interface.

#include "core/include/channel.h"
#include "policymn.h"
#include "pmevent.h"
#include "pmevntmn.h"
#include "pmchwrap.h"
#include "pmchan.h"
#include "gpu/utility/semawrap.h"
#include "core/include/utility.h"
#include "core/include/tasker.h"
#include "mdiag/sysspec.h"
#include <algorithm>

#include "class/cl0000.h"  // LW01_NULL_OBJECT

// Value that can be used for non-methods that still need to be counted
// for the policy manager method triggers.  This value is chosen so that
// all values between NO_METHOD and (NO_METHOD+maxCount) are all too
// large to be valid methods
static UINT32 NO_METHOD = 0xf0000000;

PmChannelWrapper::PmChannelWrapper
(
    Channel       *pChannel
) :
    ChannelWrapper(pChannel),
    m_pPmChannel(NULL),
    m_bCSWarningPrinted(false),
    m_TotalMethodsWritten(0),
    m_NextOnWrite(0),
    m_bProcessOnWrite(true),
    m_bRelwrsive(false),
    m_bPendingMethodId(false),
    m_PendingMethodIdClass(0),
    m_PendingMethodIdMethod(0),
    m_NestedInsertedMethods(0),
    m_StalledThreadId(Tasker::NULL_THREAD),
    m_bBlockFlush(false),
    m_bChannelNeedsFlushing(false),
    m_Mutex(NULL)
{
    MASSERT(IsSupported(pChannel));
    m_Mutex = Tasker::AllocMutex("PmChannelWrapper::m_Mutex",
                                 Tasker::mtxUnchecked);
}

bool PmChannelWrapper::IsSupported(Channel *pChannel)
{
    return SemaphoreChannelWrapper::IsSupported(pChannel);
}

//--------------------------------------------------------------------
//!\brief Sets how many methods must be exelwted prior to calling
//! the next OnWrite.  Since this could be called from many different
//! triggers, make sure that the minimum number is set
//!
//! \param nextOnWrite is the number of methods that must be written
//!                    prior to calling OnWrite again
//!
void PmChannelWrapper::SetNextOnWrite(UINT32 nextOnWrite)
{
    // A zero value means that a particular trigger that is activated OnWrite
    // for this PmChannelListener will never fire again.  Do not update to zero
    if ((nextOnWrite != 0) && (nextOnWrite < m_NextOnWrite))
        m_NextOnWrite = nextOnWrite;
}

//--------------------------------------------------------------------
//!\brief Sets that the channel is lwrrently stalled and remembers
//! which thread stalled the channel.  The operations that the stalling
//! thread may perform on a channel may be limited since if the stalling
//! thread causes the channel to poll waiting for methods, a deadlock
//! condition is guaranteed
//!
void PmChannelWrapper::SetStalled()
{
    // The channel should only ever be stalled from one thread
    MASSERT((m_StalledThreadId == Tasker::NULL_THREAD) ||
            (m_StalledThreadId == Tasker::LwrrentThread()));
    m_StalledThreadId = Tasker::LwrrentThread();
}

//--------------------------------------------------------------------
//!\brief Sets the flag to enable/disable channel flush. This does not
//! prevent channel writes (but there's a likelihood that the push
//! buffer fifo becomes full)
//
RC PmChannelWrapper::BlockFlush(bool Blocked)
{
    RC rc;
    m_bBlockFlush = Blocked;
    if (!Blocked && m_bChannelNeedsFlushing)
    {
        CHECK_RC(Flush());
        m_bChannelNeedsFlushing = false;
    }
    return rc;
}

//--------------------------------------------------------------------
//!\brief Sets that the channel is no longer stalled
//!
void PmChannelWrapper::ClearStalled()
{
    if (Tasker::LwrrentThread() == m_StalledThreadId)
    {
        m_StalledThreadId = Tasker::NULL_THREAD;
    }
    else
    {
        Printf(Tee::PriDebug,
               "Attempting to unstall a channel from outside the stalling"
               "thread is invalid.  Channel is still stalled.\n");
    }
}

//--------------------------------------------------------------------
//! \brief Return true if this channel's test is running
//!
//! This colwenience method is basically the same as calling
//! PolicyManager::InTest() on the test returned by
//! m_pPmChannel->GetTest(), with the necessary NULL-checking.
//!
//! Note: If this function returns true, the caller can assume that
//! m_pPmChannel is non-NULL.
//!
bool PmChannelWrapper::InTest() const
{
    return ( (m_pPmChannel != NULL) &&
             PolicyManager::HasInstance() && 
             PolicyManager::Instance()->InTest(m_pPmChannel->GetTest()) );
}

//--------------------------------------------------------------------
//! \brief This method should be called at the end of the test
//!
//! This method is called by PmChannel::EndTest at the end of the
//! test.  It clears the data (especially setting m_pPmChannel to
//! NULL) so that this object does not try to generate any more
//! events.
//!
RC PmChannelWrapper::EndTest()
{
    m_pPmChannel = NULL;
    m_bProcessOnWrite = false;
    m_bPendingMethodId = false;

    return OK;
}

/* virtual */ RC PmChannelWrapper::Write
(
    UINT32 Subchannel,
    UINT32 Method,
    UINT32 Data
)
{
    RC rc;
    UINT32 writeCount;

    CHECK_RC(ValidateOperation(1));
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    CHECK_RC(PreWrite(INCREMENTING, Subchannel, Method, 1, &writeCount));
    MASSERT(writeCount == 1);
    CHECK_RC(m_pWrappedChannel->Write(Subchannel, Method, Data));
    return rc;
}
/* virtual */ RC PmChannelWrapper::Write
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    RC rc;
    UINT32 writeCount;

    CHECK_RC(ValidateOperation(Count));
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    while (Count)
    {
        CHECK_RC(PreWrite(INCREMENTING, Subchannel, Method, Count,
                          &writeCount));
        CHECK_RC(m_pWrappedChannel->Write(Subchannel, Method,
                                          writeCount, pData));
        Count -= writeCount;
        Method += writeCount * 4;
        pData += writeCount;
    }

    return OK;
}
/* virtual */ RC PmChannelWrapper::WriteNonInc
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    RC rc;
    UINT32 writeCount;

    CHECK_RC(ValidateOperation(Count));
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    while (Count)
    {
        CHECK_RC(PreWrite(NON_INC, Subchannel, Method, Count, &writeCount));
        CHECK_RC(m_pWrappedChannel->WriteNonInc(Subchannel,
                                                Method,
                                                writeCount,
                                                pData));
        Count -= writeCount;
        pData += writeCount;
    };

    return OK;
}
/* virtual */ RC PmChannelWrapper::WriteIncOnce
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count,
    const UINT32 *  pData
)
{
    RC rc;
    UINT32 writeCount;

    CHECK_RC(ValidateOperation(Count));
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    CHECK_RC(PreWrite(INC_ONCE, Subchannel, Method, Count, &writeCount));
    CHECK_RC(m_pWrappedChannel->WriteIncOnce(Subchannel, Method,
                                             writeCount, pData));
    Count -= writeCount;

    if (Count)
        CHECK_RC(WriteNonInc(Subchannel, Method+4, Count, pData+writeCount));

    return OK;
}
/* virtual */ RC PmChannelWrapper::WriteImmediate
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Data
)
{
    RC rc;
    UINT32 writeCount;

    CHECK_RC(ValidateOperation(1));
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    CHECK_RC(PreWrite(INCREMENTING, Subchannel, Method, 1, &writeCount));
    MASSERT(writeCount == 1);
    CHECK_RC(m_pWrappedChannel->WriteImmediate(Subchannel, Method, Data));
    return OK;
}
/* virtual */ RC PmChannelWrapper::BeginInsertedMethods()
{
    RC rc;
    CHECK_RC(m_pWrappedChannel->BeginInsertedMethods());
    ++m_NestedInsertedMethods;
    return rc;
}
/* virtual */ RC PmChannelWrapper::EndInsertedMethods()
{
    MASSERT(m_NestedInsertedMethods > 0);
    --m_NestedInsertedMethods;
    return m_pWrappedChannel->EndInsertedMethods();
}
/* virtual */ void PmChannelWrapper::CancelInsertedMethods()
{
    MASSERT(m_NestedInsertedMethods > 0);
    --m_NestedInsertedMethods;
    m_pWrappedChannel->CancelInsertedMethods();
}
/* virtual */ RC PmChannelWrapper::WriteHeader
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count
)
{
    RC rc;
    CHECK_RC(ValidateOperation(1));
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    CHECK_RC(m_pWrappedChannel->WriteHeader(Subchannel, Method, Count));
    return rc;
}
/* virtual */ RC PmChannelWrapper::WriteNonIncHeader
(
    UINT32          Subchannel,
    UINT32          Method,
    UINT32          Count
)
{
    RC rc;
    CHECK_RC(ValidateOperation(1));
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    CHECK_RC(m_pWrappedChannel->WriteNonIncHeader(Subchannel, Method, Count));
    return rc;
}

/* virtual */ RC PmChannelWrapper::WriteNop()
{
    RC rc;
    UINT32 writeCount = 0;

    CHECK_RC(ValidateOperation(1));
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    CHECK_RC(PreWrite(NON_INC, 0, NO_METHOD, 1, &writeCount));
    MASSERT(writeCount == 1);
    CHECK_RC(m_pWrappedChannel->WriteNop());
    return rc;
}

/* virtual */ RC PmChannelWrapper::WaitIdle(FLOAT64 TimeoutMs)
{
    RC rc;

    if (IsLwrrentThreadStalling())
    {
        ErrPrintf("Cannot idle a stalled channel from the stalling thread\n");
        return RC::SOFTWARE_ERROR;
    }

    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);

    // First call into FermiGpFifoChannel::WaitIdle will allocate sempahore surface.
    // It will cause a implicit yield in Surface2D::Alloc.
    // When other thread enter into FermiGpFifoChannel::WaitIdle, it will use a incompleted surface.
    // A crash will occur.
    // make sure WaitForIdle on a given channel runs serially
    Tasker::MutexHolder mh(m_Mutex);

    CHECK_RC(m_pWrappedChannel->WaitIdle(TimeoutMs));
    CHECK_RC(OnWaitIdle());

    return rc;
}

/* virtual */ RC PmChannelWrapper::Flush()
{
    if (IsLwrrentThreadStalling())
    {
        ErrPrintf("Cannot flush a stalled channel from the stalling thread\n");
        return RC::SOFTWARE_ERROR;
    }

    if (m_bBlockFlush)
    {
        Printf(Tee::PriNormal, "Channel blocked: flushing is posponed\n");
        m_bChannelNeedsFlushing = true;
        return OK;
    }

    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    RC rc = m_pWrappedChannel->Flush();

    return rc;
}

namespace
{
    const char *s_SubroutineWarning =
        "\n"
        "**** WARNING ************ WARNING ************* WARNING ****\n"
        "* Methods passed via subroutine calls are not parsed.      *\n"
        "* Using subroutines with Policy Manager OnMethodCount or   *\n"
        "* OnMethodExelwte triggers will result in unreliable       *\n"
        "* method counting.  In addition if any of the unparsed     *\n"
        "* methods are channel-semaphre methods, the system is      *\n"
        "* likely to hang if OnMethodExelwte triggers are used.     *\n"
        "************************************************************\n";
}

/* virtual */ RC PmChannelWrapper::CallSubroutine(UINT64 Offset, UINT32 Size)
{
    RC rc = m_pWrappedChannel->CallSubroutine(Offset, Size);

    if ((rc == OK) && !m_bCSWarningPrinted)
    {
        WarnPrintf("%s", s_SubroutineWarning);
        m_bCSWarningPrinted = true;
    }

    return rc;
}

/* virtual */ RC PmChannelWrapper::InsertSubroutine
(
    const UINT32 *pBuffer,
    UINT32 count
)
{
    RC rc = m_pWrappedChannel->InsertSubroutine(pBuffer, count);

    if ((rc == OK) && !m_bCSWarningPrinted)
    {
        WarnPrintf("%s", s_SubroutineWarning);
        m_bCSWarningPrinted = true;
    }

    return rc;
}

/* virtual */ RC PmChannelWrapper::SetPrivEnable(bool Priv)
{
    // SetPrivEnable implicitly writes a GpFifo entry, so it has the
    // same restrictions as Flush().
    //
    if (IsLwrrentThreadStalling())
    {
        ErrPrintf("Cannot set priv on a stalled channel from the stalling thread\n");
        return RC::SOFTWARE_ERROR;
    }
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    RC rc = m_pWrappedChannel->SetPrivEnable(Priv);

    return rc;
}

/* virtual */ RC PmChannelWrapper::WriteSetSubdevice(UINT32 mask)
{
    RC rc;
    UINT32 writeCount = 0;

    CHECK_RC(ValidateOperation(1));
    PolicyManager::PmMutexSaver savedPmMutex(
        PolicyManager::HasInstance() ? PolicyManager::Instance() : nullptr);
    CHECK_RC(PreWrite(NON_INC, 0, NO_METHOD, 1, &writeCount));
    MASSERT(writeCount == 1);
    CHECK_RC(m_pWrappedChannel->WriteSetSubdevice(mask));
    return rc;
}
/* virtual */ RC PmChannelWrapper::SetCachedPut(UINT32 PutPtr)
{
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ RC PmChannelWrapper::SetPut(UINT32 PutPtr)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC PmChannelWrapper::RecoverFromRobustChannelError()
{
    RC rc;

    CHECK_RC(m_pWrappedChannel->RecoverFromRobustChannelError());

    if (InTest() && 
        PolicyManager::HasInstance() && 
        PolicyManager::Instance()->GetEnableRobustChannel())
    {
        PmEvent_RobustChannel event(m_pPmChannel, CheckError());
        Printf(Tee::PriDebug,
               "PolicyManager: Received robust channel error %d: %s\n",
               event.GetRC().Get(), event.GetRC().Message());
        CHECK_RC(PolicyManager::Instance()->GetEventManager()->HandleEvent(&event));
    }
    return rc;
}

/* virtual */ PmChannelWrapper * PmChannelWrapper::GetPmChannelWrapper()
{
    return this;
}

//--------------------------------------------------------------------
//!\brief Return whether the current thread has stalled the channel
//!
//!\return true if the current thread has stalled the channel, false
//! otherwise
//!
bool PmChannelWrapper::IsLwrrentThreadStalling()
{
    return (m_StalledThreadId == Tasker::LwrrentThread());
}

//--------------------------------------------------------------------
//!\brief Validate that the current operation being performed on the
//! channel is valid
//!
//! This verifies that the thread that has stalled the channel (i.e.
//! prevented any further method processing in the channel) does not
//! perform any operation that may cause the channel to poll for
//! methods to be processed as that will always result in a deadlock
//! condition
//!
//! \param Count : The number of methods that are about to be
//!                written if validating a method write operation.
//!                Zero if validating an operation that does not
//!                write methods
//!
//!\return OK if the operation is valid, UNSUPPORTED_FUNCTION otherwise
//!
RC PmChannelWrapper::ValidateOperation(UINT32 Count)
{
    if (IsLwrrentThreadStalling())
    {
        UINT32 GpEntries;
        UINT32 PbSpace;

        if ((m_pWrappedChannel->GetPushbufferSpace(&PbSpace) == OK) &&
            (Count > PbSpace))
        {
            ErrPrintf("Cannot perform channel actions on a stalled channel from the stalling thread (not enough space "
                      "for %d methods)\n", Count);
            return RC::UNSUPPORTED_FUNCTION;
        }
        if ((m_pWrappedChannel->GetGpEntries(&GpEntries) == OK) &&
            (GpEntries == 0))
        {
            ErrPrintf("Cannot perform channel actions on a stalled channel from the stalling thread (No GpFifoEntries)\n");
            return RC::UNSUPPORTED_FUNCTION;
        }
    }
    return OK;
}

//--------------------------------------------------------------------
//!\brief Called to notify policy manager that a certain number of
//!       methods are about to be written, generates an on method
//!       write event if necessary and returns the number of methods
//!       that should actually be written.
//!
//! Returns OK if successful, anything else otherwise
//!
RC PmChannelWrapper::PreWrite
(
    WriteType Type,
    UINT32 Subch,
    UINT32 Method,
    UINT32 Count,
    UINT32 *pWriteCount
)
{
    PmEventManager *pEventManager = 
        PolicyManager::Instance() ? 
            PolicyManager::Instance()->GetEventManager() : nullptr;
    UINT32 ClassId;
    UINT32 LastMethod = 0;
    RC rc = OK;

    *pWriteCount = Count;

    // Do nothing if the test isn't running
    //
    if (!InTest())
        return OK;

    // Do nothing if this is an inserted method, rather than part of
    // the actual test.
    //
    if (m_NestedInsertedMethods > 0)
        return OK;

    // In theory, it should be impossible for PreWrite to be called
    // relwrsively.  All PolicyManager actions are supposed to put
    // BeginInsertedMethods/EndInsertedMethods around the methods they
    // write, which should get caught by the inserted-method check
    // above.
    //
    // But just to be safe, MASSERT if we got here relwrsively.
    //
    MASSERT(!m_bRelwrsive);
    m_bRelwrsive = true;

    // Cleanup
    Utility::CleanupValue<bool> m_bRelwrsiveCleanup(m_bRelwrsive, false);

    // Generate a MethodIdWrite event if the previous write left a
    // pending "afterWrite" event for us.
    //
    if (InTest() && m_bPendingMethodId)
    {
        m_bPendingMethodId = false;
        PmEvent_MethodIdWrite MethodIdWriteEvent(m_pPmChannel,
                                                 m_PendingMethodIdClass,
                                                 m_PendingMethodIdMethod,
                                                 true);
        CHECK_RC(pEventManager->HandleEvent(&MethodIdWriteEvent));
    }

    // Generate an MethodWrite event, if needed.
    //
    if (InTest() && m_bProcessOnWrite)
    {
        // If we have reaced the trigger point
        if (m_NextOnWrite == 0)
        {
            // Set the next time to call this OnWrite callback to
            // the max value
            m_NextOnWrite = (UINT32)~0;

            PmEvent_MethodWrite methodWriteEvent(m_pPmChannel,
                                                 this,
                                                 m_TotalMethodsWritten);
            CHECK_RC(pEventManager->HandleEvent(&methodWriteEvent));

            // If there are no more requests to use this notifier
            // for OnWrite, then the next time to call OnWrite
            // will still be the max count.  If so, disable
            // processing of this OnWrite callback
            if (m_NextOnWrite == (UINT32)~0)
                m_bProcessOnWrite = false;
        }

        if (m_bProcessOnWrite)
            *pWriteCount = min(*pWriteCount, m_NextOnWrite);
    }

    // Generate a MethodIdWrite event before the next write, if needed.
    //
    if (InTest() &&
        pEventManager->NeedMethodIdEvent(this, Subch, Method, false, &ClassId))
    {
        PmEvent_MethodIdWrite MethodIdWriteEvent(m_pPmChannel, ClassId,
                                                 Method, false);
        CHECK_RC(pEventManager->HandleEvent(&MethodIdWriteEvent));
    }

    // Decrease *pWriteCount if we'll send a MethodIdWrite event soon
    //
    for (UINT32 ii = 0; ii < *pWriteCount; ++ii)
    {
        switch (Type)
        {
            case INCREMENTING:
                break;
            case NON_INC:
                break;
            case INC_ONCE:
                break;
            default:
                MASSERT(!"Illegal case in PmChannelWrapper::PreWrite");
        }
        if (ii > 0 &&
            pEventManager->NeedMethodIdEvent(this, Subch, Method, false, NULL))
        {
            *pWriteCount = ii;
            break;
        }
        if (pEventManager->NeedMethodIdEvent(this, Subch, Method, true, NULL))
        {
            *pWriteCount = ii + 1;
            break;
        }
    }

    // Keep a running total of all the methods written
    //
    if (m_bProcessOnWrite && InTest())
    {
        m_TotalMethodsWritten += (*pWriteCount);
        m_NextOnWrite -= (*pWriteCount);
    }

    // If we're supposed to generate a MethodIdWrite event after this
    // write, then record the event as pending so that we can generate
    // it before the next write.
    //
    // We don't generate the event right now because there are
    // potential problems with triggering after the last method in the
    // test: we may insert methods that run after the test starts
    // cleaning up.  So we postpone the event until the next write,
    // and never generate the event at all if this was the last write.
    //
    MASSERT(*pWriteCount > 0);
    switch (Type)
    {
    case INCREMENTING:
        LastMethod = Method + 4 * (*pWriteCount - 1);
        break;
    case NON_INC:
        LastMethod = Method;
        break;
    case INC_ONCE:
        LastMethod = (*pWriteCount > 1) ? (Method + 4) : Method;
        break;
    default:
        MASSERT(!"Illegal case in PmChannelWrapper::PreWrite");
    }

    if (pEventManager->NeedMethodIdEvent(this, Subch, LastMethod, true,
                                         &ClassId))
    {
        m_bPendingMethodId = true;
        m_PendingMethodIdClass = ClassId;
        m_PendingMethodIdMethod = LastMethod;
    }

    return rc;
}

//--------------------------------------------------------------------
//!\brief Called to notify policy manager when a wait for idle oclwrs
//!
//! Returns OK if successful, anything else otherwise
//!
RC PmChannelWrapper::OnWaitIdle()
{
    RC rc;

    if (InTest())
    {
        PmEvent_WaitForIdle waitForIdleEvent(m_pPmChannel);
        if (PolicyManager::HasInstance())
        {
            CHECK_RC(PolicyManager::Instance()->GetEventManager()->HandleEvent(&waitForIdleEvent));
        }
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Constructor - flag the channel as stalled
PmChannelWrapper::ChannelStalledHolder::ChannelStalledHolder
(
    PmChannelWrapper *pWrap
) : m_pWrapper(pWrap)
{
    if (m_pWrapper)
        m_pWrapper->SetStalled();
}

//--------------------------------------------------------------------
//! \brief Destructor - unstall the channel
PmChannelWrapper::ChannelStalledHolder::~ChannelStalledHolder()
{
    if (m_pWrapper)
        m_pWrapper->ClearStalled();
}
