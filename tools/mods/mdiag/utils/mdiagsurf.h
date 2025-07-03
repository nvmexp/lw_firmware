/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

/**
 * @file  mdiagsurf.h
 * @brief Abstraction representing a surface in mdiag
 */

#ifndef _MDIAGSURF_H
#define _MDIAGSURF_H

#include "gpu/utility/surf2d.h"
#include "mdiag/iommu/iommudrv.h"
#include "mdiag/utils/utils.h"

class MdiagSurf;
class MdiagSeg;
class Surface2D;
class LWGpuChannel;

namespace PixelFormatTransform
{
    enum BlockLinearLayout
    {
        NAIVE_BL,
        XBAR_RAW,
        FB_RAW
    };

    UINT32 GetOffsetInRawGob
    (
        UINT32 offsetInNaiveGob,
        BlockLinearLayout layout,
        GpuDevice *pGpuDevice
    );

    void ColwertBetweenRawAndNaiveBlSurf
    (
        UINT08 *buff,
        UINT32 size,
        BlockLinearLayout srcLayout,
        BlockLinearLayout dstLayout,
        GpuDevice *pGpuDevice
    );

    void ColwertNaiveToRawSurf
    (
        MdiagSurf *surf,
        UINT08 *data,
        UINT32 size
    );

    void ColwertRawToNaiveSurf
    (
        MdiagSurf *surf,
        UINT08 *data,
        UINT32 size
    );
}

class ArgReader;
class LWGpuResource;
class GpuVerif;
class MdiagSegList;

class MdiagSurf
{
public:
    MdiagSurf();
    virtual ~MdiagSurf();

    enum OpType
    {
        RdSurf,
        WrSurf
    };

    enum SurfIommuVAType
    {
        No,
        GVA,
        GPA
    };

