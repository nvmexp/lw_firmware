/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2006-2022 by LWPU Corporation. All rights reserved.
 * All information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */
#ifndef INCLUDED_TRACERELOC_H
#define INCLUDED_TRACERELOC_H

#include "mdiag/utils/types.h"
#include "mdiag/utils/tracefil.h"
#include "mdiag/resource/lwgpu/verif/GpuVerif.h"
#include "gpu/utility/blocklin.h"
#include "gputrace.h"
#include "mdiag/utils/tex.h"

#include "class/cl9097.h" // FERMI_A
#include "class/cl90c0.h" // FERMI_COMPUTE_A

#define USE_DEFAULT_RM_MAPPING  (UINT32)~0

class TraceModule; // Here TraceModule is only used as a pointer. (tracemod.h)
class TextureHeader;
class GpuDevice;

//---------------------------------------------------------------------------
// Trace3d in Sli mode has different modes of operations:
// 1) "Milestone1 mode":
//   - All the surfaces are mirrored on the Gpu subdevices, and have the same
//     content;
//   - All the subdevices exelwtes the same pushbuffer;
//   - All the subdevices should return the same CRCs for each surface.
// 2) "Milestone2 mode":
//   - All the surfaces are mirrored and all the surfaces that are pointed
//   through a Texture header, can be peer-ed.
//   They typically, but not necessarily, have the same content.
//   - All the subdevices execute the same pushbuffer;
//   - If the peer-ed surfaces have the same content the CRCs must be the
//   same, otherwise the CRCs can be different.
// 3) "Milestone3 mode":
//   - All the surfaces are mirrored and peer-ed;
//   - All the subdevices execute the same pushbuffer, but it contains
//   SetMask commands, and so the subdevices can execute same operations on
//   different (eg peered surfaces).
//   - If the peer-ed surfaces have the same content the CRCs should be the
//   same, otherwise the CRCs can be different.
class TraceRelocation
{
public:
    TraceRelocation(UINT32 offset,
                    TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
                    UINT32 peerNum = Gpu::MaxNumSubdevices,
                    UINT32 peerID = USE_DEFAULT_RM_MAPPING);

    virtual ~TraceRelocation() {}

    virtual RC DoOffset(TraceModule *, UINT32 subdev) = 0;

    virtual void PrintHeader(TraceModule *module,
            const string &header,
            UINT32 subdev);

    //! Find all surfaces that this object refers to.  Used to
    //! associate channels with the surfaces they operate on.
    //! \param pModule The channel's pushbuffer
    //! \param[out] pSurfaces On exit, the surfaces are inserted into
    //!     *pSurfaces.  This method does not remove any entries that were
    //!     already in *pSurfaces on entry, so it can build a cumulative list.
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const = 0;

    //! Return the TraceModule target, or NULL if none
    virtual TraceModule *GetTarget() const;

    // *** For Debugging Purposes *** //
    // This variable determines if tihs instance of a TraceRelocation
    // should store the relocation in the map or patch the pushbuffer
    // immediately.
    bool m_UseMap;
protected:
    // All the DoOffset methods must check if they can be exelwted in the
    // current SLI mode, calling SliSupported.
    RC SliSupported(TraceModule *pModule) const;
    // For now all TraceRelocations are Milestone1 compliant.
    // Thus the base version of this function returns true.
    virtual bool IsSliMilestone1Compliant() const { return true; }
    // By default a TraceRelocation is not Milestone2 compliant.
    // Thus the base version of this function returns false.
    // All the relocations that support peer surface in SLI mode, should
    // overide this function and return true.
    virtual bool IsSliMilestone2Compliant() const { return false; }
    // By default a TraceRelocation is not Milestone3 compliant.
    // Thus the base version of this function returns false.
    // All the relocations that support peer surface in SLI mode, should
    // overide this function and return true.
    virtual bool IsSliMilestone3Compliant() const { return false; }

