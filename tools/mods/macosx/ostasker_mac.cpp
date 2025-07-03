/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012,2014,2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

/**
 * @file   ostasker.cpp
 * @brief  OS-specific implementation of Tasker.
 *
 */

extern "C"
{
#include <unistd.h>
#include <pthread.h>
#include <mach/mach.h>
}

#include <list>
#include "core/include/tasker.h"
#include "core/include/tasker_p.h"
#include "core/include/tee.h"
#include "core/include/xp.h"

static void* PthreadThreadWrapper (void * pvPthreadModsThread);
static pthread_mutex_t s_mods_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t s_tmp_mutex  = PTHREAD_MUTEX_INITIALIZER;

//------------------------------------------------------------------------------
//! @class(PthreadModsThread) Pthread Macos specific variant of ModsThread.
//!
//! Adds pthread_t thread handle
//! To cripple the pthread's kernel thread library (preemptive) into a cooperative
//! scheduler, we use a condition variable "run", condition mutext, and make the
//! thread's run function to wait until the condition "run" is set to true.
//!
struct PthreadModsThread : public ModsThread
{
    //The pthread thread handle
    pthread_t        thread;

    //Thread only run if condition variable "run" is true;
    bool             run;

    //pthread_cond_t for the thread
    pthread_cond_t   run_cond;

    //this thread's attr
    pthread_attr_t attr;

    PthreadModsThread
    (
        Tasker::ThreadID   id,
        Tasker::ThreadFunc func,
        void *             args,
        UINT32             stackSize,
        const char *       name
    );
    ~PthreadModsThread ();
};

PthreadModsThread::PthreadModsThread
(
    Tasker::ThreadID   id,
    Tasker::ThreadFunc func,
    void *             args,
    UINT32             stackSize,
    const char *       name
)
: ModsThread(id, func, args, name)
{
    if (stackSize < Tasker::MIN_STACK_SIZE)
        stackSize = Tasker::MIN_STACK_SIZE;

    // Initialize the event
    pthread_cond_init(&run_cond, NULL);

    run = false;

    if (id == 0)
    {
        // Don't start a thread if this is the fake main ModsThread.
        thread = pthread_self();

        // Get the One True Mutex, aka cooperative multithreading mutex
        pthread_mutex_lock(&s_mods_mutex);
    }
    else
    {
        //Set the stack size thread attribute first
        if (0 != pthread_attr_init(&attr))
        {
            Printf(Tee::PriHigh, "Failed to init thread's attr.");
        }
        pthread_attr_setstacksize(&attr, stackSize);

        // Sync message from new thread
        pthread_mutex_lock(&s_tmp_mutex);

        //create the actual thread
        int status = pthread_create(&thread, &attr, &PthreadThreadWrapper, (void*)this);
        if (0 != status)
        {
            Printf(Tee::PriHigh, "Failed to create thread.");
        }

        // Allow the new thread to get into the One True Mutex system
        pthread_mutex_unlock(&s_mods_mutex);

        // the other thread will unlock this when it's done
        pthread_mutex_lock(&s_tmp_mutex);

        // Reacquire the One True Mutex
        pthread_mutex_lock(&s_mods_mutex);
        pthread_mutex_unlock(&s_tmp_mutex);
    }
}

PthreadModsThread::~PthreadModsThread ()
{
    pthread_cond_destroy(&run_cond);

    //If this is not the main thread
    if (Id != 0)
    {
        pthread_attr_destroy(&attr);
    }
}

//------------------------------------------------------------------------------
//! Helper class to wait for the run condition of a thread
void WaitForRun(PthreadModsThread* pThread)
{
    //Wait to be signalled. This wakeup also implies that
    // we have gained control of the One True Mutex

    pthread_cond_wait( &(pThread->run_cond), &s_mods_mutex );
}

//------------------------------------------------------------------------------
//! Helper function to set run of thread to false
void SuspendRun(PthreadModsThread* pThread)
{
    pThread->run = false;
}

//------------------------------------------------------------------------------
//! Helper function to set run of thread to true and signal the condition
void StartRun(PthreadModsThread* pThread)
{
    pThread->run = true;
    pthread_cond_signal(&pThread->run_cond);
}

//------------------------------------------------------------------------------
//! Thread wrapper, starts the mods thread and has hooks for exiting cleanly.
//
static void* PthreadThreadWrapper (void * pvPthreadModsThread)
{
    PthreadModsThread * pThread = (PthreadModsThread *) pvPthreadModsThread;

    //Aquire the One True Mutex
    pthread_mutex_lock(&s_mods_mutex);
    pthread_mutex_unlock(&s_tmp_mutex);

    //Wait until run is true
    WaitForRun(pThread);

    // Run the mods thread function.
    (*pThread->Func)(pThread->Args);

    // Bye bye.
    Tasker::ExitThread();
    return NULL; // never exelwted.
}

