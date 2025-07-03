/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tasker.cpp
 *
 * This file contains the portable parts of class Tasker's implementation.
 */

#include "core/include/tasker.h"
#include "core/include/tasker_p.h"
#include "jsapi.h"
#include "core/include/tee.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "core/include/abort.h"
#include "core/include/utility.h"
#include "core/include/cpu.h"
#include "core/utility/sharedsysmem.h"
#include <limits.h>
#include <math.h>
#include <algorithm> // for std::min
#include <stack>
#include <string.h>
#include <memory>

#include <boost/range/algorithm/transform.hpp>

// Here is the actual implementation object:
static TaskerPriv * s_pPriv = 0;
static FLOAT64 s_DefaultTimeoutMs = 1000;
static FLOAT64 s_SimTimeoutMs = Tasker::NO_TIMEOUT;

// Structure passed to PollAndAcquireMutexPollFunc
struct PollAndAcquireMutexPollArgs
{
    Tasker::PollFunc pollFunc;
    volatile void *pArgs;
    void *MutexHandle;
};

// This doesn't really fit with the thread stuff that lives in TaskerPriv,
// so I'm adding it at the "top level"
static bool s_CanThreadYield = true;

// Used to give each thread a unique number since the ID value
// can be reused from thread to thread.
static UINT32 s_LwrrentThreadInstance = 0;

// Time to sleep each time around PollHw, controlled by SetMaxHwPollHz.
// See GetHwPollSleepNsec().
static const UINT64 DEFAULT_HW_POLL_SLEEP_NSEC = ~(UINT64)0;
static UINT64 s_HwPollSleepNsec = DEFAULT_HW_POLL_SLEEP_NSEC;

// MODS tasker TLS implementation.  Not used when using Native Tasker.
static UINT32 TlsIndexAllocMask[TaskerPriv::TLS_NUM_INDEXES/32] = { 0 };
static UINT32 TlsIndexInheritMask[TaskerPriv::TLS_NUM_INDEXES/32] = { 0 };

namespace Tasker
{
    RC PollCommon
    (
        PollFunc        Function,
        volatile void * pArgs,
        FLOAT64         TimeoutMs,
        const char *    callingFunctionName,
        const char *    pollingFunctionName,
        bool            ObeyHwPollFreqLimit
    );

    UINT64 GetHwPollSleepNsec();

    RC AddTimeout(FLOAT64 timeoutMs,
                  const char *callingFunctionName,
                  const char *pollingFunctionName,
                  UINT32     *pTimeoutIndex);
    bool RemoveTimeout(UINT32 timeoutIndex);

    bool PollAndAcquireMutexPollFunc(void *pArgs);

    void   TlsInheritFromLwrrentThread (void **TlsArray);
}

//------------------------------------------------------------------------------
RC Tasker::Initialize()
{
    RC rc;

    // ModsEvent can initialize by itself (doesn't need to use any other class).
    CHECK_RC( ModsEvent::Initialize() );

    // TaskerPriv creates the "main" thread and its DeathEvent, so it must be
    // set up after ModsEvent is ready.
    s_pPriv = new TaskerPriv();

    // ModsTimeout creates a thread of its own, so it must be set up after
    // TaskerPriv is ready.
    CHECK_RC( ModsTimeout::Initialize() );

    CHECK_RC(SharedSysmem::Initialize());

    // Call Yield once, so the poll and timeout threads fall into
    // the standard place to block in OsNextThread.
    // This prevents ugly crashes-on-exit if Tasker::Cleanup gets called before
    // the other threads ever get going properly.
    return Yield();
}

//------------------------------------------------------------------------------
void Tasker::Cleanup()
{
    if (!IsInitialized())
        return;

    delete s_pPriv;
    s_pPriv = 0;

    ModsTimeout::Cleanup(); // Call after "s_pPriv = 0" so we can wait for the
                           //  Tick thread to finish

    ModsEvent::Cleanup();
    SharedSysmem::Shutdown();
}

//------------------------------------------------------------------------------
bool Tasker::CanThreadYield()
{
    return s_CanThreadYield;
}

//! \brief Tell the tasker that the current thread cannot be yielded.
void Tasker::SetCanThreadYield(bool canThreadYield)
{
    s_CanThreadYield = canThreadYield;
}

//------------------------------------------------------------------------------
Tasker::ThreadID Tasker::CreateThread
   (
      ThreadFunc     Function,
      void *         Args,
      UINT32         StackSize,
      const char *   Name
   )
{
    return s_pPriv->CreateThread (Function, Args, StackSize, Name);
}

//------------------------------------------------------------------------------
Tasker::DetachThread::DetachThread()
{
}

//------------------------------------------------------------------------------
Tasker::DetachThread::~DetachThread()
{
}

//------------------------------------------------------------------------------
Tasker::AttachThread::AttachThread()
{
}

//------------------------------------------------------------------------------
Tasker::AttachThread::~AttachThread()
{
}

//------------------------------------------------------------------------------
class ModsThreadGroup
{
public:
    ModsThreadGroup(Tasker::ThreadFunc func, void* arg, UINT32 numThreads);
    ~ModsThreadGroup();
    void Release();

    enum State
    {
        stateInit
        ,stateFailedCreation
        ,stateRunning
        ,stateBarrierError
        ,stateDestroy
    };

    Tasker::ThreadFunc       m_ThreadFunc;
    void*                    m_ThreadArg;
    UINT32                   m_Size;
    UINT32                   m_NumThreadsAlive;
    UINT32                   m_NumThreadsWaiting;
    vector<Tasker::ThreadID> m_ThreadIds;
    UINT32                   m_BarrierReleaseCount;
    State                    m_State;
    void*                    m_Mutex;
    SemaID                   m_Sema;
};

//------------------------------------------------------------------------------
ModsThreadGroup::ModsThreadGroup(Tasker::ThreadFunc func, void* arg, UINT32 numThreads)
: m_ThreadFunc(func)
, m_ThreadArg(arg)
, m_Size(numThreads)
, m_NumThreadsAlive(0)
, m_NumThreadsWaiting(0)
, m_BarrierReleaseCount(0)
, m_State(stateInit)
, m_Mutex(Tasker::AllocMutex("ModsThreadGroup::m_Mutex", Tasker::mtxUnchecked))
, m_Sema(Tasker::CreateSemaphore(0, "Barrier semaphore"))
{
}

//------------------------------------------------------------------------------
ModsThreadGroup::~ModsThreadGroup()
{
    Tasker::DestroySemaphore(m_Sema);
    Tasker::FreeMutex(m_Mutex);
}

//------------------------------------------------------------------------------
void ModsThreadGroup::Release()
{
    ++m_BarrierReleaseCount;
    for ( ; m_NumThreadsWaiting; --m_NumThreadsWaiting)
    {
        Tasker::ReleaseSemaphore(m_Sema);
    }
}

//------------------------------------------------------------------------------
namespace Tasker
{
    struct ModsThreadGroupWrapperArg
    {
        ModsThreadGroup* pGroup;
        UINT32 threadIdx;
    };

    void ModsThreadGroupWrapper(void* arg)
    {
        // Extract group from the init arg
        unique_ptr<ModsThreadGroupWrapperArg> pInit(
            static_cast<ModsThreadGroupWrapperArg*>(arg));
        ModsThreadGroup* const pGroup = pInit->pGroup;

        // Bail out if the group creation failed
        if (pGroup->m_State != ModsThreadGroup::stateRunning)
        {
            return;
        }

        // Setup thread information
        s_pPriv->m_pLwrrentThread->pGroup = pGroup;
        s_pPriv->m_pLwrrentThread->GThreadIdx = pInit->threadIdx;

        // Release the init structure
        pInit.reset(0);

        // Run the thread function
        pGroup->m_ThreadFunc(pGroup->m_ThreadArg);

        // Detach the thread from the group
        UINT32 numThreadsRemaining = ~0U;
        {
            MutexHolder lock(pGroup->m_Mutex);
            --pGroup->m_NumThreadsAlive;
            numThreadsRemaining = pGroup->m_NumThreadsAlive;

            // Release group if all remaining alive threads are waiting
            if ((numThreadsRemaining == pGroup->m_NumThreadsWaiting) &&
                (numThreadsRemaining > 0))
            {
                pGroup->Release();
            }
        }

        // Delete group if this is the last thread
        if (numThreadsRemaining == 0)
        {
            delete pGroup;
        }
    }
} // anonymous namespace

//------------------------------------------------------------------------------
vector<Tasker::ThreadID> Tasker::CreateThreadGroup
(
    ThreadFunc function,
    void* arg,
    UINT32 numThreads,
    const char* name,
    bool bStartGroup,
    void **ppvThreadGroup
)
{
    MASSERT(IsInitialized());

    // If the group is being started, Tasker has control of the group so
    // ppvThreadGroup should not be returned.  If not, then ppvThreadGroup must
    // not be null so there can be external control
    MASSERT(bStartGroup != (ppvThreadGroup != nullptr));

    // Create object identifying thread group
    unique_ptr<ModsThreadGroup> pGroup(
            new ModsThreadGroup(function, arg, numThreads));

    for (UINT32 i=0; i < numThreads; i++)
    {
        // Prepare thread init struct
        unique_ptr<ModsThreadGroupWrapperArg> pInit(new ModsThreadGroupWrapperArg);
        pInit->pGroup = pGroup.get();
        pInit->threadIdx = i;

        // Create unique thread name
        const string threadName = name + Utility::StrPrintf("%u", i);

        // Create the thread
        const ThreadID tid = CreateThread(
                ModsThreadGroupWrapper, pInit.get(), 0, threadName.c_str());
        if (tid == NULL_THREAD)
        {
            // Delete thread init structure for the failed thread
            pInit.reset(0);

            // Destroy previously created threads
            pGroup->m_State = ModsThreadGroup::stateFailedCreation;
            Yield();
            Join(pGroup->m_ThreadIds, NO_TIMEOUT);
            pGroup->m_ThreadIds.clear();
            break;
        }
        else
        {
            // Successfuly created thread will destroy the init structure
            // so don't delete it here
            pInit.release();
        }
        pGroup->m_ThreadIds.push_back(tid);
    }

    vector<Tasker::ThreadID> threadIds = pGroup->m_ThreadIds;
    if (!pGroup->m_ThreadIds.empty())
    {
        // Set the number of alive threads
        pGroup->m_NumThreadsAlive += numThreads;

        if (bStartGroup)
        {
            StartThreadGroup(pGroup.get());
        }
        else
        {
            *ppvThreadGroup = pGroup.get();
        }

        // Don't delete the group if any threads were created
        pGroup.release();
    }

    return threadIds;
}

