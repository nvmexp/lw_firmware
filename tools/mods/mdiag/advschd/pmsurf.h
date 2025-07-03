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

#ifndef INCLUDED_PMSURF_H
#define INCLUDED_PMSURF_H

#ifndef INCLUDED_LWRM_H
#include "core/include/lwrm.h"
#endif

#ifndef INCLUDED_POLICYMN_H
#include "policymn.h"
#endif

#include "mmu/gmmu_fmt.h"
#include "mdiag/utils/mmuutils.h"

class MdiagSurf;
class PmMemMappingsHelper;
class MmuLevelTree;
class MmuLevel;
class MmuLevelSegment;
struct MmuLevelSegmentInfo;
class MemAttrs;

//--------------------------------------------------------------------
//! \brief Class that holds all kinds of PDEs/PTEs information of a surface
//!
//! PmMmuLevel owns the APIs which PmSurface need to use
//! Common API for MmuLevel modification is stored at mdiag/utils/mmuutils.h
//!
class PmMmuLevel : public MmuLevel
{
public:
    PmMmuLevel(GpuDevice               *pGpuDevice,
               MdiagSurf               *pSurface,
               const struct GMMU_FMT   *pGmmuFmt,
               UINT32                   levelId,
               MmuLevelTree            *pMmuLevelTree);
    virtual ~PmMmuLevel() {};
    RC ModifyMmuEntries(MmuSubLevelIndex subLevelIdx, 
                        PmChannel *pInbandChannel,
                        PmMemMapping *pPmMemMapping, const EntryOps& entryOps);
    RC SetAperture(MmuSubLevelIndex subLevelIdx, PmChannel *pInbandChannel, 
        PmMemMapping *pPmMemMapping);

    RC SetValidPcf
    (
        MemAttrs* pMemAttrs,
        MmuLevelSegment* pSegment,
        UINT64 offset,
        UINT64* pSize,
        MmuSubLevelIndex subLevelIdx
    );

    RC SetIlwalidPcf
    (
        MemAttrs* pMemAttrs,
        MmuLevelSegment* pSegment,
        UINT64 offset,
        UINT64* pSize,
        MmuSubLevelIndex subLevelIdx
    );
};

//--------------------------------------------------------------------
//! \brief Class that holds all kinds of PDEs/PTEs information of a surface
//!
//! v1MmuLevel owns the APIs which only work for the 2 layer MMU
//! v1MmuLevel is a speicial case of the PmMmuLevel
//! Common API for MmuLevel modification is stored at mdiag/utils/mmuutils.h
//!
class V1MmuLevel : public PmMmuLevel
{
public:
    V1MmuLevel(GpuDevice               *pGpuDevice,
               MdiagSurf               *pSurface,
               const struct GMMU_FMT   *pGmmuFmt,
               UINT32                   levelId,
               MmuLevelTree            *pMmuLevelTree);
    virtual ~V1MmuLevel() {};
    RC FillPteMem(LwRm::Handle hMem, UINT64 MemOffset, UINT32 PteFlags,
                  UINT64 PhysAddr, LwRm::Handle hSrcVASpace, LwRm::Handle hTgtVASpace,
                  PmChannel *pInbandChannel, const PmMemRange* pPmMemRange);

};

//--------------------------------------------------------------------
//! \brief Thin wrapper around MdiagSurf for PolicyManager
//!
//! This class is basically just a MdiagSurf with extra fields used
//! by PolicyManager, such as:
//! - A set of PmSubsurfaces to deal with the surface as a set of
//!   named subsurfaces.  This is required for TraceBuffer, which has
//!   named subsurfaces.  Non-TraceBuffer surfaces have just one
//!   subsurface.
//! - A set of PmMemMapping objects, which show how the surface is
//!   mapped in the GPU's virtual-address space.
//! - A reference-counting mechanism to delete the PmSurface when it
//!   is no longer in use.  "External" surfaces, which are created by
//!   the test, are not freed until the test ends.  But "hidden"
//!   surfaces, used to allocate some physical memory, may be deleted
//!   whenever that memory is no longer in use.
//!
class PmSurface
{
public:
    PmSurface(PolicyManager *pPolicyManager, PmTest *pTest,
              MdiagSurf *pMdiagSurf, bool ExternalSurf2D);
    ~PmSurface();

