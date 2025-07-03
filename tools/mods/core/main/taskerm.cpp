/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file taskerm.cpp
 *
 * This file contains Tasker implementation based on native threads.
 */

#include "core/include/tasker.h"
#include "core/include/nativethread.h"
#include "core/include/abort.h"
#include "core/include/xp.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "jsapi.h"
#include "core/include/utility.h"
#include "core/include/cpu.h"
#include "core/utility/sharedsysmem.h"
#include "thread_mill.h"
#include <atomic>
#include <limits.h>
#include <vector>
#include <string>
#include <set>
#include <map>
#include <memory>

#include <boost/range/adaptor/filtered.hpp>
#include <boost/range/algorithm/transform.hpp>

namespace
{
    using ModsThread = Tasker::ModsThread;
    class ModsThreadGroup;
    class ModsMutex;

    class TaskermPriv
    {
    public:
        TaskermPriv()
        : m_NextTid(0)
        {
        }
        void Yield();

        enum AttachReason
        {
            dueToResume,
            dueToAttach
        };
        void Attach(AttachReason reason);

        enum DetachReason
        {
            dueToYield,
            dueToSuspend,
            dueToDetach
        };
        void Detach(DetachReason reason);

        Tasker::Private::ThreadMill m_ThreadMill;

        Tasker::NativeMutex m_ThreadStructsMutex;
        UINT32 m_GThreadIdxTlsId;
        UINT32 m_GroupTlsId;
        int m_NextTid;
        typedef map<int,ModsThread*> Threads;
        Threads m_Threads;

        Tasker::NativeMutex m_TlsMutex;
        set<UINT32> m_TlsInherited;

        Tasker::NativeMutex     m_SystemEventMutex;
        Tasker::NativeCondition m_SystemEventCond;

    private:
        // non-copyable
        TaskermPriv(const TaskermPriv&);
        TaskermPriv& operator=(const TaskermPriv&);
    };

    TaskermPriv* s_pPriv = 0;

    // Mutex to synchronize access to s_HwPollSleepNsec
    void * s_UpdateHzMutex = NULL;

#ifdef THREAD_STATISTICS
    volatile bool s_ThreadStats = false;
#else
    constexpr bool s_ThreadStats = false;
#endif

    atomic<UINT64> s_NumDetach[3];

    inline ModsThread* GetLwrThread()
    {
        return reinterpret_cast<ModsThread*>(s_pPriv->m_ThreadMill.GetThreadState());
    }

    inline ModsThreadGroup* GetLwrGroup()
    {
        return static_cast<ModsThreadGroup*>(
                Tasker::TlsGetValue(s_pPriv->m_GroupTlsId));
    }
} // anonymous namespace

namespace Tasker
{
    class ModsThread: public Private::ThreadMill::ThreadState
    {
    public:
        explicit ModsThread(const char* name)
        : m_Name(name ? name : "")
        {
        }

#ifdef THREAD_STATISTICS
        static constexpr int MAX_TRACKED_THREADS = 128;
#endif

        void RecordTime(TaskermPriv::AttachReason reason)
        {
            m_State = StateAttached;

            if (s_ThreadStats)
            {
                const UINT64 lwrTimeNs = Xp::GetWallTimeNS();
                const UINT64 deltaNs   = lwrTimeNs - m_LastTimeNs;
                m_LastTimeNs           = lwrTimeNs;
                if (reason == TaskermPriv::dueToAttach)
                {
                    m_TimeDetachedNs += deltaNs;
                }
                else
                {
                    m_TimeWaitingNs += deltaNs;
                }

                // Track thread switching
#ifdef THREAD_STATISTICS
                const int prevId = m_PrevThreadId;
                if (prevId != -1 && prevId != m_Id)
                {
                    const int idx = (prevId >= 0 && prevId < MAX_TRACKED_THREADS)
                                    ? prevId : (MAX_TRACKED_THREADS - 1);
                    ++m_SwitchCounts[idx];
                }
#endif
                m_PrevThreadId = m_Id;
            }
        }

        void RecordTime(TaskermPriv::DetachReason reason)
        {
            m_State = (reason == TaskermPriv::dueToYield)   ? StateYield   :
                      (reason == TaskermPriv::dueToSuspend) ? StateWaiting :
                                                              StateDetached;
            if (s_ThreadStats)
            {
                const UINT64 lwrTimeNs = Xp::GetWallTimeNS();
                const UINT64 deltaNs   = lwrTimeNs - m_LastTimeNs;
                m_LastTimeNs           = lwrTimeNs;
                m_TimeAttachedNs      += deltaNs;
            }
        }

        void ResetStats()
        {
            m_TimeDetachedNs = 0U;
            m_TimeAttachedNs = 0U;
            m_TimeWaitingNs  = 0U;
        }

        void PrintStats()
        {
            if (s_ThreadStats)
            {
                constexpr INT32 pri = Tee::PriNormal;
                const UINT64 total = m_TimeDetachedNs + m_TimeAttachedNs + m_TimeWaitingNs;
                if (total == 0)
                    return;
                Printf(pri, "Statistics: \"%s\""
                            " detached %llu%% (%llums),"
                            " attached %llu%% (%llums),"
                            " yielding %llu%% (%llums)\n",
                       m_Name.c_str(),
                       ((m_TimeDetachedNs*100U)/total),
                       m_TimeDetachedNs/1000000U,
                       ((m_TimeAttachedNs*100U)/total),
                       m_TimeAttachedNs/1000000U,
                       ((m_TimeWaitingNs*100U)/total),
                       m_TimeWaitingNs/1000000U);

#ifdef THREAD_STATISTICS
                for (int fromId = 0; fromId < MAX_TRACKED_THREADS; fromId++)
                {
                    const UINT64 count = m_SwitchCounts[fromId];
                    if (count > 1)
                    {
                        Printf(pri, "    switches from tid %d: %llu\n", fromId, count);
                    }
                }
#endif
            }
        }

        enum AttachState
        {
            StateDetached,
            StateAttached,
            StateYield,
            StateWaiting
        };

        Tasker::NativeThread::Id m_Thread;
        int                      m_Id             = -1;
        atomic<AttachState>      m_State          { StateDetached };
        static atomic<int>       m_PrevThreadId;
        string                   m_Name;
        bool                     m_Dead           = false;
        ModsThread*              m_Joining        = nullptr;
        int                      m_DetachCount    = 0;
        JSContext*               m_pJsContext     = nullptr;
        UINT64                   m_LastTimeNs     = 0U;
        UINT64                   m_TimeAttachedNs = 0U;
        UINT64                   m_TimeDetachedNs = 0U;
        UINT64                   m_TimeWaitingNs  = 0U;
#ifdef THREAD_STATISTICS
        UINT64                   m_SwitchCounts[MAX_TRACKED_THREADS] = { };
#endif
    };

    atomic<int> ModsThread::m_PrevThreadId{-1};
}

namespace
{
    struct ModsThreadWrapperArg
    {
        ModsThread* pThread;
        Tasker::ThreadFunc threadFunc;
        void* threadFuncArg;
        map<UINT32,void*> tlsInit;
    };

    void ModsThreadWrapper(void* arg)
    {
        // Scope out the exelwtion to make sure everything is destroyed
        // before we call ExitThread
        {
            PROFILE_ZONE(GENERIC)

            // Get the arguments for the thread function
            ModsThreadWrapperArg* const pInit = static_cast<ModsThreadWrapperArg*>(arg);
            ModsThread* const pThread = pInit->pThread;
            Tasker::ThreadFunc threadFunc = pInit->threadFunc;
            void* threadFuncArg = pInit->threadFuncArg;

            PROFILE_SET_THREAD_NAME(pThread->m_Name.c_str())

            // Record time for measuring attachment/detachment
            pThread->m_LastTimeNs = Xp::GetWallTimeNS();

            // Set current thread structure
            s_pPriv->m_ThreadMill.InitLwrrentThread(pThread);

            // Initialize TLS
            map<UINT32,void*>::const_iterator tlsIt = pInit->tlsInit.begin();
            for ( ; tlsIt != pInit->tlsInit.end(); ++tlsIt)
            {
                Tasker::TlsSetValue(tlsIt->first, tlsIt->second);
            }

            // Delete the init structure as it's no longer needed
            delete pInit;

            // Wait for the thread to be "scheduled"
            s_pPriv->Attach(TaskermPriv::dueToAttach);

            Printf(Tee::PriDebug, "Created thread %s\n", pThread->m_Name.c_str());

            // Run the thread function
            threadFunc(threadFuncArg);
        }

        // Stop the thread
        Tasker::ExitThread();

        // This point is never reached
        MASSERT(0);
    }

    // Objects of this class do not really detach the thread from the Tasker.
    // This class is for temporary detachment of attached threads. This is
    // useful if we want to wait on a resource such as a mutex, so we want
    // other threads to run normally. After the resource is acquired (or not)
    // the destructor will reattach the thread.
    // This has no influence on detached threads.
    class SuspendAttachment
    {
    public:
        SuspendAttachment()
        : m_Suspended(!GetLwrThread()->m_DetachCount)
        {
            if (m_Suspended)
            {
                s_pPriv->Detach(TaskermPriv::dueToSuspend);
            }
        }
        ~SuspendAttachment()
        {
            if (m_Suspended)
            {
                s_pPriv->Attach(TaskermPriv::dueToResume);
            }
        }
        SuspendAttachment(const SuspendAttachment&)            = delete;
        SuspendAttachment& operator=(const SuspendAttachment&) = delete;
        bool IsSuspended() const
        {
            return m_Suspended;
        }

    private:
        const bool m_Suspended;
    };

    // Objects of this class lock a pthread mutex. If the mutex can be
    // locked without waiting, it will be locked immediately. However if
    // somebody else is holding the mutex, the current thread will be
    // temporarily detached from the Tasker and let other threads to run
    // while it's waiting for the mutex. The destructor will reattach the
    // thread back to the tasker. For detached threads nothing will
    // happen (i.e. there is no need to temporarily detach).
    class SuspendAttAndLock
    {
    public:
        explicit SuspendAttAndLock(Tasker::NativeMutex& mutex);
        ~SuspendAttAndLock();
        SuspendAttAndLock(const SuspendAttAndLock&)            = delete;
        SuspendAttAndLock& operator=(const SuspendAttAndLock&) = delete;
        void Suspend();
        void Unsuspend();

    private:
        Tasker::NativeMutex& m_Mutex;
        bool m_Suspended;
    };

