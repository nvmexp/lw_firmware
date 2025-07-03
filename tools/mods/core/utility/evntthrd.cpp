/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/nativmtx.h"
#include "core/include/evntthrd.h"
#include "core/include/platform.h"
#include "core/include/cpu.h"
#include <memory>

map<ModsEvent*, EventThread*> EventThread::s_EventThreads;
Tasker::NativeMutex EventThread::s_EventThreadsMutex;

//--------------------------------------------------------------------
//! \brief EventThread constructor
//!
EventThread::EventThread
(
    UINT32      stackSize,
    const char *name,
    bool        bDetachThread
) :
    m_pEvent(NULL),
    m_StackSize(stackSize),
    m_Name(name),
    m_Handler(NULL),
    m_pHandlerData(NULL),
    m_Count(0),
    m_ThreadId(Tasker::NULL_THREAD),
    m_ExitThreadId(Tasker::NULL_THREAD),
    m_bDetachThread(bDetachThread)
{
}

//--------------------------------------------------------------------
//! \brief EventThread destructor
//!
//! Stop the thread (if running) and free the ModsEvent.
//!
EventThread::~EventThread()
{
    if (Platform::IsForcedTermination())
        return;

    RC rc = SetHandler(NULL);
    MASSERT(rc == OK);
    if (rc != OK)
    {
        Printf(Tee::PriHigh,
               "Error in EventThread::~EventThread: rc = %d\n",
               (INT32)rc);
    }
    if (m_pEvent != NULL)
    {
        Tasker::NativeMutexHolder Lock(s_EventThreadsMutex);
        s_EventThreads.erase(m_pEvent);
        Tasker::FreeEvent(m_pEvent);
    }
}

//--------------------------------------------------------------------
//! Given a ModsEvent, return the EventThread that owns it, or NULL if
//! the ModsEvent is not owned by an EventThread.
//!
EventThread *EventThread::FindEventThread(ModsEvent *pEvent)
{
    Tasker::NativeMutexHolder Lock(s_EventThreadsMutex);
    map<ModsEvent*, EventThread*>::iterator iter = s_EventThreads.find(pEvent);

    if (iter == s_EventThreads.end())
        return NULL;
    else
        return iter->second;
}

//--------------------------------------------------------------------
//! \brief Shut down all EventThreads.
//!
//! Should be called before Tasker::Cleanup().
//!
void EventThread::Cleanup()
{
    Tasker::NativeMutexHolder Lock(s_EventThreadsMutex);
    map<ModsEvent*, EventThread*>::iterator iter;
    for (iter = s_EventThreads.begin(); iter != s_EventThreads.end(); ++iter)
    {
        iter->second->SetHandler(NULL);
        if (iter->second->m_pEvent != NULL)
        {
            Tasker::FreeEvent(iter->second->m_pEvent);
            iter->second->m_pEvent = NULL;
        }
    }
}

//--------------------------------------------------------------------
//! \brief Return the ModsEvent owned by this object.
//!
//! Return the ModsEvent owned by this EventThread object.
//!
//! The ModsEvent is lazily instantiated the first time GetEvent() is
//! called.  This lets us get away with creating an EventThread object
//! before the Tasker is initialized, as long as we don't call
//! GetEvent() until later.  This feature is mostly to support
//! old-style tests in which all the data is global namespace
//! variables (which get constructed before main() starts), but it's
//! also useful if you want an EventThread as a static member.
//!
ModsEvent *EventThread::GetEvent()
{
    if (m_pEvent == NULL)
    {
        Tasker::NativeMutexHolder Lock(s_EventThreadsMutex);
        m_pEvent = Tasker::AllocEvent(m_Name.c_str(), false);
        s_EventThreads[m_pEvent] = this;
    }
    return m_pEvent;
}

