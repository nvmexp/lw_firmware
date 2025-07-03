/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2010-2011,2014-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file   posixthread.cpp
 */

#include "posixthread.h"
#include "core/include/xp.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "core/include/tee.h"
#include <errno.h>
#include <time.h>
#include <sys/time.h>

// The general algorithm for setting up CPU affinit is the same on all platforms
// however the calls that need to be made to actually perform the operation are
// different depending upon the platform
#if !defined(MACOSX)
    #if defined(BIONIC)
        #include <sched.h>
        #include <unistd.h>
        #define TID_TYPE pid_t
        #define GETTID(threadId)  gettid()
        #define ILWALID_TID(__tid) (__tid <= 0)
        #define MODS_CPU_SETAFFINITY_S(i,maskSize,mask)  sched_setaffinity(i, sizeof(cpu_set_t), &mask)
        #define MODS_CPU_SET_S(i,maskSize,mask)  CPU_SET(i, &mask)
        #define MODS_CPU_FREE(mask) ((void)0)
    #elif defined(QNX)
        #include <pthread.h>
        #include <sys/neutrino.h>
        #include <sys/syspage.h>
        #define TID_TYPE pthread_t
        #define GETTID(threadId)  threadId
        #define ILWALID_TID(__tid) (__tid <= 0)
        #define MODS_CPU_SETAFFINITY_S(i,maskSize,mask)  ThreadCtl(_NTO_TCTL_RUNMASK, (void*)&mask)
        #define MODS_CPU_SET_S(i,maskSize,mask) RMSK_SET(i, &mask)
        #define MODS_CPU_FREE(mask) (void(0))
        #define cpu_set_t UINT32
        #define CPU_SETSIZE (UINT32(RMSK_SIZE(_syspage_ptr->num_cpu)))
        #define CPU_ZERO(x) (*x=0)
    #else
        #if __GLIBC_PREREQ(2, 7)
            #include <pthread.h>
            #define TID_TYPE pthread_t
            #define GETTID(threadId)  threadId
            #define ILWALID_TID(__tid) (false)
            #define DYNAMIC_CPU_SET
            #define MODS_CPU_SETAFFINITY_S(i,pMaskSize,pMask)  pthread_setaffinity_np(i, pMaskSize, pMask)
            #define MODS_CPU_SET_S(i,pMaskSize,pMask)  CPU_SET_S(i, pMaskSize, pMask)
            #define MODS_CPU_FREE(pMask) CPU_FREE(pMask)
        #elif __GLIBC_PREREQ(2, 4)
            #include <pthread.h>
            #define TID_TYPE pthread_t
            #define GETTID(threadId)  threadId
            #define ILWALID_TID(__tid) (false)
            #define MODS_CPU_SETAFFINITY_S(i,maskSize,mask)  pthread_setaffinity_np(i, sizeof(cpu_set_t), &mask)
            #define MODS_CPU_SET_S(i,maskSize,mask)  CPU_SET(i, &mask)
            #define MODS_CPU_FREE(mask) ((void)0)
        #else
            #include <sched.h>
            #include <sys/syscall.h>
            #define TID_TYPE pid_t
            #define GETTID(threadId)  syscall(SYS_gettid)
            #define ILWALID_TID(__tid) (__tid <= 0)
            #define MODS_CPU_SETAFFINITY_S(i,maskSize,mask)  sched_setaffinity(i, &mask)
            #define MODS_CPU_SET_S(i,maskSize,mask)  CPU_SET(i, &mask)
            #define MODS_CPU_FREE(mask) ((void)0)
        #endif
    #endif
#endif

namespace
{
    Tasker::NativeCondition::WaitResult TranslateError(int ret)
    {
        switch (ret)
        {
        case 0:
            return Tasker::NativeCondition::success;
        case ETIMEDOUT:
            return Tasker::NativeCondition::timeout;
        default:
            break;
        }
        return Tasker::NativeCondition::error;
    }
}

Tasker::NativeCondition::WaitResult Tasker::NativeCondition::Wait
(
    LinuxMutex& mutex
)
{
    const int ret = pthread_cond_wait(&m_Cond, mutex);
    return TranslateError(ret);
}

Tasker::NativeCondition::WaitResult Tasker::NativeCondition::Wait
(
    LinuxMutex& mutex,
    double timeoutMs
)
{
    // Ensure input is reasonable
    MASSERT(timeoutMs < 365.0 * 24.0 * 60.0 * 60.0 * 1000.0);

    struct timeval tv;
    struct timespec t;

    // Do not use GetWallTime* here, it doesn't return absolute time on mac
    gettimeofday(&tv, NULL);
    const UINT64 usecs = tv.tv_usec + static_cast<UINT64>(timeoutMs * 1000);
    t.tv_sec = tv.tv_sec + usecs / 1000000;
    t.tv_nsec = static_cast<unsigned>(usecs % 1000000) * 1000;

    const int ret = pthread_cond_timedwait(&m_Cond, mutex, &t);
    return TranslateError(ret);
}

RC Tasker::NativeThread::SetCpuAffinity(Id id, const vector<UINT32> &cpuMasks)
{
#if !defined(MACOSX)
    if (cpuMasks.empty())
        return OK;

    // Enforce the least common denominator which is non-pthread affinity
    // system calls (these require only adjusting the lwrrently running thread)
    if (!pthread_equal(id, pthread_self()))
    {
        Printf(Tee::PriHigh,
               "%s : Only the lwrrently running thread may be changed!\n",
               __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

#if defined(DYNAMIC_CPU_SET)
    const UINT32 nrCpus = cpuMasks.size() * 32;
    cpu_set_t* cpuSet = CPU_ALLOC(nrCpus);
    const size_t cpuSetSize = CPU_ALLOC_SIZE(nrCpus);
    CPU_ZERO_S(cpuSetSize, cpuSet);
#else
    if ((cpuMasks.size() * 32) > CPU_SETSIZE)
    {
        Printf(Tee::PriHigh,
               "CPU mask contains too many CPUs (%u) for CPU set (max %u)\n",
               static_cast<UINT32>(cpuMasks.size() * 32),
               CPU_SETSIZE);
        return RC::SOFTWARE_ERROR;
    }

    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
#endif

    for (size_t maskIdx = 0; maskIdx < cpuMasks.size(); maskIdx++)
    {
        if (cpuMasks[maskIdx] == 0)
            continue;
        INT32 bit = Utility::BitScanForward(cpuMasks[maskIdx], 0);
        while (bit >= 0)
        {
            MODS_CPU_SET_S(bit + maskIdx*32, cpuSetSize, cpuSet);
            bit = Utility::BitScanForward(cpuMasks[maskIdx], bit + 1);
        }
    }

    TID_TYPE tid = GETTID(id);
    if (ILWALID_TID(tid))
    {
        Printf(Tee::PriHigh,
               "Unable to get thread id for setting affinity!\n");
        MODS_CPU_FREE(cpuSet);
        return RC::SOFTWARE_ERROR;
    }

    int ret = MODS_CPU_SETAFFINITY_S(tid, cpuSetSize, cpuSet);
    if (ret)
    {
        Printf(Tee::PriHigh, "Setting CPU affinity failed : %d (errno %d)\n",
               ret, errno);
        MODS_CPU_FREE(cpuSet);
        return RC::SOFTWARE_ERROR;
    }
    MODS_CPU_FREE(cpuSet);
#endif
    return OK;
}
