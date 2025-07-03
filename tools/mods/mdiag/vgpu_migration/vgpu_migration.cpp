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

#include "vgpu_migration.h"
#include "core/include/massert.h"
#include "core/include/utility.h"
#include "gpu/include/gpudev.h"
#include "gpu/include/gpusbdev.h"
#include "mdiag/sysspec.h"
#include "mdiag/lwgpu.h"

#include "fermi/gf100/dev_bus.h"
#include "fermi/gf100/dev_ram.h"
// LW_MMU_PTE_KIND_GENERIC_MEMORY*, drivers/common/inc/hwref/ampere/ga100/dev_mmu.h
#include "ampere/ga100/dev_mmu.h"
// LW_RUNLIST_SUBMIT_BASE*
#include "ampere/ga100/dev_runlist.h"
#include "mdiag/vmiopmods/vmiopmdiagelwmgr.h"
#include "mdiag/utils/mdiagsurf.h"
#include "mdiag/utils/lwgpu_channel.h"
#include "mdiag/resource/lwgpu/dmard.h"
#include "mdiag/smc/smcengine.h"
#include "mdiag/smc/gpupartition.h"

#include "vm_hexfile.h"

extern const ParamDecl trace_3d_params[];

static const ParamDecl s_VmParams[] = {
    SIMPLE_PARAM("-enable_vgpu_migration", "Enable MODS vGpu Migration from command line."),

    PARAM_SUBDECL(trace_3d_params),
    LAST_PARAM
};

namespace MDiagVmScope
{
    ///////////Task Descriptor///////////////

    void Task::Desc::QueryFbMemSize(UINT64* pSize)
    {
        const UINT64 fbSize = GetGpuUtils()->GetFbSize();
        DebugPrintf("FB Copy task, " vGpuMigrationTag " Got FB mem length 0x%llx(%lldM).\n",
            fbSize,
            fbSize >> 20);
        if (pSize != nullptr)
        {
            *pSize = fbSize;
        }
    }

    void Task::DescCreator::AddDesc(Desc* pData)
    {
        MASSERT(pData != nullptr);
        unique_ptr<Desc> ptr;
        ptr.reset(pData);
        ptr->SetCreator(this);
        const UINT64 id = ptr->GetId();
        auto it = m_List.find(id);
        MASSERT(it == m_List.end());
        if (it == m_List.end())
        {
            m_List[id] = std::move(ptr);
        }
    }

    void Task::DescCreator::FreeDesc(UINT64 id)
    {
        auto it = m_List.find(id);
        MASSERT(it != m_List.end());
        if (it != m_List.end())
        {
            MASSERT(it->second != nullptr);
            m_List.erase(it);
        }
    }

    Task::Desc* Task::DescCreator::GetDesc(UINT64 id)
    {
        auto it = m_List.find(id);
        if (it != m_List.end())
        {
            return it->second.get();
        }

        return nullptr;
    }

    void Task::SetDstTaskDesc(Desc* pDesc)
    {
        MASSERT(pDesc != nullptr);
        m_pTaskDescDst = pDesc;
        const MemSegDesc* pMemDesc = m_pTaskDescDst->GetMemDescs()->GetMemSegDesc(0);
        MASSERT(pMemDesc != nullptr);
        DebugPrintf("Task::%s, " vGpuMigrationTag " Print first MemSegDesc ...\n",
            __FUNCTION__);
        pMemDesc->Print();
        m_pTaskDescDst->SetupMemAlloc(m_pAllocDst.get());
    }

    //////////////////////Stream Save/Restore Task//////////////////////

    //////////// Surface //////////////

    void StreamTask::Surf::Print() const
    {
        const MdiagSurf* pSurf = GetSurfConst();
        DebugPrintf("MDiagVm Surf, " vGpuMigrationTag " Print FB Copy surface (%s) ...\n",
            pSurf->GetName().c_str());
        const string prefix = pSurf->GetName() + ": ";     
        const_cast<MdiagSurf*>(pSurf)->GetSurf2D()->Print(Tee::PriDebug, prefix.c_str());
        DebugPrintf("  GPU PA 0x%llx. GPU VA 0x%llx CPU PA 0x%llx CPU VA %p.\n"
            "FB Copy surface (%s) print END.\n"
            ,
            GetAllocGpuPa(),
            GetGpuVa(),
            GetCpuPa(),
            GetCpuVa(),
            pSurf->GetName().c_str()
            );
    }

    void StreamTask::Surf::SetupSurf(const Memory::Location loc)
    {
        const UINT32 pageSize = MemStream::PageSize; // in Bytes
        const UINT32 physPageSize = pageSize >> 10; // in KB

        static UINT32 surfId = 0;
        const string surfName = "FbCopyPrivSurf" + to_string(surfId++);
        MdiagSurf* pSurf = GetSurf();
        pSurf->SetName(surfName);
        pSurf->SetAlignment(pageSize);
        pSurf->SetColorFormat(ColorUtils::Y8);
        pSurf->SetForceSizeAlloc(true);
        pSurf->SetLocation(loc);
        pSurf->SetLayout(Surface2D::Pitch);
        pSurf->SetType(LWOS32_TYPE_TEXTURE);
        pSurf->SetPhysContig(true);
        pSurf->SetPageSize(physPageSize);
        pSurf->SetPhysAddrPageSize(physPageSize);
        pSurf->SetGpuVASpace(GetGpuUtils()->GetChannels()->GetVaSpaceHandle());
    }

    RC StreamTask::Surf::CreateSurf()
    {
        MASSERT(GetFbMemSeg() != nullptr);
        const UINT32 size = UINT32(GetFbMemSeg()->GetSize());
        MASSERT(size > 0);

        RC rc;

        SetupSurf(m_MemLoc);
        MdiagSurf* pSurf = GetSurf();
        pSurf->SetArrayPitch(size);
        DebugPrintf("MDiagVm Surf, %s, " vGpuMigrationTag " Allocating FB Copy surface in locaiton %s VA space 0x%x, mem size 0x%x.\n",
            __FUNCTION__,
            Memory::Fb == m_MemLoc ? "FB" : "Sysmem",
            pSurf->GetGpuVASpace(),
            size);
        LWGpuResource* pLwGpu = GetGpuUtils()->GetChannels()->GetLwGpu();
        MASSERT(pLwGpu != nullptr);

        GpuDevice* pGpuDev = pLwGpu->GetGpuDevice();
        LwRm* pLwRm = GetGpuUtils()->GetChannels()->GetLwRm();
        CHECK_RC(pSurf->Alloc(pGpuDev, pLwRm));
        if (m_bNeedCpuMap)
        {
            CHECK_RC(pSurf->Map(0, pLwRm));
        }
    
        m_FbSeg.pCpuPtr = reinterpret_cast<void*>(GetCpuVa());
        Print();
        DebugPrintf("MDiagVm Surf, %s, " vGpuMigrationTag " FB Copy surface (%s) created.\n",
            __FUNCTION__,
            pSurf->GetName().c_str()
            );

        return rc;
    }

