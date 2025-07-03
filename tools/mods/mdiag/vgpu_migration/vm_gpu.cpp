/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "vm_gpu.h"
#include "lwmisc.h" // DRF
#include "core/include/massert.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/sysspec.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/resource/lwgpu/dmard.h"
// LW_MMU_PTE_KIND_GENERIC_MEMORY*, drivers/common/inc/hwref/ampere/ga100/dev_mmu.h
#include "ampere/ga100/dev_mmu.h"
// LW0080_CTRL_DMA_FILL_PTE_MEM
#include "ctrl/ctrl0080/ctrl0080fb.h"
#include "mdiag/smc/smcengine.h"

#include "vgpu_migration.h"

namespace MDiagVmScope
{
    ///////////////////////Resource per Channel//////////////////////////////

    void Channels::ChRes::Init(LWGpuResource* pLwGpu, LwRm* pChLwRm, UINT32 engId, SmcEngine* pSmcEngine)
    {
        MASSERT(pLwGpu != nullptr);
        MASSERT(pChLwRm != nullptr);

        m_pLwGpu = pLwGpu;
        m_pChLwRm = pChLwRm;
        m_pSubdev = pLwGpu->GetGpuSubdevice();        
        m_pLwRm = GetGpuUtils()->GetLwRm();
        m_EngId = engId;
        m_pSmcEngine = pSmcEngine;
    }

    const RegHal& Channels::ChRes::GetRegHal() const
    {
        MASSERT(m_pLwGpu != nullptr);
        MASSERT(m_pSubdev != nullptr);
        MASSERT(m_pChLwRm != nullptr);
        return m_pLwGpu->GetRegHal(m_pSubdev, m_pChLwRm, m_pSmcEngine);
    }
    UINT32 Channels::ChRes::RegRead32(ModsGpuRegAddress regAddr) const
    {
        MASSERT(m_pLwGpu != nullptr);
        return GetRegHal().Read32(m_EngId, regAddr);
    }

    ///////////////////////Channels and DMA channel/reader/////////////////

    LWGpuResource* Channels::GetLwGpu() const
    {
        LWGpuResource* pLwGpu = GetGpuUtils()->GetLwGpu();
        if (m_pDmaCh != nullptr)
        {
            pLwGpu = m_pDmaCh->GetLWGpu();
        }

        return pLwGpu;
    }

    LwRm::Handle Channels::GetVaSpaceHandle()
    {
        const shared_ptr<VaSpace> pVaSpace = GetVaSpace(m_pDmaCh);
        MASSERT(pVaSpace);
        return pVaSpace->GetHandle();
    }

    LwRm* Channels::GetLwRm() const
    {
        LwRm* pLwRm = GetGpuUtils()->GetLwRm();
        if (m_pDmaCh != nullptr)
        {
            pLwRm = m_pDmaCh->GetLwRmPtr();
        }

        return pLwRm;
    }

    LWGpuChannel* Channels::GetDmaChannel() const
    {
        return m_pDmaCh;
    }
    RC Channels::StartDmaChannel()
    {
        RC rc;

        MASSERT(m_pDmaCh);
        LWGpuResource* pLwGpu = GetLwGpu();
        LwRm* pLwRm = GetLwRm();
        MASSERT(pLwGpu != nullptr);
        MASSERT(pLwRm != nullptr);
        DebugPrintf("%s, " vGpuMigrationTag ", Enable DMA channel scheduling, start DMA channel runlist, engineId 0x%x. HwClass 0x%x LwGpu %p.\n",
            __FUNCTION__,
            m_pDmaCh->GetEngineId(),
            m_pDmaCh->GetChannelClass(),
            m_pDmaCh->GetLWGpu());
        CHECK_RC(m_pDmaCh->ScheduleChannel(true));
        GpuDevice* pGpuDev = pLwGpu->GetGpuDevice();
        CHECK_RC(pGpuDev->StartEngineRunlist(m_pDmaCh->GetEngineId(), pLwRm));

        return rc;
    }
    RC Channels::StopDmaChannel()
    {
        RC rc;

        MASSERT(m_pDmaCh);
        LWGpuResource* pLwGpu = GetLwGpu();
        LwRm* pLwRm = GetLwRm();
        MASSERT(pLwGpu != nullptr);
        MASSERT(pLwRm != nullptr);
        DebugPrintf("%s, " vGpuMigrationTag ", Disable DMA channel scheduling, stop DMA channel runlist, engineId 0x%x. HwClass 0x%x LwGpu %p.\n",
            __FUNCTION__,
            m_pDmaCh->GetEngineId(),
            m_pDmaCh->GetChannelClass(),
            m_pDmaCh->GetLWGpu());
        m_pDmaCh->ScheduleChannel(false);
        GpuDevice* pGpuDev = pLwGpu->GetGpuDevice();
        CHECK_RC(pGpuDev->StopEngineRunlist(m_pDmaCh->GetEngineId(), pLwRm));

        return rc;
    }

