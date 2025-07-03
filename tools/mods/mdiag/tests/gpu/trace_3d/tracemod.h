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
#ifndef INCLUDED_TRACEMOD_H
#define INCLUDED_TRACEMOD_H

#include "mdiag/utils/types.h"
#include "mdiag/utils/tracefil.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "gputrace.h"

#include <map>
#include <set>
#include <deque>

#define USE_DEFAULT_RM_MAPPING  (UINT32)~0

class Trace3DTest;
class TraceChannel;
// See tracerel.h (only used as pointer in this file)
class TraceRelocation;
// Forward declaration (only used as pointer in this file) See tracemod.h
class TraceModule;
class SurfaceFormat;
class ViewTraceModule;
class SubCtx;

// //////////////////////////////////////////////////////////////////////////
// This class represents the old m_Data array in TraceModule.
// This class:
// - Is Sli-aware. It stores N surfaces (on an N-way SLI system) in an memory
// efficient way (relying on a lazy copy approach).
// - Owns the memory regions, deallocating everything on shutdown.
// NOTES:
// - Offsets are always in terms of bytes inside the surface.
// - Passed or returned sizes are always in terms of UINT32.
// - EDIT in 2015Nov:
//   1. Revised some size type to SizeType along bug fixing for #200148307.
//   2. Added SurfaceCopy to fulfill delegates from CachedSurface.
//   3. Revised CopyAll/AcquireOwnership, etc, buffer lifetime management.
class CachedSurface
{
public:
    typedef size_t      SizeType;
    // Users intend to manipulate buffers on a 4-byte basis.
    typedef UINT32      DataType;
    // Ability to be manipulated as a generic buffer of bytes.
    typedef UINT08      ByteType;

    // DataCount(), SafeSize() and LikelyOverflow() below are expected to be used
    // outside of CachedSurface and xTraceModule as consistant utilities when manipulating
    // data on a 4B basis.
    const SizeType DataCount(const SizeType size) const
    {
        return size / sizeof(DataType);
    }

    // Users might pass non-4B aligned size and data to us. So we reserve bytes by
    // rounded up to 4B when buffer allocation. But this must not affect real size
    // returned by SurfaceCopy::GetSize().
    const SizeType SafeSize(const SizeType size) const
    {
        return (size + 3) & ~3;
    }

    // E.g. Per a 5 bytes surface, access-overflow happens when manipulate Get32/Put32
    // at 4B-offset 1 (that is, 2nd 4B) on a 4B basis, because bytes at byte-offset
    // 5-7 are invalid.
    //
    // Mainly for debugging purpose.
    //
    // @return true if access-overflow might happen, also prints an alert.
    const bool LikelyOverflow(const SizeType size, const SizeType offset) const;

private:
    // Pointers to the copy of the surfaces, and size of each copy.
    // SurfaceCopy isn't in charge of any alloc/dealloc.
    class SurfaceCopy
    {
    private:
        union {
            ByteType* m_pBytes;
            // Handy for 4B basis.
            DataType* m_pData;
        };
        SizeType    m_Size;
        // Store surface size specified in trace header or by args
        SizeType    m_TraceSurfSize;


    public:
        SurfaceCopy()
            : m_pBytes(0), m_Size(0)
        {}

        // Fixing Bug 200148307 "ARB mods args "-crc_chunk_size" make a lwca trace replay failed on MODS+AModel with crc_mismatch issue".
        //   Former CachedSurface::SetSize() saves the size value to a 4-byte rounded one,
        //   if so we failed to get back the original raw size value, thus too many bytes
        //   are CRC checked in SurfInfo2::DoCpuComputeCRCByChunkForPitch, etc.
        // So here removed the round-up code and saved the raw value directly.
        void SetSize(const SizeType size) { m_Size = size; }
        const SizeType GetSize() const { return m_Size; }
        void SetTraceSurfSize(const SizeType size) { m_TraceSurfSize = size; }
        const SizeType GetTraceSurfSize() const { return m_TraceSurfSize; }
        // Handy for 4B basis.
        DataType *GetData() const { return m_pData; }
        // As it ought to be.
        ByteType *GetCopy(SizeType *pSizeOut = 0) const;
        void SetCopy(void* ptr, const SizeType size);
        bool NoCopy() const { return 0 == m_pBytes; }
        bool IsMyCopy(const void *ptr) const { return !NoCopy() && m_pBytes == static_cast<const ByteType*>(ptr); }

    };

    typedef vector<SurfaceCopy>         CacheType;

public:
    CachedSurface(TraceModule* traceModule, const SizeType subdeviceNum,
                  bool copyOnWrite, bool bLazyLoadFile);
    ~CachedSurface();
    // Dealloc the surface for a given subdevice.
    void Dealloc(const SizeType gpuSubdevIdx);
    // Dealloc all the surfaces.
    void DeallocAll();
    // Pass a surface to this object. Cached surface will *own* the surface.
    // EDIT in 2015Nov: Renaming AcquireOwnership() to Import().
    // IMPORTANT: the *surf* shall not be referenced outside anymore after Import() called,
    // because the *surf* may be resized to be a new surface copy as Import() returned.
    // So reference the returned one instead when needed.
    ByteType* Import(const SizeType subdevIdx, void* surf, const SizeType size);
    // Renew() deallocates all existing copies and *import* a copy for a specified subdevice.
    ByteType* Renew(const SizeType subdevIdx, void* surf, const SizeType size);
    // RenewAll() combines 2 operations: Renew(0, ...) + CopyAll()
    void RenewAll(void *surf, const SizeType size);
    // MakeCopy() allocates required buffer in CachedSurface internally,
    // callers needn't create new buffer as Import() may require.
    ByteType* MakeCopy(const SizeType subdevIdx, const void* surf, const SizeType size);
    // Release the surface, returning the surface pointer and passing the
    // ownership to the caller.
    // EDIT in 2015Nov: Renaming ReleaseOwnership() to Export().
    // Note saved size is only for reference, because the copy may be only a part
    // of a specific surface which kept in caller's hand.
    DataType* Export(const SizeType subdevIdx, SizeType *pSavedSize = 0);