    SuspendAttAndLock::SuspendAttAndLock(Tasker::NativeMutex& mutex)
    : m_Mutex(mutex)
    , m_Suspended(false)
    {
        if (!GetLwrThread()->m_DetachCount)
        {
            // If the thread is attached, suspend attachment only
            // if unable to lock the mutex immediately.
            if (!mutex.TryLock())
            {
                // Suspend attachment
                m_Suspended = true;
                s_pPriv->Detach(TaskermPriv::dueToSuspend);

                // Lock the mutex
                mutex.Lock();
            }
        }
        // For detached threads just lock the mutex
        else
        {
            mutex.Lock();
        }
    }

    SuspendAttAndLock::~SuspendAttAndLock()
    {
        // Unlock the mutex first, because reattaching back to
        // the Tasker can take a significant amount of time.
        m_Mutex.Unlock();

        // Reattach back to the Tasker
        if (m_Suspended)
        {
            s_pPriv->Attach(TaskermPriv::dueToResume);
        }
    }

    void SuspendAttAndLock::Suspend()
    {
        if (!m_Suspended && !GetLwrThread()->m_DetachCount)
        {
            m_Suspended = true;
            s_pPriv->Detach(TaskermPriv::dueToSuspend);
        }
    }

    void SuspendAttAndLock::Unsuspend()
    {
        if (m_Suspended)
        {
            s_pPriv->Attach(TaskermPriv::dueToResume);
            m_Suspended = false;
        }
    }

    FLOAT64 s_DefaultTimeoutMs = 1000;
    FLOAT64 s_SimTimeoutMs = Tasker::NO_TIMEOUT;
    bool s_bIgnoreCpuAffinityFailure = false;
    constexpr UINT64 defaultHwPollSleepNs = ~0ULL;
    UINT64 s_HwPollSleepNsec = defaultHwPollSleepNs;

    class ModsMutex
    {
    public:
        ModsMutex(const char * name, unsigned order)
        : m_Owner(-1)
        , m_LockCount(0)
        , m_Name(name)
        , m_Order(order)
        {
        }
        ~ModsMutex()
        {
            MASSERT(Cpu::AtomicRead(&m_LockCount) == 0);
            MASSERT(Cpu::AtomicRead(&m_Owner) == -1);
        }
        void Acquire();
        bool TryAcquire();
        void Release();
        bool IsMutexLocked() const
        {
            return Cpu::AtomicRead(const_cast<volatile INT32*>(&m_Owner)) != -1;
        }
        bool IsMutexLockedByLwrThread() const
        {
            return Cpu::AtomicRead(const_cast<volatile INT32*>(&m_Owner)) == GetLwrThread()->m_Id;
        }
        RC WaitCondition(Tasker::NativeCondition &m_Cond, FLOAT64 timeoutMs);

    private:
        volatile INT32 m_Owner;
        volatile INT32 m_LockCount;
        Tasker::NativeMutex m_Mutex;

    public:
        const char * const m_Name;
        const unsigned m_Order;
    };

    void ModsMutex::Acquire()
    {
        ModsThread* const pLwrThread = GetLwrThread();

        // Lock the mutex only if the current thread doesn't already own it
        if (Cpu::AtomicRead(&m_Owner) != pLwrThread->m_Id)
        {
            // Attempt to lock the mutex
            if (pLwrThread->m_DetachCount)
            {
                m_Mutex.Lock();
            }
            else if (!m_Mutex.TryLock())
            {
                // The mutex has already been locked by some other thread, so:

                // Allow other Tasker threads to execute while we're waiting
                SuspendAttachment suspend;

                // Lock the mutex
                m_Mutex.Lock();
            }

            // Claim mutex ownership
            MASSERT(Cpu::AtomicRead(&m_Owner) == -1);
            Cpu::AtomicWrite(&m_Owner, pLwrThread->m_Id);

            MASSERT(Cpu::AtomicRead(&m_LockCount) == 0);
        }

        // Increment lock count
        Cpu::AtomicAdd(&m_LockCount, 1);
    }

    bool ModsMutex::TryAcquire()
    {
        ModsThread* const pLwrThread = GetLwrThread();

        // Lock the mutex only if the current thread doesn't already own it
        if (Cpu::AtomicRead(&m_Owner) != pLwrThread->m_Id)
        {
            // Attempt to lock the mutex
            if (!m_Mutex.TryLock())
            {
                // Some other thread owns it, give up
                return false;
            }

            // Claim mutex ownership
            MASSERT(Cpu::AtomicRead(&m_Owner) == -1);
            Cpu::AtomicWrite(&m_Owner, pLwrThread->m_Id);

            MASSERT(Cpu::AtomicRead(&m_LockCount) == 0);
        }

        // Increment lock count
        Cpu::AtomicAdd(&m_LockCount, 1);

        return true;
    }

    void ModsMutex::Release()
    {
        const int lwrId = GetLwrThread()->m_Id;

        // Make sure we're the owner
        MASSERT(Cpu::AtomicRead(&m_Owner) == lwrId);
        if (Cpu::AtomicRead(&m_Owner) != lwrId)
        {
            return;
        }

        // Decrement lock count
        if (Cpu::AtomicAdd(&m_LockCount, -1) == 1)
        {
            // Release the mutex if the lock count reached 0
            Cpu::AtomicWrite(&m_Owner, -1);
            m_Mutex.Unlock();
        }
    }

    RC ModsMutex::WaitCondition(Tasker::NativeCondition &m_Cond, FLOAT64 timeoutMs)
    {
        ModsThread* const pLwrThread = GetLwrThread();
        const int lwrId = pLwrThread->m_Id;

        MASSERT(Cpu::AtomicRead(&m_Owner) == lwrId);
        if (Cpu::AtomicRead(&m_Owner) != lwrId)
        {
            return RC::SOFTWARE_ERROR;
        }

        MASSERT(Cpu::AtomicRead(&m_LockCount) == 1);
        if (Cpu::AtomicRead(&m_LockCount) != 1)
        {
            return RC::SOFTWARE_ERROR;
        }

        Cpu::AtomicAdd(&m_LockCount, -1);

        // Release mods mutex
        Cpu::AtomicWrite(&m_Owner, -1);

        Tasker::NativeCondition::WaitResult ret =
            (timeoutMs != Tasker::NO_TIMEOUT)
                ? m_Cond.Wait(m_Mutex, timeoutMs)
                : m_Cond.Wait(m_Mutex);

        // Claim mods mutex ownership
        MASSERT(Cpu::AtomicRead(&m_Owner) == -1);
        Cpu::AtomicWrite(&m_Owner, lwrId);

        MASSERT(Cpu::AtomicRead(&m_LockCount) == 0);
        Cpu::AtomicAdd(&m_LockCount, 1);

        if (Tasker::NativeCondition::timeout == ret)
        {
            return RC::TIMEOUT_ERROR;
        }
        if (Tasker::NativeCondition::success != ret)
        {
            return RC::SOFTWARE_ERROR;
        }
        return OK;
    }

    // Timeouts on simulated elwironments work differently.  For example, sim MODS has hacks to
    // make all timeouts infinite.
    // Note: We limit detection of simulated elwironments to sim MODS only.  We treat full stack
    // elwironments like VDK as "hardware" for the purpose of Tasker, otherwise some fragile
    // display tests start to fail in GVS.
#ifdef SIM_BUILD
    bool IsSimulation()
    {
        return Platform::GetSimulationMode() != Platform::Hardware;
    }
#else
    constexpr bool IsSimulation()
    {
        return false;
    }
#endif

    UINT64 GetPlatformTimeMS(SuspendAttAndLock* pLock)
    {
        const bool unsuspend = IsSimulation();
        if (unsuspend)
            pLock->Unsuspend();

        const UINT64 time = Platform::GetTimeMS();

        if (unsuspend)
            pLock->Suspend();

        return time;
    }

    UINT64 GetHwPollSleepNs()
    {
        if (s_HwPollSleepNsec == defaultHwPollSleepNs)
        {
            // The min sleep value for Amodel/Fmodel/RTL is copied from tasker.cpp
            // See CL 8161395 and bug 755940
            constexpr UINT64 defSimSleepTimeNs = 10;

            return (Platform::GetSimulationMode() == Platform::Hardware) ? 0 : defSimSleepTimeNs;
        }

        return s_HwPollSleepNsec;
    }
}

void TaskermPriv::Yield()
{
    ModsThread* const pLwrThread = GetLwrThread();

    if (!pLwrThread->m_DetachCount)
    {
        ++s_NumDetach[dueToYield];

        pLwrThread->RecordTime(dueToYield);

        m_ThreadMill.SwitchToNextThread();

        pLwrThread->RecordTime(dueToResume);
    }
}

void TaskermPriv::Attach(AttachReason reason)
{
    ModsThread* const pLwrThread = GetLwrThread();

    if (reason == dueToAttach)
    {
        // Until this spot, the thread was detached, so report
        // that it is being attached now.
        pLwrThread->RecordTime(dueToAttach);
        // And the following coop mutex lock is blocking, so
        // report the time spent when waiting on the mutex as
        // yielding, while we are waiting for other cooperative
        // threads to pass control to us.
        pLwrThread->RecordTime(dueToYield);
    }

    m_ThreadMill.Attach();

    pLwrThread->RecordTime(dueToResume);
}

void TaskermPriv::Detach(DetachReason reason)
{
    if (s_pPriv)
    {
        ModsThread* const pLwrThread = GetLwrThread();
        pLwrThread->RecordTime(reason);
    }

    m_ThreadMill.Detach();

    ++s_NumDetach[reason];
}

class ModsEvent
{
public:
    enum EventType
    {
        manualResetEvent,
        autoResetEvent,
        systemEvent
    };
    ModsEvent(const char* name, EventType eventType);
    ~ModsEvent();
    void Set();
    void Reset();
    bool IsSet();
    void ForceSet();
    bool CheckAndResetSystemEvent(SuspendAttAndLock* pSuspend);
    RC Wait(FLOAT64 timeoutMs);
    void *GetOsEvent(UINT32 hClient, UINT32 gpuInstance);
    void* GetOsEvent();
    bool IsOsEvent() const
    {
        return m_OsEvent && (m_OsEvent != this);
    }
    EventType GetType() const { return m_EventType; }

    static Tasker::NativeMutex& GetSystemEventMutex() { return s_pPriv->m_SystemEventMutex; }
    static Tasker::NativeCondition& GetSystemEventCond() { return s_pPriv->m_SystemEventCond; }

private:
    ModsEvent();

    // non-copyable
    ModsEvent(const ModsEvent&);
    ModsEvent& operator=(const ModsEvent&);

    RC SetResetWait(SuspendAttAndLock *pLock);

    void OsAllocEvent(UINT32 hClient, UINT32 hDevice);
    void OsFreeEvent(UINT32 hClient, UINT32 hDevice);

