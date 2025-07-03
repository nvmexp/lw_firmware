/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2012,2014,2016-2017 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   thread.cpp
 * @brief  Test of Tasker functionality.
 */

#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/tee.h"
#include "core/include/tasker.h"
#include "core/include/xp.h"
#include "random.h"
#include "core/include/utility.h"
#include "lwtypes.h"
#include <stdio.h>
#include <time.h>
#include <vector>

#define THREAD_BENCHMARK_LOOPS 10000

// Don't keep fmodel idle for too long or it will exit:
#define THREAD_BENCHMARK_LOOPS_FMODEL 100

//! Namespace for the Tasker, Thread, and Event test functions.
namespace ThreadTest
{
   bool     m_Initialized = false;     //!< Overall "initialized" flag.

   Tasker::EventID *m_EventArr;        //!< Used in Event test.
   FLOAT64  m_TimeoutStart;            //!< Used in Timeout test.
   FLOAT64  m_PollStart;               //!< Used in Poll test.
   FLOAT64  m_PerfFreq;
   FLOAT64  m_TimeoutMs;               //!< Used in Poll test.
   Random   m_Random;

   //! Structure used in Event tests
   struct CountingEventData
   {
      Tasker::EventID Event;
      UINT32          Counter;
      RC              rc;
      UINT32         *pWaitCount;
      bool           *pbDone;
   };

   /** @name General-purpose
    */
   //@{
   void Initialize();                   //!< Initialize the test package.
   RC RunAll();                         //!< Run all the tests, one after the other.
   //@}

   /** @name Random Number test
    */
   //@{
   RC RandomNumbers();                  //!< Random number count subtest.
   void RandCounter(void *arg);         //!< Thread control function for
                                        //!< random counter test.
   //@}

   /** @name Event test
    */
   //@{
   RC Event();                          //!< Event subtest.
   RC SetEvent();                       //!< SetEvent sub-subtest.
   RC ResetEvent(bool bAutoReset);      //!< ResetEvent sub-subtest.
   void EventThread(void *arg);         //!< Takes an (Tasker::EventID *) to wait on.
   void SignallerThread(void *arg);     //!< Takes an int giving the size of m_EventArr.
   void CountingEventThread(void *arg); //!< Takes a (CountingEventData *).
   //@}

   /** @name Benchmark test
    */
   //@{
   RC Benchmark();                      //!< Benchmark subtest.
   FLOAT64 DoBenchmark();               //!< Performs a benchmark measuring the
                                        //!< time of a Yield() call.

   RC BlockingBenchmark();              //!< Blocking benchmark sub-subtest.
   void BlockerThread(void *arg);       //!< Takes an (Tasker::EventID *) to wait on.

   RC YieldingBenchmark();              //!< Yielding benchmark sub-subtest.
   void YielderThread(void *arg);       //!< Thread function for yielding benchmark.

   void YieldFunc();                    //!< Benchmarking function that just calls Yield.
   //@}

   /** @name Timeout test
    */
   //@{
   RC TimeoutTest();                    //!< Timeout subtest.
   void TimeoutThread(void *args);      //!< Thread for the Timeout test.
   //@}

   /** @name Polling test
    */
   //@{
   RC PollTest(FLOAT64 timeoutMs = 100);//!< Polling subtest.
   bool RandomTrue(void *arg);          //!< Randomly returns true.
   void PollThread(void *arg);          //!< Thread ctrl. func for polling subtest.
   //@}

   /** @name Semaphore test
    */
   //@{
   RC SemaphoreTest();                    //!< semapohre subtest.
   void Sema4Thread1(void *args);      //!< Thread for the semaphore test.
   void Sema4Thread2(void *args);      //!< Thread for the semaphore test.
   void Sema4Thread3(void *args);      //!< Thread for the semaphore test.
   //@}

   /** @name Join test
    */
   //@{
   RC JoinTest();                       //!< Join subtest.
   void JoinSleeperThread(void *args);
   void JoinAndCreateThread(void *args);
   Tasker::ThreadID m_SecondSleeper;
   //@}

   /** @name Utility
    */
   //@{
   void TestPassed();
   void TestFailed();
   //@}
}

void ThreadTest::Initialize()
{
   if (!m_Initialized)
   {
      time_t t = time(0);
      Printf(Tee::PriLow, "Seeding srand with %d.\n", (int)t);
      m_Random.SeedRandom((UINT32) t);

      m_PerfFreq = (FLOAT64)(INT64)Xp::QueryPerformanceFrequency();

      m_Initialized = true;
   }
}

