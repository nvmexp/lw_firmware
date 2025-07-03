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

// Simulated page table for 32-bit systems
#include <vector>
#include <map>
#include "core/include/massert.h"
#include "simpage.h"
#include "core/include/xp.h"
#include "core/include/tee.h"
#include "simpdpolicy.h"

#include "cheetah/include/tegrasocdevice.h"
#include "cheetah/include/tegrasocdeviceptr.h"

#ifdef PPC64LE
const UINT64 SimPageTable::NativePageSize = 65536;
#else
const UINT64 SimPageTable::NativePageSize = 4096;
#endif

UINT64 SimPageTable::SimPageSize = 0;
UINT64 SimPageTable::SimPageMask = 0;
UINT32 SimPageTable::SimPageShift = 0;

const size_t NoPteIndex = ~static_cast<size_t>(0);

// struct PageTable
//------------------------------------------------------------------------------
PageTable::PageTable()
{
    RefCount = 0;
    memset(PteArray, 0, sizeof(PageTableEntry) * PTSize);
}

// Cache the last few PTEs looked up for physical-to-virtual translation
// TLB hit rate reaches 99% with size 16
const int PHYS_TO_VIRT_TLB_SIZE = 16;
static size_t s_PhysToVirtTlb[PHYS_TO_VIRT_TLB_SIZE];
static int s_NextTlbEntry = 0;

//------------------------------------------------------------------------------
static inline PageTableEntry *GetPte(uintptr_t Addr)
{
    size_t pdeIdx = PageDirPolicy()->GetPdeIndex(Addr);
    PageTable *Table = PageDirPolicy()->GetPageTablebyIdx(pdeIdx);
    if (!Table)
        return NULL;
    return &Table->PteArray[(Addr >> SimPageTable::SimPageShift) & PageTable::PTMask];
}

//------------------------------------------------------------------------------
const PageTableEntry *SimPageTable::GetVirtualAddrPte(const volatile void *VirtualAddr)
{
    const PageTableEntry *Pte = GetPte((uintptr_t)VirtualAddr);
    if (!Pte)
        return NULL;
    if (!(Pte->Flags & PAGE_VALID))
        return NULL;
    return Pte;
}

//------------------------------------------------------------------------------
UINT32 SimPageTable::GetVirtualAddrPteOffset(const volatile void *VirtualAddr)
{
    return (UINT32)(uintptr_t)VirtualAddr & (SimPageTable::SimPageSize-1);
}

//------------------------------------------------------------------------------
void SimPageTable::InitializeSimPageSize(UINT64 PageSize)
{
    SimPageSize = PageSize << 10;
    SimPageMask = SimPageSize - 1;

    UINT64 temp = SimPageMask;
    int i = 0;

    while (temp != 0)
    {
        temp >>= 1;
        ++i;
    }

    SimPageShift = i;

    PageDirPolicy()->Initialize(
        sizeof(void*) * 8,                                                 // vaBitsNum
        sizeof(void*) * 8 - PageTable::PTBits - SimPageTable::SimPageShift // pagedirBitsNum
    );

}

//------------------------------------------------------------------------------
RC SimPageTable::PopulatePages
(
    void * VirtualAddress,
    size_t NumBytes
)
{
    // Compute the first and last byte being mapped, and therefore the first and
    // last page table we need to exist
    uintptr_t FirstAddr = (uintptr_t)VirtualAddress;
    uintptr_t LastAddr  = (uintptr_t)VirtualAddress + NumBytes - 1;
    size_t FirstPde = PageDirPolicy()->GetPdeIndex(FirstAddr);
    size_t LastPde  = PageDirPolicy()->GetPdeIndex(LastAddr);

    // Populate the missing page tables
    for (size_t i = FirstPde; i <= LastPde; i++)
    {
        if (!PageDirPolicy()->GetPageTablebyIdx(i))
        {
            PageTable* pNewPageTable = PageDirPolicy()->AllocNewPageTable(i);
            if (!pNewPageTable)
            {
                // Before we return an error, unpopulate any previous page
                // tables that we created as part of this operation.
                for (size_t j = FirstPde; j < i; j++)
                {
                    PageTable* pPageTable = PageDirPolicy()->GetPageTablebyIdx(j);
                    if (pPageTable && !pPageTable->RefCount)
                    {
                        PageDirPolicy()->FreePageTable(j);
                    }
                }
                return RC::CANNOT_ALLOCATE_MEMORY;
            }
            *pNewPageTable = { };
        }
    }

    return OK;
}