    RC Channels::CreateDmaReader()
    {
        RC rc;
        //const UINT32 size = FbCopyLine::DmaCopyChunkSize;
        const UINT32 size = 0x10000;//FbCopyLine::DmaCopyChunkSize;
        LWGpuResource* pLwGpu = GetLwGpu();
        LwRm* pLwRm = GetLwRm();
        if (nullptr == m_pDmaReader)
        {
            MASSERT(pLwGpu != nullptr);
            MASSERT(pLwRm != nullptr);
            const UINT32 subdev = 0;
            DebugPrintf("%s, " vGpuMigrationTag ", Creating DMA reader, size 0x%x.\n",
                __FUNCTION__,
                size);
            DmaReader* reader = DmaReader::CreateDmaReader(
                    DmaReader::CE,
                    DmaReader::NOTIFY,
                    pLwGpu,
                    m_pDmaCh,
                    size,
                    GetMDiagVmResources()->GetArgs(),
                    pLwRm,
                    0,
                    subdev
                );

            reader->SetCopyLocation(Memory::Fb);
            reader->AllocateNotifier(PAGE_VIRTUAL_4KB);
            reader->AllocateObject();

            m_pDmaReader = reader;
            DebugPrintf("%s, " vGpuMigrationTag ", new DMA reader created.\n",
                __FUNCTION__);
        }
        else
        {
            DebugPrintf("%s, " vGpuMigrationTag ", Reuse existing DMA reader.\n",
                __FUNCTION__);
        }
        MASSERT(m_pDmaReader);

        m_pDmaCh = m_pDmaReader->GetChannel();
        MASSERT(m_pDmaCh);

        return rc;
    }

    void Channels::FreeDmaReader()
    {
        if (m_pDmaReader != nullptr)
        {
            DebugPrintf("%s, " vGpuMigrationTag ", Freeing DMA reader ...\n",
                __FUNCTION__);
            delete m_pDmaReader;
        }
        m_pDmaReader = nullptr;
    }

    const shared_ptr<VaSpace>& Channels::GetVaSpace(LWGpuChannel* pCh)
    {
        if (pCh != nullptr)
        {
            if (pCh->GetVASpace() != nullptr)
            {
                DebugPrintf("%s, " vGpuMigrationTag ", Get VaSpace from input channel directly.\n",
                    __FUNCTION__);
                return pCh->GetVASpace();
            }
        }
        if (m_pVaSpace != nullptr)
        {
            return m_pVaSpace;
        }

        LWGpuResource* pLwGpu = GetLwGpu();
        LwRm* pLwRm = GetLwRm();

        // Use the default VAS with handle 0
        string name = "default";
        const ArgReader* pArgs = GetMDiagVmResources()->GetArgs();
        if (pArgs->ParamPresent("-address_space_name"))
        {
            name = pArgs->ParamStr("-address_space_name");
            m_pVaSpace =  pLwGpu->GetVaSpaceManager(pLwRm)->GetResourceObject(LWGpuResource::TEST_ID_GLOBAL, name);
            DebugPrintf("%s, " vGpuMigrationTag ", Get VaSpace from cmdline argument.\n",
                __FUNCTION__);
        }
        else
        {
            // test Id doesn't matter here
            const UINT32 testId = 0;
            m_pVaSpace = pLwGpu->GetVaSpaceManager(pLwRm)->GetResourceObject(testId, name);

            // not created yet
            if (m_pVaSpace == nullptr)
            {
                m_pVaSpace = pLwGpu->GetVaSpaceManager(pLwRm)->CreateResourceObject(testId, name);
                DebugPrintf("%s, " vGpuMigrationTag ", Created default VaSpace.\n",
                    __FUNCTION__);
            }
            else
            {
                DebugPrintf("%s, " vGpuMigrationTag ", Got default VaSpace.\n",
                    __FUNCTION__);
            }
        }

        return m_pVaSpace;
    }

