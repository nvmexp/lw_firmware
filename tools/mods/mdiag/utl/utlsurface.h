/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#ifndef INCLUDED_UTLSURFACE_H
#define INCLUDED_UTLSURFACE_H

#include "mdiag/utils/mdiagsurf.h"

#include "utlpython.h"

class UtlChannel;
class UtlGpuVerif;
class UtlTest;

class UtlMmuEntry;
class UtlMmuSegment;
class UtlVaSpace;
class UtlGpu;

#include "mdiag/utils/mmuutils.h"
#include "utlmmu.h"

// This class represents a logical surface from the point of view of a
// UTL script.  Lwrrently this class is a wrapper around MdiagSurf, but
// a one-to-one mapping between UtlSurface and MdiagSurf objects is not
// required.  For example, a future enhancement might allow multiple
// UtlSurface objects to be designated as part of one chunk allocation.
// Those UtlSurface objects could all share the same MdiagSurf object.
//
class UtlSurface
{
public:
    using CreateFunction = function<unique_ptr<UtlSurface>(MdiagSurf*)>;

    static void Register(py::module module);

    virtual ~UtlSurface();

    static UtlSurface* GetFromMdiagSurf(MdiagSurf* mdiagSurface);
    static UtlSurface* GetFromMdiagSurf(MdiagSurf* mdiagSurface, CreateFunction createFunction);
    static void FreeSurfaces();
    static void FreeSurface(MdiagSurf* mdiagSurface);

    // SAMPLE_MODE
    enum AAMode
    {
        AA_1X1,
        AA_2X1,
        AA_2X1_D3D,
        AA_2X2,
        AA_4X2,
        AA_4X2_D3D,
        AA_4X4,
        AA_2X2_VC_4,
        AA_2X2_VC_12,
        AA_4X2_VC_8,
        AA_4X2_VC_24
    };

    enum AttrCompr : UINT32
    {
        ComprNone = LWOS32_ATTR_COMPR_NONE,
        ComprRequired = LWOS32_ATTR_COMPR_REQUIRED,
        ComprAny = LWOS32_ATTR_COMPR_ANY
    };

    enum AttrZLwll : UINT32
    {
        ZLwllNone = LWOS32_ATTR_ZLWLL_NONE,
        ZLwllRequired = LWOS32_ATTR_ZLWLL_REQUIRED,
        ZLwllAny = LWOS32_ATTR_ZLWLL_ANY
    };

    enum AttrType : UINT32
    {
        TypeImage = LWOS32_TYPE_IMAGE,
        TypeDepth = LWOS32_TYPE_DEPTH,
        TypeTexture = LWOS32_TYPE_TEXTURE,
        TypeVideo = LWOS32_TYPE_VIDEO
    };

    // GPU cache, SMMU or ZBC mode on-off
    enum Toggle
    {
        ToggleDefault = 0,
        ToggleOff = 1,
        ToggleOn = 2,
        NumToggle
    };

    // The following functions are called from the Python interpreter.
    enum FORMAT
    {
        RAW,
        PITCH
    };

    enum VirtualAddressType
    {
        GmmuLocal,
        GmmuPeer
    };

    enum PhysicalAddressType
    {
        Cpu,
        Gpu
    };

    enum class PteKindDataType
    {
        Str,
        Num
    };

    static UtlSurface* Create(py::kwargs kwargs);
    // Create a UtlSurface from an existent one.
    static UtlSurface* CreateFromSurface(py::kwargs kwargs);
    void Allocate(py::kwargs kwargs);
    void Free();
    void WriteData(py::kwargs kwargs);
    vector<UINT08> ReadData(py::kwargs kwargs);
    UINT64 GetVirtualAddress(py::kwargs kwargs) const;
    UINT64 GetPhysicalAddress(py::kwargs kwargs);
    void MapPy();
    void UnmapPy();
    bool IsAllocated() const;
    void SetChannel(UtlChannel* channel);
    UtlChannel * GetChannel();
    UINT64 GetPageSize() const;
    void SetPageSize(UINT64 pageSize);

