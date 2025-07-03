/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2012 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   unit_threadpool.cpp
 * @brief  Unit tests for the threadpool implementation
 */

#include "unittest.h"
#include "core/include/threadpool.h"
#include <vector>
#include <memory>
#include <numeric>
#include "core/include/tasker.h"
using namespace Tasker;
namespace
{
    class ThreadPoolTest: public UnitTest
    {
    public:
        ThreadPoolTest();
        virtual ~ThreadPoolTest();
        virtual void Run();
    private:
        void SimpleCreatePoolTest();
        void ParallelSumTest(bool, UINT32, UINT32, bool);
        void SimpleDoubleTest(bool);
    };

    ThreadPoolTest::ThreadPoolTest() :
            UnitTest("ThreadPoolTest")
    {
    }

    ThreadPoolTest::~ThreadPoolTest()
    {

    }
    ADD_UNIT_TEST(ThreadPoolTest);

    void ThreadPoolTest::Run()
    {
        SimpleCreatePoolTest();
        SimpleDoubleTest(false);
        //test detaching (since this test is threadsafe)
        SimpleDoubleTest(true);
        ParallelSumTest(false, 1, 10, false);
        ParallelSumTest(false, 10, 100, false);
        ParallelSumTest(false, 100, 500, false);
        ParallelSumTest(false, 50, 5000, false);
        ParallelSumTest(false, 100, 5000, false);
        ParallelSumTest(false, 200, 1000, false);

        //Check Reusing the pool
        ParallelSumTest(false, 100, 500, true);
    }

    void ThreadPoolTest::SimpleCreatePoolTest()
    {
        typedef void (*func)();
        std::vector<ThreadPool<func>*> pools;
        for (UINT32 i = 0; i < 10; i++)
        {
            ThreadPool<func> *pool = new ThreadPool<func>();
            pool->Initialize(3);
            pools.push_back(pool);
        }

        for (UINT32 i = 0; i < 10; ++i)
        {
            delete pools[i];
        }
    }

    template<typename Iterator>
    struct DoubleFunctor
    {
        Iterator begin, end;
        DoubleFunctor(Iterator _begin, Iterator _end) :
                begin(_begin), end(_end)
        {
        }
        void operator ()()
        {
            for (; begin != end; ++begin)
            {
                *begin *= 2;
            }
        }

    };

    void ThreadPoolTest::SimpleDoubleTest(bool detach)
    {
        std::unique_ptr<Tasker::DetachThread> det(
                detach ? new Tasker::DetachThread : NULL);
        std::vector<UINT32> vec;
        for (UINT32 i = 0; i < 100; ++i)
        {
            vec.push_back(i);
        }

        // creating a threadpool with four threads
        typedef std::vector<UINT32>::iterator vec_it;
        typedef DoubleFunctor<vec_it> t_functor;
        ThreadPool<t_functor> *pool = new ThreadPool<t_functor>();
        t_functor f(vec.begin(), vec.begin() + 25);
        t_functor f1(vec.begin() + 25, vec.begin() + 50);
        t_functor f2(vec.begin() + 50, vec.begin() + 75);
        t_functor f3(vec.begin() + 75, vec.begin() + 100);
        pool->Initialize(3);
        pool->EnqueueTask(f);
        pool->EnqueueTask(f1);
        pool->EnqueueTask(f2);
        pool->EnqueueTask(f3);
        //wait for it to finish
        pool->ShutDown();
        delete pool;
        for (UINT32 i = 0; i < 100; ++i)
        {
            UNIT_ASSERT_EQ(2*i, vec[i]);
        }
    }

    template<typename Iterator, typename T>
    struct Sum_Functor
    {
        Iterator begin, end, target;
        Sum_Functor(Iterator _beg, Iterator _end, Iterator _tar) :
                begin(_beg), end(_end), target(_tar)
        {
        }
        void operator()()
        {
            *target = std::accumulate(begin, end, 0);
        }
    };

    template<typename Iterator, typename T>
    struct Aclwmulator
    {
        T operator()(T x, T y)
        {
            return x + y;
        }
    };

    void ThreadPoolTest::ParallelSumTest(bool detach, UINT32 numThreads,
            UINT32 numWorkFunctions, bool reuse)
    {
        UINT32 arraysize = 10000;
        UINT32 functorsize = arraysize / numWorkFunctions;
        std::unique_ptr<Tasker::DetachThread> det(
                detach ? new Tasker::DetachThread : NULL);

        std::vector<UINT32> vec;
        std::vector<UINT32> vec_target(numWorkFunctions, 0);
        for (UINT32 i = 0; i < arraysize; ++i)
        {
            vec.push_back(1);
        }

        typedef Sum_Functor<std::vector<UINT32>::iterator, size_t> t_functor;
        ThreadPool<t_functor> *pool = new ThreadPool<t_functor>();
        pool->Initialize(numThreads);
        for (UINT32 i = 1; i <= numWorkFunctions; ++i)
        {
            UINT32 start = (i - 1) * functorsize;
            UINT32 end = (
                    i == numWorkFunctions ? arraysize : start + functorsize);
            t_functor f(vec.begin() + start, vec.begin() + end,
                    vec_target.begin() + i - 1);
            pool->EnqueueTask(f);
        }
        pool->ShutDown();
        UINT32 sum = std::accumulate(vec_target.begin(), vec_target.end(), 0);
        UNIT_ASSERT_EQ(sum, arraysize);

        //Run the same test again,using the same pool , make sure that it works
        //Also test with queuing tasks before initialize
        if (reuse)
        {
            for (UINT32 i = 1; i <= numWorkFunctions; ++i)
            {
                UINT32 start = (i - 1) * functorsize;
                UINT32 end = (
                       i == numWorkFunctions ? arraysize : start + functorsize);
                t_functor f(vec.begin() + start, vec.begin() + end,
                        vec_target.begin() + i - 1);
                pool->EnqueueTask(f);
            }
            pool->Initialize(numThreads);
            pool->ShutDown();

            //test multiple shutdowns to make sure we don't run into a crash
            pool->ShutDown();
            pool->ShutDown();
            UINT32 sum = std::accumulate(vec_target.begin(), vec_target.end(),
                    0);
            UNIT_ASSERT_EQ(sum, arraysize);
        }
        delete pool;
    }

}