    void StreamTask::Surf::Free()
    {
        MASSERT(GetFbMemSeg() != nullptr);
        DebugPrintf("MDiagVm Surf, %s, " vGpuMigrationTag " Freeing FB Copy surface (%s).\n",
            __FUNCTION__,
            m_Surf.GetName().c_str()
            );
        m_Surf.Free();
    }

    // We can't physically allocate a surface again at the FB mem segment PA if
    // there's already a surface allocated in this segment.
    // We can't allocate the surface in the FB mem range if it's in SRIOV VM segment.
    // For those cases, we need to remap a normally allocated FB Copy surface's VA
    // to the required FB mem segment PA.
    RC StreamTask::Surf::RemapPa()
    {
        MASSERT(GetFbMemSeg() != nullptr);
        FbMemSeg* pFbSeg = GetFbMemSeg();
        const PHYSADDR physAddr = pFbSeg->GetAddr();

        MdiagSurf* pSurf = &m_Surf;
        const UINT64 size = pSurf->GetAllocSize();
        MASSERT(size >= 0);

        LWGpuResource* pLwGpu = GetGpuUtils()->GetChannels()->GetLwGpu();
        MASSERT(pLwGpu != nullptr);

        GpuDevice* pGpuDev = pLwGpu->GetGpuDevice();
        LwRm* pLwRm = GetGpuUtils()->GetChannels()->GetLwRm();

        RC rc;

        DebugPrintf("MDiagVm Surf, %s, " vGpuMigrationTag " Attemp to remap FB Copy surface (%s) to "
            "PA 0x%llx from 0x%llx size 0x%llx, LwRm %p virt handle 0x%x phys handle 0x%x.\n",
            __FUNCTION__,
            pSurf->GetName().c_str(),
            physAddr,
            GetAllocGpuPa(),
            size,
            pLwRm,
            pSurf->GetVirtMemHandle(pLwRm),
            pSurf->GetMemHandle(pLwRm)
            );
        CHECK_RC(GetGpuUtils()->FillPteMemPa(pLwRm,
            pGpuDev,
            GetGpuVa(),
            size,
            pSurf->GetMemHandle(pLwRm),
            physAddr,
            pSurf->GetGpuVASpace()));
        DebugPrintf("MDiagVm Surf, %s, " vGpuMigrationTag " FB Copy surface (%s) remapped.\n",
            __FUNCTION__,
            pSurf->GetName().c_str()
            );

        return rc;
    }

    PHYSADDR StreamTask::Surf::GetAllocGpuPa() const
    {
        MASSERT(GetFbMemSegConst() != nullptr);
        MASSERT(GetFbMemSegConst()->GetSize() == m_Surf.GetAllocSize());
        RC rc;
        PHYSADDR off0Pa = 0;
        rc = m_Surf.GetPhysAddress(0, &off0Pa);
        MASSERT(OK == rc);
        return off0Pa;
    }

    UINT64 StreamTask::Surf::GetGpuVa() const
    {
        MASSERT(GetFbMemSegConst() != nullptr);
        MASSERT(GetFbMemSegConst()->GetSize() == m_Surf.GetAllocSize());
        return m_Surf.GetCtxDmaOffsetGpu();
    }

    void* StreamTask::Surf::GetCpuVa() const
    {
        return m_Surf.GetAddress();
    }

    PHYSADDR StreamTask::Surf::GetCpuPa() const
    {
        return m_Surf.GetPhysAddress();
    }

    RC StreamTask::Surf::Map()
    {
        MASSERT(GetFbMemSeg() != nullptr);
        RC rc;

        CHECK_RC(CreateSurf());
        GetFbMemSeg()->Print();
        DebugPrintf("MDiagVm Surf, %s, " vGpuMigrationTag " FB Copy surface (%s) required PA 0x%llx, allocated PA 0x%llx.\n",
                __FUNCTION__,
                GetSurf()->GetName().c_str(),
                GetFbMemSeg()->GetAddr(),
                GetAllocGpuPa()
                );
        if (!(GetFbMemSeg()->GetAddr() == FB_COPY_ILW_ADDR ||
            GetFbMemSeg()->GetAddr() == GetAllocGpuPa()))
        {
            DebugPrintf("MDiagVm Surf, " vGpuMigrationTag " FB Copy surface need be remapped.\n");
            CHECK_RC(RemapPa());
        }
        else
        {
            DebugPrintf("MDiagVm Surf, " vGpuMigrationTag " FB Copy surface needn't be remapped.\n");
        }

        return rc;
    }

    RC StreamTask::Surf::Unmap()
    {
        RC rc;
        MASSERT(GetFbMemSeg() != nullptr);

        // Mapping back to originally alloccated PA.
        CHECK_RC(m_Surf.ReMapPhysMemory());
        Free();
        return rc;
    }

    //////////// BAR1 allocator //////////////

    RC StreamTask::Bar1Alloc::Map()
    {
        MASSERT(GetFbMemSeg() != nullptr);
        return GetGpuUtils()->MapFbMem(GetFbMemSeg(), this);
    }

    RC StreamTask::Bar1Alloc::Unmap()
    {
        MASSERT(GetFbMemSeg() != nullptr);
        return GetGpuUtils()->UnmapFbMem(GetFbMemSeg(), this);
    }

    StreamTask::StreamTask()
        : m_HexFileNumData(FbCopyLine::NumData)
    {
        SetDstMemAlloc(new Bar1Alloc);
    }

    void StreamTask::SetAllocType(const AllocType type)
    {
        if (AllocSurf == type)
        {
            SetDstMemAlloc(new Surf);
        }
    }

    RC StreamTask::StartReadFile(const string& file)
    {
        return m_fh.Open(file.c_str(), "r");
    }

    RC StreamTask::StartWriteFile(const string& file)
    {
        return m_fh.Open(file.c_str(), "w");
    }

    RC StreamTask::VerifySrcSize()
    {
        if (!GetMemDescsRef().VerifySrcSize(m_SrcMemDescs))
        {
            return RC::SOFTWARE_ERROR;
        }

        return OK;
    }

    RC StreamTask::SaveMemToFile(const string& file)
    {
        MASSERT(GetDstTaskDesc() != nullptr);
        RC rc;

        CHECK_RC(StartWriteFile(file));
        StreamHexFile* pCli = HexFileStreamCreator::CreateHexFileStream(GetHexFileNumData());
        pCli->SetFile(m_fh.GetFile());
        m_Stream.SetStreamClient(pCli);
        m_Stream.SetMemAlloc(GetDstMemAlloc());
        
        if (GetDstTaskDesc()->HasMeta())
        {
            MemMeta meta;
            GetMemDescsRefConst().GetMeta(&meta);
            CHECK_RC(m_Stream.SaveMeta(meta));
        }
        CHECK_RC(Prepare());
        CHECK_RC(m_Stream.SaveData(GetMemDescsRef()));

        return rc;
    }

