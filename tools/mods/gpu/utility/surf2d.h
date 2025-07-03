/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2004-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  surf2d.h
 * @brief Abstraction representing a surface
 */

#ifndef INCLUDED_SURF2D_H
#define INCLUDED_SURF2D_H

#ifndef INCLUDED_TYPES_H
#include "core/include/types.h"
#endif
#ifndef INCLUDED_RC_H
#include "core/include/rc.h"
#endif
#ifndef INCLUDED_COLOR_H
#include "core/include/color.h"
#endif
#ifndef INCLUDED_MEMTYPES_H
#include "core/include/memtypes.h"
#endif

#include "core/include/lwrm.h"
#include "lwos.h"
#include <memory>

#include "ctrl/ctrl003e.h"
#include "ctrl/ctrl0041.h"
#ifndef _cl2080_h_
#include "class/cl2080.h"   // LW2080_MAX_SUBDEVICES
#endif

#ifndef INCLUDED_GPU_H
#include "core/include/gpu.h"
#endif

class GpuDevice;
class GpuSubdevice;
struct JSObject;
struct JSContext;

//! \class Surface2D
//! \brief Represents an array of pixels.
//!
//! A Surface2D is an abstraction representing a
//! surface filled with pixel data.  Surface2D is aware of the surface's color
//! format and other such information so that it can provide easier-to-use
//! pixel-level access to the surface.
//!
//! Also, Surface2D carries around with it all the data needed to work with
//! BlockLinear surfaces (Gob layout, for example).

class Surface2D
{
public:
    static const INT32 NO_LOCATION_OVERRIDE = -1;

    enum Layout
    {
        Pitch,
        Swizzled,
        BlockLinear,
        Tiled,
        NumLayouts // Always last
    };

    enum VASpace
    {
        //! Default VA space is:
        //! - GPUVASpace on big GPU and CheetAh with big GPU (e.g. T124)
        //! - TegraVASpace on CheetAh with Aurora GPU (e.g. T114)
        DefaultVASpace          = 0
        ,GPUVASpace             = 1
        ,TegraVASpace           = 2
        ,GPUAndTegraVASpace     = 3
    };

    enum AAMode
    {
        AA_1,
        AA_2,
        AA_4,
        AA_4_Rotated,
        AA_6,
        AA_8,
        AA_16,
        AA_4_Virtual_8,
        AA_4_Virtual_16,
        AA_8_Virtual_16,
        AA_8_Virtual_32
    };

    enum GpuCacheMode
    {
        GpuCacheDefault,
        GpuCacheOff,
        GpuCacheOn
    };

    enum ZbcMode
    {
        ZbcDefault,
        ZbcOff,
        ZbcOn
    };

    enum MemoryMappingMode
    {
        MapDefault,
        MapDirect,
        MapReflected
    };

    enum FbSpeed
    {
        FbSpeedDefault,
        FbSpeedSlow,
        FbSpeedFast
    };

    // Normally a Surface2D object represents a combination of a virtual
    // allocation, a physical allocation, and a map.  This enumerated
    // is used to specify a subset of those three items, with NoSpecialization
    // representing the default.
    enum Specialization
    {
        NoSpecialization,
        VirtualOnly,
        PhysicalOnly,
        MapOnly,
        VirtualPhysicalNoMap
    };

    // To specify SMMU mapping for GPU surfaces
    // GpuSmmuDefault : Use the default RM policy for SMMU mapping
    // GpuSmmuOff     : Disbale SMMu mapping for GPU surfaces
    // GpuSmmuOn      : Enable SMMU mapping for GPU surfaces
    enum GpuSmmuMode
    {
        GpuSmmuDefault = LWOS32_ATTR2_SMMU_ON_GPU_DEFAULT,
        GpuSmmuOff     = LWOS32_ATTR2_SMMU_ON_GPU_DISABLE,
        GpuSmmuOn      = LWOS32_ATTR2_SMMU_ON_GPU_ENABLE
    };

    // This field specifies the black..white input value range for the input surface.
    // This is compared against the associated head's black level and a colwersion
    // is done if needed. Use BYPASS to prevent such automatic colwersion.
    enum InputRange
    {
        BYPASS  = 0,
        LIMITED = 1,
        FULL    = 2
    };

    enum InheritPteKind
    {
        InheritPhysical,
        InheritVirtual
    };

    typedef map<LwRm::Handle, UINT64> GpuObjectMappings;

private:
    // Surface2D class is multi client compliant and should never use the
    // LwRmPtr constructor without specifying which client
    DISALLOW_LWRMPTR_IN_CLASS(Surface2D);

    bool m_IsSPAPeerAccess;
    UINT32 m_Width;
    UINT32 m_Height;
    UINT32 m_Depth;
    UINT32 m_ArraySize;
    UINT32 m_BytesPerPixel;
    ColorUtils::Format m_ColorFormat;
    UINT32 m_LogBlockWidth;
    UINT32 m_LogBlockHeight;
    UINT32 m_LogBlockDepth;
    UINT32 m_Type;
    string m_Name;
    ColorUtils::YCrCbType m_YCrCbType;
    Memory::Location m_Location;
    Memory::Protect m_Protect;
    Memory::Protect m_ShaderProtect;
    Memory::AddressModel m_AddressModel;
    Memory::DmaProtocol m_DmaProtocol;
    Layout m_Layout;
    VASpace m_VASpace;
    UINT32 m_VASpaceId;
    bool m_Tiled;
    bool m_Compressed;
    UINT32 m_CompressedFlag;
    UINT32 m_ComptagStart;
    UINT32 m_ComptagCovMin;
    UINT32 m_ComptagCovMax;
    UINT32 m_Comptags;
    AAMode m_AAMode;
    bool m_D3DSwizzled;
    bool m_Displayable;
    INT32 m_PteKind;
    INT32 m_SplitPteKind;
    UINT32 m_PartStride;
    UINT32 m_PageSize;
    UINT32 m_PhysAddrPageSize;
    bool m_DualPageSize;
    UINT64 m_Alignment;
    UINT64 m_VirtAlignment;
    bool m_MemAttrsInCtxDma;
    bool m_VaReverse;
    bool m_PaReverse;
    UINT32 m_MaxCoalesce;
    bool m_ComponentPacking;
    bool m_Scanout;
    UINT32 m_ZLwllFlag;
    UINT32 m_WindowOffsetX;
    UINT32 m_WindowOffsetY;
    UINT32 m_ViewportOffsetX;
    UINT32 m_ViewportOffsetY;
    bool m_ViewportOffsetExist;
    ZbcMode m_ZbcMode;
    UINT32 m_ParentClass;
    UINT32 m_ParentClassAttr;
    bool m_LoopBack;
    UINT32 m_LoopBackPeerId;
    bool m_Priv;
    UINT64 m_GpuVirtAddrHint;
    UINT64 m_GpuVirtAddrHintMax;
    UINT64 m_GpuPhysAddrHint;
    UINT64 m_GpuPhysAddrHintMax;
    bool m_ForceSizeAlloc;
    bool m_CreatedFromAllocSurface;
    MemoryMappingMode m_MemoryMappingMode;
    Specialization m_Specialization;
    bool m_GpuVirtAddrHintUseVirtAlloc;
    bool m_NeedsPeerMapping;
    bool m_FLAPeerMapping;
    UINT32 m_FLAPageSizeKb;
    bool m_GhostSurface;
    bool m_bAllocInUpr;
    bool m_bVideoProtected;
    bool m_bAcrRegion1;
    bool m_bAcrRegion2;
    InheritPteKind m_InheritPteKind;
    bool m_bEgmOn;

    // Force SMMU mapping on GK20A
    GpuSmmuMode m_GpuSmmuMode;

    // LwDisplay
    InputRange m_InputRange;

    bool m_Split;
    Memory::Location m_SplitLocation;