    ///////////////////////GPU Utils//////////////////////////////

    RC GpuUtils::Init()
    {
        RC rc;

        if (Platform::GetSimulationMode() == Platform::Amodel)
        {
            DebugPrintf("GpuUtils::%s, " vGpuMigrationTag
                " Skip init in Amodel.\n", __FUNCTION__);
            return OK;
        }
        // Check if -skip_dev_int ever specified.
        if (GpuPtr()->IsInitSkipped())
        {
            DebugPrintf("GpuUtils::%s, " vGpuMigrationTag
                " Device init skipped, skip other init step here as well.\n", __FUNCTION__);
            return OK;
        }

        SetLwGpu();

        // It's possible to have no GPU when running a Switch-only test, so just
        // return OK here.
        if (nullptr == m_pLwGpu)
        {
            DebugPrintf("GpuUtils::%s, " vGpuMigrationTag
                " No GPU detected. Returning immediately.\n", __FUNCTION__);
            return OK;
        }

        CHECK_RC(SetFbSize());

        return rc;
    }

    void GpuUtils::SetLwGpu()
    {
        m_pLwGpu = GetSmcResources()->GetDefaultLwGpu();
        if (nullptr == m_pLwGpu)
        {
            m_pLwGpu = LWGpuResource::FindFirstResource();
        }
    }

    GpuDevice* GpuUtils::GetDevice() const
    {
        return GetLwGpu()->GetGpuDevice();
    }

    GpuSubdevice* GpuUtils::GetSubdevice() const
    {
        return GetLwGpu()->GetGpuSubdevice();
    }

    RC GpuUtils::SetFbSize()
    {
        RC rc;
        GpuSubdevice* pSubdev = GetSubdevice();
        LwRm* pLwRm = GetLwRm();
        LW2080_CTRL_FB_INFO info = {0};
        LW2080_CTRL_FB_GET_INFO_PARAMS params = { 1, LW_PTR_TO_LwP64(&info) };

        info.index = LW2080_CTRL_FB_INFO_INDEX_USABLE_RAM_SIZE;

        CHECK_RC(pLwRm->ControlBySubdevice(
             pSubdev,
             LW2080_CTRL_CMD_FB_GET_INFO,
             &params, sizeof(params)));

        DebugPrintf("%s, " vGpuMigrationTag " LW2080_CTRL_FB_INFO_INDEX_USABLE_RAM_SIZE:\n"
            "  RamSizeKb 0x%x.\n",
            __FUNCTION__,
            info.data);

        m_FbSize = static_cast<UINT64>(info.data) << 10;

        return rc;
    }

    const RegHal& GpuUtils::GetRegHal() const
    {
        return GetSubdevice()->Regs();
    }

    UINT64 GpuUtils::GetFbSize() const
    {
        MASSERT(m_FbSize > 0);
        return m_FbSize; 
    }

    RC GpuUtils::QueryBar1MapSize(size_t* pSize)
    {
        RC rc;
        GpuSubdevice* pSubdev = GetSubdevice();
        LwRm* pLwRm = GetLwRm();

        LW2080_CTRL_FB_INFO info = {0};
        info.index = LW2080_CTRL_FB_INFO_INDEX_BAR1_MAX_CONTIGUOUS_AVAIL_SIZE;
        LW2080_CTRL_FB_GET_INFO_PARAMS params = { 1, LW_PTR_TO_LwP64(&info) };
        CHECK_RC(pLwRm->ControlBySubdevice(
             pSubdev,
             LW2080_CTRL_CMD_FB_GET_INFO,
             &params, sizeof(params)));
        m_Bar1MapSize = UINT64(info.data) << 10;
        DebugPrintf("%s, " vGpuMigrationTag " Available BAR1 size 0x%llx(%lluM).\n",
            __FUNCTION__,
            UINT64(m_Bar1MapSize),
            UINT64(m_Bar1MapSize) >> 20
            );
        if (pSize != nullptr)
        {
            *pSize = m_Bar1MapSize;
        }

        return OK;
    }