    //! Body of GetSurfaces() for subclasses that refer to surfaces by name.
    void GetSurfacesFromName(const string &surfName,
                             TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    //! Body of GetSurfaces() for subclasses that refer to surfaces by type.
    void GetSurfacesFromType(SurfaceType surfType,
                             TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    //! Body of GetSurfaces() for subclasses that refer to a surface by
    //! TraceModule.
    void GetSurfacesFromTarget(TraceModule *pTarget,
                               TraceModule *pModule,
                               SurfaceSet *pSurfaces) const;

    friend class GenericTraceModule;
    //! Get offset
    UINT32 GetOffset() const { return m_Offset; }
    //! Update offset
    void SetOffset(UINT32 val) { m_Offset = val; }

    UINT32 m_Offset;
    TraceModule::SubdevMaskDataMap *m_pSubdevMaskDataMap;
    UINT32 m_PeerNum;
    UINT32 m_PeerID;
};

class TraceRelocationMask: public TraceRelocation
{
public:
    TraceRelocationMask(UINT64 mask, UINT32 offset,
        TraceModule *target, bool badd,
        TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
        UINT32 peerNum = Gpu::MaxNumSubdevices,
        UINT32 peerID = USE_DEFAULT_RM_MAPPING)
        : TraceRelocation(offset, pSubdevMaskDataMap, peerNum, peerID), m_pTarget(target),
        m_mask(mask), m_Add(badd) {}
    virtual ~TraceRelocationMask() {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev) = 0;
    virtual TraceModule *GetTarget() const;

protected:
    UINT64 MaskedValue(UINT64 val, UINT64 mask) const;
    UINT64 UnMaskedValue(UINT64 val, UINT64 mask) const;
    UINT64 CallwlateValue(UINT64 orig_val, UINT64 new_val, UINT64 mask_in) const;
    UINT64 CallwlateScaledValue(UINT64 origVal, UINT64 scale, UINT64 maskIn) const;

    TraceModule *m_pTarget;
    UINT64 m_mask;
    bool m_Add;
};

class TraceRelocationCtxDMAHandle : public TraceRelocation
{
public:
    TraceRelocationCtxDMAHandle( UINT32 offset, string surfname,
            TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap)
        : TraceRelocation(offset, pSubdevMaskDataMap), m_SurfName(surfname) {}

    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;

private:
    string m_SurfName;
};

class TraceRelocationOffset : public TraceRelocation
{
public:
    TraceRelocationOffset(UINT32 offset, GpuTrace* trace, string surfname,
            TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
            UINT32 peerNum, UINT32 peerID)
        : TraceRelocation(offset, pSubdevMaskDataMap, peerNum, peerID),
          m_pTrace(trace), m_SurfName(surfname) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;

protected:
    // This relocation is SLI Milestone2/3 compliant.
    bool IsSliMilestone2Compliant() const { return true; }
    bool IsSliMilestone3Compliant() const { return true; }

private:
    const GpuTrace* m_pTrace;
    string m_SurfName;
};

class TraceRelocationFile : public TraceRelocationMask
{
public:
    TraceRelocationFile(UINT32 offset, TraceModule *target,
                        UINT64 mask_out, UINT64 mask_in,
                        UINT32 target_offset = 0,
                        TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap = 0,
                        UINT32 peerNum = Gpu::MaxNumSubdevices)
        : TraceRelocationMask(mask_out, offset, target, true, pSubdevMaskDataMap, peerNum),
          m_TargetOffset(target_offset),
          m_MaskIn(mask_in), m_Swap(false),
          m_bUseTex(false) {}
    TraceRelocationFile(UINT32 offset, TraceModule *target,
                        UINT64 mask_out, UINT64 mask_in, bool swap,
                        UINT32 array_index, UINT32 lwbemap_face,
                        UINT32 miplevel, UINT32 x, UINT32 y, UINT32 z,
                        TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap = 0,
                        UINT32 peerNum = Gpu::MaxNumSubdevices)
        : TraceRelocationMask(mask_out, offset, target, true, pSubdevMaskDataMap, peerNum),
          m_TargetOffset(0),
          m_MaskIn(mask_in),
          m_Swap(swap),
          m_bUseTex(true),
          m_ArrayIndex(array_index),
          m_LwbmapFace(lwbemap_face),
          m_Miplevel(miplevel),
          m_X(x), m_Y(y), m_Z(z) { }
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;

private:
    UINT32 m_TargetOffset;
    UINT64 m_MaskIn;
    bool   m_Swap;
    bool   m_bUseTex;
    UINT32 m_ArrayIndex;
    UINT32 m_LwbmapFace;
    UINT32 m_Miplevel;
    UINT32 m_X;
    UINT32 m_Y;
    UINT32 m_Z;
};

class TraceRelocationFileMs32 : public TraceRelocation
{
public:
    TraceRelocationFileMs32(UINT32 offset, TraceModule *target,
            TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
            UINT32 peerNum)
        : TraceRelocation(offset, pSubdevMaskDataMap, peerNum), m_pTarget(target) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    virtual TraceModule *GetTarget() const;

private:
    TraceModule *m_pTarget;
};

class TraceRelocationSurface : public TraceRelocationMask
{
public:
    // For some reason, relocations used to relocate texture offset in
    // texture header are swapped we need to be able to do the same thing
    // here for RTT
    TraceRelocationSurface(UINT32 offset, SurfaceType Surf,
                           UINT32 surf_offset, bool Swapped,
                           TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
                           UINT32 peerNum, UINT64 mask_out, UINT64 mask_in)
        : TraceRelocationMask(mask_out, offset, 0, true, pSubdevMaskDataMap, peerNum), m_Surf(Surf),
          m_SurfName(""), m_SurfOffset(surf_offset), m_Swapped(Swapped), m_GpuDev(0),
          m_MaskIn(mask_in) {}
    TraceRelocationSurface(UINT32 offset, string& Surf,
                           UINT32 array_index, UINT32 lwbemap_face,
                           UINT32 miplevel, UINT32 x, UINT32 y, UINT32 z,
                           bool Swapped,
                           TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
                           UINT32 peerNum, GpuDevice *gpudev,
                           UINT64 mask_out, UINT64 mask_in)
        : TraceRelocationMask(mask_out, offset, 0, true, pSubdevMaskDataMap, peerNum),
          m_Surf(SURFACE_TYPE_UNKNOWN),
          m_SurfName(Surf),
          m_SurfOffset(0),
          m_Swapped(Swapped),
          m_ArrayIndex(array_index),
          m_LwbmapFace(lwbemap_face),
          m_Miplevel(miplevel),
          m_X(x), m_Y(y), m_Z(z),
          m_GpuDev(gpudev),
          m_MaskIn(mask_in) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    virtual ~TraceRelocationSurface();

private:
    SurfaceType m_Surf;
    string m_SurfName;
    UINT64 m_SurfOffset;
    bool m_Swapped;
    UINT32 m_ArrayIndex;
    UINT32 m_LwbmapFace;
    UINT32 m_Miplevel;
    UINT32 m_X;
    UINT32 m_Y;
    UINT32 m_Z;
    GpuDevice *m_GpuDev;
    UINT64 m_MaskIn;
};

class TraceRelocation40BitsSwapped: public TraceRelocation
{
public:
    TraceRelocation40BitsSwapped(UINT32 offset, TraceModule *target,
            TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap, bool peer,
            UINT32 peerNum)
        : TraceRelocation(offset, pSubdevMaskDataMap, peerNum),
          m_pTarget(target), m_Peer(peer) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    virtual TraceModule *GetTarget() const;

protected:
    // This relocation is SLI Milestone2/3 compliant.
    bool IsSliMilestone2Compliant() const { return true; }
    bool IsSliMilestone3Compliant() const { return true; }

private:
    TraceModule *m_pTarget;
    bool        m_Peer;
};

class TraceRelocationDynTex: public TraceRelocation
{
public:
    enum {TEXTURE_HEADER_STATE_SIZE = 8};

    TraceRelocationDynTex
    (
        string &surf,
        TextureMode Mode,
        UINT32 index,
        bool no_offset,
        bool no_sa,
        TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
        UINT32 peerNum,
        bool center_spoof,
        TextureHeader::HeaderFormat headerFormat,
        bool bOptimalPromotion = false
    ) : TraceRelocation(index, pSubdevMaskDataMap, peerNum), m_SurfName(surf),
        m_Mode(Mode),
        m_NoOffset(no_offset),
        m_NoSurfaceAttr(no_sa),
        m_CenterSpoof(center_spoof),
        m_TextureHeaderFormat(headerFormat),
        m_OptimalPromotion(bOptimalPromotion)
    {
    }

    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;

private:
    string      m_SurfName;
    TextureMode m_Mode;
    bool        m_NoOffset;
    bool        m_NoSurfaceAttr;
    bool        m_CenterSpoof;  // Enable 2X2_CENTER etc mode by command line
    TextureHeader::HeaderFormat m_TextureHeaderFormat;
    bool        m_OptimalPromotion;
};

class TraceRelocationLong: public TraceRelocation
{
public:
    TraceRelocationLong(UINT32 offset_low, UINT32 offset_hi,
            SurfaceType surf, TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
            UINT32 peerNum)
        : TraceRelocation(offset_low, pSubdevMaskDataMap, peerNum), m_pTarget(0),
        m_OffsetHi(offset_hi), m_Surf(surf) {}
    TraceRelocationLong(UINT32 offset_low, UINT32 offset_hi,
            TraceModule *target, TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
            UINT32 peerNum)
        : TraceRelocation(offset_low, pSubdevMaskDataMap, peerNum), m_pTarget(target),
        m_OffsetHi(offset_hi) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    virtual TraceModule *GetTarget() const;

private:
    TraceModule *m_pTarget;
    UINT32       m_OffsetHi;
    SurfaceType  m_Surf;
};

class TraceRelocationSizeLong: public TraceRelocation
{
public:
    TraceRelocationSizeLong(UINT32 offset_low, UINT32 offset_hi,
            TraceModule *target, TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap)
        : TraceRelocation(offset_low, pSubdevMaskDataMap), m_pTarget(target),
        m_OffsetHi(offset_hi) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    virtual TraceModule *GetTarget() const;

protected:
    // This relocation is SLI Milestone2/3 compliant.
    bool IsSliMilestone2Compliant() const { return true; }
    bool IsSliMilestone3Compliant() const { return true; }

private:
    TraceModule *m_pTarget;
    UINT32       m_OffsetHi;
};

class TraceRelocationClear: public TraceRelocation
{
public:
    TraceRelocationClear(UINT32 offset,
        TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap) :
            TraceRelocation(offset, pSubdevMaskDataMap) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const {}
};

class TraceRelocationZLwll: public TraceRelocation
{
public:
    TraceRelocationZLwll(UINT32 offset_a, UINT32 offset_b, UINT32 offset_c,
            UINT32 offset_d, const MdiagSurf* zlwll_storage, string surfname,
            TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap,
            UINT32 peerNum)
        : TraceRelocation(0, pSubdevMaskDataMap, peerNum), m_OffsetA(offset_a),
        m_OffsetB(offset_b), m_OffsetC(offset_c), m_OffsetD(offset_d),
        m_Storage(zlwll_storage), m_ZBuffer(surfname) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;

private:
    UINT32 m_OffsetA, m_OffsetB, m_OffsetC, m_OffsetD;
    const MdiagSurf* m_Storage;
    string     m_ZBuffer;
};

class TraceRelocationActiveRegion: public TraceRelocation
{
public:
    TraceRelocationActiveRegion(UINT32 offset, const char* target,
            GpuTrace* trace, TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap, INT32 forcedRegionId = -1)
        : TraceRelocation(offset, pSubdevMaskDataMap), m_forcedRegionId(forcedRegionId), m_pTarget(target),
        m_pTrace(trace) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;

private:
    INT32 m_forcedRegionId;
    string m_pTarget;
    const GpuTrace *m_pTrace;
};

class TraceRelocationType: public TraceRelocation
{
public:
    TraceRelocationType(UINT32 offset, TraceModule *target,
        TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap) :
            TraceRelocation(offset, pSubdevMaskDataMap), m_pTarget(target) {}
    virtual ~TraceRelocationType() {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    virtual TraceModule *GetTarget() const;

protected:
    // This relocation is SLI Milestone2/3 compliant.
    bool IsSliMilestone2Compliant() const { return true; }
    bool IsSliMilestone3Compliant() const { return true; }

private:
    TraceModule *m_pTarget;
};

class TraceRelocationBranch: public TraceRelocationMask
{
public:
    TraceRelocationBranch(UINT64 mask_out, UINT64 mask_in, UINT32 offset,
            bool bSwap, TraceModule *target, bool badd,
            TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap)
        : TraceRelocationMask(mask_out, offset, target, badd, pSubdevMaskDataMap), m_MaskIn(mask_in), m_Swap(bSwap) {}
    virtual ~TraceRelocationBranch() {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;

protected:
    // This relocation is SLI Milestone2/3 compliant.
    bool IsSliMilestone2Compliant() const { return true; }
    bool IsSliMilestone3Compliant() const { return true; }

    UINT64 m_MaskIn;
    bool m_Swap;
};

class TraceRelocationSize: public TraceRelocationMask
{
public:
    TraceRelocationSize(UINT64 mask_out, UINT64 mask_in, UINT32 offset, const string& surf_name,
            bool badd, TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap)
        : TraceRelocationMask(mask_out, offset, 0, badd, pSubdevMaskDataMap),
          m_MaskIn(mask_in), m_SurfName(surf_name) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    bool operator== (const TraceRelocationSize& relocSize) const;
protected:
    // This relocation is SLI Milestone2/3 compliant.
    bool IsSliMilestone2Compliant() const { return true; }
    bool IsSliMilestone3Compliant() const { return true; }

    UINT64 m_MaskIn;
    const string m_SurfName;
};

class TraceRelocationSurfaceProperty: public TraceRelocationMask
{
public:
    TraceRelocationSurfaceProperty
    (
        UINT64 mask_out,
        UINT64 mask_in,
        UINT32 offset,
        string &property,
        const string &surfaceName,
        SurfaceType surf_type,
        TraceModule *target,
        bool badd,
        bool bswap,
        bool progZtAsCt0,
        TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap
    )
    :   TraceRelocationMask(mask_out, offset, target, badd, pSubdevMaskDataMap),
        m_MaskIn(mask_in),
        m_Property(property),
        m_SurfaceName(surfaceName),
        m_SurfType(surf_type),
        m_Swap(bswap),
        m_ProgZtAsCt0(progZtAsCt0)
    {
    }

    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
protected:
    UINT64 m_MaskIn;
    string m_Property;
    string m_SurfaceName;
    SurfaceType m_SurfType;
    bool m_Swap;
    bool m_ProgZtAsCt0;
};

class TraceRelocation64 : public TraceRelocationMask
{
public:
    TraceRelocation64(UINT64 mask_out, UINT64 mask_in, UINT32 offset, bool swap,
        string module_name, bool peer, bool badd, TraceModule* target,
        TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap = 0,
        UINT32 peerNum = Gpu::MaxNumSubdevices,
        UINT32 peerID = USE_DEFAULT_RM_MAPPING,
        UINT64 signExtendMask = 0,
        string signExtendMode = "");
    virtual ~TraceRelocation64() {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;

protected:
    // This relocation is SLI Milestone2/3 compliant.
    bool IsSliMilestone2Compliant() const { return true; }
    bool IsSliMilestone3Compliant() const { return true; }

    bool   m_Swap;
    UINT64 m_MaskIn;
    string m_SurfName;
    bool   m_Peer;
    UINT64 m_SignExtendMask;
    string m_SignExtendMode;
};

class TraceRelocatiolwP2Pitch : public TraceRelocation
{
public:
    TraceRelocatiolwP2Pitch(UINT32 offset, TraceModule *target,
            UINT32 StartBitPosition, UINT32 NumberOfBits,
            TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap)
        : TraceRelocation(offset, pSubdevMaskDataMap), m_pTarget(target),
        m_StartBitPosition(StartBitPosition),
        m_NumberOfBits(NumberOfBits) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    virtual TraceModule *GetTarget() const;

private:
    TraceModule *m_pTarget;
    UINT32       m_StartBitPosition;
    UINT32       m_NumberOfBits;
};

class TraceRelocatiolwP2TileMode : public TraceRelocation
{
public:
    TraceRelocatiolwP2TileMode(UINT32 offset, TraceModule *target,
            UINT32 StartBitPosition, UINT32 NumberOfBits,
            TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap)
        : TraceRelocation(offset, pSubdevMaskDataMap), m_pTarget(target),
        m_StartBitPosition(StartBitPosition),
        m_NumberOfBits(NumberOfBits) {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const;
    virtual TraceModule *GetTarget() const;

private:
    TraceModule *m_pTarget;
    UINT32       m_StartBitPosition;
    UINT32       m_NumberOfBits;
};

class TraceRelocationConst32 : public TraceRelocation
{
public:
    TraceRelocationConst32(UINT32 offset, UINT32 value,
            TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap = 0)
        : TraceRelocation(offset, pSubdevMaskDataMap), m_Value(value) {}
    virtual ~TraceRelocationConst32() {}
    virtual RC DoOffset(TraceModule *, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
                             SurfaceSet *pSurfaces) const {}

private:
    UINT32 m_Value;
};

class TraceRelocationScale : public TraceRelocationMask
{
public:
    TraceRelocationScale
    (
        UINT64 maskOut,
        UINT32 offset,
        UINT32 offset2,
        TraceModule *target,
        bool swapWords,
        bool scaleByTPCCount,
        bool scaleByComputeWarpsPerTPC,
        bool scaleByGraphicsWarpsPerTPC,
        TraceModule::SubdevMaskDataMap *pSubdevMaskDataMap
    )
    : TraceRelocationMask(maskOut, offset, target, false, pSubdevMaskDataMap),
      m_Offset2(offset2),
      m_SwapWords(swapWords),
      m_ScaleByTPCCount(scaleByTPCCount),
      m_ScaleByComputeWarpsPerTPC(scaleByComputeWarpsPerTPC),
      m_ScaleByGraphicsWarpsPerTPC(scaleByGraphicsWarpsPerTPC) {}

    virtual RC DoOffset(TraceModule *module, UINT32 subdev);
    virtual void GetSurfaces(TraceModule *pModule,
        SurfaceSet *pSurfaces) const {}

protected:
    UINT32 m_Offset2;
    bool m_SwapWords;
    bool m_ScaleByTPCCount;
    bool m_ScaleByComputeWarpsPerTPC;
    bool m_ScaleByGraphicsWarpsPerTPC;
};

#endif