    UINT32 m_AllocWidth, m_AllocHeight, m_AllocDepth;
    UINT32 m_GobWidth, m_GobHeight, m_GobDepth;
    UINT32 m_BlockWidth, m_BlockHeight, m_BlockDepth;
    UINT64 m_Size;
    UINT32 m_Pitch;
    UINT64 m_ArrayPitch;
    UINT64 m_HiddenAllocSize, m_ExtraAllocSize;
    INT64 m_Limit;
    UINT64 m_ArrayPitchAlignment;
    UINT32 m_TileWidthInGobs;
    UINT32 m_ExtraWidth;
    UINT32 m_ExtraHeight;
    ColorUtils::Format m_PlanesColorFormat = ColorUtils::LWFMT_NONE;

    void *m_Address;
    void *m_CdeAddress;
    PHYSADDR m_PhysAddress;
    UINT32 m_MappedSubdev;
    UINT32 m_MappedPart;
    UINT64 m_MappedOffset;
    UINT64 m_MappedSize;
    LwRm  *m_MappedClient;
    bool   m_VirtMemAllocated;
    bool   m_TegraVirtMemAllocated;
    bool   m_CtxDmaAllocatedIso;
    bool   m_CtxDmaAllocatedGpu;
    bool   m_VirtMemMapped;
    bool   m_TegraMemMapped;
    bool   m_CtxDmaMappedGpu;
    bool   m_TurboCipherEncryption;
    GpuCacheMode m_GpuCacheMode;
    GpuCacheMode m_P2PGpuCacheMode;
    GpuCacheMode m_SplitGpuCacheMode;

    // Need two flags for VidHeap control interface:
    // one is for va allocation and another is for pa allocation
    UINT32 m_VaVidHeapFlags;
    UINT32 m_PaVidHeapFlags;

    UINT32 m_VidHeapAttr;
    UINT32 m_VidHeapAttr2;
    UINT64 m_VidHeapAlign;
    UINT32 m_VidHeapCoverage;
    UINT32 m_AllocSysMemFlags;
    UINT32 m_MapMemoryDmaFlags;
    INT32  m_ZLwllRegion;
    UINT32 m_CtagOffset;
    mutable UINT32 m_ActualPageSizeKB; // Cached value for GetActualPageSizeKB()

    UINT64 m_SplitHalfSize;
    UINT32 m_SplitVidHeapAttr;
    UINT32 m_SplitVidHeapAttr2;

    bool   m_DmaBufferAlloc;
    bool   m_KernelMapping;
    UINT32* m_pChannelsBound;
    UINT32  m_NumChannelsBound;

    bool m_IsViewOnly;
    UINT64 m_CpuMapOffset;

    bool m_SkedReflected;
    UINT32 m_MappingObjHandle;

    bool m_IsHostReflectedSurf;
    string m_GpuManagedChName; // name of channle that host-reflected surf bind to
    UINT32 m_GpuManagedChId;

    UINT32 m_MipLevels;
    UINT32 m_Border;
    UINT32 m_Dimensions;
    JSObject  *m_pJsObj;
    JSContext *m_pJsCtx;

    // Enum to make the following definitions easier to read
    enum { MaxSD = LW2080_MAX_SUBDEVICES, MaxD = LW0080_MAX_DEVICES };

    // Resources for remote mapping support (SLI peer-dma, Hybrid peer-dma,
    // and sysmem sharing)
    bool m_RemoteIsMapped[MaxD];

    bool m_AmodelRestrictToFb;

    class RemoteMapping
    {
    public:
        RemoteMapping(UINT32 locSD, UINT32 remD, UINT32 remSD, UINT32 peerId);
        UINT32 m_LocSD, m_RemD, m_RemSD, m_PeerId;
        bool operator<( const RemoteMapping& rhs) const;
    };
    class AllocData
    {
    public:
        AllocData();
        void Print(Tee::Priority pri, const char* linePrefix);
        LwRm::Handle m_hGpuVASpace;
        LwRm::Handle m_hMem;
        LwRm::Handle m_hSplitMem;
        LwRm::Handle m_hVirtMem;
        LwRm::Handle m_hTegraVirtMem;
        LwRm::Handle m_hImportMem;
        LwRm::Handle m_hCtxDmaGpu;
        LwRm::Handle m_hCtxDmaIso;
        map<UINT32, LwRm::Handle> m_hCtxDmaGpuRemote;
        map<RemoteMapping, LwRm::Handle> m_hVirtMemGpuRemote;
        UINT64 m_GpuVirtAddr;
        UINT64 m_TegraVirtAddr;
        UINT64 m_CtxDmaOffsetGpu;
        UINT64 m_CtxDmaOffsetIso;
        UINT64 m_VidMemOffset;
        UINT64 m_SplitVidMemOffset;
        UINT64 m_SplitGpuVirtAddr;
        map<RemoteMapping, UINT64> m_CtxDmaOffsetGpuRemote;
        map<RemoteMapping, UINT64> m_GpuVirtAddrRemote;

        LwRm::Handle m_hCdeVirtMem;
        UINT64       m_CdeVirtAddr;

        LwRm::Handle m_hFLA;
    };

    // Keep m_pLwRm's AllocData outside of the map to avoid wasting time on
    // unnecessary lookups, just make sure to keep it in sync with m_pLwRm...
    AllocData m_DefClientData;

    map<LwRm *, AllocData> m_PeerClientData;

    // Mapping of the surface to other GPU objects (for instance a surface is
    // "mapped" onto a PMU object so that the PMU can access the surface
    // directly)
    GpuObjectMappings m_GpuObjectMappings;

    bool   m_PhysContig;
    bool   m_AlignHostPage;

    bool   m_bUseVidHeapAlloc;

    bool   m_bUseExtSysMemFlags;
    UINT32 m_ExtSysMemAllocFlags;
    UINT32 m_ExtSysMemCtxDmaFlags;

    bool   m_bUsePeerMappingFlags;
    UINT32 m_PeerMappingFlags;

    GpuDevice *m_pGpuDev;
    LwRm      *m_pLwRm;

    bool m_IsSparse;
    FbSpeed m_FbSpeed;

    bool m_bCdeAlloc;
    UINT64 m_CdeAllocSize;

    LwRm::Handle m_ExternalPhysMem;
    bool m_IsHtex;

    bool m_RouteDispRM;

public:
    Surface2D();
    ~Surface2D();

    void SetIsSPAPeerAccess(bool IsSPAPeerAccess) { m_IsSPAPeerAccess = IsSPAPeerAccess; }
    //! \brief Set surface width in pixels.
    void SetWidth(UINT32 Width) { m_Width = Width; }
    //! \brief Set surface height in pixels.
    void SetHeight(UINT32 Height) { m_Height = Height; }
    //! \brief Set surface depth in pixels.
    void SetDepth(UINT32 Depth) { m_Depth = Depth; }
    void SetArraySize(UINT32 ArraySize) { m_ArraySize = ArraySize; }
    //! \brief Set the color format. Defines the size of a pixel.
    void SetColorFormat(ColorUtils::Format ColorFormat) { m_ColorFormat = ColorFormat; }
    UINT32 GetWidth() const { return m_Width; }
    UINT32 GetHeight() const { return m_Height; }
    UINT32 GetDepth() const { return m_Depth; }
    UINT32 GetArraySize() const { return m_ArraySize; }
    ColorUtils::Format GetColorFormat() const { return m_ColorFormat; }

