/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <stddef.h>
#include "core/include/mheap.h"
#include "core/include/tee.h"
#include "core/include/massert.h"
#include "core/include/platform.h"

MultiHeap::MultiHeap()
{
    for ( int i = 0; i < GetNumHeapKinds(); i++ )
    {
        m_Heaps[i] = NULL;
        m_HeapAllocMin[i] = m_HeapAllocMax[i] = 0;
    }
}

MultiHeap::~MultiHeap()
{
    if (Platform::IsForcedTermination())
        return;

    // we don't do any allocation in this class, so nothing to delete
    for ( int i = 0; i < GetNumHeapKinds(); i++ )
    {
        MASSERT(m_Heaps[i] == NULL);
    }
}

Heap *MultiHeap::GetHeapWithRoom( int i, UINT64 size, PHYSADDR maxphysaddr, UINT32 gpuInst )
{
    Heap *h = GetHeapByIndex(i);
    while (h != NULL)
    {
       // Check if the maxphysaddr is inside of this heap
       if (( h->GetLargestFree() >= size ) &&
           (( h->GetBase() + h->GetSize()) <= maxphysaddr)
#if defined(PPC64LE)
           // only PPC64LE cares which gpu this heap was allocated for
           && ( h->GetGpuInst() == gpuInst ||
                gpuInst == static_cast<UINT32>(Platform::UNSPECIFIED_GPUINST))
#endif
           )
           return h;

       if ( h->GetTotalFree() >= size )
       {
          Printf(Tee::PriDebug, "Heap %d fragmented: cannot accomodate size %llx\n",
             i, size );
       }
       h = h->GetNext();
    }
    Printf(Tee::PriDebug, "no heap in list %d found with free space for %llx for gpuInst %d\n",
        i, size, gpuInst );
    return NULL;
}

Heap *MultiHeap::GetHeapByIndex(int i)
{
    MASSERT(i>=0);
    MASSERT(i<GetNumHeapKinds());
    return m_Heaps[i];
}

RC const MultiHeap::SpansPhysAddr( PHYSADDR p)
{
    if ( GetHeapForAddr( p ) == NULL ) return RC::SOFTWARE_ERROR;
    else return OK;
}

RC const MultiHeap::HeapSpansPhysAddr(Heap *h, PHYSADDR p)
{
    MASSERT(h);
    return ((h->GetBase() <= p) && (p < (h->GetBase()+h->GetSize())));
}

Heap *MultiHeap::GetHeapForAddr( PHYSADDR p )
{
    for ( int i = 0; i < GetNumHeapKinds(); i++ )
    {
        Heap *h = m_Heaps[i];
        while ( h != NULL )
        {
            if ( HeapSpansPhysAddr( h, p ) )
            {
               return h;
            }
            h = h->GetNext();
        }
    }
    return NULL;
}

RC MultiHeap::Free( PHYSADDR p)
{
    Heap *h = GetHeapForAddr( p );
    if ( h )
    {
       Printf(Tee::PriDebug, "MultiHeap::Free( %llx)\n", p );
       h->Free( p );
       return OK;
    }
    else
    {
       MASSERT( 0 && "Bad PHYSADDR given to MultiHeap::Free");
       return RC::SOFTWARE_ERROR;
    }
}

RC const MultiHeap::GetAllocBase( PHYSADDR p, UINT64 *b)
{
    Heap *h = GetHeapForAddr( p );
    if ( h )
    {
        *b = h->GetBase();
        return OK;
    }
    else
    {
        MASSERT( 0 && "BAD physical address in MultiHeap::GetAllocBase" );
        return RC::SOFTWARE_ERROR;
    }
}

RC const MultiHeap::GetAllocSize( PHYSADDR p, UINT64 *s)
{
    Heap *h = GetHeapForAddr( p );
    if ( h )
    {
        *s = h->GetSize();
        return OK;
    }
    else
    {
        MASSERT( 0 && "BAD physical address in MultiHeap::GetAllocSize" );
        return RC::SOFTWARE_ERROR;
    }
}

void MultiHeap::SetHeapAllocMax( int i, UINT64 max )
{
    MASSERT(i>=0);
    MASSERT(i<GetNumHeapKinds());
    m_HeapAllocMax[i] = max;
}