//------------------------------------------------------------------------------
//! Pthread specific version of TaskerPriv::OsCreateThread
//
ModsThread * TaskerPriv::OsCreateThread
(
    Tasker::ThreadID   id,
    Tasker::ThreadFunc func,
    void *             args,
    UINT32             stacksize,
    const char *       name
)
{
    return new PthreadModsThread (id, func, args, stacksize, name);
}

//------------------------------------------------------------------------------
//! Clean up a dead thread, now that it is not the current thread.
//! Called by Tasker::Join.
//
void TaskerPriv::OsCleanupDeadThread (ModsThread * pModsThread)
{
    MASSERT(pModsThread);
    MASSERT(pModsThread->Dead);

    // This cast is necessary, since the destructor is NOT virtual.
    // Otherwise, the base-class destructor only would be called.
    PthreadModsThread * pThread = (PthreadModsThread *)pModsThread;

    if (0 == pThread->Id)
    {
        // Special case for the "main" thread, don't kill this thread.
        // We are being called from Tasker::Cleanup.
    }
    else
    {
        // Wake the thread at the "wrong" time, so that it will exit from
        // where it is lwrrently blocked in OsNextThread.

        pthread_mutex_unlock(&s_mods_mutex);
        StartRun(pThread);
        pthread_join(pThread->thread, NULL);

        pthread_mutex_lock(&s_mods_mutex);
    }

    m_Threads[pThread->Id] = 0;
    delete pThread;
}

//Needed?
PthreadModsThread * GetLwrrThread(vector<ModsThread *> m_Threads)
{
    PthreadModsThread * Th;

    pthread_t threadid = pthread_self();

    for (UINT32 i = 0; i < m_Threads.size(); i++)
    {
        Th = (PthreadModsThread *)m_Threads[i];

        if (Th->thread == threadid)
            return Th;
    }

    MASSERT(0);
    return NULL;
}

//------------------------------------------------------------------------------
//! Pthread specific version of TaskerPriv::OsNextThread
//
void TaskerPriv::OsNextThread ()
{
    // Wake threads whose events/timeouts have oclwrred.
    ModsEvent::ProcessPendingSets();

    // Loop until we find a thread we can switch to.
    while (! m_Ready.size())
    {
        // Let the non-mods threads (if any) run.
        // Only one mods thread is marked as runnable at a time,
        // so we will come back here.
        ModsEvent::ProcessPendingSets();
    }

    // Go to the next ready thread.
    // Note that we walk the Ready list from end()-1 to begin(), because of
    // details of the way list::insert and list::erase work.
    if (m_Ready.begin() == m_ReadyIt)
        m_ReadyIt = m_Ready.end();
    --m_ReadyIt;

    PthreadModsThread * pOld = (PthreadModsThread *)m_pLwrrentThread;
    m_pLwrrentThread         = *m_ReadyIt;
    PthreadModsThread * pNew = (PthreadModsThread *)m_pLwrrentThread;
    //Printf(Tee::PriDebug, "NextThread: %d (%s) to %d (%s)\n", pOld->Id, pOld->Name, pNew->Id, pNew->Name);

    //Change the old thread's run = false, and signal the new thread to run
    //There should be exactly one Mods thread runnable at a time,
    if (pOld != pNew)
    {
        SuspendRun(pOld);

        StartRun(pNew);

        //Wait until the we are let go
        WaitForRun(pOld);
    }

    if (m_pLwrrentThread == pOld)
    {
        // If pLwrrent == pOld, we are the current legitimate MODS thread, and we
        // should keep on exelwting.
        return;
    }

    // Else, we are Dead, and our event was signalled by CleanupDeadThread.
    MASSERT(pOld->Dead);

    pthread_mutex_unlock(&s_mods_mutex);

    void* retval = NULL;
    pthread_exit(retval);
}

//! A pthread thread for generating the ticking event.
void TickThread(void* pvMsecs)
{
    UINT32 msecs = *(UINT32*)pvMsecs;

    while (1)
    {
        UINT64 Lwrr = 0;
        //Stall for the desired number of msec.
        UINT64 End = Xp::QueryPerformanceCounter()
            + (msecs * (UINT64)Xp::QueryPerformanceFrequency() / 1000LL);

        while ((Lwrr = Xp::QueryPerformanceCounter()) < End)
        {
            Tasker::Yield();
        }
        ModsTimeout::s_IsrTickCount++;

        if (Tasker::IsInitialized())
            Tasker::SetEvent(ModsTimeout::s_TickEvent);
        else
            break;   // Tasker::Cleanup has been called.
    }
}