    // Copy the same surface to all the subdevices.
    //
    // EDIT in 2015Nov:
    // Simplifed CopyAll() logic. It doesn't "import" the inputed copy in CopyAll()
    // in any case.
    // So if a caller wants to pass to us a new created copy which the caller
    // will not use anymore in order to reduce alloc/dealloc cost as old CopyAll()
    // has been done, the caller may call an Import() prior to calling CopyAll()
    // by passing in Import() returned surface copy.
    //
    // @return true if the inputed copy was just one of my copies before calling,
    // taking the opportunity, the user may determine whether to dealloc the copy
    // after calling.
    bool CopyAll(const void* surfacePtr, const SizeType size);
    // Returns whether the surface is present for a given subdevice.
    bool IsCopyPresent(const SizeType gpuSubdevIdx) const;
    // Offset in PutO32 and GetO32 is the byte offset of the UINT32 to
    // write/read.
    void    Put032(const SizeType gpuSubdevIdx, const SizeType offset, UINT32 value);
    UINT32  Get032(const SizeType gpuSubdevIdx, const SizeType offset);
    UINT32* Get032Addr(const SizeType gpuSubdevIdx, const SizeType offset);
    // Return the pointer to the first word in the surface.
    UINT32* GetPtr(const SizeType gpuSubdevIdx);
    // Return the number of surfaces allocated. When CopyAll is called
    // the surface is replicated for all the subdevices.
    const SizeType GetCopyNumber() const;
    // Return a vector of UINT32* pointing to all the surfaces stored.
    void GetAllPtrs(set<UINT32*>* ptrs);
    // Return size of a subdevice's size.
    const SizeType GetSize(const SizeType gpuSubdevIdx) const;
    // Return surf size specified in trace header or by args
    const SizeType GetTraceSurfSize(const SizeType gpuSubdevIdx) const;
    // Set a subdevice's size.
    void SetSize(const SizeType gpuSubdevIdx, const SizeType size);
    // Set a surf size in trace header or cmdline
    void SetTraceSurfSize(const SizeType gpuSubdevIdx, const SizeType size);
    // Return the number of subdevices that the surface is handling.
    const SizeType GetSubdeviceNumber() const;
    //
    void Print() const;
    void Print(const SurfaceCopy* pCopy) const;
    void SetWasDownloaded() { m_WasDownloaded = true; }
    bool WasDownloaded() const { return m_WasDownloaded; }
    void SetIsFilled(bool IsFilled) {m_IsFilled = IsFilled; }
    bool IsFilled() const {return m_IsFilled; }

    bool IsInLazyLoadStatus() const { return m_InLazyLoadStatus; }

    bool IsConstContBuffer() const { return m_IsConstContBuffer; }
    void SetIsConstContBuffer() { m_IsConstContBuffer = true; }
    void ClearIsConstContBuffer() { m_IsConstContBuffer = false; }

    RC SetConstantData(const SizeType Size = 0);

private:

    // The internal verison of MakeCopy().
    // Unlike Import(), AllocAndCopy() allocates buffer and copy inputed data
    // to the buffer just created.
    ByteType* AllocAndCopy(SurfaceCopy& copy, const void *ptr, const SizeType size);

    // Return whether there is at least one copy of the surface stored.
    bool IsEmpty() const;
    // Return the pointer of the copy of the surface for a subdevice.
    // It's like a Copy-On-Write.
    UINT32* GetPointerToWrite(const SizeType gpuSubdevIdx, const SizeType dataSize);
    // No copy ctor.
    CachedSurface(const CachedSurface&);
    // No assignment.
    CachedSurface& operator = (const CachedSurface&);

    // Moved m_Sizes and m_Ptrs into the cache.
    CacheType m_Cache;

    // TraceModule that owns this object.
    TraceModule* m_TraceModule;
    SizeType m_SubdeviceNum;
    bool m_CacheOnWrite;
    bool m_WasDownloaded;
    bool m_IsFilled;

    // flag to identify lazy loading status
    bool m_InLazyLoadStatus;

    // flag to identify the buffer with constant value 0
    bool m_IsConstContBuffer;
};

// //////////////////////////////////////////////////////////////////////////

typedef UINT32 (GenericTraceModule::*MethodMassager)(UINT32 subdev, UINT32 method, UINT32 data);
typedef list<TraceModule *> TraceModules;
typedef TraceModules::const_iterator ModuleIter;
class Massager;

// //////////////////////////////////////////////////////////////////////////
// The TraceModule class is an abstract base class for all other types of
// TraceModules. A specific type of TraceModule is created for each FILE
// declaration in a trace file.
class TraceModule
{
public:
    // Used for storing relocation data so the relocations
    // for the subdevices can all be done at once.
    typedef map<UINT32, UINT32> Subdev2DataMap; // <subdev, data>
    typedef map<UINT32, Subdev2DataMap> SubdevMaskDataMap; // <offset, MAP>

    // not sure where to put this yet
    static const UINT32 PitchLinearAllignment = 256;

    // A TraceModule contains a list<TraceRelocation*>.
    typedef list<TraceRelocation*>::const_iterator RelocIter;
    typedef size_t SegmentH;

    TraceModule(Trace3DTest *test,
                string moduleName,
                GpuTrace::TraceFileType ftype);
    virtual ~TraceModule() { }