    void SetMipLevels(UINT32 ml) { m_MipLevels = ml; }
    void SetBorder(UINT32 border) { m_Border = border; }
    void SetDimensions(UINT32 dim) { m_Dimensions = dim; }
    UINT32 GetMipLevels() const { return m_MipLevels; }
    UINT32 GetBorder() const { return m_Border; }
    UINT32 GetDimensions() const { return m_Dimensions; }
    void SetLogBlockWidth(UINT32 LogBlockWidth) { m_LogBlockWidth = LogBlockWidth; }
    void SetLogBlockHeight(UINT32 LogBlockHeight) { m_LogBlockHeight = LogBlockHeight; }
    void SetLogBlockDepth(UINT32 LogBlockDepth) { m_LogBlockDepth = LogBlockDepth; }
    //! \brief Set the surface pitch in bytes.
    void SetPitch(UINT32 Pitch) { m_Pitch = Pitch; }
    void SetArrayPitch(UINT64 ArrayPitch) { m_ArrayPitch = ArrayPitch; }
    void SetArrayPitchAlignment(UINT64 Alignment) { m_ArrayPitchAlignment = Alignment; }
    //! Forces allocating extra memory which is not mapped
    void SetHiddenAllocSize(UINT64 Size) { m_HiddenAllocSize = Size; }
    //! Forces allocating extra memory at the beginning of the surface
    void SetExtraAllocSize(UINT64 Size) { m_ExtraAllocSize = Size; }
    //! Forces limiting allocation size regardless of the callwlated surface size
    void SetLimit(INT64 Limit) { m_Limit = Limit; }
    UINT32 GetLogBlockWidth() const { return m_LogBlockWidth; }
    UINT32 GetLogBlockHeight() const { return m_LogBlockHeight; }
    UINT32 GetLogBlockDepth() const { return m_LogBlockDepth; }
    UINT32 GetPitch() const { return m_Pitch; }
    UINT64 GetArrayPitch() const { return m_ArrayPitch; }
    UINT64 GetHiddenAllocSize() const { return m_HiddenAllocSize; }
    UINT64 GetExtraAllocSize() const { return m_ExtraAllocSize; }
    INT64 GetLimit() const { return m_Limit; }

    void SetCdeAlloc(bool bCdeAlloc) { m_bCdeAlloc = bCdeAlloc; }
    bool GetCdeAlloc() const { return m_bCdeAlloc; }
    enum CdeType
    {
        CDE_HORZ
       ,CDE_VERT
    };
    UINT64 GetCdeAllocSize(CdeType type) const;
    RC MapCde();
    void UnmapCde();
    RC GetCtxDmaOffsetCde(CdeType type, UINT64 *pOffset);
    RC GetCdeAddress(CdeType type, void **ppvAddress);

    void SetType(UINT32 Type) { m_Type = Type; }
    UINT32 GetType() const { return m_Type; }
    void SetName(const string& Name) { m_Name = Name; }
    string GetName() const { return m_Name; }

    void SetKernelMapping(bool isNotifier) { m_KernelMapping = isNotifier; }
    bool GetKernelMapping() const { return m_KernelMapping; }
    void SetYCrCbType(ColorUtils::YCrCbType Type) { m_YCrCbType = Type; }
    ColorUtils::YCrCbType GetYCrCbType() { return m_YCrCbType; }

    void SetLocation(Memory::Location Location) { m_Location = Location; }
    void SetEncryption(bool encrypt) {m_TurboCipherEncryption = encrypt; }
    void SetGpuCacheMode(GpuCacheMode mode) {m_GpuCacheMode = mode; }
    void SetP2PGpuCacheMode(GpuCacheMode mode) {m_P2PGpuCacheMode = mode; }
    void SetSplitGpuCacheMode(GpuCacheMode mode) {m_SplitGpuCacheMode = mode; }
    void SetProtect(Memory::Protect Protect) { m_Protect = Protect; }
    void SetShaderProtect(Memory::Protect ShaderProtect) { m_ShaderProtect = ShaderProtect; }
    void SetAddressModel(Memory::AddressModel Model) { m_AddressModel = Model; }
    void SetLayout(Layout Layout) { m_Layout = Layout; }
    void SetVASpace(VASpace vaSpace) { m_VASpace = vaSpace; }
    void SetVASpaceId(UINT32 vaSpaceId) { m_VASpaceId = vaSpaceId; }
    void SetDmaProtocol(Memory::DmaProtocol DmaProtocol) { m_DmaProtocol = DmaProtocol; }
    void SetTiled(bool Tiled) { m_Tiled = Tiled; }
    void SetCompressed(bool Compressed);
    void SetCompressedFlag(UINT32 CompressedFlag) { m_CompressedFlag = CompressedFlag; }
    void SetComptagStart(UINT32 ComptagStart) { m_ComptagStart = ComptagStart; }
    void SetComptagCovMin(UINT32 ComptagCovMin) { m_ComptagCovMin = ComptagCovMin; }
    void SetComptagCovMax(UINT32 ComptagCovMax) { m_ComptagCovMax = ComptagCovMax; }
    void SetComptags(UINT32 Comptags) { m_Comptags = Comptags; }
    void SetAASamples(UINT32 AASamples);
    void SetAAMode(AAMode Mode) { m_AAMode = Mode; }
    void SetD3DSwizzled(bool swizzled) { m_D3DSwizzled = swizzled; }
    void SetDisplayable(bool Displayable) { m_Displayable = Displayable; }
    void SetPteKind(INT32 PteKind) { m_PteKind = PteKind; }
    RC ChangePteKind(INT32 PteKind);
    void SetSplitPteKind(INT32 PteKind) { m_SplitPteKind = PteKind; }
    void SetPartStride(UINT32 PartStride) { m_PartStride = PartStride; }
    void SetPhysContig(bool PhysContig) { m_PhysContig = PhysContig; }
    void SetAlignHostPage(bool AlignHostPage) { m_AlignHostPage = AlignHostPage; }
    void SetExtSysMemFlags(UINT32 AllocFlags, UINT32 CtxDmaFlags);
    void SetPeerMappingFlags(UINT32 PeerMappingFlags);
    void SetDefaultVidHeapAttr(UINT32 Attr);
    void SetDefaultVidHeapAttr2(UINT32 Attr2);
    void SetPageSize(UINT32 PageSize) { m_PageSize = PageSize; }
    void SetPhysAddrPageSize(UINT32 PageSize) { m_PhysAddrPageSize = PageSize; }
    //! Sets physical and virtual memory alignment
    void SetAlignment(UINT64 Alignment) { m_Alignment = Alignment; }
    //! Overrides virtual memory alignment
    void SetVirtAlignment(UINT64 Alignment) { m_VirtAlignment = Alignment; }
    void SetMemAttrsInCtxDma(bool Force) { m_MemAttrsInCtxDma = Force; }
    void SetVaReverse(bool Reverse) { m_VaReverse = Reverse; }
    void SetPaReverse(bool Reverse) { m_PaReverse = Reverse; }
    void SetMaxCoalesce(UINT32 MaxCoalesce) { m_MaxCoalesce = MaxCoalesce; }
    void SetSplit(bool Split) { m_Split = Split; }
    void SetSplitLocation(Memory::Location Location) { m_SplitLocation = Location; }
    void SetComponentPacking(bool Packing) { m_ComponentPacking = Packing; }
    void SetScanout(bool Scanout) { m_Scanout = Scanout; }
    void SetZLwllFlag(UINT32 ZLwllFlag) { m_ZLwllFlag = ZLwllFlag; }
    void SetWindowOffsetX(UINT32 X) { m_WindowOffsetX = X; }
    void SetWindowOffsetY(UINT32 Y) { m_WindowOffsetY = Y; }
    void SetViewportOffsetX(UINT32 X) { m_ViewportOffsetX = X; }
    void SetViewportOffsetY(UINT32 Y) { m_ViewportOffsetY = Y; }
    void SetViewportOffsetExist(bool Exist) { m_ViewportOffsetExist = Exist; }
    void SetZbcMode(ZbcMode ZbcMode) { m_ZbcMode = ZbcMode; }
    void SetMemoryMappingMode(MemoryMappingMode mode) { m_MemoryMappingMode = mode; }
    void SetInheritPteKind(InheritPteKind inheritPteKind) { m_InheritPteKind = inheritPteKind; }
    //! Sets class for a parent object for the allocated memory.
    //!
    //! When provided, an object of this class will be created and
    //! used as a parent for the allocated memory instead of using
    //! current context dma. The handle to the created object is held
    //! in m_hCtxDmaGpu.
    void SetParentClass(UINT32 ClassNum) { m_ParentClass = ClassNum; }
    void SetParentClassAttr(UINT32 Attr) { m_ParentClassAttr = Attr; }
    //! Enables P2P loopback on a vidmem surface.
    void SetLoopBack(bool LoopBack) { m_LoopBack = LoopBack; }
    void SetLoopBackPeerId(UINT32 peerId) { m_LoopBackPeerId = peerId; }
    //! Enables privileged mode
    void SetPriv(bool Priv) { m_Priv = Priv; }
    void SetGpuVirtAddrHint(UINT64 Addr) { m_GpuVirtAddrHint = Addr; }
    void SetGpuVirtAddrHintMax(UINT64 Addr) { m_GpuVirtAddrHintMax = Addr; }
    void SetGpuPhysAddrHint(UINT64 Addr) { m_GpuPhysAddrHint = Addr; }
    void SetGpuPhysAddrHintMax(UINT64 Addr) { m_GpuPhysAddrHintMax = Addr; }
    void SetDmaBufferAlloc(bool b) { m_DmaBufferAlloc = b; }
    //! Forces allocation using VidHeapAllocSizeEx for pitch surfaces.
    void SetForceSizeAlloc(bool b) { m_ForceSizeAlloc = b; }
    void SetCreatedFromAllocSurface() { m_CreatedFromAllocSurface = true; }
    void SetSpecialization(Specialization specialization)
        { m_Specialization = specialization; }
    void SetCtagOffset(UINT32 u) { m_CtagOffset = u; }
    void SetGpuVirtAddrHintUseVirtAlloc(bool value)
         { m_GpuVirtAddrHintUseVirtAlloc = value; }
    void SetNeedsPeerMapping(bool value) { m_NeedsPeerMapping = value; }
    void SetFLAPeerMapping(bool value) { m_FLAPeerMapping = value; }
    void SetFLAPageSizeKb(UINT32 pageSizeKb) { m_FLAPageSizeKb = pageSizeKb; }

