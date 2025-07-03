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
#ifndef INCLUDED_AZAUTIL_H
#define INCLUDED_AZAUTIL_H

#include "core/include/rc.h"
#include "core/include/memoryif.h"
#include "core/include/tasker.h"
#include "cheetah/include/controller.h"

class AzaliaController;

typedef struct
{
    AzaliaController* pAzalia;
    UINT32 reg;
    UINT32 mask;
    UINT32 value;
} AzaRegPollArgs;
bool AzaRegWaitPoll(void* pArgs);

namespace AzaliaUtilities
{
    void SeedRandom(UINT32 seed);
    UINT32 GetRandom(UINT32 min, UINT32 max);

    // TODO: I'd like to just move this functionality directly into
    // the MemTracker class.
    RC AlignedMemAlloc(UINT32 size, UINT32 align_size, void** pAddress,
                       UINT32 AddressBits, Memory::Attrib attrib, bool randomize = true,
                       Controller *pCtrl = nullptr);
    RC AlignedMemFree(void* address);

    RC SetTimeScaler(UINT32 scaler);
    UINT32 GetTimeScaler();
    RC SetMaxSingleWaitTime(UINT32 time);
    UINT32 GetMaxSingleWaitTime();
    void Pause(UINT32 time);
};

#endif // INCLUDED_AZAUTIL_H

