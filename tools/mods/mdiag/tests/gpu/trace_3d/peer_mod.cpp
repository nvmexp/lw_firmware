
/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

// To facilitate faster compiles via pre-compiled headers, keep "stdtest.h" first.
#include "mdiag/tests/stdtest.h"

#include "peer_mod.h"
#include "mdiag/utils/perf_mon.h"
#include "mdiag/utils/tex.h"

PeerTraceModule::PeerTraceModule
(
    Trace3DTest *test,
    string moduleName,
    GpuTrace::TraceFileType ftype,
    Trace3DTest *peerTest,
    string peerFileName
) : TraceModule(test, moduleName, ftype),
    m_pPeerTest(peerTest),
    m_PeerFileName(peerFileName),
    m_pPeerModule(NULL),
    m_pPeerSurface(NULL),
    m_bVP2TileRelocPresent(false),
    m_bVP2PitchRelocPresent(false),
    m_bPeerSetup(false)
{
}

/* virtual */ PeerTraceModule::~PeerTraceModule() { }

/* virtual */ bool PeerTraceModule::IsPeer() const
{
    return true;
}
/* virtual */ RC   PeerTraceModule::AddFreezeReloc(UINT32 offset)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ bool PeerTraceModule::IsFrozenAt(UINT32 offset)
{
    return false;
}

/* virtual */ RC   PeerTraceModule::LoadFromFile(TraceFileMgr * pTraceFileMgr)
{
    return OK;
}

/* virtual */ void PeerTraceModule::GetSurfaces(set<UINT32*>* ptrs)
{
    ptrs->clear();
}

/* virtual */ UINT32 * PeerTraceModule::GetSurfacePtr(UINT32 subdev)
{
    MASSERT(!"PeerTraceModule::GetSurfacePtr not supported\n");
    return NULL;
}

/* virtual */ RC PeerTraceModule::Allocate()
{
    return OK;
}