    // The following functions are also called from the Python interpreter,
    // specifically as property accessor functions.
    bool GetDisplayable() const { return m_Surface->GetDisplayable(); }
    void SetDisplayable(bool displayable);
    bool GetUpr() const { return m_Surface->GetAllocInUpr(); }
    void SetUpr(bool bAllocInUpr);
    string GetAccess() const;
    void SetAccess(const string& access);
    string GetAperture() const;
    void SetAperture(const string& aperture);
    UINT32 GetHeight() const { return m_Surface->GetHeight(); }
    void SetHeight(UINT32 height);
    string GetName() const { return m_Surface->GetName(); }
    bool GetPhysContig() const { return m_Surface->GetPhysContig(); }
    void SetPhysContig(bool physContig);
    UINT64 GetSize() const { return m_Surface->GetArrayPitch(); }
    void SetSize(UINT64 size);
    py::object GetFixedVirtualAddress() const;
    void SetFixedVirtualAddress(py::object address);
    py::object GetVirtualAddressRange() const;
    void SetVirtualAddressRange(py::object range);
    py::object GetFixedPhysicalAddress() const;
    void SetFixedPhysicalAddress(py::object address);
    py::object GetPhysicalAddressRange() const;
    void SetPhysicalAddressRange(py::object range);
    UINT32 GetWidth() const { return m_Surface->GetWidth(); }
    void SetWidth(UINT32 width);
    string GetColorFormat() const;
    void SetColorFormat(string formatString);
    AAMode GetAAMode() const;
    void SetAAMode(const AAMode propVal);
    Surface2D::Layout GetLayout() const;
    void SetLayout(const Surface2D::Layout propVal);
    AttrCompr GetCompression() const;
    void SetCompression(const AttrCompr propVal);
    AttrZLwll GetZLwll() const;
    void SetZLwll(const AttrZLwll propVal);
    AttrType GetType() const;
    void SetType(const AttrType propVal);
    Memory::Protect GetShaderProtect() const;
    void SetShaderProtect(const Memory::Protect propVal);
    bool GetPriv() const { return m_Surface->GetPriv(); }
    void SetPriv(bool propVal);
    Toggle GetGpuCacheMode() const;
    void SetGpuCacheMode(const Toggle propVal);
    Toggle GetP2PGpuCacheMode() const;
    void SetP2PGpuCacheMode(const Toggle propVal);
    Toggle GetZbcMode() const;
    void SetZbcMode(const Toggle propVal);
    UINT32 GetDepth() const { return m_Surface->GetDepth(); }
    void SetDepth(UINT32 propVal);
    UINT32 GetPitch() const { return m_Surface->GetPitch(); }
    void SetPitch(UINT32 propVal);
    UINT32 GetLogBlockWidth() const { return m_Surface->GetLogBlockWidth(); }
    void SetLogBlockWidth(UINT32 propVal);
    UINT32 GetLogBlockHeight() const { return m_Surface->GetLogBlockHeight(); }
    void SetLogBlockHeight(UINT32 propVal);
    UINT32 GetLogBlockDepth() const { return m_Surface->GetLogBlockDepth(); }
    void SetLogBlockDepth(UINT32 propVal);
    UINT32 GetArraySize() const { return m_Surface->GetArraySize(); }
    void SetArraySize(UINT32 propVal);
    UINT32 GetMipLevels() const { return m_Surface->GetMipLevels(); }
    void SetMipLevels(UINT32 propVal);
    UINT32 GetBorder() const { return m_Surface->GetBorder(); }
    void SetBorder(UINT32 propVal);
    bool GetLwbemap() const { return m_Surface->GetDimensions() == 6 ? true : false; }
    void SetLwbemap(bool propVal);
    UINT64 GetAlignment() const { return m_Surface->GetAlignment(); }
    void SetAlignment(UINT64 propVal);
    UINT64 GetVirtAlignment() const { return m_Surface->GetVirtAlignment(); }
    void SetVirtAlignment(UINT64 propVal);
    UINT32 GetAttrOverride() const;
    void ConfigFromAttr(UINT32 propVal);
    UINT32 GetAttr2Override() const;
    void ConfigFromAttr2(UINT32 propVal);
    // TYPE_OVERRIDE not yet added, owned by TraceModule
    bool GetLoopback() const { return m_Surface->GetLoopBack(); }
    void SetLoopback(bool loopback);
    bool GetSkedReflected() const { return m_Surface->GetSkedReflected(); }
    void SetSkedReflected(bool propVal);
    // PAGE_SIZE not yet added, not GpuDev in UtlSurface
    Surface2D::MemoryMappingMode GetMemoryMappingMode() const
    {
        return m_Surface->GetMemoryMappingMode();
    }
    void SetMemoryMappingMode(const Surface2D::MemoryMappingMode propVal);
    // VPR not yet added, owned by TraceModule
    UINT32 GetTileWidthInGobs() const { return m_Surface->GetTileWidthInGobs(); }
    void SetTileWidthInGobs(UINT32 propVal);
    bool GetAtsMapped() const { return m_Surface->IsAtsMapped(); }
    void SetAtsMapped(bool propVal);
    bool GetNoGmmuMap() const;
    void SetNoGmmuMap(bool propVal);
    UINT32 GetAtsPageSize() const { return m_Surface->GetAtsPageSize(); }
    void SetAtsPageSize(UINT32 propVal);
    bool GetIsHtex() const { return m_Surface->GetSurf2D()->IsHtex(); }
    void SetIsHtex(bool propVal);
    void SetSpecialization(py::kwargs kwargs);
    // Keep consistant with Surface2D and MdiagSurf to have
    // IsVirtualOnly() and HasVirtual(), etc., but no GetSpecialization()
    bool IsVirtualOnly() const { return m_Surface->IsVirtualOnly(); }
    bool IsPhysicalOnly() const { return m_Surface->IsPhysicalOnly(); }
    bool IsMapOnly() const { return m_Surface->IsMapOnly(); }
    bool HasVirtual() const { return m_Surface->HasVirtual(); }
    bool HasPhysical() const { return m_Surface->HasPhysical(); }
    bool HasMap() const { return m_Surface->HasMap(); }
    UINT32 CallwlateCrc(py::kwargs kwargs);
    string GetCrcKey(py::kwargs kwargs);
    void DumpImageToFile(py::kwargs kwargs);
    MdiagSurf * GetRawPtr() const { return m_Surface; }
    UINT64 GetOffset() const { return m_Surface->GetOffset(); }
    void SetIsSurfaceView();

