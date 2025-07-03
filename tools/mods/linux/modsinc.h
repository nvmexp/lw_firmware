/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Common include file for all MODS source files.

#ifndef INCLUDED_MODSINC_H
#define INCLUDED_MODSINC_H

#if (__GNUC__ > 3) || ((__GNUC__ == 3) && (__GNUC_MINOR__ > 3))
#   define PRAGMA_ONCE_SUPPORTED
#   pragma once
#endif

// Use the new/delete overrides only if MODS allocator is included
#if defined(INCLUDE_MODSALLOC)

// This solves problems with exported operators new and delete and Fmodel.
// Lwrrently if we build MODS with GCC 4, Fmodel tends to take operator new
// from libstdc++ and operator delete from MODS which is deadly.
// When operators new and delete are inline, they are emitted as weak symbols,
// so the dynamic linker prefers to pick up these operators from libstdc++
// when an so refers to them, instead of taking them from the MODS exelwtable.

#ifdef  MODS_INLINE_OPERATOR_NEW
#ifdef __cplusplus
#include <new>

void* ModsNew(size_t size);
void* ModsNewArray(size_t size);
void ModsDelete(void* p);
void ModsDeleteArray(void* p);

inline void* operator new(size_t size)
{
    return ModsNew(size);
}

inline void* operator new[](size_t size)
{
    return ModsNewArray(size);
}

inline void operator delete(void* ptr)
{
    ModsDelete(ptr);
}

inline void operator delete[](void* ptr)
{
    ModsDeleteArray(ptr);
}

// no exceptions to handle in the following functions
// because Mods heap code will not throw any exception
inline void* operator new(size_t size, const std::nothrow_t&)
{
    return ModsNew(size);
}

inline void* operator new[](size_t size, const std::nothrow_t&)
{
    return ModsNewArray(size);
}

inline void operator delete(void* ptr, const std::nothrow_t&)
{
    ModsDelete(ptr);
}

inline void operator delete[](void* ptr, const std::nothrow_t&)
{
    ModsDeleteArray(ptr);
}
#endif // __cplusplus
#endif // MODS_INLINE_OPERATOR_NEW
#endif // defined(INCLUDE_MODSALLOC)
#if !defined(NO_ONETIME_INIT)
#include "cpuopsys.h"

// Take care of Microsoft Visual C++ / Microsoft Windows specifics

#define DLLEXPORT
#define DLLIMPORT

// Enable type limit macros
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#include <stdlib.h>

// Lots of MODS source files rely on memcpy-class functions to be available
#include <string.h>

#ifdef QNX
// QNX does not implement memmem, like Linux and MacOS do
void* memmem(const void* haystack, size_t haystackLen,
             const void* needle, size_t needleLen);
#endif

#define ENOENT 2        /* No such file or directory            */

char* itoa( int n, char* str, int base );

#define stricmp(x,y) (strcasecmp((x), (y)))

#include "../core/include/macros.h"

// We want to include this from all source files
#include "../core/include/onetime.h"
#endif // NO_ONETIME_INIT
#endif // !INCLUDED_MODSINC_H