    void SetIsSPAPeerAccess(bool isSPAPeerAccess) { m_Surface2D.SetIsSPAPeerAccess(isSPAPeerAccess); }
    void SetWidth(UINT32 Width)  {  m_Surface2D.SetWidth( Width ); }
    void SetHeight(UINT32 Height)  {  m_Surface2D.SetHeight( Height ); }
    void SetDepth(UINT32 Depth)  {  m_Surface2D.SetDepth( Depth ); }
    void SetArraySize(UINT32 ArraySize)  {  m_Surface2D.SetArraySize( ArraySize ); }
    void SetColorFormat(ColorUtils::Format ColorFormat)  {  m_Surface2D.SetColorFormat( ColorFormat ); }
    UINT32 GetWidth() const  { return m_Surface2D.GetWidth(  ); }
    UINT32 GetHeight() const { return m_useAlignedHeight ? m_Surface2D.GetAllocHeight() : m_Surface2D.GetHeight(); }
    UINT32 GetDepth() const  { return m_Surface2D.GetDepth(  ); }
    UINT32 GetArraySize() const  { return m_Surface2D.GetArraySize(  ); }
    ColorUtils::Format GetColorFormat() const  { return m_Surface2D.GetColorFormat(  ); }
    void SetMipLevels(UINT32 ml)  {  m_Surface2D.SetMipLevels( ml ); }
    void SetBorder(UINT32 border)  {  m_Surface2D.SetBorder( border ); }
    void SetDimensions(UINT32 dim)  {  m_Surface2D.SetDimensions( dim ); }
    UINT32 GetMipLevels() const  { return m_Surface2D.GetMipLevels(  ); }
    UINT32 GetBorder() const  { return m_Surface2D.GetBorder(  ); }
    UINT32 GetDimensions() const  { return m_Surface2D.GetDimensions(  ); }
    void SetLogBlockWidth(UINT32 LogBlockWidth)  {  m_Surface2D.SetLogBlockWidth( LogBlockWidth ); }
    void SetLogBlockHeight(UINT32 LogBlockHeight)  {  m_Surface2D.SetLogBlockHeight( LogBlockHeight ); }
    void SetLogBlockDepth(UINT32 LogBlockDepth)  {  m_Surface2D.SetLogBlockDepth( LogBlockDepth ); }
    void SetPitch(UINT32 Pitch)  {  m_Surface2D.SetPitch( Pitch ); }
    void SetArrayPitch(UINT64 ArrayPitch)  {  m_Surface2D.SetArrayPitch( ArrayPitch ); }
    void SetArrayPitchAlignment(UINT64 Alignment)  {  m_Surface2D.SetArrayPitchAlignment( Alignment ); }
    void SetHiddenAllocSize(UINT64 Size)  {  m_Surface2D.SetHiddenAllocSize( Size ); }
    void SetExtraAllocSize(UINT64 Size)  {  m_Surface2D.SetExtraAllocSize( Size ); }
    void SetLimit(INT64 Limit)  {  m_Surface2D.SetLimit( Limit ); }
    UINT32 GetLogBlockWidth() const  { return m_Surface2D.GetLogBlockWidth(  ); }
    UINT32 GetLogBlockHeight() const  { return m_Surface2D.GetLogBlockHeight(  ); }
    UINT32 GetLogBlockDepth() const  { return m_Surface2D.GetLogBlockDepth(  ); }
    UINT32 GetPitch() const  { return m_Surface2D.GetPitch(  ); }
    UINT64 GetArrayPitch() const  { return m_Surface2D.GetArrayPitch(  ); }
    UINT64 GetHiddenAllocSize() const  { return m_Surface2D.GetHiddenAllocSize(  ); }
    UINT64 GetExtraAllocSize() const  { return m_Surface2D.GetExtraAllocSize(  ); }
    INT64 GetLimit() const  { return m_Surface2D.GetLimit(  ); }
    void SetCdeAlloc(bool bCdeAlloc)  {  m_Surface2D.SetCdeAlloc( bCdeAlloc ); }
    bool GetCdeAlloc() const  { return m_Surface2D.GetCdeAlloc(  ); }
    void SetType(UINT32 Type)  {  m_Surface2D.SetType( Type ); }
    UINT32 GetType() const  { return m_Surface2D.GetType(  ); }
    void SetName(const string& Name)  {  m_Surface2D.SetName( Name); }
    string GetName() const  { return m_Surface2D.GetName(  ); }
    void SetKernelMapping(bool isNotifier)  {  m_Surface2D.SetKernelMapping( isNotifier ); }
    bool GetKernelMapping() const  { return m_Surface2D.GetKernelMapping(  ); }
    void SetLocation(Memory::Location Location)  {  m_Surface2D.SetLocation( Location ); }
    void SetEncryption(bool encrypt)  {  m_Surface2D.SetEncryption( encrypt ); }
    void SetGpuCacheMode(Surface2D::GpuCacheMode mode)  {  m_Surface2D.SetGpuCacheMode( mode ); }
    void SetP2PGpuCacheMode(Surface2D::GpuCacheMode mode)  {  m_Surface2D.SetP2PGpuCacheMode( mode ); }
    void SetProtect(Memory::Protect Protect)  {  m_Surface2D.SetProtect( Protect ); }
    void SetLayout(Surface2D::Layout Layout)  {  m_Surface2D.SetLayout( Layout ); }
    void SetVASpace(Surface2D::VASpace vaSpace)  {  m_Surface2D.SetVASpace( vaSpace ); }
    void SetTiled(bool Tiled)  {  m_Surface2D.SetTiled( Tiled ); }
    void SetCompressed(bool Compressed) { m_Surface2D.SetCompressed( Compressed ); }
    void SetCompressedFlag(UINT32 CompressedFlag)  {  m_Surface2D.SetCompressedFlag( CompressedFlag ); }
    void SetComptagStart(UINT32 ComptagStart)  {  m_Surface2D.SetComptagStart( ComptagStart ); }
    void SetComptagCovMin(UINT32 ComptagCovMin)  {  m_Surface2D.SetComptagCovMin( ComptagCovMin ); }
    void SetComptagCovMax(UINT32 ComptagCovMax)  {  m_Surface2D.SetComptagCovMax( ComptagCovMax ); }
    void SetComptags(UINT32 Comptags)  {  m_Surface2D.SetComptags( Comptags ); }
    void SetAAMode(Surface2D::AAMode Mode)  {  m_Surface2D.SetAAMode( Mode ); }
    void SetD3DSwizzled(bool swizzled)  {  m_Surface2D.SetD3DSwizzled( swizzled ); }
    void SetPteKind(INT32 PteKind)  {  m_Surface2D.SetPteKind( PteKind ); }
    void SetInheritPteKind(Surface2D::InheritPteKind inheritPteKind)  {  m_Surface2D.SetInheritPteKind( inheritPteKind ); }
    RC ChangePteKind(INT32 PteKind) { return m_Surface2D.ChangePteKind( PteKind ); }
    void SetPhysContig(bool PhysContig)  {  m_Surface2D.SetPhysContig( PhysContig ); }
    void SetPageSize(UINT32 PageSize)  {  m_Surface2D.SetPageSize( PageSize ); }
    void SetPhysAddrPageSize(UINT32 PageSize)  {  m_Surface2D.SetPhysAddrPageSize( PageSize ); }
    void SetAlignment(UINT64 Alignment)  {  m_Surface2D.SetAlignment( Alignment ); }
    void SetVirtAlignment(UINT64 Alignment)  {  m_Surface2D.SetVirtAlignment( Alignment ); }
    void SetMemAttrsInCtxDma(bool Force)  {  m_Surface2D.SetMemAttrsInCtxDma( Force ); }
    void SetVaReverse(bool Reverse)  {  m_Surface2D.SetVaReverse( Reverse ); }
    void SetPaReverse(bool Reverse)  {  m_Surface2D.SetPaReverse( Reverse ); }
    void SetMaxCoalesce(UINT32 MaxCoalesce)  {  m_Surface2D.SetMaxCoalesce( MaxCoalesce ); }
    void SetComponentPacking(bool Packing)  {  m_Surface2D.SetComponentPacking( Packing ); }
    void SetScanout(bool Scanout)  {  m_Surface2D.SetScanout( Scanout ); }
    void SetZLwllFlag(UINT32 ZLwllFlag)  {  m_Surface2D.SetZLwllFlag( ZLwllFlag ); }
    void SetWindowOffsetX(UINT32 X)  {  m_Surface2D.SetWindowOffsetX( X ); }
    void SetWindowOffsetY(UINT32 Y)  {  m_Surface2D.SetWindowOffsetY( Y ); }
    void SetViewportOffsetX(UINT32 X)  {  m_Surface2D.SetViewportOffsetX( X ); }
    void SetViewportOffsetY(UINT32 Y)  {  m_Surface2D.SetViewportOffsetY( Y ); }
    void SetViewportOffsetExist(bool Exist)  {  m_Surface2D.SetViewportOffsetExist( Exist ); }
    void SetZbcMode(Surface2D::ZbcMode ZbcMode)  {  m_Surface2D.SetZbcMode( ZbcMode ); }
    void SetMemoryMappingMode(Surface2D::MemoryMappingMode mode)  {  m_Surface2D.SetMemoryMappingMode( mode ); }
    void SetLoopBack(bool LoopBack)  {  m_Surface2D.SetLoopBack( LoopBack ); }
    void SetLoopBackPeerId(UINT32 peerId)  {  m_Surface2D.SetLoopBackPeerId(peerId); }
    void SetPriv(bool Priv)  {  m_Surface2D.SetPriv( Priv ); }
    void SetDmaBufferAlloc(bool b)  {  m_Surface2D.SetDmaBufferAlloc( b ); }
    void SetForceSizeAlloc(bool b)  {  m_Surface2D.SetForceSizeAlloc( b ); }
    void SetCreatedFromAllocSurface()  {  m_Surface2D.SetCreatedFromAllocSurface(  ); }
    void SetCtagOffset(UINT32 u)  {  m_Surface2D.SetCtagOffset( u ); }
    void SetNeedsPeerMapping(bool value)  {  m_Surface2D.SetNeedsPeerMapping( value ); }
    void SetAllocInUpr(bool bAllocInUpr)  {  m_Surface2D.SetAllocInUpr( bAllocInUpr ); }
    void SetVideoProtected(bool bProtected)  {  m_Surface2D.SetVideoProtected( bProtected ); }
    void SetAcrRegion1(bool bProtected)  {  m_Surface2D.SetAcrRegion1( bProtected ); }
    void SetAcrRegion2(bool bProtected)  {  m_Surface2D.SetAcrRegion2( bProtected ); }
    void SetIsSparse(bool isSparse)  {  m_Surface2D.SetIsSparse( isSparse ); }
    void SetTileWidthInGobs(UINT32 width)  {  m_Surface2D.SetTileWidthInGobs( width ); }
    void SetExtraWidth(UINT32 extraWidth)  {  m_Surface2D.SetExtraWidth( extraWidth ); }
    void SetExtraHeight(UINT32 extraHeight)  {  m_Surface2D.SetExtraHeight( extraHeight ); }
    void SetExternalPhysMem(LwRm::Handle physMem, PHYSADDR physAddr) { m_Surface2D.SetExternalPhysMem(physMem, physAddr); }
    void SetSurfaceAsRemote(GpuDevice* hostingDevice);
    void SetEgmAttr() { m_Surface2D.SetEgmAttr(); }
    bool IsRemoteAllocated() const;
    UINT32 GetCtagOffset() const  { return m_Surface2D.GetCtagOffset(  ); }
    Memory::Location GetLocation() const  { return m_Surface2D.GetLocation(  ); }
    bool GetEncryption() const  { return m_Surface2D.GetEncryption(  ); }
    Surface2D::GpuCacheMode GetGpuCacheMode() const  { return m_Surface2D.GetGpuCacheMode(  ); }
    Surface2D::GpuCacheMode GetP2PGpuCacheMode() const  { return m_Surface2D.GetP2PGpuCacheMode(  ); }
    Memory::Protect GetProtect() const  { return m_Surface2D.GetProtect(  ); }
    Surface2D::Layout GetLayout() const  { return m_Surface2D.GetLayout(  ); }
    Surface2D::VASpace GetVASpace() const  { return m_Surface2D.GetVASpace(  ); }
    const char * GetLayoutStr(Surface2D::Layout layout) const { return m_Surface2D.GetLayoutStr( layout ); }
    bool GetTiled() const  { return m_Surface2D.GetTiled(  ); }
    bool GetCompressed() const;
    UINT32 GetCompressedFlag() const  { return m_Surface2D.GetCompressedFlag(  ); }
    UINT32 GetComptagStart() const  { return m_Surface2D.GetComptagStart(  ); }
    UINT32 GetComptagCovMin() const  { return m_Surface2D.GetComptagCovMin(  ); }
    UINT32 GetComptagCovMax() const  { return m_Surface2D.GetComptagCovMax(  ); }
    UINT32 GetComptags() const  { return m_Surface2D.GetComptags(  ); }
    UINT32 GetAAWidthScale() const { return m_Surface2D.GetAAWidthScale(  ); }
    UINT32 GetAAHeightScale() const { return m_Surface2D.GetAAHeightScale(  ); }
    Surface2D::AAMode GetAAMode() const  { return m_Surface2D.GetAAMode(  ); }
    bool IsD3DSwizzled() const  { return m_Surface2D.IsD3DSwizzled(  ); }
    INT32 GetPteKind() const  { return m_Surface2D.GetPteKind(  ); }
    bool GetPhysContig() const  { return m_Surface2D.GetPhysContig(  ); }
    UINT32 GetPageSize() const  { return m_Surface2D.GetPageSize(  ); }
    UINT32 GetPhysAddrPageSize() const  { return m_Surface2D.GetPhysAddrPageSize(  ); }
    UINT64 GetAlignment() const  { return m_Surface2D.GetAlignment(  ); }
    UINT64 GetVirtAlignment() const  { return m_Surface2D.GetVirtAlignment(  ); }
    bool GetMemAttrsInCtxDma() const  { return m_Surface2D.GetMemAttrsInCtxDma(  ); }
    bool GetVaReverse() const  { return m_Surface2D.GetVaReverse(  ); }
    bool GetPaReverse() const  { return m_Surface2D.GetPaReverse(  ); }
    UINT32 GetMaxCoalesce() const  { return m_Surface2D.GetMaxCoalesce(  ); }
    bool GetComponentPacking() const  { return m_Surface2D.GetComponentPacking(  ); }
    bool GetScanout() const  { return m_Surface2D.GetScanout(  ); }
    UINT32 GetZLwllFlag() const  { return m_Surface2D.GetZLwllFlag(  ); }
    UINT32 GetWindowOffsetX() const  { return m_Surface2D.GetWindowOffsetX(  ); }
    UINT32 GetWindowOffsetY() const  { return m_Surface2D.GetWindowOffsetY(  ); }
    UINT32 GetViewportOffsetX() const  { return m_Surface2D.GetViewportOffsetX(  ); }
    UINT32 GetViewportOffsetY() const  { return m_Surface2D.GetViewportOffsetY(  ); }
    UINT32 GetViewportOffsetExist() const  { return m_Surface2D.GetViewportOffsetExist(  ); }
    Surface2D::ZbcMode GetZbcMode() const  { return m_Surface2D.GetZbcMode(  ); }
    Surface2D::MemoryMappingMode GetMemoryMappingMode() const  { return m_Surface2D.GetMemoryMappingMode(  ); }
    bool GetLoopBack() const  { return m_Surface2D.GetLoopBack(  ); }
    UINT32 GetLoopBackPeerId() const  { return m_Surface2D.GetLoopBackPeerId(); }
    bool GetPriv() const  { return m_Surface2D.GetPriv(  ); }
    bool GetForceSizeAlloc() const  { return m_Surface2D.GetForceSizeAlloc(  ); }
    bool GetCreatedFromAllocSurface() const  { return m_Surface2D.GetCreatedFromAllocSurface(  ); }
    bool IsVirtualOnly() const  { return m_Surface2D.IsVirtualOnly(  ); }
    bool IsPhysicalOnly() const  { return m_Surface2D.IsPhysicalOnly(  ); }
    bool IsMapOnly() const  { return m_Surface2D.IsMapOnly(  ); }
    bool HasVirtual() const { return m_Surface2D.HasVirtual(  ); }
    bool HasPhysical() const { return m_Surface2D.HasPhysical(  ); }
    bool HasMap() const { return m_Surface2D.HasMap(  ); }
    bool GetNeedsPeerMapping() const  { return m_Surface2D.GetNeedsPeerMapping(  ); }
    bool GetAllocInUpr() const  { return m_Surface2D.GetAllocInUpr(  ); }
    bool GetVideoProtected() const  { return m_Surface2D.GetVideoProtected(  ); }
    bool GetAcrRegion1() const  { return m_Surface2D.GetAcrRegion1(  ); }
    bool GetAcrRegion2() const  { return m_Surface2D.GetAcrRegion2(  ); }
    UINT32 GetAllocWidth() const  { return m_Surface2D.GetAllocWidth(  ); }
    UINT32 GetAllocHeight() const  { return m_Surface2D.GetAllocHeight(  ); }
    UINT32 GetAllocDepth() const  { return m_Surface2D.GetAllocDepth(  ); }
    UINT64 GetSize() const  { return m_Surface2D.GetSize(  ); }
    UINT64 EstimateSize(GpuDevice *pGpuDev) const { return m_Surface2D.EstimateSize( pGpuDev ); }
    UINT64 GetAllocSize() const  { return m_Surface2D.GetAllocSize(  ); }
    LwRm::Handle GetMemHandle(LwRm *pLwRm = 0) const { return m_Surface2D.GetMemHandle( pLwRm ); }
    LwRm::Handle GetVirtMemHandle(LwRm *pLwRm = 0) const { return m_Surface2D.GetVirtMemHandle( pLwRm ); }
    UINT32 GetVidHeapAttr() const  { return m_Surface2D.GetVidHeapAttr(  ); }
    UINT32 GetVidHeapAttr2() const  { return m_Surface2D.GetVidHeapAttr2(  ); }
    INT32 GetZlwllRegion() const  { return m_Surface2D.GetZlwllRegion(  ); }
    UINT32 GetActualPageSizeKB() const { return m_Surface2D.GetActualPageSizeKB(  ); }
    bool GetIsSparse() const  { return m_Surface2D.GetIsSparse(  ); }
    UINT32 GetTileWidthInGobs() const  { return m_Surface2D.GetTileWidthInGobs(  ); }
    UINT32 GetExtraWidth() const  { return m_Surface2D.GetExtraWidth(  ); }
    UINT32 GetExtraHeight() const  { return m_Surface2D.GetExtraHeight(  ); }
    LwRm::Handle GetCtxDmaHandle(LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaHandle( pLwRm ); }
    LwRm::Handle GetCtxDmaHandleGpu(LwRm *pLwRm = 0) const;
    LwRm::Handle GetCtxDmaHandleIso(LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaHandleIso( pLwRm ); }
    UINT64 GetVidMemOffset(LwRm *pLwRm = 0) const { return m_Surface2D.GetVidMemOffset( pLwRm ); }
    void * GetAddress() const  { return m_Surface2D.GetAddress(  ); }
    PHYSADDR GetPhysAddress() const  { return m_Surface2D.GetPhysAddress(  ); }
    RC GetPhysAddress(UINT64 virtOffset, PHYSADDR* pPhysAddr, LwRm *pLwRm = 0) const;
    RC GetDevicePhysAddress(UINT64 virtOffset, Surface2D::VASpace vaSpace, PHYSADDR *pPhysAddr, LwRm *pLwRm = 0) const;
    UINT64 GetOffset(LwRm *pLwRm = 0) const { return m_Surface2D.GetOffset( pLwRm ); }
    UINT64 GetCtxDmaOffsetGpu(LwRm *pLwRm = 0) const;
    UINT64 GetCtxDmaOffsetIso(LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaOffsetIso( pLwRm ); }
    RC Alloc(GpuDevice *gpudev, LwRm *pLwRm = 0);
    LwRm* GetLwRmPtr() const { return m_Surface2D.GetLwRmPtr(); };
    void Free();
    RC Map(UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV, LwRm *pLwRm = 0) { return m_Surface2D.Map( subdev, pLwRm ); }
    RC MapPart(UINT32 part, UINT32 subdev, LwRm *pLwRm = 0) { return m_Surface2D.MapPart( part, subdev, pLwRm ); }
    RC MapRegion(UINT64 offset, UINT64 size,
                 UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV, LwRm *pLwRm = 0) { return m_Surface2D.MapRegion(offset, size, subdev, pLwRm ); }
    bool IsMapped() const  { return m_Surface2D.IsMapped(  ); }
    void Unmap() { m_Surface2D.Unmap(  ); }
    RC MapPeer();
    RC MapPeer(GpuDevice *pPeerDev);
    RC MapLoopback();
    RC MapLoopback(UINT32 loopbackPeerId);
    RC MapShared(GpuDevice *pRemoteDev) { return m_Surface2D.MapShared( pRemoteDev ); }
    bool IsPeerMapped(GpuDevice *pRemoteDev = NULL) const { return m_Surface2D.IsPeerMapped( pRemoteDev ); }
    bool IsMapShared(GpuDevice *pSharedDev) const { return m_Surface2D.IsMapShared( pSharedDev ); }
    UINT64 GetCtxDmaOffsetGpuPeer(UINT32 subdev, LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaOffsetGpuPeer( subdev, pLwRm ); }
    UINT64 GetCtxDmaOffsetGpuPeer(UINT32 subdev, UINT32 peerId, LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaOffsetGpuPeer( subdev, peerId, pLwRm ); }
    UINT64 GetGpuVirtAddrPeer(UINT32 subdev, LwRm *pLwRm = 0) const { return m_Surface2D.GetGpuVirtAddrPeer( subdev, pLwRm ); }
    LwRm::Handle GetVirtMemHandleGpuPeer(UINT32 subdev, LwRm *pLwRm = 0) const { return m_Surface2D.GetVirtMemHandleGpuPeer( subdev, pLwRm ); }
    LwRm::Handle GetCtxDmaHandleGpuPeer(GpuDevice *pRemoteDev,
                                        LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaHandleGpuPeer(pRemoteDev, pLwRm ); }
    UINT64 GetCtxDmaOffsetGpuPeer(UINT32     locSD,
            GpuDevice *pRemoteDev,
            UINT32     remSD,
            UINT32     peerId,
            LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaOffsetGpuPeer(locSD, pRemoteDev, remSD, peerId, pLwRm ); }
    UINT64 GetCtxDmaOffsetGpuPeer(UINT32     locSD,
            GpuDevice *pRemoteDev,
            UINT32     remSD,
            LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaOffsetGpuPeer(locSD, pRemoteDev, remSD, pLwRm ); }
    UINT64 GetGpuVirtAddrPeer(UINT32     locSD,
            GpuDevice *pRemoteDev,
            UINT32     remSD,
            LwRm *pLwRm = 0) const { return m_Surface2D.GetGpuVirtAddrPeer(locSD, pRemoteDev, remSD, pLwRm ); }
    UINT64 GetGpuVirtAddrPeer(UINT32     locSD,
            GpuDevice *pRemoteDev,
            UINT32     remSD,
            UINT32     peerId,
            LwRm *pLwRm = 0) const { return m_Surface2D.GetGpuVirtAddrPeer(locSD, pRemoteDev, remSD, peerId, pLwRm ); }
    LwRm::Handle GetVirtMemHandleGpuPeer(UINT32     locSD,
            GpuDevice *pRemoteDev,
            UINT32     remSD,
            LwRm *pLwRm = 0) const { return m_Surface2D.GetVirtMemHandleGpuPeer(locSD, pRemoteDev, remSD, pLwRm ); }
    LwRm::Handle GetVirtMemHandleGpuPeer(UINT32     locSD,
            GpuDevice *pRemoteDev,
            UINT32     remSD,
            UINT32     peerId,
            LwRm *pLwRm = 0) const { return m_Surface2D.GetVirtMemHandleGpuPeer(locSD, pRemoteDev, remSD, peerId, pLwRm ); }
    LwRm::Handle GetCtxDmaHandleGpuShared(GpuDevice *pSharedDev,
                                          LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaHandleGpuShared(pSharedDev, pLwRm ); }
    UINT64 GetCtxDmaOffsetGpuShared(GpuDevice *pSharedDev,
                                    LwRm *pLwRm = 0) const { return m_Surface2D.GetCtxDmaOffsetGpuShared(pSharedDev, pLwRm ); }
    UINT64 GetGpuVirtAddrShared(GpuDevice *pSharedDev,
                                LwRm *pLwRm = 0) const { return m_Surface2D.GetGpuVirtAddrShared(pSharedDev, pLwRm ); }
    UINT32 GetVirtMemHandleGpuShared(GpuDevice *pSharedDev,
                                     LwRm *pLwRm = 0) const { return m_Surface2D.GetVirtMemHandleGpuShared(pSharedDev, pLwRm ); }
    UINT32 GetBytesPerPixel() const { return m_Surface2D.GetBytesPerPixel(  ); }
    void SetBytesPerPixel(UINT32 value)  {  m_Surface2D.SetBytesPerPixel( value ); }
    UINT64 GetPixelOffset(UINT32 x, UINT32 y);
    UINT64 GetPixelOffset(UINT32 x, UINT32 y, UINT32 z, UINT32 a);
    UINT32 ReadPixel(UINT32 x, UINT32 y) const { return m_Surface2D.ReadPixel( x, y ); }
    UINT32 ReadPixel(UINT32 x, UINT32 y, UINT32 z, UINT32 a) const { return m_Surface2D.ReadPixel( x, y, z, a ); }
    void WritePixel(UINT32 x, UINT32 y, UINT32 Value) { m_Surface2D.WritePixel( x, y, Value ); }
    void WritePixel(UINT32 x, UINT32 y, UINT32 z, UINT32 a, UINT32 Value) { m_Surface2D.WritePixel( x, y, z, a, Value ); }
    RC Fill(UINT32 Value, UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) { return m_Surface2D.Fill( Value, subdev ); }
    RC FillRect(UINT32 x, UINT32 y, UINT32 Width, UINT32 Height, UINT32 Value,
            UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) { return m_Surface2D.FillRect(x, y, Width, Height, Value, subdev ); }
    RC FillRect64(UINT32 x, UINT32 y, UINT32 Width, UINT32 Height,
                  UINT16 r, UINT16 g, UINT16 b, UINT16 a,
                  UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) { return m_Surface2D.FillRect64(x, y, Width, Height, r, g, b, a, subdev ); }
    RC WriteTga(const char *FileName,
                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) { return m_Surface2D.WriteTga(FileName, subdev ); }
    RC WritePng(const char *FileName,
                UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) { return m_Surface2D.WritePng(FileName, subdev ); }
    RC CreatePitchImage(UINT08 *pExistingBuf, size_t bufSize, UINT32 subdev =
            Gpu::UNSPECIFIED_SUBDEV) { return m_Surface2D.CreatePitchImage(pExistingBuf, bufSize, subdev); }
    RC CreatePitchImage(vector<UINT08> *pBuf,
            UINT32 subdev = Gpu::UNSPECIFIED_SUBDEV) { return m_Surface2D.CreatePitchImage(pBuf, subdev ); }
    void ComputeParams() { m_Surface2D.ComputeParams(  ); }
    GpuDevice *GetGpuDev() const { return m_Surface2D.GetGpuDev(  ); }
    GpuSubdevice* GetGpuSubdev(UINT32 subdev) const { return m_Surface2D.GetGpuSubdev( subdev ); }
    void ConfigFromAttr(UINT32 Attr) { m_Surface2D.ConfigFromAttr( Attr ); }
    void ConfigFromAttr2(UINT32 Attr2) { m_Surface2D.ConfigFromAttr2( Attr2 ); }
    RC ReMapPhysMemory() { return m_Surface2D.ReMapPhysMemory(  ); }
    void SetIsSurfaceView()  {  m_Surface2D.SetIsSurfaceView(  ); }
    bool IsSurfaceView() const  { return m_Surface2D.IsSurfaceView(  ); }
    RC SetSurfaceViewParent(const MdiagSurf* surface, UINT64 offset);
    UINT64 GetCpuMapOffset() const  { return m_Surface2D.GetCpuMapOffset(  ); }
    void SetSkedReflected(bool b)  {  m_Surface2D.SetSkedReflected( b ); }
    bool GetSkedReflected() const  { return m_Surface2D.GetSkedReflected(  ); }
    void SetMappingObj(UINT32 h)   {  m_Surface2D.SetMappingObj( h ); }
    UINT32 GetMappingObj() const   { return m_Surface2D.GetMappingObj(  ); }
    const MdiagSurf* GetParentSurface() const  { return m_ParentSurface; }
    bool IsAllocated() const  { return m_Surface2D.IsAllocated( ); }
    bool IsGpuMapped() const  { return m_Surface2D.IsGpuMapped( ); }
    LwRm::Handle GetVASpaceHandle(Surface2D::VASpace vaSpace) const { return m_Surface2D.GetVASpaceHandle( vaSpace ); }
    bool IsUseVidHeapAlloc() const  { return m_Surface2D.IsUseVidHeapAlloc(  ); }
    void SetFbSpeed(Surface2D::FbSpeed speed)  {  m_Surface2D.SetFbSpeed( speed ); }
    Surface2D::FbSpeed GetFbSpeed() const  { return m_Surface2D.GetFbSpeed(  ); }