    PolicyManager   *GetPolicyManager() const { return m_pPolicyManager; }
    PmTest          *GetTest()          const { return m_pTest; }
    MdiagSurf       *GetMdiagSurf()     const { return m_pMdiagSurf; }
    UINT64 GetSize() const;

    RC AddSubsurface(UINT64 Offset, UINT64 Size,
                     const string &Name, const string &TypeName);
    const PmSubsurfaces &GetSubsurfaces();

    RC CreateMemMappings();

    LwRm::Handle GetOrigMemHandle(UINT64 Offset) const;
    UINT64       GetOrigMemOffset(UINT64 Offset) const;
    UINT64       GetOrigMemSize(UINT64 Offset) const;

    void       SetDefaultChannel(PmChannel *pCh) { m_pDefaultChannel = pCh; }
    PmChannel *GetDefaultChannel();

    RC OnEvent();
    RC EndTest();

    void AddRef();
    void Release();
    void AddChildRef();
    void ReleaseChild();
    UINT32 GetRefs()      const { return m_Refs; }
    UINT32 GetChildRefs() const { return m_ChildRefs; }

    bool IsExternalSurf2D() const { return m_ExternalSurf2D; }
    bool IsSurfaceValidated() const { return m_bSurfaceValidated; }
    void SetSurfaceValidated(bool bValidated) { m_bSurfaceValidated = bValidated; }

    PolicyManager::CpuMappingMode GetCpuMappingMode() const { return m_CpuMappingMode; }
    void SetCpuMappingMode(PolicyManager::CpuMappingMode mode) { m_CpuMappingMode = mode; }

    bool HasGpuSmmuMappings(const PmMemRange* pMemRange) const;

    PmMemMappingsHelper *GetMemMappingsHelper() const { return m_pMemMappingsHelper; }
    PmMemMappingsHelper *GetHiddenTegraMappingsHelper() const { return m_pHiddenTegraMappingsHelper; }

    RC Fill(UINT32 Value, UINT32 SubdevNum, PmChannel *pInbandChannel);

    bool IsMmuLevelFmtSupported() const
    { return m_pMmuLevelTree? m_pMmuLevelTree->IsMmuLevelFmtSupported() :  false; }

    RC GetMmuLevel(MmuLevelTree::LevelIndex index, MmuLevel** ppMmuLevel);
    RC GetMmmuPteLevel(UINT64 pageSize, MmuLevel** ppMmuLevel);

    MmuLevelTree *GetMmuLevelTree() const { return m_pMmuLevelTree; }

    // Debugging function for PmAction.Print
    bool PrintMmuLevelSegmentInfo(string * pString, bool moreInfo);
    // Debugging function for gdb
    bool PrintMmuLevelSegmentInfo();
    LwRm* GetLwRmPtr();

private:
    PmSurface(const PmSurface &surf);            // Not implemented
    PmSurface &operator=(const PmSurface &surf); // Not implemented

    PolicyManager   *m_pPolicyManager;  //!< PolicyManager that owns this surf
    PmTest          *m_pTest;           //!< Test that owns this surf, or NULL
    MdiagSurf       *m_pMdiagSurf;      //!< The embedded MdiagSurf
    bool             m_ExternalSurf2D;  //!< If true, do not delete
                                        //!< m_pMdiagSurf in the destructor
    PmSubsurfaces    m_Subsurfaces;     //!< Keep track of the surface's
                                        //!< named subsurfaces.
    PmChannel       *m_pDefaultChannel; //!< Default channel
    UINT32           m_Refs;            //!< Reference counter
    UINT32           m_ChildRefs;       //!< Number of reference counts that
                                        //!< are from internal objects
    bool             m_InDestructor;    //!< True if we are in the destructor
    bool             m_bSurfaceValidated;//!< True if the surface has been
                                         //!< validated
    PolicyManager::CpuMappingMode m_CpuMappingMode; //!< Direct/Reflected/Default mapping