RC ThreadTest::RunAll()
{
   RC rc;

   CHECK_RC(RandomNumbers());
   CHECK_RC(Event());
   CHECK_RC(ResetEvent(true));
   CHECK_RC(ResetEvent(false));
   CHECK_RC(Benchmark());
   CHECK_RC(TimeoutTest());
   CHECK_RC(JoinTest());
   CHECK_RC(PollTest());
   return OK;
}

RC ThreadTest::RandomNumbers()
{
   Initialize();

   const int numThreads = 5;
   UINT32 highBound = 20;

   Tasker::ThreadID *threadArr = new Tasker::ThreadID[numThreads];

   Printf(Tee::PriLow, "*** Random Counter Test ***\n");
   Printf(Tee::PriLow, "Spawning %d threads; each will count to a random number in range [0, %d]\n", numThreads, highBound);
   char buf[64];
   int i;
   for (i = 0; i < numThreads; i++) {
      sprintf(buf, "Random Counter #%d", i+1);
      threadArr[i] = Tasker::CreateThread(ThreadTest::RandCounter, (void *)&highBound, 0, buf);
   }
   for (i = 0; i < numThreads; i++) {
      Tasker::Join(threadArr[i]);
   }

   delete[] threadArr;

   TestPassed();
   return OK;
}

/**
 * Picks a random number between 0 and *arg, inclusive.  Counts
 * from zero to that random number, yielding after every number
 * it counts.  Returns when it has finished counting.
 *
 * @param arg A (UINT32 *).  @a *arg specifies the highest number
 *            we're allowed to count to.
 */
void ThreadTest::RandCounter(void *arg)
{
   UINT32 *highBound = (UINT32 *)arg;
   UINT32 target = m_Random.GetRandom(0, *highBound);
   const char *threadName = Tasker::ThreadName(Tasker::LwrrentThread());

   Printf(Tee::PriLow, "%s is counting to %d.\n", threadName, target);

   Tasker::Yield();
   for (UINT32 i = 0; i <= target; i++) {
      Printf(Tee::PriDebug, "%s : %d\n", threadName, i);
      Tasker::Yield();
   }
   Printf(Tee::PriLow, "%s has finished counting.\n", threadName);
}

RC ThreadTest::Event()
{
   RC rc;
   Initialize();

   Printf(Tee::PriLow, "*** Event Test ***\n");
   Printf(Tee::PriLow, "A series of Tasker Event Tests.\n\n");
   rc = SetEvent();
   if (rc == OK)
   {
      rc = ResetEvent(false);
   }
   if (rc == OK)
   {
      rc = ResetEvent(true);
   }
   if (rc != OK)
      TestFailed();
   else
      TestPassed();

   return rc;
}

RC ThreadTest::SetEvent()
{
   const int REPEAT = 2;
   const UINT32 numThreads = 5;

   m_EventArr = new Tasker::EventID[numThreads];

   UINT32 i;
   for (i = 0; i < numThreads; i++) {
      m_EventArr[i] = Tasker::AllocEvent();
   }

   Tasker::ThreadID *threadArr = new Tasker::ThreadID[numThreads];

   // Run the whole thing REPEAT times, with the same Events, just to make sure we
   // can signal events multiple times.
   for (int j = 0; j < REPEAT; j++) {
      Printf(Tee::PriLow, "*** SetEvent *** (Round %d)\n", j + 1);
      Printf(Tee::PriLow, "Spawning %d threads and %d events.\n", numThreads, numThreads);
      Printf(Tee::PriLow, "Each thread will wait on one event, and a helper thread will randomly signal events.\n");

      char buf[64];
      for (i = 0; i < numThreads; i++) {
         sprintf(buf, "Event thread #%d", i+1);
         int rNum = m_Random.GetRandom(0, numThreads - 1);
         threadArr[i] = Tasker::CreateThread(ThreadTest::EventThread, &(m_EventArr[rNum]), 0, buf);
      }

      Tasker::ThreadID signallerThread = Tasker::CreateThread(
          ThreadTest::SignallerThread, (void*)(size_t)numThreads, 0, "Event Signaller");

      for (UINT32 i = 0; i < numThreads; i++) {
         Tasker::Join(threadArr[i]);
      }

      Tasker::Join(signallerThread);
   }

   for (i = 0; i < numThreads; i++) {
      Tasker::FreeEvent(m_EventArr[i]);
   }

   delete[] m_EventArr;
   delete[] threadArr;

   return OK;
}

