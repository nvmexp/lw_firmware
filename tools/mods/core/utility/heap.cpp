/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2017,2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include <stddef.h>
#include "core/include/heap.h"
#include "core/include/tee.h"
#include "random.h"
#include "core/include/massert.h"

struct Heap::HeapBlock
{
    bool IsFree;
    UINT64 Offset;
    UINT64 Size;
    HeapBlock *Next;
};

Heap::Heap(UINT64 Base, UINT64 Size, UINT32 gpuInst, AllocationType AllocType)
{
    MASSERT(Size > 0);

    m_Base = Base;
    m_Size = Size;
    m_AllocType = AllocType;
    m_ArraySize = 2;
    m_GpuInst = gpuInst;
    m_RawBlockArray = new HeapBlock[2];
    m_BlockList = &m_RawBlockArray[0];
    m_SpareBlockList = &m_RawBlockArray[1];
    m_Next = nullptr;

    m_BlockList->IsFree = true;
    m_BlockList->Offset = Base;
    m_BlockList->Size = Size;
    m_BlockList->Next = NULL;

    m_SpareBlockList->Next = NULL;
}

Heap::~Heap()
{
    delete[] m_RawBlockArray;
}

void Heap::Dump() const
{
    // List the blocks and their status
    Printf(Tee::PriNormal, "Heap of base %llx, size %lld\n", m_Base, m_Size);

    for (HeapBlock *block = m_BlockList; block; block = block->Next)
    {
        Printf(Tee::PriNormal, "  %s block at location %llx of size %lld\n",
               block->IsFree ? "Free" : "Used", block->Offset, block->Size);
    }

}

RC Heap::Alloc
(
    UINT64 Size,
    UINT64 *Offset,
    UINT64 Align,
    UINT64 AlignOffset
)
{
    return AllocRange(Size, Offset, 0, 0xFFFFFFFFFFFFFFFFULL, Align, AlignOffset);
}

RC Heap::AllocAt
(
    UINT64 Size,
    UINT64 Offset
)
{
    UINT64 TmpOffset;
    RC rc;

    // Use range feature to implement this
    CHECK_RC(AllocRange(Size, &TmpOffset, Offset, Offset+Size-1));
    MASSERT(TmpOffset == Offset);

    return OK;
}

RC Heap::AllocRandom
(
    Random *pRandom,
    UINT64 Size,
    UINT64 *Offset,
    UINT64 Min,
    UINT64 Max,
    UINT64 Align,
    UINT64 AlignOffset
)
{
    // For colwenience: allow pRandom to be NULL for no randomization
    if (!pRandom)
        return AllocRange(Size, Offset, Min, Max, Align, AlignOffset);

    // Make 16 tries to do a randomly-located allocation
    for (int i = 0; i < 16; i++)
    {
        // Pick a random offset within the heap and align it to be valid
        // We might end up with something that falls outside the heap
        // boundaries, or that is already allocated, or outside the user's
        // range; that's OK, it'll get filtered out below if so
        *Offset = m_Base + pRandom->GetRandom64(0, m_Size-1);
        *Offset = (*Offset / Align) * Align;
        *Offset += AlignOffset;

        // Check whether this satisfies the user's min/max
        if (*Offset < Min)
            continue;
        if (*Offset+Size-1 > Max)
            continue;

        // Try allocating at this spot
        if (OK == AllocAt(Size, *Offset))
            return OK;
    }

    // No luck in 16 tries.  Just do a regular allocation
    return AllocRange(Size, Offset, Min, Max, Align, AlignOffset);
}

