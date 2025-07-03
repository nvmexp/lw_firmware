/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   unit_tasker.cpp
 * @brief  Unit tests for the Tasker.
 */

#include "unittest.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"
#include "random.h"
#include "core/include/cpu.h"
#include "core/include/utility.h"
#include "core/include/filesink.h"
#include <vector>
#include <map>
#include <memory>
#include <math.h>

//****************************************************************************
// Base class for tests which create lots of threads
//****************************************************************************
namespace
{
    class MultiThreadTest : public UnitTest
    {
    public:
        unsigned GetTotalThreads() const
        {
            return m_AttachedThreadCount + m_DetachedThreadCount;
        }

    protected:
        MultiThreadTest
        (
            const char* name,
            unsigned AttThreadCnt,
            unsigned DetThreadCnt
        );
        virtual ~MultiThreadTest();
        void CreateThreads();
        void JoinThreads();
        UINT32 GetThreadIndex() const
        {
            return static_cast<UINT32>(reinterpret_cast<size_t>(
                        Tasker::TlsGetValue(m_ThreadIndex)));
        }
        unsigned GetAttachedThreadCount() const { return m_AttachedThreadCount; }
        unsigned GetDetachedThreadCount() const { return m_DetachedThreadCount; }

    private:
        virtual void Thread() = 0;
        static void ThreadFun(void*);
        struct ThreadFunArg
        {
            ThreadFunArg(MultiThreadTest* pTest, bool detach, unsigned index)
            : pTest(pTest)
            , detach(detach)
            , index(index)
            {
            }

            MultiThreadTest* pTest;
            bool detach;
            unsigned index;
        };

        const unsigned m_AttachedThreadCount;
        const unsigned m_DetachedThreadCount;
        UINT32 m_ThreadIndex;
        vector<Tasker::ThreadID> m_Threads;
    };

    MultiThreadTest::MultiThreadTest
    (
        const char* name,
        unsigned AttThreadCnt,
        unsigned DetThreadCnt
    )
    : UnitTest(name)
    , m_AttachedThreadCount(AttThreadCnt)
    , m_DetachedThreadCount(DetThreadCnt)
    , m_ThreadIndex(Tasker::TlsAlloc())
    {
    }

    MultiThreadTest::~MultiThreadTest()
    {
        Tasker::TlsFree(m_ThreadIndex);
    }

    void MultiThreadTest::CreateThreads()
    {
        MASSERT(m_Threads.empty());
        const unsigned totalThreads = GetTotalThreads();
        m_Threads.reserve(totalThreads);
        for (unsigned i=0; i < totalThreads; i++)
        {
            const Tasker::ThreadID id =
                Tasker::CreateThread(
                        ThreadFun,
                        new ThreadFunArg(this, i >= m_AttachedThreadCount, i),
                        Tasker::MIN_STACK_SIZE,
                        "UnitTest");
            m_Threads.push_back(id);
        }
    }

    void MultiThreadTest::JoinThreads()
    {
        const unsigned totalThreads = GetTotalThreads();
        MASSERT(totalThreads == m_Threads.size());

        // Shuffle thread ids to join threads in random order
        vector<UINT32> ids;
        ids.reserve(totalThreads);
        for (UINT32 i=0; i < totalThreads; i++)
        {
            ids.push_back(i);
        }
        Random rand;
        rand.SeedRandom(static_cast<UINT32>(Xp::GetWallTimeUS()));
        rand.Shuffle(totalThreads, &ids[0]);

        for (unsigned i=0; i < totalThreads; i++)
        {
            Tasker::Join(m_Threads[ids[i]]);
        }
        m_Threads.clear();
    }

    void MultiThreadTest::ThreadFun(void* arg)
    {
        unique_ptr<ThreadFunArg> pArg(static_cast<ThreadFunArg*>(arg));
        unique_ptr<Tasker::DetachThread> pDetach;
        if (pArg->detach)
        {
            pDetach = make_unique<Tasker::DetachThread>();
        }
        Tasker::TlsSetValue(pArg->pTest->m_ThreadIndex, reinterpret_cast<void*>(pArg->index));
        pArg->pTest->Thread();
    }

} // anonymous namespace

//****************************************************************************
// Tasker mutex lock test
//****************************************************************************
namespace
{
    class MutexLockTest : public MultiThreadTest
    {
    public:
        MutexLockTest();
        virtual ~MutexLockTest();
        virtual void Run();

    private:
        virtual void Thread();

        void* m_Mutex;
        unsigned m_Count;
        unsigned m_RefCount;
        unsigned m_TestCount;
    };

    ADD_UNIT_TEST(MutexLockTest);

    MutexLockTest::MutexLockTest()
    : MultiThreadTest("TaskerMutexLock", 10, 10)
    , m_Mutex(Tasker::AllocMutex("MutexLockTest::m_Mutex", Tasker::mtxUnchecked))
    , m_Count(0)
    , m_RefCount(0)
    , m_TestCount(100000)
    {
    }

    MutexLockTest::~MutexLockTest()
    {
        Tasker::FreeMutex(m_Mutex);
    }

    void MutexLockTest::Thread()
    {
        unsigned localCount = 0;

        for (unsigned i=0; i < m_TestCount; i++)
        {
            {
                Tasker::MutexHolder lock(m_Mutex);
                m_Count++;
                localCount++;

                // Yield every 20th loop while the mutex is locked
                if ((i % 20) == 0)
                {
                    Tasker::Yield();
                }
            }

            // Yield every 20th loop while the mutex is unlocked
            if ((i % 20) == 10)
            {
                Tasker::Yield();
            }
        }

        Tasker::MutexHolder lock(m_Mutex);
        m_RefCount += localCount;
    }

    void MutexLockTest::Run()
    {
        CreateThreads();
        JoinThreads();

        UNIT_ASSERT_EQ(m_Count, m_RefCount);
        UNIT_ASSERT_EQ(m_Count, GetTotalThreads()*m_TestCount);
    }

} // anonymous namespace

//****************************************************************************
// Tasker semaphore lock test
//****************************************************************************
namespace
{
    class SemaphoreLockTest : public MultiThreadTest
    {
    public:
        SemaphoreLockTest();
        virtual ~SemaphoreLockTest();
        virtual void Run();

    private:
        virtual void Thread();

        SemaID m_Sema;
        unsigned m_Count;
        unsigned m_RefCount;
        unsigned m_TestCount;
    };

    ADD_UNIT_TEST(SemaphoreLockTest);

    SemaphoreLockTest::SemaphoreLockTest()
    : MultiThreadTest("TaskerSemaphoreLock", 10, 10)
    , m_Sema(Tasker::CreateSemaphore(1, "test"))
    , m_Count(0)
    , m_RefCount(0)
    , m_TestCount(10000)
    {
    }

    SemaphoreLockTest::~SemaphoreLockTest()
    {
        Tasker::DestroySemaphore(m_Sema);
    }

