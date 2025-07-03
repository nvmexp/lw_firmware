/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/


#ifndef INCLUDED_MMUUTILS_H
#define INCLUDED_MMUUTILS_H

class GpuDevice;
class MdiagSurf;
class MmuLevel;
class MmuLevelSegment;
struct MmuLevelSegmentInfo;

#include "core/include/lwrm.h"
#include "mmu/gmmu_fmt.h"

//--------------------------------------------------------------------
//! \brief Class that holds all kinds of PDEs/PTEs information of a surface
//!
//! In case vmm is enabled in RM(default), a surface in non-lazy mode
//! may have 4~5 mmu levels organized like a tree. Each level may have
//! multiple continuous tables(segments).
//!
//! MmuLevelTreeManager does not allocate/free PDEs/PTEs tables. It depends on RM
//! to allocate/free them during surface allocation. In RM non-lazy mode,
//! all PDEs/PTEs are allocated once a surface is allocated. MmuUtils
//! just modify PDEs/PTEs to point to existed page(directory) tables.
//!
//! One surface only has one MmuLevelTree. Mmu information is explored
//! via ExploreMmuLevels() after the surface is allocated.
//!
class MmuLevelTree
{
public:
    MmuLevelTree(GpuDevice *pGpuDevice, MdiagSurf *pSurface,
                 UINT64 virtAddress, LwRm::Handle hVASpace,
                 bool isUtl = false);
    // Access a level in MmuLevelTree via index
    // For 2M surface: PTE_4K and PTE_64K - Invalid
    //    Invalid means:
    //      case 1: They might be allocated but not connected to PDE0;
    //      case 2: They should be empty(no mmu info) if they are not allocated.
    // For 4K surface: PTE_64K - Invalid
    // For 64K surface: PTE_4k - Invalid
    enum LevelIndex
    {
        GMMU_LEVEL_PTE_4K   = 0,
        GMMU_LEVEL_PTE_64K  = 1,
        GMMU_LEVEL_PTE_2M   = 2,
        GMMU_LEVEL_PDE0 = GMMU_LEVEL_PTE_2M,
        GMMU_LEVEL_PTE_512M = 3,
        GMMU_LEVEL_PDE1     = GMMU_LEVEL_PTE_512M,
        GMMU_LEVEL_PDE2     = 4,
        GMMU_LEVEL_PDE3     = 5,
        GMMU_LEVEL_PDE4     = 6,
        GMMU_LEVEL_PDE_LAST = 7
    };

    RC GetMmuLevel(LevelIndex level, MmuLevel** ppMmuLevel);

    RC ExploreMmuLevels();

    UINT64 GetBaseVirtAddr() const { return m_BaseVirtAddr; }

    bool IsMmuLevelFmtSupported() const { return m_pGmmuFmt != NULL; }

    RC GetSurfaceActivePageSize(UINT64 offset, UINT64 *pageSize);
    RC GetSurfacePhysAddr(UINT64 offset, UINT64 pageSize,
                          UINT64 *pPhysAddr, GMMU_APERTURE *pAperture);

    UINT64 GetActivePageSize() { return m_ActivePageSize; }
    void SetActivePageSize(UINT64 pageSize) { m_ActivePageSize = pageSize; }
    void ChangePageSize() { m_IsChangedPageSize = true; }
    bool IsChangedPageSize() { return m_IsChangedPageSize; }
private:
    RC ExploreMmuLevelsGetPageLevelInfo();
    RC ExploreMmuLevelsGetPdeInfo();

    vector<unique_ptr<MmuLevel> > m_MmuLevels; //!< a list of levels in the tree
                                  //!< a tree may have 4~5 levels

    GpuDevice   *m_pGpuDevice;
    UINT64       m_BaseVirtAddr;
    LwRm::Handle m_hVASpace;
    MdiagSurf   *m_pSurface;
    const struct GMMU_FMT *m_pGmmuFmt;
    const bool   m_IsUtl;
    UINT64 m_ActivePageSize = ~0x0ull;
    bool m_IsChangedPageSize = false;
};


//--------------------------------------------------------------------
//! \brief Class that tracks and modifies info of a level in mmu tree
//!
//!        A MmuLevel may have multiple mmu segments;
//!        Each mmu segment is a PDEs/PTEs table;
//!        The class breaks PTEs/PDEs modification request into MmuLevelSegment request
//!        The class provides a set of interface to modify PTEs/PDEs
//!
class MmuLevel
{
public:
    MmuLevel(GpuDevice               *pGpuDevice,
             MdiagSurf               *pSurface,
             const struct GMMU_FMT   *pGmmuFmt,
             UINT32                   levelId,
             MmuLevelTree            *pMmuLevelTree);
    virtual ~MmuLevel() {};

