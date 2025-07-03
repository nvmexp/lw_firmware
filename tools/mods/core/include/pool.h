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

#pragma once

#include <map>
#include <stddef.h>
#include "types.h"

namespace Utility
{
namespace Pool
{
    enum CounterIdx
    {
        CID_ALLOC = 0,  // total number of allocations through Pool::Alloc
        CID_NEW,        // number of allocations through new
        CID_BLOCK,      // number of allocations through new[]
        CID_EXTERN,     // number of allocations through ModsDrv or Xp
        CID_NONPOOL,    // number of allocations not made through Pool::Alloc
        CID_BYTES,      // total number of bytes lwrrently held
        CID_MAXBYTES,   // maximum amount of memory held at any one point
        NUM_COUNTERS
    };

    /** Alloc memory.
     *  Returned pointers will be 8-byte aligned.
     *  Some care is taken to reduce memory fragmentation.
     */
    void * Alloc( size_t Size );

    /** Alloc memory with a given pointer alignment (or misalignment).
     *
     * The returned pointer's *linear* address will be such that
     *    (address % Align) == Offset.
     *
     *  Align must be a power of 2, and must be <= 4096.
     *  Offset must be less than Align.
     *  Use Pool::Free to delete the memory.
     *
     *  If you call Realloc to a larger size, a new pointer will be returned
     *  with alignment 8, offset 0.  Don't Realloc if you care about alignment!
     */
    void * AllocAligned( size_t Size, size_t Align, size_t Offset=0 );

    /** Realloc memory.
     * If the new size is > the old size, you get a new pointer, with alignment
     * of 8, offset 0.  If the new size is the same or less than the old, you get
     * the old pointer back.
     */
    void * Realloc(volatile void * OldPtr, size_t NewSize, CounterIdx cid = CID_ALLOC);

    /** Free memory allocated by Pool.
     */
    void   Free( volatile void * Address);

    //! Memory leak tracking hooks
    INT64 GetCount(CounterIdx type);
    void EnforceThreshold(INT64 threshold);

#if DEBUG_TRACE_LEVEL >= 1
    void AddTrace(void *p, size_t size, const char *file, long line, CounterIdx type);
    void RemoveTrace(void *p, CounterIdx type);
#endif

    void LeakCheckInit();
    bool LeakCheckShutdown();
    void SetTraceEnabled(bool enabled);
    void AddNonPoolAlloc(void *p, size_t size);
    void RemoveNonPoolAlloc(void *p, size_t size);
    void TestAlloc(const char* msg, size_t size);
    bool CanAlloc(size_t size);
} // Pool
} // Utility

// We put Pool in namespace Utility to avoid clashes with Pool class from OpenGL.
// But in MODS we want to use Pool directly, so we create a namespace alias.
namespace Pool = Utility::Pool;

// For debug builds, mark each alloc/free in the trace log
// For release builds, just perform the operation without tracing
#ifdef TRACY_ENABLE
#define POOL_ADDTRACE(a, s, t) PROFILE_MALLOC(a, s)
#define POOL_REMOVETRACE(a, t) PROFILE_FREE(a)
#elif DEBUG_TRACE_LEVEL >= 1
#define POOL_ADDTRACE(a, s, t) Pool::AddTrace(a, s, __FILE__, __LINE__, t)
#define POOL_REMOVETRACE(a, t) Pool::RemoveTrace(a, t)
#else
#define POOL_ADDTRACE(a, s, t)
#define POOL_REMOVETRACE(a, t)
#endif