    void SemaphoreLockTest::Thread()
    {
        unsigned localCount = 0;

        for (unsigned i=0; i < m_TestCount; i++)
        {
            if (OK != Tasker::AcquireSemaphore(m_Sema, Tasker::NO_TIMEOUT))
            {
                Printf(Tee::PriHigh, "AcquireSemaphore FAILED!\n");
                return;
            }

            m_Count++;
            localCount++;

            // Yield every 20th loop while the semaphore is locked
            if ((i % 20) == 0)
            {
                Tasker::Yield();
            }

            Tasker::ReleaseSemaphore(m_Sema);

            // Yield every 20th loop while the semaphore is unlocked
            if ((i % 20) == 10)
            {
                Tasker::Yield();
            }
        }

        if (OK != Tasker::AcquireSemaphore(m_Sema, Tasker::NO_TIMEOUT))
        {
            Printf(Tee::PriHigh, "AcquireSemaphore FAILED!\n");
            return;
        }
        m_RefCount += localCount;
        Tasker::ReleaseSemaphore(m_Sema);
    }

    void SemaphoreLockTest::Run()
    {
        CreateThreads();
        JoinThreads();

        UNIT_ASSERT_EQ(m_Count, m_RefCount);
        UNIT_ASSERT_EQ(m_Count, GetTotalThreads()*m_TestCount);
    }

} // anonymous namespace

//****************************************************************************
// Tasker semaphore wait test
//****************************************************************************
namespace
{
    class SemaphoreWaitTest : public MultiThreadTest
    {
    public:
        SemaphoreWaitTest();
        virtual ~SemaphoreWaitTest();
        virtual void Run();

    private:
        virtual void Thread();

        SemaID m_Sema;
        void* m_Mutex;
        unsigned m_RefCount;
        unsigned m_TestCount;
    };

    ADD_UNIT_TEST(SemaphoreWaitTest);

    SemaphoreWaitTest::SemaphoreWaitTest()
    : MultiThreadTest("TaskerSemaphoreWait", 10, 10)
    , m_Sema(Tasker::CreateSemaphore(0, "test"))
    , m_Mutex(Tasker::AllocMutex("SemaphoreWaitTest::m_Mutex",
                                 Tasker::mtxUnchecked))
    , m_RefCount(0)
    , m_TestCount(100)
    {
    }

    SemaphoreWaitTest::~SemaphoreWaitTest()
    {
        Tasker::FreeMutex(m_Mutex);
        Tasker::DestroySemaphore(m_Sema);
    }

    void SemaphoreWaitTest::Thread()
    {
        unsigned localCount = 0;
        RC rc;

        for (unsigned i=0; i < m_TestCount; i++)
        {
            rc = Tasker::AcquireSemaphore(m_Sema, Tasker::GetDefaultTimeoutMs());
            UNIT_ASSERT_EQ(rc.Get(), OK);
            if (OK != rc)
            {
                return;
            }
            localCount++;
        }

        Tasker::MutexHolder lock(m_Mutex);
        m_RefCount += localCount;
    }

    void SemaphoreWaitTest::Run()
    {
        // Test timeout
        {
            const unsigned requestedTimeout = 10;
            const unsigned numTimeoutTests = 10;
            UINT64 timeoutSum = 0;
            for (unsigned i=0; i < numTimeoutTests; i++)
            {
                const UINT64 startTime = Xp::GetWallTimeUS();
                const RC rc = Tasker::AcquireSemaphore(m_Sema, requestedTimeout);
                const UINT64 endTime =
                    (rc != RC::TIMEOUT_ERROR)
                        ? 0
                        : (Xp::GetWallTimeUS() - startTime);
                timeoutSum += endTime;
            }
            const unsigned avgTimeout =
                static_cast<unsigned>((timeoutSum / numTimeoutTests) / 1000);
            UNIT_ASSERT_LT(avgTimeout, requestedTimeout+2);
            UNIT_ASSERT_LT(requestedTimeout-2, avgTimeout);
        }

        CreateThreads();

        const unsigned releaseCount = m_TestCount * GetTotalThreads();
        for (unsigned i=0; i < releaseCount/2; i++)
        {
            Tasker::ReleaseSemaphore(m_Sema);
            Tasker::Yield();
        }
        {
            Tasker::DetachThread detach;
            for (unsigned i=0; i < releaseCount-releaseCount/2; i++)
            {
                Tasker::ReleaseSemaphore(m_Sema);
                Tasker::Yield();
            }
        }

        JoinThreads();

        UNIT_ASSERT_EQ(releaseCount, m_RefCount);
    }

} // anonymous namespace

//****************************************************************************
// Tasker semaphore release test
//****************************************************************************
namespace
{
    class SemaphoreReleaseTest : public MultiThreadTest
    {
    public:
        SemaphoreReleaseTest();
        virtual ~SemaphoreReleaseTest();
        virtual void Run();

    private:
        virtual void Thread();

        SemaID m_Sema;
        volatile INT32 m_ErrorCount;
        volatile INT32 m_TimeoutCount;
        UINT32 m_TestCount;
    };

    ADD_UNIT_TEST(SemaphoreReleaseTest);

    SemaphoreReleaseTest::SemaphoreReleaseTest()
    : MultiThreadTest("TaskerSemaphoreRelease", 10, 10)
    , m_Sema(Tasker::CreateSemaphore(0, "test"))
    , m_ErrorCount(0)
    , m_TimeoutCount(0)
    , m_TestCount(100)
    {
    }

    SemaphoreReleaseTest::~SemaphoreReleaseTest()
    {
        Tasker::DestroySemaphore(m_Sema);
    }

    void SemaphoreReleaseTest::Thread()
    {
        const UINT32 threadIdx = GetThreadIndex();
        for (UINT32 i=0; i < m_TestCount; i++)
        {
            if ((threadIdx & 1) == 0)
            {
                Tasker::ReleaseSemaphore(m_Sema);
                if (OK != Tasker::AcquireSemaphore(m_Sema, 1))
                {
                    Printf(Tee::PriHigh, "AcquireSemaphore FAILED on the double-acquire thread! (thread %u, loop %u)\n", threadIdx, i);
                    Cpu::AtomicAdd(&m_ErrorCount, 1);
                }
                if (OK == Tasker::AcquireSemaphore(m_Sema, 1))
                {
                    Tasker::ReleaseSemaphore(m_Sema);
                }
                else
                {
                    Cpu::AtomicAdd(&m_TimeoutCount, 1);
                }
            }
            else
            {
                Tasker::ReleaseSemaphore(m_Sema);
                Tasker::Yield();
                if (OK != Tasker::AcquireSemaphore(m_Sema, 1))
                {
                    Printf(Tee::PriHigh, "AcquireSemaphore FAILED on the single-acquire thread! (thread %u, loop %u)\n", threadIdx, i);
                    Cpu::AtomicAdd(&m_ErrorCount, 1);
                }
            }
        }
    }

    void SemaphoreReleaseTest::Run()
    {
        CreateThreads();
        JoinThreads();
        Printf(Tee::PriLow, "%d timeouts\n", m_TimeoutCount);
        UNIT_ASSERT_EQ(m_ErrorCount, 0);
    }

} // anonymous namespace

//****************************************************************************
// Tasker event tests
//****************************************************************************
namespace
{
    class EventTest : public MultiThreadTest
    {
    public:
        EventTest(const char * name, bool bAutoReset);
        virtual ~EventTest();
        virtual void Run();

    private:
        virtual void Thread();