    RC StreamTask::RestoreMemFromFile(const string& file)
    {
        MASSERT(GetDstTaskDesc() != nullptr);
        RC rc;

        CHECK_RC(StartReadFile(file));
        StreamHexFile* pCli = HexFileStreamCreator::CreateHexFileStream(GetHexFileNumData());
        pCli->SetFile(m_fh.GetFile());
        m_Stream.SetStreamClient(pCli);
        m_Stream.SetMemAlloc(GetDstMemAlloc());
        if (GetDstTaskDesc()->HasMeta())
        {
            MemMeta meta;
            CHECK_RC(m_Stream.LoadMeta(&meta));
            m_SrcMemDescs.ImportFromMeta(meta);
        }
        CHECK_RC(Prepare());
        CHECK_RC(m_Stream.LoadData(GetMemDescsRef()));

        return rc;
    }

    RC StreamTask::ReadMem(MemData* pOut)
    {
        MASSERT(GetDstTaskDesc() != nullptr);
        RC rc;

        StreamLocal* pCli = new StreamLocal;
        const MemSegDesc* pSeg = GetMemDescsRef().GetMemSegDesc(0);
        MASSERT(pSeg != nullptr);
        DebugPrintf("%s, " vGpuMigrationTag " taskDesc %p memDesc.addr 0x%llx size 0x%llx hasLocalData %s.\n",
            __FUNCTION__,
            GetDstTaskDesc(),
            pSeg->GetAddr(),
            pSeg->GetSize(),
            GetDstTaskDesc()->HasLocalMemData() ? "Yes" : "No"
            );
        MASSERT(pSeg->GetSize() > 0);
        m_Stream.SetStreamClient(pCli);
        m_Stream.SetMemAlloc(GetDstMemAlloc());
        CHECK_RC(Prepare());
        CHECK_RC(m_Stream.SaveData(GetMemDescsRef()));
        pCli->GetStreamData(pOut);
        if (GetDstTaskDesc()->HasLocalMemData())
        {
            GetDstTaskDesc()->SetMemData(*pOut);
        }

        return rc;
    }

    RC StreamTask::WriteMem(const MemData& data)
    {
        MASSERT(GetDstTaskDesc() != nullptr);
        RC rc;

        StreamLocal* pCli = new StreamLocal;
        const MemSegDesc* pSeg = GetMemDescsRef().GetMemSegDesc(0);
        MASSERT(pSeg != nullptr);
        DebugPrintf("%s, " vGpuMigrationTag " taskDesc %p in.size 0x%llx memDesc.addr 0x%llx size 0x%llx hasLocalData %s.\n",
            __FUNCTION__,
            GetDstTaskDesc(),
            data.size(),
            pSeg->GetAddr(),
            pSeg->GetSize(),
            GetDstTaskDesc()->HasLocalMemData() ? "Yes" : "No"
            );
        MASSERT(pSeg->GetSize() > 0);
        MASSERT(pSeg->GetSize() == data.size());
        pCli->SetStreamData(data);
        if (GetDstTaskDesc()->HasLocalMemData())
        {
            GetDstTaskDesc()->SetMemData(data);
        }
        m_Stream.SetStreamClient(pCli);
        m_Stream.SetMemAlloc(GetDstMemAlloc());
        CHECK_RC(Prepare());
        CHECK_RC(m_Stream.LoadData(GetMemDescsRef()));

        return rc;
    }

    RC StreamTask::WriteSavedMem()
    {
        MASSERT(GetDstTaskDesc() != nullptr);
        RC rc;

        StreamLocal* pCli = new StreamLocal;
        const MemSegDesc* pSeg = GetMemDescsRef().GetMemSegDesc(0);
        MASSERT(pSeg != nullptr);
        DebugPrintf("%s, " vGpuMigrationTag " taskDesc %p memDesc.addr 0x%llx size 0x%llx hasLocalData %s.\n",
            __FUNCTION__,
            GetDstTaskDesc(),
            pSeg->GetAddr(),
            pSeg->GetSize(),
            GetDstTaskDesc()->HasLocalMemData() ? "Yes" : "No"
            );
        MASSERT(pSeg->GetSize() > 0);
        MASSERT(GetDstTaskDesc()->HasLocalMemData());
        MASSERT(GetDstTaskDesc()->GetMemData() != nullptr);
        pCli->SetStreamData(*GetDstTaskDesc()->GetMemData());
        m_Stream.SetStreamClient(pCli);
        m_Stream.SetMemAlloc(GetDstMemAlloc());
        CHECK_RC(Prepare());
        CHECK_RC(m_Stream.LoadData(GetMemDescsRef()));

        return rc;
    }

    RC StreamTask::Prepare()
    {
        MASSERT(GetDstTaskDesc() != nullptr);
        RC rc;

        CHECK_RC(VerifySrcSize());
        AddSrcMemDescs();
        GetMDiagvGpuMigration()->ConfigFbCopyProgressPrintGran(GetMemDescsRefConst().GetTotalSize());
        size_t bar1Size = 0;
        CHECK_RC(GetGpuUtils()->QueryBar1MapSize(&bar1Size));
        GetMemDescsRef().Split(bar1Size);
        m_Stream.SetMemStreamBandwidth(size_t(GetMemDescsRef().GetMinSegSize()));
        m_Stream.SetClientBandwidth(size_t(GetMemDescsRef().GetMinSegSize()));

        return rc;
    }

