/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#include "core/include/nativethread.h"

namespace Tasker
{
namespace Private
{
#ifdef _MSC_VER
    using NativeMutex = Tasker::NativeMutex;

    using NativeLock = Tasker::NativeMutexHolder;

    class NativeTls
    {
        public:
            NativeTls()
            : m_Key(Tasker::NativeTlsValue::Alloc())
            {
            }
            ~NativeTls()
            {
                Tasker::NativeTlsValue::Free(m_Key);
            }
            void Set(void* value)
            {
                Tasker::NativeTlsValue::Set(m_Key, value);
            }
            void* Get() const
            {
                return Tasker::NativeTlsValue::Get(m_Key);
            }
        private:
            UINT32 m_Key;
    };
#else
    // Workaround for Tasker::NativeMutex being relwrsive by default.
    // We want a non-relwrsive mutex here.
    class NativeMutex: public Tasker::LinuxMutex
    {
        public:
            NativeMutex() : LinuxMutex(Tasker::normal_mutex) { }
    };

    using NativeLock = Tasker::LinuxMutexHolder;

    using NativeTls = Tasker::LinuxTlsValue;
#endif

    /// Manages thread scheduling in a round-robin fashion.
    ///
    /// Threads scheduled this way are cooperative and are called "attached".
    /// Only one attached/cooperative thread can make progress at a time.
    /// Attached threads must explicitly "yield" to other threads.
    ///
    /// Supports "detaching" threads from the cooperative scheduling regime,
    /// so that they can be exelwted in parallel by becoming normal,
    /// preemptive threads.
    ///
    /// Only one object of this class should exist (having multiple such
    /// objects would be error-prone).
    ///
    /// The cooperative scheduling is implemented with a queue of attached
    /// threads to schedule and a per-thread "active" flag with a condition
    /// variable.  Each attached thread waits on the "active" flag using the
    /// condition variable until it is scheduled.  Scheduling subsequent
    /// threads works by selecting which thread should run next, setting the
    /// "active" flag and waking up the thread via the condition variable.
    class ThreadMill
    {
        public:
            ThreadMill() = default;
            ThreadMill(const ThreadMill&)            = delete;
            ThreadMill& operator=(const ThreadMill&) = delete;
            ~ThreadMill();

            enum State
            {
                Attached,
                Detached
            };

            /// Keeps thread state and scheduling information
            class ThreadState
            {
                public:
                    ThreadState* pNextScheduled = nullptr;
                    ThreadState* pNextExisting  = nullptr;
                    State        state          = Detached;

                    bool IsActive() const;
                    void Activate();
                    void MarkInactive();
                    void Wait(NativeMutex& mutex);

                private:
                    NativeCondition m_SchedCond;
                    bool            m_Active = false;
            };

            /// Initializes a new thread state.
            ///
            /// This is done right before spawning a new thread and the thread
            /// state obtained will passed to the new thread via thread function
            /// argument.  The new thread must immediately call InitLwrrentThread().
            ///
            /// The caller must make sure that the pThread object has been
            /// cleanly initialized and that it will not be moved or destroyed
            /// until after Destroy() is called.
            void Initialize(ThreadState* pThread, State state);

            /// Initializes a newly created thread.
            ///
            /// This is called once at the beginning of a new thread inside
            /// the thread function in order to initialize it correctly.
            void InitLwrrentThread(ThreadState* pThread);

            /// Removes/unregisters thread state from the thread mill.
            ///
            /// This is called for a thread which has already been joined, in
            /// order to stop tracking it.
            ///
            /// The caller is responsible for deallocating thread state
            /// storage.
            void Destroy(ThreadState* pThread);

            /// Returns state object for the current thread.
            ///
            /// Returns nullptr if Initialize() was not called in the current thread.
            ThreadState* GetThreadState();

            /// Passes control to another attached thread.
            ///
            /// When this is called from an attached thread, it suspends the current
            /// thread and lets another attached thread run.  Subsequent threads
            /// are selected (scheduled) in a round-robin fashion.
            ///
            /// If there is no other thread to be scheduled, this function returns
            /// false and does not do anything.
            ///
            /// When this function is called from a detached thread, it returns false
            /// and does nothing.
            ///
            /// Returns true if another attached thread was successfuly scheduled
            /// to run or false if nothing happened.
            bool SwitchToNextThread();

            /// Attaches the current thread to become cooperative.
            ///
            /// If the current thread is detached, it calls this function and
            /// starts waiting on its scheduling mutex until it is scheduled.
            ///
            /// If threre are no other attached threads, the current thread will
            /// continue running uninterrupted.
            ///
            /// If the current thread is already attached, this function does
            /// nothing and no rescheduling oclwrs.
            ///
            /// Returns true if the thread successfuly changed state from
            /// detached to attached or false if this thread was already attached.
            bool Attach();

            /// Detaches the current thread to become preemptive.
            ///
            /// If the current thread is attached, it schedules another attached
            /// thread to run and then continues running as detached/preemptive
            /// thread.
            ///
            /// This function is non-blocking.
            ///
            /// If the current thread is already detached, this function does
            /// nothing.
            ///
            /// Typical uses of this function include waiting on a resource
            /// (e.g. mutex, event or semaphore) or joining another thread.
            ///
            /// Returns true if the thread successfuly changed state from
            /// attached to detached or false if this thread was already detached.
            bool Detach();

            /// Returns state object for the lwrrently active attached thread
            ///
            /// Returns nullptr if there is no attached thread.
            ///
            /// NOTE: m_Mutex must be locked when calling this function.
            ThreadState* GetLwrrentAttachedThread() { return m_pScheduledHead; }

            /// Get access to the mutex to prevent threads from switching
            ///
            /// This is needed e.g. when dumping the state of threads.
            NativeMutex& GetMutex() { return m_Mutex; }

        private:
            void Schedule(ThreadState* pThread);

            NativeTls    m_TlsThreadPtr;
            NativeMutex  m_Mutex;
            ThreadState* m_pAllThreads    = nullptr;
            ThreadState* m_pScheduledHead = nullptr; // Current attached thread
            ThreadState* m_pScheduledTail = nullptr; // Last scheduled thread
    };
} // namespace Private
} // namespace Tasker
