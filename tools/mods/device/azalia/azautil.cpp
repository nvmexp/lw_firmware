/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "azautil.h"
#include "azactrl.h"
#include "core/include/platform.h"
#include "device/include/memtrk.h"
#include "core/include/tasker.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaliaUtilities"

namespace AzaliaUtilities
{
#ifdef SIM_BUILD
    UINT32 TimeScaler = 80;
    UINT32 MaxSingleWaitTime = 400;
#else
    UINT32 TimeScaler = 1;
    UINT32 MaxSingleWaitTime = 0;
#endif
}

RC AzaliaUtilities::SetTimeScaler(UINT32 scaler)
{
    TimeScaler = scaler;
    return OK;
}

UINT32 AzaliaUtilities::GetTimeScaler()
{
    return TimeScaler;
}

RC AzaliaUtilities::SetMaxSingleWaitTime(UINT32 time)
{
    MaxSingleWaitTime = time;
    return OK;
}

UINT32 AzaliaUtilities::GetMaxSingleWaitTime()
{
    return MaxSingleWaitTime;
}

bool AzaRegWaitPoll(void* pArgs)
{
    MASSERT(pArgs);
    RC rc;
    UINT32 value;
    AzaRegPollArgs* pPollArgs = (AzaRegPollArgs*) pArgs;
    MASSERT(pPollArgs->pAzalia);
    CHECK_RC(pPollArgs->pAzalia->RegRd(pPollArgs->reg, &value));
    return ((value & pPollArgs->mask) == (pPollArgs->value & pPollArgs->mask));
}

namespace AzaliaUtilities
{
    Random m_RGen;
    map<void*,void*> m_AllocAddrs;
}

void AzaliaUtilities::SeedRandom(UINT32 seed)
{
    m_RGen.SeedRandom(seed);
}

UINT32 AzaliaUtilities::GetRandom(UINT32 min, UINT32 max)
{
    return m_RGen.GetRandom(min, max);
}

RC AzaliaUtilities::AlignedMemAlloc
(
    UINT32 size,
    UINT32 AlignSize,
    void** pAddress,
    UINT32 AddressBits,
    Memory::Attrib attrib,
    bool randomize,
    Controller *pCtrl
)
{
    RC rc = OK;
    void* original_addr;
    *pAddress = NULL;

    if (randomize)
    {
        UINT32 expandedSize = size + Platform::GetPageSize() - 1;
        CHECK_RC(MemoryTracker::AllocBufferAligned(expandedSize,
                                                   &original_addr,
                                                   AlignSize,
                                                   AddressBits,
                                                   attrib,
                                                   pCtrl));
        UINT32 range = (Platform::GetPageSize()) / AlignSize;
        UINT32 randomNumber = m_RGen.GetRandom(0, range-1);
        *pAddress = (void*) (((UINT08*)original_addr) + (randomNumber * AlignSize));
    }
    else
    {
        CHECK_RC(MemoryTracker::AllocBufferAligned(size,
                                                   &original_addr,
                                                   AlignSize,
                                                   AddressBits,
                                                   attrib,
                                                   pCtrl));
        *pAddress = original_addr;
    }

    m_AllocAddrs[*pAddress] = original_addr;
    return OK;
}

RC AzaliaUtilities::AlignedMemFree(void* address)
{
    map<void*,void*>::iterator itr;
    itr = m_AllocAddrs.find(address);
    if (itr == m_AllocAddrs.end())
    {
        Printf(Tee::PriError, "AzaliaUtilities::AlignedMemFree: "
                              "could not match address 0x%p!\n",
               address);
        MASSERT(0);
        return RC::SOFTWARE_ERROR;
    }
    MemoryTracker::FreeBuffer(itr->second);
    m_AllocAddrs.erase(address);
    return OK;
}

void AzaliaUtilities::Pause(UINT32 time)
{
#ifdef SIM_BUILD
// The simulator will timeout if it detects that the program is idle
// for more than a certain amount of time. I think it's about 400 us
// of simulator time.
    time *= TimeScaler;
    Printf(Tee::PriDebug, "AzaliaUtilities::Pause for %d time units (scaler=%d, max=%d)\n",
           time, TimeScaler, MaxSingleWaitTime);

    while (time > 0)
    {
        if (time < MaxSingleWaitTime)
        {
            Platform::SleepUS(time);
            break;
        }
        Platform::SleepUS(MaxSingleWaitTime);
        time -= MaxSingleWaitTime;
        Printf(Tee::PriDebug, "AzaliaUtilities::Pause continuing to wait %d time units\n", time);
    }
#else
    Tasker::Sleep(time * TimeScaler);

#endif
}