    enum EventState
    {
        esSET
      , esRESET
      , esWAKING
      , esCHANGING_STATE
    };

    EventState              m_State = esRESET;
    EventType               m_EventType;
    Tasker::NativeMutex     m_Mutex;
    int                     m_NumWaiting = 0;
    int                     m_NumChangingState = 0;
    Tasker::NativeCondition m_Cond;
    const string            m_Name;
    void                   *m_OsEvent = nullptr;
    UINT32                  m_hOsEventClient = 0;
    UINT32                  m_hOsEventDevice = 0;
};

ModsEvent::ModsEvent(const char* name, EventType eventType)
  : m_EventType(eventType)
  , m_Name(name ? name : "")
{}

ModsEvent::~ModsEvent()
{
    // Release all threads waiting on this event
    {
        SuspendAttAndLock lock(m_Mutex);

        if (IsOsEvent())
        {
            OsFreeEvent(m_hOsEventClient, m_hOsEventDevice);
            m_OsEvent = nullptr;
        }

        m_State = esWAKING;
        m_EventType = manualResetEvent;
        if (m_NumWaiting || m_NumChangingState)
        {
            m_Cond.Broadcast();
            // In order to avoid tearing down the event while other threads are
            // still waiting on the event, it is necessary  to wait for all
            // suspended threads to wake up before destroying the event
            while (m_State == esWAKING)
            {
                lock.Suspend();
                const Tasker::NativeCondition::WaitResult ret =
                    m_Cond.Wait(m_Mutex);
                if (Tasker::NativeCondition::success != ret)
                {
                    return;
                }
            }
        }
    }
}

void ModsEvent::Reset()
{
    SuspendAttAndLock lock(m_Mutex);
    if (m_EventType != manualResetEvent)
    {
        Printf(Tee::PriHigh,
               "Error : ModsEvent::Reset() called on auto reset event %s\n",
               m_Name.c_str());
        return;
    }

    if (OK != SetResetWait(&lock))
        return;

    // Only need to update the event and broadcast if this is the last
    // changing state to be processed (i.e. the one that will stick)
    if (!m_NumChangingState)
    {
        m_State = esRESET;
        m_Cond.Broadcast();
    }
}

void ModsEvent::Set()
{
    if (IsOsEvent())
    {
        SuspendAttAndLock lock(m_Mutex);
        m_State = esSET;
        Xp::SetOsEvent(GetOsEvent());
        return;
    }

    if (m_EventType == systemEvent)
    {
        {
            SuspendAttAndLock lock(GetSystemEventMutex());
            m_State = esSET;
        }
        GetSystemEventCond().Broadcast();
        return;
    }

    SuspendAttAndLock lock(m_Mutex);

    if (OK != SetResetWait(&lock))
        return;

    // Only need to update the event and broadcast if this is the last
    // changing state to be processed (i.e. the one that will stick)
    if (!m_NumChangingState)
    {
        m_State = m_NumWaiting ? esWAKING : esSET;

        // Need to broadcast even if no thread was waiting for the event
        // because the thread may still have been waiting for esCHANGING_STATE
        // to clear
        m_Cond.Broadcast();
    }
}

bool ModsEvent::CheckAndResetSystemEvent(SuspendAttAndLock* pSuspend)
{
    MASSERT(m_EventType == systemEvent);

    if (!m_Mutex.TryLock())
    {
        pSuspend->Suspend();
        m_Mutex.Lock();
    }

    const bool set = m_State == esSET;

    if (set)
    {
        m_State = esRESET;
    }

    m_Mutex.Unlock();

    return set;
}

bool ModsEvent::IsSet()
{
    SuspendAttAndLock lock(m_Mutex);
    return (m_State != esRESET);
}

void ModsEvent::ForceSet()
{
    m_Mutex.Lock();
    m_State = (m_EventType == manualResetEvent) ? esSET : esRESET;
    m_Mutex.Unlock();
}

RC ModsEvent::Wait(FLOAT64 timeoutMs)
{
    if (m_EventType == systemEvent)
    {
        ModsEvent* pEvent     = this;
        UINT32 completedIndex = ~0U;
        UINT32 numCompleted   = 0;
        const RC rc = Tasker::WaitOnMultipleEvents(&pEvent, 1, &completedIndex, 1, &numCompleted,
                                                   timeoutMs);
        if (rc == RC::OK)
        {
            MASSERT(numCompleted == 1);
            MASSERT(completedIndex == 0);
        }
        return rc;
    }

    if (IsOsEvent())
    {
        SuspendAttachment suspend;

        m_Mutex.Lock();
        const auto lwrState = m_State;
        m_Mutex.Unlock();

        if (lwrState == esSET)
        {
            return RC::OK;
        }

        UINT32 completed    = 0;
        UINT32 completedIdx = 0;

        void* pEvent = GetOsEvent();
        const RC rc = Xp::WaitOsEvents(&pEvent, 1, &completed, 1, &completedIdx, timeoutMs);
        if (rc == RC::OK)
        {
            ForceSet();
        }
        return rc;
    }

    // Lock the event mutex
    SuspendAttAndLock lock(m_Mutex);
    while ((m_State == esWAKING) || (m_State == esCHANGING_STATE))
    {
        lock.Suspend();
        const Tasker::NativeCondition::WaitResult ret = m_Cond.Wait(m_Mutex);
        if (Tasker::NativeCondition::success != ret)
        {
            return RC::SOFTWARE_ERROR;
        }
    }

    // If the event is already set - do nothing
    if (m_State == esSET)
    {
        // Auto reset the event auto reset events only ever let one thread
        // run.  This is a carry over from the design of the original tasker
        if (m_EventType != manualResetEvent)
        {
            m_State = esRESET;
        }
        return OK;
    }

    // Increment number of threads waiting for the event
    m_NumWaiting++;

    // Wait for event to be set
    bool ok = true;
    UINT64 LwrTimeMs = GetPlatformTimeMS(&lock);

    // Allow other Tasker threads to execute while we're waiting.  Do not suspend
    // attachment until after getting the initial time because GetTimeMS on sim
    // builds can try and acquire a mutex
    lock.Suspend();

    UINT64 LwrTimeout = static_cast<UINT64>(timeoutMs);
    const UINT64 EndTimeMs = LwrTimeout + LwrTimeMs;
    Tasker::NativeCondition::WaitResult ret;
    while (m_State != esWAKING)
    {
        if (timeoutMs != Tasker::NO_TIMEOUT)
        {
            LwrTimeMs = GetPlatformTimeMS(&lock);
            if (LwrTimeMs >= EndTimeMs)
            {
                ok = false;
                break;
            }
            else
            {
                LwrTimeout = EndTimeMs - LwrTimeMs;
            }
            ret = m_Cond.Wait(m_Mutex, static_cast<double>(LwrTimeout));
        }
        else
        {
            ret = m_Cond.Wait(m_Mutex);
        }

        if (Tasker::NativeCondition::timeout == ret)
        {
            if (m_State != esWAKING)
            {
                ok = false;
            }
            break;
        }
        if (Tasker::NativeCondition::success != ret)
        {
            m_NumWaiting--;
            return RC::SOFTWARE_ERROR;
        }
    }

    // Decrement number of waiting threads
    m_NumWaiting--;

    // Auto reset the event auto reset events only ever let one thread
    // run.  This is a carry over from the design of the original tasker
    if (ok)
    {
        if (m_EventType != manualResetEvent)
        {
            m_State = esRESET;
            m_Cond.Broadcast();
        }
        else if (0 == m_NumWaiting)
        {
            m_State = m_NumChangingState ? esCHANGING_STATE : esSET;
            m_Cond.Broadcast();
        }
    }

    return ok ? OK : RC::TIMEOUT_ERROR;
}

void *ModsEvent::GetOsEvent(UINT32 hClient, UINT32 hDevice)
{
    if (Xp::HasClientSideResman())
    {
        m_OsEvent = this;
        return this;
    }

    bool needNewEvent = !m_OsEvent;

    if (!needNewEvent && (Xp::GetOperatingSystem() != Xp::OS_WINDOWS))
    {
        // On Linux, make sure that hClient and hDevice have not changed.
        // On Windows hClient and hDevice are not used, so no need to check it there.
        MASSERT((!m_hOsEventClient && !m_hOsEventDevice) ||
                (m_hOsEventClient == hClient && m_hOsEventDevice == hDevice));
        if ((hClient != m_hOsEventClient) || (hDevice != m_hOsEventDevice))
        {
            needNewEvent = true;
        }
    }

    if (needNewEvent)
    {
        if (IsOsEvent())
        {
            OsFreeEvent(m_hOsEventClient, m_hOsEventDevice);
        }
        m_hOsEventClient = hClient;
        m_hOsEventDevice = hDevice;
        OsAllocEvent(hClient, hDevice);
    }
    return m_OsEvent;
}

void* ModsEvent::GetOsEvent()
{
    if (!m_OsEvent)
    {
        if (Xp::HasClientSideResman())
        {
            m_OsEvent = this;
        }
        else
        {
            OsAllocEvent(0, 0);
        }
    }
    return m_OsEvent;
}

RC ModsEvent::SetResetWait(SuspendAttAndLock *pLock)
{
    m_NumChangingState++;
    while (m_State == esWAKING)
    {
        pLock->Suspend();
        const Tasker::NativeCondition::WaitResult ret = m_Cond.Wait(m_Mutex);
        if (Tasker::NativeCondition::success != ret)
        {
            MASSERT(!"ModsEvent::WaitNotWaking : Native wait failed");
            m_NumChangingState--;
            return RC::SOFTWARE_ERROR;
        }
    }
    m_NumChangingState--;
    return OK;
}

void ModsEvent::OsAllocEvent(UINT32 hClient, UINT32 hDevice)
{
    m_OsEvent = Xp::AllocOsEvent(hClient, hDevice);
}

void ModsEvent::OsFreeEvent(UINT32 hClient, UINT32 hDevice)
{
    MASSERT(!Xp::HasClientSideResman());
    Xp::FreeOsEvent(m_OsEvent, hClient, hDevice);
}

struct ModsCondition::Impl
{
    Tasker::NativeCondition m_Cond;
};

ModsCondition::ModsCondition()
{
    m_pImpl = std::make_unique<Impl>();
}

ModsCondition::~ModsCondition()
{
}

ModsCondition::ModsCondition(ModsCondition&&) = default;
ModsCondition& ModsCondition::operator=(ModsCondition&&) = default;

RC ModsCondition::Wait(void *mutex)
{
    return ModsCondition::Wait(mutex, Tasker::NO_TIMEOUT);
}