RC ThreadTest::ResetEvent(bool bAutoReset)
{
   const UINT32 setResetLoops = 100;
   const UINT32 numThreads = 5;
   m_TimeoutMs = 1000;

   CountingEventData eventThreadData[numThreads];
   bool              bExitEventThreads = false;
   UINT32            waitCount = 0;
   Tasker::EventID eventId = Tasker::AllocEvent("ResetEvent", !bAutoReset);

   UINT32 i;
   for (i = 0; i < numThreads; i++)
   {
      eventThreadData[i].Event      = eventId;
      eventThreadData[i].Counter    = 0;
      eventThreadData[i].rc         = OK;
      eventThreadData[i].pWaitCount = &waitCount;
      eventThreadData[i].pbDone     = &bExitEventThreads;
   }

   Tasker::ThreadID *threadArr = new Tasker::ThreadID[numThreads];

   Printf(Tee::PriLow, "*** %sResetEvent ***\n",
          bAutoReset ? "Auto" : "Manual");
   Printf(Tee::PriLow, "Spawning %d threads to wait on a single event.\n",
          numThreads);
   if (bAutoReset)
   {
      Printf(Tee::PriLow,
            "Each thread will wait on the event, and the test thread will\n"
            "Set the event which will be auto reset when one thread wakes.\n");
   }
   else
   {
      Printf(Tee::PriLow,
             "Each thread will wait on the event, and the test thread \n"
             "will Set and immediately Reset the event.\n");
   }

   char buf[64];
   for (i = 0; i < numThreads; i++) {
      sprintf(buf, "%sResetEvent thread #%d",
              bAutoReset ? "Auto" : "Manual", i+1);
      threadArr[i] = Tasker::CreateThread(ThreadTest::CountingEventThread,
                                          &(eventThreadData[i]), 0, buf);
   }

   // Wait for all event threads to be waiting before starting the test
   while (waitCount != numThreads)
   {
      Tasker::Yield();
   }
   Tasker::Sleep(20);

   for (i = 0; i < setResetLoops; i++)
   {
      Tasker::SetEvent(eventId);
      if (!bAutoReset)
      {
         Tasker::ResetEvent(eventId);

         // Manual event support in the new Tasker requires some time after
         // a reset before a set
         Tasker::Sleep(10);
      }
      else
      {
         Tasker::Sleep(1);
      }
   }

   // Wait for any event threads to finish their operations
   Tasker::Sleep(20);

   // FreeEvent turns auto reset off and sets the event then waits
   // for all threads to process the event
   bExitEventThreads = true;
   Tasker::FreeEvent(eventId);
   for (UINT32 i = 0; i < numThreads; i++)
   {
      Tasker::Join(threadArr[i]);
   }

   delete[] threadArr;

   bool bFailed = false;
   UINT32 autoResetCount = 0;
   for (UINT32 i = 0; i < numThreads; i++)
   {
      if (OK != eventThreadData[i].rc)
      {
         Printf(Tee::PriNormal,
                "%sResetEvent thread #%d : Error %d (%s)\n",
                bAutoReset ? "Auto" : "Manual",
                i+1, (UINT32)eventThreadData[i].rc,
                eventThreadData[i].rc.Message());
         bFailed = true;
      }
      // When not testing auto reset, every thread should wake up for each set
      // so the counter in all threads should be the number of loops
      if (!bAutoReset && (eventThreadData[i].Counter != setResetLoops))
      {
         Printf(Tee::PriNormal,
                "ManualResetEvent thread #%d : Counter mismatch, expected %d, "
                "got %d\n",
                i+1, setResetLoops, eventThreadData[i].Counter);
         bFailed = true;
      }
      else if (bAutoReset)
      {
         // When testing auto reset, each set should only wake up one thread
         // so the sum of the counters in all threads should be the number of
         // loops
         Printf(Tee::PriLow,
                "AutoResetEvent thread #%d : Counter = %d\n",
                i+1, eventThreadData[i].Counter);
         autoResetCount += eventThreadData[i].Counter;
      }
   }

   if (bAutoReset && (autoResetCount != setResetLoops))
   {
      Printf(Tee::PriNormal,
             "AutoResetEvent Test : Counter mismatch, expected %d, got %d\n",
             setResetLoops, autoResetCount);
      bFailed = true;
   }
   return bFailed ? RC::SOFTWARE_ERROR : OK;
}