//------------------------------------------------------------------------------
void Tasker::StartThreadGroup(void * pThreadGroup)
{
    ModsThreadGroup *pGroup = static_cast<ModsThreadGroup *>(pThreadGroup);
    MASSERT(pGroup->m_State == ModsThreadGroup::stateInit);
    // Release all threads in the group
    pGroup->m_State = ModsThreadGroup::stateRunning;
    Yield();
}

//------------------------------------------------------------------------------
void Tasker::DestroyThreadGroup(void * pThreadGroup)
{
    ModsThreadGroup *pGroup = static_cast<ModsThreadGroup *>(pThreadGroup);

    // Cannot destroy a thread group in anything but the init state (once it is
    // running, it is owned by tasker
    MASSERT(pGroup->m_State == ModsThreadGroup::stateInit);

    // Destroy all threads in the group
    pGroup->m_State = ModsThreadGroup::stateDestroy;
    Yield();

    Join(pGroup->m_ThreadIds, NO_TIMEOUT);

    delete pGroup;
}

//------------------------------------------------------------------------------
UINT32 Tasker::GetThreadIdx()
{
    return s_pPriv->m_pLwrrentThread->GThreadIdx;
}

//------------------------------------------------------------------------------
UINT32 Tasker::GetGroupSize()
{
    ModsThreadGroup* const pGroup = s_pPriv->m_pLwrrentThread->pGroup;
    return pGroup ? pGroup->m_Size : 1;
}

//------------------------------------------------------------------------------
UINT32 Tasker::GetNumThreadsAliveInGroup()
{
    ModsThreadGroup* const pGroup = s_pPriv->m_pLwrrentThread->pGroup;
    return pGroup ? pGroup->m_NumThreadsAlive : 1;
}

//------------------------------------------------------------------------------
UINT32 Tasker::GetBarrierReleaseCount()
{
    ModsThreadGroup* const pGroup = s_pPriv->m_pLwrrentThread->pGroup;
    return pGroup ? pGroup->m_BarrierReleaseCount : 0;
}

//------------------------------------------------------------------------------
void Tasker::WaitOnBarrier()
{
    // Get thread's group
    ModsThreadGroup* const pGroup = s_pPriv->m_pLwrrentThread->pGroup;
    if (!pGroup)
    {
        return;
    }

    bool lastThread = false;

    {
        // Lock the group mutex.
        // This prevents released threads from reentering WaitOnBarrier().
        MutexHolder lock(pGroup->m_Mutex);

        // Check group state
        if (pGroup->m_State != ModsThreadGroup::stateRunning)
        {
            return;
        }

        // Increment number of waiting threads
        ++pGroup->m_NumThreadsWaiting;

        // Determine if this is the last thread
        lastThread = pGroup->m_NumThreadsWaiting == pGroup->m_NumThreadsAlive;

        // If it's the last thread, release the other threads
        if (lastThread)
        {
            --pGroup->m_NumThreadsWaiting; // Don't count this (the last) thread
            pGroup->Release();
        }
    }

    // If it's not the last thread, wait on the semaphore for other threads
    if (!lastThread)
    {
        if (OK != Tasker::AcquireSemaphore(pGroup->m_Sema, NO_TIMEOUT))
        {
            MASSERT(!"AcquireSemaphore failed in barrier");
        }
    }
}

//------------------------------------------------------------------------------
//! Set CPU affinity for the specified thread ID to the CPUs specified
//! by the provided mask
RC Tasker::SetCpuAffinity(ThreadID thread, const vector<UINT32> &cpuMasks)
{
    // Return OK (no way to set affinity)
    return OK;
}

//------------------------------------------------------------------------------
RC Tasker::Yield()
{
    if (!IsInitialized())
    {
        return RC::SOFTWARE_ERROR;
    }

    if (!Tasker::CanThreadYield())
    {
        MASSERT(!"Tasker::Yield() is not allowed at this point since CanThreadYield is set to False");
        return RC::SOFTWARE_ERROR;
    }

    // Leave this thread in the ready list.

    // Call the tasker to run the next ready thread.
    s_pPriv->OsNextThread();

    return OK;
}

//------------------------------------------------------------------------------
void Tasker::ExitThread()
{
    MASSERT(s_pPriv->m_pLwrrentThread->Id != 0);

    // Signal anyone waiting for this thread to die.
    SetEvent( s_pPriv->m_pLwrrentThread->DeathEventId );

    // Mark the thread dead, run the next ready thread.
    s_pPriv->m_pLwrrentThread->Dead = true;
    s_pPriv->StupefyThread();
    s_pPriv->OsNextThread();

    // The thread is now a zombie, i.e. it will never be scheduled again by
    // OsNextThread, but it is still taking up space.
    //
    // When someone calls Join on this thread's ID, Join will call
    // OsCleanupDeadThread to free the memory.
    //
    // Otherwise, TaskerPriv::Cleanup will call OsCleanupDeadThread at exit.
}

