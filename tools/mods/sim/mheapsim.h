/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007,2011,2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifdef PRAGMA_ONCE_SUPPORTED
#pragma once
#endif

#ifndef INCLUDED_MHEAPSIM_H
#define INCLUDED_MHEAPSIM_H

#ifndef INCLUDED_MHEAP_H
#include "core/include/mheap.h"
#endif
#ifndef INCLUDED_MEMORYIF_H
#include "core/include/memoryif.h"
#endif
#ifndef INCLUDED_SINGLTON_H
#include "core/include/singlton.h"
#endif
#ifndef _IMULTIHEAP_H_
#include "IMultiHeap.h"
#endif
#ifndef _IMULTIHEAP2_H_
#include "IMultiHeap2.h"
#endif

class Heap;

/**
 * @class( MHeapSim )
 *
 * A singleton class implementing multiple heaps distinguished by Memory::Attrib
 */

class MHeapSim : public MultiHeap
{
public:
    MHeapSim();
    ~MHeapSim();

    Heap *Alloc(Memory::Attrib ma, UINT64 size, PHYSADDR maxphysaddr, UINT32 gpuInst);
    Heap *AllocAligned(Memory::Attrib ma, UINT64 size,
                       UINT64 alignQuantum, PHYSADDR maxphysaddr, UINT32 gpuInst);

    void SetMemAlloc64(IMultiHeap *alloc64Iface ) { m_pMemAlloc64Iface = alloc64Iface; }
    void SetMemAlloc64_2(IMultiHeap2 *alloc64Iface2) { m_pMemAlloc64Iface2 = alloc64Iface2; }
    void SetMemAlloc32(IMultiHeap *alloc32Iface ) { m_pMemAlloc32Iface = alloc32Iface; }
    void SetMemAlloc32_2(IMultiHeap2 *alloc32Iface2) { m_pMemAlloc32Iface2 = alloc32Iface2; }

    void SetHeapAllocMax( Memory::Attrib ma, UINT64 max );
    void SetHeapAllocMin( Memory::Attrib ma, UINT64 min );

protected:
    // MHeapSim(); see above: no way to create this as a singleton via SINGLETON macro

private:
    friend class MHeapSimPtr;

    Heap *GetHeapListForMemAttrib(Memory::Attrib ma);
    Heap *GetHeapWithRoom( Memory::Attrib ma, UINT64 size, PHYSADDR maxphysaddr, UINT32 gpuInst );
    UINT64 GetTotalHeapSize( Memory::Attrib ma );

    // map from Memmory::Attrib to an index
    int GetHeapIndexFromMemAttrib( Memory::Attrib ma );

    Heap *DoAlloc( Memory::Attrib ma, UINT64 size, UINT64 alignQuantum, PHYSADDR maxphysaddr, UINT32 gpuInst );

    IMultiHeap * m_pMemAlloc64Iface;
    IMultiHeap2 * m_pMemAlloc64Iface2;
    IMultiHeap * m_pMemAlloc32Iface; // bleh
    IMultiHeap2 * m_pMemAlloc32Iface2;
};

SINGLETON(MHeapSim) // see singleton.h: creates class MHeapSimPtr

#endif // INCLUDED_MHEAPSIM_H