RC ModsTimeout::OsInitialize()
{
    s_TicksPerSec = 10.0;

    static UINT32 msecsPerTick = UINT32(1000.0 / s_TicksPerSec);
    s_SafeTimeoutMs = 2*msecsPerTick;

    // XXX Shouldn't we use a native thread here?  Using a MODS thread here seems
    // to defeat the whole design of the MODS timeout code.
    Tasker::ThreadID threadId = Tasker::CreateThread
    (
        TickThread,
        &msecsPerTick,
        Tasker::MIN_STACK_SIZE,
        "Ticker"
    );

    return OK;
}

void ModsTimeout::OsCleanup()
{
    // XXX Wait for TickThread to finish here if it becomes a native thread
    //  one day
}

//! Report number of bytes free on stack of current thread.
int TaskerPriv::OsStackBytesFree () const
{
    const PthreadModsThread * plwr = (const PthreadModsThread *)m_pLwrrentThread;
    char * stackbase;

    if (plwr->Id)
    {
        pthread_attr_getstackaddr(&plwr->attr, (void**)&stackbase);
        char tmp;
        return &tmp - stackbase;
    }
    else
    {
        // The main thread can't run out of space in a virtual memory system.
        // Right?
        return 1;
    }
}

//------------------------------------------------------------------------------
void ModsEvent::OsAllocEvent(UINT32 hClient, UINT32 hDevice)
{
#if defined(INCLUDE_GPU) && !defined(CLIENT_SIDE_RESMAN)
    m_OsEvent = Xp::AllocOsEvent(hClient, hDevice);
#else
    m_OsEvent = this;
#endif
}

void ModsEvent::OsFreeEvent(UINT32 hClient, UINT32 hDevice)
{
#if defined(INCLUDE_GPU) && !defined(CLIENT_SIDE_RESMAN)
    Xp::FreeOsEvent(m_OsEvent, hClient, hDevice);
#endif
}

//------------------------------------------------------------------------------
void ModsEvent::OsProcessPendingSets()
{
#if defined(INCLUDE_GPU) && !defined(CLIENT_SIDE_RESMAN)
    mach_msg_empty_rcv_t msg;
    kern_return_t status;

    for (UINT32 ii = 0; ii < s_Events.size(); ii++)
    {
        ModsEvent *pEvent = s_Events[ii];
        if (pEvent != NULL && pEvent->m_OsEvent != NULL)
        {
            mach_port_t port = (mach_port_t)(uintptr_t)pEvent->m_OsEvent;
            bool done = false;
            while (!done)
            {
                status = mach_msg
                (
                    &msg.header,
                    MACH_RCV_MSG | MACH_RCV_TIMEOUT,
                    0,
                    sizeof(msg),
                    port,
                    0,
                    MACH_PORT_NULL
                );
                if (status == MACH_MSG_SUCCESS)
                {
                    pEvent->Set();
                }
                else
                {
                    MASSERT(status == MACH_RCV_TIMED_OUT);
                    done = true;
                }
            }
        }
    }
#endif
}
//------------------------------------------------------------------------------
void Tasker::AcquireMutex(void *MutexHandle)
{
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;
    ThreadID tid = LwrrentThread();

    while ((Mutex->LockCount > 0) && (Mutex->Owner != tid))
    {
        Yield();
    }

    // Mods uses a cooperative tasker, thread switch oclwrs only at Yield time.
    // This code would be unsafe in a preemptive system.
    Mutex->Owner = tid;
    Mutex->LockCount++;
}
//------------------------------------------------------------------------------
void Tasker::ReleaseMutex(void *MutexHandle)
{
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;

    MASSERT(Mutex->LockCount > 0);

    MASSERT(Mutex->Owner == LwrrentThread());

    Mutex->LockCount--;
}
//------------------------------------------------------------------------------
bool Tasker::TryAcquireMutex(void *MutexHandle)
{
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;
    ThreadID tid = LwrrentThread();

    // Mods uses a cooperative tasker, thread switch oclwrs only at Yield time.
    // This code would be unsafe in a preemptive system.
    if ((Mutex->LockCount == 0) || (Mutex->Owner == tid))
    {
        Mutex->Owner = tid;
        Mutex->LockCount++;
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------
bool Tasker::CheckMutexOwner(void *MutexHandle)
{
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;
    return (Mutex->Owner == LwrrentThread());
}