    bool GetSplit() const { return m_Surface2D.GetSplit(); }
    LwRm::Handle GetSplitMemHandle(LwRm *pLwRm = 0) const { return m_Surface2D.GetSplitMemHandle(pLwRm); }
    void SetAddressModel(Memory::AddressModel Model) { m_Surface2D.SetAddressModel(Model); }
    Memory::AddressModel GetAddressModel() const { return m_Surface2D.GetAddressModel(); }
    void SetSplitGpuCacheMode(Surface2D::GpuCacheMode mode) { m_Surface2D.SetSplitGpuCacheMode(mode); }
    RC BindGpuChannel(UINT32 hCh, LwRm *pLwRm = 0) { return m_Surface2D.BindGpuChannel(hCh, pLwRm); }
    void SetSpecialization(Surface2D::Specialization specialization) { m_Surface2D.SetSpecialization(specialization); }
    void SetShaderProtect(Memory::Protect ShaderProtect) { m_Surface2D.SetShaderProtect(ShaderProtect); }
    Memory::Protect GetShaderProtect() const { return m_Surface2D.GetShaderProtect(); }
    void SetGpuManagedChName(string chName) { m_Surface2D.SetGpuManagedChName(chName); }
    UINT64 GetCtxDmaOffsetGpuObject(LwRm::Handle hObject) const { return m_Surface2D.GetCtxDmaOffsetGpuObject(hObject); }
    UINT32 GetNumParts() const { return m_Surface2D.GetNumParts(); }
    UINT64 GetPartSize(UINT32 part) const { return m_Surface2D.GetPartSize(part); }
    void   SetGpuManagedChId(UINT32 id) { m_Surface2D.SetGpuManagedChId(id); }
    bool IsHostReflectedSurf() const { return m_Surface2D.IsHostReflectedSurf(); }
    void SetHostReflectedSurf() { m_Surface2D.SetHostReflectedSurf(); }
    bool GetGpuVirtAddrHintUseVirtAlloc() const { return m_Surface2D.GetGpuVirtAddrHintUseVirtAlloc(); }
    RC MapVirtToPhys(GpuDevice *gpudev, MdiagSurf *virtAlloc,
        MdiagSurf *physAlloc, UINT64 virtualOffset, UINT64 physicalOffset, LwRm* pLwRm);
    string GetGpuManagedChName() const { return m_Surface2D.GetGpuManagedChName(); }
    void SetSplitLocation(Memory::Location Location) { m_Surface2D.SetSplitLocation(Location); }
    void SetSplit(bool Split) { m_Surface2D.SetSplit(Split); }
    void SetSplitPteKind(INT32 PteKind) { m_Surface2D.SetSplitPteKind(PteKind); }
    void SetPartStride(UINT32 PartStride) { m_Surface2D.SetPartStride(PartStride); }
    const Surface2D::GpuObjectMappings& GetGpuObjectMappings() const { return m_Surface2D.GetGpuObjectMappings(); }
    void SetParentClass(UINT32 ClassNum) { m_Surface2D.SetParentClass(ClassNum); }
    UINT32 GetParentClass() const { return m_Surface2D.GetParentClass(); }
    void SetYCrCbType(ColorUtils::YCrCbType Type) { m_Surface2D.SetYCrCbType(Type); }
    ColorUtils::YCrCbType GetYCrCbType() { return m_Surface2D.GetYCrCbType(); }
    void SetDmaProtocol(Memory::DmaProtocol DmaProtocol) { m_Surface2D.SetDmaProtocol(DmaProtocol); }
    Memory::DmaProtocol GetDmaProtocol() const { return m_Surface2D.GetDmaProtocol(); }
    void SetDisplayable(bool Displayable) { m_Surface2D.SetDisplayable(Displayable); }
    bool GetDisplayable() const { return m_Surface2D.GetDisplayable(); }
    Memory::Location GetSplitLocation() const { return m_Surface2D.GetSplitLocation(); }
    Surface2D::GpuCacheMode GetSplitGpuCacheMode() const { return m_Surface2D.GetSplitGpuCacheMode(); }
    UINT64 GetPartOffset(UINT32 part) const { return m_Surface2D.GetPartOffset(part); }
    bool IsDualPageSize() const { return m_Surface2D.IsDualPageSize(); }
    UINT64 GetSplitVidMemOffset(LwRm *pLwRm = 0) const { return m_Surface2D.GetSplitVidMemOffset(pLwRm); }
    RC EnableDualPageSize(bool DualPageSize) { return m_Surface2D.EnableDualPageSize(DualPageSize); }
    void SetGpuVASpace(LwRm::Handle hVASpace) { m_Surface2D.SetGpuVASpace(hVASpace); }
    LwRm::Handle GetGpuVASpace() const { return m_Surface2D.GetGpuVASpace(); }
    void SetIsHtex(bool isHtex) { m_Surface2D.SetIsHtex(isHtex); }
    bool IsHtex(bool isHtex) const { return m_Surface2D.IsHtex(); }
    bool FormatHasXBits() const;