/**
 * This function prints out the name of the current thread and the
 * event it's waiting on.  It then waits on that event, then exits.
 *
 * @param arg An Tasker::EventID * which tells this thread the event it
 *            should wait on.
 */
void ThreadTest::EventThread(void *arg)
{
   Tasker::EventID *event = (Tasker::EventID *) arg;
   const char *threadName = Tasker::ThreadName(Tasker::LwrrentThread());
   Printf(Tee::PriLow, "\"%s\" waiting on event #%p.\n",
          threadName, *event);
   Tasker::WaitOnEvent(*event);
   Printf(Tee::PriLow, "\"%s\" woken up.  Now dying...\n", threadName);
}

/**
 *  Signals all events in ThreadTest::EventArr in random order.
 */
void ThreadTest::SignallerThread(void *arg)
{
    UINT32 numElements = (UINT32)(size_t) arg;
    m_Random.Shuffle(numElements, (void **)m_EventArr);
    for (UINT32 i = 0; i < numElements; i++) {
        Printf(Tee::PriLow, "Signalling Event %p\n", m_EventArr[i]);
        Tasker::SetEvent(m_EventArr[i]);
        Tasker::Yield();
   }
}

/**
 * This thread increments a counter every time it receives an
 * event until it is told to exit.
 *
 * @param arg A CountingEventData * which provides this thread
 *            an event and a counter to increment
 */
void ThreadTest::CountingEventThread(void *arg)
{
   CountingEventData *pData = (CountingEventData *) arg;
   const char *threadName = Tasker::ThreadName(Tasker::LwrrentThread());

   (*(pData->pWaitCount))++;
   while (!(*(pData->pbDone)))
   {
      pData->rc = Tasker::WaitOnEvent(pData->Event, m_TimeoutMs);
      if (OK != pData->rc)
      {
         Printf(Tee::PriNormal, "\"%s\" timeout.  Now dying...\n", threadName);
         return;
      }
      if (!(*(pData->pbDone)))
      {
         pData->Counter++;
      }
      Tasker::Sleep(1);
   }
   Printf(Tee::PriLow, "\"%s\" finished.  Now dying...\n", threadName);
}

RC ThreadTest::Benchmark()
{
   Initialize();

   RC rc;

   Printf(Tee::PriLow, "*** Benchmark Test ***\n");
   Printf(Tee::PriLow, "A series of Tasker benchmarks.\n\n");
   CHECK_RC(BlockingBenchmark());
   CHECK_RC(YieldingBenchmark());
   TestPassed();
   return OK;
}

FLOAT64 ThreadTest::DoBenchmark()
{
   return Utility::Benchmark(YieldFunc,
       (Platform::GetSimulationMode() == Platform::Fmodel)
       ? THREAD_BENCHMARK_LOOPS_FMODEL : THREAD_BENCHMARK_LOOPS);
}

RC ThreadTest::BlockingBenchmark()
{
   Printf(Tee::PriLow, "*** Blocking Benchmark ***\n");
   Printf(Tee::PriLow, "One active thread calling yield, an increasing number of threads blocking on the same event.\n");
   Printf(Tee::PriLow,
          "When blocking benchmark was called, there were %d threads in the Tasker:\n", Tasker::Threads());
   Tasker::PrintAllThreads(Tee::PriLow);

   const int numThreads = 20;
   Tasker::ThreadID *threadArr = new Tasker::ThreadID[numThreads];
   // Run the benchmark with an increasing number of
   // blocking threads.
   Tasker::EventID bigBlock = Tasker::AllocEvent();
   char buf[64];
   int i;
   int j;
   for (i = 0; i <= numThreads; i++) {
      for (j = 0; j < i; j++) {
         sprintf(buf, "Blocker thread #%d", j);
         threadArr[j] = Tasker::CreateThread(ThreadTest::BlockerThread, &bigBlock, 0, buf);
         // Yield to the new thread so that it will wait on
         // the event.
         Tasker::Yield();
      }
      // Now we have j threads set up and blocking on bigBlock.
      FLOAT64 secs = DoBenchmark();
      Printf(Tee::PriLow, "With %d thread(s) blocking, yield takes %E seconds.\n", i, secs);

      // Now wake and kill the j threads.
      Tasker::SetEvent(bigBlock);
      for (j = 0; j < i; j++)
         Tasker::Join(threadArr[j]);
   }
   Tasker::FreeEvent(bigBlock);
   delete[] threadArr;

   return OK;
}

