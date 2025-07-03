/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2007 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_MHEAP_H
#define INCLUDED_MHEAP_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif
#ifndef INCLUDED_HEAP_H
#include "heap.h"
#endif

class Heap;

/**
 * @class( MultiHeap )
 *
 * A singleton class implementing multiple heaps distinguished by Memory::Attrib
 */

class MultiHeap
{
public:
    MultiHeap();
    ~MultiHeap();

    RC AddHeap( int heapKind, Heap *);
    RC RmvHeap( int heapKind, Heap *);

    RC Free( PHYSADDR p);

    RC const SpansPhysAddr( PHYSADDR p);
    RC const GetAllocBase( PHYSADDR p, PHYSADDR *b);
    RC const GetAllocSize( PHYSADDR p, UINT64 *s);
    UINT64 GetTotalHeapSize( int idx );

    void SetHeapAllocMax( int idx, UINT64 max );
    void SetHeapAllocMin( int idx, UINT64 min );
    UINT64 GetHeapAllocMax( int idx );
    UINT64 GetHeapAllocMin( int idx );

    Heap *GetHeapWithRoom( int i, UINT64 size, PHYSADDR maxphysaddr, UINT32 gpuInst );

    const int GetNumHeapKinds(void) { return MaxNumHeaps; }

protected:
    Heap *GetHeapByIndex(int i);

private:

    enum { MaxNumHeaps = 7 }; // but could easily be 5 (for number of distinct Memmory::Attrib)

    RC const HeapSpansPhysAddr(Heap *h, PHYSADDR p);
    Heap *GetHeapForAddr( PHYSADDR p );

    Heap * m_Heaps[MaxNumHeaps];
    UINT64 m_HeapAllocMin[MaxNumHeaps];
    UINT64 m_HeapAllocMax[MaxNumHeaps];
};

#endif // INCLUDED_MHEAP_H