RC ModsCondition::Wait(void *mutex, FLOAT64 timeoutMs)
{
    SuspendAttachment suspend;
    ModsMutex *m = static_cast<ModsMutex*>(mutex);
    MASSERT(m->IsMutexLockedByLwrThread());
    return m->WaitCondition(m_pImpl->m_Cond, timeoutMs);
}

void ModsCondition::Signal()
{
    m_pImpl->m_Cond.Signal();
}

void ModsCondition::Broadcast()
{
    m_pImpl->m_Cond.Broadcast();
}

RC Tasker::Initialize()
{
    // Do not allow dual initialization
    MASSERT(!s_pPriv);
    RC rc;

    if (s_pPriv)
    {
        Printf(Tee::PriHigh, "Tasker already initialized!\n");
        return RC::SOFTWARE_ERROR;
    }

    // Create the global private Tasker object
    s_pPriv = new TaskermPriv;

    // Create TLS index for thread index and group
    s_pPriv->m_GThreadIdxTlsId = TlsAlloc(false);
    s_pPriv->m_GroupTlsId = TlsAlloc(false);

    // Create a thread object for the main thread
    if (0 != CreateThread(0, 0, 0, "main"))
    {
        Printf(Tee::PriHigh, "Invalid main thread id!\n");
        MASSERT(!"Invalid main thread id!");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(SharedSysmem::Initialize());

    return OK;
}

void Tasker::Cleanup()
{
    // Allow Cleanup to be called only once
    if (!s_pPriv)
    {
        return;
    }

    // Make sure other threads had a chance to run
    s_pPriv->Yield();

    TaskermPriv* const pPriv = s_pPriv;
    ModsThread* pLwrThread = 0;
    if (s_UpdateHzMutex)
        Tasker::FreeMutex(s_UpdateHzMutex);

    // Lock the mutex guarding thread structure storage
    {
        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);

        // Save the main thread for later
        TaskermPriv::Threads::const_iterator threadIt = s_pPriv->m_Threads.begin();
        MASSERT(threadIt != s_pPriv->m_Threads.end());
        pLwrThread = threadIt->second;
        MASSERT(threadIt->first == 0);
        MASSERT(GetLwrThread() == pLwrThread);
        ++threadIt;

        // Release all memory (skip the main thread of id 0)
        for ( ; threadIt != s_pPriv->m_Threads.end(); ++threadIt)
        {
            ModsThread* const pThread = threadIt->second;
            MASSERT(pThread);

            // Check if the JS context has been freed
            MASSERT(!pThread->m_pJsContext);

            // Last chance to clean up
            if (!pThread->m_Dead)
            {
                s_pPriv->Yield();
            }

            // Attempt to delete thread structure
            MASSERT(pThread->m_Dead);
            if (pThread->m_Dead)
            {
                NativeThread::Join(pThread->m_Thread);
                s_pPriv->m_ThreadMill.Destroy(pThread);
                delete pThread;
            }
            else
            {
                // It is an offense to not join a thread. This will cause
                // memory leaks and who knows what else.
                Printf(Tee::PriLow, "Unable to kill thread \"%s\", detaching it\n",
                        pThread->m_Name.c_str());
                NativeThread::Detach(pThread->m_Thread);
            }
        }
        s_pPriv->m_Threads.clear();

        SharedSysmem::Shutdown();

        // Null out the global private Tasker object
        s_pPriv = 0;
    }

    // Print thread run times (attached, detached)
    MASSERT(!pLwrThread->m_DetachCount);
    pLwrThread->RecordTime(TaskermPriv::dueToDetach);
    pLwrThread->PrintStats();
    const INT32 pri = s_ThreadStats ? Tee::PriNormal : Tee::PriLow;
    Printf(pri, "Tasker Yields:   %llu\n", s_NumDetach[TaskermPriv::dueToYield].load());
    Printf(pri, "Tasker Suspends: %llu\n", s_NumDetach[TaskermPriv::dueToSuspend].load());
    Printf(pri, "Tasker Detaches: %llu\n", s_NumDetach[TaskermPriv::dueToDetach].load());

    // Unlock the coop mutex before it is freed
    pPriv->Detach(TaskermPriv::dueToDetach);

    // Release TLS indices used for thread groups
    NativeTlsValue::Free(pPriv->m_GroupTlsId);
    NativeTlsValue::Free(pPriv->m_GThreadIdxTlsId);

    // Delete the main thread's structure
    MASSERT(!pLwrThread->m_DetachCount);
    pPriv->m_ThreadMill.Destroy(pLwrThread);
    delete pLwrThread;

    // Really delete the global private Tasker object now
    // This is after the mutex inside of it has been unlocked
    delete pPriv;
}

bool Tasker::CanThreadYield()
{
    return true;
}

void Tasker::SetCanThreadYield(bool)
{
}

Tasker::ThreadID Tasker::CreateThread
(
    ThreadFunc function,
    void* arg,
    UINT32,
    const char* name
)
{
    MASSERT(IsInitialized());

    // Create thread object
    ModsThread* const pThread = new ModsThread(name);

    s_pPriv->m_ThreadMill.Initialize(pThread, Private::ThreadMill::Detached);

    // Allocate thread id and save the thread object pointer
    int tid = -1;
    {
        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);
        while (s_pPriv->m_Threads.find(s_pPriv->m_NextTid)
               != s_pPriv->m_Threads.end())
        {
            s_pPriv->m_NextTid++;
        }
        tid = s_pPriv->m_NextTid;
        s_pPriv->m_NextTid++;
        s_pPriv->m_Threads[tid] = pThread;
        pThread->m_Id = tid;
    }

    // Run thread function
    if (function)
    {
        // Prepare thread init struct
        ModsThreadWrapperArg* const pInit = new ModsThreadWrapperArg;
        pInit->pThread = pThread;
        pInit->threadFunc = function;
        pInit->threadFuncArg = arg;

        // Make a copy of all inherited TLS values for the new thread
        {
            NativeMutexHolder lock(s_pPriv->m_TlsMutex);
            set<UINT32>::const_iterator tlsIt = s_pPriv->m_TlsInherited.begin();
            for ( ; tlsIt != s_pPriv->m_TlsInherited.end(); ++tlsIt)
            {
                pInit->tlsInit[*tlsIt] = TlsGetValue(*tlsIt);
            }
        }

        // Create new thread
        if (!NativeThread::Create(&pThread->m_Thread, ModsThreadWrapper,
                                  pInit, name))
        {
            s_pPriv->m_ThreadMill.Destroy(pThread);
            delete pThread;
            delete pInit;
            Printf(Tee::PriHigh, "Failed to create thread.");
            MASSERT(!"Failed to create thread.");
            return -1;
        }
    }
    // If it's the main thread, just mark it as ready and running
    else
    {
        pThread->m_LastTimeNs = Xp::GetWallTimeNS();
        pThread->m_Thread = NativeThread::Self();
        s_pPriv->m_ThreadMill.InitLwrrentThread(pThread);
        s_pPriv->Attach(TaskermPriv::dueToAttach);
    }
    return tid;
}

namespace
{
    class ModsThreadGroup
    {
    public:
        ModsThreadGroup(Tasker::ThreadFunc func, void* arg, UINT32 numThreads);
        void Release();

        enum State
        {
            stateInit
            ,stateFailedCreation
            ,stateRunning
            ,stateBarrierError
            ,stateDestroy
        };
        void SetState(State state);

        Tasker::ThreadFunc       m_ThreadFunc;
        void*                    m_ThreadArg;
        UINT32                   m_Size;
        UINT32                   m_NumThreadsAlive;
        UINT32                   m_NumThreadsWaiting;
        vector<Tasker::ThreadID> m_ThreadIds;
        volatile UINT32          m_BarrierReleaseCount;
        volatile State           m_State;
        Tasker::NativeMutex      m_Mutex;
        Tasker::NativeCondition  m_Cond;
    };

    ModsThreadGroup::ModsThreadGroup(Tasker::ThreadFunc func, void* arg, UINT32 numThreads)
    : m_ThreadFunc(func)
    , m_ThreadArg(arg)
    , m_Size(numThreads)
    , m_NumThreadsAlive(0)
    , m_NumThreadsWaiting(0)
    , m_BarrierReleaseCount(0)
    , m_State(stateInit)
    {
    }

    void ModsThreadGroup::Release()
    {
        ++m_BarrierReleaseCount;
        m_NumThreadsWaiting = 0;
        m_Cond.Broadcast();
    }

    void ModsThreadGroup::SetState(State state)
    {
        m_State = state;
        m_Cond.Broadcast();
    }

    struct ModsGroupThreadWrapperArg
    {
        ModsThreadGroup* pGroup;
        UINT32 threadIdx;
    };