        ModsEvent* m_Event;
        void* m_Mutex;
        unsigned m_RefCount;
        unsigned m_TestCount;
        bool     m_TestRunning;
        bool     m_AutoReset;
    };

    EventTest::EventTest(const char * name, bool bAutoReset)
    : MultiThreadTest(name, 10, 10)
    , m_Event(Tasker::AllocEvent("test", !bAutoReset))
    , m_Mutex(Tasker::AllocMutex("EventTest::m_Mutex",
                                 Tasker::mtxUnchecked))
    , m_RefCount(0)
    , m_TestCount(100)
    , m_TestRunning(false)
    , m_AutoReset(bAutoReset)
    {
    }

    EventTest::~EventTest()
    {
        Tasker::FreeMutex(m_Mutex);
        Tasker::FreeEvent(m_Event);
    }

    void EventTest::Thread()
    {
        unsigned localCount = 0;

        while (m_TestRunning)
        {
            const RC rc = Tasker::WaitOnEvent(m_Event, 500);
            if (OK == rc)
            {
                localCount++;
            }
            else if (RC::TIMEOUT_ERROR != rc)
            {
                UNIT_ASSERT_EQ(rc.Get(), OK);
                return;
            }
            if (m_AutoReset)
                Tasker::Yield();
            else
                Tasker::Sleep(1);
        }

        Tasker::MutexHolder lock(m_Mutex);
        m_RefCount += localCount;
    }

    void EventTest::Run()
    {
        // Test timeout
        {
            const unsigned requestedTimeout = 10;
            const unsigned numTimeoutTests = 10;
            UINT64 timeoutSum = 0;
            for (unsigned i=0; i < numTimeoutTests; i++)
            {
                const UINT64 startTime = Xp::GetWallTimeUS();
                const RC rc = Tasker::WaitOnEvent(m_Event, requestedTimeout);
                const UINT64 endTime =
                    (rc != RC::TIMEOUT_ERROR)
                        ? 0
                        : (Xp::GetWallTimeUS() - startTime);
                timeoutSum += endTime;
            }
            const unsigned avgTimeout =
                static_cast<unsigned>((timeoutSum / numTimeoutTests) / 1000);
            UNIT_ASSERT_LT(avgTimeout, requestedTimeout+2);
            UNIT_ASSERT_LT(requestedTimeout-2, avgTimeout);
        }

        m_TestRunning = true;
        CreateThreads();

        const unsigned referenceCount = m_AutoReset ? m_TestCount :
                                           (m_TestCount * GetTotalThreads());
        Tasker::Sleep(10);
        for (unsigned i=0; i < m_TestCount/2; i++)
        {
            Tasker::SetEvent(m_Event);
            if (!m_AutoReset)
            {
               Tasker::ResetEvent(m_Event);

               // Manual event support in the new Tasker requires some time after
               // a reset before a set
               Tasker::Sleep(10);
            }
            else
            {
               Tasker::Sleep(1);
            }
        }
        {
            Tasker::DetachThread detach;
            for (unsigned i=0; i < m_TestCount-m_TestCount/2; i++)
            {
                Tasker::SetEvent(m_Event);
                if (!m_AutoReset)
                {
                   Tasker::ResetEvent(m_Event);

                   // Manual event support in the new Tasker requires some time after
                   // a reset before a set
                   Tasker::Sleep(10);
                }
                else
                {
                    Tasker::Sleep(1);
                }
            }
        }

        Tasker::Sleep(10);
        m_TestRunning = false;
        JoinThreads();

        UNIT_ASSERT_EQ(referenceCount, m_RefCount);
    }

    class AutoEventTest : public EventTest
    {
    public:
        AutoEventTest();
        virtual ~AutoEventTest() { }
    };
    ADD_UNIT_TEST(AutoEventTest);

    AutoEventTest::AutoEventTest()
    : EventTest("TaskerAutoResetEvent", true)
    {
    }

    class ManualEventTest : public EventTest
    {
    public:
        ManualEventTest();
        virtual ~ManualEventTest() { }
    };
    ADD_UNIT_TEST(ManualEventTest);

    ManualEventTest::ManualEventTest()
    : EventTest("TaskerManualResetEvent", false)
    {
    }

} // anonymous namespace

//****************************************************************************
// Tasker system event
//****************************************************************************
namespace
{
    class SystemEventTest : public MultiThreadTest
    {
    public:
        SystemEventTest();
        virtual ~SystemEventTest();
        virtual void Run();

    private:
        virtual void Thread();

        static constexpr UINT32 numEvents  = 20;

        ModsEvent*      m_Event[numEvents] = { };
        void*           m_Mutex            = nullptr;
        UINT32          m_TriggerBits      = 0;
        const unsigned  m_TestCount        = 50;
        bool            m_TestRunning      = false;
    };

    SystemEventTest::SystemEventTest()
    : MultiThreadTest("TaskerSystemEvent", numEvents / 2, numEvents - numEvents / 2)
    , m_Mutex(Tasker::AllocMutex("SystemEventTest::m_Mutex",
                                 Tasker::mtxUnchecked))
    {
        for (UINT32 i = 0; i < NUMELEMS(m_Event); i++)
        {
            m_Event[i] = Tasker::AllocSystemEvent("test");
            MASSERT(m_Event[i]);
        }
    }

    SystemEventTest::~SystemEventTest()
    {
        Tasker::FreeMutex(m_Mutex);
        for (UINT32 i = 0; i < NUMELEMS(m_Event); i++)
        {
            Tasker::FreeEvent(m_Event[i]);
        }
    }

    void SystemEventTest::Thread()
    {
        const UINT32 threadIndex = GetThreadIndex();
        const UINT32 threadMask  = 1U << threadIndex;
        for (;;)
        {
            UINT32 trigger = 0U;

            {
                Tasker::MutexHolder lock(m_Mutex);

                if (!m_TestRunning)
                {
                    break;
                }
                trigger = m_TriggerBits & threadMask;
                if (trigger)
                {
                    m_TriggerBits &= ~threadMask;
                }
            }

            if (trigger)
            {
                Printf(Tee::PriLow, "    Set event %u (0x%05x)\n", threadIndex, threadMask);
                Tasker::SetEvent(m_Event[threadIndex]);
            }

            Tasker::Yield();
        }
    }