    /* The current supported formats have a maximum of 2 parallel sub-levels,
     * often referred to as "dual PDE" or "dual page table" support.
     *
     * Example for Fermi GPU HW:
     *      Sub-level 0 corresponds to big page table pointer.
     *      Sub-level 1 corresponds to small page table pointer.*/
    enum MmuSubLevelIndex
    {
        GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT = 0,
        GMMU_SUB_LEVEL_PAGE_TABLE_BIG = 0,
        GMMU_SUB_LEVEL_PAGE_TABLE_SMALL
    };

    RC GetLowerMmuLevel(MmuLevel::MmuSubLevelIndex subLevelIdx, MmuLevel **ppMmuLevel) const;

    struct EntryOps
    {
        enum MmuEntryOpTypes
        {
            MMUENTRY_OP_SET_VALID           = 1 << 0,
            MMUENTRY_OP_CLEAR_VALID         = 1 << 1,

            MMUENTRY_OP_SET_CACHE           = 1 << 2,
            MMUENTRY_OP_CLEAR_CACHE         = 1 << 3,

            MMUENTRY_OP_SET_SPARSE          = 1 << 4,
            MMUENTRY_OP_CLEAR_SPARSE        = 1 << 5,

            MMUENTRY_OP_SET_ATOMICDISABLE   = 1 << 6,
            MMUENTRY_OP_CLEAR_ATOMICDISABLE = 1 << 7,

            MMUENTRY_OP_SET_ACCESSRO        = 1 << 8,
            MMUENTRY_OP_CLEAR_ACCESSRO      = 1 << 9,

            MMUENTRY_OP_CHANGETYPE_TOPTE    = 1 << 10,
            MMUENTRY_OP_CHANGETYPE_TOPDE    = 1 << 11,

            MMUENTRY_OP_CONNECTLOWER        = 1 << 12,

            MMUENTRY_OP_SET_CONTI_PA        = 1 << 13,

            MMUENTRY_OP_SET_PRIV            = 1 << 14,
            MMUENTRY_OP_CLEAR_PRIV          = 1 << 15,
        };

        UINT32 MmuEntryOps;
        UINT64 EntryPhysAddr;        // only valid for MMUENTRY_OP_SET_PA
        GMMU_APERTURE EntryAperture; // only valid for MMUENTRY_OP_SET_PA
    };

    RC GetMmuLevelSegmentInfo(UINT64 offset, MmuLevelSegmentInfo *pInfo) const;
    RC GetMmuLevelSegment(UINT64 offset, MmuLevelSegment **ppSegment);

    RC AddMmuLevelSegment(const MmuLevelSegmentInfo& info);

    GpuDevice *GetGpuDevice() const { return m_pGpuDevice; }
    const struct GMMU_FMT *GetGmmuFmt() const { return m_pGmmuFmt; }
    UINT32 GetLevelId() const { return m_LevelId; }
    MdiagSurf *GetSurface() const { return m_pSurface; }
    MmuLevelTree *GetMmuLevelTree() const { return m_pMmuLevelTree; }

protected:
    GpuDevice               *m_pGpuDevice;
    MdiagSurf               *m_pSurface;
    UINT32                   m_LevelId;

private:
    RC GetSegmentId(UINT64 offset, UINT32& segmentId) const;

    const struct GMMU_FMT   *m_pGmmuFmt;
    MmuLevelTree            *m_pMmuLevelTree;
    vector<unique_ptr<MmuLevelSegment> > m_MmuLevelSegments; //<! a list of segments of the level
                                                 //<! each segment is a entries table
};

//
// Infomation holds in a mmu segment
//
// Notes: page table vs. segment
//        page table has all entries of a va range;
//        segment just has those entries covered by the surface;
//        If a surface covers whole table, tableBasePhysAddress == physAddress
struct MmuLevelSegmentInfo
{
    struct MMU_FMT_LEVEL       *pFmtLevel;
    vector<char>                fmtLevelVec;
    vector<char>                fmtSubLevelVecs;
    UINT64                      levelPageSize;        // page size of the level
    UINT64                      tableBasePhysAddress; // base PA of page table
    UINT64                      physAddress;          // base PA of the segment
    Memory::Location            location;
    UINT32                      entrySize;
    UINT08                     *pCpuAddress;
    LwRm::Handle                mappedHandle;

    UINT64                      surfOffset;
    UINT64                      surfSegmentSize;
};