    RC CopyTask::DmaSurf::DmaCopy(DmaSurf* pDst)
    {
        MASSERT(GetFbMemSeg() != nullptr);
        MASSERT(pDst != nullptr);
        MASSERT(pDst->GetFbMemSeg() != nullptr);
        MASSERT(GetFbMemSeg()->GetSize() == GetCopySize());

        RC rc;
        const UINT64 copySize = GetCopySize();
        MASSERT(copySize >= MemStream::PageSize);
        CHECK_RC(GetGpuUtils()->GetChannels()->StartDmaChannel());
        DmaReader* pDmaReader = GetGpuUtils()->GetChannels()->GetDmaReader();
        MASSERT(pDmaReader != nullptr);

        UINT64 SrcOffset = GetGpuVa();
        UINT64 DstOffset = pDst->GetGpuVa();
        MdiagSurf* pSurfSrc = GetSurf();
        MdiagSurf* pSurfDst = pDst->GetSurf();
        pDmaReader->BindSurface(pSurfSrc->GetCtxDmaHandleGpu());

        const UINT32 chunkSize = FbCopyLine::DmaCopyChunkSize;
        UINT32 lineCount = static_cast<UINT32>(copySize / chunkSize);
        if (!lineCount || (copySize % chunkSize))
        {
            // copySize < chunkSize or not a multiple of chunkSize
            lineCount ++;
        }

        MASSERT(chunkSize);
        DebugPrintf("MDiagVm Surf, %s, " vGpuMigrationTag " Start FB mem DMA copy from source surface (%s) VA 0x%llx "
            "to dest surface (%s) VA 0x%llx size 0x%llx, lineCount 0x%x, lineLength 0x%x(%uM) each copy granularity.\n",
            __FUNCTION__,
            pSurfSrc->GetName().c_str(),
            SrcOffset,
            pSurfDst->GetName().c_str(),
            DstOffset,
            copySize,
            lineCount,
            chunkSize,
            chunkSize >> 20
            );

        UINT64 writeSize = copySize;

        UINT32 lineIndex = 0;
        while (writeSize > 0)
        {
            DebugPrintf("MDiagVm Surf, %s, " vGpuMigrationTag " DMA copy line# %u, lineCount %u, source surf (%s) start VA 0x%llx dest surf (%s) start VA 0x%llx.\n",
                __FUNCTION__,
                lineIndex ++,
                lineCount,
                pSurfSrc->GetName().c_str(),
                GetGpuVa(),
                pSurfDst->GetName().c_str(),
                pDst->GetGpuVa()
                );
            if (writeSize > chunkSize)
            {
                // ReadLine does not support multiple lines each time.
                CHECK_RC(pDmaReader->ReadLine(pSurfSrc->GetCtxDmaHandle(),
                        SrcOffset, 0,
                        1, chunkSize, 0, pSurfSrc->GetSurf2D(), pSurfDst, DstOffset));
                writeSize -= chunkSize;
                SrcOffset += chunkSize;
                DstOffset += chunkSize;
            }
            else
            {
                CHECK_RC(pDmaReader->ReadLine(pSurfSrc->GetCtxDmaHandle(),
                        SrcOffset, 0,
                        1, UINT32(writeSize), 0, pSurfSrc->GetSurf2D(), pSurfDst, DstOffset));
                writeSize = 0;
            }
        }
        (void)lineIndex;
        DebugPrintf("MDiagVm Surf, %s, " vGpuMigrationTag " Finished DMA copy.\n",
            __FUNCTION__);
        CHECK_RC(GetGpuUtils()->GetChannels()->StopDmaChannel());

        return OK;
    }

    CopyTask::CopyTask()
    {
        SetDstMemAlloc(DmaSurf::Create());
        SetSrcMemAlloc(DmaSurf::Create());
    }

    void CopyTask::SetSrcTaskDesc(Desc* pDesc)
    {
        MASSERT(pDesc != nullptr);
        m_pTaskDescSrc = pDesc;
        const MemSegDesc* pMemDesc = m_pTaskDescSrc->GetMemDescs()->GetMemSegDesc(0);
        MASSERT(pMemDesc != nullptr);
        DebugPrintf("CopyTask::%s, " vGpuMigrationTag " Print first MemSegDesc ...\n",
            __FUNCTION__);
        pMemDesc->Print();
        m_pTaskDescSrc->SetupMemAlloc(m_pAllocSrc.get());
    }

    void CopyTask::SaveSurf()
    {
        MemAlloc* pAlloc = ReleaseDstMemAlloc();
        DmaSurf* pSurf = dynamic_cast<DmaSurf*>(pAlloc);
        MASSERT(pSurf != nullptr);
        m_pSurfList->push_back(pSurf);
        SetDstMemAlloc(DmaSurf::Create());
        DebugPrintf("CopyTask::%s, " vGpuMigrationTag " Saved dest surface (%s) to Surf list.\n",
            __FUNCTION__,
            pSurf->GetSurf()->GetName().c_str()
            );
        pSurf->Print();
    }

    void CopyTask::LoadSurf()
    {
        auto it = m_pSurfList->begin();
        MASSERT(it != m_pSurfList->end());
        DmaSurf* pSurf = dynamic_cast<DmaSurf*>(*it);
        MASSERT(pSurf != nullptr);
        m_pSurfList->erase(it);
        SetSrcMemAlloc(pSurf);
        DebugPrintf("CopyTask::%s, " vGpuMigrationTag " Loaded surface (%s) from Surf list and set it as source surface.\n",
            __FUNCTION__,
            pSurf->GetSurf()->GetName().c_str()
            );
        pSurf->Print();
    }

    RC CopyTask::Copy()
    {
        RC rc;

        MASSERT(GetSrcTaskDesc() != nullptr);
        MASSERT(GetDstTaskDesc() != nullptr);

        const MemDescs& descSrc = GetSrcTaskDesc()->GetMemDescsRefConst();
        const MemDescs& descDst = GetDstTaskDesc()->GetMemDescsRefConst();

        MemDescs::DescList listSrc;
        descSrc.GetDescList(&listSrc);
        MemDescs::DescList listDst;
        descDst.GetDescList(&listDst);
        if (!listSrc.empty())
        {
            CHECK_RC(GetGpuUtils()->GetChannels()->CreateDmaReader());
        }
        auto itSrc = listSrc.begin();
        auto itDst = listDst.begin();
        for (; itSrc != listSrc.end(); itSrc++, itDst++)
        {
            const auto segSrc = *itSrc;
            const auto segDst = *itDst;
            MASSERT(segSrc.GetSize() >= FbCopyLine::NumDataBytes);

            // Src
            FbMemSeg fbSegSrc(segSrc);
            GetSrcMemAlloc()->SetFbMemSeg(&fbSegSrc);
            // Dst
            FbMemSeg fbSegDst(segDst);
            GetDstMemAlloc()->SetFbMemSeg(&fbSegDst);

            // Src
            GetSrcMemAlloc()->SetKind(LW_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE);
            CHECK_RC(GetSrcMemAlloc()->Map());
            DebugPrintf("DmaCopyFb, " vGpuMigrationTag " Created source FB mem.\n");

            // Dst
            GetDstMemAlloc()->SetKind(LW_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE_DISABLE_PLC);  
            CHECK_RC(GetDstMemAlloc()->Map());
            DebugPrintf("DmaCopyFb, " vGpuMigrationTag " Created dest FB mem.\n");

            CHECK_RC(GetSrcSurf()->DmaCopy(GetDstSurf()));

            GetDstMemAlloc()->Unmap();
            GetSrcMemAlloc()->Unmap();
        }
        if (!listSrc.empty())
        {
            GetGpuUtils()->GetChannels()->FreeDmaReader();
        }

        return rc;
    }