    const string &GetName() const { return m_ModuleName; }
    GpuTrace::TraceFileType GetFileType() const { return m_FileType; }
    virtual bool IsPeer() const;
    MdiagSurf *GetSurface(SurfaceType s, UINT32 subdev) const;
    IGpuSurfaceMgr *GetSurfaceMgr() const;
    const MdiagSurf *GetClipIDSurface() const;
    MdiagSurf *GetClipIDSurface();
    UINT32 GetDebugPrintSize() const { return m_DebugPrintSize; }
    void SetDebugPrintSize(UINT32 size) { m_DebugPrintSize = size; }
    SubdevMaskDataMap *GetSubdevMaskDataMap() { return &m_SubdevMaskDataMap; }
    // Trace_3d has 3 kinds of "surfaces":
    //
    //  - FILE is an explicitly declared subsurface such as "semaphore_0.bin".
    //  -- FILEs of the same FileType are suballocated within a DmaBuf and
    //     thus might not be page-aligned, and will share a common ctxdma in
    //     segmentation model.
    //  -- FILE is represented by the C++ class TraceModule.
    //
    //  - FileType refers to the shared surface for all FILEs of the same type.
    //  -- FileType is represented by the enum TraceFileType.
    //
    //  - Surface is one of the implicitly allocated surfaces such as Z or zlwll.
    //  -- Each of these has its own memory allocation.
    //     Color[0] and Z are always present, others are allocated if the Tesla
    //     methods in the trace show they are needed.
    //  -- Surface is represented by the enum SurfaceType.
    //
    // Many RELOC-type commands want to be able to refer to any of these
    // "surface types".
    //
    // All of these will ultimately refer to a range of offsets within a ctxdma.
    // Unfortunately, the test.hdr parsing oclwrs before surfaces are allocated,
    // so we can't represent them that way during parsing.
    //
    enum { SURF_TYPE_DMABUFFER, SURF_TYPE_LWSURF, SURF_TYPE_PEER };
    struct SomeKindOfSurface
    {
        UINT64  CtxDmaOffset;
        UINT32  CtxDmaHandle;
        UINT64  Size;
        UINT32  SurfaceType;
        void  * SurfacePtr;
        Memory::Location Location;
        LwRm::Handle GetVASpaceHandle() const
        {
            if (SurfaceType == SURF_TYPE_PEER)
                return ((TraceModule*)SurfacePtr)->GetVASpaceHandle();
            else
                return ((MdiagSurf*)SurfacePtr)->GetGpuVASpace();
        }
    };
    void StringToSomeKindOfSurface
    (
        UINT32 subdev,
        UINT32 peerNum,
        string name,
        SomeKindOfSurface * pskos,
        UINT32 peerID = USE_DEFAULT_RM_MAPPING
    );

    virtual MdiagSurf   *SKOS2DmaBufPtr(const SomeKindOfSurface *pSKOS);
    virtual MdiagSurf   *SKOS2LWSurfPtr(const SomeKindOfSurface *pSKOS);
    virtual TraceModule *SKOS2ModPtr(const SomeKindOfSurface *pSKOS);

    virtual RC   AddFreezeReloc(UINT32 offset) = 0;
    virtual bool IsFrozenAt(UINT32 offset) = 0;
    virtual RC   LoadFromFile(TraceFileMgr * pTraceFileMgr) = 0;
    virtual void GetSurfaces(set<UINT32*>* ptrs) = 0;
    virtual UINT32 * GetSurfacePtr(UINT32 subdev) = 0;
    virtual RC Allocate() = 0;
    virtual RC AllocPushbufSurf() = 0;
    virtual RC PrepareToCheck(CHECK_METHOD CheckMethod, const char* CheckFilename, UINT32 gpuSubdevIdx) = 0;
    virtual RC Download() = 0;
    virtual RC Update(UINT32 Offset, const char *UpdateFilename, TraceFileMgr * pTraceFileMgr) = 0;
    virtual UINT64 GetSize() const = 0;
    virtual UINT32 GetCtxDmaHandle() = 0;
    virtual UINT64 GetOffset() const = 0;
    virtual UINT64 GetOffset(UINT32 locSD, UINT32 remSD, UINT32 peerID = USE_DEFAULT_RM_MAPPING) const = 0;
    virtual UINT64 GetOffsetWithinDmaBuf() const = 0;
    virtual CachedSurface* GetCachedSurface() const = 0;
    virtual void ReleaseCachedSurface(UINT32 gpuSubdevIdx) = 0;
    virtual const MdiagSurf *GetDmaBuffer() const = 0;

    // Return a non-constant version of a DMA buffer.  If a module is read
    // only (i.e. a PeerTraceModule) then this function will always assert
    // and return NULL
    virtual MdiagSurf *GetDmaBufferNonConst() const = 0;

    virtual RC        RelocAdd(TraceRelocation*) = 0;
    virtual RelocIter RelocBegin() const = 0;
    virtual RelocIter RelocEnd() const = 0;
    virtual RC   RelocResetMethodAdd(UINT32 PushBufferOffset) = 0;
    virtual bool HasRelocResetMethod(UINT32 PushBufferOffset) const = 0;
    virtual UINT32 Get032(UINT32 offset, UINT32 subdev) = 0;
    virtual UINT32 *Get032Addr(UINT32 offset, UINT32 subdev) = 0;
    virtual void Put032(UINT32 offset, UINT32 data, UINT32 subdev) = 0;
    virtual void DoReloc(UINT32 offset, UINT32 data, UINT32 subdev) = 0;
    virtual void RedoRelocations() = 0;
    virtual void AddTextureIndex(UINT32 index) = 0;
    virtual UINT32 GetTextureIndex(UINT32 num=0) const = 0;
    virtual UINT32 GetTextureIndexNum() const = 0;
    virtual TextureHeader *GetTxParams() const = 0;
    virtual bool SetTextureParams(const TextureHeaderState *textureState, const ArgReader *params) = 0;
    virtual Memory::Protect GetProtect() const = 0;
    virtual void SetLocation(_DMA_TARGET Location) = 0;
    virtual _DMA_TARGET GetLocation() const = 0;
    virtual void MassagePushBuffer(Massager& massager) = 0;
    virtual void PrintPushBuffer(const char *prefix, bool ofs) = 0;

    virtual void MassagePushBuffer2() = 0;
    virtual bool MassageAndInsertSubdeviceMasks() = 0;
    virtual bool MassageAndAddPushBuffer(GpuTrace *trace, const ArgReader *params) = 0;

    virtual bool   HasAttrSet() const = 0;
    virtual UINT32 GetHeapAttr() const = 0;
    virtual bool   HasAttr2Set() const = 0;
    virtual UINT32 GetHeapAttr2() const = 0;
    virtual bool   SetHeapAttr(UINT32 attr) = 0;
    virtual bool   SetHeapAttr2(UINT32 attr2) = 0;
    virtual UINT32 GetHeapType() const = 0;

    virtual bool HasVprSet() const = 0;
    virtual bool GetVpr() const = 0;
    virtual void SetVpr(bool vpr)  = 0;

    virtual bool GetCompressed() const = 0;
    virtual RC   RemoveFermiMethods(UINT32 subdev) = 0;

    // Public interface to our secret m_SendSegments data, for use by
    // GpuTrace during parsing, and TraceOpSendMethods during playback.
    virtual RC AddMethodSegment(UINT32 start, UINT32 size, SegmentH * pSegmentH) = 0;
    virtual RC SendMethodSegment(SegmentH h) = 0;