    void SetCtxDmaHandleGpu(LwRm::Handle dmaHandle, LwRm *pLwrm = 0);
    void SetCtxDmaOffsetGpu(UINT64 dmaOffset, LwRm *pLwrm = 0);
    void SetAllocInUpr(bool bAllocInUpr) { m_bAllocInUpr = bAllocInUpr; }
    void SetVideoProtected(bool bProtected) { m_bVideoProtected = bProtected; }
    void SetAcrRegion1(bool bProtected) { m_bAcrRegion1 = bProtected; }
    void SetAcrRegion2(bool bProtected) { m_bAcrRegion2 = bProtected; }
    void SetGpuSmmuMode(GpuSmmuMode mode) { m_GpuSmmuMode = mode; }
    void SetGpuVASpace(LwRm::Handle hVASpace) { m_DefClientData.m_hGpuVASpace = hVASpace; }
    void SetIsSparse(bool isSparse) { m_IsSparse = isSparse; }
    void SetTileWidthInGobs(UINT32 width) { m_TileWidthInGobs = width; }
    void SetExtraWidth(UINT32 extraWidth) { m_ExtraWidth = extraWidth; }
    void SetExtraHeight(UINT32 extraHeight) { m_ExtraHeight = extraHeight; }
    void SetRouteDispRM(bool routing) { m_RouteDispRM = routing; }
    bool GetRouteDispRM() const { return m_RouteDispRM; }
    void SetPlanesColorFormat(ColorUtils::Format format) { m_PlanesColorFormat = format; }

    // LwDisplay
    void SetInputRange(InputRange inputRange) { m_InputRange = inputRange; }

    // EGM
    void SetEgmAttr() { m_bEgmOn = true; }
    void CommitEgmAttr();

    LwRm::Handle GetGpuVASpace() const { return m_DefClientData.m_hGpuVASpace; }
    UINT32 GetCtagOffset() const { return m_CtagOffset; }
    Memory::Location GetLocation() const { return m_Location; }
    bool GetEncryption() const {return m_TurboCipherEncryption; }
    GpuCacheMode GetGpuCacheMode() const {return m_GpuCacheMode; }
    GpuCacheMode GetP2PGpuCacheMode() const {return m_P2PGpuCacheMode; }
    GpuCacheMode GetSplitGpuCacheMode() const {return m_SplitGpuCacheMode; }
    Memory::Protect GetProtect() const { return m_Protect; }
    Memory::Protect GetShaderProtect() const { return m_ShaderProtect; }
    Memory::AddressModel GetAddressModel() const { return m_AddressModel; }
    Layout GetLayout() const { return m_Layout; }
    VASpace GetVASpace() const { return m_VASpace; }
    UINT32 GetVASpaceId() const { return m_VASpaceId; }
    Memory::DmaProtocol GetDmaProtocol() const { return m_DmaProtocol; }
    const char * GetLayoutStr(Layout layout) const;
    bool GetTiled() const { return m_Tiled; }
    bool GetCompressed() const { return m_Compressed; }
    UINT32 GetCompressedFlag() const { return m_CompressedFlag; }
    UINT32 GetComptagStart() const { return m_ComptagStart; }
    UINT32 GetComptagCovMin() const { return m_ComptagCovMin; }
    UINT32 GetComptagCovMax() const { return m_ComptagCovMax; }
    UINT32 GetComptags() const { return m_Comptags; }
    UINT32 GetAAWidthScale() const;
    UINT32 GetAAHeightScale() const;
    UINT32 GetAASamples() const;
    AAMode GetAAMode() const { return m_AAMode; }
    bool IsD3DSwizzled() const { return m_D3DSwizzled; }
    bool GetDisplayable() const { return m_Displayable; }
    INT32 GetPteKind() const { return m_PteKind; }
    INT32 GetSplitPteKind() const { return m_SplitPteKind; }
    UINT32 GetPartStride() const { return m_PartStride; }
    bool GetPhysContig() const { return m_PhysContig; }
    bool GetAlignHostPage() const { return m_AlignHostPage; }
    UINT32 GetPageSize() const { return m_PageSize; }
    UINT32 GetPhysAddrPageSize() const { return m_PhysAddrPageSize; }
    UINT64 GetAlignment() const { return m_Alignment; }
    UINT64 GetVirtAlignment() const { return m_VirtAlignment; }
    bool GetMemAttrsInCtxDma() const { return m_MemAttrsInCtxDma; }
    bool GetVaReverse() const { return m_VaReverse; }
    bool GetPaReverse() const { return m_PaReverse; }
    UINT32 GetMaxCoalesce() const { return m_MaxCoalesce; }
    bool GetSplit() const { return m_Split; }
    Memory::Location GetSplitLocation() const { return m_SplitLocation; }
    bool GetComponentPacking() const { return m_ComponentPacking; }
    bool GetScanout() const { return m_Scanout; }
    UINT32 GetZLwllFlag() const { return m_ZLwllFlag; }
    UINT32 GetWindowOffsetX() const { return m_WindowOffsetX; }
    UINT32 GetWindowOffsetY() const { return m_WindowOffsetY; }
    UINT32 GetViewportOffsetX() const { return m_ViewportOffsetX; }
    UINT32 GetViewportOffsetY() const { return m_ViewportOffsetY; }
    UINT32 GetViewportOffsetExist() const { return m_ViewportOffsetExist; }
    ZbcMode GetZbcMode() const { return m_ZbcMode; }
    MemoryMappingMode GetMemoryMappingMode() const { return m_MemoryMappingMode; }
    UINT32 GetParentClass() const { return m_ParentClass; }
    UINT32 GetParentClassAttr() const { return m_ParentClassAttr; }
    bool GetLoopBack() const { return m_LoopBack; }
    UINT32 GetLoopBackPeerId() const { return m_LoopBackPeerId; }
    bool GetPriv() const { return m_Priv; }
    UINT64 GetGpuVirtAddrHint() const { return m_GpuVirtAddrHint; }
    UINT64 GetGpuVirtAddrHintMax() const { return m_GpuVirtAddrHintMax; }
    UINT64 GetGpuPhysAddrHint() const { return m_GpuPhysAddrHint; }
    UINT64 GetGpuPhysAddrHintMax() const { return m_GpuPhysAddrHintMax; }
    bool GetDmaBufferAlloc() const { return m_DmaBufferAlloc; }
    bool GetForceSizeAlloc() const { return m_ForceSizeAlloc; }
    bool GetCreatedFromAllocSurface() const { return m_CreatedFromAllocSurface; }
    bool IsVirtualOnly() const { return m_Specialization == VirtualOnly; }
    bool IsPhysicalOnly() const { return m_Specialization == PhysicalOnly; }
    bool IsMapOnly() const { return m_Specialization == MapOnly; }
    bool HasVirtual() const;
    bool HasPhysical() const;
    bool HasMap() const;
    bool GetGpuVirtAddrHintUseVirtAlloc() const
        { return m_GpuVirtAddrHintUseVirtAlloc; }
    bool GetNeedsPeerMapping() const { return m_NeedsPeerMapping; }
    bool GetFLAPeerMapping() const { return m_FLAPeerMapping; }
    UINT32 GetFLAPageSizeKb() const { return m_FLAPageSizeKb; }
    bool GetAllocInUpr() const { return m_bAllocInUpr; }
    bool GetVideoProtected() const { return m_bVideoProtected; }
    bool GetAcrRegion1() const { return m_bAcrRegion1; }
    bool GetAcrRegion2() const { return m_bAcrRegion2; }