//------------------------------------------------------------------------------
void SimPageTable::UnpopulatePages
(
    void * VirtualAddress,
    size_t NumBytes
)
{
    // Compute the first and last byte, and therefore the first and last page
    // table
    uintptr_t FirstAddr = (uintptr_t)VirtualAddress;
    uintptr_t LastAddr  = (uintptr_t)VirtualAddress + NumBytes - 1;
    size_t FirstPde = PageDirPolicy()->GetPdeIndex(FirstAddr);
    size_t LastPde  = PageDirPolicy()->GetPdeIndex(LastAddr);

    // Unpopulate the empty page tables
    for (size_t i = FirstPde; i <= LastPde; i++)
    {
        PageTable* pPageTable = PageDirPolicy()->GetPageTablebyIdx(i);
        if (pPageTable && !pPageTable->RefCount)
        {
            PageDirPolicy()->FreePageTable(i);
        }
    }
}

//------------------------------------------------------------------------------
void SimPageTable::Cleanup()
{
    // We're not really cleaning up anything; we're just checking for leaks.
    // All the page tables should have been freed as we unmapped things earlier.
    auto pageTables = PageDirPolicy()->EnumerateAllPageTables();
    if(pageTables)
    {
        for (size_t i = 0; i < pageTables->size(); i++)
        {
            MASSERT(!((*pageTables)[i].second));
        }
    }
}

//------------------------------------------------------------------------------
void SimPageTable::MapPages
(
    void *   VirtualAddress,
    PHYSADDR PhysicalAddress,
    size_t   NumBytes,
    UINT32   Flags
)
{
    // Fill in the PTEs
    for (size_t i = 0; i < NumBytes; i += SimPageTable::SimPageSize)
    {
        // Look up the PTE
        uintptr_t Addr = (uintptr_t)VirtualAddress + i;
        size_t PdeIndex = PageDirPolicy()->GetPdeIndex(Addr);
        PageTable *Table = PageDirPolicy()->GetPageTablebyIdx(PdeIndex);
        MASSERT(Table);
        MASSERT(Table->RefCount < PageTable::PTSize);
        PageTableEntry *Pte = &Table->PteArray[(Addr >> SimPageTable::SimPageShift) & PageTable::PTMask];
        MASSERT(!(Pte->Flags & PAGE_VALID));

        // Fill in the PTE
        Pte->PhysAddr = PhysicalAddress + i;
        Pte->Flags = Flags | PAGE_VALID;
        Table->RefCount++;
    }
}

//------------------------------------------------------------------------------
void SimPageTable::UnmapPages
(
    void * VirtualAddress,
    size_t NumBytes
)
{
    // Blank out the PTEs
    for (size_t i = 0; i < NumBytes; i += SimPageTable::SimPageSize)
    {
        // Look up the PTE
        uintptr_t Addr = (uintptr_t)VirtualAddress + i;
        size_t PdeIndex = PageDirPolicy()->GetPdeIndex(Addr);
        PageTable *Table = PageDirPolicy()->GetPageTablebyIdx(PdeIndex);
        MASSERT(Table);
        MASSERT(Table->RefCount > 0);
        PageTableEntry *Pte = &Table->PteArray[(Addr >> SimPageTable::SimPageShift) & PageTable::PTMask];
        MASSERT(Pte->Flags & PAGE_VALID);

        // Clear out the PTE; destroy its page table if possible
        Pte->PhysAddr = 0;
        Pte->Flags = 0;
        Table->RefCount--;
        if (!Table->RefCount)
        {
            PageDirPolicy()->FreePageTable(PdeIndex);
        }
    }
}