void MultiHeap::SetHeapAllocMin( int i, UINT64 min )
{
    MASSERT(i>=0);
    MASSERT(i<GetNumHeapKinds());
    m_HeapAllocMin[i] = min;
}

UINT64 MultiHeap::GetHeapAllocMax( int i )
{
    MASSERT(i>=0);
    MASSERT(i<GetNumHeapKinds());
    return m_HeapAllocMax[i];
}

UINT64 MultiHeap::GetHeapAllocMin( int i )
{
    MASSERT(i>=0);
    MASSERT(i<GetNumHeapKinds());
    return m_HeapAllocMin[i];
}

RC MultiHeap::AddHeap( int mheapIdx, Heap *h )
{
    MASSERT( h && "MultiHeap::AddHeap called with null heap pointer!");
    MASSERT( ((mheapIdx >= 0) && (mheapIdx < GetNumHeapKinds())) && "MultiHeap::AddHeap called with out-of-range heap idx");
    UINT64 size = h->GetSize();

    UINT64 minSize = m_HeapAllocMin[mheapIdx];
    if ( size < minSize )
    {
        Printf(Tee::PriAlways, "allocation of %llx is less than min (%llx) for %d\n",
            size, minSize, mheapIdx);
        MASSERT( 0 && "MultiHeap::AddHeap: heap is less than minimum size for heap type!");
    }

    UINT64 maxSize = m_HeapAllocMax[mheapIdx];
    if ( (GetTotalHeapSize(mheapIdx) + size ) > maxSize )
    {
        Printf(Tee::PriAlways, "allocation of %llx would exceed max (%llx) for heap set %d\n",
            size, maxSize, mheapIdx);
        MASSERT(0 && "max alloc for heap type exceeded");
        return RC::SOFTWARE_ERROR;
    }

    // prepend to start of list of heaps for this kind
    h->SetNext( m_Heaps[mheapIdx] );
    m_Heaps[mheapIdx] = h;

    Printf(Tee::PriDebug, "Addition of heap 0x%p for heap set %d succeeded\n", h, mheapIdx);

    return OK;
}

RC MultiHeap::RmvHeap( int mheapIdx, Heap *h )
{
// Charlie suggested the following refinement:
/*
    Heap ** ppHeap = &m_Heaps[i];

    while (*ppHeap != NULL && *ppHeap != pHeapToRemove)
        ppHeap = &((*ppHeap)->next);

    *ppHeap = (*ppHeap)->next;  // patches either m_Heaps[i] or (some-heap->next)
*/
// But m_next is private.  And "ppHeap = &((*ppHeap)->GetNext());" garnered the following compiler error:
//  non-lvalue in unary `&'
// TODO: figure out whether that's unavoidable...

    MASSERT( h && "MultiHeap::RmvHeap called with null heap pointer!");
    MASSERT( ((mheapIdx >= 0) && (mheapIdx < GetNumHeapKinds())) && "MultiHeap::RmvHeap called with out-of-range heap idx");
    Heap *lwr = m_Heaps[mheapIdx];
    Heap *prev = lwr;

    while ( lwr && lwr != h )
    {
        prev = lwr;
        lwr = lwr->GetNext();
    }

    if ( lwr == NULL )
    {
        Printf(Tee::PriAlways, "Removal of heap 0x%p for heap set %d failed because 0x%p not found in this heap set\n",
            h, mheapIdx, h);
        MASSERT(0 && "MultiHeap::RmvHeap: attempt to remove unknown heap!");
        return RC::SOFTWARE_ERROR;
    }

    if ( prev == lwr )
    {
        m_Heaps[mheapIdx] = lwr->GetNext();
    }
    else
    {
        prev->SetNext( lwr->GetNext() );
    }
    Printf(Tee::PriDebug, "Removal of heap 0x%p for heap set %d succeeded\n", h, mheapIdx);

    return OK;
}

UINT64 MultiHeap::GetTotalHeapSize( int mheapIdx )
{
    MASSERT( ((mheapIdx >= 0) && (mheapIdx < GetNumHeapKinds())) && "MultiHeap::GetTotalHeapSize called with out-of-range heap idx");
    UINT64 r = 0;
    Heap *h = m_Heaps[mheapIdx];
    while (h != NULL)
    {
        r += h->GetTotalAllocated();
        h = h->GetNext();
    }
    return r;
}