    void SystemEventTest::Run()
    {
        //////////////////////////////////////////////////////////////
        // No events specified, return OK
        {
            UINT32 numCompleted = ~0U;
            RC rc;
            UNIT_ASSERT_RC(Tasker::WaitOnMultipleEvents(nullptr, 0,
                                                        nullptr, 0, &numCompleted,
                                                        Tasker::NO_TIMEOUT));
            UNIT_ASSERT_EQ(numCompleted, 0);
        }

        //////////////////////////////////////////////////////////////
        // Make sure we fail if no events are set
        {
            UINT32 index = ~0U;
            UINT32 numCompleted = ~0U;
            const RC rc = Tasker::WaitOnMultipleEvents(m_Event, numEvents,
                                                       &index, 1, &numCompleted,
                                                       1);
            UNIT_ASSERT_EQ(rc, RC::TIMEOUT_ERROR);
            UNIT_ASSERT_EQ(index, ~0U);
            UNIT_ASSERT_EQ(numCompleted, ~0U);
        }

        //////////////////////////////////////////////////////////////
        // Set multiple events from the same thread that waits
        {
            Tasker::DetachThread detach;

            static const UINT32 indices[] = { 2, 3, 5, 8 };
            UINT32 expectedMask = 0;
            for (const auto idx: indices)
            {
                MASSERT(idx < numEvents);
                Tasker::SetEvent(m_Event[idx]);
                expectedMask |= 1U << idx;
            }
            Tasker::Yield();

            for (const auto idx: indices)
            {
                UNIT_ASSERT_EQ(Tasker::IsEventSet(m_Event[idx]), true);
            }

            UINT32 setEvents[6] = { };
            UINT32 numCompleted = ~0U;
            RC rc;
            UNIT_ASSERT_RC(Tasker::WaitOnMultipleEvents(m_Event, numEvents,
                                                        setEvents, NUMELEMS(setEvents),
                                                        &numCompleted,
                                                        0));

            UNIT_ASSERT_EQ(numCompleted, 4U);
            UINT32 setMask = 0;
            for (UINT32 idx = 0; idx < numCompleted; idx++)
            {
                setMask |= 1U << setEvents[idx];
            }
            UNIT_ASSERT_EQ(setMask, expectedMask);

            for (const auto idx: indices)
            {
                UNIT_ASSERT_EQ(Tasker::IsEventSet(m_Event[idx]), false);
            }
        }

        //////////////////////////////////////////////////////////////
        // Retrieve events by two
        {
            Tasker::DetachThread detach;

            static const UINT32 indices[] = { 0, 2, 4, 6, 8 };
            UINT32 setMask = 0;
            for (const auto idx: indices)
            {
                MASSERT(idx < numEvents);
                Tasker::SetEvent(m_Event[idx]);
                setMask |= 1U << idx;
            }
            Tasker::Yield();

            for (const auto idx: indices)
            {
                UNIT_ASSERT_EQ(Tasker::IsEventSet(m_Event[idx]), true);
            }

            for (;;)
            {
                UINT32 setEvents[2] = { };
                UINT32 numCompleted = ~0U;
                const RC rc = Tasker::WaitOnMultipleEvents(m_Event, numEvents,
                                                           setEvents, NUMELEMS(setEvents),
                                                           &numCompleted,
                                                           0);
                if (rc == RC::TIMEOUT_ERROR)
                {
                    UNIT_ASSERT_EQ(numCompleted, ~0U);
                    break;
                }

                UNIT_ASSERT_EQ(rc, RC::OK);
                UNIT_ASSERT_LE(numCompleted, 2U);

                UINT32 thisLoopMask = 0;
                for (UINT32 idx = 0; idx < numCompleted; idx++)
                {
                    thisLoopMask |= 1U << setEvents[idx];
                }
                // No unexpected events
                UNIT_ASSERT_EQ(thisLoopMask & ~setMask, 0U);

                setMask &= ~thisLoopMask;
            }
            UNIT_ASSERT_EQ(setMask, 0U);

            for (const auto idx: indices)
            {
                UNIT_ASSERT_EQ(Tasker::IsEventSet(m_Event[idx]), false);
            }
        }

        //////////////////////////////////////////////////////////////
        // Wait on a single system event using WaitOnEvent()
        {
            UNIT_ASSERT_EQ(Tasker::IsEventSet(m_Event[0]), false);
            RC rc = Tasker::WaitOnEvent(m_Event[0], 0);
            UNIT_ASSERT_EQ(rc, RC::TIMEOUT_ERROR);
            rc.Clear();

            Tasker::SetEvent(m_Event[0]);
            Tasker::Yield();
            UNIT_ASSERT_EQ(Tasker::IsEventSet(m_Event[0]), true);

            UNIT_ASSERT_RC(Tasker::WaitOnEvent(m_Event[0], 0));

            UNIT_ASSERT_EQ(Tasker::IsEventSet(m_Event[0]), false);
        }

        //////////////////////////////////////////////////////////////
        // Set events from multiple threads
        {
            Tasker::DetachThread detach;

            m_TestRunning = true;
            CreateThreads();

            DEFER
            {
                {
                    Tasker::MutexHolder lock(m_Mutex);
                    m_TestRunning = false;
                }
                JoinThreads();
            };

            Random rand;
            rand.SeedRandom(static_cast<UINT32>(Xp::GetWallTimeUS()));
            const UINT32 fullTestMask = (1U << numEvents) - 1U;

            for (unsigned loop = 0; loop < m_TestCount; loop++)
            {
                for (UINT32 i = 0; i < numEvents; i++)
                {
                    UNIT_ASSERT_EQ(Tasker::IsEventSet(m_Event[i]), false);
                }

                UINT32 setMask = rand.GetRandom(1U, fullTestMask);

                {
                    Tasker::MutexHolder lock(m_Mutex);
                    m_TriggerBits = setMask;
                    Printf(Tee::PriLow, "Trigger events: 0x%05x\n", setMask);
                }

                while (setMask)
                {
                    UINT32 setEvents[10] = { };
                    UINT32 numCompleted = ~0U;
                    RC rc;
                    UNIT_ASSERT_RC(Tasker::WaitOnMultipleEvents(m_Event, numEvents,
                                                                setEvents, NUMELEMS(setEvents),
                                                                &numCompleted,
                                                                1000));

                    UNIT_ASSERT_LE(numCompleted, 10U);

                    UINT32 thisLoopMask = 0;
                    for (UINT32 idx = 0; idx < numCompleted; idx++)
                    {
                        thisLoopMask |= 1U << setEvents[idx];
                    }
                    Printf(Tee::PriLow, "    Found %u events: 0x%05x\n",
                           numCompleted, thisLoopMask);

                    // No unexpected events
                    UNIT_ASSERT_EQ(thisLoopMask & ~setMask, 0U);

                    setMask &= ~thisLoopMask;
                }

                Tasker::MutexHolder lock(m_Mutex);
                UNIT_ASSERT_EQ(m_TriggerBits, 0U);

                for (UINT32 i = 0; i < numEvents; i++)
                {
                    UNIT_ASSERT_EQ(Tasker::IsEventSet(m_Event[i]), false);
                }
            }
        }
    }
    ADD_UNIT_TEST(SystemEventTest);

} // anonymous namespace

//****************************************************************************
// Tasker TLS test
//****************************************************************************
namespace
{
    class TLSTest : public MultiThreadTest
    {
    public:
        TLSTest();
        virtual ~TLSTest();
        virtual void Run();

    private:
        virtual void Thread();

        UINT32 m_ValueInherited;
        UINT32 m_ValueNotInherited;
    };

    ADD_UNIT_TEST(TLSTest);

    TLSTest::TLSTest()
    : MultiThreadTest("TaskerTLS", 2, 2)
    , m_ValueInherited(Tasker::TlsAlloc(true))
    , m_ValueNotInherited(Tasker::TlsAlloc())
    {
        Tasker::TlsSetValue(m_ValueInherited, reinterpret_cast<void*>(1));
        Tasker::TlsSetValue(m_ValueNotInherited, reinterpret_cast<void*>(2));
    }

