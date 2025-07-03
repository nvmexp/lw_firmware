/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012,2016,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   threadpool.h
 * @brief  Header file for a simple thread pool implementation using Tasker
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_THREADPOOL_H
#define INCLUDED_THREADPOOL_H

#include "tasker.h"
#include <vector>
#include <queue>
#include "cpu.h"

namespace Tasker
{
    /**
     * \class ThreadPool
     *
     * \brief Provides a simple implementation of threadpool using Tasker
     *
     * This class can be used to initialize a group of threads that perform
     * tasks submitted in a queue.
     * Construct the object with the number of threads to create.
     * Calling Initialize will create the threads,
     * ShutDown joins the threads. The class is templated on a type which
     * should support (). They should also be copyable.
     *
     * Sample Usage:
     * ThreadPool<void(*)()> p();
     * p.Initialize(5);
     * p.EnqueueTask(foo);
     * p.ShutDown();
     *
     *
     * ShutDown is automatically called when the object is destroyed
     *
     */
    template<typename Task>
    class ThreadPool
    {

    public:

        //! Construct object with number of threads in the pool
        //!< @param Number of threads to be used in pool
        ThreadPool() :
                 m_Exit(0)
                 ,m_WorkQueueMutex(Tasker::AllocMutex("ThreadPool::m_WorkQueueMutex", Tasker::mtxUnchecked))
                 ,m_QueueSema(Tasker::CreateSemaphore(0,"Work Queue semaphore"))
                 ,m_InitializeMutex(Tasker::AllocMutex("ThreadPool::m_InitializeMutex", Tasker::mtxUnchecked))
                 ,m_EnqueuedTasks(0)
                 ,m_CompletedTasks(0)
        {
            MASSERT(m_WorkQueueMutex);
            MASSERT(m_QueueSema);
            MASSERT(m_InitializeMutex);
        }

        //! Creates threads, mutexes,semaphores. Threads wait for work in the
        //! queue
        RC Initialize(UINT32 numThreads, bool detachedThreads)
        {
            Tasker::MutexHolder holder(m_InitializeMutex);

            if (!m_Threads.empty()|| numThreads==0)
                return RC::SOFTWARE_ERROR;

            if (!m_QueueSema)
            {
                MASSERT(!"QueueSemaphore creation failed");
                return RC::CANNOT_ALLOCATE_MEMORY;
            }
            m_Threads = Tasker::CreateThreadGroup(
                (detachedThreads ? DetachedThreadPoolThreadFunc : ThreadPoolThreadFunc),
                this,
                numThreads,
                "ThreadPoolGroup",
                true,
                NULL);
            if (m_Threads.empty())
            {
                MASSERT(!"Thread Creation Failed");
                return RC::SOFTWARE_ERROR;
            }
            return OK;
        }

        //! Block/Wait until the threads finish running all the workitems.
        //! Calling this from a worker function, might lead to a deadlock !!
        void WaitUntilWorkQueueEmpty(FLOAT64 milliseconds)
        {
            while (!IsQueueEmpty())
                Tasker::Sleep(milliseconds);
        }

        // This function waits for tasks to complete once the queue has been
        // depleted. Careful: this function does not do diligence in protecting
        // against race-conditions for enqueuing tasks while simultaneously
        // waiting for tasks to complete.
        void WaitUntilTasksCompleted(FLOAT64 milliseconds)
        {
            WaitUntilWorkQueueEmpty(milliseconds);

            MASSERT(m_EnqueuedTasks);

            // Wait for Number of Completed tasks to catch up to Enqueued tasks.
            // This code is expecting that only m_CompletedTasks might change.
            while (m_EnqueuedTasks != m_CompletedTasks)
                Tasker::Sleep(milliseconds);

            // Reset Enqueued/Completed task counters
            Cpu::AtomicXchg32(reinterpret_cast<volatile UINT32*>(&m_EnqueuedTasks), 0);
            Cpu::AtomicXchg32(reinterpret_cast<volatile UINT32*>(&m_CompletedTasks), 0);
        }

