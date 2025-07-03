/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007,2009,2011,2014-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_EVNTTHRD_H
#define INCLUDED_EVNTTHRD_H

#ifndef INCLUDED_TASKER_H
#include "tasker.h"
#endif
#ifndef INCLUDED_STL_MAP
#include <map>
#endif
#ifndef INCLUDED_STL_STRING
#include <string>
#endif

namespace Tasker
{
    class NativeMutex;
}

//! \brief Class that manages an event-handling thread
//!
//! This class takes care of the boilerplate parts of creating a
//! ModsEvent and an event-handling thread.  The typical use is:
//!
//! -# Instantiate an EventThread object.
//! -# Call GetEvent() to get the ModsEvent that this object created.
//! -# Call SetHandler() to set the handler function that will be
//!    called whenever the event gets set.  Setting the handler causes
//!    the thread to start.
//!
//! To stop the thread and free all resources:
//!
//! -# Call SetHandler() to set the handler to NULL.  This gracefully
//!    stops the thread.  (The destructor also does this, but it's
//!    better to do it manually because the destructor can't return
//!    the RC.)
//! -# Delete the EventThread object.  This frees the ModsEvent, and
//!    stops the thread if you didn't stop it manually.
//!
class EventThread
{
public:
    typedef void (*HandlerFunc)(void *pData);

    EventThread(UINT32 stackSize = Tasker::MIN_STACK_SIZE,
                const char *name = "EventThread",
                bool bDetachThread = false);
    ~EventThread();
    static EventThread *FindEventThread(ModsEvent *pEvent);
    static void Cleanup();

    ModsEvent   *GetEvent();
    void        *GetOsEvent(UINT32 hClient, UINT32 hDevice)
    {
        return Tasker::GetOsEvent(GetEvent(), hClient, hDevice);
    }
    RC           SetHandler(HandlerFunc handler, void *pData);
    RC           SetHandler(void (*handler)());
    HandlerFunc  GetHandler() { return m_Handler; }
    void         SetCountingEvent();

private:
    static void ThreadFunction(void *pEventThread);

    ModsEvent        *m_pEvent;       //!< Event that triggers the handler
    UINT32            m_StackSize;    //!< Thread's stack size
    string            m_Name;         //!< Name of event & thread
    HandlerFunc       m_Handler;      //!< Handler function
    void             *m_pHandlerData; //!< Data passed to handler func
    volatile INT32    m_Count;        //!< Num. pending events
    Tasker::ThreadID  m_ThreadId;     //!< Thread that calls the handler
    volatile Tasker::ThreadID m_ExitThreadId; //!< Set this to m_ThreadId to exit it
    static map<ModsEvent*, EventThread*> s_EventThreads;
    static Tasker::NativeMutex           s_EventThreadsMutex;
    bool              m_bDetachThread;//!< True if the processing thread should
                                      //!< be detached
};

#endif // INCLUDED_EVNTTHRD_H
