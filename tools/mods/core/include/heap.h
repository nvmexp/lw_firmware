/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2008,2011,2015,2017,2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_HEAP_H
#define INCLUDED_HEAP_H

#ifndef INCLUDED_TYPES_H
#include "types.h"
#endif
#ifndef INCLUDED_RC_H
#include "rc.h"
#endif

class Random;

/**
 * @class( Heap )
 *
 * A heap to track allocations of memory in some external memory space (e.g.
 * video memory or simulator system memory).
 */

class Heap
{
public:

    enum class AllocationType
    {
        IFACE32,
        IFACE64,
    };

    Heap(UINT64 Base, UINT64 Size, UINT32 gpuInst, AllocationType AllocType = AllocationType::IFACE32);
    ~Heap();

    RC Alloc
    (
        UINT64 Size,
        UINT64 *Offset,
        UINT64 Align = 1,
        UINT64 AlignOffset = 0
    );
    RC AllocAt
    (
        UINT64 Size,
        UINT64 Offset
    );
    RC AllocRange
    (
        UINT64 Size,
        UINT64 *Offset,
        UINT64 Min,
        UINT64 Max,
        UINT64 Align = 1,
        UINT64 AlignOffset = 0
    );
    RC AllocRandom
    (
        Random *pRandom,
        UINT64 Size,
        UINT64 *Offset,
        UINT64 Min,
        UINT64 Max,
        UINT64 Align = 1,
        UINT64 AlignOffset = 0
    );
    RC Free
    (
        UINT64 Offset
    );

    RC GetAllocSize
    (
        UINT64 Offset,
        UINT64 *Size
    ) const;

    UINT64 GetBase() const { return m_Base; }
    UINT64 GetSize() const { return m_Size; }
    AllocationType GetAllocType() const { return m_AllocType; }
    UINT64 GetTotalAllocated() const;
    UINT64 GetTotalFree() const;
    UINT64 GetLargestFree() const;
    UINT64 GetOffsetLargestFree() const; // for alignment
    UINT32 GetGpuInst() const { return m_GpuInst; }

    Heap  *GetNext() { return m_Next; }
    void  SetNext(Heap *n) { m_Next = n; }

    void Dump() const;

private:
    // Forward declaration of heap block data structure
    struct HeapBlock;

    UINT64 m_Base;
    UINT64 m_Size;
    AllocationType m_AllocType;
    UINT32 m_ArraySize;
    UINT32 m_GpuInst;
    HeapBlock *m_RawBlockArray;
    HeapBlock *m_BlockList;
    HeapBlock *m_SpareBlockList;

    Heap *m_Next;
};

#endif // INCLUDED_HEAP_H