    GpuSmmuMode GetGpuSmmuMode() const { return m_GpuSmmuMode; }
    UINT32 GetAllocWidth() const { return m_AllocWidth; }
    UINT32 GetAllocHeight() const { return m_AllocHeight; }
    UINT32 GetAllocDepth() const { return m_AllocDepth; }
    UINT32 GetGobWidth() const { return m_GobWidth; }
    UINT32 GetGobHeight() const { return m_GobHeight; }
    UINT32 GetGobDepth() const { return m_GobDepth; }
    UINT32 GetBlockWidth() const { return m_BlockWidth; }
    UINT32 GetBlockHeight() const { return m_BlockHeight; }
    UINT32 GetBlockDepth() const { return m_BlockDepth; }
    //! \brief Returns effective surface size in bytes excluding extra allocated space.
    UINT64 GetSize() const { return m_Size - m_HiddenAllocSize - m_ExtraAllocSize; }
    UINT64 EstimateSize(GpuDevice *pGpuDev) const;
    //! \brief Returns total allocated memory size in bytes.
    UINT64 GetAllocSize() const { return m_Size; }
    LwRm::Handle GetMemHandle(LwRm *pLwRm = 0) const;
    LwRm::Handle GetVirtMemHandle(LwRm *pLwRm = 0) const;
    LwRm::Handle GetTegraMemHandle(LwRm *pLwRm = 0) const;
    UINT32 GetVidHeapAttr() const { return m_VidHeapAttr; }
    UINT32 GetVidHeapAttr2() const { return m_VidHeapAttr2; }
    UINT64 GetVidHeapAlign() const { return m_VidHeapAlign; }
    UINT32 GetVidHeapCoverage() const { return m_VidHeapCoverage; }
    UINT32 GetAllocSysMemFlags() const { return m_AllocSysMemFlags; }
    UINT32 GetMapMemoryDmaFlags() const { return m_MapMemoryDmaFlags; }
    INT32 GetZlwllRegion() const { return m_ZLwllRegion; }
    UINT32 GetActualPageSizeKB() const;
    bool GetIsSparse() const { return m_IsSparse; }
    UINT32 GetTileWidthInGobs() const { return m_TileWidthInGobs; }
    UINT32 GetExtraWidth() const { return m_ExtraWidth; }
    UINT32 GetExtraHeight() const { return m_ExtraHeight; }

    // LwDisplay
    InputRange GetInputRange() const { return m_InputRange; }

    UINT32 GetNumParts() const { return m_Split ? 2 : 1; }
    UINT64 GetPartOffset(UINT32 part) const;
    UINT64 GetPartSize(UINT32 part) const;
    LwRm::Handle GetSplitMemHandle(LwRm *pLwRm = 0) const;
    UINT32 GetSplitVidHeapAttr() const { return m_SplitVidHeapAttr; }
    UINT64 GetSplitVidMemOffset(LwRm *pLwRm = 0) const;

    //! Deprecated, equivalent to GetCtxDmaHandleIso.
    LwRm::Handle GetCtxDmaHandle(LwRm *pLwRm = 0) const;

    //! Get ctxdma handle to be used with the GPU.
    LwRm::Handle GetCtxDmaHandleGpu(LwRm *pLwRm = 0) const;

    //! Get ctxdma handle to be used with the ISO display chip.
    LwRm::Handle GetCtxDmaHandleIso(LwRm *pLwRm = 0) const;

    UINT64 GetVidMemOffset(LwRm *pLwRm = 0) const;
    void * GetAddress() const { return m_Address; }
    //! Return physical address of the beginning of a contiguous vidmem surface.
    //! The surface must be mapped before calling this function.
    PHYSADDR GetPhysAddress() const { return m_PhysAddress; }
    RC GetPhysAddress(UINT64 virtOffset, PHYSADDR* pPhysAddr, LwRm *pLwRm = 0) const;

    //! Return the device physical address, i.e the physical address sent to the memory controller.
    //! In CheetAh, GPU can treat an SMMU address as physical.
    RC GetDevicePhysAddress(UINT64 virtOffset, VASpace vaSpace, PHYSADDR *pPhysAddr, LwRm *pLwRm = 0) const;

    //! Deprecated, equivalent to GetCtxDmaOffsetIso.
    UINT64 GetOffset(LwRm *pLwRm = 0) const;

    //! Get ctxdma offset (in bytes) of the surface, for use with the GPU.
    UINT64 GetCtxDmaOffsetGpu(LwRm *pLwRm = 0) const;

    //! Get virtual address (ctxdma offset) (in bytes) of the surface, for use with the GPU.
    UINT64 GetVirtAddress(LwRm *pLwRm = 0) const { return GetCtxDmaOffsetGpu(pLwRm); }

    //! Get ctxdma offset (in bytes) of the surface, for use with the ISO display chip.
    UINT64 GetCtxDmaOffsetIso(LwRm *pLwRm = 0) const;
    UINT64 GetTegraVirtAddr(LwRm *pLwRm = 0) const;

    LwRm::Handle GetExternalPhysMem() const { return m_ExternalPhysMem; }

    RC Alloc(GpuDevice *gpudev, LwRm *pLwRm = 0);
    RC AllocGhost(LwRm::Handle hMem, GpuDevice *gpudev = 0, LwRm *pLwRm = 0);
    RC MapVirtToPhys(GpuDevice *gpudev, Surface2D *virtAlloc,
        Surface2D *physAlloc, UINT64 virtualOffset, UINT64 physicalOffset,
        LwRm* pLwRm = nullptr, bool needGmmuMap = true);
    void Free();
    RC Map(UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV, LwRm *pLwRm = 0);
    RC MapPart(UINT32 part, UINT32 subdev, LwRm *pLwRm = 0);
    RC MapOffset(UINT64 offset, UINT64* pRelOffset = 0, LwRm *pLwRm = 0);
    RC MapRegion(UINT64 offset, UINT64 size,
                 UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV, LwRm *pLwRm = 0);
    RC MapRect(UINT32 x, UINT32 y, UINT32 width, UINT32 height, UINT32 Subdev);
    bool IsMapped() const { return m_Address != 0; }
    void Unmap();
    UINT32 GetMappedSubdev() const {return m_MappedSubdev; }
    UINT32 GetMappedPart() const { return m_MappedPart; }

