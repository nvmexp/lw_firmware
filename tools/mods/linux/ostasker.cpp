/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2010,2013-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwRmApi.h"
#include "core/include/lwrm.h"
#include <vector>
#include <sys/poll.h>

#if !defined(USE_PTHREADS)
#include "coop_tasker.cpp"
#else

#include "core/include/platform.h"
#include "linuxmutex.h"

#if defined(INCLUDE_GPU) && !defined(CLIENT_SIDE_RESMAN) && !defined(TEGRA_MODS)
#define REMOTE_RM_EVENTS
#endif

#if defined(REMOTE_RM_EVENTS)
#include "core/include/xp.h"
#endif

/**
 * @file   ostasker.cpp
 * @brief  OS-specific implementation of Tasker.
 *
 */

extern "C"
{
#include <unistd.h>
#include <pthread.h>
#include <time.h>       // for nanosleep()
}

#include <list>
#include "core/include/tasker.h"
#include "core/include/tasker_p.h"
#include "core/include/tee.h"

static void* LinuxThreadWrapper (void * pvLinuxModsThread);

namespace
{
    Tasker::LinuxMutex s_TaskSwitchMutex(Tasker::normal_mutex);
}

//------------------------------------------------------------------------------
//! @class(LinuxModsThread) Linux-specific variant of ModsThread.
//!
//! Adds pthread_t thread handle
//! To cripple the Linux's kernel thread library (preemptive) into a cooperative
//! scheduler, we use a condition variable "run", condition mutex, and make the
//! thread's run function to wait until the condition "run" is set to true.
//!
struct LinuxModsThread : public ModsThread
{
    //The linux thread handle
    pthread_t        thread;

    //Thread only run if condition variable "run" is true;
    bool             run;

    //Mutex for condition to prevent race condition
    Tasker::LinuxMutex cond_mutex;

    //pthread_cond_t for the thread
    pthread_cond_t   run_cond;

    //this thread's attr
    pthread_attr_t attr;

    LinuxModsThread
    (
        Tasker::ThreadID   id,
        Tasker::ThreadFunc func,
        void *             args,
        UINT32             stackSize,
        const char *       name
    );
    ~LinuxModsThread ();
};

LinuxModsThread::LinuxModsThread
(
    Tasker::ThreadID   id,
    Tasker::ThreadFunc func,
    void *             args,
    UINT32             stackSize,
    const char *       name
)
: ModsThread(id, func, args, name),
  cond_mutex(Tasker::normal_mutex)
{
    if (stackSize < Tasker::MIN_STACK_SIZE)
        stackSize = Tasker::MIN_STACK_SIZE;

    //We need to do this because we can't assign the macros other than
    //in a initialization right after a variable is declared.
    pthread_cond_t run_cond_temp = PTHREAD_COND_INITIALIZER;
    run_cond = run_cond_temp;

    run = false;

    if (id == 0)
    {
        // Don't start a thread if this is the fake main ModsThread.
        thread = pthread_self();
    }
    else
    {
        //Set the stack size thread attribute first
        if (0 != pthread_attr_init(&attr))
        {
            Printf(Tee::PriHigh, "Failed to init thread's attr.");
        }
        pthread_attr_setstacksize(&attr, stackSize);

        //create the actual thread
        int status = pthread_create(&thread, &attr, &LinuxThreadWrapper, (void*)this);
        if (0 != status)
        {
            Printf(Tee::PriHigh, "Failed to create thread.");
        }
    }
}

LinuxModsThread::~LinuxModsThread ()
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
void WaitForRun(LinuxModsThread* pThread)
{
    //This only looks bad. We are NOT polling the flag run. The thread is
    //suspended while we are waiting for somebody to call pthread_cond_signal.
    //The loop is necessary because, according to documentation, there are (rarely)
    //sporadic condition event? I don't know why Linux thread lib could be so
    //crappy. So we are making sure that we wake up only if run is true.

    //Lock the mutex for run
    Tasker::LinuxMutexHolder lock(pThread->cond_mutex);
    while (!pThread->run) //Not polling!
    {
        pthread_cond_wait( &(pThread->run_cond), pThread->cond_mutex );
    }
}

