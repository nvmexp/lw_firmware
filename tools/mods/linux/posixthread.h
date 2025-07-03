/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   posixthread.h
 * @brief  pthreads-based implementation of native threads.
 */

#ifndef INCLUDED_POSIXTHREAD_H
#define INCLUDED_POSIXTHREAD_H

#include "linuxmutex.h"
#include "core/include/types.h"
#include "core/include/rc.h"
#include <time.h>
#include <vector>

namespace Tasker
{
    namespace NativeTlsValue
    {
        inline unsigned Alloc()
        {
            pthread_key_t key;
            pthread_key_create(&key, 0);
            return (unsigned)key;
        }
        inline void Free(unsigned key)
        {
            pthread_key_delete((pthread_key_t)key);
        }
        inline void Set(unsigned key, void* value)
        {
            pthread_setspecific((pthread_key_t)key, value);
        }
        inline void* Get(unsigned key)
        {
            return pthread_getspecific((pthread_key_t)key);
        }
    }

    class NativeCondition
    {
        public:
            NativeCondition()
            {
                pthread_cond_init(&m_Cond, 0);
            }
            ~NativeCondition()
            {
                pthread_cond_destroy(&m_Cond);
            }
            void Signal()
            {
                pthread_cond_signal(&m_Cond);
            }
            void Broadcast()
            {
                pthread_cond_broadcast(&m_Cond);
            }
            enum WaitResult
            {
                success,
                timeout,
                error
            };
            WaitResult Wait(LinuxMutex& mutex);
            WaitResult Wait(LinuxMutex& mutex, double timeoutMs);

        private:
            // Non-copyable
            NativeCondition(const NativeCondition&);
            NativeCondition& operator=(const NativeCondition&);

            pthread_cond_t m_Cond;
    };

    namespace NativeThread
    {
        typedef pthread_t Id;
        typedef void (*Func)(void*);
        inline bool Create(Id* pId, Func func, void* pInit, const char *name)
        {
            typedef void* (*PosixFunc)(void*);
            return 0 == pthread_create(pId, 0, (PosixFunc)func, pInit);
        }
        inline void Exit()
        {
            pthread_exit(0);
        }
        inline void Join(Id id)
        {
            pthread_join(id, 0);
        }
        inline void Detach(Id id)
        {
            pthread_detach(id);
        }
        inline Id Self()
        {
            return pthread_self();
        }
        inline void SleepUS(unsigned long long usec)
        {
            timespec t;
            t.tv_sec = static_cast<time_t>(usec / 1000000);
            t.tv_nsec = static_cast<long>((usec % 1000000) * 1000);
            nanosleep(&t, 0);
        }
        RC SetCpuAffinity(Id id, const vector<UINT32> &cpuMasks);
        inline void Yield()
        {
#if defined(MACOSX) || defined(TEGRA_MODS)
            sched_yield();
#else
            pthread_yield();
#endif
        }
    }
}

#endif // !INCLUDED_POSIXTHREAD_H