    RC MapToVASpace(LwRm::Handle hVASpace, LwRm::Handle* phVirtMem, UINT64* phVirtAddr, LwRm* pLwRm = 0);

    //! Flags for FlushCpuCache
    enum CacheFlushFlags
    {
        //! Use this if surface is written by CPU and read by GPU.
        Flush               = 1,
        //! Use this if surface is written by GPU and read by CPU.
        Ilwalidate          = 2,
        //! Use this if surface is read and written by both CPU and GPU.
        FlushAndIlwalidate  = 3
    };

    //! Flush CPU cache for the surface if it's mapped.
    //!
    //! @param flags Indicates whether the surface should be flushed
    //!              and/or ilwalidated in CPU cache.
    //! @sa CacheFlushFlags
    RC FlushCpuCache(UINT32 flags = FlushAndIlwalidate);

    RC EnableDualPageSize(bool DualPageSize);
    bool IsDualPageSize() const;

    // Generic mapping members.  Used to map the surface onto a generic GPU
    // object.  Current use is to map the surface onto the PMU
    RC     MapToGpuObject(LwRm::Handle hObject);
    bool   IsGpuObjectMapped(LwRm::Handle hObject) const;
    UINT64 GetCtxDmaOffsetGpuObject(LwRm::Handle hObject) const;
    const GpuObjectMappings& GetGpuObjectMappings() const { return m_GpuObjectMappings; }

    struct MapContext;
    MapContext SaveMapping() const;
    RC RestoreMapping(const MapContext& Ctx);
    bool IsSurface422Compressed() const;
    bool IsSurface420Compressed() const;

    class MappingSaver
    {
    public:
        explicit MappingSaver(Surface2D& Surface);
        ~MappingSaver();

    private:
        // Non-copyable
        MappingSaver(const MappingSaver&);
        MappingSaver& operator=(const MappingSaver&);

        Surface2D& m_Surface;
        unique_ptr<Surface2D::MapContext> m_pContext;
    };

    RC MapPeer();
    RC MapPeer(GpuDevice *pPeerDev);
    RC MapLoopback();
    RC MapLoopback(UINT32 loopbackPeerId);
    RC MapShared(GpuDevice *pRemoteDev);
    bool IsPeerMapped(GpuDevice *pRemoteDev = NULL) const;
    bool IsMapShared(GpuDevice *pSharedDev) const;

    UINT64 GetCtxDmaOffsetGpuPeer(UINT32 subdev, LwRm *pLwRm = 0) const;
    UINT64 GetCtxDmaOffsetGpuPeer(UINT32 subdev, UINT32 peerId, LwRm *pLwRm = 0) const;

    UINT64 GetGpuVirtAddrPeer(UINT32 subdev, LwRm *pLwRm = 0) const;
    LwRm::Handle GetVirtMemHandleGpuPeer(UINT32 subdev, LwRm *pLwRm = 0) const;

    LwRm::Handle GetCtxDmaHandleGpuPeer(GpuDevice *pRemoteDev,
                                        LwRm *pLwRm = 0) const;
    UINT64 GetCtxDmaOffsetGpuPeer(UINT32     locSD,
                                  GpuDevice *pRemoteDev,
                                  UINT32     remSD,
                                  UINT32     peerId,
                                  LwRm *pLwRm = 0) const;
    UINT64 GetCtxDmaOffsetGpuPeer(UINT32     locSD,
                                  GpuDevice *pRemoteDev,
                                  UINT32     remSD,
                                  LwRm *pLwRm = 0) const;
    UINT64 GetGpuVirtAddrPeer(UINT32     locSD,
                              GpuDevice *pRemoteDev,
                              UINT32     remSD,
                              LwRm *pLwRm = 0) const;
    UINT64 GetGpuVirtAddrPeer(UINT32     locSD,
                              GpuDevice *pRemoteDev,
                              UINT32     remSD,
                              UINT32     peerId,
                              LwRm *pLwRm = 0) const;
    LwRm::Handle GetVirtMemHandleGpuPeer(UINT32     locSD,
                                         GpuDevice *pRemoteDev,
                                         UINT32     remSD,
                                         LwRm *pLwRm = 0) const;
    LwRm::Handle GetVirtMemHandleGpuPeer(UINT32     locSD,
                                         GpuDevice *pRemoteDev,
                                         UINT32     remSD,
                                         UINT32     peerId,
                                         LwRm *pLwRm = 0) const;
    LwRm::Handle GetCtxDmaHandleGpuShared(GpuDevice *pSharedDev,
                                          LwRm *pLwRm = 0) const;
    UINT64 GetCtxDmaOffsetGpuShared(GpuDevice *pSharedDev,
                                    LwRm *pLwRm = 0) const;
    UINT64 GetGpuVirtAddrShared(GpuDevice *pSharedDev,
                                LwRm *pLwRm = 0) const;
    UINT32 GetVirtMemHandleGpuShared(GpuDevice *pSharedDev,
                                     LwRm *pLwRm = 0) const;

    UINT32 GetElemsPerPixel() const;
    UINT32 GetBytesPerPixel() const;
    UINT32 GetBitsPerPixel() const;
    void SetBytesPerPixel(UINT32 value) { m_BytesPerPixel = value; }
    UINT32 GetWordSize() const;

    //! Init this Surface2D object from JS properties.
    RC InitFromJs
    (
        JSObject *  testObj,    //!< JS Random2d object (or other test)
        const char * surfName   //!< I.e. "SurfA" or "SurfB".
    );

    void SaveJSCtxObj (JSContext *ctx, JSObject *obj);
    void GetJSCtxObj (JSContext **ctx, JSObject **obj);

    //! Print this object's properties to the log file.
    void Print (int TeePriority, const char * linePrefix);
    void PrintAllocData(Tee::Priority pri, const char *linePrefix);

#ifdef MODS_INCLUDE_DEBUG_PRINTS
    void Print(Tee::PriDebugStub, const char* linePrefix)
    {
        Print(Tee::PriSecret, linePrefix);
    }
#else
    static constexpr void Print(Tee::PriDebugStub, const char*)
    {
    }
#endif

    //! \brief Bind the Gpu ctxdma handle to the given channel.
    //!
    //! Skip binding if the surface is Paging address model, since the
    //! common ctxdma is already bound to the channel.
    //! Double-binding is treated as an error by the RM.
    RC BindGpuChannel(UINT32 hCh, LwRm *pLwRm = 0);

    //! \brief Bind the Iso ctxdma handle to the given channel.
    //!
    //! Skip binding if the surface is using a shared ctxdma handle
    //! (i.e. the all-FB physical ctxdma) for ISO.
    //! Also, skip binding if the surface is not Displayable.
    //! Double-binding is treated as an error by the RM.
    RC BindIsoChannel(UINT32 hCh, LwRm *pLwRm = 0);

    RC DuplicateSurface(LwRm *pLwRm);

    // Low-level pixel-at-a-time access functions
    UINT64 GetPixelOffset(UINT32 x, UINT32 y) const;
    UINT64 GetPixelOffset(UINT32 x, UINT32 y, UINT32 z, UINT32 a) const;
    UINT32 ReadPixel(UINT32 x, UINT32 y) const;
    UINT32 ReadPixel(UINT32 x, UINT32 y, UINT32 z, UINT32 a) const;
    void WritePixel(UINT32 x, UINT32 y, UINT32 Value);
    void WritePixel(UINT32 x, UINT32 y, UINT32 z, UINT32 a, UINT32 Value);