//------------------------------------------------------------------------------
static size_t FindPhysAddrPteIndex(PHYSADDR PhysAddr)
{
    PHYSADDR PageAddr = (PhysAddr >> SimPageTable::SimPageShift) << SimPageTable::SimPageShift;

    // Check cached PTEs
    for (int TlbEntry = 0; TlbEntry < PHYS_TO_VIRT_TLB_SIZE; TlbEntry++)
    {
        size_t PteIndex = s_PhysToVirtTlb[TlbEntry];
        PageTableEntry *Pte = GetPte(PteIndex << SimPageTable::SimPageShift);
        if (Pte && (Pte->Flags & PAGE_VALID) && (Pte->PhysAddr == PageAddr))
        {
            // TLB hit
            return PteIndex;
        }
    }

    auto pageTables = PageDirPolicy()->EnumerateAllPageTables();
    if(!pageTables)
    {
        return NoPteIndex;
    }

    for (size_t i = 0; i < pageTables->size(); i++)
    {
        PageTable *Table = (*pageTables)[i].second;
        if (!Table)
            continue;
        for (size_t j = 0; j < PageTable::PTSize; j++)
        {
            PageTableEntry *Pte = &Table->PteArray[j];

            if ((Pte->Flags & PAGE_VALID) && (Pte->PhysAddr == PageAddr) &&
                !(Pte->Flags & PAGE_DIRECT_MAP_DUP))
            {
                // Put this PTE in the TLB
                size_t PteIndex = (*pageTables)[i].first * PageTable::PTSize + j;
                s_PhysToVirtTlb[s_NextTlbEntry++] = PteIndex;
                if (s_NextTlbEntry == PHYS_TO_VIRT_TLB_SIZE)
                    s_NextTlbEntry = 0;
                return PteIndex;
            }
        }
    }

    // Didn't find it
    return NoPteIndex;
}

//------------------------------------------------------------------------------
static UINT32 GetPhysAddrPteOffset(PHYSADDR PhysAddr)
{
    return UINT32(PhysAddr) & (SimPageTable::SimPageSize-1);
}

//------------------------------------------------------------------------------
void * SimPageTable::PhysicalToVirtual(PHYSADDR PhysicalAddress, bool IsSOC)
{
    uintptr_t VirtAddr;

    // Find the PTE for this page
    size_t PteIndex = FindPhysAddrPteIndex(PhysicalAddress);
    if (PteIndex == NoPteIndex)
    {
        if (IsSOC)
        {
            // The device may not have been mapped yet.
            void* pLinAperture = 0;
            CheetAh::DeviceDesc devDesc;

            if (OK == CheetAh::SocPtr()->GetDeviceDescByPhysAddr(PhysicalAddress, &devDesc))
            {
                if (OK == CheetAh::SocPtr()->GetAperture(devDesc.devIndex, &pLinAperture))
                {
                    PteIndex = FindPhysAddrPteIndex(PhysicalAddress);
                }
            }
        }
        if (PteIndex == NoPteIndex)
        {
            // Not found, bogus address
            MASSERT(!"PhysicalToVirtual translation failed");
            return NULL;
        }
    }

    // Construct address
    VirtAddr  = PteIndex << SimPageTable::SimPageShift;
    VirtAddr |= GetPhysAddrPteOffset(PhysicalAddress);

    return (void *)VirtAddr;
}

//------------------------------------------------------------------------------
void SimPageTable::Dump()
{
    MASSERT(!"SimPageTable::Dump has not yet been reimplemented");
}