    RC CopyTask::Save(bool bSysmem)
    {
        RC rc;

        MASSERT(GetSrcTaskDesc() != nullptr);

        const MemDescs& descSrc = GetSrcTaskDesc()->GetMemDescsRefConst();

        MemDescs::DescList listSrc;
        descSrc.GetDescList(&listSrc);
        if (!listSrc.empty())
        {
            CHECK_RC(GetGpuUtils()->GetChannels()->CreateDmaReader());
        }
        for (const auto segSrc : listSrc)
        {
            MASSERT(segSrc.GetSize() >= FbCopyLine::NumDataBytes);

            // Src
            FbMemSeg fbSegSrc(segSrc);
            GetSrcMemAlloc()->SetFbMemSeg(&fbSegSrc);
            // Dst
            MemSegDesc segDst(FB_COPY_ILW_ADDR, segSrc.GetSize());
            FbMemSeg fbSegDst(segDst);
            GetDstMemAlloc()->SetFbMemSeg(&fbSegDst);
            if (bSysmem)
            {
                SetDstMemLocation(Memory::NonCoherent);
            }

            // Src
            GetSrcMemAlloc()->SetKind(LW_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE);
            CHECK_RC(GetSrcMemAlloc()->Map());
            DebugPrintf("DmaCopyFb, " vGpuMigrationTag " Created source FB mem.\n");

            // Dst
            CHECK_RC(GetDstMemAlloc()->Map());
            DebugPrintf("DmaCopyFb, " vGpuMigrationTag " Created dest FB mem.\n");

            CHECK_RC(GetSrcSurf()->DmaCopy(GetDstSurf()));

            GetSrcMemAlloc()->Unmap();
            SaveSurf();
        }

        return rc;
    }

    RC CopyTask::Restore()
    {
        RC rc;

        MASSERT(GetDstTaskDesc() != nullptr);

        const MemDescs& descDst = GetDstTaskDesc()->GetMemDescsRefConst();

        MemDescs::DescList listDst;
        descDst.GetDescList(&listDst);
        for (const auto segDst : listDst)
        {
            MASSERT(segDst.GetSize() >= FbCopyLine::NumDataBytes);

            // Src
            LoadSurf();
            // Dst
            FbMemSeg fbSegDst(segDst);
            GetDstMemAlloc()->SetFbMemSeg(&fbSegDst);

            // Dst
            GetDstMemAlloc()->SetKind(LW_MMU_PTE_KIND_GENERIC_MEMORY_COMPRESSIBLE_DISABLE_PLC);  
            CHECK_RC(GetDstMemAlloc()->Map());
            DebugPrintf("DmaCopyFb, " vGpuMigrationTag " Created dest FB mem.\n");

            CHECK_RC(GetSrcSurf()->DmaCopy(GetDstSurf()));

            GetDstMemAlloc()->Unmap();
            GetSrcMemAlloc()->Unmap();
        }
        if (!listDst.empty())
        {
            GetGpuUtils()->GetChannels()->FreeDmaReader();
        }

        return rc;
    }

    bool DataVerif::Compare(const void* pIn, const void* pMem) const
    {
        MASSERT(m_Seg.GetSize() >= 8);

        const UINT64* pIn64 = static_cast<const UINT64*>(pIn);
        const UINT64* pMem64 = static_cast<const UINT64*>(pMem);
        DebugPrintf("DataVerif, " vGpuMigrationTag ", Compare 1st 8-byte left data 0x%016llx"
            " to right data 0x%016llx.\n",
            *pIn64,
            *pMem64);
        const UINT64* pEnd = pIn64 + m_Seg.GetSize() / 8;
        UINT64 lwrOff = m_Seg.GetAddr();
        size_t count = 0;
        while (pIn64 < pEnd)
        {
            if (*pIn64 != *pMem64)
            {
                ErrPrintf("DataVerif, " vGpuMigrationTag ", Failed to verify, 8-bytes @ paOff 0x%llx"
                    " expect 0x%016llx read back 0x%016llx mismatching.\n",
                    lwrOff,
                    *pIn64,
                    *pMem64);
                count ++;
                if (count >= 2)
                {
                    break;
                }
            }
            pIn64 ++;
            pMem64 ++;
            lwrOff += 8;
        }

        if (0 == count)
        {
            DebugPrintf("DataVerif, " vGpuMigrationTag ", Verif passed paOff 0x%llx len 0x%llx all matched.\n",
                m_Seg.GetAddr(),
                m_Seg.GetSize());
        }

        return 0 == count;
    }

    //////////////////////SRIOV Resources/////////////////////////////////////////

    RC SriovResources::TaskDesc::Setup(UINT32 gfid)
    {
        RC rc;

        SriovResources* pSriov = dynamic_cast<SriovResources*>(GetCreator());
        MASSERT(pSriov != nullptr);

        UINT64 fbSize = 0;
        QueryFbMemSize(&fbSize);
        CHECK_RC(pSriov->InitVmmuSegs(gfid));
        MASSERT(GetId() == gfid);
        MASSERT(GetMemDescs()->GetCount() == 0);
        SetMemDescs(*pSriov->GetVmmuSegs(gfid));
        MASSERT(GetMemDescs()->GetMemSegDesc(0)->GetSize() > 0 && GetMemDescs()->GetTotalSize() < fbSize);

        return rc;
    }

    RC SriovResources::InitVmmuSegs(UINT32 gfid)
    {
        auto it = m_VmmuSegs.find(gfid);
        if (it != m_VmmuSegs.end())
        {
            return OK;
        }

        MemDescs mem;

        VmiopMdiagElwManager * pElwMgr = VmiopMdiagElwManager::Instance();
        vector<UINT64> vmmuSetting;
        GetVfVmmuSetting(gfid, &vmmuSetting);
        const UINT64 segSize = pElwMgr->GetVmmuSegmentSize();
        DebugPrintf("%s, " vGpuMigrationTag " Use VMMU segment size 0x%llx(%lluM).\n",
            __FUNCTION__,
            segSize,
            segSize >> 20);
            
        for (const auto& vmmuMask : vmmuSetting)
        {
            for (PHYSADDR i = 0; i < 64; i++)
            {
                if ((vmmuMask >> i) & 0x1ULL)
                {
                    mem.Add((i * segSize), segSize);
                }
            }
        }
        MASSERT(mem.GetCount() > 0);
        m_VmmuSegs[gfid] = mem;

        return OK;
    }

    void SriovResources::GetVfVmmuSetting(UINT32 gfid, vector<UINT64>* pSetting) const
    {
        VmiopMdiagElwManager * pElwMgr = VmiopMdiagElwManager::Instance();
        vector<VmiopMdiagElwManager::VFSetting*> vfSettings = pElwMgr->GetVFSetting(gfid);
        const auto& it = vfSettings.begin();
        if (it != vfSettings.end())
        {
            *pSetting = (*it)->vmmuSetting;
        }
    }

    const MemDescs* SriovResources::GetVmmuSegs(UINT32 gfid) const
    {
        const auto it = m_VmmuSegs.find(gfid);
        if (it != m_VmmuSegs.end())
        {
            return &it->second;
        }

        return nullptr;
    }