    virtual RC SetTraceChannel (TraceChannel * ptc, bool is_pushbuffer=true) = 0;
    virtual TraceChannel* GetTraceChannel() const { return m_pTraceChannel; }
    virtual string GetChannelName();

    virtual TraceModule* GetParentModule() { return 0; }

    virtual RC MassagePushBuffer(UINT32 subdev, GpuTrace *trace, const ArgReader *params) = 0;
    virtual void DoSkipGeometry() = 0;
    virtual void DoSkipComputeGrid() = 0;
    virtual void AddZLwllId(UINT32 id) = 0;

    virtual bool IsVP2TilingSupported() const = 0;
    virtual bool IsVP2TilingActive() const = 0;
    virtual bool IsVP2SourceCmdLine() const = 0;
    virtual bool IsVP2LayoutPitchLinear() const = 0;
    virtual RC   ExelwteVP2TilingMode() = 0;
    virtual RC   DoVP2TilingLayout() = 0;
    virtual RC   GetVP2TilingModeID(UINT32 *tiling_mode_id) const = 0;
    virtual RC   GetVP2PitchValue(UINT32 *pitch_value) const = 0;
    virtual RC   GetVP2Width(UINT32* pWidth) const = 0;
    virtual RC   GetVP2Height(UINT32* pHeight) const = 0;
    virtual RC   GetVP2BlockRows(UINT32* pBlockRows) const { return 0; }
    virtual void SetMapToBackingStore() = 0;
    virtual bool GetMapToBackingStore() const = 0;

    virtual void SetVP2TileModeRelocPresent() = 0;
    virtual void SetVP2PitchRelocPresent() = 0;
    virtual bool AreRequiredVP2TileModeRelocsPresent() = 0;
    virtual bool AreRequiredVP2PitchRelocsNeeded() = 0;

    virtual const vector<UINT32>& GetGeometry() const = 0;
    virtual const BlockLinear*    GetBlockLinear() const = 0;

    virtual bool HasAddressRange() const;
    virtual pair<UINT64, UINT64> GetAddressRange() const { return m_AddrRange; }

    virtual bool IsPMUSupported() = 0;
    virtual RC   GetOffsetPmu(UINT32 subdev, UINT64 *pOffset) = 0;
    virtual void AddClearInitMethod(UINT32 offset) = 0;

    GpuDevice *GetGpuDev() const;

    virtual void AddPeerGpuMappings(set<GpuDevice *> *pGpuPeers) = 0;
    virtual RC   SetupPeer() = 0;

    virtual bool HasTypeOverride() const { return false; }
    virtual bool HasAttrOverride() const { return false; }

    void SetGpuCacheMode(const Surface2D::GpuCacheMode mode) { m_GpuCacheMode = mode; }
    void SetP2PGpuCacheMode(const Surface2D::GpuCacheMode mode) { m_P2PGpuCacheMode = mode; }
    void SetLoopback(bool bLoopback) { m_bLoopback = bLoopback; }
    void SetPeerID(UINT32 peerID) { m_PeerIDs.push_back(peerID); }
    Surface2D::GpuCacheMode GetGpuCacheMode() const { return m_GpuCacheMode; }
    Surface2D::GpuCacheMode GetP2PGpuCacheMode() const { return m_P2PGpuCacheMode; }
    bool GetLoopback() const { return m_bLoopback; }
    vector<UINT32> GetPeerIDs() const { return m_PeerIDs; }

    void SetIsDynamic() { m_IsDynamic = true; }
    bool IsDynamic() const { return m_IsDynamic; }
    virtual bool IsMap() const { return false; }
    virtual bool IsShared() const { return false; }
    TraceFileMgr *GetTraceFileMgr() { return m_Trace->GetTraceFileMgr(); }

    bool AllocIndividualBuffer() const;

    virtual SurfaceType GetSurfaceType() const { return SURFACE_TYPE_UNKNOWN; }

    void SetIsColorOrZ() { m_IsColorOrZ = true; }
    bool IsColorOrZ() const { return m_IsColorOrZ; }
    Trace3DTest * GetTest() const { return m_Test; }
    virtual bool IsSurfaceView() const { return false; }

    virtual void SaveTrace3DSurface(MdiagSurf *surface);
    virtual RC AllocateSurface(MdiagSurf *surface, GpuDevice *gpudev);

    // Some trace modules have a surface object that is only used to store
    // the parameters describing the surface.  (The actual surface object
    // used for allocation is owned by a Trace3DTest object.)
    // This function copies the MdiagSurf object that has those parameters.
    virtual MdiagSurf *GetParameterSurface() { return 0; }

    struct ModCheckInfo
    {
        ModCheckInfo();

        string CheckFilename;
        TraceChannel* pTraceCh;
        UINT64 Offset;
        UINT64 Size;
        CHECK_METHOD CheckMethod;
        bool isRawCRC;
    };
    virtual bool SetCheckInfo(const ModCheckInfo& checkInfo) { return false; }
    virtual bool GetCheckInfo(ModCheckInfo& checkInfo) { return false; }

    struct SendSegment
    {
        UINT32 Start;
        UINT32 Size;
    };
    typedef vector<SendSegment> SendSegments;
    virtual SendSegments* GetSegments() { return 0; }

    void ReleaseSurfaceViews();

    void AddSurfaceView(ViewTraceModule *viewModule)
        { m_SurfaceViews.push_back(viewModule); }

    void DisconnectFromGpuVerif();

    virtual void Release();
    bool HasRelocTarget(TraceModule *target);
    void AddRelocTarget(TraceModule *target);
    bool CountRemoteSurfInRelocTargets(UINT32 &count, UINT32 subdev);
    RC GetAccessingSubdevice(UINT32 hostSubdev, UINT32 *accessingSubdev);
    bool HasSelfReloc();
    bool CanBeSliRemote();

    void AddTag(string tag) { m_TagList.push_back(tag); }
    const vector<string> &GetTagList() { return m_TagList; }

    bool GetSharedByTegra() { return m_SharedByTegra; }
    void SetSharedByTegra(bool shared) { m_SharedByTegra = shared; }