    RC DownloadWithCE
    (
        const ArgReader* params,
        GpuVerif* gpuVerif,
        UINT08* Data,
        UINT32 DataSize,
        UINT64 bufOffset,
        bool repeat,
        UINT32 subdev
    );

    Surface2D* GetSurf2D() { return &m_Surface2D; }
    const Surface2D* GetSurf2D() const { return &m_Surface2D; }

    void SetAtsMapped() { m_SurfIommuVAType = GVA; }
    bool IsAtsMapped() { return m_SurfIommuVAType == GVA; }
    void SetNoGmmuMap() { m_NoGmmuMap = true; }

    void SetAtsPageSize(UINT32 atsPageSize);
    UINT32 GetAtsPageSize() { return m_AtsPageSize; }
    void SetAtsReadPermission(bool value) { m_AtsReadPermission = value; }
    void SetAtsWritePermission(bool value) { m_AtsWritePermission = value; }
    RC UpdateAtsPermission(UINT64 virtAddr, const string &type, bool value);
    RC UpdateAtsMapping(IommuDrv::MappingType updateType, UINT64 virtAddr,
        UINT64 physAddr, const string &aperture);
    LwRm::Handle GetExternalPhysMem() const;
    bool IsExternalMemoryCpuMappable() const;

    void SetFixedPhysAddr(UINT64 address);
    UINT64 GetFixedPhysAddr() const;
    bool HasFixedPhysAddr() const;
    void SetPhysAddrRange(UINT64 minAddress, UINT64 maxAddress);
    UINT64 GetPhysAddrRangeMin() const;
    UINT64 GetPhysAddrRangeMax() const;
    bool HasPhysAddrRange() const;
    void SetFixedVirtAddr(UINT64 address);
    UINT64 GetFixedVirtAddr() const;
    bool HasFixedVirtAddr() const;
    void SetVirtAddrRange(UINT64 minAddress, UINT64 maxAddress);
    UINT64 GetVirtAddrRangeMin() const;
    UINT64 GetVirtAddrRangeMax() const;
    bool HasVirtAddrRange() const;
    void SetVASpaceId(UINT32 vaSpaceId) { m_Surface2D.SetVASpaceId(vaSpaceId); }
    bool NeedDirectMapping(const ArgReader *params, OpType type) const;
    bool UseCEForMemAccess(const ArgReader *params, OpType memOp) const;
    bool MemAccessNeedsColwersion(const ArgReader *params, OpType memOp) const;