/* virtual */ RC PeerTraceModule::AllocPushbufSurf()
{
    MASSERT(!"PeerTraceModule::AllocPushbufSurf not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC PeerTraceModule::AllocateDynamicSurface()
{
    return OK;
}

/* virtual */ RC PeerTraceModule::Download()
{
    return OK;
}

/* virtual */ RC PeerTraceModule::Update(UINT32 Offset,
                                         const char *UpdateFilename,
                                         TraceFileMgr * pTraceFileMgr)
{
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ UINT64 PeerTraceModule::GetSize() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->GetSize();

    return m_pPeerSurface->GetSize();
}

/* virtual */ UINT32 PeerTraceModule::GetCtxDmaHandle()
{
    MASSERT(m_bPeerSetup);
    if (m_pPeerModule)
    {
        if (m_pPeerModule->GetDmaBuffer()->GetLocation() == Memory::Fb)
        {
            return m_pPeerModule->GetDmaBuffer()->GetCtxDmaHandleGpuPeer(GetGpuDev());
        }
        else if (GetGpuDev() != m_pPeerTest->GetBoundGpuDevice())
        {
            return m_pPeerModule->GetDmaBuffer()->GetCtxDmaHandleGpuShared(GetGpuDev());
        }
        else
        {
            return m_pPeerModule->GetDmaBuffer()->GetCtxDmaHandle();
        }
    }

    if (m_pPeerSurface->GetLocation() == Memory::Fb)
    {
        return m_pPeerSurface->GetCtxDmaHandleGpuPeer(GetGpuDev());
    }
    else if (GetGpuDev() != m_pPeerTest->GetBoundGpuDevice())
    {
        return m_pPeerSurface->GetCtxDmaHandleGpuShared(GetGpuDev());
    }
    else
    {
        return m_pPeerSurface->GetCtxDmaHandleGpu();
    }

    return 0;
}

/* virtual */ UINT64 PeerTraceModule::GetOffset() const
{
    MASSERT(m_bPeerSetup);

    if ((GetGpuDev() != m_pPeerTest->GetBoundGpuDevice()) &&
        ((GetGpuDev()->GetNumSubdevices() > 1) ||
         (m_pPeerTest->GetBoundGpuDevice()->GetNumSubdevices() > 1)))
    {
        MASSERT(!"Peer mappings between devices with more than one "
                 "subdevice must use GetOffset(locSD, remSD)\n");
        return 0;
    }

    return GetOffset(0, 0);

}

// ----------------------------------------------------------------------------
//! \brief Get the offset for the peer module using the specified subdevices
//!
//! \param locSD : The local subdevice to get the offset on.  "local" always
//!                refers to the device where the memory is allocated
//! \param remSD : The remote subdevice that is accessing the peer'd memory
//!                "remote" always refers to the device that is accessing the
//!                memory
/* virtual */ UINT64 PeerTraceModule::GetOffset(UINT32 locSD, UINT32 remSD, UINT32 peerID) const
{
    MASSERT(m_bPeerSetup);
    UINT64 Offset = 0;

    // Unspecified local subdevices always refer to subdevice 0
    if (locSD == Gpu::MaxNumSubdevices)
    {
        locSD = 0;
    }
    if (m_pPeerModule)
    {
        if (m_pPeerModule->GetDmaBuffer()->GetLocation() == Memory::Fb)
        {
            Offset = (m_pPeerModule->GetDmaBuffer()->GetCtxDmaOffsetGpuPeer(locSD, GetGpuDev(), remSD) +
                     m_pPeerModule->GetOffsetWithinDmaBuf());
        }
        else if (GetGpuDev() != m_pPeerTest->GetBoundGpuDevice())
        {
            Offset = (m_pPeerModule->GetDmaBuffer()->GetCtxDmaOffsetGpuShared(GetGpuDev()) +
                     m_pPeerModule->GetOffsetWithinDmaBuf());
        }
        else
        {
            // This call already includes the extra alloc size
            return m_pPeerModule->GetOffset();
        }
    }
    else
    {
        if (m_pPeerSurface->GetLocation() == Memory::Fb)
        {
            Offset = m_pPeerSurface->GetCtxDmaOffsetGpuPeer(locSD, GetGpuDev(), remSD);
        }
        else if (GetGpuDev() != m_pPeerTest->GetBoundGpuDevice())
        {
            Offset = m_pPeerSurface->GetCtxDmaOffsetGpuShared(GetGpuDev());
        }
        else
        {
            Offset = m_pPeerSurface->GetCtxDmaOffsetGpu();
        }
    }
    return Offset + m_ExtraAllocSize;
}

/* virtual */ UINT64 PeerTraceModule::GetOffsetWithinDmaBuf() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->GetOffsetWithinDmaBuf();

    return 0;
}

/* virtual */ CachedSurface* PeerTraceModule::GetCachedSurface() const
{
    MASSERT(!"PeerTraceModule::GetCachedSurface not supported\n");
    return NULL;
}

/* virtual */ void PeerTraceModule::ReleaseCachedSurface(UINT32 gpuSubdevIdx)
{
    MASSERT(!"PeerTraceModule::ReleaseCachedSurface not supported\n");
}

/* virtual */ const MdiagSurf *PeerTraceModule::GetDmaBuffer() const
{
    MASSERT(m_bPeerSetup);
    MASSERT(m_pPeerModule);
    return m_pPeerModule->GetDmaBuffer();
}

/* virtual */ MdiagSurf *PeerTraceModule::GetDmaBufferNonConst() const
{
    MASSERT(!"PeerTraceModule::GetDmaBufferNonConst not supported\n");
    return NULL;
}

/* virtual */ RC        PeerTraceModule::RelocAdd(TraceRelocation*)
{
    MASSERT(!"PeerTraceModule::RelocAdd not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC        PeerTraceModule::RemoveFermiMethods(UINT32 gpuSubdevIdx)
{
    MASSERT(!"PeerTraceModule::RemoveFermiMethods not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ TraceModule::RelocIter PeerTraceModule::RelocBegin() const
{
    return m_EmptyRelocs.begin();
}

/* virtual */ TraceModule::RelocIter PeerTraceModule::RelocEnd() const
{
    return m_EmptyRelocs.end();
}

/* virtual */ RC   PeerTraceModule::RelocResetMethodAdd(UINT32 PushBufferOffset)
{
    MASSERT(!"PeerTraceModule::RelocResetMethodAdd not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}
/* virtual */ bool PeerTraceModule::HasRelocResetMethod(UINT32 PushBufferOffset) const
{
    return false;
}

/* virtual */ UINT32 PeerTraceModule::Get032(UINT32 offset, UINT32 subdev)
{
    MASSERT(!"PeerTraceModule::Get032 not supported\n");
    return 0;
}

/* virtual */ UINT32 *PeerTraceModule::Get032Addr(UINT32 offset, UINT32 subdev)
{
    MASSERT(!"PeerTraceModule::Get032Addr not supported\n");
    return NULL;
}

/* virtual */ void PeerTraceModule::Put032(UINT32 offset, UINT32 data, UINT32 subdev)
{
    MASSERT(!"PeerTraceModule::Put032 not supported\n");
}

/* virtual */ void PeerTraceModule::DoReloc(UINT32 offset, UINT32 data, UINT32 subdev)
{
    MASSERT(!"PeerTraceModule::DoReloc not supported\n");
}

/* virtual */ void PeerTraceModule::RedoRelocations()
{
    MASSERT(!"PeerTraceModule::RedoRelocations not supported\n");
}

/* virtual */ UINT32 PeerTraceModule::GetTextureIndex(UINT32 num) const
{
    MASSERT( num < m_TextureIndexList.size() );
    return m_TextureIndexList[num];
}

/* virtual */ UINT32 PeerTraceModule::GetTextureIndexNum() const
{
    return m_TextureIndexList.size();
}

/* virtual */ TextureHeader *PeerTraceModule::GetTxParams() const
{
    if (m_TxParams)
        return m_TxParams;

    if (m_pPeerModule)
        return m_pPeerModule->GetTxParams();

    return NULL;
}

/* virtual */ bool PeerTraceModule::SetTextureParams(const TextureHeaderState *textureState,
                                                     const ArgReader *params)
{
    TextureHeader* header = TextureHeader::CreateTextureHeader(textureState,
        m_pPeerTest->GetBoundGpuDevice()->GetSubdevice(0), m_Test->GetTextureHeaderFormat());

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

/* virtual */ Memory::Protect PeerTraceModule::GetProtect() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->GetProtect();

    return m_pPeerSurface->GetProtect();
}

/* virtual */ void PeerTraceModule::SetLocation(_DMA_TARGET Location)
{
    MASSERT(!"PeerTraceModule::SetLocation not supported\n");
}

/* virtual */ _DMA_TARGET PeerTraceModule::GetLocation() const
{
    Memory::Location location;
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->GetLocation();
    else
        location = m_pPeerSurface->GetLocation();

    return LocationToTarget(location);
}

/* virtual */ void PeerTraceModule::MassagePushBuffer(Massager& massager)
{
    MASSERT(!"PeerTraceModule::MassagePushBuffer not supported\n");
}

/* virtual */ void PeerTraceModule::PrintPushBuffer(const char *prefix, bool ofs)
{
    MASSERT(!"PeerTraceModule::PrintPushBuffer not supported\n");
}

/* virtual */ void PeerTraceModule::MassagePushBuffer2()
{
    MASSERT(!"PeerTraceModule::MassagePushBuffer2 not supported\n");
}

/* virtual */ bool PeerTraceModule::MassageAndInsertSubdeviceMasks()
{
    MASSERT(!"PeerTraceModule::MassageAndInsertSubdeviceMasks not supported\n");
    return false;
}

/* virtual */ bool PeerTraceModule::MassageAndAddPushBuffer(GpuTrace *trace, const ArgReader *params)
{
    MASSERT(!"PeerTraceModule::MassageAndAddPushBuffer not supported\n");
    return false;
}

/* virtual */ bool PeerTraceModule::HasAttrSet() const
{
    MASSERT(m_bPeerSetup);

    return true;
}

/* virtual */ UINT32 PeerTraceModule::GetHeapAttr() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->GetHeapAttr();

    return m_pPeerSurface->GetVidHeapAttr();
}

/* virtual */ bool PeerTraceModule::HasAttr2Set() const
{
    MASSERT(m_bPeerSetup);

    return true;
}

/* virtual */ UINT32 PeerTraceModule::GetHeapAttr2() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->GetHeapAttr2();

    return m_pPeerSurface->GetVidHeapAttr2();
}

/* virtual */ bool   PeerTraceModule::SetHeapAttr(UINT32 attr)
{
    MASSERT(!"PeerTraceModule::SetHeapAttr not supported\n");
    return false;
}

/* virtual */ bool   PeerTraceModule::SetHeapAttr2(UINT32 attr2)
{
    MASSERT(!"PeerTraceModule::SetHeapAttr2 not supported\n");
    return false;
}

/* virtual */ UINT32 PeerTraceModule::GetHeapType() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->GetHeapType();

    return m_pPeerSurface->GetType();
}

/* virtual */ bool PeerTraceModule::GetCompressed() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->GetCompressed();

    return m_pPeerSurface->GetCompressed();
}

// Public interface to our secret m_SendSegments data, for use by
// GpuTrace during parsing, and TraceOpSendMethods during playback.
/* virtual */ RC PeerTraceModule::AddMethodSegment(UINT32 start, UINT32 size, SegmentH * pSegmentH)
{
    MASSERT(!"PeerTraceModule::AddMethodSegment not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC PeerTraceModule::SendMethodSegment(SegmentH h)
{
    MASSERT(!"PeerTraceModule::SendMethodSegment not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC PeerTraceModule::SetTraceChannel (TraceChannel * ptc,
                                                   bool is_pushbuffer /*=true*/)
{
    MASSERT(ptc);
    MASSERT(!is_pushbuffer);

    if (m_pTraceChannel && (m_pTraceChannel != ptc))
    {
        // User is not allowed to use the same FILE with different CHANNELs.
        return RC::ILWALID_FILE_FORMAT;
    }
    m_pTraceChannel = ptc;
    return OK;
}

/* virtual */ RC PeerTraceModule::MassagePushBuffer(UINT32 subdev, GpuTrace *trace, const ArgReader *params)
{
    MASSERT(!"PeerTraceModule::MassagePushBuffer not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ void PeerTraceModule::DoSkipGeometry()
{
    MASSERT(!"PeerTraceModule::DoSkipGeometry not supported\n");
}

/* virtual */ void PeerTraceModule::DoSkipComputeGrid()
{
    MASSERT(!"PeerTraceModule::DoSkipComputeGrid not supported\n");
}

/* virtual */ void PeerTraceModule::AddZLwllId(UINT32 id)
{
    m_Trace->AddZLwllId(id);

    if (id != ~UINT32(0) && m_Test->GetGpuResource()->PerfMon())
    {
        PerformanceMonitor::AddZLwllId(id);
    }
}

/* virtual */ bool PeerTraceModule::IsVP2TilingSupported() const
{
    return ( (GpuTrace::FT_VP2_0 <= m_FileType) && (m_FileType <= GpuTrace::FT_VP2_9) );
}

/* virtual */ bool PeerTraceModule::IsVP2TilingActive() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->IsVP2TilingActive();

    return false;
}

/* virtual */ bool PeerTraceModule::IsVP2SourceCmdLine() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->IsVP2SourceCmdLine();

    return false;
}

/* virtual */ bool PeerTraceModule::IsVP2LayoutPitchLinear() const
{
    MASSERT(m_bPeerSetup);

    if (m_pPeerModule)
        return m_pPeerModule->IsVP2LayoutPitchLinear();

    return false;
}

/* virtual */ RC   PeerTraceModule::ExelwteVP2TilingMode()
{
    MASSERT(!"PeerTraceModule::ExelwteVP2TilingMode not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC   PeerTraceModule::DoVP2TilingLayout()
{
    return OK;
}

/* virtual */ RC   PeerTraceModule::GetVP2TilingModeID(UINT32 *tiling_mode_id) const
{
    MASSERT(m_bPeerSetup);
    MASSERT(m_pPeerModule);

    return m_pPeerModule->GetVP2TilingModeID(tiling_mode_id);
}

/* virtual */ RC   PeerTraceModule::GetVP2PitchValue(UINT32 *pitch_value) const
{
    MASSERT(m_bPeerSetup);
    MASSERT(m_pPeerModule);

    return m_pPeerModule->GetVP2TilingModeID(pitch_value);
}

/* virtual */ RC   PeerTraceModule::GetVP2Width(UINT32* pWidth) const
{
    MASSERT(m_pPeerModule);

    return m_pPeerModule->GetVP2Width(pWidth);
}

/* virtual */ RC   PeerTraceModule::GetVP2Height(UINT32* pHeight) const
{
    MASSERT(m_pPeerModule);

    return m_pPeerModule->GetVP2Height(pHeight);
}

/* virtual */ void PeerTraceModule::SetVP2TileModeRelocPresent()
{
    m_bVP2TileRelocPresent = true;
}

/* virtual */ void PeerTraceModule::SetVP2PitchRelocPresent()
{
    m_bVP2PitchRelocPresent = true;
}

/* virtual */ bool PeerTraceModule::AreRequiredVP2TileModeRelocsPresent()
{
    MASSERT(m_bPeerSetup);
    MASSERT(m_pPeerModule);

    if (!m_pPeerModule->IsVP2TilingActive())
        return true;

    if (!m_pPeerModule->IsVP2SourceCmdLine())
        return true;

    return m_bVP2TileRelocPresent;
}

/* virtual */ bool PeerTraceModule::AreRequiredVP2PitchRelocsNeeded()
{
    MASSERT(m_bPeerSetup);
    MASSERT(m_pPeerModule);

    if (!m_pPeerModule->IsVP2TilingActive())
        return true;

    if (!m_pPeerModule->IsVP2SourceCmdLine())
        return true;

    if (!m_pPeerModule->IsVP2LayoutPitchLinear())
        return true;

    return m_bVP2PitchRelocPresent;
}

/* virtual */ const vector<UINT32>&  PeerTraceModule::GetGeometry() const
{
    MASSERT(m_bPeerSetup);
    MASSERT(m_pPeerModule);
    return m_pPeerModule->GetGeometry();
}

/* virtual */ const BlockLinear*  PeerTraceModule::GetBlockLinear() const
{
    MASSERT(m_bPeerSetup);
    MASSERT(m_pPeerModule);
    return m_pPeerModule->GetBlockLinear();
}

/* virtual */ bool PeerTraceModule::IsPMUSupported()
{
    return false;
}

/*  virtual */ RC PeerTraceModule::GetOffsetPmu(UINT32 subdev, UINT64 *pOffset)
{
    MASSERT(!"PeerTraceModule::GetOffsetPmu not supported\n");
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ void PeerTraceModule::AddClearInitMethod(UINT32 offset)
{
    MASSERT(!"PeerTraceModule::AddClearInitMethod not supported\n");
}

/* virtual */ void PeerTraceModule::AddPeerGpuMappings(set<GpuDevice *> *pGpuPeers)
{
    MASSERT(!"PeerTraceModule::AddPeerGpuMappings not supported\n");
}

// ----------------------------------------------------------------------------
//! \brief Pull in the appropriate information for this peer module from the
//!        peer test
//!
/* virtual */ RC PeerTraceModule::SetupPeer()
{
    SurfaceType st = m_Trace->FindSurface(m_PeerFileName.c_str());

    // The peer test better be not started at this point in setup, otherwise
    // the peer test Setup() has failed or it has already competed.  In either
    // case, this routine will fail
    if (m_pPeerTest->GetStatus() != Test::TEST_NOT_STARTED)
    {
        ErrPrintf("Peer Test %s has either failed setup or completed\n",
                  m_pPeerTest->GetTestName());
        return RC::SOFTWARE_ERROR;
    }

    m_ExtraAllocSize = 0;
    // If the surface type is unknown then the surface being peer'd is actually
    // a GenericTraceModule from the peer test, otherwise it is either a "Z"
    // "COLOR" or "CLIPID" surface
    if (st == SURFACE_TYPE_UNKNOWN)
    {
        m_pPeerModule = m_pPeerTest->GetTrace()->ModFind(m_PeerFileName.c_str());
        if (m_pPeerModule)
        {
            m_ExtraAllocSize = m_pPeerModule->GetDmaBuffer()->GetExtraAllocSize();
        }
    }
    else
    {
        if (st == SURFACE_TYPE_CLIPID)
            m_pPeerSurface = m_pPeerTest->GetSurfaceMgr()->GetClipIDSurface();
        else
        {
            m_pPeerSurface = m_pPeerTest->GetSurfaceMgr()->GetSurface(st, 0);
            m_ExtraAllocSize = m_pPeerSurface->GetExtraAllocSize();
        }
    }

    // At least one of the peer'd surfaces must be present
    if ((NULL == m_pPeerModule) && (NULL == m_pPeerSurface))
    {
        ErrPrintf("Unable to find peer file %s from peer test (%s)\n",
                  m_PeerFileName.c_str(), m_pPeerTest->GetTestName());
        return RC::SOFTWARE_ERROR;
    }

    m_bPeerSetup = true;
    return OK;
}