    RC GpuUtils::MapFbMem(FbMemSeg* pFbSeg, MemAlloc* pAlloc)
    {
        MASSERT(pFbSeg != nullptr);
        MASSERT(pAlloc != nullptr);
        UINT32 kind = pAlloc->GetKind();
        if (pAlloc->NeedKind())
        {
            MASSERT(LW_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE_DISABLE_PLC == kind ||
                LW_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE == kind);
        }

        RC rc;

        const UINT64 pageSize = MemStream::PageSize;  // 4k pagesize specified in flags
        UINT32 handle = 0;
        UINT64 addr = pFbSeg->GetAddr();
        UINT64 size = pFbSeg->GetSize();
        const UINT32 align = pageSize;
        CHECK_RC(MapFbMem(&addr,
            &size,
            align,
            kind,
            &handle,
            &pFbSeg->pCpuPtr));

        pAlloc->SetHandle(handle);
        pFbSeg->m_AllocDesc.SetAddr(addr);
        pFbSeg->m_AllocDesc.SetSize(size);
        DebugPrintf("%s - 1, " vGpuMigrationTag " Mapped PA 0x%llx(%lluM) size 0x%llx(%lluM), alignment 0x%x.\n",
            __FUNCTION__,
            addr,
            addr >> 20,
            size,
            size >> 20,
            align);

        return rc;
    }

    RC GpuUtils::UnmapFbMem(FbMemSeg* pFbSeg, MemAlloc* pAlloc)
    {
        MASSERT(pFbSeg != nullptr);
        MASSERT(pAlloc != nullptr);
        RC rc;

        if (pFbSeg->pCpuPtr)
        {
            MASSERT(pAlloc->GetHandle());
            CHECK_RC(UnmapFbMem(pAlloc->GetHandle()));
        }
        pFbSeg->pCpuPtr = nullptr;
        pAlloc->SetHandle(0);

        return rc;
    }