    //! Primary memmappings:
    //!   TOP layer MMU:
    //!       GPUVASpace surface - Gmmu
    //!       TegraVASpace surface - CheetAh Smmu
    //!       GPUAndTegraVASpace surface - Gmmu
    //!       DefaultVASpace surface - Gmmu
    //!   BOTTOM layer MMU: (only valid on Smmu enabled gpu or shared surface)
    //!       GPUVASpace and GpuSmmuOn surface - Gpu Smmu
    //!       GPUAndTegraVASpace and GpuSmmuOn surface - Gpu Smmu
    PmMemMappingsHelper *m_pMemMappingsHelper; //!< Helper object to manage TOP
                                               //!< MMU mappings of PmSurface
                                               //!< Not NULL in whole PmSurface
                                               //!< life cycle
    PmMemMappingsHelper *m_pOrigMemMappingsHelper; //!< Helper object to manage
                                                   //!< copy of original memory
                                                   //!< mappings. Do not modify
                                                   //!< after CreateMemMappings

    //! Secondary memmappings:
    //!    CheetAh memmappings of shared surface
    //!    Only valid for shared surface
    //!         GPUAndTegraVASpace - CheetAh Smmu
    PmMemMappingsHelper *m_pHiddenTegraMappingsHelper; //!< Helper object to
                                                       //!< manage cheetah part
                                                       //!< mappings of shared surface
    PmMemMappingsHelper *m_pOrigHiddenTegraMappingsHelper;//!< Helper object to manage
                                                          //!< copy of original cheetah
                                                          //!< memory mappings

    MmuLevelTree * m_pMmuLevelTree = nullptr; //!< MmuLevelTree object pointer

    RC ExploreMmuLevelTree();
};

//--------------------------------------------------------------------
//! \brief A range of memory.  Basically a (PmSurface, offset, size) tuple
//!
class PmMemRange
{
public:
    PmMemRange() : m_pSurface(NULL), m_Offset(0), m_Size(0) {}
    PmMemRange(PmSurface *pSurface, UINT64 Offset, UINT64 Size);
    PmMemRange(const PmMemRange &rhs);
    virtual ~PmMemRange();
    PmMemRange &operator=(const PmMemRange &rhs);
    bool operator<(const PmMemRange &rhs) const;
    void Set(PmSurface *pSurface, UINT64 Offset, UINT64 Size);

    PmSurface       *GetSurface()   const { return m_pSurface; }
    UINT64           GetOffset()    const { return m_Offset; }
    UINT64           GetSize()      const { return m_Size; }

    MdiagSurf       *GetMdiagSurf() const { return m_pSurface->GetMdiagSurf();}
    UINT64           GetGpuAddr()   const { return GetGpuAddr(NULL, NULL); }
    UINT64           GetGpuAddr(GpuSubdevice *pLocalSubdev,
                                GpuSubdevice *pRemoteSubdev)   const;
    RC MapPeer(GpuSubdevice *pLocalSubdev  = NULL,
               GpuSubdevice *pRemoteSubdev = NULL);

    bool Overlaps(const PmMemRange *rhs) const;
    PmMemRange GetIntersection(const PmMemRange *rhs) const;
    RC ModifyPteFlags(PolicyManager::PageSize PageSize,
                      UINT32 PteFlags, UINT32 PteMask,
                      PmChannel *pInbandChannel,
                      PolicyManager::AddressSpaceType TargetVaSpace,
                      PolicyManager::Level MmuLevelType,
                      bool DeferTlbIlwalidate) const;
    RC MovePhysMem(Memory::Location Location,
                   PolicyManager::MovePolicy MovePolicy,
                   PolicyManager::LoopBackPolicy Loopback,
                   bool DeferTlbIlwalidate,
                   PmChannel *pInbandChannel,
                   PolicyManager::AddressSpaceType TargetVaSpace,
                   Surface2D::VASpace SurfAllocType,
                   Surface2D::GpuSmmuMode SurfGpuSmmuMode,
                   const PmMemRange * pAllocatedSrcRange,
                   bool DisablePostL2Compression) const;
    RC Read(vector<UINT08> *pBytes, GpuSubdevice *pGpuSubdevice) const;
    RC Write(const vector<UINT08> *pBytes, GpuSubdevice *pGpuSubdevice) const;
    RC ReadPhys(vector<UINT08> *pBytes, GpuSubdevice *pGpuSubdevice) const;
    RC WritePhys(const vector<UINT08> *pBytes,
                 GpuSubdevice *pGpuSubdevice) const;
    RC GpuInbandPhysFill(UINT32 Value, UINT32 SubdevNum, PmChannel *pInbandChannel);