    UINT32 SriovResources::GetSwizzId(UINT32 gfid) const
    {
        VmiopMdiagElwManager * pElwMgr = VmiopMdiagElwManager::Instance();
        vector<VmiopMdiagElwManager::VFSetting*> vfSettings = pElwMgr->GetVFSetting(gfid);
        const auto& it = vfSettings.begin();
        if (it != vfSettings.end())
        {
            return (*it)->swizzid;
        }

        return 0;
    }

    //////////////////////SMC Resources/////////////////////////////////////////
    
    RC SmcResources::TaskDesc::Setup(UINT32 syspipe)
    {
        RC rc;

        SmcResources* pSmcRes = dynamic_cast<SmcResources*>(GetCreator());
        MASSERT(pSmcRes != nullptr);

        UINT64 fbSize = 0;
        QueryFbMemSize(&fbSize);
        MASSERT(GetId() == syspipe);
        LwRm* pLwRm = pSmcRes->GetEngineLwRmPtr(syspipe);
        GpuSubdevice* pSubdev = pSmcRes->GetGpuSubdevice(syspipe);
        MemSegDesc seg;
        CHECK_RC(pSmcRes->GetPartitionInfo(pLwRm, pSubdev, &seg));
        MASSERT(GetMemDescs()->GetCount() == 0);
        MASSERT(seg.GetSize() > 0 && seg.GetSize() < fbSize);
        GetMemDescsRef().Add(seg);

        return rc;
    }

    RC SmcResources::TaskDesc::SetupSriovSmc(UINT32 gfid)
    {
        GpuPartition* pGpuPartition = GetMDiagVmResources()->GetGpuPartitionByGfid(gfid);
        if (pGpuPartition != nullptr)
        {
            RC rc;

            SmcResources* pSmcRes = dynamic_cast<SmcResources*>(GetCreator());
            MASSERT(pSmcRes != nullptr);

            UINT64 fbSize = 0;
            QueryFbMemSize(&fbSize);
            LwRm* pLwRm = pGpuPartition->GetLWGpuResource()->GetLwRmPtr(pGpuPartition);
            GpuSubdevice* pSubdev = pGpuPartition->GetLWGpuResource()->GetGpuSubdevice();
            MemSegDesc seg;
            CHECK_RC(pSmcRes->GetPartitionInfo(pLwRm, pSubdev, &seg));
            MASSERT(GetMemDescs()->GetCount() == 0);
            MASSERT(seg.GetSize() > 0 && seg.GetSize() < fbSize);
            GetMemDescsRef().Add(seg);

            return rc;
        }

        return RC::ILWALID_ARGUMENT;
    }

    void SmcResources::AddSmcEngine(SmcEngine* pSmcEngine)
    {
        MASSERT(pSmcEngine != nullptr);
        GpuPartition* pGpuPartition = pSmcEngine->GetGpuPartition();
        DebugPrintf("%s, " vGpuMigrationTag " Add SmcEngine %s syspipe 0x%x in paritition %s.\n",
            __FUNCTION__,
            pSmcEngine->GetName().c_str(),
            pSmcEngine->GetSysPipe(),
            pGpuPartition->GetName().c_str());
        MASSERT(pGpuPartition != nullptr);
        auto it = m_SmcEngines.find(pSmcEngine->GetSysPipe());
        if (it == m_SmcEngines.end())
        {
            m_SmcEngines[pSmcEngine->GetSysPipe()] = pSmcEngine;
        }
        MASSERT(m_SmcEngines.find(pSmcEngine->GetSysPipe()) != m_SmcEngines.end());
    }

    SmcEngine* SmcResources::GetDefaultSmcEngine() const
    {
        auto it = m_SmcEngines.begin();
        if (it != m_SmcEngines.end())
        {
            return it->second;
        }
        return nullptr;
    }

    LWGpuResource* SmcResources::GetDefaultLwGpu() const
    {
        SmcEngine* pSmcEngine = GetDefaultSmcEngine();
        if (pSmcEngine != nullptr)
        {
            GpuPartition* pGpuPartition = pSmcEngine->GetGpuPartition();
            MASSERT(pGpuPartition != nullptr);

            return pGpuPartition->GetLWGpuResource();
        }

        return nullptr;
    }
    
    SmcResourceController* SmcResources::GetDefaultSmcResCtrl() const
    {
        LWGpuResource* pLwGpu = GetDefaultLwGpu();
        if (pLwGpu != nullptr)
        {
            return pLwGpu->GetSmcResourceController();
        }
        return nullptr;
    }

    LWGpuChannel* SmcResources::CreateDefaultChannel() const
    {
        SmcEngine* pSmcEngine = GetDefaultSmcEngine();
        if (pSmcEngine != nullptr)
        {
            GpuPartition* pGpuPartition = pSmcEngine->GetGpuPartition();
            MASSERT(pGpuPartition != nullptr);
            LWGpuResource* pLwGpu = pGpuPartition->GetLWGpuResource();
            LwRm* pLwRm = pLwGpu->GetLwRmPtr(pSmcEngine);
            if (pLwRm != nullptr && pLwGpu != nullptr)
            {
                return pLwGpu->CreateChannel(pLwRm, pSmcEngine);
            }
        }

        return nullptr;
    }

    SmcEngine* SmcResources::GetSmcEngine(UINT32 syspipe) const
    {
        DebugPrintf("%s, " vGpuMigrationTag " Finding SmcEngine syspipe 0x%x.\n",
            __FUNCTION__,
            syspipe);
        PrintEngines();
        const auto it = m_SmcEngines.find(syspipe);
        if (it != m_SmcEngines.end())
        {
            return it->second;
        }
        return nullptr;
    }

    GpuSubdevice* SmcResources::GetGpuSubdevice(UINT32 syspipe) const
    {
        const SmcEngine* pSmcEngine = GetSmcEngine(syspipe);
        MASSERT(pSmcEngine != nullptr);
        GpuPartition* pGpuPartition = pSmcEngine->GetGpuPartition();
        MASSERT(pGpuPartition != nullptr);
        return pGpuPartition->GetLWGpuResource()->GetGpuSubdevice();
    }

    LwRm* SmcResources::GetEngineLwRmPtr(UINT32 syspipe) const
    {
        SmcEngine* pSmcEngine = GetSmcEngine(syspipe);
        MASSERT(pSmcEngine != nullptr);
        GpuPartition* pGpuPartition = pSmcEngine->GetGpuPartition();
        MASSERT(pGpuPartition != nullptr);
        return pGpuPartition->GetLWGpuResource()->GetLwRmPtr(pSmcEngine);
    }

