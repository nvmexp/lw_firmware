/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   ostasker.cpp
 * @brief  DJGPP 32-bit dos specific implementation of Tasker.
 *
 */
#include "core/include/tasker.h"
#include "core/include/tasker_p.h"
#include "core/include/xp.h"
#include "core/include/tee.h"
#include <list>
#include <signal.h>
#include <sys/time.h>

//------------------------------------------------------------------------------
//! @class(DosModsThread)  DOS specific variant of ModsThread.
//!
//! Adds a stack base and stack top pointers to ModsThread.
//! Under DOS we use direct register manipulation to switch threads.
//!
//! Adds thread-local-storage.
//!
struct DosModsThread : public ModsThread
{
    void * StackBase;
    void * StackPtr;

    DosModsThread
    (
        Tasker::ThreadID   id,
        Tasker::ThreadFunc func,
        void *             args,
        UINT32             stackSize,
        const char *       name
    );
    ~DosModsThread ();
};

void KillThread();

DosModsThread::DosModsThread
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

    if (id == 0)
    {
        // Special case for the main thread.  Don't allocate or prepare a stack.
        StackBase = 0;
        StackPtr  = 0;
    }
    else
    {
        StackBase = new char [stackSize];
        size_t * pStack = (size_t *)((char*)StackBase + stackSize);

#ifdef __x86_64__
        // What gets popped in SwitchThreads
        // 0 popq    %rdi          /* arg for initial launch */
        // 1 popq    %r15
        // 2 popq    %r14
        // 3 popq    %r13
        // 4 popq    %r12
        // 5 popq    %rbp
        // 6 popq    %rbx
        // 7 ret
        // And then when the thread is done, it will pop a ret:
        // 8 ret

        pStack -= 9;
        memset(pStack, 0, 9*sizeof(size_t));

        pStack[0] = (size_t) args;
        pStack[5] = (size_t) &(pStack[9]);
        pStack[7] = (size_t) func;
        pStack[8] = (size_t) &KillThread;

#else // __x86_64__

        // pStack is pointing just past the end of the allocated stack.
        //
        // Our job is to fake up the stack to look like:
        //   KillThread() called
        //      pFunction which called
        //         TaskerPriv::OsNextThread().
        //
        // If we get it right, the first time OsNextThread switches to the new
        // stack, it will return to ThreadFunction.  When ThreadFunction returns
        // it will return to KillThread, which will handle cleanup.
        //
        //    --------------------
        //    | Args             |
        //    --------------------
        //    | &KillThread      |
        //    --------------------
        //    | pFunction        |
        //    --------------------
        //    |                  |
        //    -   -   -   -   -  -
        //    | pushal Registers |
        //    -   -   -   -   -  -
        //    |   (32 bytes)     |
        //    -   -   -   -   -  -
        //    |                  |<--- pThread->StackPointer();
        //    --------------------
        //    |       ...        |
        //    --------------------
        //    |       ...        |
        //    --------------------
        //    | THREAD_FENCEPOST | <--- pThread->EndOfStack();
        //    --------------------
        //

        pStack--; *pStack = (long) args;
        pStack--; *pStack = (long) &KillThread;
        pStack--; *pStack = (long) func;

        // Space for the 8 registers pushed by pushal.
        pStack -= 8;

        // We're going to write to what will be put in %ebp. (yes, ebp, not esp)
        // Because of the way the compiler processes the ThreadSwitch
        // function, we want it to point to the address one word below
        // where we're storing pThread->Function().
        memset(pStack, 0, 8*4);
        pStack[2] = (long) &(pStack[7]);
#endif // __x86_64__

        StackPtr = pStack;
    }

#if defined(DO_STACK_CHECKS)
    if (id != 0)
    {
        unsigned long * pLastStackWord = (unsigned long *)StackBase;
        *pLastStackWord = 0xFEDCBA98;
    }
#endif

}

DosModsThread::~DosModsThread ()
{
    if (Id)
        delete [] (char*)StackBase;
}

//------------------------------------------------------------------------------
//! This is a static function for marking the current thread dead when
//! the threadfunc returns.
//! The actual cleanup is put off until later when we aren't running out of
//! the stack that is about to be freed.
void KillThread ()
{
    Tasker::ExitThread();
}

//------------------------------------------------------------------------------
//! Dos specific version of TaskerPriv::OsCreateThread
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
    return new DosModsThread (id, func, args, stacksize, name);
}

//------------------------------------------------------------------------------
//! Report number of bytes free on stack of current thread.
int TaskerPriv::OsStackBytesFree () const
{
    const DosModsThread * plwr = (const DosModsThread *)m_pLwrrentThread;
    if (plwr->Id)
    {
        char tmp;
        return &tmp - (char *)(plwr->StackBase);
    }
    else
    {
        // The main stack is huge in linux and windows, and (I think) grows
        // as needed, so we can't run out of space.
        return 1;
    }
}