    virtual SurfaceFormat* GetFormatHelper() const { return 0; }

    LwRm::Handle GetVASpaceHandle() const { return  m_hVASpace; }
    void SetVASpaceHandle(LwRm::Handle h) { m_hVASpace = h; }

    void SetIsTextureHeader(bool isTextureHeader)
        { m_IsTextureHeader = isTextureHeader; }
    bool IsTextureHeader() { return m_IsTextureHeader; }

protected:
    bool NeedsPeerMapping();

    Trace3DTest *m_Test;
    GpuTrace::TraceFileType m_FileType;
    GpuTrace *m_Trace;
    bool m_IsDynamic;
    string m_ModuleName;
    SubdevMaskDataMap m_SubdevMaskDataMap;

    // Trace channel attated to the module
    TraceChannel* m_pTraceChannel;

    list<TraceModule*> m_RelocTargets;

    vector<string> m_TagList;

    pair<UINT64, UINT64>   m_AddrRange;

private:
    UINT32 m_DebugPrintSize;

    // Cache mode values from the trace, if any exist.
    Surface2D::GpuCacheMode m_GpuCacheMode;
    Surface2D::GpuCacheMode m_P2PGpuCacheMode;
    bool                    m_bLoopback;
    vector<UINT32>          m_PeerIDs;

    bool m_IsColorOrZ;
    vector<ViewTraceModule *> m_SurfaceViews;
    bool m_NeedsPeerMapping;

    bool m_SharedByTegra;
    LwRm::Handle m_hVASpace;
    bool m_IsTextureHeader = false;
};

// //////////////////////////////////////////////////////////////////////////
// The GenericTraceModule class is used for any non-PEER file declaration
// in the trace file and covers many different types of files and special
// behaviors for the file types.
//
// The GenericTraceModule class should be broken into several related types:
//   - a GPU surface
//   - a block of pushbuffer commands
//   - a selfgild module
//   - etc.
class GenericTraceModule : public TraceModule
{
public:

    GenericTraceModule
    (
        Trace3DTest *test,
        string filename,
        GpuTrace::TraceFileType ftype,
        size_t filesize
    );

    ~GenericTraceModule();
    // no relocations should be done in this module at offset <offset>
    RC   AddFreezeReloc(UINT32 offset);
    bool IsFrozenAt(UINT32 offset) {
        return m_FreezeRelocs.find(offset) != m_FreezeRelocs.end();
    }

    // Just copy data into CachedSurface, not "import".
    void StoreInlineData(const void * pData, const size_t dataSize);
    RC LoadFromFile(TraceFileMgr * pTraceFileMgr);
    void GetSurfaces(set<UINT32*>* ptrs);
    UINT32 * GetSurfacePtr(UINT32 subdev);

    RC Allocate();
    RC AllocPushbufSurf();
    RC PrepareToCheck(CHECK_METHOD CheckMethod, const char* CheckFilename, UINT32 gpuSubdevIdx);
    RC Download();
    virtual RC Update(UINT32 Offset, const char *UpdateFilename, TraceFileMgr * pTraceFileMgr);

    CachedSurface* GetCachedSurface() const { return m_CachedSurface; }
    virtual void ReleaseCachedSurface(UINT32 gpuSubdevIdx);
    // Combine operations: ReleaseCachedSurface() + CachedSurface.Import() for ONE subdev.
    // Save Release-Import operation if the new *surf* is exactly the same as that which
    // CachedSurface holds already.
    void ImportCachedSurface(const CachedSurface::SizeType subdevIdx, void* surf, const CachedSurface::SizeType size);

    const MdiagSurf* GetDmaBuffer() const { return m_pDmaBuf; }
    MdiagSurf *GetDmaBufferNonConst() const { return m_pDmaBuf; }
    void SetFilename(string);
    string GetFilename() const { return m_FileToLoad; }
    UINT64 GetSize() const;
    UINT32 GetCtxDmaHandle() { return m_pDmaBuf->GetCtxDmaHandle(); }
    UINT64 GetOffset() const { return m_Offset; }
    UINT64 GetOffset(UINT32 locSD, UINT32 remSD, UINT32 peerID = USE_DEFAULT_RM_MAPPING) const;
    UINT64 GetOffsetWithinDmaBuf() const { return m_BufOffset; }
    void SetOffsetWithinDmaBuf(UINT64 offset) { m_BufOffset = offset; }
    GpuTrace::TraceFileType GetFileType() const { return m_FileType; }

    // A TraceModule contains a list<TraceRelocation*>.
    RC        RelocAdd(TraceRelocation*);
    TraceModule::RelocIter RelocBegin() const;
    TraceModule::RelocIter RelocEnd() const;
    TraceRelocation* RelocBack() const;

    RC   RelocResetMethodAdd(UINT32 PushBufferOffset);
    bool HasRelocResetMethod(UINT32 PushBufferOffset) const;

    // TODO(gsaggese): Remove the subdev args when all the calling functions
    // are SLI aware.
    UINT32 Get032(UINT32 offset, UINT32 subdev);
    UINT32 *Get032Addr(UINT32 offset, UINT32 subdev);
    void Put032(UINT32 offset, UINT32 data, UINT32 subdev);
    virtual void DoReloc(UINT32 offset, UINT32 data, UINT32 subdev);
    virtual void RedoRelocations();

    void AddTextureIndex(UINT32 index) { m_TextureIndexList.push_back(index); }
    UINT32 GetTextureIndex(UINT32 num=0) const;
    UINT32 GetTextureIndexNum() const { return (UINT32)m_TextureIndexList.size(); }
    TextureHeader *GetTxParams() const { return m_TxParams; }
    bool SetTextureParams(const TextureHeaderState *textureState, const ArgReader *params);
    void SetSwapInfo(UINT32 swap_size);
    bool SetCheckInfo(const ModCheckInfo& checkInfo);
    bool GetCheckInfo(ModCheckInfo& checkInfo);
    bool SetCrcCheckMatch(bool match);

    Memory::Protect GetProtect() const { return m_Protect; }
    _DMA_TARGET GetLocation() const { return m_Location; }
    void SetProtect(Memory::Protect Protect) { m_Protect = Protect; }
    void SetLocation(_DMA_TARGET Location) { m_Location = Location; }
    void SetAllocPushbufSurf(bool bAllocPb) { m_bAllocPushbufSurface = bAllocPb; }