    static RC AliasPhysMem(
        const PmSubsurface *sourceSubsurface,
        const PmSubsurface *destSubsurface,
        UINT64 aliasSourceOffset,
        UINT64 aliasDestOffset,
        UINT64 size,
        bool deferTlbIlwalidate);

    static RC CreateSetPteInfoStructs
    (
        const PmMemRanges *pRanges,
        vector<LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS> *pParams,
        GpuDevices *pGpuDevices,
        vector<LwRm*> *pLwRms
    );

private:
    static void ReadData(vector<UINT08> *pBytes, void *pAddress,
                         UINT64 MemOffset, UINT64 MemSize);
    static void WriteData(const vector<UINT08> *pBytes, size_t *pByteOffset,
                          void *pAddress, UINT64 MemOffset, UINT64 MemSize);
    static RC CopyPhysMemInband(
        PmMemMapping *pSourceMapping,
        PmMemRange *pDestRange,
        GpuDevice *pGpuDevice,
        PmChannel *pInbandChannel,
        bool DisablePostL2Compression);

    static RC FillPhysMemInband(UINT32 Value, UINT32 SubdevNum, PmChannel *pCh,
                                UINT64 PhysAddr, UINT32 Aperture, UINT64 Size);

    MmuLevelTree::LevelIndex GetLevelIndex(UINT64 pagesize, PolicyManager::Level levelType) const;

    RC ApplyMovePolicy(PolicyManager::MovePolicy MovePolicy,
                       bool DeferTlbIlwalidate,
                       bool isInband) const;
    PmSurface *m_pSurface;  //!< Surface that owns this memory
    UINT64     m_Offset;    //!< Offset of this range in the surface
    UINT64     m_Size;      //!< Size of the range, in bytes
};

ostream& operator<< (ostream& os, const PmMemRange& pmMemRange);

//--------------------------------------------------------------------
//! \brief A named subsurface of a PmSurface
//!
//! A PmSubsurface is used to identify a named subsurface of a
//! PmSurface.  Every PmSurface divides itself into subsurfaces.
//! Aside from TraceBuffer surfaces, most surfaces have only one
//! subsurface, which covers the entire surface.
//!
//! A PmSubsurface object should only only be created by the PmSurface
//! that owns it.  PmSubsurfaces are normally obtained via
//! PmSurface::GetSubsurfaces().
//!
class PmSubsurface : public PmMemRange
{
public:
    PmSubsurface(PmSurface *pSurface, UINT64 Offset, UINT64 Size,
                 const string &Name, const string &TypeName);
    virtual ~PmSubsurface();
    const string &GetName()     const { return m_Name; }
    const string &GetTypeName() const { return m_TypeName; }

private:
    PmSubsurface();                             // Not implemented
    PmSubsurface(const PmSubsurface &);         // Not implemented
    void operator=(const PmSubsurface&);        // Not implemented
    string m_Name;      //!< Name of this subsurface
    string m_TypeName;  //!< Type of this subsurface
};

//--------------------------------------------------------------------
//! \brief Class that keeps track of how a PmSurface is mapped
//!
//! PmMemMapping objects keep track of how a PmSurface is mapped.
//! Each PmMemMapping keeps track of a contiguous part of the surface
//! that was allocated from the same memory handle, has contiguous
//! physical memory, has the same bits set in the PTEs, and so on.
//!
//! When a surface is first created, it usually only have one
//! PmMemMapping (except for split surfaces, which have two).
//! However, as PolicyManager modifies the PTEs by moving pages,
//! unmapping pages, etc, it splits and/or re-joins the
//! PmMemMapping(s) on page boundaries to keep track of the different
//! PTE properties in different parts of the surface.
//!
//! A PmMemMapping object should only only be created by the PmSurface
//! that owns it.  PmMemMappings are normally obtained via
//! PmSurface::GetMemMappings().
//!
class PmMemMapping : public PmMemRange
{
public:
    enum MappingType //!< Format: TargetMmu_SourceAddressType
    {
        Gmmu_Default = 0
        ,Gmmu_SysmemPa
        ,Ats_GvaSpa
    };