    void SmcResources::AddGpuPartition(GpuPartition* pGpuPartition)
    {
        MASSERT(pGpuPartition != nullptr);
        DebugPrintf("%s, " vGpuMigrationTag " Add SMC partition %s.\n",
            __FUNCTION__,
            pGpuPartition->GetName().c_str());
        m_GpuPartitions.push_back(pGpuPartition);
    }

    void SmcResources::GetGpuPartitions(vector<GpuPartition*>* pGpuPartitions) const
    {
        DebugPrintf("%s, " vGpuMigrationTag " Count of GpuParitions %d.\n",
            __FUNCTION__,
            m_GpuPartitions.size());
        *pGpuPartitions = m_GpuPartitions;
    }

    GpuPartition* SmcResources::GetGpuPartition(UINT32 swizzId) const
    {
        SmcResourceController* pSmcResCtrl = GetDefaultSmcResCtrl();
        if (pSmcResCtrl != nullptr)
        {
            return pSmcResCtrl->GetGpuPartition(swizzId);
        }
        return nullptr;
    }

    RC SmcResources::GetPartitionInfo(LwRm* pLwRm, GpuSubdevice* pSubdev, MemSegDesc* pSeg) const
    {
        MASSERT(pLwRm != nullptr);
        MASSERT(pSubdev != nullptr);
        MASSERT(pSeg != nullptr);

        RC rc;

        LW2080_CTRL_FB_INFO info = {0};
        info.index = LW2080_CTRL_FB_INFO_INDEX_HEAP_START;
        LW2080_CTRL_FB_GET_INFO_PARAMS params = { 1, LW_PTR_TO_LwP64(&info) };
        CHECK_RC(pLwRm->ControlBySubdevice(
                     pSubdev,
                     LW2080_CTRL_CMD_FB_GET_INFO,
                     &params, sizeof(params)));
        pSeg->SetAddr(static_cast<PHYSADDR>(info.data) << 10);
        info.index = LW2080_CTRL_FB_INFO_INDEX_HEAP_SIZE;
        info.data = 0;
        CHECK_RC(pLwRm->ControlBySubdevice(
                     pSubdev,
                     LW2080_CTRL_CMD_FB_GET_INFO,
                     &params, sizeof(params)));
        pSeg->SetSize(static_cast<UINT64>(info.data) << 10);
        DebugPrintf("%s, " vGpuMigrationTag " lwrm %p Partition start 0x%llx size 0x%llx(%lluM).\n",
            __FUNCTION__,
            pLwRm,
            pSeg->GetAddr(),
            pSeg->GetSize(),
            pSeg->GetSize() >> 20
            );

        return OK;
    }

    RC SmcResources::GetAllPartitionInfo(MemDescs* pMem) const
    {
        RC rc;

        DebugPrintf("%s, " vGpuMigrationTag " Count of GpuParitions %d.\n",
            __FUNCTION__,
            m_GpuPartitions.size());
        for (const auto& pGpuPartition : m_GpuPartitions)
        {
            GpuSubdevice* pSubdev = pGpuPartition->GetLWGpuResource()->GetGpuSubdevice();
            LwRm* pLwRm = pGpuPartition->GetLWGpuResource()->GetLwRmPtr(pGpuPartition);
            if (!pSubdev || !pLwRm)
            {
                InfoPrintf(vGpuMigrationTag " Resources not avaiable for partition %s swizzId %u."
                    " Skip it.\n",
                    __FUNCTION__,
                    pGpuPartition->GetName().c_str(),
                    pGpuPartition->GetSwizzId());
                continue;
            }
            MemSegDesc seg;
            CHECK_RC(GetPartitionInfo(pLwRm, pSubdev, &seg));
            pMem->Add(seg);
            DebugPrintf("%s, " vGpuMigrationTag " SMC partition %s swizzId %u FB offset 0x%llx size 0x%llx.\n",
                __FUNCTION__,
                pGpuPartition->GetName().c_str(),
                pGpuPartition->GetSwizzId(),
                seg.GetAddr(),
                seg.GetSize());
        }
        return rc;
    }

    void SmcResources::PrintEngines() const
    {
        DebugPrintf("%s, " vGpuMigrationTag " Count of SmcEngines %d.\n",
            __FUNCTION__,
            m_SmcEngines.size());
        for(const auto& it : m_SmcEngines)
        {
            const auto pSmcEngine = it.second;
            MASSERT(pSmcEngine != nullptr);
            DebugPrintf("%s, " vGpuMigrationTag " SmcEngine %s syspipe 0x%x partition %s.\n",
                __FUNCTION__,
                pSmcEngine->GetName().c_str(),
                pSmcEngine->GetSysPipe(),
                pSmcEngine->GetGpuPartition()->GetName().c_str());
        }
    }

    ////////////////////Runlists/////////////////////////////////////////////

    void Runlists::Resources::GetFbInfo(MemSegDesc* pSeg) const
    {
        MASSERT(GetLwGpu() != nullptr);
        MASSERT(GetChLwRm() != nullptr);
        MASSERT(pSeg != nullptr);

        UINT32 baseLo = RegRead32(MODS_RUNLIST_SUBMIT_BASE_LO);
        UINT32 baseHi = RegRead32(MODS_RUNLIST_SUBMIT_BASE_HI);
        DebugPrintf("%s, " vGpuMigrationTag " Runlist engId 0x%x baseLo 0x%x baseHi 0x%x.\n",
            __FUNCTION__,
            m_EngId,
            baseLo,
            baseHi);
        UINT64 base = ((UINT64)DRF_VAL(_RUNLIST, _SUBMIT_BASE_HI, _PTR_HI, baseHi)
            << DRF_SIZE(LW_RUNLIST_SUBMIT_BASE_LO_PTR_LO))
            | (UINT64)DRF_VAL(_RUNLIST, _SUBMIT_BASE_LO, _PTR_LO, baseLo);
        base <<= LW_RUNLIST_SUBMIT_BASE_LO_PTR_ALIGN_SHIFT;
        MASSERT(base != 0);
        DebugPrintf("%s, " vGpuMigrationTag " Runlist base 0x%llx.\n",
            __FUNCTION__,
            base);
        UINT32 submit = RegRead32(MODS_RUNLIST_SUBMIT);
        DebugPrintf("%s, " vGpuMigrationTag " Runlist submit 0x%x.\n",
            __FUNCTION__,
            submit);
        MASSERT(submit != 0);
        UINT32 len = DRF_VAL(_RUNLIST, _SUBMIT, _LENGTH, submit);
        DebugPrintf("%s, " vGpuMigrationTag " Runlist length 0x%x.\n",
            __FUNCTION__,
            len);

        MASSERT(len > 0);
        len *= 16;

        pSeg->Init(base, len);
        DebugPrintf("%s, " vGpuMigrationTag " Runlists FB mem 0x%llx len 0x%x for device engine ID 0x%x.\n",
            __FUNCTION__,
            pSeg->GetAddr(),
            pSeg->GetSize(),
            GetEngineId());
    }