    void ModsGroupThreadWrapper(void* arg)
    {
        // Extract group from the init arg
        unique_ptr<ModsGroupThreadWrapperArg> pInit(
            static_cast<ModsGroupThreadWrapperArg*>(arg));
        ModsThreadGroup* const pGroup = pInit->pGroup;

        // Setup TLS
        Tasker::TlsSetValue(s_pPriv->m_GThreadIdxTlsId,
                reinterpret_cast<void*>(
                    static_cast<size_t>(pInit->threadIdx)));
        Tasker::TlsSetValue(s_pPriv->m_GroupTlsId, pGroup);

        // Release the init structure
        pInit.reset(0);

        {
            // Wait for start
            SuspendAttachment suspend;
            Tasker::NativeMutexHolder lock(pGroup->m_Mutex);
            while (pGroup->m_State == ModsThreadGroup::stateInit)
            {
                const Tasker::NativeCondition::WaitResult ret =
                    pGroup->m_Cond.Wait(pGroup->m_Mutex);
                MASSERT(ret == Tasker::NativeCondition::success);
                if (ret != Tasker::NativeCondition::success)
                {
                    --pGroup->m_NumThreadsAlive;
                    // This may cause a memory leak
                    return;
                }
            }

            // Bail out if the group creation failed
            if ((pGroup->m_State == ModsThreadGroup::stateFailedCreation) ||
                (pGroup->m_State == ModsThreadGroup::stateDestroy))
            {
                return;
            }
        }

        // Run the thread function
        pGroup->m_ThreadFunc(pGroup->m_ThreadArg);

        // Detach the thread from the group
        UINT32 numThreadsRemaining = ~0U;
        {
            Tasker::NativeMutexHolder lock(pGroup->m_Mutex);
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
        unique_ptr<ModsGroupThreadWrapperArg> pInit(new ModsGroupThreadWrapperArg);
        pInit->pGroup = pGroup.get();
        pInit->threadIdx = i;

        // Create unique thread name
        const string threadName = name + Utility::StrPrintf("%u", i);

        // Create the thread
        const ThreadID tid = CreateThread(
                ModsGroupThreadWrapper, pInit.get(), 0, threadName.c_str());
        if (tid == NULL_THREAD)
        {
            // Delete thread init structure for the failed thread
            pInit.reset(0);

            // Destroy previously created threads
            pGroup->SetState(ModsThreadGroup::stateFailedCreation);
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
        // Lock the group's mutex
        Tasker::NativeMutexHolder lock(pGroup->m_Mutex);

        // Set the number of alive threads
        pGroup->m_NumThreadsAlive += numThreads;

        if (bStartGroup)
        {
            // Release all threads in the group
            pGroup->SetState(ModsThreadGroup::stateRunning);
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

    // Lock the group's mutex
    Tasker::NativeMutexHolder lock(pGroup->m_Mutex);
    MASSERT(pGroup->m_State == ModsThreadGroup::stateInit);

    // Release all threads in the group
    pGroup->SetState(ModsThreadGroup::stateRunning);
}

//------------------------------------------------------------------------------
void Tasker::DestroyThreadGroup(void * pThreadGroup)
{
    ModsThreadGroup *pGroup = static_cast<ModsThreadGroup *>(pThreadGroup);
    {
        Tasker::NativeMutexHolder lock(pGroup->m_Mutex);

        // Cannot destroy a thread group in anything but the init state (once it
        // is running, it is owned by tasker
        MASSERT(pGroup->m_State == ModsThreadGroup::stateInit);

        // Destroy all threads in the group
        pGroup->SetState(ModsThreadGroup::stateDestroy);
    }

    Join(pGroup->m_ThreadIds, NO_TIMEOUT);
    delete pGroup;
}

UINT32 Tasker::GetThreadIdx()
{
    return static_cast<UINT32>(
            reinterpret_cast<size_t>(
                Tasker::TlsGetValue(s_pPriv->m_GThreadIdxTlsId)));
}

UINT32 Tasker::GetGroupSize()
{
    ModsThreadGroup* const pGroup = GetLwrGroup();
    return pGroup ? pGroup->m_Size : 1;
}

UINT32 Tasker::GetNumThreadsAliveInGroup()
{
    ModsThreadGroup* const pGroup = GetLwrGroup();
    return pGroup ? pGroup->m_NumThreadsAlive : 1;
}

UINT32 Tasker::GetBarrierReleaseCount()
{
    ModsThreadGroup* const pGroup = GetLwrGroup();
    if (!pGroup)
    {
        return 0;
    }
    // No need to lock the mutex here, because the value of
    // m_BarrierReleaseCount is incremented only when this thread
    // calls WaitOnBarrier(), and before exiting WaitOnBarrier()
    // the thread will call pthread_mutex_unlock(), which serves
    // as a memory barrier, therefore m_BarrierReleaseCount will
    // be up to date and constant until the next WaitOnBarrier().
    return pGroup->m_BarrierReleaseCount;
}

void Tasker::WaitOnBarrier()
{
    // Get thread's group
    ModsThreadGroup* const pGroup = GetLwrGroup();
    if (!pGroup)
    {
        return;
    }

    // Lock the group mutex.
    // This prevents released threads from reentering WaitOnBarrier().
    SuspendAttAndLock lock(pGroup->m_Mutex);

    // Check group state
    if (pGroup->m_State != ModsThreadGroup::stateRunning)
    {
        return;
    }

    // Increment number of waiting threads
    ++pGroup->m_NumThreadsWaiting;

    // If it's the last thread, release the other threads
    if (pGroup->m_NumThreadsWaiting == pGroup->m_NumThreadsAlive)
    {
        pGroup->Release();
    }
    else
    {
        // Allow other Tasker threads to execute while we're waiting
        lock.Suspend();

        // Wait for the barrier to release
        const UINT32 releaseCount = pGroup->m_BarrierReleaseCount;
        while ((releaseCount == pGroup->m_BarrierReleaseCount) &&
               (pGroup->m_State == ModsThreadGroup::stateRunning))
        {
            const Tasker::NativeCondition::WaitResult ret =
                pGroup->m_Cond.Wait(pGroup->m_Mutex);

            // If wait failed (that's a critical error), update group state
            if (Tasker::NativeCondition::success != ret)
            {
                pGroup->SetState(ModsThreadGroup::stateBarrierError);
            }
        }
    }
}

//! Set CPU affinity for the specified thread ID to the CPUs specified
//! by the provided mask
RC Tasker::SetCpuAffinity(ThreadID thread, const vector<UINT32> &cpuMasks)
{
    NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);

    const TaskermPriv::Threads::const_iterator threadIt =
        s_pPriv->m_Threads.find(thread);
    if ((threadIt == s_pPriv->m_Threads.end()) ||
        !threadIt->second ||
        threadIt->second->m_Dead)
    {
        Printf(Tee::PriHigh, "Thread %d does not exist anymore! ", thread);
        return RC::SOFTWARE_ERROR;
    }

    RC rc = NativeThread::SetCpuAffinity(threadIt->second->m_Thread, cpuMasks);
    if (s_bIgnoreCpuAffinityFailure)
    {
        Printf(Tee::PriHigh, "Ignoring SetCpuAffinity failure for thread %d!\n", thread);
        rc.Clear();
    }

    return rc;
}

RC Tasker::Yield()
{
    if (!IsInitialized())
    {
        return RC::SOFTWARE_ERROR;
    }

    s_pPriv->Yield();

    return OK;
}

Tasker::DetachThread::DetachThread()
: m_pLwrThread(nullptr)
{
    MASSERT(IsInitialized());

    // Note: Disable the ability to detach threads in sim MODS.
    // There is a case in RM where RM has pelwliar behavior in SIM_BUILD
    // where RM spins in a tight loop when retrieving workqueue items
    // and only calling ModsDrvYield on a detached thread.  This works
    // in RM because in sim MODS there are no detached threads.
    // This code will need to be cleaned up in the future.  There are
    // probably lots of other cases where we can't detach threads,
    // because for example simulator needs to be run on a separate
    // thread and any GPU accesses through BARs actually access the
    // simulator, so detaching such threads would cause races inside
    // the simulator.
#ifndef SIM_BUILD
    ModsThread* const pLwrThread = GetLwrThread();
    m_pLwrThread = pLwrThread;

    // Increment detach count to support nested detaching
    pLwrThread->m_DetachCount++;

    // If the thread is being detached, release next Tasker thread
    if (pLwrThread->m_DetachCount == 1)
    {
        s_pPriv->Detach(TaskermPriv::dueToDetach);
    }
#endif
}

Tasker::DetachThread::~DetachThread()
{
    MASSERT(IsInitialized());

#ifndef SIM_BUILD
    ModsThread* const pLwrThread = m_pLwrThread;

    MASSERT(GetLwrThread() == pLwrThread);

    // Decrement detach count
    pLwrThread->m_DetachCount--;

    // If the thread is being reattached, wait for its turn
    if (pLwrThread->m_DetachCount == 0)
    {
        s_pPriv->Attach(TaskermPriv::dueToAttach);
    }
#endif
}

Tasker::AttachThread::AttachThread()
: m_pLwrThread(nullptr), m_OrigDetachCount(0)
{
    MASSERT(IsInitialized());

    ModsThread* const pLwrThread = GetLwrThread();

    m_pLwrThread      = pLwrThread;
    m_OrigDetachCount = pLwrThread->m_DetachCount;

    // If the thread is detached, reattach it back
    if (m_OrigDetachCount)
    {
        pLwrThread->m_DetachCount = 0;
        s_pPriv->Attach(TaskermPriv::dueToAttach);
    }
}

Tasker::AttachThread::~AttachThread()
{
    MASSERT(IsInitialized());

    ModsThread* const pLwrThread = m_pLwrThread;

    MASSERT(pLwrThread == GetLwrThread());

    MASSERT(pLwrThread->m_DetachCount == 0);

    // Detach the thread if it was originally in detached state
    if (m_OrigDetachCount)
    {
        pLwrThread->m_DetachCount = m_OrigDetachCount;
        s_pPriv->Detach(TaskermPriv::dueToDetach);
    }
}

void Tasker::ExitThread()
{
    MASSERT(IsInitialized());

    ModsThread* const pLwrThread = GetLwrThread();

    // Don't do anything if this is the main thread
    if (pLwrThread->m_Id == 0)
    {
        MASSERT(!"Attempt to exit from main thread!");
        return;
    }

    Printf(Tee::PriDebug, "Exit thread %s\n", pLwrThread->m_Name.c_str());

    // Destroy JS context
    if (pLwrThread->m_pJsContext)
    {
        JS_DestroyContext(pLwrThread->m_pJsContext);
        pLwrThread->m_pJsContext = 0;
        JavaScriptPtr()->ClearZombies();
    }

    // Print thread run times (attached, detached) before marking thread as dead
    if (pLwrThread->m_DetachCount)
    {
        pLwrThread->RecordTime(TaskermPriv::dueToAttach);
    }
    else
    {
        pLwrThread->RecordTime(TaskermPriv::dueToDetach);
    }
    pLwrThread->PrintStats();

    // Mark the thread as dead
    pLwrThread->m_Dead = true;
    pLwrThread->m_Id = -1;

    // Release the cooperation mutex for other Tasker threads
    if (!pLwrThread->m_DetachCount)
    {
        if (s_pPriv)
        {
            s_pPriv->Detach(TaskermPriv::dueToDetach);
        }
    }
    else
    {
        pLwrThread->m_DetachCount = 0;
    }

    // Kill the thread
    NativeThread::Exit();
}

RC Tasker::Join(ThreadID thread, FLOAT64 timeoutMs)
{
    MASSERT(IsInitialized());

    if (timeoutMs != Tasker::NO_TIMEOUT)
    {
        Printf(Tee::PriDebug, "This implementation of Tasker::Join ignores timeout\n");
    }

    // Allow other Tasker threads to execute while we're waiting
    SuspendAttachment suspend;

    // Find thread which we want to join and remove it from the list
    ModsThread* pThread = 0;
    {
        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);
        const TaskermPriv::Threads::iterator threadIt =
            s_pPriv->m_Threads.find(thread);
        if (threadIt == s_pPriv->m_Threads.end())
        {
            Printf(Tee::PriHigh, "Thread %d does not exist anymore! ", thread);
            Printf(Tee::PriHigh, "Or multiple Joins are being called on it.\n");
            return RC::SOFTWARE_ERROR;
        }
        pThread = threadIt->second;
        if (pThread->m_Joining)
        {
            Printf(Tee::PriHigh,
                   "Attempted Join on Thread %d which is already being joined! ",
                   thread);
            MASSERT(0);
            return RC::SOFTWARE_ERROR;
        }
        pThread->m_Joining = GetLwrThread();
    }

    // Wait for the thread to finish
    NativeThread::Join(pThread->m_Thread);

    {
        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);
        const TaskermPriv::Threads::iterator threadIt =
            s_pPriv->m_Threads.find(thread);
        if (threadIt != s_pPriv->m_Threads.end())
            s_pPriv->m_Threads.erase(threadIt);
    }

    // Delete thread structure
    s_pPriv->m_ThreadMill.Destroy(pThread);
    delete pThread;
    return OK;
}

RC Tasker::Join(const vector<ThreadID>& threads, FLOAT64 timeoutMs)
{
    StickyRC rc;
    for (size_t i=0; i < threads.size(); i++)
    {
        rc = Join(threads[i], timeoutMs);
    }
    return rc;
}

namespace
{
    RC PollCommonSim
    (
        Tasker::PollFunc function,
        volatile void*   arg,
        FLOAT64          timeoutMs,
        const char*      pollingFunctionName,
        bool             obeyHwPollFreqLimit,
        UINT64*          pIterations
    )
    {
        const UINT64 timeoutNs    = static_cast<UINT64>(timeoutMs * 1'000'000.0);
        const UINT64 targetTimeNs = Platform::GetTimeNS() + timeoutNs;

        while (!Platform::PollfuncWrap(function, const_cast<void*>(arg), pollingFunctionName))
        {
            if (Platform::GetTimeNS() >= targetTimeNs)
            {
                return RC::TIMEOUT_ERROR;
            }

            ++*pIterations;

            Tasker::PollHwSleep();
        }

        return RC::OK;
    }

