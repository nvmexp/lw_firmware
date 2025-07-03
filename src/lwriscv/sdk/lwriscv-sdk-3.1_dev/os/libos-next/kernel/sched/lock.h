/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */

#pragma once

#include "libos.h"
#include "kernel.h"
#include "sched/port.h"

struct Lock
{
    Port    port;
    Shuttle unlockShuttle;
    LwU32   holdCount;
#if !LIBOS_TINY
    struct Task* holder;
#else
    LibosTaskHandle  holder;
#endif
};

LibosStatus KernelLockRelease(Lock *lock);
void KernelLockDestroy(Lock *lock);