//--------------------------------------------------------------------
//! \brief Class that tracks and modifies PDEs/PTEs of a segment in a mmu level
//!
//!        A MmuLevelSegment is a PDEs/PTEs table;
//!        The class provides a set of interface to modify PTEs/PDEs
//!        on specified surface offset
//!
class MmuLevelSegment
{
public:
    enum class PtePcfField
    {
        Invalid,
        Lw4K,
        Sparse,
        NoMapping,
        ACE,
        Volatile, // Uncached when valid = 1
        AtomicDisable,
        ReadOnly,
        Privilege,
    };

    enum class PdePcfField
    {
        Invalid,
        Sparse,
        Volatile, 
        AtsAllow,
    };

    enum class FetchMode
    {
        FetchCache,
        Reset
    };

    MmuLevelSegment(MmuLevel *pMmuLevel, const MmuLevelSegmentInfo &info);
    ~MmuLevelSegment();

    RC GetMmuLevelSegmentInfo(MmuLevelSegmentInfo *pInfo) const;

    RC ConnectToLowerLevel(UINT64 offset, UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx);

    RC SetAperture(UINT64 offset, UINT64 *pSize, 
        MmuLevel::MmuSubLevelIndex subLevelIdx, Memory::Location location);

    RC SetSegmentValid(UINT64 offset, UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx, bool valid);

    RC SetSegmentVolatile(UINT64 offset, UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx, bool vol);
    
    RC SetSegmentSparse(UINT64 offset, UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx, bool sparse);

    RC SetSegmentAtomicDisable(UINT64 offset, UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx, bool atomicDisable);

    RC SetSegmentPriv(UINT64 offset, UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx, bool PrivEnable);

    RC SetSegmentAccessRO(UINT64 offset, UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx, bool accessRO);

    RC SetEntryPcf
    (
        UINT08* lwrrentData,
        MmuLevel::MmuSubLevelIndex subLevelIdx,
        const map<PtePcfField, bool>& pteFields,
        const map<PdePcfField, bool>& pdeFields,
        FetchMode fetchMode
    );

    RC SetSegmentPcf
    (
        UINT64 offset,
        UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx,
        const map<PtePcfField, bool>& pteFields,
        const map<PdePcfField, bool>& pdeFields,
        FetchMode fetchMode
    );

    RC SetContiguousPA(UINT64 offset, UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx,
        UINT64 physAddr, GMMU_APERTURE aperture);

    RC ChangePageEntryType(UINT64 offset, UINT64 *pSize,
        MmuLevel::MmuSubLevelIndex subLevelIdx, bool bToPte);

    RC FillPteMem(UINT64 offset, UINT64 *pSize, LwRm::Handle hMem,
                  UINT64 MemOffset, UINT32 PteFlags, UINT64 PhysAddr,
                  LwRm::Handle hSrcVASpace, LwRm::Handle hTgtVASpace);

    RC GetMmuEntriesData(UINT64 offset, UINT64 *pSize, vector<UINT08> *pData);

    RC UpdateLocalEntriesData(UINT64 offset, UINT64 *pSize, vector<UINT08> *pData);

    RC WriteMmuEntries(UINT64 offset, UINT64 *pSize, 
            Channel *pInbandChannel, UINT32 subchNum);

    bool IsEntryValid(UINT64 offset, MmuLevel::MmuSubLevelIndex subLevelIdx, bool *isPte);
    bool IsPteValid(const UINT08 * pEntry);


    RC GetEntryPhysAddr(UINT64 offset, MmuLevel::MmuSubLevelIndex subLevelIdx,
                        UINT64 *pPhysAddr, GMMU_APERTURE *pAperture);

    bool IsPteEntry(const UINT08 *pEntry);
    MmuLevel * GetMmuLevel() const { return m_pMmuLevel; }
    UINT08* GetEntryLocalData(UINT64 offset, UINT64 size,
                              UINT64 *pRealSize, UINT64 *pPageCnt);
    RC UpdateAllPtePcf(UINT08* pEntryData,
                       const map<PtePcfField, bool>& pteFields,
                       FetchMode fetchMode);
    RC UpdatePtePcf(UINT08 * pEntryData, 
                    PtePcfField ptePcfField, bool ptePcfValue);
    RC GetBoolFromPtePcf(const UINT08 * pEntryData,
                    PtePcfField ptePcfField, bool * pPtePcfValue);
    