//------------------------------------------------------------------------------
RC Tasker::Join
(
    Tasker::ThreadID thread,
    FLOAT64     timeoutMs
)
{
    MASSERT(IsInitialized());

    if (! IsValidThreadId(thread) )
        return RC::SOFTWARE_ERROR;

    ModsThread * pThread = s_pPriv->m_Threads[thread];

    // Save off this information since there is a chance our thread
    // memory can be overwritten while we are waiting on the death event
    UINT32 origInstance = pThread->Instance;

    RC rc = WaitOnEvent (pThread->DeathEventId, timeoutMs);
    if (OK == rc)
    {
        // There is a chance threads where switched on us and our thread ID
        // actually points to another valid thread.
        // Check for this so we don't cleanup a brand new thread that wants to live!
        ModsThread * pNewThread = s_pPriv->m_Threads[thread];

        if ((pNewThread) &&
            (pNewThread->Instance == origInstance)) // Same thread, ok to clean up
        {
            // If we got here due to a valid (same) thread, clean it up
            if (IsValidThreadId(pNewThread->Id))
            {
                if (pNewThread->pJsContext)
                {
                    JS_DestroyContext(pNewThread->pJsContext);
                    pNewThread->pJsContext = NULL;
                }
                s_pPriv->OsCleanupDeadThread (pNewThread);
            }
        }
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Tasker::Join(const vector<ThreadID>& threads, FLOAT64 timeoutMs)
{
    StickyRC rc;
    for (size_t i=0; i < threads.size(); i++)
    {
        rc = Join(threads[i], timeoutMs);
    }
    return rc;
}

//------------------------------------------------------------------------------
RC Tasker::WaitOnEvent
(
    ModsEvent * pEvent,
    FLOAT64     timeoutMs
)
{
    RC rc;
    MASSERT(IsInitialized());
    if (!Tasker::CanThreadYield())
    {
        MASSERT(!"Tasker::WaitOnEvent() is not allowed when you can't yield thread.");
        return RC::SOFTWARE_ERROR;
    }

    if (!pEvent && (timeoutMs == NO_TIMEOUT))
        return Yield();

    ThreadID thread = s_pPriv->m_pLwrrentThread->Id;

    if (pEvent)
    {
        if (! pEvent->AddListener(thread))
        {
            // Event is already set; don't wait.
            return OK;
        }
    }

    UINT32 TimeoutIndex;
    CHECK_RC(AddTimeout(timeoutMs, __FUNCTION__, 0, &TimeoutIndex));

    // Remove the current thread from the ready list, mark it blocked.
    s_pPriv->StupefyThread();

    // Call the scheduler, let other threads run.
    s_pPriv->OsNextThread();

    // Now we are awake again, we may have some cleanup to do.
    if (pEvent)
    {
        if (s_pPriv->m_pLwrrentThread->TimedOut)
           pEvent->RemoveListener(thread);
    }
    // else, no event, so we must have timed out, timeout was cleaned up already.

    if (RemoveTimeout(TimeoutIndex))
        rc = RC::TIMEOUT_ERROR;

    // else
    return rc;
}

//------------------------------------------------------------------------------
RC Tasker::WaitOnMultipleEvents
(
    ModsEvent** pEvents,
    UINT32      numEvents,
    UINT32*     pCompletedIndices,
    UINT32      maxCompleted,
    UINT32*     pNumCompleted,
    FLOAT64     timeoutMs
)
{
    MASSERT(pNumCompleted);

    if (numEvents == 0)
    {
        *pNumCompleted = 0;
        return RC::OK;
    }

    MASSERT(maxCompleted > 0);
    MASSERT(pCompletedIndices);

    const UINT64 endTimeMs = (timeoutMs == NO_TIMEOUT)
            ? 0 : Platform::GetTimeMS() + static_cast<UINT64>(timeoutMs);

    for (;;)
    {
        UINT32 numCompleted = 0;
        for (UINT32 i = 0; i < numEvents && numCompleted < maxCompleted; i++)
        {
            if (pEvents[i]->IsSet())
            {
                ResetEvent(pEvents[i]);
                *pCompletedIndices = i;
                ++pCompletedIndices;
                ++numCompleted;
            }
        }

        if (numCompleted)
        {
            *pNumCompleted = numCompleted;
            return RC::OK;
        }

        const UINT64 lwrTimeMs = Platform::GetTimeMS();
        if (lwrTimeMs >= endTimeMs)
        {
            return RC::TIMEOUT_ERROR;
        }

        Yield();
    }

    return RC::OK;
}

//------------------------------------------------------------------------------
FLOAT64 Tasker :: UpdateHwPollFreq :: s_SlowestPollHz = 0.0;
INT32 Tasker :: UpdateHwPollFreq :: s_NumTests = 0;
FLOAT64 Tasker :: UpdateHwPollFreq :: s_PretestPollHz = 0.0;

void Tasker::UpdateHwPollFreq :: Apply(FLOAT64 PollHz)
{

    if (!(s_SlowestPollHz)
            ||
            (PollHz < s_SlowestPollHz))
    {
        s_SlowestPollHz = PollHz;
        if(s_NumTests == 0)
            s_PretestPollHz = Tasker::GetMaxHwPollHz();
        Tasker::SetMaxHwPollHz(s_SlowestPollHz);
    }
    s_NumTests++;
}

Tasker :: UpdateHwPollFreq :: ~UpdateHwPollFreq()
{

    if (s_NumTests > 0)
    {
        //If only values are overridden
        s_NumTests--;
        if(s_NumTests == 0)
        {
            //All the conlwrrent threads have finished run so now we can
            //restore the hw poll hz.
            Tasker::SetMaxHwPollHz(s_PretestPollHz);
        }
    }
}

//------------------------------------------------------------------------------
void Tasker::SetMaxHwPollHz(FLOAT64 Hz)
{
    if (Hz <= 0.0)
    {
        s_HwPollSleepNsec = 0;
    }
    else
    {
        double sleepNsec = 1e9 / Hz;
        s_HwPollSleepNsec = (UINT64)ceil(min(sleepNsec, 1e15));
    }
}

FLOAT64 Tasker::GetMaxHwPollHz()
{
    UINT64 HwPollSleepNsec = GetHwPollSleepNsec();

    if (HwPollSleepNsec == 0)
        return 0.0F;

    return (float)(1e9 / HwPollSleepNsec);
}

//------------------------------------------------------------------------------
//! Local function, common implementation of Poll and PollHw.
RC Tasker::PollCommon
(
    PollFunc        pollFunc,
    volatile void * pArgs,
    FLOAT64         timeoutMs,
    const char *    callingFunctionName,
    const char *    pollingFunctionName,
    bool            ObeyHwPollFreqLimit
)
{
    RC rc;

    MASSERT(IsInitialized());
    MASSERT(pollFunc);

    if (callingFunctionName && pollingFunctionName)
    {
        Printf(Tee::PriDebug, "%s polls on %s.\n",
                callingFunctionName, pollingFunctionName);
    }

    if (Platform::GetLwrrentIrql() == Platform::NormalIrql)
    {
        // We're not inside an ISR.  We can use our standard poll implementation
        // that yields to other threads and that uses the standard MODS timeout
        // mechanism.

        UINT32 TimeoutIndex;
        CHECK_RC(AddTimeout(timeoutMs,
                            !callingFunctionName ? __FUNCTION__ : callingFunctionName,
                            pollingFunctionName,
                            &TimeoutIndex));

        // Make sure to poll once before yielding, so that if the event we're waiting for
        // has already oclwred, we don't yield.
        while (!Platform::PollfuncWrap(pollFunc, const_cast<void*>(pArgs), pollingFunctionName))
        {
            if (s_pPriv->m_pLwrrentThread->TimeOuts[TimeoutIndex].TimedOut)
            {
                rc = RC::TIMEOUT_ERROR;
                break;
            }

            if (ObeyHwPollFreqLimit && GetHwPollSleepNsec())
            {
                PollHwSleep();
            }
            else
            {
                Yield();
            }
        }

        // If we return from the yield and the thread has timed out but the
        // poll function condition has been satisfied, ensure that OK is
        // returned (i.e. when both conditions occur simultaneously, give
        // priority to the successful polling condition being met)
        if (RemoveTimeout(TimeoutIndex) && rc == RC::TIMEOUT_ERROR &&
            pollFunc(const_cast<void*>(pArgs)))
        {
            rc = OK;
        }

        if (rc == OK)
        {
            rc = Abort::CheckAndReset();
        }
    }
    else
    {

        // We're inside an ISR.  We can't yield, so we need to do a plain old
        // spin loop.
        static stack<UINT64> s_EndStack;
        UINT64 End = Xp::GetWallTimeUS() + (UINT64)(timeoutMs * 1e3);
        if (timeoutMs < 0.0)
            End = 0xFFFFFFFFFFFFFFFFull; // no timeout

        if (s_EndStack.size() &&
            (s_EndStack.top() != 0xFFFFFFFFFFFFFFFFull) &&
            (End >= s_EndStack.top()))
        {
            End = s_EndStack.top() - 1;

            UINT64 actualTimeout = End - Xp::GetWallTimeUS();
            if (callingFunctionName && pollingFunctionName)
            {
                Printf(Tee::PriDebug,
                       "%s polling on %s, requested timeout %fus, actual "
                       "timeout %lldus (changed due to nested timeouts).\n",
                       callingFunctionName, pollingFunctionName,
                       timeoutMs * 1e3, actualTimeout);
            }
            else
            {
                Printf(Tee::PriDebug,
                       "Tasker::PollCommon requested timeout %fus, "
                       "actual timeout %lldus (changed due to nested "
                       "timeouts).\n",
                       timeoutMs * 1e3, actualTimeout);
            }
        }
        s_EndStack.push(End);

        while ((rc == OK) && !pollFunc(const_cast<void*>(pArgs)))
        {
            if (Xp::GetWallTimeUS() > s_EndStack.top())
            {
                rc = RC::TIMEOUT_ERROR;
                continue;
            }

            // No Tasker::Yield in case of raised IRQL
            UINT32 pollSleepNs = static_cast<UINT32>(GetHwPollSleepNsec());
            if (pollSleepNs)
            {
                Platform::DelayNS(pollSleepNs);
            }
            else
            {
                Platform::Pause();
            }
        }

        s_EndStack.pop();
    }

    if (callingFunctionName && pollingFunctionName)
    {
        Printf(Tee::PriDebug, "%s polled on %s and returned %d.\n",
                callingFunctionName, pollingFunctionName, UINT32(rc));
    }

    return rc;
}

//------------------------------------------------------------------------------
//! Get the amount of time to wait during PollHw.  Controlled by
//! SetMaxPollHz().
//!
UINT64 Tasker::GetHwPollSleepNsec()
{
    if (s_HwPollSleepNsec == DEFAULT_HW_POLL_SLEEP_NSEC)
    {
        // Return a default value if SetMaxPollHz wasn't called.
        switch (Platform::GetSimulationMode())
        {
            case Platform::Hardware:
                return 0;
            case Platform::RTL:
            case Platform::Fmodel:
            case Platform::Amodel:
                return 10;
            default:
                MASSERT(!"Illegal case in Tasker::GetHwPollSleepNsec");
                return 0;
        }
    }
    return s_HwPollSleepNsec;
}

//------------------------------------------------------------------------------
// Add a timeout on the current thread.  Timeouts may be nested.  With nested
// timeouts, ensure that the "inner" timeout will time out before the "outer"
// timeout.  If the "outer" timeout has already expired when trying to add a
// new inner timeout, then the inner timeout immediately times out.
RC Tasker::AddTimeout(FLOAT64 timeoutMs,
                      const char * callingFunctionName,
                      const char * pollingFunctionName,
                      UINT32     * pTimeoutIndex)
{
    MASSERT(callingFunctionName);

    ThreadID thread = s_pPriv->m_pLwrrentThread->Id;
    FLOAT64 actualTimeoutMs = timeoutMs;

    if (s_pPriv->m_pLwrrentThread->TimeOuts.size())
    {
         // This AddTimeout was called from within an Poll function which has
         // already timed out, no need to do anything here other than return
         // timeout (since the event was already checked above)
         if (s_pPriv->m_pLwrrentThread->TimeOuts.back().TimedOut)
            return RC::TIMEOUT_ERROR;

         // Do not allow an infinite timeout inside a valid timeout
         if ((timeoutMs < 0.0) &&
             (s_pPriv->m_pLwrrentThread->TimeOuts.back().TimeoutMs >= 0.0))
         {
             actualTimeoutMs = s_pPriv->m_pLwrrentThread->TimeOuts.back().TimeoutMs;

             Printf(Tee::PriDebug,
                    "%s polling on %s, NO_TIMEOUT nested inside a valid "
                    "timeout.  Timeout changed to %.3fms\n",
                    callingFunctionName, pollingFunctionName,
                    actualTimeoutMs);
         }
    }

    ModsThreadTimeout threadTimeout = {actualTimeoutMs, false};
    *pTimeoutIndex = (UINT32)s_pPriv->m_pLwrrentThread->TimeOuts.size();

    s_pPriv->m_pLwrrentThread->TimeOuts.push_back(threadTimeout);

    if (actualTimeoutMs >= 0.0)
    {
        ModsTimeout::Add(thread, *pTimeoutIndex);

        actualTimeoutMs =
            s_pPriv->m_pLwrrentThread->TimeOuts[*pTimeoutIndex].TimeoutMs;
        if (timeoutMs != actualTimeoutMs)
        {
            if (callingFunctionName && pollingFunctionName)
            {
                Printf(Tee::PriDebug,
                       "%s polling on %s, requested timeout %.3fms, actual "
                       "timeout %.3fms (changed due to nested timeouts).\n",
                       callingFunctionName, pollingFunctionName,
                       timeoutMs, actualTimeoutMs);
            }
            else
            {
                Printf(Tee::PriDebug,
                       "%s requested timeout %.3fms, actual timeout %.3fms "
                       "(changed due to nested timeouts).\n",
                       callingFunctionName, timeoutMs, actualTimeoutMs);
            }
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
// Remove a timeout from the current thread.  Timeouts must be removed in LIFO
// order
bool Tasker::RemoveTimeout(UINT32 timeoutIndex)
{
    MASSERT(s_pPriv->m_pLwrrentThread->TimeOuts.size());
    if (timeoutIndex != (s_pPriv->m_pLwrrentThread->TimeOuts.size() - 1))
    {
        Printf(Tee::PriDebug,
               "Tasker::RemoveTimeout : Timeout removed out of order\n");
        MASSERT(0);
    }

    bool bTimedOut = s_pPriv->m_pLwrrentThread->TimeOuts.back().TimedOut;
    if (!bTimedOut)
    {
        ModsTimeout::Remove (s_pPriv->m_pLwrrentThread->Id);
    }
    s_pPriv->m_pLwrrentThread->TimeOuts.pop_back();

    return bTimedOut;
}

//--------------------------------------------------------------------
//! \brief Poll on function until it returns true or times out.
//!
//! Poll on Function() until it returns true or times out.  The
//! current thread will yield during polling.
//!
//! \param Function The function to poll on
//! \param pArgs Argument to pass to Function
//! \param TimeoutMs The timeout, in seconds.  The timeout does not
//!     automatically become NO_TIMEOUT in simulation; use
//!     FixTimeout() or POLLWRAP() if you want that behavior.
//! \params callingFunctionName Used for debug spew, may be NULL.
//! \params pollingFunctionName Used for debug spew, may be NULL.
//! \return OK on success, or RC::TIMEOUT on timeout.
//!
RC Tasker::Poll
(
    PollFunc        Function,
    volatile void * pArgs,
    FLOAT64         TimeoutMs,
    const char *    callingFunctionName,
    const char *    pollingFunctionName
)
{
    return PollCommon (Function, pArgs, TimeoutMs,
            callingFunctionName, pollingFunctionName, false);
}

//--------------------------------------------------------------------
//! \brief Poll on function, staying below max HW polling frequency
//!
//! This method works like Tasker::Poll(), except that it inserts a
//! delay in the polling loop so that Function is called below the max
//! polling frequency from -poll_hw_hz.  This avoids some hardware
//! issues.
//!
//! \sa Tasker::Poll()
//!
RC Tasker::PollHw
(
    PollFunc        Function,
    volatile void * pArgs,
    FLOAT64         TimeoutMs,
    const char *    callingFunctionName,
    const char *    pollingFunctionName
)
{
    return PollCommon (Function, pArgs, TimeoutMs,
            callingFunctionName, pollingFunctionName, true);
}

//------------------------------------------------------------------------------
RC Tasker::SetDefaultTimeoutMs(FLOAT64 TimeoutMs)
{
    s_DefaultTimeoutMs = TimeoutMs;
    s_SimTimeoutMs = TimeoutMs;
    return OK;
}

//------------------------------------------------------------------------------
FLOAT64 Tasker::GetDefaultTimeoutMs()
{
    return (Platform::GetSimulationMode() == Platform::Hardware ?
            s_DefaultTimeoutMs : s_SimTimeoutMs);
}

//------------------------------------------------------------------------------
// Nothing to do since affinity is not supported with this tasker implementation
RC Tasker::SetIgnoreCpuAffinityFailure(bool bIgnoreFailure) { return OK; }
bool Tasker::GetIgnoreCpuAffinityFailure() { return true; }

//------------------------------------------------------------------------------
// On simulation, we usually shouldn't bother with timeouts.  LSF jobs
// that take too long will time out.  This function turns any nonzero
// timeout into a long (usually infinite) timeout in simulation.
//
FLOAT64 Tasker::FixTimeout(FLOAT64 TimeoutMs)
{
    if ((TimeoutMs == 0) ||
        (Platform::GetSimulationMode() == Platform::Hardware))
    {
        return TimeoutMs;
    }
    else
    {
        return s_SimTimeoutMs;
    }
}

//------------------------------------------------------------------------------
void Tasker::Sleep(FLOAT64 Milliseconds)
{
    // Compute the time as of when the sleep should complete
    UINT64 End = Xp::QueryPerformanceCounter() +
        (UINT64)(Milliseconds * Xp::QueryPerformanceFrequency() / 1000);

    // Go to sleep for an interval that we think is safe, based on the
    // resolution of the OS timer

    if (Milliseconds > ModsTimeout::s_SafeTimeoutMs)
    {
        WaitOnEvent(NULL, Milliseconds - ModsTimeout::s_SafeTimeoutMs);
    }

    // Use a more lightweight yield to finish off the sleep operation
    do
    {
        Yield();
    } while (Xp::QueryPerformanceCounter() < End);
}

//------------------------------------------------------------------------------
void Tasker::PollHwSleep()
{
    UINT64 HwPollSleepNsec = GetHwPollSleepNsec();
    if (HwPollSleepNsec)
    {
        UINT64 tmp = Platform::GetTimeNS() + GetHwPollSleepNsec();
        do
        {
            Yield();
        }
        while (tmp > Platform::GetTimeNS());
    }
}

//------------------------------------------------------------------------------
void Tasker::PollMemSleep()
{
    // Limit how fast we poll on BAR1 addresses to avoid saturating the pex bus.
    // Sims where mods back-door accesses FB should not pause.

    if (s_HwPollSleepNsec == DEFAULT_HW_POLL_SLEEP_NSEC)
    {
        switch (Platform::GetSimulationMode())
        {
            case Platform::RTL:
            case Platform::Fmodel:
            case Platform::Amodel:
                return;
            default:
                break;
        }
    }
    PollHwSleep();
}

//------------------------------------------------------------------------------
Tasker::EventID Tasker::AllocEvent(const char *name, bool ManualReset /*default = true*/)
{
    return ModsEvent::Alloc(name, ManualReset);
}

Tasker::EventID Tasker::AllocSystemEvent(const char *name)
{
    return ModsEvent::Alloc(name, false);
}

void Tasker::FreeEvent(ModsEvent* pEvent)
{
    ModsEvent::Free(pEvent);
}

void Tasker::ResetEvent(ModsEvent* pEvent)
{
    pEvent->Reset();
}

void Tasker::SetEvent(ModsEvent* pEvent)
{
    pEvent->Set();
}

void *Tasker::GetOsEvent(ModsEvent* pEvent, UINT32 hClient, UINT32 hDevice)
{
    return pEvent->GetOsEvent(hClient, hDevice);
}

//------------------------------------------------------------------------------
void * Tasker::AllocMutex(const char * /*unused*/, unsigned /*unused*/)
{
    // When created, the mutex is not owned by anyone.
    ModsMutex *Mutex = new ModsMutex;
    Mutex->Owner = 0;
    Mutex->LockCount = 0;
    return Mutex;
}

//------------------------------------------------------------------------------
void Tasker::FreeMutex(void *MutexHandle)
{
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;

    // A mutex should not be freed when it is lwrrently owned by someone
    MASSERT(!Mutex->LockCount);

    delete Mutex;
}

//------------------------------------------------------------------------------
bool Tasker::CheckMutex(void *MutexHandle)
{
    ModsMutex *Mutex = (ModsMutex *)MutexHandle;
    return (Mutex->LockCount>0);
}

//--------------------------------------------------------------------
//! Polling function used by PollAndAcquireMutex &
//! PollHwAndAcquireMutex
//!
bool Tasker::PollAndAcquireMutexPollFunc(void *pArgs)
{
    PollAndAcquireMutexPollArgs *pPollArgs =
        (PollAndAcquireMutexPollArgs*)pArgs;

    if (TryAcquireMutex(pPollArgs->MutexHandle))
    {
        if (pPollArgs->pollFunc(const_cast<void*>(pPollArgs->pArgs)))
        {
            return true;
        }
        ReleaseMutex(pPollArgs->MutexHandle);
    }
    return false;
}

//--------------------------------------------------------------------
//! \brief Lock a mutex when a polling function returns true
//!
//! This method waits until a mutex is available and a polling
//! function becomes true at the same time, and acquires the mutex.
//! If the polling function times out, return TIMEOUT_ERROR and do not
//! acquire the mutex.
//!
//! This is basically the simplest form of a condition variable.  It's
//! roughly equivalent to running POLLWRAP() and AcquireMutex() at the
//! same time.
//!
RC Tasker::PollAndAcquireMutex
(
    Tasker::PollFunc pollFunc,
    volatile void *pArgs,
    void *MutexHandle,
    FLOAT64 TimeoutMs
)
{
    PollAndAcquireMutexPollArgs PollArgs;
    PollArgs.pollFunc = pollFunc;
    PollArgs.pArgs = pArgs;
    PollArgs.MutexHandle = MutexHandle;

    return Poll(PollAndAcquireMutexPollFunc, &PollArgs, TimeoutMs);
}

//--------------------------------------------------------------------
//! \brief Same as PollAndAcquireMutex(), but polls at MaxHwPollHz.
//!
RC Tasker::PollHwAndAcquireMutex
(
    Tasker::PollFunc pollFunc,
    volatile void *pArgs,
    void *MutexHandle,
    FLOAT64 TimeoutMs
)
{
    PollAndAcquireMutexPollArgs PollArgs;
    PollArgs.pollFunc = pollFunc;
    PollArgs.pArgs = pArgs;
    PollArgs.MutexHandle = MutexHandle;

    return PollHw(PollAndAcquireMutexPollFunc, &PollArgs, TimeoutMs, 0, 0);
}

//------------------------------------------------------------------------------
Tasker::SpinLock::SpinLock(volatile UINT32 *pSpinLock) : m_pSpinLock(pSpinLock)
{
    MASSERT(m_pSpinLock);
    if (Tasker::IsInitialized())
    {
        while (!Cpu::AtomicCmpXchg32(m_pSpinLock, 0, 1))
        {
            Tasker::Yield();
        }
    }
    else
    {
        if (0 != Cpu::AtomicXchg32(m_pSpinLock, 1))
        {
            printf("Relwrsive spinlock!\n");
            Xp::BreakPoint();
        }
    }
}

//------------------------------------------------------------------------------
Tasker::SpinLock::~SpinLock()
{
    Cpu::AtomicXchg32(m_pSpinLock, 0);
}

//------------------------------------------------------------------------------
SemaID Tasker::CreateSemaphore (INT32 initialCount, const char* name /*default name =""*/)
{
    if(initialCount < 0)
    {
        MASSERT(!"Invalid init count\n");
        return 0;
    }
    MASSERT(name);

    SemaID sema4 = new ModsSemaphore;
    if(!sema4)
    {
        MASSERT(!"Cannot alloc new semaphore\n");
        return 0;
    }
    sema4->Sema4Event = Tasker::AllocEvent(name, false);
    if(!sema4->Sema4Event)
    {
        MASSERT(!"Cannot alloc new event\n");
        return 0;
    }

    sema4->Name = new char [strlen(name) + 1];
    MASSERT(sema4->Name);
    strcpy(sema4->Name, name);

    sema4->Count = initialCount;
    sema4->IsZombie = false;
    sema4->SleepingThreadCount = 0;

    return sema4;
}
//------------------------------------------------------------------------------
RC Tasker::AcquireSemaphore(SemaID semaphore, FLOAT64 timeoutMs )
{
    MASSERT(CanThreadYield());
    MASSERT(semaphore);

    // Keep yielding until there is a node available
    while (semaphore->Count == 0)
    {
        semaphore->SleepingThreadCount++;
        RC rc = WaitOnEvent(semaphore->Sema4Event, timeoutMs);
        semaphore->SleepingThreadCount--;
        if(rc == RC::TIMEOUT_ERROR)
            return rc;
        if(semaphore->IsZombie)
        {
            Yield();
            return RC::SOFTWARE_ERROR;
        }
    }
    MASSERT(semaphore->Count > 0);
    semaphore->Count--;
    return OK;
}
//------------------------------------------------------------------------------
void Tasker::ReleaseSemaphore(SemaID semaphore)
{
    MASSERT(semaphore);
    MASSERT(semaphore->Count < 0x7FFFFFFF);
    semaphore->Count++;
    if (semaphore->SleepingThreadCount > 0)
    {
        SetEvent(semaphore->Sema4Event);

        // Yield after signaling the event.  If a semaphore is released multiple
        // times in succession without yielding in between then only one other
        // thread that is waiting will be woken up since events are auto-resetting
        // and are not reset until a different thread that was waiting on the
        // event starts running.  This can happen (for instance) if a single
        // thread is using multiple semaphore protected resources and is done
        // with them all at the same time and releases them in a tight loop.
        Yield();
    }
}
//------------------------------------------------------------------------------
void Tasker::DestroySemaphore(SemaID semaphore)
{
    MASSERT(semaphore);
    semaphore->IsZombie = true;
    //wake up all the sleeping threads ; they will return error sinze IsZombie is true
    while(semaphore->SleepingThreadCount)
    {
        SetEvent(semaphore->Sema4Event);
        Yield();
    }

    FreeEvent(semaphore->Sema4Event);
    delete [] semaphore->Name;
    delete semaphore;
}
//------------------------------------------------------------------------------
const char * Tasker::GetSemaphoreName(SemaID semaphore)
{
    return semaphore->Name;
}

//------------------------------------------------------------------------------
void *Tasker::AllocRmSpinLock()
{
    Tasker::RmSpinLock *lock = new Tasker::RmSpinLock;
    if (!lock)
    {
        MASSERT(!"Could not alloc new SpinLock!\n");
    }
    lock->lock = 0;
    return lock;
}
//------------------------------------------------------------------------------
void Tasker::AcquireRmSpinLock(SpinLockID lock)
{
    MASSERT(lock);
    MASSERT(Tasker::IsInitialized());

    while (!Cpu::AtomicCmpXchg32(&lock->lock, 0, 1))
    {
        Tasker::Yield();
    }
}
//------------------------------------------------------------------------------
void Tasker::ReleaseRmSpinLock(SpinLockID lock)
{
    MASSERT(lock);
    MASSERT(Tasker::IsInitialized());

    Cpu::AtomicXchg32(&lock->lock, 0);
}
//------------------------------------------------------------------------------
void Tasker::DestroyRmSpinLock(SpinLockID lock)
{
    MASSERT(lock);
    delete lock;
}

//------------------------------------------------------------------------------
bool Tasker::IsInitialized()
{
    return (s_pPriv != 0);
}

//------------------------------------------------------------------------------
UINT32 Tasker::Threads()
{
    MASSERT(IsInitialized());

    int num = 0;

    vector<ModsThread*>::const_iterator iter = s_pPriv->m_Threads.begin();
    for(/**/; iter != s_pPriv->m_Threads.end(); ++iter)
    {
        if (*iter)
            num++;
    }
    return num;
}
//------------------------------------------------------------------------------
void * Tasker::OsLwrrentThread()
{
    MASSERT(IsInitialized());
    return s_pPriv->m_pLwrrentThread;
}

//------------------------------------------------------------------------------
JSContext * Tasker::GetLwrrentJsContext()
{
    MASSERT(IsInitialized());
    Tasker::ThreadID tid = LwrrentThread();

    if (IsValidThreadId(tid))
        return s_pPriv->m_Threads[tid]->pJsContext;

    return NULL;
}

//------------------------------------------------------------------------------
JSContext * Tasker::GetJsContext(Tasker::ThreadID threadId)
{
    MASSERT(IsInitialized());

    if (IsValidThreadId(threadId))
        return s_pPriv->m_Threads[threadId]->pJsContext;

    return nullptr;
}

//------------------------------------------------------------------------------
void Tasker::SetLwrrentJsContext(JSContext *pContext)
{
    MASSERT(IsInitialized());
    Tasker::ThreadID tid = LwrrentThread();
    if (IsValidThreadId(tid))
    {
        if (s_pPriv->m_Threads[tid]->pJsContext)
        {
            JS_DestroyContext(s_pPriv->m_Threads[tid]->pJsContext);
        }
        s_pPriv->m_Threads[tid]->pJsContext = pContext;
    }
}

void Tasker::DestroyJsContexts()
{
    s_pPriv->DestroyJsContexts();
}

//------------------------------------------------------------------------------
const char * Tasker::ThreadName(ThreadID thread)
{
    MASSERT(IsInitialized());
    if (IsValidThreadId(thread))
        return s_pPriv->m_Threads[thread]->Name;
    // else
    return "ilwalid_thread_id!";
}
//------------------------------------------------------------------------------
Tasker::ThreadID Tasker::LwrrentThread()
{
    if (!IsInitialized())
    {
        // Allow MASSERT to run before/after tasker is started/stopped.
        return 0;
    }

    Platform::Irql OldIrql = Platform::RaiseIrql(Platform::ElevatedIrql);
    Tasker::ThreadID retID = s_pPriv->m_pLwrrentThread->Id;
    // assuming that SetCanThreadYield() is called from Lame OSes
    if (!CanThreadYield())
    {
        retID = ISR_THREADID;
    }
    Platform::LowerIrql(OldIrql);
    return retID;
}
//------------------------------------------------------------------------------
void Tasker::PrintAllThreads(INT32 Priority /*=Tee::PriNormal*/)
{
    Printf(Priority, "Threads:\n");
    Tasker::ThreadID i;
    for (i = 0; i < (Tasker::ThreadID) s_pPriv->m_Threads.size(); i++)
    {
       ModsThread * p = s_pPriv->m_Threads[i];
       if (p)
       {
           Printf(Priority, "   %ld \"%s\"", (long)i, p->Name);

           if (p == s_pPriv->m_pLwrrentThread)
               Printf(Priority, " running");
           else if (p->Dead)
               Printf(Priority, " dead");
           else if (p->Awake)
               Printf(Priority, " ready");
           else
           {
               Printf(Priority, " waiting for:");

               ModsEvent::PrintEventsListenedFor(i, Priority);

               if (ModsTimeout::HasTimeout(i))
                   Printf(Priority, " timeout");
           }
           Printf(Priority, "\n");
       }
    }
    Printf(Priority, "Events:\n");
    ModsEvent::  PrintAll(Priority);

    Printf(Priority, "Timeouts:\n");
    ModsTimeout::PrintAll(Priority);
}

//------------------------------------------------------------------------------
vector<Tasker::ThreadID> Tasker::GetAllThreads()
{
    MASSERT(IsInitialized());
    vector<ThreadID> threadIds;
    boost::transform(s_pPriv->m_Threads, back_inserter(threadIds), [](auto t) { return t->Id; });
    return threadIds;
}

//------------------------------------------------------------------------------
vector<Tasker::ThreadID> Tasker::GetAllAttachedThreads()
{
    return GetAllThreads();
}

//------------------------------------------------------------------------------
UINT32 Tasker::TlsAlloc(bool bInherit /* = false */)
{
    MASSERT(IsInitialized());

    // Find an allocated index in TlsIndexAllocMask.
    // Each word contains the alloc/free status of 32 Tls indexes.
    // A '1' bit indicates an allocated index.
    UINT32 idx;
    for (idx = 0; idx < TaskerPriv::TLS_NUM_INDEXES; /**/)
    {
        UINT32 freemask = ~(TlsIndexAllocMask[ idx/32 ]);
        if (!freemask)
        {
            // This word in the mask array shows all 32 indexes allocated.
            idx += 32;
            continue;
        }
        else
        {
            // This mask word has at least one free index.
            UINT32 newmask = freemask ^ (freemask-1);
            // Mark the bit for this index as allocated.
            TlsIndexAllocMask[ idx/32 ] |= newmask;

            // Colwert from mask to index number.
            newmask >>= 1;
            while (newmask)
            {
                idx++;
                newmask >>= 1;
            }

            if (bInherit)
                TlsIndexInheritMask[ idx/32 ] |= (1 << (idx % 32));
            return idx;
        }
    }
    // Shouldn't get here!  We're out of space in our fixed-size TLS arrays.
    // If we ever get here, we should redesign this for dynamic sizing...
    MASSERT(!"TlsAlloc failed to allocate.");
    return 0xffffffff;
}

//------------------------------------------------------------------------------
void   Tasker::TlsFree (UINT32 tlsIndex)
{
    MASSERT(IsInitialized());
    // The index should be in range.
    MASSERT(tlsIndex < TaskerPriv::TLS_NUM_INDEXES);

    UINT32 maskIdx = tlsIndex/32;
    UINT32 mask = (1 << (tlsIndex % 32));

    // The index should be allocated.
    MASSERT(TlsIndexAllocMask[maskIdx] & mask);

    // Mark the index free.
    TlsIndexAllocMask[ maskIdx ] &= ~mask;
    TlsIndexInheritMask [ maskIdx ] &= ~mask;
}

//------------------------------------------------------------------------------
void   Tasker::TlsSetValue (UINT32 tlsIndex, void* value)
{
    MASSERT(IsInitialized());
    // The index should be in range.
    MASSERT(tlsIndex < TaskerPriv::TLS_NUM_INDEXES);
    // The index should be allocated.
    MASSERT(TlsIndexAllocMask[tlsIndex/32] & (1 << (tlsIndex%32)));

    ModsThread * pdmt = (ModsThread *)Tasker::OsLwrrentThread();
    pdmt->TlsArray[tlsIndex] = value;
}

//------------------------------------------------------------------------------
void*  Tasker::TlsGetValue (UINT32 tlsIndex)
{
    MASSERT(IsInitialized());
    if(!IsInitialized())
        return NULL;
    // The index should be in range.
    MASSERT(tlsIndex < TaskerPriv::TLS_NUM_INDEXES);
    // The index should be allocated.
    MASSERT(TlsIndexAllocMask[tlsIndex/32] & (1 << (tlsIndex%32)));

    ModsThread * pdmt = (ModsThread *)Tasker::OsLwrrentThread();
    return pdmt->TlsArray[tlsIndex];
}

//------------------------------------------------------------------------------
void Tasker::TlsInheritFromLwrrentThread (void **TlsArray)
{
    UINT32 idx;
    for (idx = 0; idx < TaskerPriv::TLS_NUM_INDEXES; idx++)
    {
        if ((TlsIndexAllocMask[idx/32] & (1 << (idx%32))) &&
            (TlsIndexInheritMask[idx/32] & (1 << (idx%32))))
        {
            TlsArray[idx] = TlsGetValue(idx);
        }
    }
}

//------------------------------------------------------------------------------
bool Tasker::IsValidThreadId (ThreadID id)
{
   if ((id >= 0) &&
       (id < (ThreadID)s_pPriv->m_Threads.size()) &&
       (s_pPriv->m_Threads[id]))
      return true;

   // else
   return false;
}

//------------------------------------------------------------------------------
int Tasker::StackBytesFree()
{
   return s_pPriv->OsStackBytesFree();
}

//------------------------------------------------------------------------------
bool Tasker::IsEventSet (ModsEvent * pEvent)
{
   return pEvent->IsSet();
}

//------------------------------------------------------------------------------
//
// Class TaskerPriv implementation:
//
//   (except for the Os* functions, which are in ostasker.cpp).
//
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
TaskerPriv::TaskerPriv ()
{
   m_ReadyIt = m_Ready.end();

   // Create main "psuedo-thread".
   Tasker::ThreadID mainId = CreateThread (0, 0, 0, "main");

   // Initialize the current thread pointer.
   m_pLwrrentThread = m_Threads[mainId];
}

//------------------------------------------------------------------------------
TaskerPriv::~TaskerPriv ()
{
   // Must only be called from the main thread.
   MASSERT(m_pLwrrentThread->Id == 0);

   // Block all threads (make none of them runnable).
   m_ReadyIt = m_Ready.end();
   m_Ready.clear();

   // Kill and delete all the threads.
   vector<ModsThread *>::iterator iter = m_Threads.begin();
   for (/**/; iter != m_Threads.end(); ++iter)
   {
      if (*iter)
      {
         (*iter)->Awake = false;
         (*iter)->Dead = true;
         if ((*iter)->pJsContext)
         {
             JS_DestroyContext((*iter)->pJsContext);
         }
         OsCleanupDeadThread(*iter);
     }
   }
}

//------------------------------------------------------------------------------
void TaskerPriv::DestroyJsContexts()
{
    vector<ModsThread *>::iterator iter = m_Threads.begin();
    for (/**/; iter != m_Threads.end(); ++iter)
    {
        if ((*iter) && (*iter)->pJsContext)
        {
            JS_DestroyContext((*iter)->pJsContext);
            (*iter)->pJsContext = NULL;
        }
    }
}

//------------------------------------------------------------------------------
Tasker::ThreadID TaskerPriv::CreateThread
(
   Tasker::ThreadFunc func,
   void *             args,
   UINT32             stackSize,
   const char *       name
)
{
   // Assign an id.
   Tasker::ThreadID id;

   for (id = 0; id < (Tasker::ThreadID) m_Threads.size(); id++)
   {
      if (! m_Threads[id])
         break;
   }
   if (id == (Tasker::ThreadID) m_Threads.size())
      m_Threads.push_back(0);

   // Create a new thread (OS specific) and store it.
   m_Threads[id] = OsCreateThread(id, func, args, stackSize, name);
   m_Threads[id]->Instance = s_LwrrentThreadInstance++;
   m_Threads[id]->pJsContext = NULL;

   // Mark a newly created thread as "ready to run".
   WakeThread(id, false/*timedOut*/);

   return id;
}

//------------------------------------------------------------------------------
void TaskerPriv::StupefyThread ()
{
   // ReadyIt should point to current thread.
   MASSERT(m_pLwrrentThread == *m_ReadyIt);

   m_pLwrrentThread->Awake = false;

   // Remove this thread from the Ready list.
   // Leaves ReadyIt pointing to item after deleted item, or at end().
   m_ReadyIt = m_Ready.erase(m_ReadyIt);
}

//------------------------------------------------------------------------------
void TaskerPriv::WakeThread (Tasker::ThreadID id, bool timedOut)
{
   // Thread id must be in range.
   MASSERT((id >= 0) && (id < (Tasker::ThreadID)m_Threads.size()));
   // Thread must be allocated.
   MASSERT(m_Threads[id]);
   // Thread should not be dead.
   MASSERT(!m_Threads[id]->Dead);

   ModsThread * pThread = m_Threads[id];

   pThread->TimedOut = timedOut;

   // Use the Awake flag to avoid double-waking, which would be a disaster.
   if (!pThread->Awake)
   {
      pThread->Awake = true;

      // Insert this thread into the Ready list.
      // Leaves ReadyIt pointing to item after inserted item, or at end().
      // Since we walk the Ready list backwards from end() to begin() this means
      // that this thread will be run next.
      m_Ready.insert(m_ReadyIt, pThread);
   }
}

//------------------------------------------------------------------------------
//
// struct ModsThread implementation:
//
//------------------------------------------------------------------------------

Tasker::ModsThread::ModsThread
   (
      Tasker::ThreadID   id,
      Tasker::ThreadFunc func,
      void *             args,
      const char *       name
   )
:  Id(id),
   Func(func),
   Args(args),
   TimedOut(false),
   Dead(false),
   Awake(false),
   Instance(0U),
   pJsContext(NULL),
   pGroup(NULL),
   GThreadIdx(0)
{
    DeathEventId = Tasker::AllocEvent("death");
    if (!name)
        name = "";
    Name = new char [strlen(name) + 1];
    strcpy(Name, name);

    memset(TlsArray, 0, sizeof(TlsArray));

    Tasker::TlsInheritFromLwrrentThread(TlsArray);
}
Tasker::ModsThread::~ModsThread()
{
    delete [] Name;
    Tasker::FreeEvent(DeathEventId);
}

//------------------------------------------------------------------------------
//
// class ModsEvent implementation:
//
//------------------------------------------------------------------------------

// Class-static data:
vector<ModsEvent *>  ModsEvent::s_Events;
volatile UINT08 *    ModsEvent::s_pPendingSets;
volatile UINT08 *    ModsEvent::s_pSpareSets;
size_t               ModsEvent::s_SetsSize;

//------------------------------------------------------------------------------
//! constructor (called only by the static ModsEvent::Alloc function)
//
ModsEvent::ModsEvent(const char * Name, int Id, bool ManualReset)
{
    if (!Name)
        Name = "";

    m_Name = new char [strlen(Name) + 1];
    strcpy(m_Name, Name);

    m_OsEvent = NULL;

    m_Id = Id;
    m_ManualReset = ManualReset;
    m_IsSet = false;
    m_hOsEventClient = 0U;
    m_hOsEventDevice = 0U;
}

//------------------------------------------------------------------------------
//! destructor (called only by ModsEvent::Free and ModsEvent::Cleanup).
//
ModsEvent::~ModsEvent()
{
    delete[] m_Name;
    if (m_OsEvent != NULL)
    {
        OsFreeEvent(m_hOsEventClient, m_hOsEventDevice);
        m_OsEvent = NULL;
    }
}

//------------------------------------------------------------------------------
//! Initialize the class static data.
//
RC ModsEvent::Initialize ()
{
    s_SetsSize  = 4;

    UINT08 * ptmp;

    ptmp = new UINT08 [s_SetsSize + 1];
    memset(ptmp, 0, s_SetsSize + 1);
    s_pPendingSets = ptmp + 1;

    ptmp = new UINT08 [s_SetsSize + 1];
    memset(ptmp, 0, s_SetsSize + 1);
    s_pSpareSets = ptmp + 1;

    return OK;
}

//------------------------------------------------------------------------------
//! Free class static data.
//
void ModsEvent::Cleanup ()
{
    vector<ModsEvent *>::iterator iter = s_Events.begin();
    for (/**/; iter != s_Events.end(); ++iter)
    {
        if (*iter)
            delete (*iter);
    }
    s_Events.clear();

    delete [] (s_pPendingSets - 1);
    s_pPendingSets = 0;
    delete [] (s_pSpareSets - 1);
    s_pSpareSets = 0;
}

//------------------------------------------------------------------------------
//! Create a new ModsEvent, store it in our private array, return its ID.
//
ModsEvent *ModsEvent::Alloc (const char * name, bool ManualReset)
{
    size_t id;

    // First, assign a slot in s_Events and an EventID.
    for (id = 0; id < s_Events.size(); id++)
    {
        if (! s_Events[id])
            break;   // Found an empty slot.
    }
    if (id >= s_Events.size())
    {
        // No slots free.  We must increase the size of our arrays.
        s_Events.push_back(NULL);

        if (s_SetsSize < s_Events.size())
        {
            // Grow by 50% each time.
            UINT32 newSize = (UINT32)(s_SetsSize * 3) / 2;
            volatile UINT08 * ptmp;

            // Free and alloc the spare array.
            ptmp = s_pSpareSets - 1;
            delete [] ptmp;
            ptmp = new UINT08 [newSize + 1];
            memset(const_cast<UINT08 *>(ptmp), 0, newSize+1);
            s_pSpareSets = ptmp + 1;

            // Swap to the new array.
            ProcessPendingSets();

            // Free and alloc the other array (which is the now the spare).
            ptmp = s_pSpareSets - 1;
            delete [] ptmp;
            ptmp = new UINT08 [newSize + 1];
            memset(const_cast<UINT08 *>(ptmp), 0, newSize+1);
            s_pSpareSets = ptmp + 1;

            s_SetsSize = newSize;
        }
    }

    // Create the new ModsEvent object, store it in the global array.
    ModsEvent *Event = new ModsEvent(name, (int)id, ManualReset);
    s_Events[id] = Event;
    Event->Reset();

    return Event;
}

//------------------------------------------------------------------------------
//! Free an event.
//
void ModsEvent::Free(ModsEvent *Event)
{
    // Return harmlessly if we tried to free a NULL event.  This is consistent
    // with all free functions in MODS (as well as with free and delete in
    // C/C++)
    if(Event == NULL)
        return;

    int id = Event->m_Id;

    // Wake (timeout) any threads that were blocked on this event.
    // This is probably unnecessary, but it might prevent a bug someday,
    // and this is not a performance sensitive path.
    MASSERT(Tasker::IsInitialized());
    TaskerPriv * pPriv = s_pPriv;

    vector<Tasker::ThreadID>::iterator iter = Event->m_Listeners.begin();
    for (/**/; iter != Event->m_Listeners.end(); ++iter)
    {
        if (Tasker::IsValidThreadId(*iter))
        {
            // Printf(Tee::PriHigh, "ModsEvent::Free %d timing out thread %d\n",id, *iter);
            pPriv->WakeThread(*iter, true/*timedOut*/);
        }
    }

    // Wait for all the listeners to disappear. Otherwise they may
    // still try to dereference ModsEvent pointer like in
    // Tasker::WaitOnEvent: Event->RemoveListener(thread).
    // The WakeThread above doesn't immediately resume threads still
    // waiting for the event.
    while (!Event->m_Listeners.empty())
    {
        Tasker::Yield();
    }

    // Delete the ModsEvent.
    delete Event;
    s_Events[id] = NULL;
    s_pPendingSets[id] = 0;
}

//------------------------------------------------------------------------------
//! Set an event.
//! Actually, just set a flag for ProcessPendingSets to deal with later.
//
void ModsEvent::Set()
{
    // This function is often called from ISRs and in elwironments like WinMfgMods
    // exelwtion of ISRs can be conlwrrent to regular Tasker threads calling
    // ProcessPendingSets. It means that the Spare exchange can occur
    // in middle of this function. Solution is to copy the pointer to
    // have consistent writes.
    // The assumption is that ProcessPendingSets is called often and
    // even when the Set() is not picked up in the current s_pPendingSets,
    // it will be shortly after the next Spare exchange.
    volatile UINT08 *ConsistentPendingSets = s_pPendingSets;

    // We should be initialized.
    MASSERT(ConsistentPendingSets);

    // This operation can be called by an ISR.
    ConsistentPendingSets[m_Id] = 1;
    ConsistentPendingSets[-1] = 1; // This should be called last to prevent
                                   // race with ProcessPendingSets checking
                                   // for [-1] first and not finding any
                                   // events set.
}

//------------------------------------------------------------------------------
//! Clear (reset) an event.
//! Since there might be a thread waiting for a pending set that hasn't been
//! processed yet, we must ProcessPendingSets first so that thread doesn't hang.
//
void ModsEvent::Reset()
{
    // We should be initialized.
    MASSERT(s_pPendingSets);

    ProcessPendingSets();
    m_IsSet = false;
}

//------------------------------------------------------------------------------
//! Check if an event is set.
//
bool ModsEvent::IsSet()
{
    return m_IsSet;
}

//------------------------------------------------------------------------------
//! Return the os-event owned by this object.  Use lazy instantiation,
//! because some platforms need to connect to the RM before creating
//! any osEvents, and ModsEvents objects are instantiated before that.
//
void *ModsEvent::GetOsEvent(UINT32 hClient, UINT32 hDevice)
{
    if (m_OsEvent == NULL)
    {
        m_hOsEventClient = hClient;
        m_hOsEventDevice = hDevice;
        OsAllocEvent(hClient, hDevice);
    }
    return m_OsEvent;
}

//------------------------------------------------------------------------------
//! Safely process any pending sets (wakes threads, sets the events).
//! Must be called by a foreground thread, not an ISR.
//
void ModsEvent::ProcessPendingSets ()
{
   MASSERT(s_pPendingSets);
   MASSERT(s_pSpareSets);
   MASSERT(s_pSpareSets != s_pPendingSets);

   // Before we start, first check whether any OS events have fired and set their
   // corresponding MODS events.
   ModsEvent::OsProcessPendingSets();

   // Switch to the spare array (this is atomic, so we don't have to worry
   // about race conditions with ISR code).
   // We assume that the spare array was left clear the last time we ran, to
   // save the time of clearing it this time.
   volatile UINT08 * pProc;
   pProc          = s_pPendingSets;
   s_pPendingSets = s_pSpareSets;
   s_pSpareSets   = pProc;

   // Since the checking for the pending event is a linear search,
   // and the common case is for there to be no pending events,
   // we keep one extra byte at offset -1 from pPendingSetEvents to indicate
   // that some event oclwrred.

   if (pProc[-1])
   {
      pProc[-1] = 0;

      MASSERT(Tasker::IsInitialized());
      TaskerPriv * pPriv = s_pPriv;

      // Process the pending events.
      volatile UINT08 * pEndProc = pProc + s_SetsSize;
      do
      {
         if (*pProc)
         {
            // Clear the pending event.
            *pProc = 0;

            // Set the event, if it is allocated (it darn well should be!)
            ModsEvent * pEvent = s_Events[pProc - s_pSpareSets];
            MASSERT(pEvent);
            pEvent->m_IsSet = true;

            vector<Tasker::ThreadID>::iterator iter = pEvent->m_Listeners.begin();
            if(pEvent->m_ManualReset)
            {
                // Wake all threads in our Listeners list, and clear the list.
                for (/**/; iter != pEvent->m_Listeners.end(); ++iter)
                {
                   //Printf(Tee::PriDebug, "PPendingSets %d waking thread %d\n", pProc - s_pSpareSets, *iter);
                   pPriv->WakeThread(*iter, false/*timedOut*/);
                }
                pEvent->m_Listeners.clear();
            }
            else    //auto reset: wake only 1 thread, and clear 1 listener
            {
                if(!pEvent->m_Listeners.empty())
                {
                    pPriv->WakeThread(*iter, false);
                    pEvent->m_Listeners.erase(iter);
                    pEvent->m_IsSet = false;
                }
            }
         }
      } while (++pProc < pEndProc);
   }
}

//------------------------------------------------------------------------------
//! Add a thread ID to the listeners of an event.
//
bool ModsEvent::AddListener (Tasker::ThreadID thread)
{
    // We should be initialized.
    MASSERT(s_pPendingSets);
    // thread should be a real live thread ID.
    MASSERT(Tasker::IsValidThreadId(thread));

    if (m_IsSet)
    {
        if (!m_ManualReset)
            m_IsSet = false;
        return false;     // This event has already oclwrred.
    }

    // else
    m_Listeners.push_back(thread);
    return true;
}

//------------------------------------------------------------------------------
//! Remove a thread ID to the listeners of an event.
//
void ModsEvent::RemoveListener(Tasker::ThreadID thread)
{
    // We should be initialized.
    MASSERT(s_pPendingSets);
    // thread should be a real live thread ID.
    MASSERT(Tasker::IsValidThreadId(thread));

    // Walk through m_Listeners back to front, since vector<>::erase is
    // much more efficient when removing the last item than any other.

    vector<Tasker::ThreadID>::iterator iter = m_Listeners.end();
    while (iter != m_Listeners.begin())
    {
        --iter;
        if (*iter == thread)
            iter = m_Listeners.erase(iter);
    }
}

//------------------------------------------------------------------------------
//! Utility used by Tasker::PrintAllThreads.
//
void ModsEvent::PrintEventsListenedFor (Tasker::ThreadID thread, INT32 pri)
{
   size_t ev;
   for (ev = 0; ev < s_Events.size(); ev++)
   {
      ModsEvent * pEv = s_Events[ev];
      if (pEv)
      {
         vector<Tasker::ThreadID>::const_iterator iter = pEv->m_Listeners.begin();
         for(/**/; iter != pEv->m_Listeners.end(); ++iter)
         {
            if ((*iter) == thread)
            {
               Printf(pri, " event %ld \"%s\"", (long) ev, pEv->m_Name);
               break;
            }
         }
      }
   }
}

//------------------------------------------------------------------------------
void ModsEvent::PrintAll (INT32 Priority)
{
   size_t ev;
   for (ev = 0; ev < s_Events.size(); ev++)
   {
      ModsEvent * pEv = s_Events[ev];
      if (pEv)
      {
         Printf(Priority, "  %ld \"%s\"", (long) ev, pEv->m_Name);

         vector<Tasker::ThreadID>::iterator iter = pEv->m_Listeners.end();
         while (iter != pEv->m_Listeners.begin())
         {
            --iter;
            Printf(Priority,  " (L=%d)", (*iter));
         }
         Printf(Priority, "\n");
      }
   }
}

//------------------------------------------------------------------------------
//
// class ModsCondition implementation:
//
//------------------------------------------------------------------------------

ModsCondition::ModsCondition()
{
    m_pImpl = std::make_unique<Impl>();
    m_pImpl->m_Event = Tasker::AllocEvent("signal", false);
}

//------------------------------------------------------------------------------
ModsCondition::~ModsCondition()
{
    m_pImpl->m_IsZombie = true;
    while (m_pImpl->m_WaitingCount > 0)
    {
        Tasker::SetEvent(m_pImpl->m_Event);
        Tasker::Yield();
    }
    Tasker::FreeEvent(m_pImpl->m_Event);
}
//------------------------------------------------------------------------------
ModsCondition::ModsCondition(ModsCondition&&) = default;
ModsCondition& ModsCondition::operator=(ModsCondition&&) = default;
//------------------------------------------------------------------------------
RC ModsCondition::Wait(void *mutex)
{
    return ModsCondition::Wait(mutex, Tasker::NO_TIMEOUT);
}

//------------------------------------------------------------------------------
RC ModsCondition::Wait(void *mutex, FLOAT64 timeoutMs)
{
    RC rc;
    m_pImpl->m_WaitingCount++;
    Tasker::ReleaseMutex(mutex);
    rc = Tasker::WaitOnEvent(m_pImpl->m_Event, timeoutMs);
    m_pImpl->m_WaitingCount--;
    Tasker::AcquireMutex(mutex);
    if (m_pImpl->m_IsZombie)
    {
        Tasker::Yield();
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//------------------------------------------------------------------------------
void ModsCondition::Signal()
{
    Tasker::SetEvent(m_pImpl->m_Event);
    Tasker::Yield();
}

//------------------------------------------------------------------------------
void ModsCondition::Broadcast()
{
    INT32 waiting = m_pImpl->m_WaitingCount;
    while (waiting > 0)
    {
        Tasker::SetEvent(m_pImpl->m_Event);
        Tasker::Yield();
        waiting--;
    }
}

//------------------------------------------------------------------------------
//
// class ModsTimeout implementation:
//
//------------------------------------------------------------------------------

// class static data
list<ModsTimeout::Timeout> ModsTimeout::s_Timeouts;
FLOAT64           ModsTimeout::s_TicksPerSec;
Tasker::ThreadID  ModsTimeout::s_TimerThreadId;
UINT32            ModsTimeout::s_OldTickCount;
ModsEvent*        ModsTimeout::s_TickEvent;
FLOAT64           ModsTimeout::s_SafeTimeoutMs;
volatile UINT32   ModsTimeout::s_IsrTickCount;

//------------------------------------------------------------------------------
RC ModsTimeout::Initialize()
{
   // Portable part: allocate s_TickEvent, start the TimerThread.
   s_TickEvent     = ModsEvent::Alloc("tick");
   s_TimerThreadId = Tasker::CreateThread
         (
         TimerThread,
         0,
         Tasker::MIN_STACK_SIZE,
         "timer"
         );

   s_IsrTickCount  = 0;
   s_OldTickCount  = 0;

   // OS-specific: set s_TicksPerSec, hook ISR to update s_IsrTickCount
   return OsInitialize();
}

//------------------------------------------------------------------------------
void ModsTimeout::Cleanup()
{
   // Os-specific: unhook the ISR.
   OsCleanup();
}

//------------------------------------------------------------------------------
void ModsTimeout::Add (Tasker::ThreadID id, UINT32 threadTimeoutIndex)
{
   // Before we add the new timeout, make sure any pending work for the timer
   // thread is cleared away.
   ProcessTicks();
   FLOAT64 timeoutMs =
       s_pPriv->m_pLwrrentThread->TimeOuts[threadTimeoutIndex].TimeoutMs;
   UINT32 timeoutTicks = UINT32(timeoutMs * s_TicksPerSec * 1e-3 + 0.5);
   UINT32 sumTicks = 0;  // Sum of ticks in previous Timeout entries

   if (timeoutTicks < 1)
       timeoutTicks = 1;

   Timeout to;
   to.Id = id;
   to.Index = threadTimeoutIndex;
   to.pThread = s_pPriv->m_pLwrrentThread;

   list<Timeout>::iterator iter;
   for (iter = s_Timeouts.begin(); iter != s_Timeouts.end(); ++iter)
   {
      if (timeoutTicks < sumTicks + (*iter).Counter)
      {
         // Insert the new timeout before this item, adjust its timeout.
          MASSERT(timeoutTicks >= sumTicks);
          to.Counter = timeoutTicks - sumTicks;
          (*iter).Counter -= to.Counter;
          s_Timeouts.insert(iter, to);
          return;
      }
      else if ((*iter).Id == id)
      {
         to.Counter = (*iter).Counter;
         (*iter).Counter = 0;
         s_Timeouts.insert(iter, to);
         s_pPriv->m_pLwrrentThread->TimeOuts[to.Index].TimeoutMs =
            (FLOAT64)(sumTicks + to.Counter) * 1000.0 / s_TicksPerSec;
         return;
      }
      else
      {
         sumTicks += (*iter).Counter;
      }
   }
   // Insert the new timeout at the end of the list.
   MASSERT(timeoutTicks >= sumTicks);
   to.Counter = timeoutTicks - sumTicks;
   s_Timeouts.push_back(to);
}

//------------------------------------------------------------------------------
//! Remove timeout from timeout list.
//! This is needed whenever a thread wakes up for a reason other than timeout.
void ModsTimeout::Remove (Tasker::ThreadID id)
{
   size_t oldNumTimeouts = s_Timeouts.size();

   if (oldNumTimeouts)
   {
      list<Timeout>::iterator iter;
      for (iter = s_Timeouts.begin(); iter != s_Timeouts.end(); ++iter)
      {
         if ((*iter).Id == id)
         {
            UINT32 count = (*iter).Counter;
            iter = s_Timeouts.erase(iter);
            if (iter != s_Timeouts.end())
               (*iter).Counter += count;
            return;
         }
      }
   }
}

//------------------------------------------------------------------------------
bool ModsTimeout::HasTimeout (Tasker::ThreadID id)
{
   list<Timeout>::iterator iter;
   for (iter = s_Timeouts.begin(); iter != s_Timeouts.end(); ++iter)
   {
      if ((*iter).Id == id)
         return true;
   }
   return false;
}

//------------------------------------------------------------------------------
void ModsTimeout::PrintAll (INT32 Priority)
{
   list<Timeout>::iterator iter;
   for (iter = s_Timeouts.begin(); iter != s_Timeouts.end(); ++iter)
   {
      Printf(Priority, "   thread %d, count %d\n",
            (*iter).Id,
            (*iter).Counter);
   }
}

//------------------------------------------------------------------------------
void ModsTimeout::TimerThread (void *)
{
   MASSERT(Tasker::IsInitialized());

   // Prevent a giant jump in time on the first entry to ProcessTicks.
   s_OldTickCount = s_IsrTickCount;

   while (true) // forever
   {
      Tasker::ResetEvent (s_TickEvent);
      Tasker::WaitOnEvent (s_TickEvent);

      ProcessTicks();
      // Optimization: block if timeout list is empty
      // Optimization: unhook isr if timeout list is empty
   }
}

//------------------------------------------------------------------------------
void ModsTimeout::ProcessTicks ()
{
   // Update our cached count, callwlate how many ticks we have to process.
   UINT32 newTickCount = s_IsrTickCount;
   UINT32 elapsed = newTickCount - s_OldTickCount;
   s_OldTickCount = newTickCount;

   // Assumptions: usually Timeouts is empty, usually elapsed is 1.

   while (s_Timeouts.size() && elapsed)
   {
      // 0 timeout shouldn't have been added to front of list.
      MASSERT(s_Timeouts.front().Counter);

      // Process one tick.
      s_Timeouts.front().Counter--;
      elapsed--;

      // Handle all the expirations.
      while (s_Timeouts.front().Counter == 0)
      {
         Timeout *pTimeout = &s_Timeouts.front();
         pTimeout->pThread->TimeOuts[pTimeout->Index].TimedOut = true;
         s_pPriv->WakeThread(pTimeout->Id, true/*timedOut*/);
         s_Timeouts.pop_front();
         if (s_Timeouts.empty())
            break;
      }
   }
}

void Tasker::EnableThreadStats()
{
}

void Tasker::PrintThreadStats()
{
}