    TLSTest::~TLSTest()
    {
        Tasker::TlsFree(m_ValueNotInherited);
        Tasker::TlsFree(m_ValueInherited);
    }

    void TLSTest::Thread()
    {
        const unsigned inh = static_cast<unsigned>(reinterpret_cast<size_t>(
                Tasker::TlsGetValue(m_ValueInherited)));
        UNIT_ASSERT_EQ(inh, 1);

        const unsigned notInh = static_cast<unsigned>(reinterpret_cast<size_t>(
                Tasker::TlsGetValue(m_ValueNotInherited)));
        UNIT_ASSERT_EQ(notInh, 0);
    }

    void TLSTest::Run()
    {
        CreateThreads();
        JoinThreads();
    }

} // anonymous namespace

//****************************************************************************
// Tasker yield test
//****************************************************************************
namespace
{
    class YieldTest : public MultiThreadTest
    {
    public:
        YieldTest();
        virtual ~YieldTest();
        virtual void Run();

    private:
        void RunInternal(bool detached);
        virtual void Thread();

        bool m_Detach;
        void* m_Mutex;
        ModsEvent* m_Event;
        unsigned m_TestCount;
        UINT64 m_SumRunTime;
        UINT64 m_MaxAvgDuration;
    };

    ADD_UNIT_TEST(YieldTest);

    YieldTest::YieldTest()
    : MultiThreadTest("TaskerYield", 10, 0)
    , m_Detach(false)
    , m_Mutex(Tasker::AllocMutex("YieldTest::m_Mutex",
                                 Tasker::mtxUnchecked))
    , m_Event(Tasker::AllocEvent("test", true))
    , m_TestCount(10000)
    , m_SumRunTime(0)
    , m_MaxAvgDuration(0)
    {
    }

    YieldTest::~YieldTest()
    {
        Tasker::FreeEvent(m_Event);
        Tasker::FreeMutex(m_Mutex);
    }

    void YieldTest::Thread()
    {
        unique_ptr<Tasker::DetachThread> pDetach;
        if (m_Detach)
        {
            pDetach = make_unique<Tasker::DetachThread>();
        }

        const RC rc = Tasker::WaitOnEvent(m_Event, 1000);
        UNIT_ASSERT_EQ(rc.Get(), OK);
        if (OK != rc)
        {
            return;
        }

        const UINT64 testStartTime = Xp::GetWallTimeUS();

        UINT64 maxDuration = 0;
        UINT64 sumDuration = 0;

        for (unsigned i=0; i < m_TestCount; i++)
        {
            // Perform an expensive operation
#ifdef LWCPU_FAMILY_ARM
            float
#else
            double
#endif
            a = i;
            for (unsigned i=0; i < 1000; i++)
            {
                a = sqrt(a + 1);
            }

            // Measure yield time
            const UINT64 startTime = Xp::GetWallTimeUS();
            Tasker::Yield();
            const UINT64 duration = Xp::GetWallTimeUS() - startTime;
            maxDuration = max(maxDuration, duration);
            sumDuration += duration;
        }

        const UINT64 avgDuration = sumDuration / m_TestCount;
        const UINT64 testDuration = Xp::GetWallTimeUS() - testStartTime;

        Printf(Tee::PriLow, "test=%lluus, max=%lluus, avg=%lluus\n",
                testDuration, maxDuration, avgDuration);

        Tasker::MutexHolder lock(m_Mutex);
        m_SumRunTime += testDuration;
        m_MaxAvgDuration = max(m_MaxAvgDuration, avgDuration);
    }

    void YieldTest::RunInternal(bool detached)
    {
        m_Detach = detached;
        m_MaxAvgDuration = 0;
        m_SumRunTime = 0;

        CreateThreads();
        Tasker::Sleep(1);

        const UINT64 startTime = Xp::GetWallTimeUS();
        Tasker::SetEvent(m_Event);
        JoinThreads();
        const UINT64 duration = Xp::GetWallTimeUS() - startTime;
        const UINT64 avgRunTime = m_SumRunTime / GetTotalThreads();

        Printf(Tee::PriLow, "Yield in %s threads: totalRunTime=%ums, avgRunTime=%ums, maxAvgYieldDuration=%ums\n",
                detached?"detached":"attached",
                static_cast<unsigned>(duration/1000),
                static_cast<unsigned>(avgRunTime/1000),
                static_cast<unsigned>(m_MaxAvgDuration/1000));

        UNIT_ASSERT_LT(m_MaxAvgDuration, detached ? static_cast<UINT64>(500) : static_cast<UINT64>(3000));
        UNIT_ASSERT_LT(duration, avgRunTime*3);
    }

    void YieldTest::Run()
    {
        RunInternal(false);
#ifndef SIM_BUILD // Detaching threads not supported in sim MODS
        RunInternal(true);
#endif
    }

} // anonymous namespace

//****************************************************************************
// Atomic add test
//****************************************************************************
namespace
{
    class AtomicAddTest : public MultiThreadTest
    {
    public:
        AtomicAddTest();
        virtual ~AtomicAddTest();
        virtual void Run();

    private:
        virtual void Thread();
        void TestPreviousValue();
        void TestAtomicity();

        INT32 m_Count;
        INT32 m_Value;
        INT32 m_Expected;
    };

    ADD_UNIT_TEST(AtomicAddTest);

    AtomicAddTest::AtomicAddTest()
    : MultiThreadTest("Atomic add", 10, 0)
    , m_Count(1000)
    , m_Value(0)
    , m_Expected(0)
    {
    }

    AtomicAddTest::~AtomicAddTest()
    {
    }

    void AtomicAddTest::Thread()
    {
        const UINT32 delta = GetThreadIndex() + 1;
        m_Expected += delta * m_Count * m_Count;
        Tasker::DetachThread detach;
        Tasker::Yield();
        for (INT32 i=0; i < m_Count; i++)
        {
            for (INT32 j=0; j < m_Count; j++)
            {
                Cpu::AtomicAdd(&m_Value, delta);
            }
            Tasker::Yield();
        }
    }

    void AtomicAddTest::TestPreviousValue()
    {
        INT32 value = 10;
        const INT32 oldValue = Cpu::AtomicAdd(&value, -20);
        UNIT_ASSERT_EQ(oldValue, 10);
        UNIT_ASSERT_EQ(value, -10);
    }

    void AtomicAddTest::TestAtomicity()
    {
        CreateThreads();
        JoinThreads();
        UNIT_ASSERT_EQ(m_Value, m_Expected);
    }

    void AtomicAddTest::Run()
    {
        TestPreviousValue();
        TestAtomicity();
    }

} // anonymous namespace

//****************************************************************************
// Atomic exchange test
//****************************************************************************
namespace
{
    template<int size>
    struct AtomicXchg;

    template<>
    struct AtomicXchg<32>
    {
        typedef UINT32 Type;
        static Type Do(volatile Type* pValue, Type data)
        {
            return Cpu::AtomicXchg32(pValue, data);
        }
    };

    template<int size>
    class AtomicXchgTest : public MultiThreadTest
    {
    public:
        typedef typename AtomicXchg<size>::Type AtomicType;
        AtomicXchgTest();
        virtual ~AtomicXchgTest();
        virtual void Run();

