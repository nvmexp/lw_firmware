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
//------------------------------------------------------------------------------
// Global variables.
//------------------------------------------------------------------------------
// 45678901234567890123456789012345678901234567890123456789012345678901234567890

#ifndef INCLUDED_GLOBALS_H
#define INCLUDED_GLOBALS_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif

#ifndef INCLUDED_STL_STRING
#include <string>
#define INCLUDED_STL_STRING
#endif

extern string g_LwWatchModuleName;
extern void * g_LwWatchModuleHandle;
extern string g_LwFlcnDbgModuleName;
extern void * g_LwFlcnDbgModuleHandle;
extern UINT32 g_ChiplibBusy;

void ClearCriticalEvent();
void SetCriticalEvent(UINT32 value);
UINT32 GetCriticalEvent();
void LwWatchLoadDLL();
void LwWatchUnloadDLL();

#endif // ! INCLUDED_GLOBALS_H