/**
 * Waits on the event specified in arg, then returns.
 *
 * @param arg An (Tasker::EventID *) for the thread to wait on.
 */
void ThreadTest::BlockerThread(void *arg)
{
   MASSERT(arg != 0);
   Tasker::EventID *ev = (Tasker::EventID *)arg;
   Tasker::WaitOnEvent(*ev);
}

RC ThreadTest::YieldingBenchmark()
{
   Printf(Tee::PriLow, "*** Yielding Benchmark ***\n");
   Printf(Tee::PriLow, "An increasing number of active threads, all calling yield.\n");
   Printf(Tee::PriLow,
          "When yielding benchmark was called, there were %d threads in the Tasker:\n", Tasker::Threads());
   Tasker::PrintAllThreads(Tee::PriLow);

   const int numThreads = 20;
   Tasker::ThreadID *threadArr = new Tasker::ThreadID[numThreads];
   // Run the benchmark with an increasing number of
   // blocking threads.
   char buf[64];
   int i;
   int j;
   for (i = 0; i <= numThreads; i++) {
      for (j = 0; j < i; j++) {
         sprintf(buf, "Yielder thread #%d", j);
         threadArr[j] = Tasker::CreateThread(ThreadTest::YielderThread, 0, 0, buf);
      }
      // Now we have j threads set up.
      FLOAT64 secs = DoBenchmark();
      Printf(Tee::PriLow, "With %d thread(s) yielding, yield takes %E seconds.\n", i, secs);

      // Now wake and kill the j threads.
      for (j = 0; j < i; j++) {
         Tasker::Join(threadArr[j]);
      }
   }
   delete[] threadArr;

   return OK;
}

/**
 * Calls yield THREAD_BENCHMARK_LOOPS number of times, then
 * exits.
 *
 * @param arg Unused.
 */
void ThreadTest::YielderThread(void *arg)
{
    INT32 benchmarkLoops =
        (Platform::GetSimulationMode() == Platform::Fmodel)
        ? THREAD_BENCHMARK_LOOPS_FMODEL : THREAD_BENCHMARK_LOOPS;

    for (INT32 i = 0; i < benchmarkLoops; i++)
    {
        Tasker::Yield();
    }
}

void ThreadTest::YieldFunc()
{
   Tasker::Yield();
}

RC ThreadTest::TimeoutTest()
{
   Initialize();

   const int numThreads = 10;

   Printf(Tee::PriLow, "*** Timeout Test ***\n");
   Printf(Tee::PriLow, "Spawns %d threads, each set to timeout after a certain period.\n\n", numThreads);

   Tasker::ThreadID *threadArr = new Tasker::ThreadID[numThreads];

   char buf[64];
   int i;
   for (i = 0; i < numThreads; i++) {
      sprintf(buf, "Timeout thread #%d", i);
      threadArr[i] = Tasker::CreateThread(ThreadTest::TimeoutThread, 0, 0, buf);
   }

   m_TimeoutStart = (FLOAT64)(INT64)Xp::QueryPerformanceCounter();

   for (i = 0; i < numThreads; i++) {
      Tasker::Join(threadArr[i]);
   }

   delete[] threadArr;

   TestPassed();

   return OK;
}

void ThreadTest::TimeoutThread(void *args)
{
   const int MAX_SLEEP_MS = 2000;
   int ms = m_Random.GetRandom(1, MAX_SLEEP_MS);
   const char *threadName = Tasker::ThreadName(Tasker::LwrrentThread());
   Printf(Tee::PriLow, "%s is timing out in %.1f seconds.\n", threadName, ms/1000.0);

   Tasker::WaitOnEvent(NULL, ms);

   FLOAT64 End = (FLOAT64) (INT64) Xp::QueryPerformanceCounter();
   MASSERT(End > m_TimeoutStart);
   FLOAT64 sleptSecs = (End - m_TimeoutStart)/(m_PerfFreq);

   Printf(Tee::PriLow, "%s woke up!  Slept for %g seconds.\n", threadName, sleptSecs);
}

