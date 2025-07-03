/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "tools.h"
#define MAX_SYNC_DEVICES 32
#define DELAY_POLLING_INTERVAL_NS 128

#ifdef __LWDACC__
__device__ void LwdaDelayNs(const UINT64 delayNs)
{
    UINT64 startTimeNs = 0;
    UINT64 lwrTimeNs = 0;
    asm volatile ("mov.u64 %0, %%globaltimer;" : "=l"(startTimeNs) :: "memory");
    do
    {
#if SM_VER >= 70
        __nanosleep(DELAY_POLLING_INTERVAL_NS);
#endif
        asm volatile ("mov.u64 %0, %%globaltimer;" : "=l"(lwrTimeNs) :: "memory");
    }
    while ((lwrTimeNs - startTimeNs) < delayNs);
}
#endif