        //! Calls Join on the threads, and frees Mutexes , Semaphores.
        void ShutDown()
        {
            Tasker::MutexHolder holder;
            if (m_InitializeMutex)
                holder.Acquire(m_InitializeMutex);

            // If already shutdown, nothing to do.
            if (!m_QueueSema || m_Threads.empty())
                return;
            WaitUntilWorkQueueEmpty(100.0f);

            // Now, tell the threads to exit
            SetExit(true);
            /// ReleaseSemaphore as many times as there are threads so that the
            /// threads can finish(Not wait acquiring for sem)
            for (UINT32 i = 0; i <m_Threads.size() ; ++i)
                Tasker::ReleaseSemaphore(m_QueueSema);

            Tasker::Join(m_Threads, Tasker::NO_TIMEOUT);
            m_Threads.clear();
            SetExit(false);
        }

        //! Destructor calls ShutDown.
        ~ThreadPool()
        {
            ShutDown();
            if (m_WorkQueueMutex)
                Tasker::FreeMutex(m_WorkQueueMutex);
            m_WorkQueueMutex = 0;
            if (m_InitializeMutex)
                Tasker::FreeMutex(m_InitializeMutex);
            m_InitializeMutex=0;
            Tasker::DestroySemaphore(m_QueueSema);
            m_QueueSema = 0;
        }

        //! Use this method to enqueue tasks . The task should be a type
        //! defining operator(), and should be copy constructable.
        //! Function pointers,and functors
        void EnqueueTask(Task &f)
        {
            Tasker::MutexHolder holder(m_WorkQueueMutex);
            m_WorkQueue.push(f);
            Tasker::ReleaseSemaphore(m_QueueSema);
            Cpu::AtomicAdd(&m_EnqueuedTasks, 1);
        }

        //! Returns if the work queue is empty
        bool IsQueueEmpty() const
        {
            Tasker::MutexHolder holder(m_WorkQueueMutex);
            return m_WorkQueue.empty();
        }

    private:

        void SetExit(bool val)
        {
            INT32 data = (val ? 1 : -1);
            Cpu::AtomicAdd(&m_Exit, data);
        }

        //! Are we done with done with the exelwtion
        bool IsExit()
        {
            return (Cpu::AtomicRead(&m_Exit) != 0);
        }

        //! The main method used by each thread for pulling a task off the
        //! queue and execute
        void WorkerThread()
        {
            while (true)
            {
                if (OK==Tasker::AcquireSemaphore(m_QueueSema,Tasker::NO_TIMEOUT))
                {
                    if (IsExit())
                        return;
                    Tasker::MutexHolder holder(m_WorkQueueMutex);
                    Task f = m_WorkQueue.front();
                    m_WorkQueue.pop();
                    holder.Release();
                    f();
                    Cpu::AtomicAdd(&m_CompletedTasks, 1);
                }
            }
        }

        static void ThreadPoolThreadFunc(void *arg)
        {
            static_cast<ThreadPool*>(arg)->WorkerThread();
        }

        static void DetachedThreadPoolThreadFunc(void *arg)
        {
            Tasker::DetachThread detach;
            static_cast<ThreadPool*>(arg)->WorkerThread();
        }

        ThreadPool operator=(const ThreadPool &other);
        ThreadPool(const ThreadPool &other);

        ///////////////////////
        /// Private Members ///
        ///////////////////////

        volatile INT32 m_Exit; //!< variable used by threads to check for exit
        void *m_WorkQueueMutex;
        std::queue<Task> m_WorkQueue;
        std::vector<Tasker::ThreadID> m_Threads;
        SemaID m_QueueSema;
        void *m_InitializeMutex;
        volatile INT32 m_EnqueuedTasks;
        volatile INT32 m_CompletedTasks;
    };

}
#endif /* INCLUDED_THREADPOOL_H */

