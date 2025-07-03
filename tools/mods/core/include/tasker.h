/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2019 by LWPU Corporation.  All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   tasker.h
 * @brief  Header file for the Tasker class.
 */

#pragma once

#ifndef INCLUDED_TASKER_H
#define INCLUDED_TASKER_H

#include "massert.h"
#include "rc.h"
#include "tee.h"
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

// Old 16-bit windows had a Yield function, modern windows does not,
// and the windows header files have an empty Yield macro to help
// nasty old windows code compile.  Of course that breaks mods...
#if defined(Yield)
 #undef Yield
#endif

// #define DO_STACK_CHECKS 1

#if defined(DO_STACK_CHECKS)
    #define STACK_CHECK()                                           \
        do {                                                        \
            if (Tasker::IsInitialized() &&                          \
                Tasker::StackBytesFree() < 1)                       \
            {                                                       \
                ModsAssertFail(__FILE__, __LINE__,                  \
                               MODS_FUNCTION, "stack overflow!");   \
            }                                                       \
        } while (0)
#else
    #define STACK_CHECK()
#endif

// Files:
//   include/tasker.h
//       Public interface
//   include/tasker_p.h
//       Private interfaces, for use by tasker.cpp and ostasker.cpp.
//   main/tasker.cpp
//       Portable parts of the implementation.
//   [platform]/ostasker.cpp
//       Non-portable parts of the implementation.

class TaskerPriv;
class ModsEvent;
struct JSContext;

//
// Tasker class definition
//

/**
 * Cooperative round robin task switcher and event handler.
 *
 * The Tasker class provides a global mechanism for the creation and
 * manipulation of threads and events.  For this system to work, all
 * threads must be programmed to "play nice" with each other, essentially
 * by inserting Yield calls in the correct places, and at reasonable
 * intervals.
 *
 * Here's a diagram that shows the life-cycle of a thread:
 * @image html thrlife.png
 *
 * Here's the terminology these docs use when talking about threads:
 * - "Active/Inactive"
 *    - An "active" thread is either lwrrently running or in the
 *      queue to get processor time.  A call to Yield() will yield
 *      the processor to the next "active" thread, and will put
 *      the current thread at the back of the queue to get
 *      processor time.
 *    - An "inactive" thread is blocking on some event and is not
 *      in the queue to get processor time.  Calls to Yield() will
 *      never give CPU time to an inactive thread.
 *
 * - "Dead/Alive"
 *    - An "alive" thread is one whose control function is still
 *      running.  Alive threads may be active or inactive.
 *    - A "dead" thread is one whose control function has reached
 *      its end and returned.  A dead thread will never again get
 *      CPU time, and its resources will be reclaimed when another
 *      thread calls Join on its ID, or when the program exits.
 *
 * - The act of a thread changing from active to inactive and
 *   waiting on something to wake it back up is called "blocking."
 */

struct ModsSemaphore;
typedef ModsSemaphore * SemaID;

class ModsCondition
{
public:
    ModsCondition();
    ~ModsCondition();
    RC Wait(void *mutex);
    RC Wait(void *mutex, FLOAT64 TimeoutMs);
    void Signal();
    void Broadcast();
    ModsCondition(const ModsCondition&)             = delete;
    ModsCondition& operator=(const ModsCondition&)  = delete;

    ModsCondition(ModsCondition&&);
    ModsCondition& operator=(ModsCondition&&);
private:
    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};