    PmMemMapping(PmSurface *pSurface, UINT64 Offset, UINT64 Size,
                 LwRm::Handle hMem, UINT64 MemOffset,
                 UINT64 PhysAddr, MappingType Type,
                 PmMemMappingsHelper *pOwnerMemMappingsHelper,
                 const MemAttrs* pMemAttrs);
    virtual ~PmMemMapping();

    const PmMemRange *GetPhysRange() const { return &m_PhysRange; }
    LwRm::Handle      GetMemHandle() const { return m_hMem; }
    UINT64            GetMemOffset() const { return m_MemOffset; }
    UINT32            GetPteFlags()  const;
    UINT64            GetPhysAddr()  const { return m_PhysAddr; }
    UINT64            GetVirtAddr()  const;
    UINT64            GetPageSize()  const;
    RC                GetAddress(GpuSubdevice *pSubdev, void **ppAddress);
    MappingType       GetMappingType() const { return m_MappingType; }

    RC ChangePageSize(UINT64 pageSize, PmChannel *pInbandChannel, bool deferTlbIlwalidate);

    PmMemMapping *Split(UINT64 VirtAddr);
    bool Join(PmMemMapping *pMemMapping);
    RC ModifyPtes(UINT32 PteFlags, UINT32 PteMask,
                  const PmMemRange *pSrcRange,
                  bool DeferTlbIlwalidate, PmChannel *pInbandChannel,
                  MmuLevelTree::LevelIndex levelNum);
    RC WritePtes(LwRm::Handle hMem, UINT64 MemOffset,
                 UINT32 PteFlags, UINT64 PhysAddr,
                 UINT64 PageSize, bool DeferTlbIlwalidate,
                 PmChannel *pInbandChannel) const;
    RC WriteMmuLevelEntries(UINT32 PdeFlags, UINT32 dstPdeMask,
                            bool bCopySrcPhysAddr, UINT64 PageSize,
                            bool deferTlbIlwalidate, PmChannel *pInbandChannel,
                            MmuLevelTree::LevelIndex LevelNum);

    void SetMemMappingsHelper(PmMemMappingsHelper *pMemMappingsHelper);
    PmMemMappingsHelper *GetMemMappingsHelper() const { return m_MemMappingsHelper; }

    void SetPhysRange(PmSurface *pSurface, UINT64 Offset, UINT64 Size);

    MemAttrs *GetMemAttrs() const { return m_MemAttrs.get(); }

    RC SetPhysAddrBits(UINT64 addrValue, UINT64 addrMask,
        bool deferTlbIlwalidate, PmChannel *inbandChannel);

private:
    PmMemMapping();                             // Not implemented
    PmMemMapping(const PmMemMapping &rhs);      // Not implemented
    void operator=(const PmMemMapping&);        // Not implemented
    void UnmapAll();

    // Smmu only support out-of-band manipulation
    RC SmmuOutOfBandWritePtes(LwRm::Handle hMem, UINT64 MemOffset,
                              UINT32 PteFlags, UINT64 PhysAddr,
                              UINT64 PageSize, PmChannel *pInbandChannel) const;
    LwRm::Handle GetFillPteSrcHandle() const;
    LwRm::Handle GetFillPteTgtHandle() const;

    bool bModifyPteEntries(MmuLevelTree::LevelIndex LevelNum) const;

    PmMemRange   m_PhysRange;
    LwRm::Handle m_hMem;          //!< Memory this range was alloc'ed from
    UINT64       m_MemOffset;     //!< Offset into m_hMem of this range
    UINT64       m_PhysAddr;      //!< Physical address of this range

    typedef struct
    {
        void * CpuAddress;
        UINT32 CpuMappingFlags;
    } CpuMappingInfo;
    map<GpuSubdevice *, CpuMappingInfo> m_SubdevAddrs; //!< Per-subdevice CPU addresses

    MappingType  m_MappingType;   //!< Mapping type

    PmMemMappingsHelper *m_MemMappingsHelper;//!< Helper object to manage
                                             //!< bottom layer memmappings in
                                             //!< gpu-asid smmu for smmu
                                             //!< enabled gpu surface

