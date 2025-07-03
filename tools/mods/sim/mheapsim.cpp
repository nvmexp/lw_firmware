/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <stddef.h>
#include "mheapsim.h"

#include "core/include/massert.h"
#include "core/include/mgrmgr.h"
#include "core/include/platform.h"
#include "core/include/tee.h"
#include "device/interface/pcie.h"
#include "gpu/include/gpumgr.h"
#include "gpu/include/gpusbdev.h"

// Declare the one global MHeapSim object that MHeapSimPtr refers to
MHeapSim * MHeapSimPtr::s_pInstance;

MHeapSim::MHeapSim()
    : MultiHeap()
{
    m_pMemAlloc64Iface = NULL;
    m_pMemAlloc64Iface2 = NULL;
    m_pMemAlloc32Iface = NULL;
    m_pMemAlloc32Iface2 = NULL;
}

MHeapSim::~MHeapSim()
{
    if (Platform::IsForcedTermination())
        return;

    for ( int i = 0; i < GetNumHeapKinds(); i++ )
    {
        Heap *h;
        while ( (h = GetHeapByIndex(i)) != NULL )
        {
           if ( h->GetTotalAllocated() > 0)
           {
               Printf(Tee::PriHigh,
                      "Physical memory leak of %lld bytes detected!\n",
                      h->GetTotalAllocated());
               MASSERT(0);
           }
           MultiHeap::RmvHeap( i, h ); // keep mheap in sync

           // Free the memory allocated by chiplib
           if (h->GetAllocType() == Heap::AllocationType::IFACE64)
           {
               if (m_pMemAlloc64Iface2)
                   m_pMemAlloc64Iface2->FreeSysMem64(h->GetBase());
               else
                   m_pMemAlloc64Iface->FreeSysMem64(h->GetBase());
           }
           else
           {
               if (m_pMemAlloc32Iface2)
                   m_pMemAlloc32Iface2->FreeSysMem32((UINT32)(h->GetBase()));
               else
                   m_pMemAlloc32Iface->FreeSysMem32((UINT32)(h->GetBase()));
           }

           delete h;                   // we do the alloc, we do the delete
        }
    }
}

Heap *MHeapSim::GetHeapWithRoom( Memory::Attrib ma, UINT64 size,
                                 PHYSADDR maxphysaddr, UINT32 gpuInst )
{
    Heap *h = MultiHeap::GetHeapWithRoom(GetHeapIndexFromMemAttrib(ma), size,
                                         maxphysaddr, gpuInst);
    if (h == NULL)
    {
        Printf(Tee::PriDebug, "no %s heap found with free space for %llx\n",
               Memory::GetMemAttribName(ma), size );
    }
    return h;
}

int MHeapSim::GetHeapIndexFromMemAttrib( Memory::Attrib ma )
{
    MASSERT((int)ma >= Memory::UC);
    MASSERT((int)ma <= Memory::WB);
    return (int)ma;
}

Heap * MHeapSim::GetHeapListForMemAttrib(Memory::Attrib ma)
{
    return MultiHeap::GetHeapByIndex(GetHeapIndexFromMemAttrib(ma));
}

void MHeapSim::SetHeapAllocMax( Memory::Attrib ma, UINT64 max )
{
    MultiHeap::SetHeapAllocMax(GetHeapIndexFromMemAttrib(ma), max);
}

void MHeapSim::SetHeapAllocMin( Memory::Attrib ma, UINT64 min )
{
    MultiHeap::SetHeapAllocMin(GetHeapIndexFromMemAttrib(ma), min);
}

UINT64 MHeapSim::GetTotalHeapSize( Memory::Attrib ma )
{
    return MultiHeap::GetTotalHeapSize(GetHeapIndexFromMemAttrib(ma));
}

Heap *MHeapSim::AllocAligned(Memory::Attrib ma, UINT64 size,
                             UINT64 alignQuantum, PHYSADDR maxphysaddr, UINT32 gpuInst)
{
    // TODO: this returns the first heap with room; should iterate on all heaps
    Heap *h = GetHeapWithRoom( ma, size, maxphysaddr, gpuInst);
    if ( h )
    {
       bool satisfies = false;
       MASSERT( 0 == (alignQuantum & (alignQuantum - 1))); // must be power of 2
       UINT64 msk = alignQuantum - 1;
       if ( msk & h->GetOffsetLargestFree() )
       {
           UINT64 s = h->GetLargestFree();
           s -= msk; // treat as a number rather than mask
           if ( s >= size )
           {
               satisfies = true;
           }
       }
       else
       {
           satisfies = true;
       }
       if ( satisfies )
       {
           Printf(Tee::PriDebug,
                  "MHeapSim::AllocAligned(%s, %llx, %llx, %d) satisfied from existing heap\n",
                  Memory::GetMemAttribName(ma), size, alignQuantum, gpuInst);
           return h;
        }
    }

    Printf(Tee::PriDebug, "MHeapSim::HeapAlloc(%s, %llx, %llx, %d) will make new heap with no align constraint\n",
           Memory::GetMemAttribName(ma), size, alignQuantum, gpuInst);

    return DoAlloc(ma, size, alignQuantum, maxphysaddr, gpuInst);
}

Heap *MHeapSim::Alloc(Memory::Attrib ma, UINT64 size, PHYSADDR maxphysaddr, UINT32 gpuInst)
{
    Heap *h = GetHeapWithRoom( ma, size, maxphysaddr, gpuInst );
    if ( h )
    {
       Printf(Tee::PriDebug, "MHeapSim::HeapAlloc(%s, %llx, %d) satisfied from existing heap\n",
              Memory::GetMemAttribName(ma), size, gpuInst);
       return h;
    }

   Printf(Tee::PriDebug, "MHeapSim::HeapAlloc(%s, %llx, %d) will make new heap with no align constraint\n",
          Memory::GetMemAttribName(ma), size, gpuInst);

    return DoAlloc( ma, size, 0, maxphysaddr, gpuInst);
}

