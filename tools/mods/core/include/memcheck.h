/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2008-2009 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MEMCHECK_H
#define INCLUDED_MEMCHECK_H

#if DEBUG_TRACE_LEVEL >= 1

#include "pool.h"

// wherever this is included, new will keep track of the calling line/file
// otherwise fall back to the new/delete implementations in pool.cpp
inline void * operator new(size_t size, const char *file, long line)
{
    void *addr = Pool::Alloc(size);
    Pool::AddTrace(addr, size, file, line, Pool::CID_NEW);
    return addr;
}

inline void * operator new[](size_t size, const char *file, long line)
{
    void *addr = Pool::Alloc(size);
    Pool::AddTrace(addr, size, file, line, Pool::CID_BLOCK);
    return addr;
}

// delete can't be tricked into saving the line/file, but matching routines
// need to be declared in the case that new throws an exception (warning C4291)
inline void operator delete(void *addr, const char *file, long line)
{
    Pool::RemoveTrace(addr, Pool::CID_NEW);
    Pool::Free(addr);
}

inline void operator delete[](void *addr, const char *file, long line)
{
    Pool::RemoveTrace(addr, Pool::CID_BLOCK);
    Pool::Free(addr);
}

// wherever we're included, replace calls to new with our more verbose version
#define new new(__FILE__, __LINE__)

#endif // DEBUG_TRACE_LEVEL >= 1
#endif // INCLUDED_MEMCHECK_H