    void SetTest(UtlTest * pTest) { m_pTest = pTest; }
    void SetVaSpacePy(py::kwargs kwargs);
    void SetFlaImportPy(py::kwargs kwargs);

    // Map a virtual allocation to a physical allocation.  This Surface2D
    // represents the map and the other two Surface2D objects represent the
    // virtual and physical allocations.
    void MapVirtToPhys(py::kwargs kwargs);
    // Align with MdiagSurf and Surf2D to export this function,
    // for surface view case, set the parent surface
    // and the DmaBuffer offset.
    void SetSurfaceViewParent(py::kwargs kwargs);

    py::list GetEntryListsPy(py::kwargs kwargs);
    vector<UtlMmuEntry *> GetEntryLists(UtlMmu::LEVEL level,
                                     UtlMmu::SUB_LEVEL_INDEX subLevelIdx,
                                     UINT64 offset, UINT32 size);

    UINT64 CreatePeerMappingPy(py::kwargs kwargs);
    py::object GetPteKind(py::kwargs kwargs) const;
    void SetPteKind(py::kwargs kwargs);

    LwRm* GetLwRm() const;
    LwRm::Handle GetVASpaceHandle() const;
    UtlVaSpace * GetVaSpacePy();
    LwRm::Handle GetVaSpace() const { return m_Surface->GetGpuVASpace();}

    UtlGpu* GetGpu() const;

    LwRm::Handle GetMemHandle() const;

    void FreeMmuLevels();

    // Make sure the compiler doesn't implement default versions
    // of these constructors and assignment operators.  This is so we can
    // catch errors with incorrect references on the Python side.
    UtlSurface() = delete;
    UtlSurface(UtlSurface&) = delete;
    UtlSurface& operator=(UtlSurface&) = delete;

private:
    UtlTest * GetTest() const { return m_pTest; }
    UtlGpuVerif * GetGpuVerif(UtlChannel * pChannel);
    void ReadRawData(UINT64 offset, UINT64 size, void * data);
    void ReadPitchData(UINT64 offset, UINT64 size, void * data, UtlChannel * pCh);

    void CheckIfAllocated(const char* prop) const;
    LwRm::Handle GetVaSpaceHandle() const { return m_Surface->GetVASpaceHandle(m_Surface->GetVASpace()); }
    RC Map();
    void Unmap();

    RC GetMmuLevel(MmuLevelTree::LevelIndex index, MmuLevel** ppMmuLevel, MmuLevelTree * pMmuLevelTree);
    RC ExploreMmuLevelTree();

    UtlMmuSegment * GetUtlMmuSegment(MmuLevelSegment * pSegment, UtlMmu::SUB_LEVEL_INDEX sublevelIdx);

    UINT64 GetGmmuPeerVirtualAddress(UtlGpu* accessingGpu) const;

    // This is the MdiagSurf object that is wrapped by this class.
    // The pointer can come from a Test object or it can come from this
    // object (in the case that the surface is created by UTL script).
    MdiagSurf* m_Surface;

    // If this UtlSurface object was created via UTL script, then this
    // pointer will have ownership of the corresponding MdiagSurf object.
    // If this UtlSurface was created by an mdiag Test object
    // (e.g. Trace3DTest), then the test has ownership of the
    // MdiagSurf object and this pointer will be null.
    unique_ptr<MdiagSurf> m_OwnedSurface;

    LwRm *m_LwRm = nullptr;

    // A map of all UtlSurface objects, referenced by MdiagSurf pointer.
    static map<MdiagSurf*, unique_ptr<UtlSurface>> s_Surfaces;

    // This map holds pointers to all surfaces that are created by UTL scripts.
    // The first level of the map is referenced by either a test pointer
    // (for test-specific scripts) or nullptr, for global scripts.
    // The second level of the map is referenced by surface name.
    static map<UtlTest*, map<string, UtlSurface*>> s_ScriptSurfaces;

    UtlGpuVerif * m_pGpuVerif = nullptr;
    UtlTest * m_pTest = nullptr;
    UtlChannel * m_pChannel = nullptr;
    bool m_KeepCpuMapping = false;
    int m_CpuMapCount = 0;

    MmuLevelTree * m_pMmuLevelTree = nullptr;
    map<MmuLevelSegment *, unique_ptr<UtlMmuSegment> > m_UtlMmuSegments;

protected:
    // UtlSurface objects should only be created by the static Create function,
    // or through a subclass.
    UtlSurface(MdiagSurf* mdiagSurface);
    static UtlSurface* DoCreate(py::kwargs kwargs, CreateFunction createFunction);
    virtual RC DoAllocate(MdiagSurf* pMdiagSurf, GpuDevice *pGpuDevice);
    virtual void DoSetSize(UINT64 size);

};

#endif
