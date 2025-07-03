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
#ifndef INCLUDED_PEER_MOD_H
#define INCLUDED_PEER_MOD_H

#include "tracemod.h"
#include "trace_3d.h"
#include "gputrace.h"

// //////////////////////////////////////////////////////////////////////////
// The PeerTraceModule class is used for any PEER file declaration
// in the trace file functionality for PEER trace modules is limited to read
// only (other than for data stored locally, i.e. the texture header if the
// peer surface is a texture).
//
class PeerTraceModule : public TraceModule
{
public:

    PeerTraceModule(Trace3DTest *test,
                    string moduleName,
                    GpuTrace::TraceFileType ftype,
                    Trace3DTest *peerTest,
                    string peerFileName);
    virtual ~PeerTraceModule();

    virtual bool IsPeer() const;
    virtual RC   AddFreezeReloc(UINT32 offset);
    virtual bool IsFrozenAt(UINT32 offset);
    virtual RC   LoadFromFile(TraceFileMgr * pTraceFileMgr);
    virtual void GetSurfaces(set<UINT32*>* ptrs);
    virtual UINT32 * GetSurfacePtr(UINT32 subdev);
    virtual RC Allocate();
    virtual RC AllocPushbufSurf();
    virtual RC AllocateDynamicSurface();
    virtual RC PrepareToCheck(CHECK_METHOD CheckMethod, const char* CheckFilename, UINT32 gpuSubdevIdx) { return OK; }
    virtual RC Download();
    virtual RC Update(UINT32 Offset, const char *UpdateFilename, TraceFileMgr * pTraceFileMgr);
    virtual UINT64 GetSize() const;
    virtual UINT32 GetCtxDmaHandle();
    virtual UINT64 GetOffset() const;
    virtual UINT64 GetOffset(UINT32 locSD, UINT32 remSD, UINT32 peerID = USE_DEFAULT_RM_MAPPING) const;
    virtual UINT64 GetOffsetWithinDmaBuf() const;
    virtual CachedSurface* GetCachedSurface() const;
    virtual void ReleaseCachedSurface(UINT32 gpuSubdevIdx);
    virtual const MdiagSurf *GetDmaBuffer() const;
    virtual MdiagSurf *GetDmaBufferNonConst() const;
    virtual RC        RelocAdd(TraceRelocation*);
    virtual TraceModule::RelocIter RelocBegin() const;
    virtual TraceModule::RelocIter RelocEnd() const;
    virtual RC   RelocResetMethodAdd(UINT32 PushBufferOffset);
    virtual bool HasRelocResetMethod(UINT32 PushBufferOffset) const;
    virtual UINT32 Get032(UINT32 offset, UINT32 subdev);
    virtual UINT32 *Get032Addr(UINT32 offset, UINT32 subdev);
    virtual void Put032(UINT32 offset, UINT32 data, UINT32 subdev);
    virtual void DoReloc(UINT32 offset, UINT32 data, UINT32 subdev);
    virtual void RedoRelocations();
    virtual void AddTextureIndex(UINT32 index) { m_TextureIndexList.push_back(index); }
    virtual UINT32 GetTextureIndex(UINT32 num=0) const;
    virtual UINT32 GetTextureIndexNum() const;
    virtual TextureHeader *GetTxParams() const;
    virtual bool SetTextureParams(const TextureHeaderState *textureState,
                                  const ArgReader *params);
    virtual Memory::Protect GetProtect() const;
    virtual void SetLocation(_DMA_TARGET Location);
    virtual _DMA_TARGET GetLocation() const;
    virtual void MassagePushBuffer(Massager& massager);
    virtual void PrintPushBuffer(const char *prefix, bool ofs);

    virtual void MassagePushBuffer2();
    virtual bool MassageAndInsertSubdeviceMasks();
    virtual bool MassageAndAddPushBuffer(GpuTrace *trace, const ArgReader *params);

    virtual bool   HasAttrSet() const;
    virtual UINT32 GetHeapAttr() const;
    virtual bool   HasAttr2Set() const;
    virtual UINT32 GetHeapAttr2() const;
    virtual bool   SetHeapAttr(UINT32 attr);
    virtual bool   SetHeapAttr2(UINT32 attr2);
    virtual UINT32 GetHeapType() const;

    virtual bool HasVprSet() const { return false;}
    virtual bool GetVpr() const { return false;}
    virtual void SetVpr(bool vpr) {}

    virtual bool GetCompressed() const;
    virtual RC   RemoveFermiMethods(UINT32 subdev);

    // Public interface to our secret m_SendSegments data, for use by
    // GpuTrace during parsing, and TraceOpSendMethods during playback.
    virtual RC AddMethodSegment(UINT32 start, UINT32 size, SegmentH * pSegmentH);
    virtual RC SendMethodSegment(SegmentH h);
    virtual RC SetTraceChannel (TraceChannel * ptc, bool is_pushbuffer=true);

    virtual RC MassagePushBuffer(UINT32 subdev, GpuTrace *trace, const ArgReader *params);
    virtual void DoSkipGeometry();
    virtual void DoSkipComputeGrid();
    virtual void AddZLwllId(UINT32 id);

    virtual bool IsVP2TilingSupported() const;
    virtual bool IsVP2TilingActive() const;
    virtual bool IsVP2SourceCmdLine() const;
    virtual bool IsVP2LayoutPitchLinear() const;
    virtual RC   ExelwteVP2TilingMode();
    virtual RC   DoVP2TilingLayout();
    virtual RC   GetVP2TilingModeID(UINT32 *tiling_mode_id) const;
    virtual RC   GetVP2PitchValue(UINT32 *pitch_value) const;
    virtual RC   GetVP2Width(UINT32* pWidth) const;
    virtual RC   GetVP2Height(UINT32* pHeight) const;
    virtual void SetMapToBackingStore() {};
    virtual bool GetMapToBackingStore() const { return false; }

    virtual void SetVP2TileModeRelocPresent();
    virtual void SetVP2PitchRelocPresent();
    virtual bool AreRequiredVP2TileModeRelocsPresent();
    virtual bool AreRequiredVP2PitchRelocsNeeded();

    virtual const vector<UINT32>& GetGeometry() const;
    virtual const BlockLinear*    GetBlockLinear() const;

    virtual bool IsPMUSupported();
    virtual RC   GetOffsetPmu(UINT32 subdev, UINT64 *pOffset);
    virtual void AddClearInitMethod(UINT32 offset);

    virtual void AddPeerGpuMappings(set<GpuDevice *> *pGpuPeers);
    virtual RC   SetupPeer();

private:
    Trace3DTest           *m_pPeerTest;
    string                 m_PeerFileName;
    const TraceModule     *m_pPeerModule;
    const MdiagSurf       *m_pPeerSurface;
    list<TraceRelocation*> m_EmptyRelocs;
    vector<UINT32>         m_TextureIndexList;
    TextureHeader         *m_TxParams;
    bool                   m_bVP2TileRelocPresent;
    bool                   m_bVP2PitchRelocPresent;
    bool                   m_bPeerSetup;
    UINT64                 m_ExtraAllocSize;
};

#endif // INCLUDED_PEER_MOD_H