    RC PollCommon
    (
        Tasker::PollFunc function,
        volatile void*   arg,
        FLOAT64          timeoutMs,
        const char*      callingFunctionName,
        const char*      pollingFunctionName,
        bool             obeyHwPollFreqLimit
    )
    {
        RC rc;

        MASSERT(Tasker::IsInitialized());
        MASSERT(function);

        // Print information about polling
        if (callingFunctionName && pollingFunctionName)
        {
            Printf(Tee::PriDebug, "%s polls on %s.\n", callingFunctionName, pollingFunctionName);
        }

        UINT64       iterations = 1;
        const UINT64 startTime  = Xp::GetWallTimeUS();
        UINT64       endTime    = startTime;

        if (IsSimulation())
        {
            rc = PollCommonSim(function,
                               arg,
                               timeoutMs,
                               pollingFunctionName,
                               obeyHwPollFreqLimit,
                               &iterations);

            endTime = Xp::GetWallTimeUS();
        }
        else
        {
            // Early exit
            if (function(const_cast<void*>(arg)))
            {
                return RC::OK;
            }

            // Subsequent calls to the polling function are done from attached thread.
            // This is to reduce CPU usage from polling functions and force all
            // polling functions to effectively use one CPU core.
            Tasker::AttachThread attach;

            // Callwlate absolute time when polling will time out
            const UINT64 destTime =
                (Tasker::NO_TIMEOUT == timeoutMs)
                    ? ~0ULL
                    : (Xp::GetWallTimeUS() + static_cast<UINT64>(timeoutMs*1000));

            FLOAT64 sleepMs   = (s_HwPollSleepNsec == defaultHwPollSleepNs)
                                ? 0.001 : (s_HwPollSleepNsec / 1'000'000.0);
            UINT64  threshold = 100U;

            do
            {
                endTime = Xp::GetWallTimeUS();
                if (endTime >= destTime)
                {
                    rc = RC::TIMEOUT_ERROR;
                    break;
                }

                // Yield to other threads while polling
                if (obeyHwPollFreqLimit)
                {
                    Tasker::Sleep(sleepMs);
                }
                else
                {
                    Tasker::Yield();

                    // After a certain number of iterations, sleep after every call to poll function
                    if (iterations >= threshold)
                    {
                        obeyHwPollFreqLimit = true;
                        sleepMs             = 0.001;
                    }
                }

                ++iterations;
            } while (!function(const_cast<void*>(arg)));
        }

        // Support for Ctrl+C
        if (rc == OK)
        {
            rc = Abort::CheckAndReset();
        }

        // Print information that polling is done
        if (!callingFunctionName)
        {
            callingFunctionName = "func?";
        }
        if (!pollingFunctionName)
        {
            pollingFunctionName = "func?";
        }
        Printf(Tee::PriLow, "%s polled %llu times in %llu us on %s and returned %d.\n",
               callingFunctionName, iterations, endTime - startTime, pollingFunctionName,
               UINT32(rc));

        return rc;
    }
}

RC Tasker::Poll
(
    PollFunc function,
    volatile void* arg,
    FLOAT64 timeoutMs,
    const char* callingFunctionName,
    const char* pollingFunctionName
)
{
    return PollCommon(function, arg, timeoutMs,
            callingFunctionName, pollingFunctionName, false);
}

RC Tasker::PollHw
(
    PollFunc function,
    volatile void* arg,
    FLOAT64 timeoutMs,
    const char* callingFunctionName,
    const char* pollingFunctionName
)
{
    return PollCommon(function, arg, timeoutMs,
            callingFunctionName, pollingFunctionName, true);
}

RC Tasker::SetDefaultTimeoutMs(FLOAT64 TimeoutMs)
{
    s_DefaultTimeoutMs = TimeoutMs;
    s_SimTimeoutMs = TimeoutMs;
    return OK;
}

FLOAT64 Tasker::GetDefaultTimeoutMs()
{
    return IsSimulation() ? s_SimTimeoutMs : s_DefaultTimeoutMs;
}

FLOAT64 Tasker::FixTimeout(FLOAT64 TimeoutMs)
{
    return (IsSimulation() && (TimeoutMs != 0)) ? s_SimTimeoutMs : TimeoutMs;
}

RC Tasker::SetIgnoreCpuAffinityFailure(bool bIgnoreFailure)
{
    s_bIgnoreCpuAffinityFailure = bIgnoreFailure;
    return OK;
}

bool Tasker::GetIgnoreCpuAffinityFailure()
{
    return s_bIgnoreCpuAffinityFailure;
}

//------------------------------------------------------------------------------
// Note: This strangeness is used only in NewWfMats... We should get rid of it.
FLOAT64 Tasker :: UpdateHwPollFreq :: s_SlowestPollHz = 0.0;
INT32 Tasker :: UpdateHwPollFreq :: s_NumTests = 0;
FLOAT64 Tasker :: UpdateHwPollFreq :: s_PretestPollHz = 0.0;

void Tasker::UpdateHwPollFreq :: Apply(FLOAT64 PollHz)
{
    if (s_UpdateHzMutex == NULL)
        s_UpdateHzMutex = Tasker::AllocMutex("Tasker::s_UpdateHzMutex",
                                             Tasker::mtxUnchecked);
    MASSERT(s_UpdateHzMutex);
    Tasker::MutexHolder mh(s_UpdateHzMutex);
    if (s_NumTests < 0)
        s_NumTests = 0;  //Reset the value of s_Numtests if it is invalid
    if (!(s_SlowestPollHz)
            ||
            (PollHz < s_SlowestPollHz))
    {
        s_SlowestPollHz = PollHz;
        if(s_NumTests == 0)
            s_PretestPollHz = GetMaxHwPollHz();
        SetMaxHwPollHz(s_SlowestPollHz);
    }
    s_NumTests++;
    Printf(Tee::PriLow, "Apply: Numtest %u, poll interval: %llu ns, freq = %f Hz\n",
            s_NumTests, s_HwPollSleepNsec, PollHz);
}

Tasker :: UpdateHwPollFreq :: ~UpdateHwPollFreq()
{
    if (s_UpdateHzMutex == NULL)
        s_UpdateHzMutex = Tasker::AllocMutex("Tasker::s_UpdateHzMutex",
                                             Tasker::mtxUnchecked);
    MASSERT(s_UpdateHzMutex);
    Tasker::MutexHolder mh(s_UpdateHzMutex);
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

void Tasker::SetMaxHwPollHz(FLOAT64 Hz)
{
    if (Hz <= 0.0)
    {
        s_HwPollSleepNsec = 0;
    }
    else
    {
        const double sleepNsec = 1'000'000'000.0 / Hz;
        s_HwPollSleepNsec = static_cast<UINT64>(ceil(min(sleepNsec, 1e15)));
    }
}

FLOAT64 Tasker::GetMaxHwPollHz()
{
    const UINT64 sleepNsec = GetHwPollSleepNs();
    return 1e9 / sleepNsec;
}

void Tasker::Sleep(FLOAT64 milliseconds)
{
    // Just yield if no timeout given
    if (milliseconds == NO_TIMEOUT)
    {
        Yield();
        return;
    }

    // Allow other Tasker threads to execute while we're sleeping
    SuspendAttachment suspend;

    // Sleep
    NativeThread::SleepUS(static_cast<UINT64>(milliseconds * 1000));
}

void Tasker::PollHwSleep()
{
    const UINT64 sleepNs = GetHwPollSleepNs();
    if (sleepNs)
    {
        if (IsSimulation())
        {
            AttachThread detach;

            const UINT64 targetTimeNs = Platform::GetTimeNS() + sleepNs;
            do
            {
                Yield();
            } while (Platform::GetTimeNS() < targetTimeNs);
        }
        else
        {
            Sleep(sleepNs / 1'000'000.0);
        }
    }
}

void Tasker::PollMemSleep()
{
    // Limit how fast we poll on BAR1 addresses to avoid saturating the pex bus.
    // Sims where mods back-door accesses FB should not pause.
    if (!IsSimulation() || (s_HwPollSleepNsec != defaultHwPollSleepNs))
    {
        PollHwSleep();
    }
}

ModsEvent* Tasker::AllocEvent(const char *name, bool manualReset)
{
    MASSERT(IsInitialized());
    return new ModsEvent(name,
            manualReset ? ModsEvent::manualResetEvent : ModsEvent::autoResetEvent);
}

ModsEvent* Tasker::AllocSystemEvent(const char* name)
{
    MASSERT(IsInitialized());
    return new ModsEvent(name, ModsEvent::systemEvent);
}

void Tasker::FreeEvent(ModsEvent* pEvent)
{
    MASSERT(IsInitialized());
    delete pEvent;
}

void Tasker::ResetEvent(ModsEvent* pEvent)
{
    MASSERT(IsInitialized());
    pEvent->Reset();
}

void Tasker::SetEvent(ModsEvent* pEvent)
{
    MASSERT(IsInitialized());
    pEvent->Set();
}

bool Tasker::IsEventSet(ModsEvent* pEvent)
{
    MASSERT(IsInitialized());
    return pEvent->IsSet();
}

RC Tasker::WaitOnEvent(ModsEvent* pEvent, FLOAT64 timeoutMs)
{
    MASSERT(IsInitialized());

    // Behave like Yield if there is no event and no timeout
    if (!pEvent && (timeoutMs == NO_TIMEOUT))
    {
        Yield();
        return OK;
    }

    // Behave like Sleep if there is no event
    if (!pEvent)
    {
        Sleep(timeoutMs);
        return OK;
    }

    return pEvent->Wait(timeoutMs);
}

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

    // Detect if we have at least one OS event.
    // Ideally, all events should be of the same type, i.e. either all OS
    // events or all non-OS events. However, life is life.
    UINT32 numOsEvents = 0;
    for (UINT32 i = 0; i < numEvents; i++)
    {
        if (pEvents[i]->IsOsEvent())
        {
            ++numOsEvents;
        }

        // While we do this loop, also check if all events are of type systemEvent.
        // This is not related to the OS events, it's just a global event type which
        // uses one global condition variable.  The WaitOnMultipleEvents() function
        // is valid only for systemEvent event type.  Events of this type are allocated
        // with AllocSystemEvent().
        MASSERT(pEvents[i]->GetType() == ModsEvent::systemEvent);
    }

    // If at least one event is an OS event, we have to go the OS event
    // path.  This uses pipes on Linux and events on Windows. The GetOsEvent()
    // function (without args) will ensure that we will colwert any
    // non-OS events to OS events.
    if (numOsEvents)
    {
        MASSERT(!Xp::HasClientSideResman());

        if (numEvents != numOsEvents)
        {
            Printf(Tee::PriWarn,
                   "Not all events (%u) are OS events (%u), creating OS events for all non-OS events\n",
                   numEvents, numOsEvents);
        }

        vector<void*> osEvents(numEvents);
        for (UINT32 i = 0; i < numEvents; i++)
        {
            osEvents[i] = pEvents[i]->GetOsEvent();
        }

        SuspendAttachment suspend;

        const RC rc = Xp::WaitOsEvents(osEvents.data(), numEvents,
                                       pCompletedIndices, maxCompleted, pNumCompleted,
                                       timeoutMs);
        if (rc == RC::OK)
        {
            for (UINT32 i = 0; i < *pNumCompleted; i++)
            {
                MASSERT(pCompletedIndices[i] < numEvents);
                pEvents[pCompletedIndices[i]]->ForceSet();
            }
        }
        return rc;
    }

    auto& mutex = ModsEvent::GetSystemEventMutex();
    auto& cond  = ModsEvent::GetSystemEventCond();

    SuspendAttAndLock lock(ModsEvent::GetSystemEventMutex());

    const UINT64 endTimeMs = (timeoutMs == NO_TIMEOUT)
            ? 0 : GetPlatformTimeMS(&lock) + static_cast<UINT64>(timeoutMs);

    for (;;)
    {
        UINT32 numCompleted = 0;
        for (UINT32 i = 0; i < numEvents && numCompleted < maxCompleted; i++)
        {
            if (pEvents[i]->CheckAndResetSystemEvent(&lock))
            {
                *pCompletedIndices = i;
                ++pCompletedIndices;
                ++numCompleted;
            }
        }

        if (numCompleted)
        {
            *pNumCompleted = numCompleted;
            break;
        }

        lock.Suspend();

        Tasker::NativeCondition::WaitResult ret;
        if (timeoutMs == NO_TIMEOUT)
        {
            ret = cond.Wait(mutex);
        }
        else
        {
            UINT64 lwrTimeMs = 0;
            {
                lwrTimeMs = GetPlatformTimeMS(&lock);
            }
            if (lwrTimeMs >= endTimeMs)
            {
                return RC::TIMEOUT_ERROR;
            }
            ret = cond.Wait(mutex, static_cast<double>(endTimeMs - lwrTimeMs));
        }

        if (ret == Tasker::NativeCondition::timeout)
        {
            return RC::TIMEOUT_ERROR;
        }
        else if (ret != Tasker::NativeCondition::success)
        {
            return RC::SOFTWARE_ERROR;
        }
    }

    return RC::OK;
}

void* Tasker::GetOsEvent(ModsEvent* pEvent, UINT32 hClient, UINT32 hDevice)
{
    return pEvent->GetOsEvent(hClient, hDevice);
}

void* Tasker::AllocMutex(const char * name, unsigned order)
{
    void* m = new ModsMutex(name, order);
    return m;
}

void Tasker::FreeMutex(void* mutexHandle)
{
    MASSERT(mutexHandle);
    delete static_cast<ModsMutex*>(mutexHandle);
}

void Tasker::AcquireMutex(void* mutexHandle)
{
    MASSERT(mutexHandle);
    static_cast<ModsMutex*>(mutexHandle)->Acquire();
}

bool Tasker::TryAcquireMutex(void* mutexHandle)
{
    MASSERT(mutexHandle);
    return static_cast<ModsMutex*>(mutexHandle)->TryAcquire();
}

void Tasker::ReleaseMutex(void* mutexHandle)
{
    MASSERT(mutexHandle);
    static_cast<ModsMutex*>(mutexHandle)->Release();
}

bool Tasker::CheckMutex(void* mutexHandle)
{
    MASSERT(mutexHandle);
    return static_cast<ModsMutex*>(mutexHandle)->IsMutexLocked();
}

bool Tasker::CheckMutexOwner(void* mutexHandle)
{
    MASSERT(mutexHandle);
    return static_cast<ModsMutex*>(mutexHandle)->IsMutexLockedByLwrThread();
}

namespace
{
    struct PollAndAcquireMutexPollArgs
    {
        Tasker::PollFunc pollFunc;
        volatile void* arg;
        void* mutexHandle;
    };