RC ThreadTest::JoinTest()
{
   StickyRC finalRc;
   Initialize();

   m_SecondSleeper = Tasker::NULL_THREAD;

   Tasker::ThreadID sleeper =
       Tasker::CreateThread(ThreadTest::JoinSleeperThread, 0, 0, "sleeper");
   Tasker::ThreadID joinAndCreater =
       Tasker::CreateThread(ThreadTest::JoinAndCreateThread,
                            &sleeper, 0,
                            "join-and-create");

   Tasker::Sleep(10);
   RC rc = Tasker::Join(joinAndCreater);
   finalRc = rc;
   if (OK != rc)
   {
      rc.Clear();
      Printf(Tee::PriHigh, "Join failed \"joinAndCreater\"\n");
   }

   Tasker::Yield();

   // Grab the extra sleeper
   rc = Tasker::Join(m_SecondSleeper);
   finalRc = rc;
   if (OK != rc)
   {
      Printf(Tee::PriHigh, "Join failed \"secondSleeper\"\n");
   }
   Tasker::PrintAllThreads(Tee::PriLow);

   if (OK == finalRc)
      TestPassed();
   else
      TestFailed();

   return finalRc;
}

void ThreadTest::JoinAndCreateThread(void *args)
{
    // Create 2nd-sleeper before joining anything, so that DOS tasker
    // doesn't reuse thread-ids and screw up the joins in the main thread.
    m_SecondSleeper = Tasker::CreateThread(ThreadTest::JoinSleeperThread, 0, 0,
            "join-and-create-2");

    Tasker::Join(*reinterpret_cast<Tasker::ThreadID*>(args));
}

void ThreadTest::JoinSleeperThread(void *args)
{
    Tasker::Sleep(1);
}

RC ThreadTest::PollTest(FLOAT64 timeoutMs)
{
   Initialize();

   MASSERT(timeoutMs >= 0);

   m_TimeoutMs = timeoutMs;
   int percentArr[] = {0, 1, 10, 25, 50, 100};

   const int numThreads = sizeof(percentArr)/sizeof(int);
   Tasker::ThreadID *threadArr = new Tasker::ThreadID[numThreads];

   char buf[64];
   int i;
   for (i = 0; i < numThreads; i++) {
      sprintf(buf, "%d%% thread", percentArr[i]);
      threadArr[i] = Tasker::CreateThread(ThreadTest::PollThread, (void *)(uintptr_t)percentArr[i], 0, buf);
   }

   m_PollStart = (FLOAT64)(INT64)Xp::QueryPerformanceCounter();
   for (i = 0; i < numThreads; i++) {
      Tasker::Join(threadArr[i]);
   }
   delete [] threadArr;

   TestPassed();

   return OK;
}

/**
 * @param args In reality an integer (not an int *) between 0 and
 *             100 (inclusive) describing what percentage of the time the
 *             function should return true.
 *
 * @return A pseudo-random result based on args.
 */
bool ThreadTest::RandomTrue(void *args)
{
    int percent = (int)(size_t) args;
    MASSERT(percent >= 0);
    MASSERT(percent <= 100);
    int rNum = m_Random.GetRandom(1, 100);

    return (rNum <= percent);
}

/**
 * @param args In reality an integer (not an int *) between 0 and
 *             100 (inclusive) describing the percentag chance
 *             this thread's poll function has of returning true.
 */
void ThreadTest::PollThread(void *args)
{
   RC rc = Tasker::Poll(ThreadTest::RandomTrue, args, m_TimeoutMs);
   if (RC::TIMEOUT_ERROR == rc) {
      Printf(Tee::PriLow, "Poll thread with percentage set to %p%% timed out.\n", args);
      FLOAT64 End = (FLOAT64)(INT64)Xp::QueryPerformanceCounter();
      MASSERT(End > m_PollStart);
      FLOAT64 timeSecs = ((FLOAT64)End - m_PollStart) / m_PerfFreq;
      Printf(Tee::PriLow, "** Timeout was after %E seconds.\n", timeSecs);
   }
   else {
      Printf(Tee::PriLow, "Poll thread with percentage set to %p%% hit true.\n", args);
   }
}

void ThreadTest::TestPassed()
{
   Printf(Tee::PriLow, "*** PASSED ***\n\n");
}
void ThreadTest::TestFailed()
{
   Printf(Tee::PriNormal, "*** FAILED ***\n\n");
}

