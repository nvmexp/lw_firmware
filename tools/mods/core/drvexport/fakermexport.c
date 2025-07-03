/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  fakermexport.cpp
 * @brief Dummy function linked by MODS so that the symbols are exported
 */

#include <stdarg.h>
#include "lwRmApi.h"

// This is a functional copy&paste of CallAllModsDrv & CallAllLwLink.
// kamild: My very limited understanding is that the dummy calls below make sure
// the functions are marked as used and available for symbol resolve
// from external libraries even when MODS itself is not calling them.
// I don't know why the error below oclwrs without it:
// "./mods: symbol lookup error: libmodsgl.so: undefined symbol: LwRmAllocRoot"

void CallAllFakeRM(va_list args)
{
    LwRmAllocRoot(0);
    LwRmAlloc(0, 0, 0, 0, 0);
    LwRmAllocMemory64(0, 0, 0, 0, 0, 0, 0);
    LwRmAllocObject(0, 0, 0, 0);
    LwRmFree(0, 0, 0);
    LwRmAllocEvent(0, 0, 0, 0, 0, 0);
    LwRmVidHeapControl(0);
    LwRmConfigGet(0, 0, 0, 0);
    LwRmConfigSet(0, 0, 0, 0, 0);
    LwRmConfigSetEx(0, 0, 0, 0, 0);
    LwRmConfigGetEx(0, 0, 0, 0, 0);
    LwRmI2CAccess(0, 0, 0);
    LwRmIdleChannels(0, 0, 0, 0, 0, 0, 0, 0, 0);
    LwRmMapMemory(0, 0, 0, 0, 0, 0, 0);
    LwRmUnmapMemory(0, 0, 0, 0, 0);
    LwRmAllocContextDma2(0, 0, 0, 0, 0, 0, 0);
    LwRmBindContextDma(0, 0, 0);
    LwRmMapMemoryDma(0, 0, 0, 0, 0, 0, 0, 0);
    LwRmUnmapMemoryDma(0, 0, 0, 0, 0, 0);
    LwRmControl(0, 0, 0, 0, 0);
    LwRmDupObject(0, 0, 0, 0, 0, 0);
    LwRmDupObject2(0, 0, 0, 0, 0, 0);
}