    PmMemMappingsHelper *m_OwnerMemMappingsHelper; //!< Helper object which holds
                                                   //!< the current PmMemMapping;
                                                   //!< This object is used to get
                                                   //!< info of other PmMemMapping
                                                   //!< in same group(PmMemMappingsHelper)

    unique_ptr<MemAttrs> m_MemAttrs;
};

//--------------------------------------------------------------------
//! \brief A range of ATS memory.
//!
class PmAtsRange
{
public:
    PmAtsRange(PmSurface *surface, UINT64 offset, UINT64 size);
    virtual ~PmAtsRange();

    PmSurface *GetSurface() const { return m_Surface; }
    UINT64     GetOffset()  const { return m_Offset;  }
    UINT64     GetSize()    const { return m_Size;    }

    MdiagSurf *GetMdiagSurf() const { return m_Surface->GetMdiagSurf(); }

private:
    PmSurface *m_Surface; //!< Surface that owns this ATS memory
    UINT64     m_Offset;  //!< Offset of this range in the surface
    UINT64     m_Size;    //!< Size of the range, in bytes
};

//--------------------------------------------------------------------
//! \brief Class to encapsulate surface attributes in user view
//!
//! User view attributes is slightly different from attributes saved in local mmu data.
//! For example, enable gpu cache on a unmapped surface is an invalid op. Gpu cache bit
//! will not be set into local mmu data, but Gpu cache attr will be set to reflect user's
//! intention. And then Gpu cache will be enabled once the surface once it's re-mapped later.
//!
class MemAttrs
{
public:
    MemAttrs(PmSurface* pPmSurface, UINT64 offset, UINT64 pageSize);

    UINT64 GetOffset() const { return m_Offset; }
    void SetOffset(UINT64 offset) { m_Offset = offset; }

    RC IsPageAttrValid(MmuLevelTree::LevelIndex levelNum, bool *pValid) const;
    RC IsPageAttrCache(MmuLevelTree::LevelIndex levelNum, bool *pCachable) const;
    RC IsPageAttrSparse(MmuLevelTree::LevelIndex levelNum, bool *pSparse) const;
    RC IsPageAttrRO(MmuLevelTree::LevelIndex levelNum, bool *pReadOnly) const;
    RC IsPageAttrAtomicDisable(MmuLevelTree::LevelIndex levelNum, bool *pAtomicDisable) const;
    RC IsPageAttrPriv(MmuLevelTree::LevelIndex levelNum, bool *pPriv) const;

    RC SetPageAttrValid(MmuLevelTree::LevelIndex levelNum, bool valid);
    RC SetPageAttrCache(MmuLevelTree::LevelIndex levelNum, bool cachable);
    RC SetPageAttrSparse(MmuLevelTree::LevelIndex levelNum, bool sparse);
    RC SetPageAttrRO(MmuLevelTree::LevelIndex levelNum, bool readOnly);
    RC SetPageAttrAtomicDisable(MmuLevelTree::LevelIndex levelNum, bool atomicDisable);
    RC SetPageAttrPriv(MmuLevelTree::LevelIndex levelNum, bool priv);

    bool IsSameMemAttrs(const MemAttrs *pMemAttrs) const;

    UINT64 GetActivePageSize() const { return m_PageSize; }
    void SetActivePageSize(UINT64 pageSize) { m_PageSize = pageSize; }

    virtual MmuLevelTree::LevelIndex GetActivePteLevel() const { return MmuLevelTree::GMMU_LEVEL_PTE_4K; }

    // To do: remove the function after the code depending on PteFlags is removed
    virtual UINT32 GetPteFlags() const = 0;

    virtual MemAttrs *Clone(UINT64 Offset) const = 0;

    // Might be removed after RM API fillPteMem is totally droped for GPU
    virtual RC UpdateMemAttrs(UINT32 fillPteFlags) = 0;

    virtual RC InitMemAttrsLevel(MmuLevelTree::LevelIndex levelNum) = 0;

protected:
    enum MemATTR
    {
        MEMATTR_ILWALID = 0,
        MEMATTR_TRUE,
        MEMATTR_FALSE
    };