    RC SetFlaImportMem(UINT64 address, UINT64 size, MdiagSurf* pDestinationSurface);
    bool HasFlaImportMem() const;
    const MdiagSurf* GetCpuMappableSurface() const;
    MdiagSurf* GetCpuMappableSurface();

    static bool s_RawImageMode;

    RC DuplicateSurface(LwRm * pLwRm) { return m_Surface2D.DuplicateSurface(pLwRm); }
    void SetGpuDev(GpuDevice * pGpuDev) { m_Surface2D.TempSetGpuDev(pGpuDev); }
    RC CreateSegmentList(string which, UINT64 offset, MdiagSegList** ppSegList);
    void SetChannel(LWGpuChannel * pCh) { m_pChannel = pCh; }
    LWGpuChannel * GetChannel() const { return m_pChannel; }
    void SetForceCpuMappable(bool value) { m_ForceCpuMappable = value; }
    bool IsOnSysmem() const;    

    void SendFreeAllocRangeEscapeWrite();

private:
    enum AddressConstraint
    {
        None,
        FixedAddress,
        AddressRange
    };

    struct FlaMemory
    {
        UINT64 address;
        UINT64 size;
        LwRm::Handle hMemory;
        MdiagSurf* pDestinationSurface;
    };

    RC CreateIommuMapping(UINT64 virtAddr, UINT64 physAddr, bool usePeer, UINT32 peerId);
    RC SendGmmuMapping(UINT64 virtAddr, UINT64 physAddr, bool usePeer, UINT32 peerId, bool doUnmap);
    RC SetPageSizeAttr(UINT32 *pAttr, UINT32 *pAttr2);
    RC AllocateExternalTraceMemory(GpuDevice *gpuDevice, LwRm *pLwRm);
    RC AllocateFlaImportMemory(GpuDevice* gpudev, LwRm *pLwRm);
    MdiagSurf* GetDestinationFlaSurface() const;
    bool GetDmaBufferAlloc() const { return m_Surface2D.GetDmaBufferAlloc(); }
    bool IsDirectMappable(OpType type) const;