    RC GpuUtils::MapFbMem(UINT64* pAddr,
        UINT64* pSize,
        UINT64 align,
        UINT32 kind,
        UINT32* pHandle,
        void** pCpuPtr
        )
    {
        MASSERT(pAddr != nullptr);
        MASSERT(pSize != nullptr && *pSize > 0);
        MASSERT(pHandle != nullptr);
        MASSERT(pCpuPtr != nullptr);

        LwRm* pLwRm = GetLwRm();
        GpuDevice* pGpuDev = GetDevice();
        UINT64 paOff = *pAddr;
        UINT64 size = *pSize;
        DebugPrintf("%s - 0, " vGpuMigrationTag " to map PA 0x%llx(%lluM) size 0x%llx(%lluM), alignment 0x%llx, PTE kind 0x%x, LwRm %p GpuDev %p.\n",
            __FUNCTION__,
            paOff,
            paOff >> 20,
            size,
            size >> 20,
            align,
            kind,
            pLwRm,
            pGpuDev
            );
        {
            // Sanity check required size against available BAR1 size.
            size_t bar1Size = 0;
            QueryBar1MapSize(&bar1Size);
            MASSERT(size <= bar1Size);
            (void)bar1Size;
        }

        RC rc;

        LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS params = {0};
        PHYSADDR validAddr = align > 0 ? ALIGN_UP(paOff, align) : paOff;
        if (validAddr != paOff)
        {
            *pAddr = validAddr;
        }
        UINT64 validLen = align > 0 ? ALIGN_UP(size, align) : size;
        if (validLen != size)
        {
            *pSize = validLen;
        }
        params.VidOffset = validAddr;
        params.Offset = params.VidOffset;
        params.ValidLength = validLen;
        params.Length = params.ValidLength;
        params.AllocHintHandle = 0;
        params.hClient = pLwRm->GetClientHandle();
        params.hDevice = pLwRm->GetDeviceHandle(pGpuDev);

        params.Flags =
            DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
            DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _MAP_CPUVA, _YES) |
            DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _TRUE) |
            DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _PAGE_SIZE, _4K) |
            DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _KIND_PROVIDED, _YES)
            ;
        params.kind = kind;
        params.subDeviceIDMask = 1; // Just enable subdevice 0

        CHECK_RC(pLwRm->CreateFbSegment(pGpuDev, &params));

        DebugPrintf("%s, " vGpuMigrationTag " After BAR1 map, LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS:\n"
            "   Offset 0x%llx\n"
            "   VidOffset 0x%llx\n"
            "   ValidLength 0x%llx\n"
            "   Length 0x%llx\n"
            "   AllocHintHandle 0x%x\n"
            "   hClient 0x%x\n"
            "   hDevice 0x%x\n"
            "   Flags 0x%x\n"
            "   kind 0x%x\n"
            "   subDeviceIDMask 0x%x\n"
            "   ppCpuAddress %p\n"
            "   pGpuAddress 0x%llx, 0x%llx\n"
            ,
            __FUNCTION__,
            params.Offset,
            params.VidOffset,
            params.ValidLength,
            params.Length,
            params.AllocHintHandle,
            params.hClient,
            params.hDevice,
            params.Flags,
            params.kind,
            params.subDeviceIDMask,
            reinterpret_cast<void*>(params.ppCpuAddress[0]), // Win DVS needs this extra cast.
            params.pGpuAddress[0], params.pGpuAddress[1]
            );
        *pHandle = params.hMemory;
        *pCpuPtr = reinterpret_cast<void*>(params.ppCpuAddress[0]); // Win DVS needs this extra cast, while Linux needn't.

        return rc;
    }

    RC GpuUtils::UnmapFbMem(UINT32 handle)
    {
        RC rc;

        if (handle != 0)
        {
            LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS destroyFbSegParams = {0};
            destroyFbSegParams.hMemory = handle;
            CHECK_RC(GetLwRm()->DestroyFbSegment(GetDevice(), &destroyFbSegParams));
        }

        return rc;
    }

    RC GpuUtils::FillPteMemPa
    (
        LwRm* pLwRm,
        GpuDevice* pGpuDev,
        UINT64 vaOff,
        UINT64 size,
        LwRm::Handle hMem,
        UINT64 PhysAddr,
        LwRm::Handle hVASpace
    )
    {
        MASSERT(pLwRm != nullptr);
        MASSERT(pGpuDev != nullptr);

        RC rc;

        const UINT64 pageSize = MemStream::PageSize;
        const UINT64 pageCnt = size / pageSize;
        UINT08* pEntryData = nullptr;

        UINT32 PteFlags = 0U;
        PteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                                   _CONTIGUOUS, _TRUE, PteFlags);
        PteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                                   _VALID, _TRUE, PteFlags);
        PteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                                   _READ_ONLY, _FALSE, PteFlags);
        PteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                                   _APERTURE, _VIDEO_MEMORY, PteFlags);
        PteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                                   _SHADER_ACCESS, _READ_WRITE, PteFlags);
        PteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                                   _USE_COMPR_INFO, _FALSE, PteFlags);
        PteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                                   _OVERRIDE_PAGE_SIZE, _TRUE, PteFlags);

        LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS params = {0};
        UINT64 pageArray = PhysAddr & ~(pageSize - 1);
        params.pageCount = static_cast<UINT32>(pageCnt);
        params.hwResource.hClient = pLwRm->GetClientHandle();
        params.hwResource.hDevice = pLwRm->GetDeviceHandle(pGpuDev);
        params.hwResource.hMemory = hMem;
        params.offset = 0;
        params.gpuAddr = vaOff;
        params.pageArray = LW_PTR_TO_LwP64(&pageArray);
        params.hSrcVASpace = hVASpace;
        params.hTgtVASpace = hVASpace;
        params.pageSize = static_cast<UINT32>(pageSize);
        params.flags = PteFlags;
        params.pteMem = (LwP64)pEntryData;
        params.comprInfo.fbKind = LW_MMU_PTE_KIND_GENERIC_MEMORY;

        DebugPrintf("%s, " vGpuMigrationTag " Update VaSpace 0x%x hMem 0x%x VA 0x%llx mapped PTE PA to 0x%llx, start offset 0x%llx, page size 0x%llx count 0x%llx.\n",
            __FUNCTION__,
            hVASpace,
            hMem,
            vaOff,
            PhysAddr,
            params.offset,
            pageSize,
            pageCnt
            );

        CHECK_RC(pLwRm->ControlByDevice(pGpuDev,
            LW0080_CTRL_CMD_DMA_FILL_PTE_MEM,
            &params, sizeof(params)));

        return rc;
    }

    //////////////////Shorten access for GpuUtils/////////////////////

    GpuUtils* GetGpuUtils()
    {
        return GetMDiagVmResources()->GetGpuUtils();
    }

} // MDiagVmScope