    struct PageAttrs
    {
        MemATTR m_AttrVal           = MEMATTR_ILWALID;
        MemATTR m_AttrCache         = MEMATTR_ILWALID;
        MemATTR m_AttrSparse        = MEMATTR_ILWALID;
        MemATTR m_AttrRo            = MEMATTR_ILWALID;
        MemATTR m_AttrAtomicDisable = MEMATTR_ILWALID;
        MemATTR m_AttrPriv          = MEMATTR_ILWALID;

        PageAttrs() = default;
    };

    const PageAttrs* GetPageAttrs(MmuLevelTree::LevelIndex levelNum) const;
    PageAttrs* GetPageAttrs(MmuLevelTree::LevelIndex levelNum);

    map<UINT32, PageAttrs> m_MmuLevelsPageAttrs;

    PmSurface *m_pPmSurface;
    UINT64     m_Offset;
    UINT64     m_PageSize;

private:

};

//--------------------------------------------------------------------
//! \brief Class for GMMU attributes
class GmmuMemAttrs : public MemAttrs
{
public:
    GmmuMemAttrs(PmSurface* pPmSurface, UINT64 offset, UINT64 pageSize);

    virtual MmuLevelTree::LevelIndex GetActivePteLevel() const;

    virtual UINT32 GetPteFlags() const;

    virtual GmmuMemAttrs *Clone(UINT64 Offset) const;

    virtual RC UpdateMemAttrs(UINT32 fillPteFlags);

    virtual RC InitMemAttrsLevel(MmuLevelTree::LevelIndex levelNum);

private:

    UINT32 UpdatePteFlagsFromMemAttrs() const;
};

//--------------------------------------------------------------------
//! \brief Class for SMMU attributes
class SmmuMemAttrs : public MemAttrs
{
public:
    SmmuMemAttrs(PmSurface* pPmSurface, UINT64 offset, UINT64 pageSize);

    virtual SmmuMemAttrs *Clone(UINT64 Offset) const;

    virtual UINT32 GetPteFlags() const { return m_PteFlags; }

    virtual RC UpdateMemAttrs(UINT32 fillPteFlags);

    virtual RC InitMemAttrsLevel(MmuLevelTree::LevelIndex levelNum);

private:
    UINT32 m_PteFlags; //!< Orignal PTE flags in PmMemMapping
                       //!< Only for current active PTE
                       //!< MemAttrs can replace PTE flags
};

//--------------------------------------------------------------------
//! \brief Class for Ats attributes
class AtsMemAttrs : public MemAttrs
{
public:
    AtsMemAttrs(PmSurface* pPmSurface, UINT64 offset, UINT64 pageSize);

    virtual MmuLevelTree::LevelIndex GetActivePteLevel() const;

    virtual UINT32 GetPteFlags() const;

    virtual AtsMemAttrs *Clone(UINT64 Offset) const;

    virtual RC UpdateMemAttrs(UINT32 fillPteFlags);

    virtual RC InitMemAttrsLevel(MmuLevelTree::LevelIndex levelNum);

private:

    UINT32 UpdatePteFlagsFromMemAttrs() const;
};

//--------------------------------------------------------------------
//! \brief Helper class to manipulate a group of PmMemMappings
//!
//! A shared surface(GPUAndTegra) with gpu-asid smmu enabled
//! may have 3 groups of PmMemMapping orgnized in the following way:
//!                                                                                     |-PmMemMapping
//!                                       |- PmMemMapping-->GpuSmmuHelper::m_MemMappings|-...
//!                                       |                                             |-PmMemMapping
//! PmSurface -> GmmuHelper::m_MemMappings|- ...
//!    |                                  |                                             |-PmMemMapping
//!    |                                   - PmMemMapping-->GpuSmmuHelper::m_MemMappings|-...
//!    |                                                                                |-PmMemMapping
//!    |
//!    |                                             |-- PmMemMapping
//!    --------> HiddenTegraSmmuHelper::m_MemMappings|-- ...
//!                                                  |-- PmMemMapping
//!
//!
//! Other kinds of surface are subset of above case. For exampe, CheetAh surface:
//!
//!                                       |- PmMemMapping-->(NULL)
//!                                       |
//! PmSurface -> SmmuHelper::m_MemMappings|- ...
//!    |                                  |
//!    |                                   - PmMemMapping-->(NULL
//!    |-->HiddenTegraSmmuHelper(NULL)
//!
class PmMemMappingsHelper
{
public:
    PmMemMappingsHelper(PmSurface *pSurface,
                        PmMemMapping *pParentMapping,
                        UINT32 vaSpaceClass);
    PmMemMappingsHelper(PmSurface *pSurface,
                        PmMemMapping *pParentMapping,
                        UINT32 vaSpaceClass,
                        UINT64 virtAddr);
    ~PmMemMappingsHelper();