//------------------------------------------------------------------------------
//! Clean up a dead thread, now that it is not the current thread.
//! Called by Tasker::Join.
//
void TaskerPriv::OsCleanupDeadThread (ModsThread * pModsThread)
{
    MASSERT(pModsThread);
    MASSERT(pModsThread->Dead);

    // Don't forget this cast!  This is not a virtual class, we must make sure
    // the right destructor gets called.
    DosModsThread * pThread = (DosModsThread *)pModsThread;

    m_Threads[pThread->Id] = 0;
    delete pThread;
}

// Extern for the assembly language implemented thread switching routine.
// Ensure that arguments are always passed in registers to be compatible with
// the assembly implementation
extern "C"
{
    void _SwitchThreads(size_t *pSaveSP, size_t NewSP) __attribute__ ((regparm(2)));
}
#define SwitchThreads _SwitchThreads

//------------------------------------------------------------------------------
//! DOS specific version of TaskerPriv::OsNextThread
//
void TaskerPriv::OsNextThread ()
{
    // Loop until we find a thread we can switch to.
    do
    {
        // Wake threads whose events/timeouts have oclwrred.
        ModsEvent::ProcessPendingSets();
    }
    while (! m_Ready.size());

    // Go to the next ready thread.
    // Note that we walk the Ready list from end()-1 to begin(), because of
    // details of the way list::insert and list::erase work.
    if (m_Ready.begin() == m_ReadyIt)
        m_ReadyIt = m_Ready.end();
    --m_ReadyIt;

    DosModsThread * pOld = (DosModsThread *)m_pLwrrentThread;
    DosModsThread * pNew = (DosModsThread *)*m_ReadyIt;

    //Printf(Tee::PriLow, "NextThread: %d (%s) to %d (%s)\n", pOld->Id, pOld->Name, pNew->Id, pNew->Name);

    // In debug builds, check for stack overruns.
#if defined(DO_STACK_CHECKS)
    if (pNew->Id != 0)
    {
        int stackBytesFree = ((char*)pNew->StackPtr) - (char *)pNew->StackBase;

        if ((0xFEDCBA98 != *(unsigned long *)pNew->StackBase) || (stackBytesFree < 1))
        {
            Printf(Tee::PriHigh, "Stack corruption in thread %d %s StackFree = %d\n",
                    pNew->Id,
                    pNew->Name,
                    stackBytesFree
                    );
            Printf(Tee::PriHigh, "  stack guard band = 0x%08lx, expected 0xFEDCBA98\n",
                    *(unsigned long *)pNew->StackBase
                    );
            Printf(Tee::PriHigh, "  prev thread %d %s\n",
                    pOld->Id,
                    pOld->Name
                    );
            fflush(stdout);
            Xp::BreakPoint();
        }
        //if ((stackBytesFree > 4) && (pNew != pOld))
        //{
        //   memset(4 + (char *)pNew->StackBase, 0xff, stackBytesFree-4);
        //}
    }
#endif

    if (pNew == pOld)
        return;

    m_pLwrrentThread     = pNew;

    size_t * pSaveSP = (size_t *)&pOld->StackPtr;
    size_t   NewSP   = (size_t)   pNew->StackPtr;

    SwitchThreads(pSaveSP, NewSP);

    // What happens now...
    // The first time a created thread is switched to, SwitchThread will return
    // to the actual thread function instead of here.  For all subsequent
    // thread switches, SwitchThreads will return here since the only way to
    // actually switch threads is to call Tasker::Yield() which calls this
    // function
}

static void AlarmHandler(int)
{
     ModsTimeout::s_IsrTickCount++;
     Tasker::SetEvent(ModsTimeout::s_TickEvent);
}

//------------------------------------------------------------------------------
//! DOS specific version of ModsTimeout::OsInitialize.
//
RC ModsTimeout::OsInitialize()
{
     // Configurable, but this should be a reasonable value
     s_TicksPerSec = 10.0;

     UINT32 msecsPerTick = UINT32(1000.0 / s_TicksPerSec);
     s_SafeTimeoutMs = 2*msecsPerTick;

     // Hook the alarm signal that the timer will generate.
     signal(SIGALRM, AlarmHandler);

     // Set up a relwrring timer that ticks every msecsPerTick milliseconds
     struct itimerval Timer;
     Timer.it_interval.tv_sec = 0;
     Timer.it_interval.tv_usec = msecsPerTick*1000;
     Timer.it_value.tv_sec = 0;
     Timer.it_value.tv_usec = msecsPerTick*1000;
     setitimer(ITIMER_REAL, &Timer, NULL);

     return OK;
}

//------------------------------------------------------------------------------
void ModsTimeout::OsCleanup()
{
     // Shut off our timer
     struct itimerval Timer;
     Timer.it_interval.tv_sec = 0;
     Timer.it_interval.tv_usec = 0;
     Timer.it_value.tv_sec = 0;
     Timer.it_value.tv_usec = 0;
     setitimer(ITIMER_REAL, &Timer, NULL);
}

//------------------------------------------------------------------------------
void Tasker::AcquireMutex(void *MutexHandle)
{
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;
    ThreadID tid = LwrrentThread();

    while ((Mutex->LockCount > 0) && (Mutex->Owner != tid))
    {
        MASSERT(CanThreadYield());
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