RC Heap::AllocRange
(
    UINT64 Size,
    UINT64 *Offset,
    UINT64 Min,
    UINT64 Max,
    UINT64 Align,
    UINT64 AlignOffset
)
{
    HeapBlock *block;

    // Must align to a power of two
    // Alignment offset should be less than the total alignment
    if ((Align & (Align-1)) || (AlignOffset >= Align))
        return RC::CANNOT_ALLOCATE_MEMORY;

    // We might need as many as two additional blocks (due to alignment waste).
    // Make sure that we have enough blocks in advance.
    if (!m_SpareBlockList || !m_SpareBlockList->Next)
    {
        // Out of spare blocks.  Get more of them.
        ptrdiff_t Delta;
        UINT32 i, NewArraySize;
        HeapBlock *NewBlockArray;

        // Grow by 8 ensures that we have at least 2 blocks and also is a little
        // more efficient than growing by 1 each time.
        NewArraySize = m_ArraySize + 8;
        NewBlockArray = new HeapBlock[NewArraySize];
        if (!NewBlockArray)
            return RC::CANNOT_ALLOCATE_MEMORY;

        // Set up all the new blocks we allocated
        Delta = (char *)NewBlockArray - (char *)m_RawBlockArray;
        for (i = 0; i < m_ArraySize; i++)
        {
            NewBlockArray[i] = m_RawBlockArray[i];
            if (NewBlockArray[i].Next)
            {
                NewBlockArray[i].Next = (HeapBlock *)
                ((char *)NewBlockArray[i].Next + Delta);
            }
        }
        for (i = m_ArraySize; i < NewArraySize; i++)
        {
            NewBlockArray[i].Next = &NewBlockArray[i + 1];
        }
        if (m_SpareBlockList)
        {
            m_SpareBlockList = (HeapBlock *)
                ((char *)m_SpareBlockList + Delta);
        }
        NewBlockArray[NewArraySize - 1].Next = m_SpareBlockList;
        delete[] m_RawBlockArray;

        // Update all our information
        m_RawBlockArray = NewBlockArray;
        m_BlockList = (HeapBlock *)((char *)m_BlockList + Delta);
        m_SpareBlockList = &NewBlockArray[m_ArraySize];
        m_ArraySize = NewArraySize;

    }

    // Scan through the list of blocks
    for (block = m_BlockList; block; block = block->Next)
    {
        // Skip blocks that are not free
        if (!block->IsFree)
        {
            continue;
        }

        // Compute location where this allocation would start in this block, based
        // on the alignment and range requested
        UINT64 NewOffset = block->Offset;
        if (NewOffset < Min)
            NewOffset = Min;
        NewOffset -= AlignOffset;
        NewOffset = (NewOffset + Align-1) & ~(Align-1);
        NewOffset += AlignOffset;
        MASSERT(NewOffset >= block->Offset);
        MASSERT(NewOffset >= Min);
        MASSERT((NewOffset & (Align-1)) == AlignOffset);
        UINT64 ExtraAlignSpace = NewOffset - block->Offset;

        // Is the block too small to fit this allocation, including the extra space
        // required for alignment?
        if (block->Size < Size + ExtraAlignSpace)
            continue;

        // Are we past the end of the range where the allocation is permitted?
        if (NewOffset + Size-1 > Max)
            break;

        // Do we need to split this block in two to start the allocation at the proper
        // alignment?
        if (ExtraAlignSpace > 0)
        {
            HeapBlock *next;

            // Grab a block off the spare list and link it into place
            next = m_SpareBlockList;
            m_SpareBlockList = next->Next;
            next->Next = block->Next;
            block->Next = next;

            // Set up that block
            next->IsFree = true;
            next->Offset = block->Offset + ExtraAlignSpace;
            next->Size = block->Size - ExtraAlignSpace;

            // Shrink the current block to match this allocation
            block->Size = ExtraAlignSpace;

            // Advance to the block we are actually going to allocate out of
            block = next;
        }

        // Do we need to split this block into two?
        if (block->Size > Size)
        {
            HeapBlock *next;

            // Grab a block off the spare list and link it into place
            next = m_SpareBlockList;
            m_SpareBlockList = next->Next;
            next->Next = block->Next;
            block->Next = next;

            // Set up that block
            next->IsFree = true;
            next->Offset = block->Offset + Size;
            next->Size = block->Size - Size;

            // Shrink the current block to match this allocation
            block->Size = Size;
        }

        MASSERT(block->Size == Size);
        block->IsFree = false;

        *Offset = block->Offset;
        return OK;
    }

    // Couldn't find anywhere to put it
    return RC::CANNOT_ALLOCATE_MEMORY;
}

RC Heap::Free
(
    UINT64 Offset
)
{
    HeapBlock *prev, *block, *next;

    // Find the block we're being asked to free
    prev = NULL;
    block = m_BlockList;
    while (block && (block->Offset != Offset))
    {
        prev = block;
        block = block->Next;
    }

    // The block we're being asked to free didn't exist or was already free
    if (!block || block->IsFree)
        return RC::ILWALID_ADDRESS;

    // This block is now a free block
    block->IsFree = true;

    // If next block is free, merge the two into one block
    next = block->Next;
    if (next && next->IsFree)
    {
        block->Size += next->Size;
        block->Next = next->Next;

        next->Next = m_SpareBlockList;
        m_SpareBlockList = next;
    }

    // If previous block is free, merge the two into one block
    if (prev && prev->IsFree)
    {
        prev->Size += block->Size;
        prev->Next = block->Next;

        block->Next = m_SpareBlockList;
        m_SpareBlockList = block;
    }

    return OK;
}

RC Heap::GetAllocSize
(
    UINT64 Offset,
    UINT64 *Size
) const
{
    HeapBlock *block;

    // Find the block we're being asked to get the size of
    block = m_BlockList;
    while (block && (block->Offset != Offset))
    {
        block = block->Next;
    }

    // The block we're being asked to get the size of didn't exist or was free
    if (!block || block->IsFree)
        return RC::ILWALID_ADDRESS;

    // Return the size
    *Size = block->Size;
    return OK;
}

UINT64 Heap::GetTotalAllocated() const
{
    UINT64 Total = 0;
    for (HeapBlock *block = m_BlockList; block; block = block->Next)
    {
        if (!block->IsFree)
            Total += block->Size;
    }
    return Total;
}

UINT64 Heap::GetTotalFree() const
{
    UINT64 Total = 0;
    for (HeapBlock *block = m_BlockList; block; block = block->Next)
    {
        if (block->IsFree)
            Total += block->Size;
    }
    return Total;
}

UINT64 Heap::GetLargestFree() const
{
    UINT64 Largest = 0;
    for (HeapBlock *block = m_BlockList; block; block = block->Next)
    {
        if (block->IsFree && (block->Size > Largest))
            Largest = block->Size;
    }
    return Largest;
}

UINT64 Heap::GetOffsetLargestFree() const
{
    UINT64 Largest = 0;
    UINT64 Offset  = 0;
    for (HeapBlock *block = m_BlockList; block; block = block->Next)
    {
        if (block->IsFree && (block->Size > Largest))
        {
            Largest = block->Size;
            Offset  = block->Offset;
        }
    }
    return Offset;
}
