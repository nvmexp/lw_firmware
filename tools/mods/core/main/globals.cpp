/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007,2011,2014,2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/globals.h"
#include "core/include/cpu.h"
#include "core/include/xp.h"

namespace
{
    volatile UINT32 g_CriticalEvent = 0;
}

string g_LwWatchModuleName = "liblwwatch" + Xp::GetDLLSuffix();
string g_LwFlcnDbgModuleName = "libflcndbg" + Xp::GetDLLSuffix();
void * g_LwWatchModuleHandle = NULL;
void * g_LwFlcnDbgModuleHandle = NULL;
UINT32 g_ChiplibBusy = 0;

void ClearCriticalEvent()
{
    Cpu::AtomicWrite(&g_CriticalEvent, 0);
}

void SetCriticalEvent(UINT32 value)
{
    UINT32 oldValue = 0;
    UINT32 newValue = 0;
    do
    {
        oldValue = Cpu::AtomicRead(&g_CriticalEvent);
        newValue = oldValue | value;
    }
    while (!Cpu::AtomicCmpXchg32(&g_CriticalEvent, oldValue, newValue));
}

UINT32 GetCriticalEvent()
{
    return Cpu::AtomicRead(&g_CriticalEvent);
}

void LwWatchLoadDLL()
{
    RC rc = Xp::LoadDLL(g_LwWatchModuleName, &g_LwWatchModuleHandle, 0);
    if (rc == OK)
    {
        Printf(Tee::PriAlways, "Lwwatch DLL loaded!\n");
    }
    else
    {
        Printf(Tee::PriAlways, "Lwwatch DLL load failed: %s\n", rc.Message());
    }
}

void LwWatchUnloadDLL()
{
    RC rc = Xp::UnloadDLL(g_LwWatchModuleHandle);
    if (rc == OK)
    {
        Printf(Tee::PriAlways, "Lwwatch DLL unloaded!\n");
    }
    else
    {
        Printf(Tee::PriAlways, "Lwwatch DLL unload failed: %s\n", rc.Message());
    }
}

