/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
// Please keep the documentation at the following page in sync with the code:
// http://engwiki/index.php/MODS/GPU_Verification/trace_3d/File_Format

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.

#include "mdiag/tests/stdtest.h"

#include "tracemod.h"
#include "tracerel.h"
#include "tracechan.h"
#include "massage.h"
#include "trace_3d.h"
#include "mdiag/utils/tex.h"
#include "selfgild.h"
#include "bufferdumper.h"
#include "core/include/simclk.h"

#include "fermi/gf100/dev_ram.h"
#include "pascal/gp100/dev_mmu.h"

#include "class/cl9097.h" // FERMI_A
#include "class/cla097.h" // KEPLER_A

#include "ctrl/ctrl0041.h"
#include "core/include/lwrm.h"
#include "Lwcm.h"

#include "core/include/channel.h"

#include "core/include/gpu.h"
#include "mdiag/utils/XMLlogging.h"
#include "mdiag/utils/perf_mon.h"
#include "mdiag/gpu/zlwll.h"

#include "lwos.h"
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <fcntl.h>
#include <errno.h>
#include <memory>
#include <algorithm>
#include <deque>

// SLI support.
#include "slisurf.h"

#include "mdiag/resource/lwgpu/dmaloader.h"
#include "gpu/utility/surfrdwr.h"
#include "gpu/utility/surffmt.h"
#include "teegroups.h"

#include "mdiag/resource/lwgpu/verif/GpuVerif.h"

#include "mdiag/resource/lwgpu/crcchain.h"

#include "core/include/chiplibtracecapture.h"

#include "mdiag/utils/sharedsurfacecontroller.h"
#include "mdiag/lwgpu.h"
#include "mdiag/utils/lwgpu_classes.h"

// //////////////////////////////////////////////////////////////////////////

#define MSGID(Group) T3D_MSGID(Group)

//////////////////////////////

CachedSurface::ByteType* CachedSurface::SurfaceCopy::GetCopy(SizeType *pSizeOut) const
{
    if (pSizeOut)
    {
        *pSizeOut = GetSize();
    }
    return m_pBytes;
}

void CachedSurface::SurfaceCopy::SetCopy(void* ptr, const SizeType size)
{
    m_pBytes = static_cast<ByteType*>(ptr);
    SetSize(size);
}

//////////////////////////////

CachedSurface::CachedSurface(TraceModule* tm,
        const SizeType subdeviceNum,
        bool cacheOnWrite,
        bool bLazyLoadFile) :
    m_TraceModule(tm),
    m_SubdeviceNum(subdeviceNum),
    m_CacheOnWrite(cacheOnWrite),
    m_WasDownloaded(false),
    m_IsFilled(false),
    m_InLazyLoadStatus(bLazyLoadFile),
    m_IsConstContBuffer(false)
{
    MASSERT(m_TraceModule != 0);
    DebugPrintf(MSGID(Surface), "CachedSurface for %d subdevices on %s with %s\n",
            subdeviceNum,
            m_TraceModule->GetName().c_str(),
            (cacheOnWrite ? "cacheOnWrite" : "no cacheOnWrite"));

    m_Cache.resize(m_SubdeviceNum);
    // All the copies are not present (set to NULL/zero) at the very beginning -
    // accomplished by SurfaceCopy's constructor.
}

CachedSurface::~CachedSurface()
{
    DebugPrintf(MSGID(Surface), "CachedSurface::%s (%s)\n", __FUNCTION__,
            m_TraceModule->GetName().c_str());
    DeallocAll();
    m_SubdeviceNum = 0;
}

const bool CachedSurface::LikelyOverflow(const SizeType size, const SizeType offset) const
{
    if (size > 0 && DataCount(offset) == DataCount(size - 1) && SafeSize(size) > size)
    {
        InfoPrintf(MSGID(Surface), "Alert: access-overflow might happen when Put32/Get32! Actual size %z, 4B rouned size %z, offset %z, only partial data would be valid.\n",
            size, SafeSize(size), offset
            );
        return true;
    }

    return false;
}

bool CachedSurface::IsCopyPresent(const SizeType gpuSubdevIdx) const
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    return !m_Cache[gpuSubdevIdx].NoCopy();
}

CachedSurface::ByteType* CachedSurface::AllocAndCopy(SurfaceCopy& cp, const void* surf, const SizeType size)
{
    ByteType *p = new ByteType[SafeSize(size)];
    memcpy(p, surf, size);
    cp.SetCopy(p, size);

    return p;
}

void CachedSurface::Dealloc(const SizeType gpuSubdevIdx)
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    // Delegate to SurfaceCopy
    SurfaceCopy& cp = m_Cache[gpuSubdevIdx];
    if (!cp.NoCopy())
    {
        delete [] cp.GetCopy();
        cp.SetCopy(0, cp.GetSize());
    }
}

void CachedSurface::DeallocAll()
{
    for (SizeType gpuSubdevIdx = 0; gpuSubdevIdx < GetSubdeviceNumber();
            ++gpuSubdevIdx)
    {
        Dealloc(gpuSubdevIdx);
    }
}

// EDIT in 2015Nov: Renaming AcquireOwnership() to Import().
CachedSurface::ByteType* CachedSurface::Import(const SizeType gpuSubdevIdx, void* surfacePtr, const SizeType size)
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    // Delegate to SurfaceCopy
    SurfaceCopy& cp = m_Cache[gpuSubdevIdx];
    // The surface must not be present.
    // An improvement is to deallocate the surface if present and then
    // replace it with the one passed.
    MASSERT(cp.NoCopy());
    MASSERT(surfacePtr != 0 && size != 0);
    // Compatible to support older 4B basis manipulation - avoid from access-overflow.
    SizeType allocSize = SafeSize(size);
    if (allocSize == size)
    {
        cp.SetCopy(surfacePtr, size);
    }
    else
    {
        AllocAndCopy(cp, surfacePtr, size);
        delete[] static_cast<ByteType*>(surfacePtr);
    }

    return cp.GetCopy();
}

CachedSurface::ByteType* CachedSurface::Renew(const SizeType subdevIdx, void* surf, const SizeType size)
{
    DeallocAll();
    return Import(subdevIdx, surf, size);
}

void CachedSurface::RenewAll(void* surf, const SizeType size)
{
    ByteType* pCopy = Renew(0, surf, size);
    CopyAll(pCopy, size);
}

CachedSurface::ByteType* CachedSurface::MakeCopy(const SizeType devIdx, const void* surf, const SizeType size)
{
    MASSERT(devIdx < GetSubdeviceNumber());
    SurfaceCopy& cp = m_Cache[devIdx];
    MASSERT(cp.NoCopy());
    MASSERT(surf != 0);

    return AllocAndCopy(cp, surf, size);
}

bool CachedSurface::CopyAll(const void* surfacePtr, const SizeType size)
{
    MASSERT(surfacePtr != 0);
    bool bOwned = false;
    for (SizeType gpuSubdevIdx = 0; gpuSubdevIdx < GetSubdeviceNumber(); ++gpuSubdevIdx)
    {
        // Delegate to SurfaceCopy
        SurfaceCopy& cp = m_Cache[gpuSubdevIdx];
        if (!cp.IsMyCopy(surfacePtr))
        {
            MASSERT(cp.NoCopy());
            AllocAndCopy(cp, surfacePtr, size);
        }
        else
        {
            // it is owned by this surface.
            bOwned = true;
        }
    }

    return bOwned;
}

// EDIT in 2015Nov: Renaming ReleaseOwnership() to Export().
CachedSurface::DataType* CachedSurface::Export(const SizeType gpuSubdevIdx, SizeType *pSavedSize)
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    // Delegate to SurfaceCopy
    SurfaceCopy& cp = m_Cache[gpuSubdevIdx];
    // The surface must be present.
    MASSERT(!cp.NoCopy());
    if (pSavedSize)
    {
        *pSavedSize = cp.GetSize();
    }
    DataType* ptr = GetPtr(gpuSubdevIdx);
    cp.SetCopy(0, 0);

    return ptr;
}

void CachedSurface::Put032(const SizeType gpuSubdevIdx,
        const SizeType offset, UINT32 value)
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    // Delegate to SurfaceCopy
    SurfaceCopy& cp = m_Cache[gpuSubdevIdx];
    // The surface must have been allocated.
    MASSERT(!cp.NoCopy());
    MASSERT(offset % sizeof(UINT32) == 0);
    MASSERT(offset < cp.GetSize());

    UINT32* ptr = cp.GetData();
    LikelyOverflow(cp.GetSize(), offset);
    ptr[DataCount(offset)] = value;
}

UINT32* CachedSurface::Get032Addr(const SizeType gpuSubdevIdx, const SizeType offset)
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    SurfaceCopy& cp = m_Cache[gpuSubdevIdx];
    if (cp.NoCopy())
    {
        // Assumption:
        //  First Get032Addr() call is regarded as first cached surface access.
        //  Load data to cached surface here
        if (m_InLazyLoadStatus)
        {
            m_InLazyLoadStatus = false;
            if (m_IsConstContBuffer)
            {
                // Load constant data
                SetConstantData();
            }
            else
            {
                // Load from file
                m_TraceModule->LoadFromFile(m_TraceModule->GetTraceFileMgr());
            }
            return Get032Addr(gpuSubdevIdx, offset);
        }
        else
        {
            // Copy not present -> returns 0.
            return 0;
        }
    }
    else
    {
        // The surface is allocated.
        UINT32* ptr = cp.GetData();
        MASSERT(ptr != 0);
        MASSERT(offset % sizeof(UINT32) == 0);
        MASSERT(offset < cp.GetSize());
        LikelyOverflow(cp.GetSize(), offset);
        return &(ptr[DataCount(offset)]);
    }
}

UINT32 CachedSurface::Get032(const SizeType gpuSubdevIdx, const SizeType offset)
{
    return *Get032Addr(gpuSubdevIdx, offset);
}

UINT32* CachedSurface::GetPtr(const SizeType gpuSubdevIdx)
{
    return Get032Addr(gpuSubdevIdx, 0);
}

void CachedSurface::GetAllPtrs(set<UINT32*>* ptrs)
{
    MASSERT(ptrs->empty());
    for (SizeType i = 0; i < m_Cache.size(); ++i)
    {
        if (m_InLazyLoadStatus)
        {
            MASSERT(0 == m_Cache[i].GetData());
            // GetPtr() will trigger LoadFromfile to load file first
            // and then return the pointer
            ptrs->insert(GetPtr(i));
        }
        else
        {
            ptrs->insert(m_Cache[i].GetData());
        }
    }
}

const CachedSurface::SizeType CachedSurface::GetCopyNumber() const
{
    SizeType num = 0;
    for (SizeType i = 0; i < m_Cache.size(); ++i)
    {
        num += (IsCopyPresent(i) ? 1 : 0);
    }
    return num;
}

const CachedSurface::SizeType CachedSurface::GetSize(const SizeType gpuSubdevIdx) const
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    return m_Cache[gpuSubdevIdx].GetSize();
}

void CachedSurface::SetSize(const SizeType gpuSubdevIdx, const SizeType size)
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    m_Cache[gpuSubdevIdx].SetSize(size);
}

const CachedSurface::SizeType CachedSurface::GetTraceSurfSize(const SizeType gpuSubdevIdx) const
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    return m_Cache[gpuSubdevIdx].GetTraceSurfSize();
}

void CachedSurface::SetTraceSurfSize(const SizeType gpuSubdevIdx, const SizeType size)
{
    MASSERT(gpuSubdevIdx < GetSubdeviceNumber());
    m_Cache[gpuSubdevIdx].SetTraceSurfSize(size);
}

const CachedSurface::SizeType CachedSurface::GetSubdeviceNumber() const
{
    return m_SubdeviceNum;
}

void CachedSurface::Print(const SurfaceCopy* pCopy) const
{
    DebugPrintf(MSGID(Surface), "Cache[%d]: Ptr= %p, Size 0x%lx\n", pCopy - &m_Cache[0], pCopy->GetData(), long(pCopy->GetSize()));
}

void CachedSurface::Print() const
{
    for (SizeType i = 0; i < m_Cache.size(); ++i)
    {
        Print(&m_Cache[i]);
    }
}

RC CachedSurface::SetConstantData(const SizeType in_size)
{
    SizeType size = in_size;

    if (0 == size)
    {
        size = GetSize(0);
    }
    MASSERT(size != 0);
    ByteType* newData = new ByteType[size];
    if (!newData)
    {
        ErrPrintf("Can not allocate %z bytes\n", size);
        return RC::CANNOT_ALLOCATE_MEMORY;
    }
    fill(newData, newData + size, 0);
    // Bug 587283: replacing AcquireOwnership with CopyAll
    //CopyAll(newData, surfaceSize);
    // EDIT in 2015Nov: Update due to CopyAll() got revised.
    // Let Import() perform defensive assert.
    newData = Import(0, newData, size);
    CopyAll(newData, size);
    m_IsFilled = true;

    return OK;
}

// //////////////////////////////////////////////////////////////////////////
TraceModule::TraceModule
(
    Trace3DTest *test,
    string moduleName,
    GpuTrace::TraceFileType ftype
) : m_Test(test),
    m_FileType(ftype),
    m_IsDynamic(false),
    m_ModuleName(moduleName),
    m_pTraceChannel(0),
    m_DebugPrintSize(32),
    m_GpuCacheMode(Surface2D::GpuCacheDefault),
    m_P2PGpuCacheMode(Surface2D::GpuCacheDefault),
    m_bLoopback(false),
    m_IsColorOrZ(false),
    m_SharedByTegra(false),
    m_hVASpace(0)
{
    m_Trace = test->GetTrace();
    m_PeerIDs.push_back(USE_DEFAULT_RM_MAPPING);

    m_AddrRange.first = 0;
    m_AddrRange.second = 0;

    if (m_FileType == GpuTrace::FT_TEXTURE_HEADER)
    {
        m_IsTextureHeader = true;
    }
}

TraceModule::ModCheckInfo::ModCheckInfo():
    pTraceCh(0),
    Offset(0),
    Size(0),
    CheckMethod(NO_CHECK),
    isRawCRC(false)
{
}

MdiagSurf *TraceModule::GetSurface(SurfaceType s, UINT32 subdev) const
{
    return m_Test->GetSurfaceMgr()->GetSurface(s, subdev);
}

IGpuSurfaceMgr *TraceModule::GetSurfaceMgr() const
{
    return m_Test->GetSurfaceMgr();
}

const MdiagSurf *TraceModule::GetClipIDSurface() const
{
    return m_Test->GetSurfaceMgr()->GetClipIDSurface();
}

MdiagSurf *TraceModule::GetClipIDSurface()
{
    return m_Test->GetSurfaceMgr()->GetClipIDSurface();
}

GpuDevice *TraceModule::GetGpuDev() const
{
    return m_Test->GetBoundGpuDevice();
}

/* virtual */ bool TraceModule::IsPeer() const { return false; }

//---------------------------------------------------------------------------
// Hide the messy details of the N different representations of a "surface"
// from the relocation code.
//
void TraceModule::StringToSomeKindOfSurface
(
    UINT32 subdev,
    UINT32 peerNum,
    string name,
    TraceModule::SomeKindOfSurface * pskos,
    UINT32 peerID
)
{
    SurfaceType st = m_Trace->FindSurface(name.c_str());
    if (st != SURFACE_TYPE_UNKNOWN && st != SURFACE_TYPE_STENCIL)
    {
        if (st == SURFACE_TYPE_CLIPID)
        {
            MASSERT(peerNum == Gpu::MaxNumSubdevices);
            const MdiagSurf *pdb = m_Test->GetSurfaceMgr()->GetClipIDSurface();
            pskos->CtxDmaHandle = pdb->GetCtxDmaHandle();
            pskos->CtxDmaOffset = pdb->GetCtxDmaOffsetGpu();
            pskos->Size         = pdb->GetSize();
            pskos->SurfacePtr   = (void *)m_Test->GetSurfaceMgr()->
                                                        GetClipIDSurface();
            pskos->SurfaceType  = SURF_TYPE_DMABUFFER;
            pskos->Location     = pdb->GetLocation();
        }
        else
        {
            MdiagSurf* pns = m_Test->GetSurfaceMgr()->GetSurface(st, subdev);
            pskos->CtxDmaHandle = pns->GetCtxDmaHandle();
            if (peerNum == Gpu::MaxNumSubdevices)
            {
                pskos->CtxDmaOffset = pns->GetCtxDmaOffsetGpu();
            }
            else
            {
                MASSERT(pns->IsPeerMapped());
                pskos->CtxDmaOffset = pns->GetCtxDmaOffsetGpuPeer(peerNum, peerID);
            }
            pskos->CtxDmaOffset += pns->GetExtraAllocSize();
            pskos->Size         = pns->GetSize();
            pskos->SurfacePtr   = (void *)pns;
            pskos->SurfaceType  = SURF_TYPE_LWSURF;
            pskos->Location     = pns->GetLocation();
        }
        return;
    }

    GpuTrace::TraceFileType ft;
    if (OK == m_Trace->MatchFileType(name.c_str(), &ft))
    {
        MdiagSurf *pdb = m_Test->GetSurface(ft, m_hVASpace);
        pskos->CtxDmaHandle = m_Test->GetSurfaceCtxDma(ft, m_hVASpace);

        if (peerNum == Gpu::MaxNumSubdevices)
        {
            pskos->CtxDmaOffset = pdb->GetCtxDmaOffsetGpu();
        }
        else
        {
            MASSERT(pdb->IsPeerMapped());
            pskos->CtxDmaOffset = pdb->GetCtxDmaOffsetGpuPeer(peerNum, peerID);
        }
        pskos->CtxDmaOffset += pdb->GetExtraAllocSize();
        pskos->Size         = pdb->GetAllocSize();
        pskos->SurfacePtr   = pdb;
        pskos->SurfaceType  = SURF_TYPE_DMABUFFER;
        pskos->Location     = pdb->GetLocation();
        return;
    }

    TraceModule * ptm;
    ptm = m_Trace->ModFind(name.c_str());
    if (0 != ptm)
    {
        pskos->CtxDmaHandle = ptm->GetCtxDmaHandle();
        pskos->CtxDmaOffset = ptm->GetOffset(peerNum, subdev, peerID);
        pskos->Size         = ptm->GetSize();
        pskos->SurfacePtr   = ptm->IsPeer() ? (void *)ptm :
                                              (void *)ptm->GetDmaBufferNonConst();
        pskos->SurfaceType  = ptm->IsPeer() ? SURF_TYPE_PEER :
                                              SURF_TYPE_DMABUFFER;
        if (ptm->GetLocation() == _DMA_TARGET_VIDEO)
            pskos->Location = Memory::Fb;
        else if (ptm->GetLocation() == _DMA_TARGET_COHERENT)
            pskos->Location = Memory::Coherent;
        else if (ptm->GetLocation() == _DMA_TARGET_NONCOHERENT)
            pskos->Location = Memory::NonCoherent;
        else
            pskos->Location = Memory::Fb;

        return;
    }

    // Shouldn't get here -- bug in test.hdr parser?
    MASSERT(!"Invalid surface name given to GpuTrace::"
            "StringToSomeKindOfSurface");
}