    Surface2D m_Surface2D;
    GpuDevice* m_AccessingGpuDevice;
    GpuDevice* m_HostingGpuDevice;

    const MdiagSurf *m_ParentSurface; // parent surface of surface view
    SurfIommuVAType m_SurfIommuVAType;
    UINT32 m_AtsPageSize;  // In kilobytes
    bool m_AtsReadPermission;
    bool m_AtsWritePermission;
    bool m_NoGmmuMap;
    AddressConstraint m_PhysAddrConstraint;
    UINT64 m_FixedPhysAddr;
    UINT64 m_FixedPhysAddrMin;
    UINT64 m_FixedPhysAddrMax;
    AddressConstraint m_VirtAddrConstraint;
    UINT64 m_FixedVirtAddr;
    UINT64 m_FixedVirtAddrMin;
    UINT64 m_FixedVirtAddrMax;
    LwRm::Handle m_ExternalTraceMem;
    shared_ptr<FlaMemory> m_FlaImportMem = nullptr;
    bool m_useAlignedHeight;
    // If this surface represents a map-only surface, this member will point
    // to the surface that represents the physical allocation of this mapping.
    const MdiagSurf *m_PhysAlloc;
    bool m_GmmuMappingSent;

    // Send 'SurfaceType' escape with specified VA, PA to all subdevices
    // associated with this surface. Aperture is based off of surface location.
    RC SendSurfaceTypeEscapeWriteBuf(
        const UINT64 virtAddrBase,
        const UINT64 virtAddrOffset,
        const UINT64 virtAllocSize,
        const UINT64 physAddrBase,
        const UINT64 physAddrOffset,
        const bool isSparse,
        const string virtSurfName);