    void Runlists::TaskDesc::SetupMemAlloc(MemAlloc* pAlloc) const
    {
        MASSERT(pAlloc!= nullptr);
        pAlloc->SetAlignment(0);
        pAlloc->SetKind(LW_MMU_PTE_KIND_GENERIC_MEMORY);
        pAlloc->SetNeedKind(false);
    }

    void Runlists::TaskDesc::Setup(const Runlists::Resources& res)
    {
        UINT64 fbSize = 0;
        QueryFbMemSize(&fbSize);
        MemSegDesc seg;
        res.GetFbInfo(&seg);
        MASSERT(GetId() == res.GetEngineId());
        MASSERT(GetMemDescs()->GetCount() == 0);
        MASSERT(seg.GetSize() > 0 && seg.GetSize() < fbSize);
        GetMemDescsRef().Add(seg);
    }

    // Shorten access for SriovResources
    SriovResources* GetSriovResources()
    {
        return GetMDiagVmResources()->GetSriovResources();
    }

    // Shorten access for SmcResources
    SmcResources* GetSmcResources()
    {
        return GetMDiagVmResources()->GetSmcResources();
    }

    // Shorten access for Runlists
    Runlists* GetRunlists()
    {
        return GetMDiagVmResources()->GetRunlists();
    }

} // MDiagVmScope

//////////////////MDiagVmResources/////////////////////

void MDiagVmResources::TaskDesc::SetupWholeFb()
{
    UINT64 fbSize = 0;
    QueryFbMemSize(&fbSize);
    MASSERT(GetMemDescs()->GetCount() == 0);
    MASSERT(GetId() == 0);
    GetMemDescsRef().Add(0, fbSize);
    SetHasMeta(true);
    SetUseFile(true);
}

void MDiagVmResources::TaskDesc::SetupSingleSeg(const MDiagVmScope::MemSegDesc& seg)
{
    UINT64 fbSize = 0;
    QueryFbMemSize(&fbSize);
    DebugPrintf("%s, " vGpuMigrationTag " To read/write FB mem segment PA offset"
        " 0x%llx(%lldM) size 0x%llx(%lldM).\n",
        __FUNCTION__,
        seg.GetAddr(),
        seg.GetAddr() >> 20,
        seg.GetSize(),
        seg.GetSize() >> 20);
    MASSERT(seg.GetSize() > 0 && seg.GetSize() <= fbSize);
    MASSERT(GetId() == seg.GetAddr());
    MASSERT(GetMemDescs()->GetCount() == 0);
    GetMemDescsRef().Add(seg);
    SetHasMeta(true);
    SetUseFile(true);
}

void MDiagVmResources::TaskDesc::SetupSegRW(const MDiagVmScope::MemSegDesc& seg)
{
    SetupSingleSeg(seg);
    SetHasMeta(false);
    SetUseFile(false);
}

MDiagVmScope::Task::Desc* MDiagVmResources::CreateTaskDescByGfid(UINT32 gfid)
{
    if (GetGpuUtils()->GetLwGpu()->IsSMCEnabled())
    {
        DebugPrintf("MDiagVmResources::%s, " vGpuMigrationTag " SMC enabled, using GFID 0x%x to create SriovSmc task desc.\n",
            __FUNCTION__,
            gfid);
        return GetSmcResources()->CreateTaskDescSriovSmc(gfid);
    }
    DebugPrintf("MDiagVmResources::%s, " vGpuMigrationTag " SMC not enabled, using GFID 0x%x to create SRIOV task desc.\n",
        __FUNCTION__,
        gfid);
    return GetSriovResources()->CreateTaskDesc(gfid);
}

void MDiagVmResources::SetArgs()
{
    m_pArgs.reset(new ArgReader(s_VmParams));
}

RC MDiagVmResources::Init()
{
    SetArgs();
    m_TimeoutMs = GetArgs()->ParamFloat("-timeout_ms", m_TimeoutMs);
    return GetGpuUtils()->Init();
}

GpuPartition* MDiagVmResources::GetGpuPartitionByGfid(UINT32 gfid)
{
    UINT32 swizzId = GetSriovResources()->GetSwizzId(gfid);
    return GetSmcResources()->GetGpuPartition(swizzId);    
}

//////////////////Shorten access for MDiagVmResources/////////////////////
MDiagVmResources* GetMDiagVmResources()
{
    return GetMDiagvGpuMigration()->GetVmResources();
}

//////////////////MDiagvGpuMigraiton/////////////////////

void MDiagvGpuMigration::ParseCmdLineArgs()
{
    if (!IsEnabled())
    {
        // Check if enable from command line.
        ArgReader args(s_VmParams);
        if (args.ParamPresent("-enable_vgpu_migration"))
        {
            Enable();
        }
    }
}

RC MDiagvGpuMigration::Init()
{
    if (IsEnabled())
    {
        return GetVmResources()->Init();
    }

    // vGpu Migration functions not enabled.
    return OK;
}

void MDiagvGpuMigration::Enable()
{
    if (IsEnabled())
    {
        return;
    }
    m_pVmRes.reset(new MDiagVmResources);
    m_bEnabled = true;
}

void MDiagvGpuMigration::SetBaseRun()
{
    m_bIsBaseRun = true;
}

void MDiagvGpuMigration::SetMigrationRun()
{
    m_bIsMigrationRun = true;
}

bool MDiagvGpuMigration::IsBaseRun() const
{
    return m_bIsBaseRun;
}

bool MDiagvGpuMigration::IsMigrationRun() const
{
    return m_bIsMigrationRun;
}

void MDiagvGpuMigration::ConfigFbCopyProgressPrintGran(UINT64 fbLen)
{
    if (fbLen >= (1ULL << 30))
    {
        m_FbCopyProgressPrintGran = 256ULL << 20;
    }
    else if (fbLen >= (512ULL << 20))
    {
        m_FbCopyProgressPrintGran = 64ULL << 20;
    }
    else if (fbLen >= (64ULL << 20))
    {
        m_FbCopyProgressPrintGran = 16ULL << 20;
    }
    else
    {
        m_FbCopyProgressPrintGran = 4ULL << 20;
    }
    DebugPrintf("%s, " vGpuMigrationTag " Set FB Copy progress printing granularity to %lldM.\n",
        __FUNCTION__,
        m_FbCopyProgressPrintGran >> 20);
}

void MDiagvGpuMigration::PrintFbCopyProgress(UINT64 off) const
{
    if (off && 0 == (off % m_FbCopyProgressPrintGran))
    {
        InfoPrintf(vGpuMigrationTag" FB Copy progress, PA offset 0x%llx(%lldM) ...\n",
            off,
            off >> 20);
    }
}

static MDiagvGpuMigration s_vGpuMigration;
MDiagvGpuMigration* GetMDiagvGpuMigration()
{
    return &s_vGpuMigration;
}    

