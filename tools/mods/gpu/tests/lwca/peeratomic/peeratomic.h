/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"

// One atomic and one reduction increment per iteration
#define INCREMENTS_PER_ITERATION 2

#define SEMA_CLEAR   0x0
#define SEMA_PROCEED 0x12345678

namespace PeerAtomic
{
    struct PeerAtomicInput
    {
        device_ptr testMemory;   //!< Memory to apply atomics to
        device_ptr localSem;     //!< Pointer to local semaphore
        device_ptr peerSem;      //!< Pointer to remote semaphore
        UINT32 loops;            //!< Number of loops to run atomics for
        UINT64 startTimeoutNs;   //!< Timeout to start the kernel in nanoseconds
    };
} // namespace PeerAtomic