    private:
        virtual void Thread();
        void TestPreviousValue();
        void TestAtomicity();
        void CheckResults();
        static AtomicType CreateValue(UINT08 value);

        UINT32 m_Count;
        vector<AtomicType> m_SharedValues;
        vector<AtomicType> m_ThreadValues;
    };

    typedef AtomicXchgTest<32> AtomicXchgTest32;

    ADD_UNIT_TEST(AtomicXchgTest32);

    template<int size>
    AtomicXchgTest<size>::AtomicXchgTest()
    : MultiThreadTest(Utility::StrPrintf("Atomic exchange %d", size).c_str(), 16, 0)
    , m_Count(1024)
    {
    }

    template<int size>
    AtomicXchgTest<size>::~AtomicXchgTest()
    {
    }

    template<int size>
    void AtomicXchgTest<size>::Thread()
    {
        // Every single thread performs swapping with two adjacent values.
        volatile AtomicType& a = m_SharedValues[GetThreadIndex() % m_SharedValues.size()];
        volatile AtomicType& b = m_SharedValues[(GetThreadIndex()+1) % m_SharedValues.size()];

        // Set initial value held by the thread from the thread value array.
        AtomicType value = m_ThreadValues[GetThreadIndex()];

        // Perform the test
        {
            // Detach from the Tasker and yield to other threads
            // to increase the potential to execute many threads simultaneoulsy.
            Tasker::DetachThread detach;
            Tasker::Yield();

            // The outer loop exelwtes inner loop and yields.
            // The yield is there to reshuffle threads and make sure
            // that the exelwtion is not "boring".
            for (UINT32 i=0; i < m_Count; i++)
            {
                // The inner loop swaps the value held by the thread
                // with one of the shared values, and then with
                // another shared value.
                for (UINT32 j=0; j < m_Count; j++)
                {
                    value = AtomicXchg<size>::Do(&a, value);
                    value = AtomicXchg<size>::Do(&b, value);
                }
                Tasker::Yield();
            }
        }

        // After the thread is done, it stores the value it ended up
        // with in its private slot in the thread value array.
        m_ThreadValues[GetThreadIndex()] = value;
    }

    template<int size>
    void AtomicXchgTest<size>::CheckResults()
    {
        // Coalesce the tested array with the thread values array
        vector<AtomicType> values;
        values.insert(values.end(), m_SharedValues.begin(), m_SharedValues.end());
        values.insert(values.end(), m_ThreadValues.begin(), m_ThreadValues.end());

        // Accumulate counts of all original/unique values
        typedef map<AtomicType, UINT32> Counts;
        Counts counts;
        for (UINT32 i=0; i < values.size(); i++)
        {
            typename Counts::iterator findIt = counts.find(values[i]);
            if (findIt != counts.end())
            {
                findIt->second++;
            }
            else
            {
                counts[values[i]] = 1;
            }
        }

        // Make sure each value repeated exactly once
        UNIT_ASSERT_EQ(static_cast<unsigned>(counts.size()), static_cast<unsigned>(values.size()));
        for (typename Counts::const_iterator it=counts.begin();
                it != counts.end(); ++it)
        {
            UNIT_ASSERT_EQ(it->second, 1U);
        }
    }

    template<int size>
    typename AtomicXchgTest<size>::AtomicType
        AtomicXchgTest<size>::CreateValue(UINT08 value)
    {
        AtomicType v = 0;
        for (UINT32 i=0; i < sizeof(v); i++)
        {
            v |= static_cast<AtomicType>(value) << (i*8);
        }
        return v;
    }

    template<int size>
    void AtomicXchgTest<size>::TestPreviousValue()
    {
        AtomicType value = 1;
        const AtomicType oldValue = AtomicXchg<size>::Do(&value, 0x10);
        UNIT_ASSERT_EQ(oldValue, 1);
        UNIT_ASSERT_EQ(value, 0x10);
    }

    template<int size>
    void AtomicXchgTest<size>::TestAtomicity()
    {
        // We will share two values between all threads
        m_SharedValues.clear();
        m_SharedValues.push_back(CreateValue(0x00U));
        m_SharedValues.push_back(CreateValue(0xFFU));

        // Initialize values for the threads.
        // These values must be unique, i.e. no two threads can share
        // two same values and they must be different from the shared values.
        // If somebody screws this up, the test will always fail.
        // I've chosen good MATS values.
        m_ThreadValues.clear();
        m_ThreadValues.push_back(CreateValue(0x55U));
        m_ThreadValues.push_back(CreateValue(0xAAU));
        m_ThreadValues.push_back(CreateValue(0x33U));
        m_ThreadValues.push_back(CreateValue(0xCLW));
        m_ThreadValues.push_back(CreateValue(0x05U));
        m_ThreadValues.push_back(CreateValue(0x50U));
        m_ThreadValues.push_back(CreateValue(0xF5U));
        m_ThreadValues.push_back(CreateValue(0x5FU));
        m_ThreadValues.push_back(CreateValue(0x0AU));
        m_ThreadValues.push_back(CreateValue(0xA0U));
        m_ThreadValues.push_back(CreateValue(0xFAU));
        m_ThreadValues.push_back(CreateValue(0xAFU));
        m_ThreadValues.push_back(CreateValue(0x5AU));
        m_ThreadValues.push_back(CreateValue(0xA5U));
        m_ThreadValues.push_back(CreateValue(0x0FU));
        m_ThreadValues.push_back(CreateValue(0xF0U));
        UNIT_ASSERT_EQ(static_cast<unsigned>(m_ThreadValues.size()), GetAttachedThreadCount());
        if (m_ThreadValues.size() != GetAttachedThreadCount())
        {
            return;
        }

        CreateThreads();
        JoinThreads();
        CheckResults();
    }

    template<int size>
    void AtomicXchgTest<size>::Run()
    {
        TestPreviousValue();
        TestAtomicity();
    }

} // anonymous namespace

//****************************************************************************
// Atomic compare-and-exchange test
//****************************************************************************
namespace
{
    template<int size>
    struct AtomicCmpXchg;

    template<>
    struct AtomicCmpXchg<32>
    {
        typedef UINT32 Type;
        static bool Do(volatile Type* pValue, Type oldData, Type newData)
        {
            return Cpu::AtomicCmpXchg32(pValue, oldData, newData);
        }
    };

    template<>
    struct AtomicCmpXchg<64>
    {
        typedef UINT64 Type;
        static bool Do(volatile Type* pValue, Type oldData, Type newData)
        {
            return Cpu::AtomicCmpXchg64(pValue, oldData, newData);
        }
    };

    template<int size>
    class AtomicCmpXchgTest : public MultiThreadTest
    {
    public:
        typedef typename AtomicCmpXchg<size>::Type AtomicType;
        AtomicCmpXchgTest();
        virtual void Run();

    private:
        virtual void Thread();
        void TestFunctionality();
        void TestAtomicity();

        UINT32 m_Count;
        vector<UINT32> m_NumSuccesses;
        volatile AtomicType m_Value;
    };

    typedef AtomicCmpXchgTest<32> AtomicCmpXchgTest32;
    typedef AtomicCmpXchgTest<64> AtomicCmpXchgTest64;