    RC UpdateAllPdePcf(UINT08 * pEntryData,
                       const map<PdePcfField, bool>& pdeFields,
                       MmuLevel::MmuSubLevelIndex subLevelIdx,
                       FetchMode fetchMode);
    RC UpdatePdePcf(UINT08 * pEntryData, 
                    PdePcfField pdePcfField, 
                    MmuLevel::MmuSubLevelIndex subLevelIdx,
                    bool pdePcfValue);
    RC GetBoolFromPdePcf(const UINT08 * pEntryData,
                    PdePcfField pdePcfField, 
                    MmuLevel::MmuSubLevelIndex subLevelIdx,
                    bool * pPdePcfValue);

    // Unify the bit in PCF which is different handle in v2 and v3 
    bool GetFieldBool(PtePcfField ptePcfField, const UINT08 * pEntry);
    bool GetFieldBool(PdePcfField pdePcfField, const UINT08 * pEntry,
                    MmuLevel::MmuSubLevelIndex subLevelIdx);
    bool IsFieldValid(PtePcfField ptePcfField);
private:
    RC MapCpuPointer();
    RC UnmapCpuPointer();

    RC WriteMmuEntriesInband
    (
        Channel *pInbandChannel,
        UINT32 subchNum,
        UINT64 ptePhysAddr,
        Memory::Location location,
        const vector<UINT08>& pteBytes
    ) const;

    RC SetSwPtePcfField
    (
        UINT32* swPtePcf,
        PtePcfField ptePcfField,
        bool ptePcfValue
    );

    RC SetSwPdePcfField
    (
        UINT32* swPdePcf,
        PdePcfField pdePcfField,
        bool pdePcfValue
    );

    GMMU_APERTURE         m_UnmappedPdeAperture;
    MmuLevel             *m_pMmuLevel;
    MmuLevelSegmentInfo   m_SegmentInfo; //<! structure to save segment info
    vector<UINT08>        m_MmuEntriesData; //<! saved PTEs/PDEs data image
                                            //<! PTEs/PDEs modification happens on the
                                            //<! saved image before downloading to hw
};

//--------------------------------------------------------------------
//! \brief Class that manager all mmu level tree 
//!
//!        A MmuLevel can be created by the Policy Manager or UTL;
//!        A MmuLevel tree can be servered by multi-Gpu(API reserved while no verified)
//!
class MmuLevelTreeManager
{
public:
    static MmuLevelTreeManager *Instance();
    static bool HasInstance();
    static void ShutDown();
    void FreeResource(MdiagSurf* pSurf);
    MmuLevelTree * GetMmuLevelTree(GpuDevice* pGpuDevice,
                                    MdiagSurf* pSurf,
                                    UINT64 virtAddress,
                                    LwRm::Handle hVASpace,
                                    bool isUtl = false);
    RC GetMmuLevel(MmuLevelTree::LevelIndex index,
                    GpuDevice* pGpuDevice, 
                    MdiagSurf* pSurf,
                    UINT64 virtAddress, 
                    LwRm::Handle hVASpace,
                    MmuLevel** pMmuLevel);
    RC GetMmuPteLevel(UINT64 pagesize, 
                      GpuDevice* pGpuDevice, 
                      MdiagSurf* pSurf,
                      UINT64 virtAddress, 
                      LwRm::Handle hVASpace, 
                      MmuLevel** pMmuLevel);
    RC SetPteInfoStruct(LwRm * pLwRm, UINT64 gpuAddr, GpuDevice * pGpuDevice,
                        LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS *pParams);
    typedef std::tuple<GpuDevice*,MdiagSurf*, UINT64, LwRm::Handle, bool> MmuLevelTreeKey;
    typedef map<MmuLevelTreeKey, unique_ptr<MmuLevelTree> > MmuLevelTrees;
private:
    MmuLevelTreeManager() {};
    static unique_ptr<MmuLevelTreeManager>  m_Instance;
    MmuLevelTrees m_MmuLevelTrees;
};

namespace MmuUtils 
{
    RC GmmuTranslatePtePcfFromHw
    (
        UINT32 ptePcfHw,
        bool bPteValid,
        UINT32* pPtePcfSw
    );

    RC GmmuTranslatePtePcfFromSw
    (
        UINT32 ptePcfSw, 
        UINT32* pPtePcfHw
    );

    RC GmmuTranslatePdePcfFromSw
    (
        UINT32  pdePcfSw,
        UINT32* pPdePcfHw
    );

    RC GmmuTranslatePdePcfFromHw
    (
        UINT32 pdePcfHw,
        GMMU_APERTURE aperture,
        UINT32* pPdePcfSw
    );

    UINT32 TrimSwPtePcf(UINT32 oldSwPtePcf, bool bPteValid);
}

#endif // INCLUDED_MMUUTILS_H