//------------------------------------------------------------------------------
//! Helper function to set run of thread to false
void SuspendRun(LinuxModsThread* pThread)
{
    Tasker::LinuxMutexHolder lock(pThread->cond_mutex);
    pThread->run = false;
}

//------------------------------------------------------------------------------
//! Helper function to set run of thread to true and signal the condition
void StartRun(LinuxModsThread* pThread)
{
    Tasker::LinuxMutexHolder lock(pThread->cond_mutex);
    pThread->run = true;
    pthread_cond_signal(&pThread->run_cond);
}

//------------------------------------------------------------------------------
//! Thread wrapper, starts the mods thread and has hooks for exiting cleanly.
//
static void* LinuxThreadWrapper (void * pvLinuxModsThread)
{
    LinuxModsThread * pThread = (LinuxModsThread *) pvLinuxModsThread;

    //Wait until run is true
    WaitForRun(pThread);

    // Run the mods thread function.
    (*pThread->Func)(pThread->Args);

    // Bye bye.
    Tasker::ExitThread();
    return NULL; // never exelwted.
}

//------------------------------------------------------------------------------
//! Linux specific version of TaskerPriv::OsCreateThread
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
    return new LinuxModsThread (id, func, args, stacksize, name);
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
    LinuxModsThread * pThread = (LinuxModsThread *)pModsThread;

    if (0 == pThread->Id)
    {
        // Special case for the "main" thread, don't kill this thread.
        // We are being called from Tasker::Cleanup.
    }
    else
    {
        // Wake the thread at the "wrong" time, so that it will exit from
        // where it is lwrrently blocked in OsNextThread.
        StartRun(pThread);
        pthread_join(pThread->thread, NULL);
    }

    m_Threads[pThread->Id] = 0;
    delete pThread;
}

//------------------------------------------------------------------------------
//! Linux specific version of TaskerPriv::OsNextThread
//
void TaskerPriv::OsNextThread ()
{
    // Perform OS yield if this is not a tasker thread
    bool isTaskerThread = false;
    {
        Tasker::LinuxMutexHolder lock(s_TaskSwitchMutex);
        isTaskerThread = pthread_self() == ((LinuxModsThread*)m_pLwrrentThread)->thread;
    }
    if (! isTaskerThread)
    {
        pthread_yield();
        return;
    }

    // Wake threads whose events/timeouts have oclwrred.
    ModsEvent::ProcessPendingSets();

    // Loop until we find a thread we can switch to.
    while (m_Ready.empty())
    {
        // Let the non-mods threads (if any) run.
        timespec t;
        t.tv_sec = 0;
        t.tv_nsec = 100 * 1000;
        nanosleep(&t, NULL);
        ModsEvent::ProcessPendingSets();
    }

    // Go to the next ready thread.
    // Note that we walk the Ready list from end()-1 to begin(), because of
    // details of the way list::insert and list::erase work.
    if (m_Ready.begin() == m_ReadyIt)
        m_ReadyIt = m_Ready.end();
    --m_ReadyIt;

    LinuxModsThread* pOld = NULL;
    LinuxModsThread* pNew = NULL;
    {
        Tasker::LinuxMutexHolder lock(s_TaskSwitchMutex);
        pOld             = (LinuxModsThread *)m_pLwrrentThread;
        m_pLwrrentThread = *m_ReadyIt;
        pNew             = (LinuxModsThread *)m_pLwrrentThread;
    }

    if (pOld != pNew)
    {
        //Change the old thread's run = false, and signal the new thread to run
        //There should be exactly one Mods thread runnable at a time,
        SuspendRun(pOld);
        StartRun(pNew);

        //Wait until the we are let go
        WaitForRun(pOld);
    }
    else
    {
        // There is only one Tasker thread active,
        // so yield to other non-Tasker threads
        pthread_yield();
    }

    if (m_pLwrrentThread == pOld)
    {
        // If pLwrrent == pOld, we are the current legitimate MODS thread, and we
        // should keep on exelwting.
        return;
    }

    // Else, we are Dead, and our event was signalled by CleanupDeadThread.
    MASSERT(pOld->Dead);
    pthread_exit(NULL);
}