    RC CreateMemMappings();
    RC CreateAtsMappings();

    RC CloneMemMappings(const PmMemMappingsHelper *pMemMappingsHelper,
                        const PmMemRange *pMemRange);

    PmMemMappings GetMemMappings(const PmMemRange *pMemRange) const;
    PmMemMappings SplitMemMappings(const PmMemRange *pMemRange);
    void JoinMemMappings(const PmMemRange *pMemRange);

    PmMemMapping* SplitMapping(UINT64 splitOffset);

    enum WritePtesMode
    {
        WritePtes_FALSE = 0,
        WritePtes_TRUE
    };
    RC UpdateMemMappings(UINT32 PteFlags,
                         UINT32 PteMask,
                         UINT32 DstMask,
                         UINT64 SrcRangeOffset,
                         PmMemMappings *pSrcMemMappings,
                         UINT64 Offset,
                         UINT64 Size,
                         WritePtesMode WritePtes,
                         bool DeferTlbIlwalidate,
                         PmChannel *pInbandChannel);

    bool IsMemMappingsChanged(
        const PmMemMappingsHelper* pSrcMemMappingsHelper) const;

    bool IsMemMappingsValid(const PmMemRange *pMemRange) const;

    void AddRef()          { ++ m_Refs; }
    void Release()         { -- m_Refs; }
    UINT32 GetRefs() const { return m_Refs; }

    UINT64 GetSize() const;

    void SetBaseVirtAddr(UINT64 virtAddr) { m_VirtAddr = virtAddr; }
    UINT64 GetVirtAddr(UINT64 offset)  const { return m_VirtAddr + offset - GetBaseOffset(); }
    UINT64 GetBaseOffset() const { return m_MemMappings.size() > 0?m_MemMappings[0]->GetOffset():0; }

    PmSurface *GetSurface() const { return m_pSurface; }
    UINT32 GetVASpaceClass() const { return m_VASpaceClass; }

    PmMemMappingsHelper *Split(UINT64 offset, UINT64 size,
                               PmMemMapping *pNewParentMapping);
    void Join(PmMemMappingsHelper *pSrcMemMappingHelper);

    // For debugging
    void PrintMemMappingsInfo() const;

private:

    RC ExploreMemMappings(LwRm::Handle hTargetVASpace,
                          UINT64 TargetVirtAddrBase,
                          UINT64 RangeSize,
                          PmMemMapping::MappingType mappingType);
    RC ExploreSmmuMemMappings(LwRm::Handle hTargetVASpace,
                              UINT64 TargetVirtAddrBase,
                              UINT64 RangeSize,
                              PmMemMapping::MappingType mappingType);
    RC ExploreGmmuMemMappings(UINT64 RangeSize,
                              PmMemMapping::MappingType mappingType);

    UINT32 FindMemMapping(UINT64 Offset) const;

    RC GetSysmemPhysAddr(LwRm::Handle hMem, UINT64 Offset,
                         UINT32 TargetVASpaceClass,
                         GpuSubdevice *pGpuSubdevice, UINT64 *pPhysAddr) const;

    PmSurface *m_pSurface;  //!< Surface that owns this memory
    PmMemMapping *m_pParentMapping; //!< Up layer PmMemMapping object;
                                    //!< only valid for Gmmu_SmmuVa type
    UINT32 m_Refs;          //!< Reference count for release
    UINT32 m_VASpaceClass;  //!< VASpace type this PmMemMappings belongs to;
                            //!< It can be FERMI_VASPACE_A or TEGRA_VASPACE_A
                            //!< or default(0)
    PmMemMappings m_MemMappings; //!< An array of PmMemMapping to track
                                 //!< memory mapping information
    UINT64        m_VirtAddr;    //!< Gpu Base virtual address of this group of
                                 //!< memmappings; memmappings in same group
                                 //!< have same base va but different offset
};
#endif // INCLUDED_PMSURF_H