    ADD_UNIT_TEST(AtomicCmpXchgTest32);
    ADD_UNIT_TEST(AtomicCmpXchgTest64);

    template<int size>
    AtomicCmpXchgTest<size>::AtomicCmpXchgTest() :
        MultiThreadTest(Utility::StrPrintf("Atomic compare and exchange %d",
                                           size).c_str(), 10, 0),
        m_Count(1000),
        m_Value(0)
    {
    }

    template<int size>
    void AtomicCmpXchgTest<size>::Thread()
    {
        AtomicType oldValue = 0;
        AtomicType newValue = 1;
        {
            Tasker::DetachThread detach;
            Tasker::Yield();
            for (UINT32 ii = 0; ii < m_Count; ++ii)
            {
                for (UINT32 jj = 0; jj < m_Count; ++jj)
                {
                    if (AtomicCmpXchg<size>::Do(&m_Value, oldValue, newValue))
                    {
                        ++m_NumSuccesses[GetThreadIndex()];
                    }
                    ++oldValue;
                    ++newValue;
                }
                Tasker::Yield();
            }
        }
    }

    template<int size>
    void AtomicCmpXchgTest<size>::TestFunctionality()
    {
        vector<AtomicType> testNumbers;
        testNumbers.push_back(0);
        for (UINT32 ii = 0; ii < 10; ++ii)
            testNumbers.push_back(testNumbers.back() * 1664525 + 1013904223);

        for (UINT32 ii = 0; ii < testNumbers.size(); ++ii)
        {
            for (UINT32 jj = 0; jj < testNumbers.size(); ++jj)
            {
                for (UINT32 kk = 0; kk < testNumbers.size(); ++kk)
                {
                    AtomicType initValue = testNumbers[ii];
                    AtomicType oldValue = testNumbers[jj];
                    AtomicType newValue = testNumbers[kk];

                    volatile AtomicType testLoc = initValue;
                    if (AtomicCmpXchg<size>::Do(&testLoc, oldValue, newValue))
                    {
                        UNIT_ASSERT_EQ(initValue, oldValue);
                        UNIT_ASSERT_EQ(testLoc, newValue);
                    }
                    else
                    {
#ifndef LWCPU_FAMILY_ARM
                        // On ARM the atomic compare&exchange can fail even if
                        // "old value" matches.
                        UNIT_ASSERT_NEQ(initValue, oldValue);
#endif
                        UNIT_ASSERT_EQ(testLoc, initValue);
                    }
                }
            }
        }
    }

    template<int size>
    void AtomicCmpXchgTest<size>::TestAtomicity()
    {
        m_NumSuccesses.resize(GetTotalThreads(), 0);
        m_Value = 0;

        CreateThreads();
        JoinThreads();

        UINT32 totalNumSuccesses = 0;
        for (UINT32 ii = 0; ii < m_NumSuccesses.size(); ++ii)
        {
            totalNumSuccesses += m_NumSuccesses[ii];
        }
        UNIT_ASSERT_EQ(totalNumSuccesses, m_Count * m_Count);
        UNIT_ASSERT_EQ(totalNumSuccesses, m_Value);
    }

    template<int size>
    void AtomicCmpXchgTest<size>::Run()
    {
        TestFunctionality();
        TestAtomicity();
    }

} // anonymous namespace

//****************************************************************************
// Barrier test
//****************************************************************************
namespace
{
    class BarrierTest : public UnitTest
    {
    public:
        BarrierTest();
        virtual ~BarrierTest();
        virtual void Run();

    private:
        void Thread();
        static void ThreadFun(void* arg);
        void TestBarrier();
        UINT32 GetThreadWaitCount(UINT32 tid) const;

        UINT32 m_GroupSize;
        UINT32 m_NumToDetach;
        const UINT32 m_BarrierWaitCount;
        const UINT32 m_TidWaitCountDelta;
        volatile INT32 m_IncorrectGroupSize;
        volatile INT32 m_SumIndices;
        volatile INT32 m_BarrierErrors;
        vector<UINT32> m_BarrierHits;
    };

    ADD_UNIT_TEST(BarrierTest);

    BarrierTest::BarrierTest()
    : UnitTest("Barrier")
    , m_GroupSize(0)
    , m_NumToDetach(0)
    , m_BarrierWaitCount(10000)
    , m_TidWaitCountDelta(10)
    , m_IncorrectGroupSize(0)
    , m_SumIndices(0)
    , m_BarrierErrors(0)
    {
    }

    BarrierTest::~BarrierTest()
    {
    }

    void BarrierTest::ThreadFun(void* arg)
    {
        static_cast<BarrierTest*>(arg)->Thread();
    }

    UINT32 BarrierTest::GetThreadWaitCount(UINT32 tid) const
    {
        return m_BarrierWaitCount + tid * m_TidWaitCountDelta;
    }

    void BarrierTest::Thread()
    {
        // Detach thread if needed
        unique_ptr<Tasker::DetachThread> pDetach;
        if (Tasker::GetThreadIdx() < m_NumToDetach)
        {
            pDetach = make_unique<Tasker::DetachThread>();
        }

        // Validate group size
        if (m_GroupSize != Tasker::GetGroupSize())
        {
            Cpu::AtomicAdd(&m_IncorrectGroupSize, 1);
        }

        // Validate thread index
        const UINT32 tid = Tasker::GetThreadIdx();
        Cpu::AtomicAdd(&m_SumIndices, tid);

        // Wait on barrier
        const UINT32 waitCount = GetThreadWaitCount(tid);
        for (UINT32 i=0; i < waitCount; i++)
        {
            ++m_BarrierHits[tid];
            if (i != Tasker::GetBarrierReleaseCount())
            {
                Cpu::AtomicAdd(&m_BarrierErrors, 1);
            }
            Tasker::WaitOnBarrier();
        }
    }

    void BarrierTest::TestBarrier()
    {
        m_BarrierHits.resize(m_GroupSize, 0);

        vector<Tasker::ThreadID> threads =
            Tasker::CreateThreadGroup
            (
                ThreadFun,
                this,
                m_GroupSize,
                "Barrier test",
                true,
                nullptr
            );
        RC rc = Tasker::Join(threads);
        UNIT_ASSERT_EQ(rc.Get(), OK);

        UNIT_ASSERT_EQ(m_IncorrectGroupSize, 0);
        m_IncorrectGroupSize = 0;

        UNIT_ASSERT_EQ(m_SumIndices,
                static_cast<INT32>((m_GroupSize*(m_GroupSize-1))/2));
        m_SumIndices = 0;

        UNIT_ASSERT_EQ(m_BarrierErrors, 0);
        m_BarrierErrors = 0;

        for (UINT32 i=0; i < m_GroupSize; i++)
        {
            UNIT_ASSERT_EQ(m_BarrierHits[i], GetThreadWaitCount(i));
        }
        m_BarrierHits.clear();
    }