RC ThreadTest::SemaphoreTest()
{
    Initialize();

    const int numThreads = 3;

    Printf(Tee::PriLow, "*** Semaphore Test ***\n");
    Printf(Tee::PriLow, "Spawns %d threads for semaphore.\n\n", numThreads);

    Tasker::ThreadID *threadArr = new Tasker::ThreadID[numThreads];

    char buf[64];
    sprintf(buf, "semaphore thread #%d", Tasker::LwrrentThread());

    SemaID sema4  = Tasker::CreateSemaphore(3, buf);

    threadArr[0] = Tasker::CreateThread(ThreadTest::Sema4Thread1, sema4, 0, buf);
    sprintf(buf, "semaphore thread #%d", 1);
    threadArr[1] = Tasker::CreateThread(ThreadTest::Sema4Thread2, sema4, 0, buf);
    sprintf(buf, "semaphore thread #%d", 2);
    threadArr[2] = Tasker::CreateThread(ThreadTest::Sema4Thread3, sema4, 0, buf);
    sprintf(buf, "semaphore thread #%d", 3);

    Tasker::WaitOnEvent(NULL, 5000);     // wait for 5 seconds

    Printf(Tee::PriLow, "Main thread tries to destroy semaphore\n");
    Tasker::DestroySemaphore(sema4);

    for (int i = 0; i < numThreads; i++) {
      Tasker::Join(threadArr[i]);
    }

    delete[] threadArr;

    TestPassed();

    return OK;
}

// last thread to start
void ThreadTest::Sema4Thread1(void *args)
{
    Printf(Tee::PriLow, "Thread 1 starts. Waits for 1 seconds\n");
    Tasker::WaitOnEvent(NULL, 1000);     // wait for 1 seconds
    Printf(Tee::PriLow, "Thread 1: acquires semaphore - will block with thread 2\n");
    RC rc = Tasker::AcquireSemaphore((SemaID)args, Tasker::NO_TIMEOUT);
    if(rc == RC::SOFTWARE_ERROR)
        Printf(Tee::PriLow, "Thread 1 Got software error: because mainthread tries to destroy semaphore\n");
    else    // something's wrong
        MASSERT(!"thread 1 is expecting error... but no error received");

    Printf(Tee::PriHigh, "Thread 1: dying\n");
}
// this thread starts second
void ThreadTest::Sema4Thread2(void *args)
{
    Printf(Tee::PriLow, "Thread 2 starts - tries to acquire semaphore with wait = 500ms\n");
    RC rc = Tasker::AcquireSemaphore((SemaID)args, 500);
    if(rc == RC::TIMEOUT_ERROR)
    {
        Printf(Tee::PriLow, "Thread 2 Got timeout error: expecting timeout error here\n");
    }
    else    // something's wrong
    {
        MASSERT(!"thread 2 is expecting error... but no error received.. exiting thread");
        return;
    }
    Printf(Tee::PriLow, "Thread 2: Tries to acquire semaphore - will block\n");
    rc = Tasker::AcquireSemaphore((SemaID)args, Tasker::NO_TIMEOUT);
    MASSERT(rc == OK);
    Printf(Tee::PriLow, "Thread 2: got semaphore ... now dying\n");
}

// this thread starts first
void ThreadTest::Sema4Thread3(void *args)
{
    Printf(Tee::PriLow, "Thread 3 starts and acquires 3 semaphores- then wait for 2 seconds\n");
    RC rc = Tasker::AcquireSemaphore((SemaID)args, Tasker::NO_TIMEOUT);
    MASSERT(rc == OK);
    rc = Tasker::AcquireSemaphore((SemaID)args, Tasker::NO_TIMEOUT);
    MASSERT(rc == OK);
    rc = Tasker::AcquireSemaphore((SemaID)args, Tasker::NO_TIMEOUT);
    MASSERT(rc == OK);
    Tasker::WaitOnEvent(NULL, 4000);     // wait for 4 seconds
    Printf(Tee::PriLow, "Thread 3: Release 1 Semaphore\n");
    Tasker::ReleaseSemaphore((SemaID)args);
    Printf(Tee::PriLow, "Thread 3: dying\n");
}

//------------------------------------------------------------------------------
// ThreadTest object, properties and methods.
//------------------------------------------------------------------------------

JS_CLASS(ThreadTest);

//! Test of Tasker, Thread, and Event functionality.
/**
 * Contains many sub-tests.
 */