    void MassagePushBuffer(Massager& massager);

    // Dumps push buffer data to DebugPrintf(...)
    // prefix is the string that will printed on every line before the data
    // ofs tells the function whether or not to include the offsets
    void PrintPushBuffer(const char *prefix, bool ofs);

    void MassagePushBuffer2();
    bool MassageAndInsertSubdeviceMasks();
    bool MassageAndAddPushBuffer(GpuTrace *trace, const ArgReader *params);
    SelfgildState* CreateSelfgildState(const string&) const;

    bool GetMassaged() const { return m_Massaged; }
    void SetMassaged() { m_Massaged = true;}
    bool   HasAttrSet() const;
    UINT32 GetHeapAttr() const;
    bool   HasAttr2Set() const;
    UINT32 GetHeapAttr2() const;
    bool   SetHeapAttr(UINT32 attr);
    bool   SetHeapAttr2(UINT32 attr2);
    UINT32 GetHeapType() const;
    void SetMapToBackingStore() { m_MapToBackingStore = true; }
    bool GetMapToBackingStore() const { return m_MapToBackingStore; }

    bool HasVprSet() const { return m_Vpr.first ;}
    bool GetVpr() const { return m_Vpr.first ? m_Vpr.second : false ;}
    void SetVpr(bool vpr) { m_Vpr = make_pair(true, vpr); }

    void SetImageRTTCompressed(bool);
    void SetDepthRTTCompressed(bool);

    bool GetCompressed() const;

    // Public interface to our secret m_SendSegments data, for use by
    // GpuTrace during parsing, and TraceOpSendMethods during playback.
    RC AddMethodSegment(UINT32 start, UINT32 size, SegmentH * pSegmentH);
    RC SendMethodSegment(SegmentH h);
    RC SendFermiMethodSegment(SegmentH h);
    RC SetTraceChannel (TraceChannel * ptc, bool is_pushbuffer=true);

    // given a method and original data, returns new (could be the same) data
    //   and MAY set cross-method state as a side-effect
    UINT32 MassageFermiComputeMethod(UINT32 subdev, UINT32 Method, UINT32 Data);
    UINT32 MassageTwodMethod(UINT32 subdev, UINT32 Method, UINT32 Data);
    UINT32 MassageMsdecMethod(UINT32 subdev, UINT32 Method, UINT32 Data);
    UINT32 MassageFermiMethod(UINT32 subdev, UINT32 Method, UINT32 Data);
    UINT32 MassageFermi_cMethod(UINT32 subdev, UINT32 Method, UINT32 Data);
    UINT32 MassageCeMethod(UINT32 subdev, UINT32 Method, UINT32 Data);
    UINT32 MassageMaxwellBMethod(UINT32 subdev, UINT32 method, UINT32 data);
    UINT32 MassageBlackwellMethod(UINT32 subdev, UINT32 method, UINT32 data);

    RC MassagePushBuffer(UINT32 subdev, GpuTrace *trace, const ArgReader *params);
    void DoSkipGeometry();
    void DoSkipComputeGrid();
    void AddZLwllId(UINT32 id);

    bool IsVP2TilingSupported() const;
    bool IsVP2TilingActive() const;
    bool IsVP2SourceCmdLine() const;
    bool IsVP2LayoutPitchLinear() const;
    bool IsVP2TilingPitchParameterNeeded(char const *mode_name);
    RC   SetVP2TilingModeFromHeader(char const *mode_name, UINT32 width, UINT32 height,
                                    UINT32 bpp, char const *endian, UINT32 pitch,UINT32 blockRows);
    RC   ExelwteVP2TilingMode();
    RC   DoVP2TilingLayout();
    RC   GetVP2TilingModeID(UINT32 *tiling_mode_id) const;
    RC   GetVP2PitchValue(UINT32 *pitch_value) const;
    RC   GetVP2Width(UINT32* pWidth) const;
    RC   GetVP2Height(UINT32* pHeight) const;

    void SetVP2TileModeRelocPresent();
    void SetVP2PitchRelocPresent();
    bool AreRequiredVP2TileModeRelocsPresent();
    bool AreRequiredVP2PitchRelocsNeeded();

    RC   SetGeometry(const vector<UINT32>& geometry);
    const vector<UINT32>& GetGeometry() const { return m_Geometry; }
    const BlockLinear*    GetBlockLinear() const { return m_Bl; }

    void SetTypeOverride(UINT32 type) { m_TypeOverride = make_pair(true, type); }
    void SetAttrOverride(UINT32 attr) { m_AttrOverride = make_pair(true, attr); }
    void SetAttr2Override(UINT32 attr2) { m_Attr2Override = make_pair(true, attr2); }
    void SetAddressRange(UINT64 low, UINT64 high) { m_AddrRange.first = low; m_AddrRange.second = high; }

    RC SetSizePerWarp(UINT64 size, shared_ptr<SubCtx> pSubCtx);
    RC SetSizePerTPC(UINT64 size, shared_ptr<SubCtx> pSubCtx);
    RC SetSizeByArg(UINT64 size);
    bool   IsPMUSupported();
    RC     GetOffsetPmu(UINT32 subdev, UINT64 *pOffset);

    virtual void AddClearInitMethod(UINT32 offset)
        { m_ClearInitMethods.push_back(offset); }
    void   SetBufferRaw() { m_DownloadRaw = true; }

    void AddPeerGpuMappings(set<GpuDevice *> *pGpuPeers);
    RC   SetupPeer() { return OK; }

    bool HasTypeOverride() const { return m_TypeOverride.first; }
    bool HasAttrOverride() const { return m_AttrOverride.first; }
    bool HasAttr2Override() const { return m_Attr2Override.first; }

    // used by the plugin system to specify that this TraceModule's initial data
    // will be supplied by the plugin system instead of from reading the
    // trace archive file.  Can be used to initialize new trace modules created
    // by a plugin, or to override initial data for existing TraceModules
    //
    void setPluginDataHandle ( void * handle );
    bool HasPluginInitData() const;
    RC UpdateSph(UINT32 gpuSubdevIdx);
    typedef enum
    {
        READ,
        WRITE
    } SPH_PS_IMAP_ACCESS;
    UINT32 AccessSphPsImapBit(UINT32 *imap, UINT32 bit, SPH_PS_IMAP_ACCESS mode);
    UINT32 PopCountMap(UINT32 *map);
    UINT32 PopCountPsMap(UINT32 *map);