//! Report number of bytes free on stack of current thread.
int TaskerPriv::OsStackBytesFree () const
{
    const LinuxModsThread * plwr = (const LinuxModsThread *)m_pLwrrentThread;
    char * stackbase;
    size_t stackSize;

    if (plwr->Id)
    {
        pthread_attr_getstack(&plwr->attr, (void**)&stackbase, &stackSize);
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
pthread_t      s_TickThread = 0;
pthread_attr_t s_TickThreadAttr;

//! A linux thread for generating the ticking event.
//! This is quite inefficient, sorry.
void * TickThread(void* pvMsecs)
{
    UINT32 msecs = *(UINT32*)pvMsecs;

    timespec t;
    t.tv_sec = msecs / 1000;
    // miliseconds * (1000 us / ms) * (1000 ns / us)
    t.tv_nsec = (msecs % 1000) * 1000 * 1000;

    while (1)
    {
        nanosleep(&t, NULL);
        ModsTimeout::s_IsrTickCount++;

        if (Tasker::IsInitialized())
        {
            Tasker::SetEvent(ModsTimeout::s_TickEvent);
        }
        else
        {
            Printf(Tee::PriDebug, "TickThread exiting\n");
            break;   // Tasker::Cleanup has been called.
        }
    }
    return NULL;
}

RC ModsTimeout::OsInitialize()
{
    s_TicksPerSec = 10.0;

    static UINT32 msecsPerTick = UINT32(1000.0 / s_TicksPerSec);
    s_SafeTimeoutMs = 2*msecsPerTick;

    if (0 != pthread_attr_init(&s_TickThreadAttr))
    {
        Printf(Tee::PriHigh, "Failed to init TickThread's attr.");
        return RC::SOFTWARE_ERROR;
    }
    pthread_attr_setstacksize(&s_TickThreadAttr, 4*1024);

    int status = pthread_create(&s_TickThread, &s_TickThreadAttr,
            &TickThread, &msecsPerTick);

    if (0 != status)
    {
        Printf(Tee::PriHigh, "Failed to create TickThread.");
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

void ModsTimeout::OsCleanup()
{
    pthread_join(s_TickThread, NULL);
}
//------------------------------------------------------------------------------
void Tasker::AcquireMutex(void *MutexHandle)
{
    Platform::Irql OldIrql = Platform::RaiseIrql(Platform::ElevatedIrql);
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;
    const long tid = static_cast<long>(pthread_self());

    while ((Mutex->LockCount > 0) && (Mutex->Owner != tid))
    {
        Platform::LowerIrql(OldIrql);
        Yield();
        OldIrql = Platform::RaiseIrql(Platform::ElevatedIrql);
    }

    // Mods uses a cooperative tasker, thread switch oclwrs only at Yield time.
    // This code would be unsafe in a preemptive system.
    Mutex->Owner = tid;
    Mutex->LockCount++;
    Platform::LowerIrql(OldIrql);
}
//------------------------------------------------------------------------------
void Tasker::ReleaseMutex(void *MutexHandle)
{
    const Platform::Irql OldIrql = Platform::RaiseIrql(Platform::ElevatedIrql);
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;

    MASSERT(Mutex->LockCount > 0);

    MASSERT(Mutex->Owner == static_cast<long>(pthread_self()));

    Mutex->LockCount--;
    Platform::LowerIrql(OldIrql);
}
//------------------------------------------------------------------------------
bool Tasker::TryAcquireMutex(void *MutexHandle)
{
    const Platform::Irql OldIrql = Platform::RaiseIrql(Platform::ElevatedIrql);
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;
    const long tid = static_cast<long>(pthread_self());

    bool acquired = false;;
    if ((Mutex->LockCount == 0) || (Mutex->Owner == tid))
    {
        Mutex->Owner = tid;
        Mutex->LockCount++;
        acquired = true;
    }
    Platform::LowerIrql(OldIrql);
    return acquired;
}
//------------------------------------------------------------------------------
bool Tasker::CheckMutexOwner(void *MutexHandle)
{
    const Platform::Irql OldIrql = Platform::RaiseIrql(Platform::ElevatedIrql);
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;
    bool bMutexOwner = ((Mutex->Owner == static_cast<long>(pthread_self())) && Mutex->LockCount > 0);
    Platform::LowerIrql(OldIrql);

    return bMutexOwner;
}

#endif // defined(USE_PTHREADS)

//------------------------------------------------------------------------------
#if defined(REMOTE_RM_EVENTS)
static int LinuxWaitForEvent(vector<void *> events, int timeout)
{
    struct pollfd ufds[events.size()];
    unsigned int ctr;
    LwU32 *fd;
    int ret;
    LwU32 moreEvents;
    LwUnixEvent unixEvent;

    //events[0] to events[events.size() - 1] must be pre-validated!
    for (ctr = 0; ctr < events.size(); ctr++)
    {
        fd = (LwU32 *) events[ctr];
        ufds[ctr].fd = *fd;
        ufds[ctr].events = POLLIN | POLLPRI;
        ufds[ctr].revents = 0;
    }

    ret = poll(ufds, events.size(), timeout);

    //-1 on error or timeout (no events found).  else index of Fd returned!
    if (ret <= 0) return -1;

    for (ctr = 0; ctr < events.size(); ctr++)
    {
        if (ufds[ctr].revents)
        {
            fd = (LwU32 *) events[ctr];
            /* empty the event FIFO */
            do {
                ret = LwRmGetEventData(LwRmPtr()->GetClientHandle(), *fd, (void *)&unixEvent, &moreEvents);
                if (ret != LW_OK)
                    return -1;
            } while (moreEvents);
            return ctr;
        }
    }
    return -1;
}
#endif // defined(REMOTE_RM_EVENTS)

//------------------------------------------------------------------------------
void ModsEvent::OsAllocEvent(UINT32 hClient, UINT32 hDevice)
{
#if defined(REMOTE_RM_EVENTS)
    m_OsEvent = Xp::AllocOsEvent(hClient, hDevice);
#else
    m_OsEvent = this;
#endif
}

//------------------------------------------------------------------------------
void ModsEvent::OsFreeEvent(UINT32 hClient, UINT32 hDevice)
{
#if defined(REMOTE_RM_EVENTS)
    Xp::FreeOsEvent(m_OsEvent, hClient, hDevice);
#endif
}

//------------------------------------------------------------------------------
void ModsEvent::OsProcessPendingSets()
{
#if defined(REMOTE_RM_EVENTS)

    vector<void *> hEvents;
    int dwCount;
    unsigned int i;

    // This is not by any means the most efficient implementation of hooking up
    // MODS events and OS events, but it should work.
    // BWAHAHAHA...

    // Build a vector of all the OS events
    dwCount = 0;
    for (i = 0; i < s_Events.size(); i++)
    {
        ModsEvent *Event = s_Events[i];
        if (Event && Event->m_OsEvent)
        {
            hEvents.push_back(Event->m_OsEvent);
            dwCount++;
        }
    }

    // Bail if no events
    if (!dwCount)
        return;

    // Ask which OS events have fired, and set their corresponding MODS events
    for (;;)
    {
        // Don't wait, just check which ones have fired - zero timeout
        int ret = LinuxWaitForEvent(hEvents, 0);

        if (ret >= 0 && ret < dwCount)
        {
            // One of the events was fired
            void * hEvent = hEvents[ret];
            for (i = 0; i < s_Events.size(); i++)
            {
                ModsEvent *Event = s_Events[i];
                if (Event && (Event->m_OsEvent == hEvent))
                {
                    Event->Set();
                }
            }

            // Don't abort out of the loop -- keep checking for more events that
            // have fired.
            // XXX Could we get stuck in an infinite loop here?  I would hope not,
            // but it seems hypothetically possible if an external agent were to
            // keep firing OS events faster than we could process them.
        }
        else
        {
            // Either timed out, or one of them was abandoned, or something else
            // went wrong.  In any of these cases, bail out; but the only error
            // condition we ever expect should happen here is the timeout.
            break;
        }
    }
#endif // defined(REMOTE_RM_EVENTS)
}