    // Send 'SurfaceType' escape  with specified VA, PA, aperture to a single subdevice.
    RC SendSurfaceTypeEscapeWriteBuf(
        GpuSubdevice* gpuSubDev,
        const UINT64 virtAddrBase,
        const UINT64 virtAddrOffset,
        const UINT64 virtAllocSize,
        const UINT64 physAddrBase,
        const UINT64 physAddrOffset,
        const string aperture,
        const bool isSparse,
        const string virtSurfName);

    // Send 'SurfaceTypeExtended' escape with specified VA, PA, isSparse to all subdevices
    // associated with this surface. Aperture is based off of surface location.
    RC SendSurfaceTypeExtendedEscapeWriteBuf(
        GpuSubdevice* gpuSubDev,
        const UINT64 virtAddrBase,
        const UINT64 virtAddrOffset,
        const UINT64 virtAllocSize,
        const string aperture,
        const UINT32 vaSpaceId,
        const UINT64 physAddrBase,
        const UINT64 physAddrOffset,
        const UINT64 mappedSize,
        const UINT32 pageSizeBit,
        const UINT32 pteKind,
        const bool isSparse,
        const string mappedSurfName,
        const string virtSurfName);

    LWGpuChannel * m_pChannel = nullptr;
    bool m_ForceCpuMappable = false;
};