    // Higher-level access functions
    RC Fill(UINT32 Value, UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC FillRect(UINT32 x, UINT32 y, UINT32 Width, UINT32 Height, UINT32 Value,
            UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC Fill64(UINT16 r, UINT16 g, UINT16 b, UINT16 a,
            UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC FillRect64(UINT32 x, UINT32 y, UINT32 Width, UINT32 Height,
                  UINT16 r, UINT16 g, UINT16 b, UINT16 a,
                  UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC FillPatternTable(UINT32 x, UINT32 y, UINT32* pPattern,
                        UINT32 PatternLength, UINT32 Repetitions);
    RC FillPattern
    (
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char *PatternType,
        const char *PatternStyle,
        const char *PatternDir,
        UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV
    );

    RC FillPatternRect
    (
        UINT32 x,
        UINT32 y,
        UINT32 Width,
        UINT32 Height,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char *PatternType,
        const char *PatternStyle,
        const char *PatternDir,
        UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV
    );

    RC DrawPixelARGB
    (
        UINT32 x,
        UINT32 y,
        UINT32 RectX,
        UINT32 RectY,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        UINT32 ColorARGB,
        UINT32 *LastColorARGB,
        UINT32 BytesPerPixel,
        UINT32 WordSize
    );

    RC FillPatternRectRandom
    (
        UINT32 RectX,
        UINT32 RectY,
        UINT32 Width,
        UINT32 Height,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char * PatternStyle
    );

    RC FillPatternRectBars
    (
        UINT32 RectX,
        UINT32 RectY,
        UINT32 Width,
        UINT32 Height,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char* PatternType,
        const char* PatternDir,
        const char* PaternStyle
    );

    RC FillPatternRectGradient
    (
        UINT32 RectX,
        UINT32 RectY,
        UINT32 Width,
        UINT32 Height,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char* PatternType,
        const char* PatternDir,
        const char* PaternStyle
    );

    RC FillPatternRectBox
    (
        UINT32 RectX,
        UINT32 RectY,
        UINT32 Width,
        UINT32 Height,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char* PatternType,
        const char* PaternStyle
    );
    RC FillPatternRectFSweep
    (
        UINT32 RectX,
        UINT32 RectY,
        UINT32 Width,
        UINT32 Height,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char* PatternType,
        const char* PatternDir,
        const char* PaternStyle
    );

    RC FillPatternRectHueCircle
    (
        UINT32 RectX,
        UINT32 RectY,
        UINT32 Width,
        UINT32 Height,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char* PatternType,
        const char* PaternStyle
    );

    RC FillPatternRectSpecial
    (
        UINT32 RectX,
        UINT32 RectY,
        UINT32 Width,
        UINT32 Height,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char* PatternType,
        const char* PaternStyle
    );

    RC FillPatternRectFpGray
    (
        UINT32 RectX,
        UINT32 RectY,
        UINT32 Width,
        UINT32 Height,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight
    );

    RC FillPatternRectFullRange
    (
        UINT32 RectX,
        UINT32 RectY,
        UINT32 Width,
        UINT32 Height,
        UINT32 RectWidth,
        UINT32 RectHeight,
        UINT32 ChunkWidth,
        UINT32 ChunkHeight,
        const char* PatternType,
        const char* PaternStyle,
        const char* PaternDir
    );

    RC ApplyFp16GainOffset (float fGainRed, float fOffsetRed,
                            float fGainGreen, float fOffsetGreen,
                            float fGainBlue, float fOffsetBlue,
                            UINT32 subdev);
    RC WriteTga(const char *FileName,
                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC WriteTgaRect(UINT32 x, UINT32 y, UINT32 Width, UINT32 Height,
                    const char *FileName,
                    UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC GetCRC(vector<UINT32> *crc, UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC WritePng(const char *FileName,
                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC WriteText(const char *FileName,
                 UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC ReadTga(const char *FileName,
               UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC ReadPng(const char *FileName,
               UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC ReadRaw(const char *FileName,
               UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC ReadSeurat(const char *FileName,
               UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);
    RC ReadTagsTga(const char *FileName);
    RC ReadTagsBinary(const char *FileName);
    RC ReadHdr(const char *FileName,
                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    //! Caller must pre-alloc correct-sized buf
    RC CreatePitchImage(UINT08 *pExistingBuf, size_t bufSize, UINT32 subdev =
            Gpu::UNSPECIFIED_SUBDEV);

    //! Caller passes a STL vector that is resized automatically
    RC CreatePitchImage(vector<UINT08> *pBuf,
            UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV);

    RC FillConstantOnGpu(UINT32 constant);
    RC FillRandomOnGpu(UINT32 seed);
    RC GetBlockedCRCOnGpu(UINT32 *crc);
    RC GetBlockedCRCOnHost(UINT32 subdev, UINT32 *crc);
    RC GetBlockedCRCOnGpuPerComponent(vector<UINT32> *pCrc);
    RC GetBlockedCRCOnHostPerComponent(UINT32 subdev, vector<UINT32> *pCrc);

    void ComputeParams();

    UINT32 GetCompTags() const;
    UINT32 GetTagsOffsetPhys() const;
    UINT32 GetTagsMin() const;
    UINT32 GetTagsMax() const;

    GpuDevice *GetGpuDev() const;
    GpuSubdevice* GetGpuSubdev(UINT32 subdev) const;

    // Temporary routine for surface dumping in mdiag
    void TempSetGpuDev(GpuDevice* pGpuDev) { m_pGpuDev = pGpuDev; }

    void ConfigFromAttr(UINT32 Attr);
    void ConfigFromAttr2(UINT32 Attr2);

    RC ReMapPhysMemory();

    static INT32 GetLocationOverride() { return s_LocationOverride; }
    static void SetLocationOverride(INT32 val) { s_LocationOverride = val; }
    static INT32 GetLocationOverrideSysmem() { return s_LocationOverrideSysmem; }
    static void SetLocationOverrideSysmem(INT32 val) { s_LocationOverrideSysmem = val; }
    static Memory::Location GetActualLocation(Memory::Location requested,
                                              GpuSubdevice *pGpuSubdevice);

    void SetIsSurfaceView() { m_IsViewOnly = true; }
    bool IsSurfaceView() const { return m_IsViewOnly; }
    RC SetSurfaceViewParent(const Surface2D *surface, UINT64 offset);
    UINT64 GetCpuMapOffset() const { return m_CpuMapOffset; }
    bool IsHostReflectedSurf() const { return m_IsHostReflectedSurf; }
    void SetHostReflectedSurf() { m_IsHostReflectedSurf = true; }
    void SetGpuManagedChName(string chName) { m_GpuManagedChName = chName; }
    string GetGpuManagedChName() const;
    void   SetGpuManagedChId(UINT32 id) { m_GpuManagedChId = id; }
    UINT32 GetGpuManagedChId() const { return m_GpuManagedChId; }

    void SetSkedReflected(bool b) { m_SkedReflected = b; }
    bool GetSkedReflected() const { return m_SkedReflected; }
    void SetMappingObj(UINT32 h)  { m_MappingObjHandle = h; }
    UINT32 GetMappingObj() const  { return m_MappingObjHandle; }

    bool IsAllocated() const { return (m_DefClientData.m_hMem != 0) || (m_DefClientData.m_hVirtMem != 0); }

    bool IsGpuMapped() const { return m_VirtMemMapped || m_CtxDmaMappedGpu; }

    LwRm::Handle GetVASpaceHandle(VASpace vaSpace) const;

    bool IsUseVidHeapAlloc() const { return m_bUseVidHeapAlloc; }

    void SetFbSpeed(FbSpeed speed) { m_FbSpeed = speed; }
    FbSpeed GetFbSpeed() const { return m_FbSpeed; }

    RC LoadPitchImage(UINT08 *Buf, UINT32 subdev);

    void SetLocationAttr(Memory::Location location, UINT32* pAttr);

    void SetExternalPhysMem(LwRm::Handle physMem, PHYSADDR physAddr);

    void SetIsHtex(bool isHtex) { m_IsHtex = isHtex; }
    bool IsHtex() const { return m_IsHtex; }
    LwRm* GetLwRmPtr() const { return m_pLwRm; };

    Memory::Location CheckForIndividualLocationOverride
    (
        Memory::Location lwrrentLocation,
        GpuSubdevice *pGpuSubdevice
    ) const;

    void SetAmodelRestrictToFb(bool amodelRestrictToFb) { m_AmodelRestrictToFb = amodelRestrictToFb; }

private:
#ifdef LW_VERIF_FEATURES
    LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS GetPhysParams(bool) const;
#endif

    void ClearMembers();

    static const UINT64 EntirePart = ~static_cast<UINT64>(0U);
    RC MapInternal(UINT32 part, UINT32 subdev, LwRm *pLwRm, UINT64 offset=0, UINT64 size=EntirePart);
    RC MapToVASpaceInternal
    (
        LwRm::Handle  hVASpace,
        bool          bIsFlaMem,
        LwRm::Handle* phVirtMem,
        UINT64*       phVirtAddr,
        LwRm*         pLwRm
    );
    UINT64 GetPhysPageSize() const;

    RC CreatePitchSubImage(UINT32 x, UINT32 y, UINT32 Width, UINT32 Height,
                           vector<UINT08>* pBuf, UINT32 subdev);

    bool IsYUVLegacy() const;
    bool IsYUV() const;

    bool CheckYCrCbFillParams(UINT32 Width, UINT32 ChunkWidth) const;
    RC ColwertYUV422ToYUV444(UINT08* inbuf, UINT08* outbuf,
                             UINT32 Width, UINT32 Height, ColorUtils::Format Format) const;
    RC ColwertYUV444ToYUV422(UINT08* inbuf, UINT08* outbuf,
                             UINT32 Width, UINT32 Height, ColorUtils::Format Format) const;

    UINT32 PaintFpGray(UINT32 Width, UINT32 Height, UINT32 x, UINT32 y, void * extra = NULL);

    bool RememberBoundChannel(UINT32 hCh);
    RC CheckSubdeviceInstance(UINT32 * pSubdev);

    // Alloc helpers
    void CommitAlignment();
    bool GetUseVirtMem() const;
    LwRm::Handle GetPrimaryVASpaceHandle() const;
    bool VirtMemAllowed() const;
    bool VirtMemNeeded() const;
    bool GetUsePitchAlloc() const;
    bool HasC24Scanout() const;
    void SetCompressAttr(Memory::Location location, UINT32* pAttr);
    // These functions set configured properties on m_VidHeapAttr
    void CommitDepthAttr();
    void CommitTiledAttr();
    void CommitAASamplesAttr();
    void CommitFormatAttr();
    void CommitFormatOverrideAttr();
    void CommitComponentPackingAttr(bool bC24Scanout);
    void CommitZLwllAttr();
    void CommitZbcAttr();
    void CommitGpuCacheAttr();
    void CommitSplitGpuCacheAttr();
    void CommitPhysicalityAttr();
    void CommitPageSizeAttr();
    void CommitGpuSmmuMode();
    RC CommitFbSpeed();
    RC CommitPhysAddrOnlyAttr(UINT32 *pAttrib, UINT32 *pAttr2);
    RC CommitFLAOnlyAttr(UINT32 *pAttrib, UINT32 *pAttr2);
    // Others
    UINT32 GetComprCoverage() const;
    UINT32 GetLocationFlags() const;
    UINT32 GetAlignmentFlags() const;
    UINT32 GetTurboCipherFlags() const;
    UINT32 GetScanoutFlags() const;
    UINT32 GetAccessFlagsSeg() const;
    UINT32 GetAccessFlagsPag() const;
    UINT32 GetShaderAccessFlags() const;
    UINT32 GetCacheSnoopFlagsSeg() const;
    UINT32 GetCacheSnoopFlagsPag() const;
    UINT32 GetKernelMappingFlags() const;
    UINT32 GetPhysicalityFlags() const;
    UINT32 GetAddrSpaceFlags() const;
    UINT32 GetDmaProtocolFlags() const;
    UINT32 GetExtSysMemCtxDmaFlags() const;
    UINT32 GetPageSizeFlagsSeg() const;
    UINT32 GetPageSizeFlagsPag() const;
    UINT32 GetMemAttrsLocationFlags() const;
    UINT32 GetVaReverseFlags(bool returlwidheapFlag) const;
    UINT32 GetPaReverseFlags(bool returlwidheapFlag) const;
    UINT32 GetPteCoalesceFlags() const;
    UINT32 GetAcrRegion1Flags() const;
    UINT32 GetAcrRegion2Flags() const;
    UINT32 GetVideoProtectedFlags() const;
    UINT32 GetPeerMappingFlags() const;
    UINT32 GetLoopBackFlags() const;
    UINT32 GetPrivFlagsPag() const;
    UINT32 GetPrivFlagsSeg() const;
    UINT64 CalcLimit(UINT64 DefaultSize) const;
    RC AllocPhysMemory();
    RC MapPhysMemory(LwRm *pLwRm);
    RC MapPhysMemToGpu(UINT32 Flags, LwRm *pLwRm);
    RC ImportMemoryToDisplay();
    RC GetDefaultAlignment(UINT64* alignment);
    void SendReflectedEscapeWrites(LwRm *pLwRm);
    RC MapPhysMemToIso(UINT32 SegFlags, UINT32 PagFlags, LwRm *pLwRm);
    void UnmapPhysMemory(LwRm *pLwRm);
    void PrintParams();

    vector<RemoteMapping> GetRemoteMappings(
        UINT32 locSD,
        UINT32 devInst,
        UINT32 remSD,
        const AllocData &clientData)const;

    RC MapPeerInternal(LwRm *pLwRm);
    RC MapPeerInternal(GpuDevice *pPeerDev, LwRm *pLwRm);
    RC MapLoopbackInternal(LwRm *pLwRm,UINT32 loopbackPeerId);
    RC MapSharedInternal(GpuDevice *pRemoteDev, LwRm *pLwRm);
    RC IsSuitableForRemoteMapping(bool bPeer);
    RC CreateRemoteMapping(GpuDevice     *pRemoteDevice,
                           UINT32         pagDmaFlags,
                           UINT32         locSD,
                           UINT32         remSD,
                           bool           bUseGlobalCtxDma,
                           UINT32         peerId,
                           LwRm          *pLwRm);
    RC CreateRemoteMapping(GpuDevice     *pRemoteDevice,
                           UINT32         pagDmaFlags,
                           UINT32         locSD,
                           UINT32         remSD,
                           bool           bUseGlobalCtxDma,
                           LwRm          *pLwRm);
    void SetRemoteMapped();
    void UnmapRemote(GpuDevice *pDevice);
    void UnmapRemoteInternal(GpuDevice *pDevice,
                             LwRm *pLwRm,
                             const AllocData &clientData);

    AllocData *GetClientData(LwRm **ppClient, const char *pCallingFunction) const;

    RC GetTiledPitch(UINT32 oldPitch, UINT32 * newPitch);

    static INT32 s_LocationOverride;
    static INT32 s_LocationOverrideSysmem;
    void GetSysMemAllocAttr(LW_MEMORY_ALLOCATION_PARAMS *pSysMemAllocParams);
};

namespace JSSurf2DTracker
{
    void EnableGarbageCollector();
    RC CollectGarbage();
};

typedef Surface2D Surface;

#endif // INCLUDED_SURF2D_H