MdiagSurf *TraceModule::SKOS2DmaBufPtr
(
    const SomeKindOfSurface *pSKOS
)
{
    if (pSKOS->SurfaceType == TraceModule::SURF_TYPE_DMABUFFER)
    {
        return (MdiagSurf *)(pSKOS->SurfacePtr);
    }
    else
    {
        // Fail ASSERT since this SomeKindOfSurface is not from a DmaBuffer
        MASSERT(!"Tried to get a pointer to a DmaBuffer, "
                "but SomeKindOfSurface has a LWSurface or TraceModule pointer!");
    }

    return 0;
}

MdiagSurf* TraceModule::SKOS2LWSurfPtr
(
    const SomeKindOfSurface *pSKOS
)
{
    if (pSKOS->SurfaceType == TraceModule::SURF_TYPE_LWSURF)
    {
        return (MdiagSurf*)(pSKOS->SurfacePtr);
    }
    else
    {
        // Fail ASSERT since this SomeKindOfSurface is not from LWSurface
        MASSERT(!"Tried to get a pointer to a LWSurface, "
                "but SomeKindOfSurface has a MdiagSurf or TraceModule pointer!");
    }

    return 0;
}

TraceModule *TraceModule::SKOS2ModPtr
(
    const SomeKindOfSurface *pSKOS
)
{
    if (pSKOS->SurfaceType == TraceModule::SURF_TYPE_PEER)
    {
        return (TraceModule *)(pSKOS->SurfacePtr);
    }
    else
    {
        // Fail ASSERT since this SomeKindOfSurface is not from a TraceModule
        MASSERT(!"Tried to get a pointer to a TraceModule, "
                "but SomeKindOfSurface has a MdiagSurf or LWSurface pointer!");
    }

    return 0;
}

// Should this module be allocated in its own buffer
// instead of a section of a larger buffer.
bool TraceModule::AllocIndividualBuffer() const
{
    // Buffers that must be allocated within a certain address
    // range need to be allocated separately.
    if (HasAddressRange())
    {
        return true;
    }

    // GMEM buffers that have attribute or type overrides
    // will be allocated separately.
    //
    // A future optimization would be to combine buffers
    // with identical overrides into one surface allocation.
    else if ((GetFileType() >= GpuTrace::FT_GMEM_A) &&
        (GetFileType() <= GpuTrace::FT_GMEM_P) &&
        (HasTypeOverride() || HasAttrOverride()))
    {
        return true;
    }

    // Buffer shared with CheetAh engines (normally from plugin)
    // need to be allocated separately. Because they will manipulate
    // MdiagSurf pointer directly so it must not be a chunked one.
    else if (m_SharedByTegra)
    {
        return true;
    }
    else if (HasVprSet())
    {
        return true;
    }

    return false;
}

void TraceModule::SaveTrace3DSurface(MdiagSurf *)
{
    MASSERT(!"Can't call SaveTrace3DSurface on this module");
}

RC TraceModule::AllocateSurface(MdiagSurf *surface, GpuDevice *gpudev)
{
    RC rc = surface->Alloc(gpudev, m_Test->GetLwRmPtr());
    if (m_pTraceChannel)
    {
        surface->SetChannel(m_pTraceChannel->GetCh());
    }

    // This is a hack to get around a problem with using explicit virtual
    // allocation to satisfy virtual address ranges.  (Bug 545822)
    // If an allocation fails due to an address range constraint,
    // try again with implicit virtual allocation.  The explicit virtual
    // allocation is more efficient, but it has some holes such that for
    // some older traces, only the implicit virtual allocation will work.
    if ((rc == RC::LWRM_INSUFFICIENT_RESOURCES) &&
        (surface->GetMemHandle() != 0) &&
        surface->HasVirtAddrRange() &&
        surface->GetGpuVirtAddrHintUseVirtAlloc())
    {
        surface->GetSurf2D()->SetGpuVirtAddrHintUseVirtAlloc(false);
        rc.Clear();
        rc = surface->ReMapPhysMemory();
    }

    return rc;
}

//--------------------------------------------------------------------------
// This function notifies any objects that are dependent on the buffer in
// this trace module that the buffer is about to be freed and should no
// longer be accessed.
// This function is intended for buffers that are explictly freed
// via a trace header command.
//
void TraceModule::Release()
{
    // Tell GpuVerif that this buffer should no longer be
    // considered for CRC checking.
    DisconnectFromGpuVerif();

    // Release any surface view modules that might be depending
    // on this module.
    ReleaseSurfaceViews();
}

//--------------------------------------------------------------------------
// If this trace module has CRC check information, this function tells
// GpuVerif to remove its surface from the list of surfaces to CRC check.
// This function should be called when the trace module's surface is freed.
//
void TraceModule::DisconnectFromGpuVerif()
{
    ModCheckInfo checkInfo;

    if (GetCheckInfo(checkInfo) && (checkInfo.CheckMethod != NO_CHECK))
    {
        MASSERT(checkInfo.pTraceCh != 0);

        GpuVerif* gpuVerif = checkInfo.pTraceCh->GetGpuVerif();
        MASSERT(gpuVerif != 0);

        gpuVerif->RemoveAllocSurface(GetDmaBufferNonConst());
    }
}

//--------------------------------------------------------------------------
// This function is for notifying the dependent surface views that they
// they should release themselves from any object that is relying on them.
// For example, a color/z surface view should notify that surface manager
// that it is no longer valid.
// This function should be called if a buffer is dynamically freed.
//
void TraceModule::ReleaseSurfaceViews()
{
    vector<ViewTraceModule *>::iterator iter;

    for (iter = m_SurfaceViews.begin();
         iter != m_SurfaceViews.end();
         ++iter)
    {
        (*iter)->Release();
    }
}

//--------------------------------------------------------------------------
// return name of the channel attached to the module
/*virtual*/ string TraceModule::GetChannelName()
{
    if (m_pTraceChannel)
        return m_pTraceChannel->GetName();

    return "no channel";
}

/*virtual*/ bool TraceModule::HasAddressRange() const
{
    UINT32 vaBits = 40;
    if (OK != GetGpuDev()->GetGpuVaBitCount(&vaBits))
    {
        if (EngineClasses::IsGpuFamilyClassOrLater(
            TraceSubChannel::GetClassNum(), LWGpuClasses::GPU_CLASS_PASCAL_A))
            vaBits = 49;
    }

    // Gpu VA range: 0 ~ 1 << vaBits - 1
    // The specified range can be ignored
    // if it includes whole Gpu VA range
    //
    // Note:
    // Behavior difference between chunk allocation and individual allocation like
    // allocation sequence may lead to allocated VA difference. Different VA may
    // lead to test failure. Some tests with 0~0x1ffffffffffff range fails during the submission.
    // The case:
    //    FILE chunk_ProgramPool.lwt ComputeProgram SWAP_SIZE 4 ADDRESS_RANGE 0x000000000000 0x1ffffffffffff
    //    Failed:  chunk_ProgramPool.lwt : 0xbe004013 0x00000000 0x1ffffff1f0000 0x22a0000
    //    Passed : chunk_ProgramPool.lwt : 0xbe004011 0x00000000    0xffff030000 0x14a0000
    //
    // To keep behavior unchanged, ignore the 0~max_va range specification to still use chunk allocation
    // http://lwbugs/200157596/26 has more details.
    //
    if ((m_AddrRange.first == 0) &&
        (m_AddrRange.second >= (1ULL << vaBits) - 1))
    {
        return false;
    }

    return (m_AddrRange.first > 0) || (m_AddrRange.second > 0);
}

//--------------------------------------------------------------------------
// Check if target tracemodule has been in list or some child's list
//
bool TraceModule::HasRelocTarget(TraceModule *target)
{
    list<TraceModule*>::iterator iter;
    list<TraceModule*>::iterator end = m_RelocTargets.end();
    for (iter = m_RelocTargets.begin(); iter != end; ++iter)
    {
        if (target == *iter ||
            ((*iter) != this && (*iter)->HasRelocTarget(target)))
        {
            // Found
            return true;
        }
    }
    return false;
}

//--------------------------------------------------------------------------
// Build up a relocation dependency graph, put all target modules in.
// This list is used to select local/peered address in SLI relocation.
//
void TraceModule::AddRelocTarget(TraceModule *target)
{
    if (!HasRelocTarget(target))
    {
        // Only add once and no reloc circle is allowed
        m_RelocTargets.push_front(target);
    }
}

//--------------------------------------------------------------------------
// Count the peered suface in the path of current module to a pushbuffer,
// the path is a relocation dependency graph, used in SLI relocation
// parameter: count - IN&OUT, includs current module itself
//            subdev - the subdevice tries to access the surface
// return value: true - found PB in the path
//               false - no PB found in the sub tree
//
bool TraceModule::CountRemoteSurfInRelocTargets(UINT32 &count, UINT32 subdev)
{
    if (m_RelocTargets.empty())
    {
        count = 0;
        return false;
    }

    bool found = false;
    UINT32 hostSubdev = GetTest()->GetSSM()->GetHostingSubdevice(
        GetDmaBufferNonConst(), GetGpuDev(), subdev);
    count = (hostSubdev == subdev) ? 0 : 1;

    list<TraceModule*>::iterator iter;
    list<TraceModule*>::iterator end = m_RelocTargets.end();
    for (iter = m_RelocTargets.begin(); iter != end; ++iter)
    {
        if ((*iter)->GetFileType() == GpuTrace::FT_PUSHBUFFER)
        {
            return true;
        }
    }
    for (iter = m_RelocTargets.begin(); iter != end; ++iter)
    {
        UINT32 countInParents = 0;
        // Relwrsion in all parent modules, count the peered surface
        // only if PB is in that sub tree
        if (((*iter) != this) &&
            (*iter)->CountRemoteSurfInRelocTargets(countInParents, subdev))
        {
            count += countInParents;
            found = true;
        }
    }

    return found;
}

//--------------------------------------------------------------------------
// This function determines which subdevice will be accessing the copy
// of this surface that is located on the specified host subdevice.
//
RC TraceModule::GetAccessingSubdevice
(
    UINT32 hostSubdev,
    UINT32 *accessingSubdev
)
{
    RC rc;

    if (m_RelocTargets.empty() || (GetDmaBufferNonConst() == nullptr))
    {
        *accessingSubdev = hostSubdev;
    }
    else
    {
        *accessingSubdev = GetTest()->GetSSM()->GetAccessingSubdevice(
            GetDmaBufferNonConst(), GetGpuDev(), hostSubdev);
    }

    return rc;
}

