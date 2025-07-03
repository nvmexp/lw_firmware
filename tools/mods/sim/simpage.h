/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 1999-2013 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// Simulated page table interface

#ifndef INCLUDED_SIMPAGE_H
#define INCLUDED_SIMPAGE_H

#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif

#define PAGE_VALID            0x00000001
#define PAGE_READABLE         0x00000002
#define PAGE_WRITEABLE        0x00000004
#define PAGE_WRITE_COMBINED   0x00000008
#define PAGE_DIRECT_MAP       0x00000010
#define PAGE_DIRECT_MAP_DUP   0x00000020

// Page Table
struct PageTableEntry
{
    PHYSADDR PhysAddr;
    UINT32 Flags;
};

struct PageTable
{
    PageTable();

    enum
    {
        PTBits = 10,
        PTSize = (1 << PTBits),
        PTMask = (PTSize - 1)
    };

    UINT32 RefCount;
    PageTableEntry PteArray[PTSize];
};

typedef PageTable*  PPageTable;
typedef vector<pair<uintptr_t, PageTable*> > PageTableList; // pair<PdIdx, PageTable*>

namespace SimPageTable
{
    extern const UINT64 NativePageSize;

    extern UINT64 SimPageSize;
    extern UINT64 SimPageMask;
    extern UINT32 SimPageShift;

    const PageTableEntry *GetVirtualAddrPte(const volatile void *VirtualAddr);
    UINT32 GetVirtualAddrPteOffset(const volatile void *VirtualAddr);

    void InitializeSimPageSize(UINT64 PageSize);

    RC PopulatePages(void *VirtualAddress, size_t NumBytes);
    void UnpopulatePages(void *VirtualAddress, size_t NumBytes);

    void Cleanup();
    void MapPages
    (
        void *   VirtualAddress,
        PHYSADDR PhysicalAddress,
        size_t   NumBytes,
        UINT32   Flags
    );
    void UnmapPages(void *VirtualAddress, size_t NumBytes);

    void *PhysicalToVirtual(PHYSADDR PhysicalAddress, bool IsSOC);

    void Dump();
}

#endif