    bool ChannelHasHwClass(UINT32 hwClass, UINT32 *subch, bool *subchValid);

    // For pushbuffers in the Fermi format, replace the given "begin" and "end"
    // methods in a given hw class within a given start/end range with a given
    // "nop" method.
    //
    void NullifyBeginEndMethodsFermi(UINT32 hwClass, UINT32 beginMethod,
                                     UINT32 endMethod, UINT32 nopMethod,
                                     UINT32 startCount, UINT32 endCount);

    void SaveTrace3DSurface(MdiagSurf *surface);

    SendSegments* GetSegments() { return &m_SendSegments; }
    const list<TraceRelocation*>& GetReloc() const { return m_Relocs; }

    void SetSkipInit(bool skip) { m_SkipInit = skip; }

protected:
    //RC DownloadData(UINT32 size);
    RC DownloadData2SubDev(UINT64 size, UINT32 subdev);
    UINT64 ScaleSizeByArgs(UINT64 size) const;

    void SkipFermiGeometry(UINT32 begin, UINT32 end);
    void SetSize(UINT64 size);

    RC RemoveFermiMethods(UINT32 subdev);

    UINT32 GetNumDWFermi(UINT32 header) const;

    RC DumpRawMemory(MdiagSurf *surface, UINT64 offset, UINT32 size, LWGpuChannel* ch);

    TraceChannel* GetCheckChannel();

    // This is the same as ModuleName, except when SWAP_FILE is used on big-endian.
    string m_FileToLoad;

    UINT32 m_PushbufMemSize;
    bool   m_bAllocPushbufSurface;

    list<TraceRelocation*> m_Relocs;
    set<UINT32> m_RelocResetMethods;

    MdiagSurf *m_pDmaBuf;
    UINT64 m_BufOffset;
    UINT64 m_Offset;

    CachedSurface* m_CachedSurface;
    UINT32 m_GpuSubdeviceNum;

    CHECK_METHOD m_Check;
    string m_CheckFilename;
    TraceChannel* m_CheckChannel;
    UINT64 m_CheckOffset;  // Offset relative to m_Offset where CRC checking begins.
    UINT64 m_CheckSize;    // Number of bytes to CRC check.
    bool m_RawCrc;
    bool m_CrcCheckMatch;
    Memory::Protect m_Protect;
    _DMA_TARGET m_Location;
    UINT32 m_HeapAttr;

    // mapping current module to the beginning of backing store
    bool   m_MapToBackingStore;

    TextureHeader *m_TxParams;
    BlockLinear   *m_Bl;
    vector<UINT32> m_TextureIndexList;
    UINT32 m_SwapSize;
    bool m_Massaged;
    UINT32 m_DebugPrintSize;
    bool m_ImageCompressed;
    bool m_DepthCompressed;
    set<UINT32> m_FreezeRelocs;

    const ArgReader* params;

    bool m_DownloadRaw;         // download this buffer in raw format (no layout);

    UINT32 m_LaunchMethodCount;
    vector<unsigned int> m_CtaRasterWidth;
    vector<unsigned int> m_CtaRasterHeight;
    vector<unsigned int> m_CtaRasterDepth;

    UINT32 MassageInsertedMethod( UINT32 Method, UINT32 Data );

    // In TraceModules that contain pushbuffer data, the data may be sent in
    // several segments rather than all at once.  This is sequenced by
    // GpuTrace::m_TraceOps.
    //
    // But, here in TraceModule we need to iterate over our data and rewrite
    // it in several ways, including *inserting methods* at various points.
    // If GpuTrace::m_TraceOps referred to actual start and end offsets in our
    // data, m_TraceOps would fall out of sync when we insert new methods.
    //
    // Instead, we keep the start and size of each SendSegment *here*, and
    // GpuTrace::m_TraceOps just refers to one of our SendSegments.
    //
    // We also track where GpEntry calls will be inserted by TraceOps,
    // so that we can parse pushbuffer headers correctly.  (A gpentry is often
    // preceeded by a header that is missing its data, which would confuse
    // our parsing/rewriting otherwise).
    SendSegments m_SendSegments;

    void PrintMethodSegmentError
    (
        TraceModule::SegmentH   segH,
        const char *            errMsg
    );

    VP2TilingParams* m_VP2TilingParams;

    RC   SetVP2TilingHeapAttr(bool block_linear);
    RC   DoVP2TilingLayoutPitchLinear();
    RC   DoVP2TilingLayoutBlockLinear();
    RC   DoVP2TilingLayoutBlockLinearTexture();
    RC   DoVP2TilingLayoutBlockLinearHelper(BlockLinear *pBlockLinear);
    RC   DoVP2TilingLayout16x16();
    RC   SetDefualtVP2TilingParams();
    void VP2SupportCleanup();
    RC   SetVP2TilingModeFromCmdLine();
    RC   GetVP2BlockRows(UINT32* pBlockRows) const;

    RC   BindPMUSurface();

    UINT64 CrcCheckMask() const;

    RC MapPeers();

    vector<UINT32> m_Geometry;
    vector<UINT32> m_RemoveMethods;
    vector<UINT32> m_ClearInitMethods;

    pair<bool, UINT32> m_TypeOverride;
    pair<bool, UINT32> m_AttrOverride;
    pair<bool, UINT32> m_Attr2Override;
    pair<bool, bool> m_Vpr;

    bool m_NoPbMassage;

    // handle to initial data for this TraceModule held by a plugin
    // NULL means there is no plugin initial data, non-NULL
    // means there is plugin initial data (and to use this data instead
    // of the trace archive initial data for this TraceModule).  Passed in to
    // LoadDataFromPlugin() (see below) to identify the data to load from the
    // plugin
    //
    void * m_pluginDataHandle;

    // load the plugin-supplied initial data for this TraceModule from the
    // plugin world (referenced by the handle) into the pData array.  The plugin
    // initial data is freed and subsequently unavailable after this copy is
    // performed.  LoadDataFromPlugin sets m_pluginDataHandle to null after the
    // copy.
    //
    RC LoadDataFromPlugin( void * handle, UINT32 * pData, UINT32 nWords );