//--------------------------------------------------------------------------
// This function returns true if a surface has a relocation command to itself
// (i.e., the surface's address will be stored in the same surface's data.)
//
bool TraceModule::HasSelfReloc()
{
    list<TraceModule*>::iterator iter;
    for (iter = m_RelocTargets.begin();
         iter != m_RelocTargets.end();
         ++iter)
    {
        if ((*iter) == this)
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------
// This function returns true if this surface is a legal candidate to be
// remotely accessed when in SLI mode.
//
bool TraceModule::CanBeSliRemote()
{
    MdiagSurf *surface = GetDmaBufferNonConst();

    // Here are the following conditions that must be met for a surface
    // to be considered as a candidate to be remotely accessed in SLI mode.
    // - The surface pointer must be valid.  E.g. pushbuffers don't have
    //   a surface pointer.
    // - The surface must be mapped to physical memory, otherwise the
    //   peer address has nothing to map to.
    // - The surface must have been allocated.  It's possible for a trace
    //   to have size zero, which tells MODS to not allocate it.
    // - The surface must not have a reloc command to itself.
    // - The surface must not have a virtual address constraint.  There isn't
    //   a way to guarantee that a peer address will match that constraint.
    //
    if ((surface != nullptr) &&
        surface->HasMap() &&
        (surface->GetAllocSize() > 0) &&
        !HasSelfReloc() &&
        !surface->HasFixedVirtAddr() &&
        !surface->HasVirtAddrRange())
    {
        return true;
    }

    return false;
}

// //////////////////////////////////////////////////////////////////////////
GenericTraceModule::GenericTraceModule
(
    Trace3DTest *test,
    string filename,
    GpuTrace::TraceFileType ftype,
    size_t filesize
) : TraceModule(test, filename, ftype)
{
    m_ModuleName = filename;
    m_FileToLoad = filename;

    m_pDmaBuf = 0;
    m_BufOffset = 0;
    m_Offset = 0;
    params = m_Test->params;

    if (m_FileType == GpuTrace::FT_PUSHBUFFER)
    {
        // In the case of the push-buffer there is only one copy for all the
        // subdevices.
        m_CachedSurface = new CachedSurface(this, 1, false,
            params->ParamPresent("-disable_memory_save_mode") == 0);
        m_CachedSurface->SetSize(0, filesize);
        m_CachedSurface->SetTraceSurfSize(0, filesize);
    }
    else
    {
        size_t subdeviceNum = test->GetBoundGpuDevice()->GetNumSubdevices();
        MASSERT(subdeviceNum >= 1);
        m_CachedSurface = new CachedSurface(this, subdeviceNum, false,
                    params->ParamPresent("-disable_memory_save_mode") == 0);
        // This part might be even removed since in any case the surfaces
        // are empty.
        for (UINT32 gpuSubdevIdx = 0; gpuSubdevIdx < subdeviceNum;
            ++gpuSubdevIdx)
        {
            m_CachedSurface->SetSize(gpuSubdevIdx, filesize);
            m_CachedSurface->SetTraceSurfSize(gpuSubdevIdx, filesize);
        }
    }

    m_Check = NO_CHECK;
    m_CheckOffset = 0;
    m_CheckSize = 0;
    m_CheckChannel = 0;
    m_RawCrc = false;
    m_Protect = Memory::Readable;
    m_Location = _DMA_TARGET_VIDEO;
    m_HeapAttr = 0;

    m_TxParams = 0;
    m_SwapSize = 0;
    m_Massaged = false;
    m_ImageCompressed = false;
    m_DepthCompressed = false;

    m_DownloadRaw = false;

    m_VP2TilingParams = 0;
    if (IsVP2TilingSupported())
        SetDefualtVP2TilingParams();

    m_CrcCheckMatch = true;
    m_Bl = 0;

    m_NoPbMassage = false;

    m_TypeOverride.first = false;
    m_AttrOverride.first = false;
    m_Attr2Override.first = false;

    m_Vpr.first = false;

    m_pluginDataHandle = 0;

    m_pGpuPeerMappings = NULL;
    m_LaunchMethodCount = 0;

    m_bAllocPushbufSurface = false;
    m_MapToBackingStore = false;

    m_WindowOrigin = LOWER_LEFT;
    m_SkipInit = false;
}

GenericTraceModule::~GenericTraceModule()
{
    if (m_CachedSurface != 0) {
        delete m_CachedSurface;
        m_CachedSurface = 0;
    }

    for (auto pRel : m_Relocs)
    {
        delete pRel;
    }

    m_Relocs.clear();

    delete m_TxParams;

    if (m_Bl)
        delete m_Bl;

    VP2SupportCleanup();
}

UINT64 GenericTraceModule::GetSize() const
{
    UINT32 gpuSubdevIdx = 0;
    if (m_CachedSurface)
    {
        return m_CachedSurface->GetSize(gpuSubdevIdx);
    }
    else
    {
        // Here for consistency with the old implementation we return 0,
        // even if it might be a bug to get here when the surface is not
        // initialized yet.
        return 0;
    }
}

UINT64 GenericTraceModule::GetOffset(UINT32 locSD, UINT32 remSD, UINT32 peerID) const
{
    if (remSD != 0)
    {
        DebugPrintf(MSGID(TraceMod), "GenericTraceModule::GetOffset : Remote subdevice %d ignored\n",
                    remSD);
    }
    if (locSD < Gpu::MaxNumSubdevices)
    {
        return m_pDmaBuf->GetCtxDmaOffsetGpuPeer(locSD, peerID) +
               m_pDmaBuf->GetExtraAllocSize() +
               m_BufOffset;
    }
    else
    {
        return GetOffset();
    }
}

RC GenericTraceModule::SetTraceChannel (TraceChannel * ptc, bool is_pushbuffer)
{
    if (ptc == nullptr)
    {
        bool allowed = params->ParamPresent("-allow_nocpuchannel_trace") && !is_pushbuffer;
        return allowed? OK : RC::ILWALID_FILE_FORMAT;
    }

    if (m_pTraceChannel && (m_pTraceChannel != ptc))
    {
        // User is not allowed to use the same FILE with different CHANNELs.
        return RC::ILWALID_FILE_FORMAT;
    }
    m_pTraceChannel = ptc;
    if( is_pushbuffer )
    {
        m_pTraceChannel->AddModule(this);
    }
    return OK;
}

RC GenericTraceModule::RelocAdd(TraceRelocation* pReloc)
{
    m_Relocs.push_back(pReloc);
    return OK;
}
GenericTraceModule::RelocIter GenericTraceModule::RelocBegin() const
{
    return m_Relocs.begin();
}
TraceModule::RelocIter GenericTraceModule::RelocEnd() const
{
    return m_Relocs.end();
}

TraceRelocation* GenericTraceModule::RelocBack() const
{
    return m_Relocs.back();
}

RC GenericTraceModule::RelocResetMethodAdd(UINT32 PushBufferOffset)
{
    m_RelocResetMethods.insert(PushBufferOffset);
    return OK;
}

bool GenericTraceModule::HasRelocResetMethod(UINT32 PushBufferOffset) const
{
    return m_RelocResetMethods.find(PushBufferOffset) != m_RelocResetMethods.end();
}

UINT32 GenericTraceModule::Get032(UINT32 offset, UINT32 gpuSubdevIdx)
{
    if (m_CachedSurface && !m_CachedSurface->WasDownloaded()) {
        if ((m_FileType == GpuTrace::FT_PUSHBUFFER) &&
            (m_CachedSurface->GetCopyNumber() == 1))
        {
            // There is only one copy of pushbuffer
            return m_CachedSurface->Get032(0, offset);
        }
        return m_CachedSurface->Get032(gpuSubdevIdx, offset);
    }
    else
    {
        UINT32 data;
        MdiagSurf* pMdiagSurf = GetParentModule() ? GetParentModule()->GetDmaBufferNonConst() : m_pDmaBuf;
        if (OK != pMdiagSurf->MapRegion(m_BufOffset+offset, sizeof(data), gpuSubdevIdx))
        {
            MASSERT(!"Cannot map buffer");
        }

        Platform::VirtualRd(pMdiagSurf->GetAddress(), (void*)&data, sizeof(data));
        pMdiagSurf->Unmap();

        return data;
    }
}

UINT32 *GenericTraceModule::Get032Addr(UINT32 offset, UINT32 gpuSubdevIdx)
{
    if ((m_FileType == GpuTrace::FT_PUSHBUFFER) &&
        (m_CachedSurface->GetCopyNumber() == 1))
    {
        // There is only one copy of pushbuffer
        return m_CachedSurface->Get032Addr(0, offset);
    }
    if (m_CachedSurface->GetPtr(gpuSubdevIdx) == 0)
    {
        // In case the cached surface has been released, return NULL.
        // Let the caller handle the error by checking the return value.
        return 0;
    }
    return m_CachedSurface->Get032Addr(gpuSubdevIdx, offset);
}

void GenericTraceModule::DoReloc(UINT32 offset, UINT32 data, UINT32 subdeviceIndex)
{
    // Save the relocation data so it can be restored later via
    // RedoRelocation.  This is not lwrrently supported in SLI mode
    // because the original offset of a relocation does not match the
    // final offset.  If a relocation for a subdevice other than zero oclwrs,
    // ignore it here and catch the error in RedoRelocations.
    if (subdeviceIndex == 0)
    {
        m_RelocMap[offset] = data;
    }

    Put032(offset, data, subdeviceIndex);
}

void GenericTraceModule::RedoRelocations()
{
    // Re-running RELOC commands is not lwrrently supported for dynamically
    // allocated surfaces.  This is because the primary user of this
    // feature, TDBG, will not have run the dynamic relocations before
    // this function is called.
    MASSERT(!IsDynamic());

    // Re-running RELOC commands is not lwrrently supported in SLI mode.
    MASSERT(m_Test->GetBoundGpuDevice()->GetNumSubdevices() == 1);

    RelocMap::iterator iter = m_RelocMap.begin();
    RelocMap::iterator iterEnd = m_RelocMap.end();

    for (; iter != iterEnd; ++iter)
    {
        Put032(iter->first, iter->second, 0);
    }
}

void GenericTraceModule::Put032(UINT32 offset, UINT32 data, UINT32 gpuSubdevIdx)
{
    if (IsFrozenAt(offset))
        return;

    if ((m_FileType == GpuTrace::FT_PUSHBUFFER) &&
        (m_CachedSurface->GetCopyNumber() == 1))
    {
        // There is only one copy of pushbuffer -- force subdevid to 0
        gpuSubdevIdx = 0;
    }

    if (m_CachedSurface && m_CachedSurface->IsCopyPresent(gpuSubdevIdx) &&
        !m_CachedSurface->WasDownloaded())
    {
        return m_CachedSurface->Put032(gpuSubdevIdx,
                offset, data);
    }
    else
    {
        MdiagSurf* pMdiagSurf = GetParentModule() ? GetParentModule()->GetDmaBufferNonConst() : m_pDmaBuf;
        if (OK != pMdiagSurf->MapRegion(m_BufOffset+offset, sizeof(data), gpuSubdevIdx))
        {
            MASSERT(!"Cannot map buffer");
        }

        Platform::VirtualWr(pMdiagSurf->GetAddress(), (void*)&data, sizeof(data));
        Platform::FlushCpuWriteCombineBuffer();

        pMdiagSurf->Unmap();
    }
}

RC GenericTraceModule::Allocate()
{
    RC rc;
    if(!params->ParamPresent("-silence_surface_info"))
        InfoPrintf("Allocating (%s)...\n", m_ModuleName.c_str());
    bool allocSeparate = false;

    UINT32 gpuSubdevIdx = 0;
    UINT64 surfaceSize = m_CachedSurface->GetSize(gpuSubdevIdx);

    if (m_FileType == GpuTrace::FT_SELFGILD)
    {
        // FT_SELFGILD type is not loaded into simulator
        // it contains info for trace_3d on how to do selfgild
        char* buffer =
            (char *)m_CachedSurface->GetPtr(gpuSubdevIdx);
        buffer[surfaceSize - 1] = '\0';
        SelfgildState* sg = CreateSelfgildState(buffer);
        if (!sg)
            return RC::CANNOT_ALLOCATE_MEMORY;
        m_Trace->AddSelfgildState(sg);
    }
    else if (m_FileType == GpuTrace::FT_PUSHBUFFER)
    {
        m_PushbufMemSize = surfaceSize;
    }
    else
    {
        // First, alloc the space we need
        if (!m_Test->TraceAllocate(this, m_FileType,
                surfaceSize, &m_pDmaBuf, &m_BufOffset, &allocSeparate))
            return RC::CANNOT_ALLOCATE_MEMORY;

        UINT32 blockRows = 0;
        CHECK_RC(GetVP2BlockRows(&blockRows));
        if (blockRows != 0)
        {
            UINT32 gobHeight = m_Test->GetBoundGpuDevice()->GobHeight();
            UINT32 blockHeight = blockRows / gobHeight;
            m_pDmaBuf->SetLogBlockHeight(log2(blockHeight));
            InfoPrintf("%s: Surface \"%s\" : block height: %d\n", 
                       __FUNCTION__, m_pDmaBuf->GetName().c_str(), blockHeight);
        }

        // Peer map the contained dma buffer to any peer GpuDevices
        // if necessary
        CHECK_RC(MapPeers());

        // Fill in some of the surface's parameters
        m_Offset = m_pDmaBuf->GetCtxDmaOffsetGpu()
            + m_pDmaBuf->GetExtraAllocSize() + m_BufOffset;

        DebugPrintf(MSGID(TraceMod), "BufOffset = 0x%llX\n", m_BufOffset);
        DebugPrintf(MSGID(TraceMod), "[hCtxDma,Offset] = [0x%08X,0x%llX]\n",
            m_pDmaBuf->GetCtxDmaHandle(), m_Offset);

        DebugPrintf(MSGID(TraceMod), " [hCtxDma,Offset,hMem] = [0x%08x,0x%llx,0x%08x]",
                m_pDmaBuf->GetCtxDmaHandle(), m_Offset, m_pDmaBuf->GetMemHandle());

        AllocateForSubdevices(surfaceSize, allocSeparate);

        if ((m_FileType >= GpuTrace::FT_PMU_0) &&
            (m_FileType <= GpuTrace::FT_PMU_7))
        {
            CHECK_RC(BindPMUSurface());
        }

        m_Test->m_BuffInfo.SetDescription(
            GpuTrace::GetFileTypeData(m_FileType).AllocTableTypeName);
    }
    DebugPrintf(MSGID(TraceMod), "\n");

    return OK;
}

RC GenericTraceModule::AllocateForSubdevices(UINT64 surfaceSize,
    bool allocSeparate)
{
    RC rc = OK;

    GpuDevice *pGpuDev = m_Test->GetBoundGpuDevice();
    UINT32 sliMask = params->ParamUnsigned("-sli_mask", 0x3) & 0x3;

    for (UINT32 subdev = 0; subdev < pGpuDev->GetNumSubdevices(); ++subdev)
    {
        string str = m_ModuleName + Utility::StrPrintf("(%d)", subdev);

        if (pGpuDev->GetNumSubdevices() > 1 &&
            m_Test->NeedPeerMapping(m_pDmaBuf))
        {
            // Only two subdevices supported for now.
            MASSERT(pGpuDev->GetNumSubdevices() <= 2);
            // SURFACE_VIEW is not supported for now
            MASSERT(!IsSurfaceView());

            bool isLocal = true;
            UINT32 hosting = subdev ? 0 : 1;
            SliSurfaceMapper::Accessing2Hosting a2h;

            switch (sliMask)
            {
                case 0:
                    ErrPrintf("-sli_mask is explicitly set to be 0, "
                        "you have to enable at least one GPU\n");
                    return RC::BAD_PARAMETER;

                case 1:
                    // Disable GPU1
                    a2h.insert(make_pair(0, 0));
                    a2h.insert(make_pair(1, 0));
                    isLocal = (subdev == 0);
                    hosting = 0;
                    break;

                case 2:
                    // Disable GPU0
                    a2h.insert(make_pair(0, 1));
                    a2h.insert(make_pair(1, 1));
                    isLocal = (subdev == 1);
                    hosting = 1;
                    break;

                case 3:
                    // Always remote
                    a2h.insert(make_pair(0, 1));
                    a2h.insert(make_pair(1, 0));
                    isLocal = false;
                    break;

                default:
                    ErrPrintf("Only 2-way SLI is supported now\n");
                    return RC::BAD_PARAMETER;
            }

            // For debug purpose update the surface name
            if (m_pDmaBuf->GetName().empty())
            {
                m_pDmaBuf->SetName(m_ModuleName);
            }

            m_Test->GetSSM()->AddDmaBuffer(pGpuDev, m_pDmaBuf, a2h, GetReloc().empty());

            if ((rc = m_pDmaBuf->MapPeer()) != OK)
            {
                ErrPrintf("%s: fail to map peer buffer for %s\n",
                    __FUNCTION__, m_pDmaBuf->GetName().c_str());
                return rc;
            }

            m_Test->m_BuffInfo.SetDmaBuff(str,
                *m_pDmaBuf,
                m_BufOffset,
                0,
                m_Test->NeedPeerMapping(m_pDmaBuf),
                surfaceSize,
                isLocal);

            m_Test->m_BuffInfo.SetVA(m_pDmaBuf->GetCtxDmaOffsetGpuPeer(hosting)
                + m_pDmaBuf->GetExtraAllocSize());
        }
        else
        {
            m_Test->m_BuffInfo.SetDmaBuff(
                pGpuDev->GetNumSubdevices() > 1 ? str : m_ModuleName,
                *m_pDmaBuf,
                m_BufOffset,
                0,
                m_Test->NeedPeerMapping(m_pDmaBuf),
                allocSeparate ? m_pDmaBuf->GetSize() : surfaceSize,
                !m_pDmaBuf->IsRemoteAllocated() && !m_pDmaBuf->HasFlaImportMem());

            if (!m_pDmaBuf->IsPhysicalOnly())
            {
                m_Test->m_BuffInfo.SetVA(m_Offset);
            }
        }

        if (m_FileType == GpuTrace::FT_TEXTURE)
            m_Test->m_BuffInfo.SetSizeBL(m_TxParams);

        if (m_FileType >= GpuTrace::FT_GMEM_A &&
            m_FileType <= GpuTrace::FT_GMEM_P)
        {
            m_Test->m_BuffInfo.SetSizeBL(m_Bl);
        }

        if ((m_FileType >= GpuTrace::FT_VP2_0 &&
                m_FileType <= GpuTrace::FT_VP2_9) ||
            m_pDmaBuf->GetCreatedFromAllocSurface())
        {
            m_Test->m_BuffInfo.SetSizeBL(*m_pDmaBuf);
            m_Test->m_BuffInfo.SetSizePix(*m_pDmaBuf);
        }
    }

    return rc;
}

RC GenericTraceModule::AllocPushbufSurf()
{
    if (!m_bAllocPushbufSurface)
        return OK;

    if(!params->ParamPresent("-silence_surface_info"))
        InfoPrintf("Allocating (%s)...\n", m_ModuleName.c_str());

    // First, alloc the space we need
    if (!m_Test->TraceAllocate(this, m_FileType,
            m_CachedSurface->GetSize(0), &m_pDmaBuf, &m_BufOffset, 0))
        return RC::CANNOT_ALLOCATE_MEMORY;

    // Fill in some of the surface's parameters
    m_Offset = m_pDmaBuf->GetCtxDmaOffsetGpu() + m_BufOffset;

    DebugPrintf(MSGID(TraceMod), "BufOffset = 0x%08X\n", m_BufOffset);
    DebugPrintf(MSGID(TraceMod), "[hCtxDma,Offset] = [0x%08X,0x%llX]\n",
        m_pDmaBuf->GetCtxDmaHandle(), m_Offset);

    DebugPrintf(MSGID(TraceMod), " [hCtxDma,Offset,hMem] = [0x%08x,0x%llx,0x%08x]\n",
            m_pDmaBuf->GetCtxDmaHandle(), m_Offset, m_pDmaBuf->GetMemHandle());

    m_Test->m_BuffInfo.SetDmaBuff(m_ModuleName,
            *m_pDmaBuf,
            m_BufOffset,
            0,
            m_Test->NeedPeerMapping(m_pDmaBuf));
    m_Test->m_BuffInfo.SetVA(m_Offset);

    return OK;
}

RC GenericTraceModule::PrepareToCheck(CHECK_METHOD CheckMethod, const char* CheckFilename, UINT32 gpuSubdevIdx)
{
    RC rc;
    UINT64 surfaceSize = m_CachedSurface->GetSize(gpuSubdevIdx);

    if ((0 == m_CheckSize) || (m_CheckSize > surfaceSize - m_CheckOffset))
        m_CheckSize = surfaceSize - m_CheckOffset;

    UINT32 width = 0, height = 0;
    if (m_Bl)
    {
        MASSERT(m_Geometry.size() == 2);
        width = m_Geometry[0];
        height = m_Geometry[1];
        m_CheckSize = m_Bl->Size();
    }

    if (!gpuSubdevIdx)
    {
        if (GetCheckChannel()->GetVASpaceHandle() != GetVASpaceHandle())
        {
            return RC::SOFTWARE_ERROR;
        }
        CHECK_RC(GetCheckChannel()->AddDmaBuffer(
                    m_pDmaBuf, CheckMethod, CheckFilename,
                    m_BufOffset + m_CheckOffset, m_CheckSize, m_SwapSize,
                    m_VP2TilingParams, m_Bl, width, height, m_CrcCheckMatch));
    }
    return OK;
}

RC GenericTraceModule::Download()
{
    RC rc;

    if (m_CachedSurface->GetCopyNumber() == 1)
    {
        UINT32* surfaceData = m_CachedSurface->GetPtr(0);
        UINT64  surfaceSize = m_CachedSurface->GetSize(0);

        m_CachedSurface->CopyAll(surfaceData, surfaceSize);
    }

    for (UINT32 gpuSubdevIdx = 0;
            gpuSubdevIdx < m_CachedSurface->GetSubdeviceNumber();
            ++gpuSubdevIdx)
    {
        DebugPrintf(MSGID(TraceMod), "GenericTraceModule::%s: gpuSubdevIdx= %u (name= %s)\n",
                __FUNCTION__, gpuSubdevIdx, GetName().c_str());
        UINT64 surfaceSize = m_CachedSurface->GetSize(gpuSubdevIdx);

        if (m_Check != NO_CHECK)
        {
            CHECK_RC(PrepareToCheck(m_Check, m_CheckFilename.c_str(), gpuSubdevIdx));
        }

        if ((m_FileType == GpuTrace::FT_ALLOC_SURFACE) &&
            m_FileToLoad.empty())
        {
            // No need to download any data for an ALLOC_SURFACE buffer
            // that doesn't have a FILE property.
        }
        else if ((m_FileType != GpuTrace::FT_SELFGILD) &&
                ((m_FileType != GpuTrace::FT_PUSHBUFFER) ||
                 m_bAllocPushbufSurface))
        {
            if ((m_FileType == GpuTrace::FT_SHADER_THREAD_STACK) ||
                    (m_FileType == GpuTrace::FT_SHADER_THREAD_MEMORY))
            {
                // See bug 138284.
                InfoPrintf("Hack!  Not loading shader thread stack/memory.\n");
            }
            else if ((m_FileType == GpuTrace::FT_TEXTURE) && m_MapToBackingStore)
            {
                InfoPrintf("texture buffer %s maps to backing store, skipping loading",
                    GetName().c_str());
            }
            else
            {
                UINT64 DataSize = surfaceSize;
                if (m_FileType == GpuTrace::FT_TEXTURE && !m_DownloadRaw)
                {
                    // Texture remapping from "naive pitch" into block-linear
                    // or pitch-linear
                    m_CachedSurface->Print();
                    // texture are never replicated
                    UINT32* surfaceData =
                        m_CachedSurface->GetPtr(0);

                    UINT32 *Temp =
                        (UINT32 *)m_TxParams->DoLayout((UINT08 *)surfaceData, surfaceSize);
                    if (!Temp)
                    {
                        ErrPrintf("Can not allocate %s\n", GetName().c_str());
                        return RC::CANNOT_ALLOCATE_MEMORY;
                    }

                    // Here there is a loop over the subdevices, so we use
                    // Import (older AcquireOwenrship) instead of CopyAll.
                    // EDIT 2015Dec, use ImportCachedSurface() combining ReleaseCachedSurface() and CachedSurface.Import().
                    DataSize = m_TxParams->Size();
                    ImportCachedSurface(gpuSubdevIdx, Temp, DataSize);
                }
                if (m_FileType >= GpuTrace::FT_GMEM_A && m_FileType <= GpuTrace::FT_GMEM_P)
                {
                    if (m_Bl)
                    {
                        // remapping 2D gmem regions
                        MASSERT(m_Geometry.size() == 2);
                        UINT08 *Temp = new UINT08[m_Bl->Size()];
                        if (!Temp)
                        {
                            return RC::CANNOT_ALLOCATE_MEMORY;
                        }

                        UINT32* surfaceData =
                            m_CachedSurface->GetPtr(gpuSubdevIdx);
                        UINT08 *Src = (UINT08*)surfaceData;

                        UINT32 Width = m_Geometry[0];
                        UINT32 Height = m_Geometry[1];
                        for (UINT32 i = 0; i < Height; ++i)
                        {
                            for (UINT32 j = 0; j < Width; ++j)
                            {
                                Temp[m_Bl->Address(Width*i + j)] = Src[Width*i + j];
                            }
                        }
                        // Here there is a loop over the subdevices, so we use
                        // Import (older AcquireOwnership) instead of CopyAll.
                        // EDIT 2015Dec, use ImportCachedSurface() combining ReleaseCachedSurface() and CachedSurface.Import().
                        ImportCachedSurface(gpuSubdevIdx, Temp, surfaceSize);
                    }
                    else
                    {
                        // MASSERT(0);
                    }
                }

                // Modify SPH for shader files
                if (m_FileType == GpuTrace::FT_SHADER_PROGRAM)
                {
                    rc = UpdateSph(gpuSubdevIdx);
                    if (rc != OK)
                    {
                        ErrPrintf("%s: Update SPH for %s failed\n", __FUNCTION__,
                                m_ModuleName.c_str());
                        return rc;
                    }
                } // SPH

                CHECK_RC(DownloadData2SubDev(DataSize, gpuSubdevIdx));
                m_CachedSurface->SetWasDownloaded();
            }

            // Dealloc the cached surface to save some memory
            if ((params->ParamPresent("-disable_memory_save_mode") == 0) &&
                (m_FileType != GpuTrace::FT_TEXTURE))
            {
                DebugPrintf(MSGID(TraceMod), "GenericTraceModule::%s: Release CachedSurface copy "
                        "for subdevIdx= %u (name= %s) to save memory.\n",
                        __FUNCTION__, gpuSubdevIdx, GetName().c_str());
                ReleaseCachedSurface(gpuSubdevIdx);
            }
        }
    }

    //
    // Release FT_TEXTURE surface at the end of downloading
    // because it's special: textures are never replicated and
    // cached surface of subdev0 is used for other subdev.
    if ((params->ParamPresent("-disable_memory_save_mode") == 0) &&
        (m_FileType == GpuTrace::FT_TEXTURE))
    {
        for (UINT32 gpuSubdevIdx = 0;
             gpuSubdevIdx < m_CachedSurface->GetSubdeviceNumber();
             ++gpuSubdevIdx)
        {
            ReleaseCachedSurface(gpuSubdevIdx);
        }
    }
    return OK;
}

RC GenericTraceModule::DownloadData2SubDev(UINT64 DataSize, UINT32 subdev)
{
    if (SkipInitialization())
    {
        return OK;
    }

    RC rc;
    UINT32* Data = m_FileType == GpuTrace::FT_TEXTURE ? m_CachedSurface->GetPtr(0) :
                                                        m_CachedSurface->GetPtr(subdev);
    m_CachedSurface->Print();
    MASSERT(Data != 0);
    SimClk::EventWrapper event(SimClk::Event::T3D_TRACE_PROFILE, "Download");
    GpuSubdevice *pSubdev =  m_Test->GetBoundGpuDevice()->GetSubdevice(subdev);

    if (m_pDmaBuf->FormatHasXBits())
    {
        WarnPrintf("X bits in %s will not be initialized," 
                "which would cause error in CRC callwlation.\n", GetName().c_str());
    }

    if (params->ParamPresent("-dma_load") > 0 &&
            m_pDmaBuf->GetLocation() == Memory::Fb)
    {
        WarnPrintf("-dma_load is unsupported, ignored.\n"); 
    }

    if (m_CachedSurface->IsFilled() && params->ParamPresent("-dma_fill") &&
            (m_pDmaBuf->GetLocation() == Memory::Fb || pSubdev->IsSOC()))
    {
        InfoPrintf("DMA filling enabled: subdev<%d> buffer<%s> size<%lld>\n",
                subdev, GetName().c_str(), DataSize);
        DmaLoader *loader = m_Trace->GetDmaLoader(GetCheckChannel());
        if (loader == 0)
        {
            ErrPrintf("Failed to create a dma loader!\n");
            return RC::UNSUPPORTED_FUNCTION;
        }

        SurfaceProtAttrModifier surfaceProtAttrModifier(m_pDmaBuf, Memory::ReadWrite);
        Utility::CleanupOnReturn<SurfaceProtAttrModifier, RC>
                restoreSurfaceProtAttr(&surfaceProtAttrModifier, &SurfaceProtAttrModifier::Restore);
        if ((m_pDmaBuf->GetProtect() & Memory::Writeable) == 0)        
        {
            CHECK_RC(surfaceProtAttrModifier.ChangeTo());
        }
        else
        {
            restoreSurfaceProtAttr.Cancel();
        }

        CHECK_RC(loader->AllocateNotifier(subdev));
        CHECK_RC(loader->FillSurface(m_pDmaBuf, m_BufOffset, Data[0], DataSize, subdev));

        return OK;
    }
    else
    {
        bool chiplibDumpDisabled = false;

        if (params->ParamPresent("-backdoor_load") && ChiplibTraceDumper::GetPtr()->IsChiplibTraceDumpEnabled())
        {
            ChiplibTraceDumper::GetPtr()->DisableChiplibTraceDump();
            chiplibDumpDisabled = true;
        }

        const UINT64 writeSize = min(DataSize, (m_pDmaBuf->GetAllocSize()));
                
        vector<UINT08> paddedData;
        if (m_pDmaBuf->MemAccessNeedsColwersion(params, MdiagSurf::WrSurf))
        {
            // gob alignment is required for format colwerting of BL surfaces w/o width/height
            UINT64 alignedSize = writeSize;
            if ((m_pDmaBuf->GetHeight() == 0) || (m_pDmaBuf->GetWidth() == 0))
            {
                alignedSize = ALIGN_UP(writeSize, GetGpuDev()->GobSize());
                if (alignedSize != writeSize)
                {
                    paddedData.resize(alignedSize);
                    memcpy(&paddedData[0], Data, writeSize);
                    for (UINT64 i = writeSize; i < alignedSize; i++)
                        paddedData[i] = 0;
                    Data = (UINT32*)(&paddedData[0]);
                }
            }
            
            PixelFormatTransform::ColwertNaiveToRawSurf(m_pDmaBuf, (UINT08*)&Data[0], alignedSize);
        }

        bool downloadWithCE = m_pDmaBuf->UseCEForMemAccess(params, MdiagSurf::WrSurf);
        if (downloadWithCE)
        {
            GpuVerif * gpuVerif = GetCheckChannel()->GetGpuVerif();
            CHECK_RC(m_pDmaBuf->DownloadWithCE(params, gpuVerif, (UINT08*)Data, DataSize, m_BufOffset, false, subdev));
        }
        else
        {
            MdiagSurf *mapSurface = GetParentModule() ?
                GetParentModule()->GetDmaBufferNonConst() :
                m_pDmaBuf;

            auto pWriter = SurfaceUtils::CreateMappingSurfaceWriter(mapSurface);
            pWriter->SetTargetSubdevice(subdev);
            CHECK_RC(pWriter->WriteBytes(m_BufOffset, Data, writeSize));
        }
        DebugPrintf(MSGID(TraceMod), "Downloading on the subdevice %d, 0x%llx bytes of %s ... "
            "(Data= %p), 0x%llx bytes total, with %s.\n",
            subdev, writeSize, GetName().c_str(), Data, DataSize,
            downloadWithCE ? "CE" : "CPU");

        if (chiplibDumpDisabled)
        {
            ChiplibTraceDumper::GetPtr()->EnableChiplibTraceDump();
        }

        if (params->ParamPresent("-backdoor_load"))
        {
            CHECK_RC(DumpRawMemory(m_pDmaBuf, m_BufOffset, writeSize, GetCheckChannel()->GetCh()));
        }

        return OK;
    }

    return OK;
}

RC GenericTraceModule::DumpRawMemory(MdiagSurf *surface, UINT64 offset, UINT32 size, LWGpuChannel* ch)
{
    RC rc;

    CHECK_RC(DumpRawSurfaceMemory(m_Test->GetLwRmPtr(), m_Test->GetSmcEngine(), surface, offset, size, m_ModuleName, false, m_Test->GetGpuResource(), ch, m_Test->params));

    return rc;
}

TraceChannel* GenericTraceModule::GetCheckChannel()
{
    if (!m_CheckChannel)
    {
        //host only channel cannot be used for crc check
        m_CheckChannel = m_pTraceChannel && !m_pTraceChannel->IsHostOnlyChannel() ? m_pTraceChannel : m_Test->GetDefaultChannelByVAS(GetVASpaceHandle());

    }
    return m_CheckChannel;
}
//---------------------------------------------------------------------------
// Overwrite part of a surface with the contents of a file.
RC GenericTraceModule::Update
(
    UINT32 Offset,
    const char *UpdateFilename,
    TraceFileMgr * pTraceFileMgr
)
{
    RC rc = OK;
    TraceFileMgr::Handle fhandle = 0;
    UINT08 *Data = 0;
    SimClk::EventWrapper event(SimClk::Event::T3D_TRACE_PROFILE, "UpdateFile");
    // The size can be 0 here.
    UINT32 gpuSubdevIdx = 0;
    UINT32 surfaceSize = m_CachedSurface->GetSize(gpuSubdevIdx);

    // We should make sure we iterate over all the subdevices (or we should
    // use copyAll).
//     MASSERT(m_CachedSurface->GetCopyNumber() == 1);

    rc = pTraceFileMgr->Open(UpdateFilename, &fhandle);
    if (OK != rc)
    {
        ErrPrintf("GenericTraceModule::Update can't open \"%s\".\n", UpdateFilename);
        return rc;
    }

    // This block ends at the Cleanup label.
    {
        CachedSurface::SizeType Size = pTraceFileMgr->Size(fhandle);
        // Read data from file
        Data = new UINT08[Size];
        if (!Data)
        {
            rc = RC::CANNOT_ALLOCATE_MEMORY;
            goto Cleanup;
        }
        CHECK_RC_CLEANUP(pTraceFileMgr->Read(Data, fhandle));

        // Texture remapping from "naive pitch" into block-linear or
        // pitch-linear
        if (m_FileType == GpuTrace::FT_TEXTURE)
        {
            UINT08 *Temp = m_TxParams->DoLayout(Data, surfaceSize);
            if (!Temp)
            {
                ErrPrintf("Can not allocate %s\n", GetName().c_str());
                goto Cleanup;
            }

            delete[] Data;
            Data = Temp;
            Size = m_TxParams->Size();
            if (!Data)
            {
                rc = RC::CANNOT_ALLOCATE_MEMORY;
                goto Cleanup;
            }

            // We only support full-surface updates
            if ((Offset != 0) || (Size != surfaceSize))
            {
                ErrPrintf("UPDATE_FILE to a texture %s must update the whole "
                        "texture, not a subregion: Size=0x%x, surfaceSize=0x%x\n",
                        m_ModuleName.c_str(), UINT32(Size), surfaceSize);
                rc = RC::BAD_TRACE_DATA;
                goto Cleanup;
            }

        }
        else if ( IsVP2TilingActive() )
        {
            // Data colwersion depending on Tiling Mode
            surfaceSize = Size;
            m_CachedSurface->Renew(gpuSubdevIdx, Data, surfaceSize);
            CHECK_RC(DoVP2TilingLayout());
            Data = (CachedSurface::ByteType*)m_CachedSurface->Export(gpuSubdevIdx, &Size);
        }

        bool chiplibDumpDisabled = false;

        const UINT32 MIN_BACKDOOR_LOAD_SIZE = 8;
        if (params->ParamPresent("-backdoor_load") && Size > MIN_BACKDOOR_LOAD_SIZE
                && ChiplibTraceDumper::GetPtr() && ChiplibTraceDumper::GetPtr()->IsChiplibTraceDumpEnabled())
        {
            ChiplibTraceDumper::GetPtr()->DisableChiplibTraceDump();
            chiplibDumpDisabled = true;
        }

        if (m_pDmaBuf->MemAccessNeedsColwersion(params, MdiagSurf::WrSurf))
        {
            PixelFormatTransform::ColwertNaiveToRawSurf(m_pDmaBuf, (UINT08*)&Data[0], Size);
        }

        if (m_pDmaBuf->UseCEForMemAccess(params, MdiagSurf::WrSurf))
        {
            GpuVerif * gpuVerif = GetCheckChannel()->GetGpuVerif();
            CHECK_RC(m_pDmaBuf->DownloadWithCE(params, gpuVerif, (UINT08*)Data,
                    Size, m_BufOffset + Offset, false, gpuSubdevIdx));
            DebugPrintf(MSGID(TraceMod), "Updated surface using copy engine\n");
        }
        else
        {
            // FIX_ME(gsaggese): Is this function called for all the subdevices?
            // Map the surface and write this update to it
            auto pWriter = SurfaceUtils::CreateMappingSurfaceWriter(m_pDmaBuf);
            pWriter->SetTargetSubdevice(gpuSubdevIdx);
            CHECK_RC(pWriter->WriteBytes(m_BufOffset+Offset, Data, Size));
            DebugPrintf(MSGID(TraceMod), "Updated surface using CPU\n");
        }

        if (chiplibDumpDisabled)
        {
            ChiplibTraceDumper::GetPtr()->EnableChiplibTraceDump();
        }

        if (params->ParamPresent("-backdoor_load") )
        {
            static int count = 0;
            string name = Utility::StrPrintf("UPDATE_FILE_%u", ++count);

            if (Size > MIN_BACKDOOR_LOAD_SIZE)
                CHECK_RC(DumpRawSurfaceMemory(m_Test->GetLwRmPtr(), 
                                              m_Test->GetSmcEngine(),
                                              m_pDmaBuf,
                                              m_BufOffset + Offset,
                                              Size,
                                              name,
                                              false,
                                              m_Test->GetGpuResource(),
                                              0,
                                              m_Test->params));
        }

        if (m_pDmaBuf->IsMapped())
        {
            m_pDmaBuf->Unmap();
        }
    }

    Cleanup:
    pTraceFileMgr->Close(fhandle);
    delete[] Data;

    return rc;
}

void GenericTraceModule::ReleaseCachedSurface(UINT32 gpuSubdevIdx)
{
    if (m_CachedSurface != 0)
    {
        m_CachedSurface->Dealloc(gpuSubdevIdx);
    }
}

void GenericTraceModule::ImportCachedSurface(const CachedSurface::SizeType subdevIdx, void* surf, const CachedSurface::SizeType size)
{
    if (m_CachedSurface != 0)
    {
        const CachedSurface::SizeType oldSize = m_CachedSurface->GetSize(subdevIdx);
        const CachedSurface::DataType *pCopy = m_CachedSurface->GetPtr(subdevIdx);
        // Try to save the Dealloc-Import operation.
        if (pCopy != 0)
        {
            if (pCopy != static_cast<const CachedSurface::DataType*>(surf) || size != oldSize)
            {
                m_CachedSurface->Dealloc(subdevIdx);
                m_CachedSurface->Import(subdevIdx, surf, size);
            }
            // else {
            //    Dealloc-Import operations saved. Mainly for reference moving forward.
            // }
        }
        else
        {
            // Only saved the Dealoc operation.
            m_CachedSurface->Import(subdevIdx, surf, size);
        }
    }
}

void GenericTraceModule::SetFilename(string newFilename)
{
    m_FileToLoad = newFilename;
}

void GenericTraceModule::StoreInlineData(const void * pData, const size_t dataSize)
{
    m_CachedSurface->DeallocAll();
    m_CachedSurface->MakeCopy(0, pData, dataSize);
}

// Return a vector with pointers to all the subdevice's surfaces.
void GenericTraceModule::GetSurfaces(set<UINT32*>* ptrs)
{
    DebugPrintf(MSGID(TraceMod), "GenericTraceModule::%s\n", __FUNCTION__);
    m_CachedSurface->Print();
    m_CachedSurface->GetAllPtrs(ptrs);
}

// Return a pointer to a specific subdevice's surface.
UINT32 * GenericTraceModule::GetSurfacePtr(UINT32 subdev)
{
    return m_CachedSurface->GetPtr(subdev);
}

RC GenericTraceModule::LoadFromFile(TraceFileMgr * pTraceFileMgr)
{
    if (m_CachedSurface->IsInLazyLoadStatus())
    {
        return OK;
    }

    SimClk::EventWrapper event(SimClk::Event::T3D_TRACE_PROFILE, "LoadFromFile");
    UINT32 gpuSubdevIdx = 0;
    UINT32* surfaceData = m_CachedSurface->GetPtr(gpuSubdevIdx);
    CachedSurface::SizeType surfaceSize = m_CachedSurface->GetSize(gpuSubdevIdx);
    if (surfaceData == 0)
    {
        MASSERT(surfaceSize != 0);

        CachedSurface::SizeType surfaceWordSize = (surfaceSize+sizeof(UINT32)-1)/sizeof(UINT32);
        surfaceData = new UINT32[surfaceWordSize];
        if (!surfaceData)
        {
            ErrPrintf("GenericTraceModule::LoadFromFile: couldn't allocate %llu "
                    "bytes of temp storage\n", surfaceWordSize);
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        // hack: clear the last word of the buffer so we won't have up to 3 bytes
        // of garbage at the end of a selfgild buffer which can cause random parser
        // failure.   This is possible because the surface size is rounded up to a
        // multiple of 4 *before* the NUL byte is tacked onto end of the buffer.
        surfaceData[surfaceWordSize - 1] = 0;

        RC rc = OK;

        if ( m_pluginDataHandle == 0 )
        {
            // normal flow -- read the TraceModule's data from the trace file
            //
            TraceFileMgr::Handle h;
            rc = pTraceFileMgr->Open(m_FileToLoad, &h);
            if (OK != rc)
            {
                ErrPrintf("GenericTraceModule::LoadFromFile: couldn't open \"%s\".\n",
                          m_FileToLoad.c_str());
                return RC::CANNOT_ALLOCATE_MEMORY;
            }

            if(pTraceFileMgr->Size(h) > surfaceWordSize * sizeof(UINT32))
            {
                ErrPrintf("GenericTraceModule::LoadFromFile: Trace file \"%s\" size(%lluB)  >  claimed size (%lluB) in hdr file. \n",
                          m_FileToLoad.c_str(), pTraceFileMgr->Size(h), surfaceWordSize * sizeof(UINT32));

                return RC::CANNOT_ALLOCATE_MEMORY;
            }

            rc = pTraceFileMgr->Read(surfaceData, h);
            pTraceFileMgr->Close(h);
        }
        else if (HasPluginInitData())
        {
            // The initial data for this TraceModule comes from a plugin.
            //
            rc = LoadDataFromPlugin( m_pluginDataHandle, surfaceData, surfaceWordSize );
        }

        // When we load a surface, if it's a PushBuffer, we don't have to
        // replicate. Actually the read-only surface can not be replicated.
        surfaceData = (UINT32*)m_CachedSurface->Import(gpuSubdevIdx, surfaceData, surfaceSize);
        if (m_FileType != GpuTrace::FT_TEXTURE)
        {
            m_CachedSurface->CopyAll(surfaceData, surfaceSize);
        }
        // Hack to reproduce a trace3d bug and pass the regressions.
        for (UINT32 gpuSubdevIdx = 0;
                gpuSubdevIdx < m_CachedSurface->GetSubdeviceNumber();
                ++gpuSubdevIdx)
        {
            m_CachedSurface->SetSize(gpuSubdevIdx, surfaceSize);
        }

        if (OK != rc)
        {
            ErrPrintf("GenericTraceModule::LoadFromFile: error reading \"%s\".\n",
                    m_FileToLoad.c_str());
            m_CachedSurface->DeallocAll();
            return rc;
        }
    }
    // success
    return OK;
}

SelfgildState* GenericTraceModule::CreateSelfgildState(const string& gi) const
{
    SelfgildState* ret = new SelfgildState;

    const string line_delims("\r\n");
    string::size_type beg_line, end_line;

    beg_line = gi.find_first_not_of(line_delims);

    while (beg_line != string::npos)
    {
        end_line = gi.find_first_of(line_delims, beg_line);
        if (end_line == string::npos)
            end_line = gi.length();

        //if (gi.substr(beg_line, end_line - beg_line) != "")
            if (!ret->AddParam(gi.substr(beg_line, end_line - beg_line)))
            {
                ErrPrintf("File parsing error\n");
                delete ret;
                return 0;
            }

        beg_line = gi.find_first_not_of(line_delims, end_line);
    }
    return ret;
}

RC GenericTraceModule::AddFreezeReloc(UINT32 offset)
{
    MASSERT(m_FreezeRelocs.find(offset) == m_FreezeRelocs.end());
    m_FreezeRelocs.insert(offset);
    return OK;
}

namespace {

    //! Method handler for oldskool GenericTraceModule::*MethodHandlers.
    class TraceModuleMethodMassager: public Massager
    {
    public:
        //! \param traceMod Trace module to which the method belongs.
        //! \param tracech  Trace channel the method need to inject.
        TraceModuleMethodMassager(GenericTraceModule* traceMod, TraceChannel* traceCh, UINT32 subdev)
            : m_TraceMod(traceMod), m_TraceChannel(traceCh), m_Subdev(subdev) {};
        virtual void Massage(PushBufferMessage& message) {
            for (UINT32 index=0; index < message.GetPayloadSize(); index++)
            {
                const UINT32 methodOffset = message.GetMethodOffset(index) >> 2;
                UINT32 data = message.GetPayload(index);
                MethodMassager massager = m_TraceChannel->GetMassager(
                    message.GetSubChannelNum(), methodOffset);
                if (massager)
                {
                    data = (m_TraceMod->*massager)(m_Subdev, methodOffset, data);
                }
                message.SetPayload(index, data);
            }
        }

    private:
        GenericTraceModule* m_TraceMod;
        TraceChannel*     m_TraceChannel;
        UINT32            m_Subdev;
    };

}

RC GenericTraceModule::MassagePushBuffer(UINT32 subdev, GpuTrace *trace, const ArgReader *params)
{
    RC rc;

    if((params && (params->ParamPresent("-no_pb_massage") > 0)) ||
        m_Test->GetNoPbMassage())
    {
        m_NoPbMassage = true;
    }

    int argnum = params->ParamPresent("-nset_cta_raster");
    for (int i=0; i<argnum; i++)
    {
        UINT32 v;
        v = params->ParamNUnsigned("-nset_cta_raster", i, 0);
        m_CtaRasterWidth.push_back(v);
        v = params->ParamNUnsigned("-nset_cta_raster", i, 1);
        m_CtaRasterHeight.push_back(v);
        v = params->ParamNUnsigned("-nset_cta_raster", i, 2);
        m_CtaRasterDepth.push_back(v);
    }

    DebugPrintf(MSGID(Massage), "MassagePushBuffer: GenericTraceModule::MassagePushBuffer()\n");
    TraceModuleMethodMassager messageMassager(this, m_pTraceChannel, subdev);
    MassagePushBuffer(messageMassager);

    return rc;
}

namespace {

    // Reads one pushbuffer segment
    class TraceModuleReader: public PushBufferMessage::SurfaceReader
    {
    public:
        TraceModuleReader(TraceModule& traceMod, UINT32 beginOfs, UINT32 endOfs, UINT32 subdev)
        : m_TraceMod(traceMod), m_LwrOfs(beginOfs), m_EndOfs(endOfs), m_Subdev(subdev) { }
        bool IsAnythingLeft() const { return m_LwrOfs+4 <= m_EndOfs; }
        UINT32 GetLwrrentOffset() const { return m_LwrOfs; }
        virtual vector<UINT32> Read(UINT32 size)
        {
            vector<UINT32> buf;
            while ((m_LwrOfs+4 <= m_EndOfs) && (buf.size() < size))
            // m_LwrOfs+4 <= m_EndOfs ensures that the UINT32 being read
            // will not get over the surface or segment end
            {
                buf.push_back(m_TraceMod.Get032(m_LwrOfs, m_Subdev));
                m_LwrOfs += 4;
            }
            return buf;
        }

    private:
        TraceModule& m_TraceMod;
        UINT32 m_LwrOfs;
        UINT32 m_EndOfs;
        UINT32 m_Subdev;
    };

}

//----------------------------------------------------------------------------
void GenericTraceModule::MassagePushBuffer(Massager& massager)
{
    DebugPrintf(MSGID(Massage), "MassagePushBuffer: Massaging pushbuffer...\n");

    // push buffer is shared between subdevs
    const UINT32 PB_SUBDEVIDX = 0;

    // Ensure that the pushbuffer is accessible
    MASSERT(m_CachedSurface->GetPtr(PB_SUBDEVIDX) != 0);

    // Choose message reader implementation
    unique_ptr<PushBufferMessage> pMessage(CreateMassagedMessage());

    // New pushbuffer and segments replacement
    deque<UINT32> newPB;
    SendSegments newSegments;
    newSegments.reserve(m_SendSegments.size());
    bool useNewPbBuf = false;

    // Track frozen relocations
    set<UINT32>::const_iterator frozenIt = m_FreezeRelocs.begin();

    bool   bMassageDone = (m_SendSegments.size() == 0) && !m_bAllocPushbufSurface;
    UINT32 iSeg = 0;
    UINT32 prevSegEnd = 0;

    while (!bMassageDone)
    {
        SendSegment seg;
        bool        bUpdateSegment = true;

        if (m_SendSegments.size() == iSeg)
        {
            seg.Start = prevSegEnd;
            seg.Size = GetSize() - prevSegEnd;
            bMassageDone = true;
            bUpdateSegment = false;
        }
        else
        {
            seg = m_SendSegments[iSeg++];
            prevSegEnd = (seg.Start + seg.Size);

            if (!m_bAllocPushbufSurface)
                bMassageDone = (m_SendSegments.size() == iSeg);
            else
                bMassageDone = (prevSegEnd == GetSize());
        }

        // Since there is on
        TraceModuleReader reader(*this, seg.Start, seg.Start+seg.Size, 0);
        const UINT32 newSegStart = newPB.size()*sizeof(UINT32);

        // Iterate over methods in a segment
        while (reader.IsAnythingLeft())
        {
            UINT32 offsetBeforeProcess = reader.GetLwrrentOffset();

            // Initialize message for massaging
            pMessage->Init(
                offsetBeforeProcess,
                useNewPbBuf? newPB.size()*sizeof(UINT32) : offsetBeforeProcess,
                reader);

            // Track frozen relocations
            const UINT32 mthStartOfs = pMessage->GetPushBufferOffset();
            const UINT32 mthEndOfs = mthStartOfs + pMessage->GetMessageSize()*4;
            while ((frozenIt != m_FreezeRelocs.end()) &&
                   (*frozenIt < mthEndOfs))
            {
                if (*frozenIt >= mthStartOfs)
                {
                    pMessage->FreezeValue((*frozenIt-mthStartOfs) / sizeof(UINT32));
                    ++frozenIt;
                }
            }

            // Massage message using custom massager
            massager.Massage(*pMessage);

            const vector<UINT32>& result = pMessage->GetMessageChunk();
            UINT32 offsetAfterProcess = reader.GetLwrrentOffset();
            if (useNewPbBuf)
            {
                // Add new message to new pushbuffer
                newPB.insert(newPB.end(), result.begin(), result.end());
            }
            else
            {
                // if method length unchanged after massage,
                // use original pb to store massaged method.
                // Otherwise, a new pb will be used.
                if (offsetBeforeProcess + result.size() * sizeof(UINT32)
                    == offsetAfterProcess)
                {
                    if (pMessage->IsModified())
                    {
                        for (UINT32 i = 0; i < result.size(); i ++)
                        {
                            Put032(offsetBeforeProcess + i * sizeof(UINT32),
                                result[i], PB_SUBDEVIDX);
                        }
                    }
                }
                else
                {
                    // copy-on-change happens on first length mismatch
                    // new pb should be empty
                    MASSERT(newPB.size() == 0);

                    // copy from old push buffer before insert new message
                    for (UINT32 i = 0; i < offsetBeforeProcess; i += 4)
                    {
                        newPB.push_back(Get032(i, PB_SUBDEVIDX));
                    }

                    // Add new message to new pushbuffer
                    newPB.insert(newPB.end(), result.begin(), result.end());

                    useNewPbBuf = true;
                }
            }

            if (offsetBeforeProcess + result.size() * sizeof(UINT32) != offsetAfterProcess)
            {
                int shiftedOffset = offsetBeforeProcess + result.size() * sizeof(UINT32) - offsetAfterProcess;
                for (auto pReloc : m_Relocs)
                {
                    if (pReloc->GetOffset() >= offsetAfterProcess)
                    {
                        auto oldOffset = pReloc->GetOffset();
                        pReloc->SetOffset(shiftedOffset + oldOffset);
                        DebugPrintf(MSGID(Massage), "MassagePushBuffer: update Pushbuffer (%s) reloc offset"
                                " old offset = 0x%x, new offset = 0x%x\n", m_ModuleName.c_str(), oldOffset, pReloc->GetOffset());
                    }
                    else if (pReloc->GetOffset() >= offsetBeforeProcess)
                    {
                        WarnPrintf(MSGID(Massage), "MassagePushBuffer: Pushbuffer (%s) method at offset 0x%x size is changed from"
                                " %u to %u, the corresponding Reloc could be ilwalidated!\n", m_ModuleName.c_str(), pReloc->GetOffset(),
                                offsetAfterProcess - offsetBeforeProcess, result.size() * sizeof(UINT32));
                    }
                    else
                    {
                        // nothing to do with Reloc before this method
                    }
                }
            }
        }

        if (bUpdateSegment)
        {
            // Update segments
            const SendSegment newSeg =
            {
                newSegStart,
                static_cast<UINT32>(newPB.size()*sizeof(UINT32)-newSegStart)
            };
            newSegments.push_back(newSeg);
        }
    }

    // Skip pushbuffer replacement if new push buffer is not used
    if (!useNewPbBuf)
    {
        DebugPrintf(MSGID(Massage), "MassagePushBuffer: Pushbuffer length not changed\n");
        return;
    }

    // Release old cached surface
    ReleaseCachedSurface(PB_SUBDEVIDX);

    // Create new cached surface buffer containing new pushbuffer data
    const UINT32 size = newPB.size() * sizeof(UINT32);
    if (!m_bAllocPushbufSurface && (m_SendSegments.size() > 0))
    {
        DebugPrintf(MSGID(Massage), "MassagePushBuffer: Replacing pushbuffer, oldsize=%u, newsize=%u\n",
            m_SendSegments[m_SendSegments.size()-1].Start+m_SendSegments[m_SendSegments.size()-1].Size,
            size);
    }
    else
    {
        DebugPrintf(MSGID(Massage), "MassagePushBuffer: Replacing pushbuffer, oldsize=%u, newsize=%u\n",
            GetSize(),
            size);
    }
    // Can't use cached surface's MakeCopy(PB_SUBDEVIDX, &newPB[0], size) here to save de/alloc
    // times. It may be dangrous if MakeCopy because newPB is a deque, not vector.
    UINT32 *pData = new UINT32[newPB.size()];
    copy(newPB.begin(), newPB.end(), pData);
    m_CachedSurface->Import(PB_SUBDEVIDX, pData, size);

    if (m_SendSegments.size() != 0)
    {
        // Use updated segments
        m_SendSegments.swap(newSegments);
    }
}

namespace {

    class ResetRelocMassager: public Massager
    {
    public:
        ResetRelocMassager(UINT32 gpuClass, TraceModule* module, Trace3DTest* test)
            : m_GpuClass(gpuClass), m_Module(module), m_Test(test) { }
        virtual void Massage(PushBufferMessage& message)
        {
            for (UINT32 index=0; index < message.GetPayloadSize(); index++)
            {
                if (HasRelocResetMethod(message, index))
                {
                    const UINT32 newData =
                        m_Test->GetMethodResetValue(
                            m_GpuClass,
                            message.GetMethodOffset(index));
                    message.SetPayload(index, newData);
                }
            }
        }
        bool HasRelocResetMethod(const PushBufferMessage& message, UINT32 index) const
        {
            const UINT32 ofs = message.GetPushBufferOffset();
            return m_Module->HasRelocResetMethod(ofs);
        }

    private:
        UINT32 m_GpuClass;
        TraceModule* m_Module;
        Trace3DTest* m_Test;
    };

}

//----------------------------------------------------------------------------
void GenericTraceModule::MassagePushBuffer2()
{
    DebugPrintf(MSGID(Massage), "MassagePushBuffer: GenericTraceModule::MassagePushBuffer2()\n");
    MASSERT(m_FileType == GpuTrace::FT_PUSHBUFFER);

    if (m_pTraceChannel)
    {
        UINT32 classID = m_pTraceChannel->GetSubChannel("")->GetClass();
        ResetRelocMassager messageMassager(classID, this, m_Test);
        MassagePushBuffer(messageMassager);
    }
}

// Print the data_map to DebugPrintf
static void PrintMap(TraceModule::SubdevMaskDataMap *data_map)
{
    DebugPrintf(MSGID(TraceMod), "GenericTraceModule::PrintMap: Printing map data...\n");

    for (TraceModule::SubdevMaskDataMap::iterator iter = data_map->begin();
            iter != data_map->end(); ++iter)
    {
        DebugPrintf(MSGID(TraceMod), "Map: Offset 0x%x\n", iter->first);
        for (TraceModule::Subdev2DataMap::iterator iter2 = iter->second.begin();
                iter2 != iter->second.end(); ++iter2)
        {
            DebugPrintf(MSGID(TraceMod), "Map:     Data for Subdev[%d] is 0x%x\n", iter2->first,
                        iter2->second);
        }
    }
}

//----------------------------------------------------------------------------
// prefix is the string with which to prefix the Printf message
// ofs tells the function whether or not to print the ofset.  (true = yes)
void GenericTraceModule::PrintPushBuffer(const char *prefix, bool ofs)
{
    // This function only prints debug messages, so don't bother parsing
    // the pushbuffer if debug messages won't be printed.
    if (!Tee::WillPrint(Tee::PriDebug)) return;

    DebugPrintf(MSGID(TraceMod), "%s: Starting to print pushbuffer\n", prefix);

    for (size_t iSeg = 0; iSeg < m_SendSegments.size(); ++iSeg)
    {
        SendSegments tmpSegs = m_SendSegments;
        if (iSeg)
        {
            // Patch start for growth in previous SendSegment.
            tmpSegs[iSeg].Start = tmpSegs[iSeg-1].Start + tmpSegs[iSeg-1].Size;
        }

        DebugPrintf(MSGID(TraceMod), "%s: Printing Segment %d\n",
                        __FUNCTION__, iSeg);

        UINT32 offset = tmpSegs[iSeg].Start;
        UINT32 offsetEnd = offset + tmpSegs[iSeg].Size;

        // For push-buffer file types, only a single copy is maintained at
        // subdevice index 0
        UINT32 gpuSubdevIdx = 0;
        MASSERT(m_CachedSurface->GetPtr(gpuSubdevIdx) != 0);

        while (offset < offsetEnd)
        {
            UINT32 header = m_CachedSurface->Get032(gpuSubdevIdx, offset);
            UINT32 num_dwords = GetNumDWFermi(header);

            if (ofs)
                DebugPrintf(MSGID(TraceMod), "%s: 0x%5x: Header 0x%5x with %d Data\n",
                    prefix, offset, header, num_dwords);
            else
                DebugPrintf(MSGID(TraceMod), "%s: Header 0x%5x with %d Data\n",
                    prefix, header, num_dwords);
            // Skip header
            offset += sizeof(UINT32);

            while ((offset < offsetEnd) && num_dwords--)
            {
                UINT32 data = m_CachedSurface->Get032(gpuSubdevIdx, offset);
                if (ofs)
                    DebugPrintf(MSGID(TraceMod), "%s:   0x%5x: Data 0x%5x\n", prefix,
                                    offset, data);
                else
                    DebugPrintf(MSGID(TraceMod), "%s:   Data 0x%5x\n", prefix, data);

                offset += sizeof(UINT32);
            }
        }
    }
}

namespace {

    class AddSubdevMask: public Massager
    {
    public:
        AddSubdevMask(TraceModule::SubdevMaskDataMap& dataMap, UINT32 subdevCount)
            : m_DataMap(dataMap), m_SubdevCount(subdevCount) { }
        virtual void Massage(PushBufferMessage& message)
        {
            // Skip if no offsets of this message are scheduled for patching
            const UINT32 mthStartOfs = message.GetPushBufferOffset();
            const UINT32 mthEndOfs = mthStartOfs + sizeof(UINT32)*message.GetMessageSize();
            TraceModule::SubdevMaskDataMap::const_iterator offsetIt =
                m_DataMap.lower_bound(mthStartOfs);
            if ((offsetIt == m_DataMap.end()) || (offsetIt->first >= mthEndOfs))
            {
                return;
            }

            // Skip if there are no subdevices scheduled for patching
            bool hasSubdevs = false;
            while ((offsetIt != m_DataMap.end()) && (offsetIt->first < mthEndOfs))
            {
                if ( ! offsetIt->second.empty())
                {
                    hasSubdevs = true;
                    break;
                }
            }
            if ( ! hasSubdevs)
            {
                return;
            }

            // Patch selected offsets
            vector< vector<UINT32> > newMessages;
            newMessages.resize(m_SubdevCount);
            for (UINT32 lwrSubdev=0; lwrSubdev < m_SubdevCount; lwrSubdev++)
            {
                // Reset original payload
                message.RestoreOriginal();

                // Patch subsequent offsets
                for (UINT32 i=0; i < message.GetPayloadSize(); i++)
                {
                    const TraceModule::Subdev2DataMap& subdevMap =
                        m_DataMap[message.GetPayloadOffset(i)];
                    TraceModule::Subdev2DataMap::const_iterator subdevIt =
                        subdevMap.find(lwrSubdev);
                    if (subdevIt == subdevMap.end())
                    {
                        continue;
                    }
                    message.SetPayload(i, subdevIt->second);
                }

                // Use patched message
                newMessages[lwrSubdev] = message.GetMessageChunk();
            }

            // Replace message with a set of patched ones
            message.SetSubdevMessages(newMessages);
        }

    private:
        TraceModule::SubdevMaskDataMap& m_DataMap;
        UINT32 m_SubdevCount;
    };

}

//----------------------------------------------------------------------------
// This function will patch offsets in the push buffer differently for each
// subdevice through the use of subdevice masks.  The data to patch is stored
// in the nested map SubdevMaskDataMap in each module, which stores data this way:
// SubdevMaskDataMap[offset_to_patch][subdevice] = data_to_patch
bool GenericTraceModule::MassageAndInsertSubdeviceMasks()
{
    if (m_SubdevMaskDataMap.empty())
    {
        return true;
    }

    // Prints push buffer to mods.log for debugging
    PrintPushBuffer("PB1",true);

    // Prints the data_map to mods.log for debugging
    PrintMap(&m_SubdevMaskDataMap);
    // Massage the pushbuffer
    DebugPrintf(MSGID(Massage), "MassagePushBuffer: GenericTraceModule::MassageAndInsertSubdeviceMasks()\n");
    AddSubdevMask messageMassager(m_SubdevMaskDataMap,
                                  m_Test->GetBoundGpuDevice()->GetNumSubdevices());
    MassagePushBuffer(messageMassager);

    // Prints push buffer to mods.log for debugging
    PrintPushBuffer("PB2",true);
    return true;
}

namespace {

    class InjectMethods: public Massager
    {
    public:
        InjectMethods(UINT32 injectMethod, UINT32 injectGran)
            : m_InjectCount(0), m_InjectMethod(injectMethod), m_InjectGran(injectGran) { }
        virtual void Massage(PushBufferMessage& message)
        {
            // Remember original message
            vector<UINT32> newMessageBuf = message.GetMessageChunk();

            // Check if this is the right place to insert a new message
            m_InjectCount += message.GetPayloadSize();
            if (m_InjectCount >= m_InjectGran)
            {
                m_InjectCount = 0;

                const UINT32 newHdr =
                    (REF_NUM(LW_FIFO_DMA_OPCODE,
                         (LW_FIFO_DMA_OPCODE_METHOD))          |
                     REF_NUM(LW_FIFO_DMA_METHOD_COUNT, 1)      |
                     REF_NUM(LW_FIFO_DMA_METHOD_SUBCHANNEL, 0) |
                     REF_NUM(LW_FIFO_DMA_METHOD_ADDRESS,
                         (m_InjectMethod >> 2))                |
                     REF_NUM(LW_FIFO_DMA_OPCODE2,
                         LW_FIFO_DMA_OPCODE2_NONE));

                DebugPrintf(MSGID(Massage), "Inserting @%04x Hdr=%08x, mthd %04x"
                    "count %04x data %08x\n",
                    message.GetPushBufferOffset() + sizeof(UINT32)*message.GetMessageSize(),
                    newHdr,
                    m_InjectMethod,
                    1,
                    0);

                // Inject the new message
                newMessageBuf.push_back(newHdr);
                newMessageBuf.push_back(0);
            }

            // Replace original message with new set of methods
            message.ReplaceMessage(newMessageBuf);
        }

    private:
        UINT32 m_InjectCount;
        UINT32 m_InjectMethod;
        UINT32 m_InjectGran;
    };

}

//---------------------------------------------------------------------------
// This version allows one to add (or subtract) methods
// to/from the push buffer.  Its in addition to the other version.
// Must be called after re-locations
// Right now, its very specific to adding methods for viewport
// offset and scale. It can be made more generic but ideally,
// we should re-architect this and the other massage functions.
bool GenericTraceModule::MassageAndAddPushBuffer(GpuTrace *trace,
                                          const ArgReader *params)
{
    UINT32 inject_method = 0;
    UINT32 inject_method_gran = 0;

    if(params->ParamPresent("-inject_method"))
    {
        inject_method = params->ParamUnsigned("-inject_method");
        inject_method_gran = 10;
        if(params->ParamPresent("-inject_method_gran"))
        {
            inject_method_gran = params->ParamUnsigned("-inject_method_gran");
        }
        InfoPrintf("will inject method 0x%x into pushbuffer every %d methods\n", inject_method, inject_method_gran);
    }
    if (inject_method_gran == 0)
    {
        return true;
    }

    DebugPrintf(MSGID(Massage), "MassagePushBuffer: GenericTraceModule::MassageAndAddPushBuffer()\n");
    InjectMethods messageMassager(inject_method, inject_method_gran);
    MassagePushBuffer(messageMassager);
    return true;
}

bool GenericTraceModule::SetTextureParams(const TextureHeaderState *textureState,
                                          const ArgReader *params)
{
    TextureHeader* header = TextureHeader::CreateTextureHeader(textureState,
        GetGpuDev()->GetSubdevice(0), m_Test->GetTextureHeaderFormat());

    if (params->ParamPresent("-optimal_block_size"))
        header->FixBlockSize();

    if (header->SanityCheck())
    {
        m_TxParams = header;
        return true;
    }
    else
        return false;
}

void GenericTraceModule::SetSwapInfo(UINT32 swap_size)
{
    m_SwapSize = swap_size;
}

bool GenericTraceModule::SetCrcCheckMatch(bool match)
{
    m_CrcCheckMatch = match;
    return true;
}

bool GenericTraceModule::SetCheckInfo(const ModCheckInfo& checkInfo)
{
    if (m_Check != NO_CHECK)
    {
        // Error in trace, this should not be called twice.
        return false;
    }

    m_Check = checkInfo.CheckMethod;
    m_CheckFilename = checkInfo.CheckFilename;
    m_CheckChannel = checkInfo.pTraceCh;
    m_CheckOffset = checkInfo.Offset;
    m_CheckSize = checkInfo.Size;
    m_RawCrc = checkInfo.isRawCRC;

    return true;
}

bool GenericTraceModule::GetCheckInfo
(
    ModCheckInfo& checkInfo
)
{
    checkInfo.CheckFilename = m_CheckFilename;
    checkInfo.pTraceCh = GetCheckChannel();
    checkInfo.Offset = m_CheckOffset;
    checkInfo.Size = m_CheckSize;
    checkInfo.CheckMethod = m_Check;
    checkInfo.isRawCRC = m_RawCrc;

    return true;
}

bool   GenericTraceModule::SetHeapAttr(UINT32 attr)
{
    if (m_HeapAttr || m_TxParams)
       return true;

    m_HeapAttr = attr;
    return false;
}

bool GenericTraceModule::SetHeapAttr2(UINT32 attr2)
{
    return false; // Not imptemented yet
}

bool GenericTraceModule::HasAttrSet() const
{
    return (m_AttrOverride.first || m_TxParams || (m_HeapAttr != 0));
}

UINT32 GenericTraceModule::GetHeapAttr() const
{
    if (m_AttrOverride.first)
        return m_AttrOverride.second;
    else
        return m_TxParams ? m_TxParams->GetHeapAttr() : m_HeapAttr;
}

bool GenericTraceModule::HasAttr2Set() const
{
    return m_Attr2Override.first;
}

UINT32 GenericTraceModule::GetHeapAttr2() const
{
    if (m_Attr2Override.first)
        return m_Attr2Override.second;
    else
        return 0;
}

UINT32 GenericTraceModule::GetHeapType() const
{
    if (m_TypeOverride.first)
        return m_TypeOverride.second;
    else
        return m_TxParams ? m_TxParams->GetHeapType() : 0;
}

void GenericTraceModule::SetImageRTTCompressed(bool compressed)
{
    m_ImageCompressed = compressed;
}

void GenericTraceModule::SetDepthRTTCompressed(bool compressed)
{
    m_DepthCompressed = compressed;
}

bool GenericTraceModule::GetCompressed() const
{
    MASSERT(m_TxParams);

    if (!m_TxParams->Compressible())
        return false;

    switch(m_TxParams->GetHeapType())
    {
        case LWOS32_TYPE_IMAGE:
            return m_ImageCompressed && m_Protect == Memory::ReadWrite;
        case LWOS32_TYPE_DEPTH:
            return m_DepthCompressed && m_Protect == Memory::ReadWrite;
        default:
            MASSERT(0);
    }
    return false;
}

//---------------------------------------------------------------------------
void GenericTraceModule::DoSkipGeometry()
{
    MASSERT(m_FileType == GpuTrace::FT_PUSHBUFFER);

    int start_geometry, end_geometry;

    if (params->ParamPresent("-alias_startGeometry"))
    {
        start_geometry = params->ParamUnsigned("-alias_startGeometry");
    }
    else
    {
        return;
    }

    if (params->ParamPresent("-endGeometry"))
    {
        end_geometry = params->ParamUnsigned("-endGeometry");
    }
    else
        end_geometry = 0x7ffffff;

    DebugPrintf(MSGID(TraceMod), "Geometry from %d to %d\n", 0, start_geometry, end_geometry);
    return SkipFermiGeometry(start_geometry, end_geometry);
}

void GenericTraceModule::DoSkipComputeGrid()
{
    MASSERT(m_FileType == GpuTrace::FT_PUSHBUFFER);

    // end_grid by default is largest possible value so that
    // -start_cta_grid can be specified on cmdline without requiring
    // -end_cta_grid
    //
    UINT32 end_grid = 0xFFFFFFFF;
    UINT32 start_grid;

    if (!params->ParamPresent("-start_cta_grid"))
        return;

    start_grid = params->ParamUnsigned("-start_cta_grid");
    if (params->ParamPresent("-end_cta_grid"))
        end_grid = params->ParamUnsigned("-end_cta_grid");

    DebugPrintf(MSGID(TraceMod), "DoSkipComputeGrid: running Compute Grid start/end pairs from "
        "#%u through and including #%u (counting starts at 0)\n", start_grid, end_grid);

    NullifyBeginEndMethodsFermi(FERMI_COMPUTE_A, LW90C0_BEGIN_GRID,
                                LW90C0_END_GRID, LW90C0_PIPE_NOP,
                                start_grid, end_grid);
}

//------------------------------------------------------------------------------
void GenericTraceModule::AddZLwllId(UINT32 id)
{
    if (id == UINT32(-1))
    {
        // Invalid id -- do nothing
        return;
    }

    m_Trace->AddZLwllId(id);

    if (m_Test->GetGpuResource()->PerfMon())
    {
        PerformanceMonitor::AddZLwllId(id);
    }
}

//------------------------------------------------------------------------------
RC GenericTraceModule::SetGeometry(const vector<UINT32>& geometry)
{
    m_Geometry = geometry;
    UINT32 bl_width, bl_height, bl_depth;
    char gmem_id;

    UINT32 gpuSubdevIdx = 0;

    switch (m_Geometry.size())
    {
        case 1:
            m_CachedSurface->SetSize(gpuSubdevIdx, m_Geometry[0]);
            break;
        case 2:
            bl_width =  params->ParamUnsigned("-block_width", 1);
            bl_height = params->ParamUnsigned("-block_height", 4);
            bl_depth =  params->ParamUnsigned("-block_depth", 1);

            gmem_id = 'A' + m_FileType - GpuTrace::FT_GMEM_A;
            bl_width =  params->ParamUnsigned((string("-block_width_") + gmem_id).c_str(), bl_width);
            bl_height = params->ParamUnsigned((string("-block_height_") + gmem_id).c_str(), bl_height);
            bl_depth =  params->ParamUnsigned((string("-block_depth_") + gmem_id).c_str(), bl_depth);

            if ((bl_width & (bl_width-1)) != 0)
            {
                ErrPrintf("Width of the block should be a power of 2\n");
                return RC::ILWALID_FILE_FORMAT;
            }

            if ((bl_height & (bl_height-1)) != 0)
            {
                ErrPrintf("Height of the block should be a power of 2\n");
                return RC::ILWALID_FILE_FORMAT;
            }

            if ((bl_depth & (bl_depth-1)) != 0)
            {
                ErrPrintf("Depth of the block should be a power of 2\n");
                return RC::ILWALID_FILE_FORMAT;
            }

            m_Bl = new BlockLinear(m_Geometry[0], m_Geometry[1],
                                   log2(bl_width), log2(bl_height),
                                   log2(bl_depth), 1,
                                   m_Test->GetBoundGpuDevice());
            m_CachedSurface->SetSize(gpuSubdevIdx, m_Bl->Size());
            break;
        default:
            ErrPrintf("%s has undefined size. Use SIZE keyword for that\n",
                    GetName().c_str());
            return RC::ILWALID_FILE_FORMAT;
    }
    return OK;
}

//------------------------------------------------------------------------------
void GenericTraceModule::PrintMethodSegmentError
(
    TraceModule::SegmentH   segH,
    const char *            errMsg
)
{
    ErrPrintf("Pushbuffer segment %d is invalid: %s\n",
            segH, errMsg);
    ErrPrintf(" Start=0x%x Size=0x%x\n",
            m_SendSegments[segH].Start,
            m_SendSegments[segH].Size);
}

//------------------------------------------------------------------------------
RC GenericTraceModule::AddMethodSegment
(
    UINT32 start,
    UINT32 size,
    TraceModule::SegmentH * pSegmentH
)
{
    MASSERT(m_FileType == GpuTrace::FT_PUSHBUFFER);
    MASSERT(pSegmentH);

    size_t i;
    for (i = 0; i < m_SendSegments.size(); ++i)
    {
        if ((m_SendSegments[i].Start == start) &&
            (m_SendSegments[i].Size  == size))
        {
            // Allow this TraceOpSendMethods to re-send the methods,
            // the segment matches the previous segment boundaries.
            *pSegmentH = i;
            return OK;
        }
    }

    // Else, we must add a new segment.
    SendSegment s;
    s.Start             = start;
    s.Size              = size;

    m_SendSegments.push_back(s);

    // We assume here and elsewhere that method segments will be added first
    // to last, and will 100% cover all our data, and will start and stop
    // on multiples of 4 bytes.  Enforce this!

    bool segmentsAreOK = true;
    UINT32 prevEndingOffset = 0;
    for (i = 0; i < m_SendSegments.size(); ++i)
    {
        if (0 == m_SendSegments[i].Size)
        {
            PrintMethodSegmentError(i, "size is 0");
            segmentsAreOK = false;
        }
        if (m_SendSegments[i].Size & 0x3)
        {
            PrintMethodSegmentError(i, "size is not 4-byte aligned");
            segmentsAreOK = false;
        }
        if (m_SendSegments[i].Start != prevEndingOffset)
        {
            PrintMethodSegmentError(i, "start != previous segment end");
            segmentsAreOK = false;
        }
        prevEndingOffset += m_SendSegments[i].Size;
    }
    if (segmentsAreOK)
    {
        *pSegmentH = m_SendSegments.size() - 1; // index of new segment
        return OK;
    }

    return RC::BAD_PARAMETER;
}

// @@@ extract the header-parsing code to an object, it is used 5 places
//     inside TraceModule, no need to track across gpentries for now
//     -- put this off for now, not needed in initial checkin!
//
// Assumptions:
// Pushbuffer modules will be 100% sent in 1 to N chunks, in order.
// A chunk always starts at a message header.
// A chunk ends at the end of an entire message.
//  - except when a gpfifo entry follows the chunk, in which case the chunk
//    ends after the message header.
// (@@@ can we enforce this?)
//
// Trace rewriting:
// Iterate over each message, skipping partial messages that are just before
// a GpEntry.
//   a) each method,data pair is sent through MassageTeslaMethod (or equivalent)
//      - data may be changed
//      - Trace3DState is updated
//      - before surface allocations, discover some surfaces used here
//   b) Apply relocs (modify method data for runtime data)
//     - after surface allocations
//   c) Apply RELOC_RESET_VALUE relocs (why not in step b?)
//   d) -inject_method (add meth,data pair every N methods)
//      increases size of data, ilwalidates old offsets in TraceOpSendMethods
//   e) -alias_startGeometry -endGeometry
//      change some SetBeginEnds to END
//   f) debug_init (gdb hackery to skip some initial methods and colwert other
//      to single-method sends (during playback)
//   g) for some PM modes and -single_method and debug_init
//      colwert multi-method sends to single-method sends (during playback)

//------------------------------------------------------------------------------
RC GenericTraceModule::SendMethodSegment(SegmentH h)
{
    MASSERT(m_FileType == GpuTrace::FT_PUSHBUFFER);
    MASSERT(h < m_SendSegments.size());
    return SendFermiMethodSegment(h);
}

void GenericTraceModule::SetSize(UINT64 size)
{
    for (size_t dev = 0; dev < m_CachedSurface->GetSubdeviceNumber(); ++dev)
    {
        m_CachedSurface->SetSize(dev, size);
    }
}

RC GenericTraceModule::SetSizeByArg(UINT64 size)
{
    // Add this routine for setting lmem size
    // Fermi has special size/alignment requirement
    // Round up to 128K
    size = (size + 0x0001FFFF) & 0xFFFFFFFFFFFE0000ULL;
    InfoPrintf("Local memory size is set to be 0x%x, any other lmem hint "
        "options are overrided\n", size);
    SetSize(size);
    return OK;
}

RC GenericTraceModule::SetSizePerWarp
(
    UINT64 size,
    shared_ptr<SubCtx> pSubCtx
)
{
    UINT32 warpsPerTpc = 0;
    if (params->ParamPresent("-force_warp_count_for_lmem_size"))
        warpsPerTpc = params->ParamUnsigned("-force_warp_count_for_lmem_size");
    else
        warpsPerTpc = GetWarpsPerTpc();

    UINT32 numTPC = 0;
    if (params->ParamPresent("-force_sm_count_for_lmem_size"))
        numTPC = params->ParamUnsigned("-force_sm_count_for_lmem_size");
    else
    {
        numTPC = GetTpcNum();
        ScalePartitionTPCnumber(&numTPC, pSubCtx);
    }

    DebugPrintf("GenericTraceModule::SetSizePerWarp:  warpsPerTpc = %u  numTpc = %u\n", warpsPerTpc, numTPC);

    SetSize( ScaleSizeByArgs(size * warpsPerTpc * numTPC) );
    return OK;
}

RC GenericTraceModule::SetSizePerTPC
(
    UINT64 size,
    shared_ptr<SubCtx> pSubCtx
)
{
    UINT32 numTPC = 0;
    if (params->ParamPresent("-force_sm_count_for_lmem_size"))
        numTPC = params->ParamUnsigned("-force_sm_count_for_lmem_size");
    else
    {
        numTPC = GetTpcNum();
        ScalePartitionTPCnumber(&numTPC, pSubCtx);
    }

    DebugPrintf("GenericTraceModule::SetSizePerTPC:  numTpc = %u\n", numTPC);

    SetSize( ScaleSizeByArgs(size * numTPC) );
    return OK;
}

UINT64 GenericTraceModule::ScaleSizeByArgs(UINT64 size) const
{
    size *= params->ParamUnsigned("-scale_up_lmem_by_factor", 1);
    size /= params->ParamUnsigned("-scale_down_lmem_by_factor", 1);

    // Scale down the size based on thread ID throttling
    if (!params->ParamPresent("-thread_mem_throttle_method_hw"))
    {
        size /= (32/params->ParamUnsigned("-thread_mem_throttle", 24));
    }

    if (params->ParamPresent("-no_colwoys"))
    {
        size /= 2;
    }

    // Fermi has special size/alignment requirement
    // Round up to 128K
    size = (size + 0x0001FFFF) & 0xFFFFFFFFFFFE0000ULL;

    DebugPrintf("GenericTraceModule::ScaleSizeByArgs:  size = 0x%llx\n", size);

    return size;
}

UINT32 GenericTraceModule::GetTextureIndex(UINT32 num) const
{
    MASSERT( num < m_TextureIndexList.size() );
    return m_TextureIndexList[num];
}

// ----------------------------------------------------------------------------
// Setup the set of peer GpuDevices that require access to this module
void GenericTraceModule::AddPeerGpuMappings(set<GpuDevice *> *pGpuPeers)
{
    m_pGpuPeerMappings = pGpuPeers;
}

// ----------------------------------------------------------------------------
// Peer map the contained dmabuffer to any GpuDevices that require access to it
// If a GpuDevice in the set to map to is the GpuDevice that this module is
// allocated on, then a loopback mapping is created
RC GenericTraceModule::MapPeers()
{
    if (m_pGpuPeerMappings == NULL)
        return OK;

    set<GpuDevice *>::iterator devIter = m_pGpuPeerMappings->begin();
    RC rc;

    while (devIter != m_pGpuPeerMappings->end())
    {
        if (m_pDmaBuf->GetLocation() == Memory::Fb)
        {
            if (*devIter == GetGpuDev())
            {
                vector<UINT32> peerIDs = GetPeerIDs();
                vector<UINT32>::iterator iter = peerIDs.begin();
                while (iter != peerIDs.end())
                {
                    CHECK_RC(m_pDmaBuf->MapLoopback(*iter));
                    iter++;
                }
            }
            else
            {
                CHECK_RC(m_pDmaBuf->MapPeer(*devIter));
            }
        }
        else if (*devIter != GetGpuDev())
        {
            CHECK_RC(m_pDmaBuf->MapShared(*devIter));
        }
        devIter++;
    }

    return OK;
}

// ChannelHasHwClass returns true if this trace module's channel contains
// an instance of class hwClass.   If the channel has multiple subchannels
// then subch is written with the subchannel of hwClass.  subch is
// set to an illegal subchannel value if the channel contains just one
// subchannel.  subchValid is set to false unless the subchannel is returned
// with a valid subchannel number.
//
bool GenericTraceModule::ChannelHasHwClass
(
    UINT32 hwClass,
    UINT32 *subch,
    bool *subchValid
)
{
    static const UINT32 numSubchannels = 8;
    static const UINT32 ilwalidSubchannel = 0xFFFFFFFF;

    *subch = ilwalidSubchannel;
    *subchValid = false;

    if (m_pTraceChannel->HasMultipleSubChs())
    {
        for (UINT32 sc = 0; sc < numSubchannels; ++sc)
        {
            UINT32 hwClassInSubch = m_pTraceChannel->GetTraceSubChannelHwClass(sc);
            if (hwClassInSubch == hwClass)
            {
                *subch = sc;
                *subchValid = true;
                return true;
            }
        }
    }
    else
    {
        // pushbuffer has single subchannel
        //
        TraceSubChannel * ptracesubch = m_pTraceChannel->GetSubChannel("");
        UINT32 subchHwClass = ptracesubch->GetClass();
        if (subchHwClass == hwClass)
            return true;
    }

    // didn't find hwClass in this channel
    //
    return false;
}

void GenericTraceModule::SaveTrace3DSurface(MdiagSurf *surface)
{
    m_pDmaBuf = surface;
}

SurfaceTraceModule::SurfaceTraceModule
(
    Trace3DTest *test,
    string moduleName,
    string fileName,
    const MdiagSurf &surface,
    size_t fileSize,
    SurfaceType surfaceType
)
:   GenericTraceModule(test, fileName, GpuTrace::FT_ALLOC_SURFACE, fileSize),
    m_ParameterSurface(surface),
    m_SurfaceType(surfaceType),
    m_Class(CrcChainManager::UNSPECIFIED_CLASSNUM),
    m_FillPresent(false)
{
    m_ModuleName = moduleName;
    m_FormatHelper = 0;
    if (surface.GetWidth() > 0 && surface.GetHeight() > 0)
    {
        // Only create SurfaceFormat object when the surface has diemension
        // information, otherwise it should be a normal buffer
        CreateFormatHelper();
    }
}

SurfaceTraceModule::~SurfaceTraceModule()
{
    if (m_FormatHelper)
    {
        delete m_FormatHelper;
        m_FormatHelper = 0;
    }
}

RC SurfaceTraceModule::CreateFormatHelper()
{
    m_FormatHelper = SurfaceFormat::CreateFormat(
        m_ParameterSurface.GetSurf2D(), GetGpuDev());
    if (0 == m_FormatHelper)
    {
        ErrPrintf("%s: Create surface format object failed\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC SurfaceTraceModule::Allocate()
{
    if (IsSurfaceTypeColorZ3D(GetSurfaceType()))
    {
        IGpuSurfaceMgr *surfaceMgr = m_Test->GetSurfaceMgr();

        m_pDmaBuf = surfaceMgr->GetSurface(GetSurfaceType());

        MASSERT(m_pDmaBuf != 0);

        UpdateOffset();
    }
    else
    {
        RC rc;

        if(!params->ParamPresent("-silence_surface_info"))
            InfoPrintf("Allocating (%s)...\n", m_ModuleName.c_str());

        bool allocSeparate = false;

        UINT32 gpuSubdevIdx = 0;
        UINT64 surfaceSize = m_CachedSurface->GetSize(gpuSubdevIdx);

        // First, alloc the space we need
        if (!m_Test->TraceAllocate(this, m_FileType, surfaceSize,
                &m_pDmaBuf, &m_BufOffset, &allocSeparate))
        {
            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        // Peer map the contained dma buffer to any peer GpuDevices
        // if necessary
        CHECK_RC(MapPeers());

        UpdateOffset();

        AllocateForSubdevices(surfaceSize, allocSeparate);

        m_Test->GetBuffInfo()->SetDescription(GpuTrace::GetFileTypeData(m_FileType).AllocTableTypeName);

        DebugPrintf(MSGID(TraceMod), "\n");
    }

    return OK;
}

void SurfaceTraceModule::UpdateOffset()
{
    if (IsSurfaceTypeColorZ3D(GetSurfaceType()))
    {
        MASSERT(m_pDmaBuf != 0);
        m_Offset = m_pDmaBuf->GetCtxDmaOffsetGpu()
            + m_pDmaBuf->GetExtraAllocSize();
    }
    else
    {
        // Fill in some of the surface's parameters
        m_Offset = m_pDmaBuf->GetCtxDmaOffsetGpu()
            + m_pDmaBuf->GetExtraAllocSize() + m_BufOffset;

        DebugPrintf(MSGID(TraceMod), "BufOffset = 0x%08X\n", m_BufOffset);

        DebugPrintf(MSGID(TraceMod), "[hCtxDma,Offset] = [0x%08X,0x%llX]\n",
            m_pDmaBuf->GetCtxDmaHandle(), m_Offset);

        DebugPrintf(MSGID(TraceMod), " [hCtxDma,Offset,hMem] = [0x%08x,0x%llx,0x%08x]",
            m_pDmaBuf->GetCtxDmaHandle(), m_Offset, m_pDmaBuf->GetMemHandle());
    }
}

RC SurfaceTraceModule::LoadFromFile(TraceFileMgr *pTraceFileMgr)
{
    RC rc = OK;

    if (!m_FileToLoad.empty())
    {
        if (m_CachedSurface->IsInLazyLoadStatus())
        {
            return OK;
        }

        rc = GenericTraceModule::LoadFromFile(pTraceFileMgr);

        if ((rc == OK) &&  !m_DownloadRaw)
        {
            UINT32 width  = m_ParameterSurface.GetWidth();  // AA scaled
            UINT32 height = m_ParameterSurface.GetHeight(); // AA scaled
            UINT32 depth  = m_ParameterSurface.GetDepth();

            MASSERT(m_Geometry.size() == 0);
            m_Geometry.push_back(width);
            m_Geometry.push_back(height);
            m_Geometry.push_back(depth);

            UINT32 gpuSubdevIdx = 0;
            UINT32 *sourceData = m_CachedSurface->GetPtr(gpuSubdevIdx);
            UINT32 sourceSize = m_CachedSurface->GetSize(gpuSubdevIdx);
            MASSERT(sourceData != 0);
            if (m_FormatHelper)
            {
                // The colwerted data buffer is allocated by SurfaceFormat class,
                // CachedSurface needs to acquire the ownership and be responsible
                // for releasing it.
                if (!m_FormatHelper->DoLayout((UINT08*)sourceData, sourceSize))
                {
                    ErrPrintf("DoLayout() failed for %s\n", GetName().c_str());
                    return RC::SOFTWARE_ERROR;
                }
                m_CachedSurface->Renew(0,
                    m_FormatHelper->ReleaseOwnership(),
                    m_FormatHelper->GetSize());
                m_CheckSize = m_FormatHelper->GetUnLayoutSize();
            }
            else
            {
                // For normal buffers, we don't have to do layout and copying all
                // data from cached surface is good enough
            }
        }
    }

    return rc;
}

UINT64 SurfaceTraceModule::GetSize() const
{
    UINT64 size = 0;
    UINT32 gpuSubdevIdx = 0;

    if (m_CachedSurface)
    {
        size = m_CachedSurface->GetSize(gpuSubdevIdx);
    }

    if ((size == 0) && (m_pDmaBuf != 0))
    {
        size = m_pDmaBuf->GetSize();
    }

    if (m_FormatHelper && m_FormatHelper->GetSize() > 0)
    {
        // For most cases, the callwlated size should greater equal than the
        // file size in trace, since trace file is stored in naive pitch but
        // MODS needs to add padding to the allocated surface whatever it's in
        // pitch or blocklinear layout.
        // An exception case is the trace file size is greater than the
        // callwlated size based on the dimension info, which probably means
        // trace wants to download raw image to the allocated surface, we need
        // to respect the size of the trace file. Therefore, here we return
        // the bigger size.
        size = max(size, m_FormatHelper->GetSize());
    }

    return size;
}

RC SurfaceTraceModule::Download()
{
    RC rc;

    const MdiagSurf *surface = GetDmaBuffer();
    MASSERT(surface != 0);

    // Now fill the cached surface.
    if (m_FillPresent)
    {
        UINT64 fillSize = surface->GetSize();

        //fix bug 1943547, BL surface need to be aligned to gob boundary
        if (surface->GetLayout() == Surface2D::BlockLinear)
        {
            UINT64 alignMask = GetGpuDev()->GobSize() - 1;
            fillSize += alignMask;
            fillSize &= ~alignMask;
        }

        FillCachedSurface(fillSize);
    }

    UINT32 *surfaceData = m_CachedSurface->GetPtr(0);
    UINT64 cachedSurfaceSize = m_CachedSurface->GetSize(0);

    if (m_CachedSurface->GetCopyNumber() == 1)
    {
        m_CachedSurface->CopyAll(surfaceData, cachedSurfaceSize);
    }

    if (surface->IsVirtualOnly())
    {
        // Virtual allocations can have files associated with them,
        // but these files are not loaded into memory until the
        // corresponding map(s) are processed.
        return rc;
    }

    for (UINT32 gpuSubdevIdx = 0;
        gpuSubdevIdx < m_CachedSurface->GetSubdeviceNumber();
        ++gpuSubdevIdx)
    {
        DebugPrintf(MSGID(TraceMod), "SurfaceTraceModule::%s: gpuSubdevIdx= %u (name= %s)\n",
            __FUNCTION__, gpuSubdevIdx, GetName().c_str());

        cachedSurfaceSize = m_CachedSurface->GetSize(gpuSubdevIdx);

        if (m_Check != NO_CHECK)
        {
            if ((0 == m_CheckSize) ||
                (m_CheckSize > cachedSurfaceSize - m_CheckOffset))
            {
                m_CheckSize = cachedSurfaceSize - m_CheckOffset;
            }

            if (!gpuSubdevIdx)
            {
                if (GetCheckChannel()->GetVASpaceHandle() != GetVASpaceHandle())
                {
                    return RC::SOFTWARE_ERROR;
                }
                CHECK_RC(GetCheckChannel()->AddAllocSurface(m_pDmaBuf,
                    m_CheckFilename, m_Check, m_CheckOffset,
                    m_CheckSize, m_SwapSize, m_CrcCheckMatch, m_CrcRect,
                    m_RawCrc, m_Class));
            }
        }

        if (m_FileToLoad.empty() && !m_FillPresent)
        {
            // No need to download any data for an ALLOC_SURFACE buffer
            // that doesn't have either a FILE or a FILL* property.
        }
        else if ((m_FileType == GpuTrace::FT_ALLOC_SURFACE) && m_MapToBackingStore)
        {
            InfoPrintf("texture buffer %s maps to backing store, skipping loading\n",
                       GetName().c_str());
        }
        else
        {
            UINT64 downloadSize = cachedSurfaceSize;

            // The cached size might be greater than the size
            // of the surface because it is legal for an initialization
            // file to be larger than the surface size.  In this case
            // the extra bytes of the cached surface should be ignored.
            if (downloadSize > surface->GetSize())
            {
                downloadSize = surface->GetSize();
            }

            CHECK_RC(DownloadData2SubDev(downloadSize, gpuSubdevIdx));
            m_CachedSurface->SetWasDownloaded();

            // Dealloc the cached surface to save some memory
            if (params->ParamPresent("-disable_memory_save_mode") == 0)
            {
                DebugPrintf(MSGID(TraceMod), "SurfaceTraceModule::%s: Release CachedSurface copy "
                        "for subdevIdx= %u (name= %s) to save memory.\n",
                        __FUNCTION__, gpuSubdevIdx, GetName().c_str());
                ReleaseCachedSurface(gpuSubdevIdx);
            }
        }
    }

    return OK;
}

void SurfaceTraceModule::SetFillValue(UINT32 val)
{
    m_FillPresent = true;
    m_FillValue = val;
}

void SurfaceTraceModule::FillCachedSurface(UINT64 surfaceSize)
{
    UINT32 gpuSubdevIdx = 0;

    UINT32* newData = new UINT32[(surfaceSize + sizeof(UINT32) - 1) / sizeof(UINT32)];
    fill(newData, newData + (surfaceSize + sizeof(UINT32) - 1) / sizeof(UINT32), m_FillValue);

    m_CachedSurface->Renew(gpuSubdevIdx, newData, surfaceSize);
    m_CachedSurface->SetIsFilled(true);
}

void SurfaceTraceModule::ScaleByTPCCount(UINT32 smCount)
{
    // for historical reason we use SM and TPC interchangeably when we scale memory
    if (smCount == 0)
    {
        // scale based on chip configuration
        SetSize(GetSize() * GetTpcNum());
    }
    else
    {
        //overriden by test
        SetSize(GetSize() * smCount);
    }

}

ViewTraceModule::ViewTraceModule
(
    Trace3DTest *test,
    string moduleName,
    string fileName,
    const MdiagSurf &surface,
    size_t fileSize,
    string parentModuleName,
    SurfaceType surfaceType
)
:   SurfaceTraceModule(test, moduleName, fileName, surface, fileSize,
    surfaceType)
{
    m_ParentModuleName = parentModuleName;
}

RC ViewTraceModule::Allocate()
{
    InfoPrintf("Allocating view (%s)...\n", m_ModuleName.c_str());

    MdiagSurf *parentSurface = GetParentSurface();

    if (parentSurface == 0)
    {
        ErrPrintf("PARENT_SURFACE %s of SURFACE_VIEW %s does not exist.\n",
            m_ParentModuleName.c_str(), m_ModuleName.c_str());
        return RC::SOFTWARE_ERROR;
    }

    if (IsSurfaceTypeColorZ3D(GetSurfaceType()))
    {
        IGpuSurfaceMgr *surfaceMgr = m_Test->GetSurfaceMgr();

        m_pDmaBuf = surfaceMgr->GetSurface(GetSurfaceType());

        MASSERT(m_pDmaBuf != 0);
    }
    else
    {
        m_pDmaBuf = GetParameterSurface();
    }

    m_pDmaBuf->SetSurfaceViewParent(parentSurface, m_BufOffset);

    m_pDmaBuf->SetLocation(parentSurface->GetLocation());

    // Fill in some of the surface's parameters
    m_Offset = m_pDmaBuf->GetCtxDmaOffsetGpu()
        + m_pDmaBuf->GetExtraAllocSize();

    TraceModule *parentModule = GetParentModule();

    parentModule->AddSurfaceView(this);

    m_pDmaBuf->SetPteKind(parentSurface->GetPteKind());
    m_pDmaBuf->SetProtect(parentSurface->GetProtect());

    AllocateForSubdevices(m_pDmaBuf->GetSize(), false);
    m_Test->GetBuffInfo()->SetDescription(GpuTrace::GetFileTypeData(m_FileType).AllocTableTypeName);

    return OK;
}

TraceModule *ViewTraceModule::GetParentModule()
{
    return m_Trace->ModFind(m_ParentModuleName.c_str());
}

MdiagSurf *ViewTraceModule::GetParentSurface()
{
    TraceModule *module = GetParentModule();

    if (module == 0)
    {
        return 0;
    }

    return module->GetDmaBufferNonConst();
}

//--------------------------------------------------------------------------
// This function is for notifying any objects depending on surface view
// that the surface view is no longer active (i.e., it's parent surface
// has been freed).
void ViewTraceModule::Release()
{
    // Tell GpuVerif that this surface view should no longer be
    // considiered for CRC checking.
    DisconnectFromGpuVerif();

    // If the surface view is a color/z, let the surface manager know
    // that it is no longer valid.
    if (IsColorOrZ())
    {
        m_Test->GetSurfaceMgr()->DisableSurface(GetSurfaceType());
    }
}

MapTraceModule::MapTraceModule
(
    Trace3DTest *test,
    string moduleName,
    string fileName,
    const MdiagSurf &surface,
    size_t fileSize,
    SurfaceType surfaceType,
    const string &virtAllocName,
    const string &physAllocName,
    UINT64 virtualOffset,
    UINT64 physicalOffset
)
:   SurfaceTraceModule(test, moduleName, fileName, surface, fileSize,
    surfaceType),
    m_VirtAllocName(virtAllocName),
    m_PhysAllocName(physAllocName),
    m_VirtualOffset(virtualOffset),
    m_PhysicalOffset(physicalOffset)
{
    m_ModuleName = moduleName;
}

RC MapTraceModule::AllocateSurface(MdiagSurf *surface, GpuDevice *gpudev)
{
    RC rc = OK;

    SurfaceTraceModule *virtModule = (SurfaceTraceModule*) m_Trace->ModFind(m_VirtAllocName.c_str());

    if (virtModule == 0)
    {
        ErrPrintf("No ALLOC_VIRTUAL command with name \'%s\' exists.\n",
            m_VirtAllocName.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    MdiagSurf *virtAlloc = virtModule->GetDmaBufferNonConst();

    MASSERT(virtAlloc != 0);

    if (!virtAlloc->IsVirtualOnly())
    {
        ErrPrintf("\'%s\' is not the name of an ALLOC_VIRTUAL command.\n",
            m_VirtAllocName.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    TraceModule *physModule = m_Trace->ModFind(m_PhysAllocName.c_str());

    if (physModule == 0)
    {
        ErrPrintf("No ALLOC_PHYSICAL command with name \'%s\' exists.\n",
            m_PhysAllocName.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    MdiagSurf *physAlloc = physModule->GetDmaBufferNonConst();

    MASSERT(physAlloc != 0);

    if (!physAlloc->HasPhysical())
    {
        ErrPrintf("\'%s\' is not the name of an ALLOC_PHYSICAL or ALLOC_SURFACE command.\n",
            m_PhysAllocName.c_str());

        return RC::ILWALID_FILE_FORMAT;
    }

    UINT64 virtualOffset = GetVirtualOffset(virtModule);

    CHECK_RC(surface->MapVirtToPhys(gpudev, virtAlloc, physAlloc, virtualOffset, m_PhysicalOffset, m_Test->GetLwRmPtr()));

    if (m_pTraceChannel)
    {
        surface->SetChannel(m_pTraceChannel->GetCh());
    }
    m_BufOffset = virtualOffset;

    return rc;
}

RC MapTraceModule::Download()
{
    const MdiagSurf *surface = GetDmaBuffer();
    MASSERT(surface != 0);
    UINT64 surfaceSize = surface->GetAllocSize();

    SurfaceTraceModule *virtModule = (SurfaceTraceModule *) m_Trace->ModFind(m_VirtAllocName.c_str());

    if (!virtModule->GetFilename().empty())
    {
        if (!m_FileToLoad.empty())
        {
            ErrPrintf("Can not use FILE property on both ALLOC_VIRTUAL %s and MAP %s.\n",
                virtModule->GetName().c_str(), GetName().c_str());

            return RC::ILWALID_FILE_FORMAT;
        }

        UINT64 offset = GetVirtualOffset(virtModule);
        UINT32 *virtualData = virtModule->GetCachedSurface()->Get032Addr(0, offset);
        ///@todo: use single line StoreInlineData(virtualData, surfaceSize) instead of
        // below new->memcpy->Renew 3 operations upon confirmation
        // if virtModule != *this*.
        // Keep older code prior to that.
        UINT08 *newData = new UINT08 [surfaceSize];

        if (newData == 0)
        {
            ErrPrintf("Can't allocate memory to download buffer %s\n",
                m_ModuleName.c_str());

            return RC::CANNOT_ALLOCATE_MEMORY;
        }

        memcpy(newData, virtualData, surfaceSize);

        m_CachedSurface->Renew(0, newData, surfaceSize);

        // Copy the filename so that the SurfaceTraceModule::Download
        // function doesn't skip downloading for this map.
        m_FileToLoad = virtModule->GetFilename();
    }

    return SurfaceTraceModule::Download();
}

SharedTraceModule::SharedTraceModule
(
    Trace3DTest *test,
    string moduleName,
    string fileName,
    const MdiagSurf &surface,
    size_t fileSize,
    SurfaceType surfaceType
)
:   SurfaceTraceModule(test, moduleName, fileName, surface, fileSize,
    surfaceType)
{
    m_ModuleName = moduleName;
}

RC SharedTraceModule::AllocateSurface(MdiagSurf *surface, GpuDevice *gpudev)
{
    RC rc = OK;

    LWGpuResource * pGpuResource = m_Test->GetGpuResource();
    SharedSurfaceController * pSurfController = pGpuResource->GetSharedSurfaceController();
    surface->SetGpuDev(gpudev);
    CHECK_RC(pSurfController->AllocSharedSurf(surface, m_Test->GetLwRmPtr()));
    if (m_pTraceChannel)
    {
        surface->SetChannel(m_pTraceChannel->GetCh());
    }

    return rc;
}

RC SharedTraceModule::Update
(
    UINT32 Offset, 
    const char *UpdateFilename, 
    TraceFileMgr * pTraceFileMgr
)
{
    RC rc;

    string updateName(UpdateFilename);
    LWGpuResource * pGpuResource = m_Test->GetGpuResource();
    SharedSurfaceController * pSurfController = pGpuResource->GetSharedSurfaceController();
    pSurfController->AcquireMutex(updateName);
    CHECK_RC(GenericTraceModule::Update(Offset, UpdateFilename, pTraceFileMgr));
    pSurfController->ReleaseMutex(updateName);

    return rc;
}

MapTextureSubimageTraceModule::MapTextureSubimageTraceModule
(
    Trace3DTest *test,
    string moduleName,
    string fileName,
    const MdiagSurf &surface,
    size_t fileSize,
    SurfaceType surfaceType,
    const string &virtAllocName,
    const string &physAllocName,
    UINT64 virtualOffset,
    UINT64 physicalOffset,
    UINT32 xOffset,
    UINT32 yOffset,
    UINT32 zOffset,
    UINT32 mipLevel,
    UINT32 dimension,
    UINT32 arrayIndex
)
:   MapTraceModule
    (
        test,
        moduleName,
        fileName,
        surface,
        fileSize,
        surfaceType,
        virtAllocName,
        physAllocName,
        virtualOffset,
        physicalOffset
    ),
    m_XOffset(xOffset),
    m_YOffset(yOffset),
    m_ZOffset(zOffset),
    m_MipLevel(mipLevel),
    m_Dimension(dimension),
    m_ArrayIndex(arrayIndex)
{
}

UINT64 MapTextureSubimageTraceModule::GetVirtualOffset(SurfaceTraceModule* virtModule)
{
    SurfaceFormat* formatHelper = virtModule->GetFormatHelper();
    MASSERT(formatHelper != 0);
    UINT64 offset;

    if (OK != formatHelper->Offset(&offset, m_XOffset, m_YOffset,
                m_ZOffset, m_MipLevel, m_Dimension, m_ArrayIndex))
    {
        MASSERT(!"Failed to get offset in MapTextureSubimageTraceModule");
        return 0xdeadbeefdeadbeefULL;
    }

    return offset;
}

MapTextureMipTailTraceModule::MapTextureMipTailTraceModule
(
    Trace3DTest *test,
    string moduleName,
    string fileName,
    const MdiagSurf &surface,
    size_t fileSize,
    SurfaceType surfaceType,
    const string &virtAllocName,
    const string &physAllocName,
    UINT64 virtualOffset,
    UINT64 physicalOffset,
    UINT32 startingMipLevel,
    UINT32 dimension,
    UINT32 arrayIndex
)
:   MapTraceModule
    (
        test,
        moduleName,
        fileName,
        surface,
        fileSize,
        surfaceType,
        virtAllocName,
        physAllocName,
        virtualOffset,
        physicalOffset
    ),
    m_StartingMipLevel(startingMipLevel),
    m_Dimension(dimension),
    m_ArrayIndex(arrayIndex)
{
}

UINT64 MapTextureMipTailTraceModule::GetVirtualOffset(SurfaceTraceModule* virtModule)
{
    SurfaceFormat* formatHelper = virtModule->GetFormatHelper();
    MASSERT(formatHelper != 0);
    UINT64 offset;

    if (OK != formatHelper->Offset(&offset, 0, 0, 0, m_StartingMipLevel,
                m_Dimension, m_ArrayIndex))
    {
        MASSERT(!"Failed to get offset in MapTextureMipTailTraceModule");
        return 0xdeadbeefdeadbeefULL;
    }

    return offset;
}

UINT32 GenericTraceModule::GetSmPerTpc()
{
    return m_Test->GetGrInfo(LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_SM_PER_TPC, 0);
}

UINT32 GenericTraceModule::GetTpcNum()
{
    return m_Test->GetGrInfo(LW2080_CTRL_GR_INFO_INDEX_SHADER_PIPE_SUB_COUNT, 0);
}

UINT32 GenericTraceModule::GetWarpsPerTpc()
{
    UINT32 smPerTpc = m_Test->GetGrInfo(LW2080_CTRL_GR_INFO_INDEX_LITTER_NUM_SM_PER_TPC, 0);
    return m_Test->GetGrInfo(LW2080_CTRL_GR_INFO_INDEX_MAX_WARPS_PER_SM, 0) * smPerTpc;
}

void GenericTraceModule::ScalePartitionTPCnumber
(
    UINT32 * numTPC,
    shared_ptr<SubCtx> pSubCtx
)
{
    if(params->ParamPresent("-partition_table") &&
        pSubCtx.get())
    {
        UINT32 tpcCount = pSubCtx->GetMaxTpcCount();

        *numTPC = tpcCount;

        DebugPrintf("GenericTraceModule::ScalePartitionTPCnumber: overwrite the tpc number via -partition_table. tpcCount = %u\n", *numTPC);
    }
}

bool GenericTraceModule::SkipInitialization() const
{
    if (m_Trace->GetSkipBufferDownloads())
    {
        return true;
    }
    else  if (params->ParamPresent("-skip_sysmem_init") &&
        m_pDmaBuf->GetLocation() != Memory::Fb)
    {
        return true;
    }
    else if (m_SkipInit)
    {
        return true;
    }

    return false;
}
