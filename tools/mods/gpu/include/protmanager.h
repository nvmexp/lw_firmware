/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2018,2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#pragma once

#ifndef __PROTMANAGER_H__
#define __PROTMANAGER_H__

#include "core/include/rc.h"
#include "core/include/lwrm.h"

class GpuDevice;

//--------------------------------------------------------------------------
//! \brief Class that allocates and manages protected memory regions (e.g. VPR)
//!
class ProtectedRegionManager
{
public:
    ProtectedRegionManager(GpuDevice *gpuDevice);
    ~ProtectedRegionManager();

    RC AllocRegions
    (
        UINT64 vprSize,
        UINT64 vprAlignment,
        UINT64 acr1Size,
        UINT64 acr1Alignment,
        UINT64 acr2Size,
        UINT64 acr2Alignment
    );

    RC FreeRegions();
    RC InitSubdevice(GpuSubdevice *gpuSubdevice);

    RC AllocVprMem
    (
        UINT64 size,
        UINT32 type,
        UINT32 flags,
        UINT32 attr,
        UINT32 attr2,
        LwRm::Handle *handle,
        PHYSADDR *physAddr
    );

    RC ReserveVprMem
    (
        UINT64 size,
        UINT64 *physAddr,
        void **sysMemDesc,
        UINT64 *sysMemOffset
    );

    RC AllocAcr1Mem
    (
        UINT64 size,
        UINT32 type,
        UINT32 flags,
        UINT32 attr,
        UINT32 attr2,
        LwRm::Handle *handle,
        PHYSADDR *physAddr
    );

    RC AllocAcr2Mem
    (
        UINT64 size,
        UINT32 type,
        UINT32 flags,
        UINT32 attr,
        UINT32 attr2,
        LwRm::Handle *handle,
        PHYSADDR *physAddr
    );

    RC AllolwprMem
    (
        UINT64 size,
        UINT32 type,
        UINT32 flags,
        UINT32 attr,
        UINT32 attr2,
        LwRm::Handle *handle,
        PHYSADDR *physAddr
    );

    vector<std::pair<PHYSADDR, UINT64>> GetUprRegionList();

    bool IsVprRegionAllocated() const { return m_VprRegion.IsAllocated(); }
    bool IsAcr1RegionAllocated() const { return m_Acr1Region.IsAllocated(); }
    bool IsAcr2RegionAllocated() const { return m_Acr2Region.IsAllocated(); }
    bool IsUprRegionAllocated() const { return m_UprRegions.size() > 0; }

private:
    struct RegionInfo
    {
        string Name;               // Name of the region (e.g. VPR)
        UINT64 Size = 0;           // Size of the region
        LwRm::Handle Handle = 0;   // Memory handle for the region allocation
        PHYSADDR BasePhysAddr = 0; // Base physical address of the region
        PHYSADDR NextPhysAddr = 0; // Next unused physical address
        UINT64 Alignment = 0;      // Required alignment of the base address
        void *MemDesc = nullptr;   // Pointer to sysmem memory descriptor
        bool ReadExisting = false; // Read registers to get the already
                                   // existing region (no need to allocate).
        bool FixedAddr = false;    // Pass BasePhysAddr to RM when allocating
        bool MemDescFromCarveout = false; 
                                   // Mark whether memdesc comes from carveout

        void Free();
        bool IsAllocated() const { return (Size > 0); }
        bool InSysmem() const { return (MemDesc != nullptr); }
        UINT64 UnusedSize() const { return Size - (NextPhysAddr - BasePhysAddr); }
    };

    RC AllolwprRegions();
    RC AllocRegion(RegionInfo *region);
    RC AllocRegionInSysmem(RegionInfo *region);
    RC AllocRegionIlwidmem(RegionInfo *region);

    RC AllocFromRegion
    (
        RegionInfo *region,
        UINT64 size,
        UINT32 type,
        UINT32 flags,
        UINT32 attr,
        UINT32 attr2,
        LwRm::Handle *handle,
        PHYSADDR *physAddr
    );

    RC ReserveFromRegion
    (
        RegionInfo *region,
        UINT64 size,
        PHYSADDR *physAddr
    );

    GpuDevice *m_GpuDevice;
    RegionInfo m_VprRegion;
    RegionInfo m_Acr1Region;
    RegionInfo m_Acr2Region;
    vector<unique_ptr<RegionInfo>> m_UprRegions;
    vector<LwRm::Handle> m_AllocatedHandles;
};

#endif // __PROTMANAGER_H__