class MdiagSegList
{
    friend RC MdiagSurf::CreateSegmentList(string which, UINT64 offset, MdiagSegList** ppSegList);
public:
    MdiagSegList(string Type);
    virtual ~MdiagSegList();

    void SetLwRmPtr(LwRm* pLwRm) { m_pLwRm = pLwRm; }
    void SetGpuDev(GpuDevice* pGpuDev) { m_pGpuDev = pGpuDev; }
    void SetLocation(UINT32 n, Memory::Location location) { m_RawMemoryList[n]->SetLocation(location); }
    void SetPhysAddress(PHYSADDR physAddress, UINT32 n) { m_RawMemoryList[n]->SetPhysAddress(physAddress); }

    const string &GetType() const { return m_Type; }
    UINT64 GetLength() const { return m_RawMemoryList.size(); }
    LwRm* GetLwRmPtr() const { return m_pLwRm; }
    GpuDevice *GetGpuDev() const { return m_pGpuDev; }
    Memory::Location GetLocation(UINT32 n) const { return m_RawMemoryList[n]->GetLocation(); }
    UINT64 GetSize(UINT32 n) const { return m_RawMemoryList[n]->GetSize(); }
    PHYSADDR GetPhysAddress(UINT32 n) const { return m_RawMemoryList[n]->GetPhysAddress(); }
    void* GetAddress(UINT32 n) const { return m_RawMemoryList[n]->GetAddress(); }
    LwRm::Handle GetMappedHandle(UINT32 n) const { return m_RawMemoryList[n]->GetMappedHandle(); }

    RC Map(UINT32 n) { return m_RawMemoryList[n]->Map(); }
    RC Unmap(UINT32 n) { return m_RawMemoryList[n]->Unmap(); }
    bool IsMapped(UINT32 n) const { return m_RawMemoryList[n]->IsMapped(); }

private:
    string m_Type;
    LwRm  *m_pLwRm;
    GpuDevice* m_pGpuDev;
    vector<unique_ptr<MDiagUtils::RawMemory> > m_RawMemoryList;
};
#endif
