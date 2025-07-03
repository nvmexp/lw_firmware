/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once
#include "../tools/tools.h"
#include "../tools/lwda_vectors.h"

// Lwrrently each LWCA uses its smid to acquire the lock
#define LOCK_INIT 0xFFFFFFFF

struct InstrPulseParams
{
    device_ptr smidToGpcPtr;
    device_ptr gpcLocksPtr;
    UINT64 runClks;
    UINT64 pulseClks;
    UINT64 delayClks;
};
