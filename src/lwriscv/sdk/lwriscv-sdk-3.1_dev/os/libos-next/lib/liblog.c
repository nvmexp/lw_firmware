/* _LWRM_COPYRIGHT_BEGIN_
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * _LWRM_COPYRIGHT_END_
 */
#include <lwtypes.h>
#include "libos-config.h"
#include "diagnostics.h"
#include "libos.h"

static LwU64   logBufferPut = 1;
static LwU64   logBufferEntryCount = 0;

static LwU64 * logBuffer = 0 ;

void LibosLogInitialize(void * buffer, LwU64 size)
{
    logBufferEntryCount = size / sizeof(LwU64);
    logBuffer = buffer;
}    

// @todo: Why isn't this a "push argument" call?
//        Because: Reversing is hard.
void LibosLogEntry(LwU64 nArgs, const LwU64 * args)
{
    nArgs++;
    for (LwU32 i = 0; i < nArgs; i++)
    {
        logBuffer[logBufferPut] = args[i];
        logBufferPut++;
        if (logBufferPut >= logBufferEntryCount)
            logBufferPut = 1;
    }
    logBuffer[0 /* streaming put */] += nArgs;
}