//! Polling macro that supplies useful debug info for error messages.
#define POLLWRAP(pollfunc, pollargs, timeoutMs)   \
    Tasker::Poll((pollfunc), (pollargs),          \
                 Tasker::FixTimeout(timeoutMs),   \
                 __FUNCTION__, #pollfunc)

#define POLLWRAP_HW(pollfunc, pollargs, timeoutMs)      \
    Tasker::PollHw((pollfunc), (pollargs),              \
                   Tasker::FixTimeout(timeoutMs),       \
                   __FUNCTION__, #pollfunc)

namespace Tasker
{
    class ModsThread;

    struct RmSpinLock;
    typedef RmSpinLock * SpinLockID;

    typedef void (* ThreadFunc)(void *);
    typedef bool (* PollFunc)(void *);

    typedef INT32 ThreadID;
    typedef ModsEvent * EventID;

    /** @name Constants   @{*/

    const FLOAT64 NO_TIMEOUT = -1.0;
        //!< Use this to wait forever for an event.
    const UINT32 MIN_STACK_SIZE = 1024 * 1024;

    // It would be nicer if this was zero, but that's more work for now.
    // At least with this constant we can easily change it to zero later.
    const ThreadID NULL_THREAD = -1;
    /**@}*/

    const ThreadID ISR_THREADID = -2;

    RC Initialize();
        //!< Initialize tasker data structures, start threads, install ISRs, etc.
    void Cleanup();
        //!< Free memory, shut down threads, remove ISRs, etc.
        //!< Call this only from the main thread!

    bool CanThreadYield();
    void SetCanThreadYield(bool canYieldThread);

    /** @name Thread Manipulators  @{*/

    ThreadID CreateThread
    (
        ThreadFunc     Function,
        void *         Args,
        UINT32         StackSize,
        const char *   Name
    );
        //!< Create a new thread.

    // Helper class for handling lambda objects with CreateThread.
    // Lambdas are functor objects so we pass them on the heap to
    // the thread, hence the use of unique_ptr.
    template<typename Func>
    class ThreadFuncHolder
    {
        public:
            template<typename T>
            explicit ThreadFuncHolder(T&& f)
            : m_pFunc(make_unique<Func>(forward<T>(f)))
            {
            }
            explicit ThreadFuncHolder(void* opaqueFuncPtr)
            : m_pFunc(static_cast<Func*>(opaqueFuncPtr))
            {
            }
            operator void*() const
            {
                return m_pFunc.get();
            }
            void operator()()
            {
                (*m_pFunc)();
            }
            void Release()
            {
                m_pFunc.release();
            }

        private:
            unique_ptr<Func> m_pFunc;
    };

    // Specialization of the above helper class for function pointers.
    // We can just pass function pointers through the native CreateThread
    // interface without the need to use unique_ptr.
    template<typename T>
    class ThreadFuncHolder<T()>
    {
        public:
            typedef T (*Func)();
            explicit ThreadFuncHolder(Func f)
            : m_pFunc(f)
            {
            }
            explicit ThreadFuncHolder(void* opaqueFuncPtr)
            : m_pFunc(static_cast<Func>(opaqueFuncPtr))
            {
            }
            operator void*() const
            {
                return static_cast<void*>(m_pFunc);
            }
            void operator()()
            {
                m_pFunc();
            }
            void Release()
            {
            }

        private:
            Func m_pFunc;
    };

    //! Create a new thread.
    //!
    //! Can be used to create a thread from any kind of function
    //! object, including function pointers, lambdas and functors.
    //! The function object is exelwted on the created thread.
    //!
    //! For function pointers we just pass the pointer to the thread.
    //! For lambdas and functors, we create a copy of them on the heap
    //! and pass it to the thread (or release if thread creation failed).
    //!
    //! The function object is passed by value, so the lambda or
    //! functor passed can be a named const variable.  For lambdas created
    //! in-place during the invocation, copy elision kicks in so we
    //! don't actually make a copy of it.  When allocating a "copy" on
    //! the heap we move the function object to avoid an actual copy.
    template<typename Func>
    ThreadID CreateThread(const char* name, Func func)
    {
        // Create a copy of the function object.  For lambdas
        // and functor objects we pass them to the thread on the
        // heap, hence the use of unique_ptr to guard allocations.
        using FuncHolder = ThreadFuncHolder<Func>;
        FuncHolder funcHolder(move(func));

        const ThreadID tid = CreateThread(
                [](void* opaqueFuncPtr)->void
                {
                    // Take ownership of the function object.
                    // For lambdas and functors this uses unique_ptr
                    // to free the memory and the function object when
                    // the thread exits.
                    FuncHolder funcHolder(opaqueFuncPtr);
                    funcHolder();
                },
                funcHolder, MIN_STACK_SIZE, name);

        // If thread creation succeeded, prevent the holder object from
        // freeing the copy of the function object, which we allocted on
        // the heap, because the created thread now owns it.
        if (tid != NULL_THREAD)
        {
            funcHolder.Release();
        }
        return tid;
    }

    vector<ThreadID> CreateThreadGroup
    (
        ThreadFunc     Function,
        void *         Args,
        UINT32         NumThreads,
        const char *   Name,
        bool           bStartGroup,
        void **        ppvThreadGroup
    );
        //!< Create a batch of new threads.
        //!<
        //!< The threads created will share a common barrier.
        //!< @param Function Thread function to be used for all created threads.
        //!< @param Args Argument passed to all created threads.
        //!< @param NumThreads Number of threads to create.
        //!< @param Name Name for the threads (zero-based thread index will be appended).
        //!< @param bStartGroup True if the group should be started after creation
        //!< @param ppvThreadGroup Pointer to return thread group pointer
        //!< @return Returns a vector of thread IDs. The size of the vector is equal
        //!<         to the number of threads requested. If there was an error when
        //!<         creating threads, the returned vector is empty.
        //!< @sa Barrier

    template<typename Func>
    vector<ThreadID> CreateThreadGroup
    (
        const char* name,
        UINT32      numThreads,
        bool        bStartGroup,
        void**      ppvThreadGroup,
        Func        func
    )
    {
        return CreateThreadGroup(
                [](void* opaqueFuncPtr)->void
                {
                    (*static_cast<Func*>(opaqueFuncPtr))();
                },
                &func, numThreads, name, bStartGroup, ppvThreadGroup);
    }

    void StartThreadGroup(void * pThreadGroup);
        //!< Start a thread group.
        //!<
        //!< @param pThreadGroup Thread group to start.
        //!< @sa Barrier

    void DestroyThreadGroup(void * pThreadGroup);
        //!< Destroy a thread group.
        //!<
        //!< @param pThreadGroup Thread group to destroy.
        //!< @sa Barrier

    //! RAII class that either starts or destroys a thread group
    class StartOrDestroyThreadGroup
    {
    public:
        StartOrDestroyThreadGroup(void *pvThreadGroup)
            : m_pvThreadGroup(pvThreadGroup)
             ,m_bStartThreadGroup(false)
        {
        }
        ~StartOrDestroyThreadGroup()
        {
            if (m_bStartThreadGroup)
                Tasker::StartThreadGroup(m_pvThreadGroup);
            else
                Tasker::DestroyThreadGroup(m_pvThreadGroup);
        }
        void SetStart(bool bStart) { m_bStartThreadGroup = bStart; }
    public:
        void *m_pvThreadGroup;
        bool  m_bStartThreadGroup;
    };

    UINT32 GetThreadIdx();
        //!< Return index of the current thread within the group.
        //!<
        //!< If the thread is not part of a group, the function will return 0.
        //!< @sa CreateThreadGroup

    UINT32 GetGroupSize();
        //!< Return size of the group the thread is part of.
        //!<
        //!< If the thread is not part of a group, the function will return 1.
        //!< @sa CreateThreadGroup

    UINT32 GetNumThreadsAliveInGroup();
        //!< Return count of alive threads in a group, the thread is part of.
        //!<
        //!< If the thread is not part of a group, the function will return 1.
        //!< @sa CreateThreadGroup

    UINT32 GetBarrierReleaseCount();
        //!< Returns number of times the group barrier was released.
        //!<
        //!< If the thread is not part of a group, the function will return 0.
        //!< @sa CreateThreadGroup

    RC SetCpuAffinity(ThreadID thread, const vector<UINT32> &cpuMasks);
        //!< Set CPU affinity for the specified thread ID to the CPUs specified
        //!< by the provided mask

    RC Yield();
        //!< Cause the current thread to yield the processor.

    //! Detaches the current thread from the Tasker for the duration of
    //! the class' object existence. Used to indicate that a section of
    //! a thread or a scope is multi-thread safe.
    class DetachThread
    {
    public:
        DetachThread();
        ~DetachThread();
        DetachThread(const DetachThread&)            = delete;
        DetachThread& operator=(const DetachThread&) = delete;

#ifdef USE_NEW_TASKER
    private:
        // Cache thread between constructor and destructor to avoid slow TLS access.
        ModsThread* m_pLwrThread;
#endif
    };

    //! Attaches current thread to the Tasker for the duration of the object's lifetime.
    //!
    //! Some specific sections of code in MODS are not thread-safe and this allows us
    //! to liberate other more sections of code from the Tasker without worrying
    //! that something inside will cause trouble.
    class AttachThread
    {
    public:
        AttachThread();
        ~AttachThread();
        AttachThread(const AttachThread&)            = delete;
        AttachThread& operator=(const AttachThread&) = delete;

#ifdef USE_NEW_TASKER
    private:
        // Cache thread between constructor and destructor to avoid slow TLS access.
        ModsThread* m_pLwrThread;
        int         m_OrigDetachCount;
#endif
    };

    void ExitThread();
        //!< Cause the current thread to die.
        //!< This is called automatically when the thread func exits.

    RC Join
    (
        ThreadID    thread,
        FLOAT64     timeoutMs = NO_TIMEOUT
    );
        //!< Cause the current thread to block until another thread dies.
        //!< This is the only way the dead thread's resources get freed, so
        //!< please, always Join your threads!

    RC Join
    (
        const vector<ThreadID>& threads,
        FLOAT64                 timeoutMs = NO_TIMEOUT
    );
        //!< Cause the current thread to block until all threads on the list die.
        //!< This is the only way the dead threads' resources get freed, so
        //!< please, always Join your threads!

    RC WaitOnEvent
    (
        ModsEvent*  pEvent,
        FLOAT64     timeoutMs = NO_TIMEOUT
    );
        //!< Cause the current thread to block inactive until an event is set.
        //!< Use a null pointer for pEvent if you just want to wait for the timeout

    //! Block current thread until any event from the list is set.
    //!
    //! This works *only* with events allocated with AllocSystemEvent().
    //!
    //! @param pEvents           Array of events to wait on.
    //! @param numEvents         Number of events to wait on in the pEvents array.
    //! @param pCompletedIndices Output: Array of indices which tells which events were
    //!                          found to be set.  The indices correspond to the pEvents
    //!                          array.  Events which were triggered and are reported
    //!                          by this function are atomically reset.
    //! @param maxCompleted      Maximum number of indices that can be held in the
    //!                          pCompletedIndices array.  This must be at least 1
    //!                          and it can be less than numEvents.  In any case,
    //!                          up to maxCompleted events are retrieved by this
    //!                          function and the remaining events remain unaffected.
    //! @param pNumCompleted     Output: Number of events that were found set.
    //!                          This is also the number of indices written into the
    //!                          pCompletedIndices array.  If there are more than
    //!                          maxCompleted events set, the remaining unreported events
    //!                          remain in set state and can be queried by a subsequent
    //!                          call to this function.
    //! @param timeoutMs         Timeout for the wait.
    RC WaitOnMultipleEvents
    (
        ModsEvent** pEvents,
        UINT32      numEvents,
        UINT32*     pCompletedIndices,
        UINT32      maxCompleted,
        UINT32*     pNumCompleted,
        FLOAT64     timeoutMs = NO_TIMEOUT
    );

    RC Poll
    (
        PollFunc Function,
        volatile void * pArgs,
        FLOAT64 TimeoutMs,
        const char * callingFunctionName = 0, //!< optional, for debug spew
        const char * pollingFunctionName = 0  //!< optional, for debug spew
    );
        //!< Poll 'Function' with 'pArgs' arguments until it returns true
        //!< or times out after 'TimeoutMs'. A timeout error will be
        //!< returned if Poll() times out.

    RC PollHw
    (
        PollFunc Function,
        volatile void * pArgs,
        FLOAT64 TimeoutMs,
        const char * callingFunctionName, //!< for debug spew
        const char * pollingFunctionName  //!< for debug spew
    );
        //!< Poll 'Function' with 'pArgs' arguments until it returns true
        //!< or times out after 'TimeoutMs'. A timeout error will be
        //!< returned if Poll times out.
        //!< Stays below the optional max polling frequency from -poll_hw_hz F

    FLOAT64 FixTimeout(FLOAT64 TimeoutMs);
        //!< Return TimeoutMs on HW, and NO_TIMEOUT on simulation

    template<typename Func>
    RC Poll
    (
        FLOAT64     timeoutMs,
        Func        func,
        const char* callingFunctionName = nullptr,
        const char* pollingFunctionName = "unnamed function"
    )
    {
        return Poll(
            [](void* opaqueFuncPtr)->bool
            {
                return (*static_cast<Func*>(opaqueFuncPtr))();
            },
            &func, FixTimeout(timeoutMs),
            callingFunctionName, pollingFunctionName);
    }

    template<typename Func>
    RC PollHw
    (
        FLOAT64     timeoutMs,
        Func        func,
        const char* callingFunctionName = nullptr,
        const char* pollingFunctionName = "unnamed function"
    )
    {
        return PollHw(
            [](void* opaqueFuncPtr)->bool
            {
                return (*static_cast<Func*>(opaqueFuncPtr))();
            },
            &func, FixTimeout(timeoutMs),
            callingFunctionName, pollingFunctionName);
    }

    RC SetDefaultTimeoutMs(FLOAT64 TimeoutMs);
    FLOAT64 GetDefaultTimeoutMs();

    RC SetIgnoreCpuAffinityFailure(bool bIgnoreFailure);
    bool GetIgnoreCpuAffinityFailure();

    void SetMaxHwPollHz(FLOAT64 Hz);
    FLOAT64 GetMaxHwPollHz();

    //! Put the current thread to sleep for a given number of milliseconds, giving
    //! up its CPU time to other threads.
    //! Warning, this function is not intented for precise delays; use
    //! Utility::Delay() instead.
    void Sleep(FLOAT64 Milliseconds);

    //! Put the current thread to sleep for the current PollHw sleep time.  No sleep
    //! will be present if the current PollHw sleep time is 0.
    void PollHwSleep();
    //! Put the current thread to sleep for the current PollHw sleep time.  No sleep
    //! will be present if the current PollHw sleep time is 0, also there will be
    //! no sleep on sims.
    void PollMemSleep();
    /**@}*/

    /** @name Event Manipulators  @{*/
    ModsEvent * AllocEvent  (const char *name = 0, bool ManualReset = true);
    ModsEvent * AllocSystemEvent(const char* name = nullptr);
    void        FreeEvent   (ModsEvent* pEvent);
    void        ResetEvent  (ModsEvent* pEvent);
    void        SetEvent    (ModsEvent* pEvent);
    /**@}*/

    //! Get the appropriate pointer for use with LwRmAllocEvent.
    //! For DOS and SIM, the client side RM will directly set a ModsEvent,
    //! so this just returns the pEvent.
    //! For win32, the kernel RM can set a win32 OS event object, and this
    //! function will return the OS event that is secretly inside the ModsEvent.
    //! Returns NULL on platforms where it is not possible to hook a
    //! GPU interrupt to an application event.
    void *      GetOsEvent(ModsEvent* pEvent, UINT32 hClient, UINT32 hDevice);

      /** @name Event Accessors  @{*/
    bool IsEventSet (ModsEvent* pEvent);
      /**@}*/

    //! Deadlock recipe:
    //!   Thread A locks mutex AA, and B locks BB,
    //!   then A blocks acquiring BB, and B blocks acquiring AA.
    //! The fix is:
    //!   Always take AA first, then BB!
    //!
    //! Ideally, we'd have a known correct ordering for all mutexes.
    //! But mods is complicated and maintained by multiple groups.
    //! Until we get our act together, let's at least impose a few
    //! limits so we can detect some potential deadlocks.
    //!
    //! THE RULE IS:
    //! A thread must take mutexes in strictly decreasing order (high to low).
    //! Except: mtxUnchecked mutexes are unordered with respect to each other.
    //!
    enum MutexOrder
    {
        //! When an mtxLast mutex is held, taking any other mutex is an error.
        mtxLast = 0,

        //! When an mtxNLast mutex is held, one mtxLast mutex may be taken.
        mtxNLast = 1,

        // Etc, think of 2 as next-to-next-to-last, 3 as NNNLast, etc.

        //! Mods will not produce error messages if this mutex is taken
        //! in inconsistent order vs. other mtxUnchecked mutexes.
        //! This is the only "magic" mutex order value.
        mtxUnchecked = 1000,

        //! This mutex may not be taken *after* an mtxUnchecked (or lower)
        //! mutex is taken.
        mtxFirst = mtxUnchecked + 1

        // Think of 1002 as "before First", 1003 as "before before First" etc.
    };

    //! Create and destroy mutex objects
    void * AllocMutex(const char * name, unsigned mutexOrder);
    void FreeMutex(void *MutexHandle);

    //! Acquire and release a mutex object.  Note that you may acquire a mutex any
    //! number of times from the same thread, but you must release it the same
    //! number of times that you acquire it.
    void AcquireMutex(void *MutexHandle);
    void ReleaseMutex(void *MutexHandle);

    //! Try and acquire the mutex but do not block if it is unavailable.  Return
    //! true if the mutex was locked, false otherwise
    bool TryAcquireMutex(void *MutexHandle);

    //! Check if the mutex is held.
    bool CheckMutex(void *MutexHandle);

    //! Check if the mutex is held by the current thread
    bool CheckMutexOwner(void *MutexHandle);

    using Mutex = unique_ptr<void, decltype(&FreeMutex)>;

    inline Mutex CreateMutex(const char* name, unsigned mutexOrder)
    {
        return Mutex(AllocMutex(name, mutexOrder), &FreeMutex);
    }

    inline Mutex NoMutex()
    {
        return Mutex(nullptr, [](void*) { });
    }

    // Condition variable: Acquire a mutex when the indicated polling
    // function returns true.
    //
    RC PollAndAcquireMutex(Tasker::PollFunc pollFunc, volatile void *pArgs,
                           void *MutexHandle, FLOAT64 TimeoutMs);
    RC PollHwAndAcquireMutex(Tasker::PollFunc pollFunc, volatile void *pArgs,
                             void *MutexHandle, FLOAT64 TimeoutMs);

    //! Automatically releases a mutex in destructor.
    class MutexHolder
    {
    public:
        MutexHolder()
        : m_Mutex(nullptr)
        {
        }
        explicit MutexHolder(void * Mutex)
        : m_Mutex(Mutex)
        {
            Tasker::AcquireMutex(m_Mutex);
        }
        explicit MutexHolder(const Mutex& m)
        : MutexHolder(m.get())
        {
        }
        ~MutexHolder()
        {
            Release();
        }

        MutexHolder(const MutexHolder&)            = delete;
        MutexHolder& operator=(const MutexHolder&) = delete;
        MutexHolder& operator=(MutexHolder&& m)    = delete;

        MutexHolder(MutexHolder&& m)
        : m_Mutex(m.m_Mutex)
        {
            m.m_Mutex = nullptr;
        }

        //! Locks a mutex, but only if it hasn't locked any mutex yet.
        bool Acquire(void* Mutex)
        {
            if (m_Mutex == 0)
            {
                m_Mutex = Mutex;
                Tasker::AcquireMutex(Mutex);
                return true;
            }
            else
            {
                return false;
            }
        }
        //! Try to lock a mutex without blocking.  Return true if the
        //! mutex was locked, false it the mutex was already locked or
        //! if this object already locked a mutex.
        bool TryAcquire(void* Mutex)
        {
            if (m_Mutex == 0 && Tasker::TryAcquireMutex(Mutex))
            {
                m_Mutex = Mutex;
                return true;
            }
            else
            {
                return false;
            }
        }
        //! Unlocks the mutex being held.
        void Release()
        {
            if (m_Mutex)
            {
                Tasker::ReleaseMutex(m_Mutex);
                m_Mutex = 0;
            }
        }
        //! Calls PollAndAcquireMutex() if it hasn't locked any mutex yet.
        RC PollAndAcquire(PollFunc pollFunc, volatile void *pArgs,
                          void* Mutex, FLOAT64 TimeoutMs)
        {
            RC rc;
            if (m_Mutex == 0)
            {
                m_Mutex = Mutex;
                rc = Tasker::PollAndAcquireMutex(pollFunc, pArgs,
                                                 Mutex, TimeoutMs);
                if (rc != OK)
                    m_Mutex = 0;
            }
            else
            {
                rc = RC::RESOURCE_IN_USE;
            }
            return rc;
        }
        //! Calls PollHwAndAcquireMutex() if it hasn't locked any mutex yet.
        RC PollHwAndAcquire(PollFunc pollFunc, volatile void *pArgs,
                            void* Mutex, FLOAT64 TimeoutMs)
        {
            RC rc;
            if (m_Mutex == 0)
            {
                m_Mutex = Mutex;
                rc = Tasker::PollHwAndAcquireMutex(pollFunc, pArgs,
                                                   Mutex, TimeoutMs);
                if (rc != OK)
                    m_Mutex = 0;
            }
            else
            {
                rc = RC::RESOURCE_IN_USE;
            }
            return rc;
        }

    private:
        void * m_Mutex;
    };

    class SpinLock
    {
        public:
            explicit SpinLock(volatile UINT32 *pSpinLock);
            ~SpinLock();
        private:
            SpinLock();
            SpinLock(const SpinLock & m); // not implemented
            const SpinLock & operator=(const SpinLock & m); // not implemented

            volatile UINT32 *m_pSpinLock;
    };

    //! Semaphore: a counter for thread synchronization, similar to the
    //! win32 Semaphore object.
    //! Can be used to block a thread until one of N resources is available.

    //! Create one Semaphore object.
    //! InitialCount must be nonnegative (between 0 and 0x7fffffff).
    //! Returns 0 on failure.

    SemaID CreateSemaphore (INT32 initialCount, const char* name = "");

    //! Block the thread until count is >0, then decrease the count by 1.
    //! The count can never become less than 0.
    //!
    //! A single thread may decrement the count by more than one by calling
    //! AcquireSemaphore multiple times.
    //!
    //! May not be called from an ISR.
    //!
    //! Use NO_TIMEOUT to wait forever (or until the semaphore is Destroyed).
    //!
    //! If the timeout expires before the count becomes >0,
    //! returns RC::TIMEOUT_ERROR.
    //! If the semaphore ID is invalid, or is Destroyed before the
    //! count becomes >0, returns RC::SOFTWARE_ERROR.

    RC AcquireSemaphore (SemaID, FLOAT64 timeoutMs);

    //! Increment count by 1, and possibly awaken a thread.
    //! Count can never be incremented beyond 0x7fffffff
    //! (causes assertion).
    //!
    //! If any threads were blocked in AcquireSemaphore for this Semaphore,
    //! wakes exactly one of them -- the one that blocked first.
    //!
    //! May be called from an ISR.

    void ReleaseSemaphore (SemaID);

    //! Frees one Semaphore object previously created with CreateSemaphore.
    //!
    //! If any threads are blocked in Acquire on this Semaphore, they are
    //! awakened and their Acquire calls return RC::SOFTWARE_ERROR.

    void DestroySemaphore (SemaID);

    const char * GetSemaphoreName(SemaID);

    void *AllocRmSpinLock();

    void AcquireRmSpinLock(SpinLockID lock);

    void ReleaseRmSpinLock(SpinLockID lock);

    void DestroyRmSpinLock(SpinLockID lock);

    //! Blocks the current thread on a barrier until all threads within the group have stopped on it.
    //!
    //! If the thread is not part of a barrier, the function will return immediately.
    //!
    //! The barrier automatically "shrinks" when other threads end their life.
    //! If a thread exits and all other threads are waiting on the barrier, they
    //! will be released.
    //!
    //! In the rare event of errors in the underlying implementation
    //! the function will return, release all waiting threads and make the barrier
    //! unusable so that any further invocation to WaitOnBarrier() will return
    //! immediately. In non-release builds this will cause a MASSERTion.
    //!
    //! @sa CreateThreadGroup
    void WaitOnBarrier();

    //Class to override default Hw poll freq value for some tests.
    class UpdateHwPollFreq
    {
    public:
        UpdateHwPollFreq(){}
        //In case of multiple devices when -conlwrrent_devices/_sync option is used we will
        //have multiple test threads requesting same overriding value.
        void Apply(FLOAT64 PollHz);
        //Default freq will be restored in destructor
        ~UpdateHwPollFreq();
    private:
        //Following static variables maintain the state of conlwrrent test threads
        //which try to override default s_HwPollSleepUsec;
        static FLOAT64 s_SlowestPollHz;
        static INT32 s_NumTests;
        static FLOAT64 s_PretestPollHz;
    };

      /** @name Accessors   @{*/

    bool IsInitialized();
        //!< Return true if tasker is ready to add tasks/events/etc.

    UINT32 Threads();
        //!< Get the number of threads present in the tasker, including main thread.

    ThreadID LwrrentThread();
        //!< Get the ID of the lwrrently running thread.

    void PrintThreadStats();
        //!< Print thread statistics if their collecting is enabled.

    void EnableThreadStats();
        //!< Enable collecting of thread statistics.

    void * OsLwrrentThread();
        //!< Get the object containing the lwrrently running thread's OS information.

    JSContext * GetLwrrentJsContext();
        //!< Get the JS context for the current thread.

    JSContext * GetJsContext(ThreadID threadId);
       //!< Get the JS context for a thread.

    void SetLwrrentJsContext(JSContext *pContext);
        //!< Set the JS context for the current thread.

    void DestroyJsContexts();
        //!< Destroy the JS contexts on all threads

    const char * ThreadName(ThreadID thread);
        //!< Get the name of a thread.

    void PrintAllThreads(INT32 Priority = Tee::PriNormal);
        //!< Print (via Printf) the names of all non-destroyed threads.
    /**@}*/

    vector<ThreadID> GetAllThreads();
        //!< Returns ids of all threads.

    vector<ThreadID> GetAllAttachedThreads();
        //!< Returns ids of all attached threads.

    template <typename Fun>
    void ForEachThread(Fun &&fun)
    {
        MASSERT(IsInitialized());
        auto threadIds = GetAllThreads();
        for_each(threadIds.begin(), threadIds.end(), forward<Fun>(fun));
    }

    template <typename Fun>
    void ForEachAttachedThread(Fun &&fun)
    {
        MASSERT(IsInitialized());
        auto threadIds = GetAllAttachedThreads();
        for_each(threadIds.begin(), threadIds.end(), forward<Fun>(fun));
    }

    /** @name Thread Local Storage @{*/
    UINT32 TlsAlloc(bool bInherit = false);
    void   TlsFree (UINT32 tlsIndex);
    void   TlsSetValue (UINT32 tlsIndex, void* value);
    void*  TlsGetValue (UINT32 tlsIndex);
    /**@}*/

    bool IsValidThreadId (ThreadID id);
        //!< For use in fancy error-checking by the JavaScript interface code.

    int StackBytesFree ();
        //!< Return number of bytes of stack space available in current thread.
}

#endif // !INCLUDED_TASKER_H