    // A set of GpuDevices that need to access the data from this module
    set<GpuDevice *> *m_pGpuPeerMappings;

    RC AllocateForSubdevices(UINT64 surfaceSize, bool allocSeparate);

    // Functions to get cbsize for Fermi/Kepler/Pascal
    UINT32 GetFermiCbsize(UINT32 gpuSubdevIdx) const;
    UINT32 GetKeplerCbsize(UINT32 gpuSubdevIdx) const;
    UINT32 GetPascalCbsize(UINT32 gpuSubdevIdx) const;

    UINT32 GetSmPerTpc();
    UINT32 GetTpcNum();
    UINT32 GetWarpsPerTpc();
    void ScalePartitionTPCnumber(UINT32 * numTPC, shared_ptr<SubCtx> pSubCtx);
    bool SkipInitialization() const;

    typedef map<UINT32, UINT32> RelocMap;
    RelocMap m_RelocMap;
    typedef enum
    {
        UPPER_LEFT,
        LOWER_LEFT
    } WINDOW_ORIGIN;
    WINDOW_ORIGIN m_WindowOrigin;

    bool m_SkipInit;
};

class SurfaceTraceModule : public GenericTraceModule
{
public:
    SurfaceTraceModule
    (
        Trace3DTest *test,
        string moduleName,
        string fileName,
        const MdiagSurf &surface,
        size_t fileSize,
        SurfaceType surfaceType
    );
    virtual ~SurfaceTraceModule();

    virtual RC Allocate();
    virtual RC LoadFromFile(TraceFileMgr *pTraceFileMgr);
    virtual UINT64 GetSize() const;
    virtual RC Download();

    // Some trace modules have a surface object that is only used to store
    // the parameters describing the surface.  (The actual surface object
    // used for allocation is owned by a Trace3DTest object.)
    // This function copies the MdiagSurf object that has those parameters.
    virtual MdiagSurf *GetParameterSurface() { return &m_ParameterSurface; }

    SurfaceType GetSurfaceType() const { return m_SurfaceType; }

    void SetCrcRect(const CrcRect& rect) { m_CrcRect = rect; }
    CrcRect& GetCrcRect() { return m_CrcRect; }

    RC CreateFormatHelper();
    virtual SurfaceFormat* GetFormatHelper() const { return m_FormatHelper; }

    void SetClass (UINT32 classNum) { m_Class = classNum; }
    UINT32 GetClass() const { return m_Class; }

    void SetFillValue(UINT32 val);
    void ScaleByTPCCount(UINT32 smCount);

    void UpdateOffset();
    void SetTraceOpType(const TraceOpType type) { m_TraceOpType = type; }
    TraceOpType GetTraceOpType() const { return m_TraceOpType; }

protected:

    void FillCachedSurface(UINT64 surfaceSize);

private:
    MdiagSurf m_ParameterSurface;
    // Color and Z surfaces are treated specially,
    // so the surface type is used to indicate color/z.
    SurfaceType m_SurfaceType;
    CrcRect m_CrcRect;
    SurfaceFormat *m_FormatHelper;
    TraceOpType m_TraceOpType = TraceOpType::AllocSurface;

    UINT32 m_Class;

    bool m_FillPresent;
    UINT32 m_FillValue;
};

class ViewTraceModule : public SurfaceTraceModule
{
public:
    ViewTraceModule
    (
        Trace3DTest *test,
        string moduleName,
        string fileName,
        const MdiagSurf &surface,
        size_t fileSize,
        string parentModuleName,
        SurfaceType surfaceType
    );

    virtual RC Allocate();

    virtual bool IsSurfaceView() const { return true; }

    virtual void Release();

    virtual TraceModule *GetParentModule();

private:
    MdiagSurf *GetParentSurface();

    string m_ParentModuleName;
};

// Used to represent a SHARED command from the trace header.
class SharedTraceModule : public SurfaceTraceModule
{
public:
    SharedTraceModule
    (
        Trace3DTest *test,
        string moduleName,
        string fileName,
        const MdiagSurf &surface,
        size_t fileSize,
        SurfaceType surfaceType
    );
  
    virtual RC AllocateSurface(MdiagSurf *surface, GpuDevice *gpudev);
    virtual RC Update(UINT32 Offset, const char *UpdateFilename, TraceFileMgr * pTraceFileMgr);

    virtual bool IsShared() const { return true; }
};

// Used to represent a MAP command from the trace header.
class MapTraceModule : public SurfaceTraceModule
{
public:
    MapTraceModule
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
    );

    virtual RC AllocateSurface(MdiagSurf *surface, GpuDevice *gpudev);
    virtual RC Download();

    virtual bool IsMap() const { return true; }

protected:
    virtual UINT64 GetVirtualOffset(SurfaceTraceModule* virtModule) { return m_VirtualOffset; }

private:
    string m_VirtAllocName;
    string m_PhysAllocName;
    UINT64 m_VirtualOffset;
    UINT64 m_PhysicalOffset;
};

// A special case of the trace header MAP command used when TEXTURE_SUBIMAGE is applied.
// This is for mapping a tile or subset of tiles in a sparse texture miplevel.
class MapTextureSubimageTraceModule : public MapTraceModule
{
public:
    MapTextureSubimageTraceModule
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
    );

protected:
    virtual UINT64 GetVirtualOffset(SurfaceTraceModule* virtModule);

private:
    UINT32 m_XOffset;
    UINT32 m_YOffset;
    UINT32 m_ZOffset;
    UINT32 m_MipLevel;
    UINT32 m_Dimension;
    UINT32 m_ArrayIndex;
};

// A special case of the trace header MAP command used when TEXTURE_MIP_TAIL is applied.
// This is for mapping the mip tail of a sparse texture.  A mip tail represents all of
// the mip levels that are too small to be allocated as sparse.
class MapTextureMipTailTraceModule : public MapTraceModule
{
public:
    MapTextureMipTailTraceModule
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
    );

protected:
    virtual UINT64 GetVirtualOffset(SurfaceTraceModule* virtModule);

private:
    UINT32 m_StartingMipLevel;
    UINT32 m_Dimension;
    UINT32 m_ArrayIndex;
};

#endif