Heap *MHeapSim::DoAlloc( Memory::Attrib ma, UINT64 size,
                         UINT64 alignQuantum, PHYSADDR maxphysaddr, UINT32 gpuInst )
{
    int index = GetHeapIndexFromMemAttrib(ma);
    UINT64 minSize = GetHeapAllocMin(index);
    if ( size < minSize )
    {
        size = minSize;
    }

    UINT64 minAlign = size; // XXX ??? true for mtrrs...
    if ( alignQuantum < minAlign )
    {
        alignQuantum = minAlign;
    }

    // constrain alignment to be a power of 2?  Possibly doesn't belong here...but the allocators likely depend on this
    if ( 0 != (alignQuantum & (alignQuantum-1)))
    {
        MASSERT(alignQuantum < (UINT64)1<<63);
        UINT64 t = 1;
        while ( t < alignQuantum )
        {
            t<<= 1;
        }
        alignQuantum = t;
    }

    // verify that we will not exceed the specified max size of this heap type
    UINT64 maxSize = GetHeapAllocMax(index);
    if ( (GetTotalHeapSize(ma) + size ) > maxSize )
    {
        Printf(Tee::PriAlways,
               "allocation of %llx would exceed max (%llx) for %s\n",
               size, maxSize, Memory::GetMemAttribName(ma));
        return NULL;
    }

    // now allocate physical memory for this heap
    // Allocate "simulator" system memory from the chiplib.  This tells us what
    // the *physical* address of simulator system memory is. The MODS suballocator
    // will be used for all real memory allocations out of this chunk.
    // "simulator" can be RTL or emulator

    LwPciDev pciDevice = {0, 0, 0, 0};
#if defined(INCLUDE_GPU)
    if (gpuInst != static_cast<UINT32>(Platform::UNSPECIFIED_GPUINST))
    {
        GpuDevMgr* const pGpuDevMgr = static_cast<GpuDevMgr*>(DevMgrMgr::d_GraphDevMgr);
        if (pGpuDevMgr)
        {
            GpuSubdevice* const pSubdev = pGpuDevMgr->GetSubdeviceByGpuInst(gpuInst);
            if (pSubdev && !pSubdev->IsSOC())
            {
                auto pGpuPcie = pSubdev->GetInterface<Pcie>();
                if (pGpuPcie)
                {
                    pciDevice.domain   = pGpuPcie->DomainNumber();
                    pciDevice.bus      = pGpuPcie->BusNumber();
                    pciDevice.device   = pGpuPcie->DeviceNumber();
                    pciDevice.function = pGpuPcie->FunctionNumber();
                }
            }
        }
    }
#endif

    PHYSADDR start = 0;
    Heap::AllocationType allocType;
    if (maxphysaddr > 0xffffffff)
    {
        MASSERT(m_pMemAlloc64Iface || m_pMemAlloc64Iface2);
        LwU064   msk = alignQuantum - 1;
        if (m_pMemAlloc64Iface2)
        {
            if (m_pMemAlloc64Iface2->DeviceAllocSysMem64(pciDevice, size, ma, msk, &start))
            {
                Printf(Tee::PriAlways, "chiplib DeviceAllocSysMem64() failed!\n");
                return NULL;
            }
        }
        else
        {
            if (m_pMemAlloc64Iface->AllocSysMem64(size, ma, msk, &start))
            {
                Printf(Tee::PriAlways, "chiplib AllocSysMem64() failed!\n");
                return NULL;
            }
        }
        allocType = Heap::AllocationType::IFACE64;
    }
    else
    {
        MASSERT(m_pMemAlloc32Iface || m_pMemAlloc32Iface2);
        MASSERT(size < (UINT64)1<<32);
        MASSERT(alignQuantum < (UINT64)1<<32);
        UINT32 s = 0;
        UINT32 sz = (UINT32)size;
        UINT32 msk = (UINT32)alignQuantum - 1;
        if (m_pMemAlloc32Iface2)
        {
            if (m_pMemAlloc32Iface2->DeviceAllocSysMem32(pciDevice, sz, ma, msk, &s))
            {
                Printf(Tee::PriAlways, "chiplib DeviceAllocSysMem() failed!\n");
                return NULL;
            }
        }
        else
        {
            if (m_pMemAlloc32Iface->AllocSysMem32(sz, ma, msk, &s))
            {
                Printf(Tee::PriAlways, "chiplib AllocSysMem() failed!\n");
                return NULL;
            }
        }
        start = s;
        allocType = Heap::AllocationType::IFACE32;
    }

    Printf(Tee::PriDebug, "System memory physical base = %llx\n",
        start);
    Printf(Tee::PriDebug, "                       size = %llx\n",
        size);

    // allocate a new heap structure
    Heap *newHeap = new Heap(start, size, gpuInst, allocType);
    if ( newHeap == NULL )
    {
        Printf(Tee::PriAlways, "Cannot allocate heap structure for mem attrib %s with start %llx and finish %llx\n",
             Memory::GetMemAttribName(ma), start, start+size );
        Printf(Tee::PriAlways, "this is a simple case of running out of process exceeding its heap limits\n");
        return NULL;
    }

    // store in mheap
    AddHeap( index, newHeap );

    return newHeap;
}