//--------------------------------------------------------------------
//! \brief Set the handler function
//!
//! Set the handler function that will be called when the event
//! oclwrs, or NULL to disable the event-handling.  This method starts
//! or stops the event thread as needed.
//!
//! Implementation note: one weird-ism about stopping the event thread
//! is that, after stopping the thread, you should call Tasker::Join()
//! on the thread to clean it up.  Join() blocks until the thread
//! finishes.  The problem is when the event thread wants to kill
//! itself: it can't call Join() on itself.  To get around this, if an
//! event thread calls SetHandler(NULL) on itself, we merely save its
//! id in m_ExitThreadId, and Join() it the next time some other
//! thread calls SetHandler().  (A bit ugly, but I'd rather isolate
//! the ugliness here.)
//!
RC EventThread::SetHandler(HandlerFunc handler, void *pData)
{
    RC rc;

    // Before setting the handler, check to see if a previous thread
    // needs to be joined.
    //
    const auto exitThreadId = Cpu::AtomicRead(&m_ExitThreadId);
    if (exitThreadId != Tasker::NULL_THREAD &&
        exitThreadId != Tasker::LwrrentThread())
    {
        CHECK_RC(Tasker::Join(exitThreadId));
        Cpu::AtomicWrite(&m_ExitThreadId, Tasker::NULL_THREAD);
    }

    if ((m_ThreadId == Tasker::NULL_THREAD) && (handler != NULL))
    {
        m_Handler = handler;
        m_pHandlerData = pData;

        // Create thread if we just added the handler, and let the
        // thread run at least once if there happens to be a pending
        // event.
        //
        m_ThreadId = Tasker::CreateThread(ThreadFunction, this,
                                          m_StackSize, m_Name.c_str());
        Tasker::Yield();
    }
    else if ((m_ThreadId != Tasker::NULL_THREAD) && (handler == NULL))
    {
        // Kill thread if we just removed the handler
        //
        if (m_ThreadId == Tasker::LwrrentThread())
        {
            // Thread is killing itself.  Do not Join().
            //
            // m_ExitThreadId should be NULL_THREAD in this branch,
            // since m_ExitThreadId and m_ThreadId can't be same
            // thread, and we cleaned up any non-current
            // m_ExitThreadId above.
            //
            MASSERT(Cpu::AtomicRead(&m_ExitThreadId) == Tasker::NULL_THREAD);
            Cpu::AtomicWrite(&m_ExitThreadId, m_ThreadId);
            m_ThreadId = Tasker::NULL_THREAD;
            Tasker::SetEvent(GetEvent());
        }
        else
        {
            // Killing some other thread.  Do the Join().
            //
            // m_ExitThreadId is either NULL_THREAD or current thread
            // in this branch.  Preserve it after the Join() in case
            // it's the current thread.
            //
            const auto savedExitThread = Cpu::AtomicRead(&m_ExitThreadId);
            const auto killedThread    = m_ThreadId;
            Cpu::AtomicWrite(&m_ExitThreadId, killedThread);
            m_ThreadId = Tasker::NULL_THREAD;
            Tasker::SetEvent(GetEvent());
            rc = Tasker::Join(killedThread);
            Cpu::AtomicWrite(&m_ExitThreadId, savedExitThread);
        }

        m_Handler = handler;
        m_pHandlerData = pData;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Set the handler function
//!
//! Set the handler function that will be called when the event
//! oclwrs, or NULL to disable the event-handling.
//!
//! This method is equivalent to SetHandler(HandlerFunc, void*),
//! except that that this variant lets the caller pass a no-argument
//! handler.
//!
RC EventThread::SetHandler(void (*handler)())
{
    return SetHandler((HandlerFunc)handler, NULL);
}

//--------------------------------------------------------------------
//! \brief Set the ModsEvent and increment a counter
//!
//! This method calls Tasker::SetEvent(), and also increments a
//! counter so that the handler will be called once for each time
//! IncrementEventCount() was called.  If you just call
//! Tasker::SetEvent(), then the handler will only be called once.
//!
void EventThread::SetCountingEvent()
{
    Cpu::AtomicAdd(&m_Count, 1);
    Tasker::SetEvent(GetEvent());
}

//--------------------------------------------------------------------
//! \brief The thread that calls the handler when the event oclwrs
//!
void EventThread::ThreadFunction(void *pEventThread)
{
    EventThread *pThis = (EventThread*)pEventThread;
    Tasker::ThreadID myThreadId = Tasker::LwrrentThread();

    pThis->GetEvent();  // Make sure m_pEvent is instantiated

    unique_ptr<Tasker::DetachThread> threadDetacher;
    if (pThis->m_bDetachThread)
        threadDetacher.reset(new Tasker::DetachThread);
    while (Cpu::AtomicRead(&pThis->m_ExitThreadId) != myThreadId)
    {
        Tasker::WaitOnEvent(pThis->m_pEvent);

        if (pThis->m_Handler == NULL)
        {
            continue;
        }

        if (Cpu::AtomicRead(&pThis->m_Count) == 0)
        {
            if (Cpu::AtomicRead(&pThis->m_ExitThreadId) != myThreadId)
            {
                pThis->m_Handler(pThis->m_pHandlerData);
            }
        }
        else
        {
            INT32 prevCount;
            do
            {
                prevCount = Cpu::AtomicAdd(&pThis->m_Count, -1);
                pThis->m_Handler(pThis->m_pHandlerData);
            } while (prevCount > 1 && Cpu::AtomicRead(&pThis->m_ExitThreadId) != myThreadId);
        }
    }
}
