/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef VM_GPU_H
#define VM_GPU_H

#include "mdiag/utils/mdiagsurf.h"
#include "core/include/lwrm.h"
#include "gpu/reghal/reghal.h"
#include "mdiag/vgpu_migration/vm_mem.h"

#ifdef _WIN32
#   pragma warning(push)
#   pragma warning(disable: 4265) // class has virtual functions, but destructor is not virtual
#endif
#ifdef _WIN32
#   pragma warning(pop)
#endif

class GpuDevice;
class GpuSubdevice;
class LwRm;
class VaSpace;
class LWGpuResource;
class LWGpuChannel;
class DmaReader;
class SmcEngine;

namespace MDiagVmScope
{
    // Channel related resources.
    class Channels
    {
    public:
        // Per channel resource.
        class ChRes
        {
        public:
            void Init(LWGpuResource* pLwGpu, LwRm* pChLwRm, UINT32 engId, SmcEngine* pSmcEngine);
            UINT32 GetEngineId() const { return m_EngId; }
            LWGpuResource* GetLwGpu() const { return m_pLwGpu; }
            // The device LwRm.
            LwRm* GetLwRm() const { return m_pLwRm; }
            // The channel specific LwRm.
            LwRm* GetChLwRm() const { return m_pChLwRm; }
            GpuSubdevice* GetGpuSubdevice() const { return m_pSubdev; }
            const RegHal& GetRegHal() const;
            UINT32 RegRead32(ModsGpuRegAddress regAddr) const;

        protected:
            LWGpuResource* m_pLwGpu = nullptr;
            LwRm* m_pChLwRm = nullptr;
            LwRm* m_pLwRm = nullptr;
            GpuSubdevice* m_pSubdev = nullptr;
            UINT32 m_EngId = 0;
            SmcEngine* m_pSmcEngine = nullptr;
        };

    public:
        LWGpuResource* GetLwGpu() const;
        LwRm::Handle GetVaSpaceHandle();
        LwRm* GetLwRm() const;
        // Get a DMA channel for FB DMA copy.
        LWGpuChannel* GetDmaChannel() const;
        RC StartDmaChannel();
        RC StopDmaChannel();
        // Create/free or get a DMA reader for FB DMA copy.
        RC CreateDmaReader();
        void FreeDmaReader();
        DmaReader* GetDmaReader() const { return m_pDmaReader; }

    private:
        const shared_ptr<VaSpace>& GetVaSpace(LWGpuChannel* pCh);

        shared_ptr<VaSpace> m_pVaSpace;
        LWGpuChannel* m_pDmaCh = nullptr;
        DmaReader* m_pDmaReader = nullptr;
    };

    class MemAlloc;

    // GPU related resources and utilities.
    class GpuUtils
    {
    public:
        RC Init();

        // Get default or 1st stuff.
        LwRm* GetLwRm() const { return m_pLwRm.Get(); }        
        LWGpuResource* GetLwGpu() const { return m_pLwGpu; }
        Channels* GetChannels() { return &m_Channels; }
        GpuDevice* GetDevice() const;
        GpuSubdevice* GetSubdevice() const;
        const RegHal& GetRegHal() const;
        // FB mem size. Return the saved size value.
        UINT64 GetFbSize() const;
        // To learn available BAR1 size for FB mem segment mapping on.
        RC QueryBar1MapSize(size_t* pSize);
        // Return the saved available BAR1 size.
        size_t GetBar1MapSize() const { return m_Bar1MapSize; }

        // Un/Map an FB mem segment on BAR1.
        RC MapFbMem(FbMemSeg* pFbSeg, MemAlloc* pAlloc);
        RC UnmapFbMem(FbMemSeg *pFbSeg, MemAlloc* pAlloc);

        // The real BAR1 un/mapping.
        RC MapFbMem(UINT64* pAddr,
            UINT64* pSize,
            UINT64 align,
            UINT32 kind,
            LwRm::Handle* pHandle,
            void** pCpuPtr
            );
        RC UnmapFbMem(UINT32 handle);

        // Used to update surface allocation's PTE PA offset to map random required
        // FB mem segment, which MdiagSurf/Surf2D can't get done.
        RC FillPteMemPa
        (
            LwRm* pLwRm,
            GpuDevice* pGpuDev,
            UINT64 vaOff,
            UINT64 size,
            LwRm::Handle hMem,
            UINT64 PhysAddr,
            LwRm::Handle hVASpace
        );

    private:
        RC SetFbSize();
        void SetLwGpu();

        Channels m_Channels;
        LwRmPtr m_pLwRm;
        LWGpuResource* m_pLwGpu = nullptr;
        UINT64 m_FbSize = 0;
        size_t m_Bar1MapSize = 0;
    };

    // Shorten access for GpuUtils
    GpuUtils* GetGpuUtils();

} // MDiagVmScope

#endif