    // Polling function used by PollAndAcquireMutex and PollHwAndAcquireMutex
    bool PollAndAcquireMutexPollFunc(void* arg)
    {
        PollAndAcquireMutexPollArgs* const pPollArgs =
            static_cast<PollAndAcquireMutexPollArgs*>(arg);
        if (Tasker::TryAcquireMutex(pPollArgs->mutexHandle))
        {
            if (pPollArgs->pollFunc(const_cast<void*>(pPollArgs->arg)))
            {
                return true;
            }
            Tasker::ReleaseMutex(pPollArgs->mutexHandle);
        }
        return false;
    }
}

RC Tasker::PollAndAcquireMutex
(
    Tasker::PollFunc pollFunc,
    volatile void* arg,
    void* mutexHandle,
    FLOAT64 timeoutMs
)
{
    PollAndAcquireMutexPollArgs pollArgs;
    pollArgs.pollFunc = pollFunc;
    pollArgs.arg = arg;
    pollArgs.mutexHandle = mutexHandle;
    return Poll(PollAndAcquireMutexPollFunc, &pollArgs, timeoutMs);
}

RC Tasker::PollHwAndAcquireMutex
(
    Tasker::PollFunc pollFunc,
    volatile void* arg,
    void* mutexHandle,
    FLOAT64 timeoutMs
)
{
    PollAndAcquireMutexPollArgs pollArgs;
    pollArgs.pollFunc = pollFunc;
    pollArgs.arg = arg;
    pollArgs.mutexHandle = mutexHandle;
    return PollHw(PollAndAcquireMutexPollFunc, &pollArgs, timeoutMs, 0, 0);
}

struct ModsSemaphore
{
public:
    ModsSemaphore(INT32 count, const char* name);
    ~ModsSemaphore();
    RC Acquire(FLOAT64 timeoutMs);
    void Release();
    const char* GetName() const { return m_Name.c_str(); }

private:
    INT32 m_Count;
    const string m_Name;
    Tasker::NativeMutex m_Mutex;
    Tasker::NativeCondition m_Cond;
};

ModsSemaphore::ModsSemaphore(INT32 count, const char* name)
: m_Count(count)
, m_Name(name)
{
}

ModsSemaphore::~ModsSemaphore()
{
}

RC ModsSemaphore::Acquire(FLOAT64 timeoutMs)
{
    // Lock the semaphore mutex
    SuspendAttAndLock lock(m_Mutex);

    // Wait until the semaphore has a non-zero count
    if (m_Count == 0)
    {
        // Allow other Tasker threads to execute while we're waiting
        lock.Suspend();

        while (m_Count == 0)
        {
            const Tasker::NativeCondition::WaitResult ret =
                (timeoutMs != Tasker::NO_TIMEOUT)
                    ? m_Cond.Wait(m_Mutex, timeoutMs)
                    : m_Cond.Wait(m_Mutex);
            if (Tasker::NativeCondition::timeout == ret)
            {
                return RC::TIMEOUT_ERROR;
            }
            if (Tasker::NativeCondition::success != ret)
            {
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    // Decrement the count
    MASSERT(m_Count > 0);
    m_Count--;

    return OK;
}

void ModsSemaphore::Release()
{
    SuspendAttAndLock lock(m_Mutex);
    m_Count++;
    m_Cond.Signal();
}

//------------------------------------------------------------------------------
Tasker::SpinLock::SpinLock(volatile UINT32 *pSpinLock) : m_pSpinLock(pSpinLock)
{
    MASSERT(m_pSpinLock);
    if (Tasker::IsInitialized())
    {
        while (Cpu::AtomicXchg32(m_pSpinLock, 1))
        {
            Tasker::Yield();
        }
    }
    else
    {
        if (Cpu::AtomicXchg32(m_pSpinLock, 1))
        {
            printf("Relwrsive spinlock!\n");
            Xp::BreakPoint();
        }
    }
}

//------------------------------------------------------------------------------
Tasker::SpinLock::~SpinLock()
{
    // We assume that the atomic write operation is implemented with
    // sequentially consistent memory ordering.
    //
    // A sequentially consistent memory ordering guarantees that no load
    // or store will be reordered before or after this store, neither
    // by the compiler nor by hardware.
    //
    // The above guarantee only applies to other atomic operations with
    // sufficiently similar memory ordering.  Other non-atomic memory accesses
    // can still be reordered and require a sufficient barrier (thread fence).
    Cpu::AtomicWrite(reinterpret_cast<volatile INT32*>(m_pSpinLock), 0);
}

SemaID Tasker::CreateSemaphore(INT32 initialCount, const char* name)
{
    MASSERT(IsInitialized());
    MASSERT(initialCount >= 0);
    return new ModsSemaphore(initialCount, name ? name : "");
}

RC Tasker::AcquireSemaphore(SemaID semaId, FLOAT64 timeoutMs)
{
    MASSERT(IsInitialized());
    MASSERT(semaId);
    return semaId->Acquire(timeoutMs);
}

void Tasker::ReleaseSemaphore(SemaID semaId)
{
    MASSERT(IsInitialized());
    MASSERT(semaId);
    semaId->Release();
}

void Tasker::DestroySemaphore(SemaID semaId)
{
    MASSERT(semaId);
    delete semaId;
}

const char* Tasker::GetSemaphoreName(SemaID semaId)
{
    MASSERT(semaId);
    return semaId->GetName();
}

struct Tasker::RmSpinLock
{
public:
    RmSpinLock() : lock(0) {};
    ~RmSpinLock() {};
    volatile UINT32 lock;
};

void *Tasker::AllocRmSpinLock()
{
    Tasker::RmSpinLock *lock = new Tasker::RmSpinLock;
    if (!lock)
    {
        MASSERT(!"Cannot alloc new spinlock!\n");
    }
    return lock;
}

void Tasker::AcquireRmSpinLock(SpinLockID lock)
{
    MASSERT(lock);
    MASSERT(Tasker::IsInitialized());

    // Detach the thread while we acquire the lock
    SuspendAttachment suspend;

    while (!Cpu::AtomicCmpXchg32(&lock->lock, 0, 1));
}

void Tasker::ReleaseRmSpinLock(SpinLockID lock)
{
    MASSERT(lock);
    MASSERT(Tasker::IsInitialized());

    Cpu::AtomicXchg32(&lock->lock, 0);
}

void Tasker::DestroyRmSpinLock(SpinLockID lock)
{
    MASSERT(lock);
    delete lock;
}

bool Tasker::IsInitialized()
{
    return s_pPriv != 0;
}

UINT32 Tasker::Threads()
{
    MASSERT(IsInitialized());

    NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);
    UINT32 count = 0;
    TaskermPriv::Threads::const_iterator threadIt = s_pPriv->m_Threads.begin();
    for ( ; threadIt != s_pPriv->m_Threads.end(); ++threadIt)
    {
        MASSERT(threadIt->second);
        if (!threadIt->second->m_Dead)
        {
            count++;
        }
    }
    return count;
}

Tasker::ThreadID Tasker::LwrrentThread()
{
    MASSERT(IsInitialized());
    return GetLwrThread()->m_Id;
}

void* Tasker::OsLwrrentThread()
{
    return GetLwrThread();
}

JSContext* Tasker::GetLwrrentJsContext()
{
    MASSERT(IsInitialized());
    return GetLwrThread()->m_pJsContext;
}

JSContext * Tasker::GetJsContext(Tasker::ThreadID threadId)
{
    MASSERT(IsInitialized());
    {
        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);
        auto it = s_pPriv->m_Threads.find(threadId);
        return s_pPriv->m_Threads.end() == it ? nullptr : it->second->m_pJsContext;
    }
}

void Tasker::SetLwrrentJsContext(JSContext* pContext)
{
    MASSERT(IsInitialized());
    ModsThread* const pThread = GetLwrThread();
    if (pThread->m_pJsContext)
    {
        JS_DestroyContext(pThread->m_pJsContext);
        JavaScriptPtr()->ClearZombies();
    }
    pThread->m_pJsContext = pContext;
}

void Tasker::DestroyJsContexts()
{
    MASSERT(IsInitialized());

    {
        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);

        TaskermPriv::Threads::const_iterator threadIt = s_pPriv->m_Threads.begin();
        for ( ; threadIt != s_pPriv->m_Threads.end(); ++threadIt)
        {
            MASSERT(threadIt->second);
            if (threadIt->second->m_pJsContext)
            {
                JS_DestroyContext(threadIt->second->m_pJsContext);
                threadIt->second->m_pJsContext = 0;
            }
        }
    }

    JavaScriptPtr()->ClearZombies();
}

const char* Tasker::ThreadName(ThreadID thread)
{
    MASSERT(IsInitialized());

    NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);