    void BarrierTest::Run()
    {
        Printf(Tee::PriNormal, "Testing a single thread...\n");
        m_GroupSize = 1;
        m_NumToDetach = 0;
        TestBarrier();

        Printf(Tee::PriNormal, "Testing two attached threads...\n");
        m_GroupSize = 2;
        m_NumToDetach = 0;
        TestBarrier();

        Printf(Tee::PriNormal, "Testing attached threads...\n");
        m_GroupSize = 10;
        m_NumToDetach = 0;
        TestBarrier();

        Printf(Tee::PriNormal, "Testing detached threads...\n");
        m_GroupSize = 100;
        m_NumToDetach = 100;
        TestBarrier();

        Printf(Tee::PriNormal, "Testing both attached and detached threads...\n");
        m_GroupSize = 100;
        m_NumToDetach = 90;
        TestBarrier();
    }

} // anonymous namespace

//****************************************************************************
// Spinlock test
//****************************************************************************
namespace
{
    class SpinLockTest : public MultiThreadTest
    {
    public:
        SpinLockTest();
        virtual ~SpinLockTest();
        virtual void Run();

    private:
        virtual void Thread();
        volatile UINT32  m_SpinLock;
        volatile INT32   m_Count;
        unsigned m_TestCount;
    };

    ADD_UNIT_TEST(SpinLockTest);

    SpinLockTest::SpinLockTest()
    : MultiThreadTest("TaskerSpinlock", 100, 100)
    , m_SpinLock(0)
    , m_Count(0)
    , m_TestCount(5000)
    {
    }

    SpinLockTest::~SpinLockTest()
    {
    }

    void SpinLockTest::Thread()
    {
        Random  rand;
        rand.SeedRandom(static_cast<UINT32>(Xp::GetWallTimeUS()));

        for (unsigned i=0; i < m_TestCount; i++)
        {
            {
                Tasker::SpinLock locker(&m_SpinLock);
                m_Count++;
            }
        }
    }

    void SpinLockTest::Run()
    {
        CreateThreads();
        JoinThreads();

        UNIT_ASSERT_EQ(m_Count, (INT32)(GetTotalThreads() * m_TestCount));
    }

} // anonymous namespace

//****************************************************************************
// Poll test
//****************************************************************************
namespace
{
    class PollTest : public UnitTest
    {
        public:
            PollTest() : UnitTest("TaskerPoll") { }

            virtual ~PollTest() { }

            virtual void Run()
            {
                RC rc;

#ifdef SIM_BUILD
                // In sim MODS, timeout may be overriden to infinite (NO_TIMEOUT).
                if (Tasker::FixTimeout(1) == Tasker::NO_TIMEOUT)
                {
                    Tasker::SetDefaultTimeoutMs(1000);
                }
#endif

                rc = Tasker::Poll(1,
                    []()->bool
                    {
                        return false;
                    }
                );
                UNIT_ASSERT_EQ(rc, RC::TIMEOUT_ERROR);
                rc.Clear();

                rc = Tasker::PollHw(1,
                    []()->bool
                    {
                        return false;
                    }
                );
                UNIT_ASSERT_EQ(rc, RC::TIMEOUT_ERROR);
                rc.Clear();

                int value = 0;

                rc = Tasker::Poll(1000,
                    [&]()->bool
                    {
                        return ++value >= 5;
                    }
                );
                UNIT_ASSERT_EQ(rc, OK);
                rc.Clear();

                UNIT_ASSERT_EQ(value, 5);

                int count = 0;

                rc = Tasker::PollHw(1000,
                    [&]()->bool
                    {
                        ++count;
                        return ++value >= 10;
                    }
                );
                UNIT_ASSERT_EQ(rc, OK);
                rc.Clear();

                UNIT_ASSERT_EQ(value, 10);
                UNIT_ASSERT_EQ(count, 5);
            }
    };

    ADD_UNIT_TEST(PollTest);

} // anonymous namespace

//****************************************************************************
// Tasker condition test
//****************************************************************************
namespace
{
    class ConditionTest : public MultiThreadTest
    {
    public:
        ConditionTest();
        virtual ~ConditionTest();
        virtual void Run();
    private:
        virtual void Thread();

        ModsCondition m_Cond;
        void *m_Mutex;
        void *m_MutexResource;
        unsigned m_Resources;
        unsigned m_RefCount;
        unsigned m_TestCount;
    };

    ADD_UNIT_TEST(ConditionTest);

    ConditionTest::ConditionTest()
    : MultiThreadTest("TaskerCondition", 10, 10)
    , m_Mutex(Tasker::AllocMutex("ConditionTest::m_Mutex", Tasker::mtxUnchecked))
    , m_MutexResource(Tasker::AllocMutex("ConditionTest::m_MutexResource", Tasker::mtxUnchecked))
    , m_Resources(0)
    , m_RefCount(0)
    , m_TestCount(100)
    {
    }

    ConditionTest::~ConditionTest()
    {
        Tasker::FreeMutex(m_MutexResource);
        Tasker::FreeMutex(m_Mutex);
    }

    void ConditionTest::Thread()
    {
        unsigned localCount = 0;
        StickyRC rc;

        for (unsigned i = 0; i < m_TestCount; i++)
        {
            {
                Tasker::MutexHolder lock(m_MutexResource);
                while (!m_Resources)
                {
                    rc = m_Cond.Wait(m_MutexResource, Tasker::GetDefaultTimeoutMs());
                }
                m_Resources--;
            }
            UNIT_ASSERT_EQ(rc.Get(), OK);
            if (OK != rc)
            {
                return;
            }
            localCount++;
        }
        Tasker::MutexHolder lock(m_Mutex);
        m_RefCount += localCount;
    }

    void ConditionTest::Run()
    {
        // Test timeout
        {
            const unsigned requestedTimeout = 10;
            const unsigned numTimeoutTests = 10;
            UINT64 timeoutSum = 0;
            for (unsigned i=0; i < numTimeoutTests; i++)
            {
                Tasker::MutexHolder lock(m_Mutex);
                const UINT64 startTime = Xp::GetWallTimeUS();
                const RC rc = m_Cond.Wait(m_Mutex, requestedTimeout);
                const UINT64 endTime =
                    (rc != RC::TIMEOUT_ERROR)
                        ? 0
                        : (Xp::GetWallTimeUS() - startTime);
                timeoutSum += endTime;
            }
            const unsigned avgTimeout =
                static_cast<unsigned>((timeoutSum / numTimeoutTests) / 1000);
            UNIT_ASSERT_LT(avgTimeout, requestedTimeout+2);
            UNIT_ASSERT_LT(requestedTimeout-2, avgTimeout);
        }

        CreateThreads();

        const unsigned signalCount = m_TestCount * GetTotalThreads();
        for (unsigned i=0; i < signalCount/2; i++)
        {
            {
                Tasker::MutexHolder lock(m_MutexResource);
                m_Resources++;
            }
            m_Cond.Signal();
            Tasker::Yield();
        }
        {
            Tasker::DetachThread detach;
            for (unsigned i=0; i < signalCount-signalCount/2; i++)
            {
                {
                    Tasker::MutexHolder lock(m_MutexResource);
                    m_Resources++;
                }
                m_Cond.Signal();
                Tasker::Yield();
            }
        }

        JoinThreads();

        UNIT_ASSERT_EQ(signalCount, m_RefCount);
        UNIT_ASSERT_EQ(m_Resources, 0);
    }

} // anonymous namespace