static SObject ThreadTest_Object
(
 "ThreadTest",
 ThreadTestClass,
 0,
 0,
 "Test of Tasker, Thread, and Event functionality."
);

C_(ThreadTest_RunAll);
//! Run all of the subtests sequentially.
static STest ThreadTest_RunAll
(
 ThreadTest_Object,
 "RunAll",
 C_ThreadTest_RunAll,
 0,
 "Run all tasker/event/thread tests."
);

C_(ThreadTest_Event);
//! Run the event test.
static STest ThreadTest_Event
(
 ThreadTest_Object,
 "Event",
 C_ThreadTest_Event,
 0,
 "Run event test."
);

C_(ThreadTest_Benchmark);
//! Run the benchmark test.
static STest ThreadTest_Benchmark
(
 ThreadTest_Object,
 "Benchmark",
 C_ThreadTest_Benchmark,
 0,
 "Run benchmark test."
);

C_(ThreadTest_RandNum);
//! Run the random number test.
static STest ThreadTest_RandNum
(
 ThreadTest_Object,
 "RandNum",
 C_ThreadTest_RandNum,
 0,
 "Run random number test."
);

C_(ThreadTest_Timeout);
//! Run the timeout test.
static STest ThreadTest_Timeout
(
 ThreadTest_Object,
 "Timeout",
 C_ThreadTest_Timeout,
 0,
 "Run timeout test."
);

C_(ThreadTest_Join);
//! Run the Join test.
static STest ThreadTest_Join
(
 ThreadTest_Object,
 "Join",
 C_ThreadTest_Join,
 0,
 "Run Join test."
);

C_(ThreadTest_Poll);
//! Run the poll test.
static STest ThreadTest_poll
(
 ThreadTest_Object,
 "Poll",
 C_ThreadTest_Poll,
 1,
 "Run poll test."
);

C_(ThreadTest_Semaphore);
//! Run the Semaphore test.
static STest ThreadTest_Semaphore
(
 ThreadTest_Object,
 "Semaphore",
 C_ThreadTest_Semaphore,
 0,
 "Run Semaphore test."
);

// Implementation
C_(ThreadTest_RunAll)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   // This is a void method.
   if (NumArguments != 0)
   {
      JS_ReportError(pContext, "Usage: ThreadTest.RunAll()");
      return JS_FALSE;
   }

   RETURN_RC(ThreadTest::RunAll());
}

C_(ThreadTest_Event)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0) {
      JS_ReportError(pContext, "Usage: ThreadTest.Event()");
      return JS_FALSE;
   }

   RETURN_RC(ThreadTest::Event());
}

C_(ThreadTest_Benchmark)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0) {
      JS_ReportError(pContext, "Usage: ThreadTest.Benchmark()");
      return JS_FALSE;
   }

   RETURN_RC(ThreadTest::Benchmark());
}

C_(ThreadTest_RandNum)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0) {
      JS_ReportError(pContext, "Usage: ThreadTest.RandNum()");
      return JS_FALSE;
   }

   RETURN_RC(ThreadTest::RandomNumbers());
}

C_(ThreadTest_Timeout)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0) {
      JS_ReportError(pContext, "Usage: ThreadTest.Timeout()");
      return JS_FALSE;
   }

   RETURN_RC(ThreadTest::TimeoutTest());
}

C_(ThreadTest_Join)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0) {
      JS_ReportError(pContext, "Usage: ThreadTest.Join()");
      return JS_FALSE;
   }

   RETURN_RC(ThreadTest::JoinTest());
}

C_(ThreadTest_Poll)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   JavaScriptPtr pJavaScript;

   FLOAT64 timeoutSecs;

   if (NumArguments != 1
       || OK != pJavaScript->FromJsval(pArguments[0], &timeoutSecs))
   {
      JS_ReportError(pContext, "Usage: ThreadTest.Poll(timeoutSecs)");
      return JS_FALSE;
   }

   RETURN_RC(ThreadTest::PollTest(timeoutSecs * 1000));
}

C_(ThreadTest_Semaphore)
{
   MASSERT(pContext     != 0);
   MASSERT(pArguments   != 0);
   MASSERT(pReturlwalue != 0);

   if (NumArguments != 0) {
      JS_ReportError(pContext, "Usage: ThreadTest.Semapohre()");
      return JS_FALSE;
   }

   RETURN_RC(ThreadTest::SemaphoreTest());
}