    const TaskermPriv::Threads::const_iterator threadIt =
        s_pPriv->m_Threads.find(thread);
    if ((threadIt == s_pPriv->m_Threads.end()) ||
        !threadIt->second ||
        threadIt->second->m_Dead)
    {
        return "";
    }
    return threadIt->second->m_Name.c_str();
}

namespace
{
    struct ThreadInfo
    {
        unsigned id;
        string name;
        const char* status;
    };
}

void Tasker::PrintAllThreads(INT32 Priority)
{
    MASSERT(IsInitialized());

    vector<ThreadInfo> threadInfos;

    {
        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);

        threadInfos.reserve(s_pPriv->m_Threads.size());
        TaskermPriv::Threads::const_iterator threadIt = s_pPriv->m_Threads.begin();
        for ( ; threadIt != s_pPriv->m_Threads.end(); ++threadIt)
        {
            ModsThread* const pThread = threadIt->second;
            if (pThread)
            {
                ThreadInfo info;
                info.id = threadIt->first;
                info.name = pThread->m_Name;
                if (pThread->m_Dead)
                {
                    info.status = "dead";
                }
                else if (pThread->m_DetachCount)
                {
                    info.status = "detached";
                }
                else
                {
                    info.status = "attached";
                }
                threadInfos.push_back(info);
            }
        }
    }

    Printf(Priority, "Threads:\n");
    for (UINT32 i=0; i < threadInfos.size(); i++)
    {
        Printf(Priority, "   %d \"%s\" %s\n",
                threadInfos[i].id,
                threadInfos[i].name.c_str(),
                threadInfos[i].status);
    }
}

vector<Tasker::ThreadID> Tasker::GetAllThreads()
{
    MASSERT(IsInitialized());
    vector<ThreadID> threadIds;
    {
        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);
        boost::transform(
            s_pPriv->m_Threads,
            back_inserter(threadIds),
            [](auto t) { return t.first; }
        );
    }
    return threadIds;
}

vector<Tasker::ThreadID> Tasker::GetAllAttachedThreads()
{
    MASSERT(IsInitialized());
    vector<ThreadID> threadIds;
    {
        using namespace boost::adaptors;

        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);
        boost::transform(
            s_pPriv->m_Threads | filtered([](auto t) { return 0 == t.second->m_DetachCount; }),
            back_inserter(threadIds),
            [](auto t) { return t.first; }
        );
    }
    return threadIds;
}

UINT32 Tasker::TlsAlloc(bool bInherit)
{
    MASSERT(IsInitialized());

    NativeMutexHolder lock(s_pPriv->m_TlsMutex);

    const UINT32 tlsIndex = NativeTlsValue::Alloc();

    if (bInherit)
    {
        s_pPriv->m_TlsInherited.insert(tlsIndex);
    }

    return (UINT32)tlsIndex;
}

void Tasker::TlsFree(UINT32 tlsIndex)
{
    MASSERT(IsInitialized());

    NativeMutexHolder lock(s_pPriv->m_TlsMutex);

    set<UINT32>::iterator tlsIt = s_pPriv->m_TlsInherited.find(tlsIndex);
    if (tlsIt != s_pPriv->m_TlsInherited.end())
    {
        s_pPriv->m_TlsInherited.erase(tlsIt);
    }

    NativeTlsValue::Free(tlsIndex);
}

void Tasker::TlsSetValue(UINT32 tlsIndex, void* value)
{
    NativeTlsValue::Set(tlsIndex, value);
}

void* Tasker::TlsGetValue(UINT32 tlsIndex)
{
    return NativeTlsValue::Get(tlsIndex);
}

bool Tasker::IsValidThreadId(ThreadID id)
{
    MASSERT(IsInitialized());

    NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);

    const TaskermPriv::Threads::const_iterator threadIt =
        s_pPriv->m_Threads.find(id);
    return (threadIt != s_pPriv->m_Threads.end()) &&
           threadIt->second &&
           !threadIt->second->m_Dead;
}

int Tasker::StackBytesFree()
{
    return 1;
}

void Tasker::EnableThreadStats()
{
#ifdef THREAD_STATISTICS
    s_ThreadStats = true;
#endif
}

void Tasker::PrintThreadStats()
{
    if (s_ThreadStats)
    {
        DetachThread detach;

        ModsThread* const pLwrThread = GetLwrThread();
        pLwrThread->PrintStats();
        pLwrThread->ResetStats();
    }
}

namespace Tasker
{
    void PrintThreads();
}

void Tasker::PrintThreads()
{
    if (!IsInitialized())
    {
        Printf(Tee::PriNormal, "Tasker not initialized, no threads to print\n");
        return;
    }

    // Inner scope to release mutices before flushing Tee queues
    {
        NativeMutexHolder lock(s_pPriv->m_ThreadStructsMutex);

        if (s_pPriv->m_Threads.empty())
        {
            Printf(Tee::PriNormal, "No threads in Tasker\n");
            return;
        }

        Printf(Tee::PriNormal, "Tasker threads:\n");

        Private::NativeLock threadMillLock(s_pPriv->m_ThreadMill.GetMutex());

        ModsThread* const pLwrThread = GetLwrThread();

        ModsThread* const pActiveAttachedThread = static_cast<ModsThread*>(
                s_pPriv->m_ThreadMill.GetLwrrentAttachedThread());

        const auto GetAttachState = [](const ModsThread* thread) -> char
        {
            static const char states[] = { 'D', 'A', 'Y', 'W' };
            const unsigned state = static_cast<unsigned>(thread->m_State);
            return state < sizeof(states) ? states[state] : '?';
        };

        for (const auto& threadIt : s_pPriv->m_Threads)
        {
            const int               tid    = threadIt.first;
            const ModsThread* const thread = threadIt.second;

            MASSERT((thread == pActiveAttachedThread) == thread->IsActive());

            string status = Utility::StrPrintf("  %3d %s %c%c%c \"%s\"%s",
                    tid,
                    thread->m_DetachCount ? "DETACHED" : "attached",
                    thread == pLwrThread ? 'C' : ' ',
                    thread == pActiveAttachedThread ? 'H' : ' ',
                    GetAttachState(thread),
                    thread->m_Name.c_str(),
                    thread->m_Dead ? " dead" : "");

            if (thread->m_Joining)
            {
                status += Utility::StrPrintf(" (being joined by thread %d)\n",
                          thread->m_Joining->m_Id);
            }

            if (s_ThreadStats)
            {
                status += Utility::StrPrintf(", attached %llu ns, detached %llu ns, watiing %llu ns",
                        thread->m_TimeAttachedNs, thread->m_TimeDetachedNs, thread->m_TimeWaitingNs);
            }

            Printf(Tee::PriNormal, "%s\n", status.c_str());
        }
    }

    Tee::FlushToDisk();
}
