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

#ifndef INCLUDED_UTLMMU_H
#define INCLUDED_UTLMMU_H

#include "utlpython.h"
#include "mdiag/utils/mmuutils.h"
#include "g00x/g000/dev_mmu.h"

class UtlMmuEntry;
class UtlMmuSegment;
class UtlChannel;
class UtlSurface;

namespace UtlMmu
{
    void Register(py::module module);

    enum class SUB_LEVEL_INDEX 
    {
        BIG = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_BIG,
        SMALL = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_SMALL,
        DEFAULT = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT,
    };

    enum class APERTURE
    {
        APERTURE_FB = GMMU_APERTURE_VIDEO,
        APERTURE_COHERENT = GMMU_APERTURE_SYS_COH,
        APERTURE_NONCOHERENT = GMMU_APERTURE_SYS_NONCOH,
        APERTURE_PEER = GMMU_APERTURE_PEER,
        APERTURE_ILWALID = GMMU_APERTURE_ILWALID
    };

    enum LEVEL
    {
        LEVEL_PTE_4K,
        LEVEL_PTE_64K,
        LEVEL_PTE_2M,
        LEVEL_PDE0 = LEVEL_PTE_2M,
        LEVEL_PTE_512M,
        LEVEL_PDE1 = LEVEL_PTE_512M,
        LEVEL_PDE2,
        LEVEL_PDE3,
        LEVEL_PDE4
    };

    enum class FIELD
    {
        ReadOnly,
        AtomicDisable,
        Volatile,
        Privilege,
        Valid,
        Acd,
        Sparse,
        AtsAllow,
    };

    enum class PCF_SW
    {
        // PTE_PCF
        PTE_PCF_ILWALID                             = SW_MMU_PTE_PCF_ILWALID,                                                         
        PTE_PCF_SPARSE                              = SW_MMU_PTE_PCF_SPARSE,                                                          
        PTE_PCF_MAPPING_NOWHERE                     = SW_MMU_PTE_PCF_MAPPING_NOWHERE,    
        PTE_PCF_NO_VALID_4KB_PAGE                   = SW_MMU_PTE_PCF_NO_VALID_4KB_PAGE,    
        PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACE        = SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACE,    
        PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACE      = SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACE,    
        PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACE      = SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACE,    
        PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACE     = SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACE,    
        PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACE   = SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACE,    
        PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACE     = SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACE,    
        PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACE   = SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACE,    
        PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACE        = SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACE,    
        PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACD        = SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACD,    
        PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACD      = SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACD,    
        PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACD      = SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACD,    
        PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACD     = SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACD,    
        PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACD   = SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACD,    
        PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACD     = SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACD,    
        PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACD   = SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACD,
        PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACD    = SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACD,
        PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACE    = SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACE,
        PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_UNCACHED_ACE = SW_MMU_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_UNCACHED_ACE,
        PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_CACHED_ACE   = SW_MMU_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_CACHED_ACE,
        PTE_PCF_PRIVILEGE_RO_ATOMIC_UNCACHED_ACE    = SW_MMU_PTE_PCF_PRIVILEGE_RO_ATOMIC_UNCACHED_ACE,
        PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_UNCACHED_ACE = SW_MMU_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_UNCACHED_ACE,
        PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_CACHED_ACE   = SW_MMU_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_CACHED_ACE,
        PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACD        = SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACD,
        PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACD      = SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACD,
        PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACE      = SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACE,
        PTE_COUNT,

        // PDE_PCF
        PDE_PCF_VALID_CACHED_ATS_ALLOWED__OR__ILWALID_ATS_ALLOWED           = SW_MMU_PDE_PCF_VALID_CACHED_ATS_ALLOWED       + PTE_COUNT,
        PDE_PCF_VALID_CACHED_ATS_ALLOWED                                    = SW_MMU_PDE_PCF_VALID_CACHED_ATS_ALLOWED       + PTE_COUNT,
        PDE_PCF_ILWALID_ATS_ALLOWED                                         = SW_MMU_PDE_PCF_ILWALID_ATS_ALLOWED            + PTE_COUNT,
        PDE_PCF_VALID_UNCACHED_ATS_ALLOWED                                  = SW_MMU_PDE_PCF_VALID_UNCACHED_ATS_ALLOWED     + PTE_COUNT,
        PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED__OR__ILWALID_ATS_NOT_ALLOWED   = SW_MMU_PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED   + PTE_COUNT,
        PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED                                = SW_MMU_PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED   + PTE_COUNT,
        PDE_PCF_ILWALID_ATS_NOT_ALLOWED                                     = SW_MMU_PDE_PCF_ILWALID_ATS_NOT_ALLOWED        + PTE_COUNT,
        PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED__OR__SPARSE_ATS_NOT_ALLOWED  = SW_MMU_PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED + PTE_COUNT,
        PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED                              = SW_MMU_PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED + PTE_COUNT,
        PDE_PCF_SPARSE_ATS_NOT_ALLOWED                                      = SW_MMU_PDE_PCF_SPARSE_ATS_NOT_ALLOWED         + PTE_COUNT,
        PCF_MAX_COUNT
    };

    enum class PteOrPde
    {
        PTEOnly,
        PDEOnly,
        BOTH
    };

    static const vector<tuple<FIELD, string, PteOrPde> > s_FieldName = 
    {
        make_tuple(FIELD::ReadOnly, "ReadOnly", PteOrPde::PTEOnly),
        make_tuple(FIELD::AtomicDisable, "AtomicDisable", PteOrPde::PTEOnly),
        make_tuple(FIELD::Volatile, "Volatile", PteOrPde::BOTH),
        make_tuple(FIELD::Privilege, "Privilege", PteOrPde::PTEOnly),
        make_tuple(FIELD::Valid, "Valid", PteOrPde::BOTH),
        make_tuple(FIELD::Acd, "Acd", PteOrPde::PTEOnly),
        make_tuple(FIELD::Sparse, "Sparse", PteOrPde::BOTH),
    };

    static const map<APERTURE, string> s_ApertureName = 
    {
        {APERTURE::APERTURE_FB, "FB"},
        {APERTURE::APERTURE_COHERENT, "COHERENT"},
        {APERTURE::APERTURE_NONCOHERENT, "NONCOHERENT"},
        {APERTURE::APERTURE_PEER, "PEER"},
        {APERTURE::APERTURE_ILWALID, "INVALID"},
    };

    static const multimap<PCF_SW, string> s_PcfName = 
    {
        // PTE_PCF
        {PCF_SW::PTE_PCF_ILWALID,                                           "LW_MMU_VER3_PTE_PCF_ILWALID"                           },
        {PCF_SW::PTE_PCF_SPARSE,                                            "LW_MMU_VER3_PTE_PCF_SPARSE"                            },
        {PCF_SW::PTE_PCF_MAPPING_NOWHERE,                                   "LW_MMU_VER3_PTE_PCF_MAPPING_NOWHERE"                   },
        {PCF_SW::PTE_PCF_NO_VALID_4KB_PAGE,                                 "LW_MMU_VER3_PTE_PCF_NO_VALID_4KB_PAGE"                 },
        {PCF_SW::PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACE,                      "LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACE"      },
        {PCF_SW::PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACE,                    "LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACE"    },
        {PCF_SW::PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACE,                    "LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACE"    },
        {PCF_SW::PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACE,                   "LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACE"   },
        {PCF_SW::PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACE,                 "LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACE" },
        {PCF_SW::PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACE,                   "LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACE"   },
        {PCF_SW::PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACE,                 "LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACE" },
        {PCF_SW::PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACD,                      "LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACD"      },
        {PCF_SW::PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACD,                    "LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACD"    },
        {PCF_SW::PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACD,                    "LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACD"    },
        {PCF_SW::PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACE,                      "LW_MMU_VER3_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACE"      },
        {PCF_SW::PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACD,                   "LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACD"   },
        {PCF_SW::PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACD,                 "LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACD" },
        {PCF_SW::PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACD,                   "LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACD"   },
        {PCF_SW::PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACD,                 "LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACD" }, 
        {PCF_SW::PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACD,                  "LW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACD"       },
        {PCF_SW::PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACE,                  "LW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACE"       },
        {PCF_SW::PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_UNCACHED_ACE,               "LW_MMU_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_UNCACHED_ACE"    },
        {PCF_SW::PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_CACHED_ACE,                 "LW_MMU_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_CACHED_ACE"      },
        {PCF_SW::PTE_PCF_PRIVILEGE_RO_ATOMIC_UNCACHED_ACE,                  "LW_MMU_PTE_PCF_PRIVILEGE_RO_ATOMIC_UNCACHED_ACE"       },
        {PCF_SW::PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_UNCACHED_ACE,               "LW_MMU_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_UNCACHED_ACE"    },
        {PCF_SW::PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_CACHED_ACE,                 "LW_MMU_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_CACHED_ACE"      },
        {PCF_SW::PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACD,                      "LW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACD"           },
        {PCF_SW::PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACD,                    "LW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACD"         },
        {PCF_SW::PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACE,                    "LW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACE"         },
        // PDE_PCF
        {PCF_SW::PDE_PCF_VALID_CACHED_ATS_ALLOWED__OR__ILWALID_ATS_ALLOWED,             "LW_MMU_VER3_PDE_PCF_VALID_CACHED_ATS_ALLOWED__OR__ILWALID_ATS_ALLOWED"         },
        {PCF_SW::PDE_PCF_VALID_CACHED_ATS_ALLOWED,                                      "LW_MMU_VER3_PDE_PCF_VALID_CACHED_ATS_ALLOWED"                                  },
        {PCF_SW::PDE_PCF_ILWALID_ATS_ALLOWED,                                           "LW_MMU_VER3_PDE_PCF_ILWALID_ATS_ALLOWED"                                       },
        {PCF_SW::PDE_PCF_VALID_UNCACHED_ATS_ALLOWED,                                    "LW_MMU_VER3_PDE_PCF_VALID_UNCACHED_ATS_ALLOWED"                                },
        {PCF_SW::PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED__OR__ILWALID_ATS_NOT_ALLOWED,     "LW_MMU_VER3_PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED__OR__ILWALID_ATS_NOT_ALLOWED" },
        {PCF_SW::PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED,                                  "LW_MMU_VER3_PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED"                              },
        {PCF_SW::PDE_PCF_ILWALID_ATS_NOT_ALLOWED,                                       "LW_MMU_VER3_PDE_PCF_ILWALID_ATS_NOT_ALLOWED"                                   },
        {PCF_SW::PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED__OR__SPARSE_ATS_NOT_ALLOWED,    "LW_MMU_VER3_PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED__OR__SPARSE_ATS_NOT_ALLOWED"},
        {PCF_SW::PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED,                                "LW_MMU_VER3_PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED"                            },
        {PCF_SW::PDE_PCF_SPARSE_ATS_NOT_ALLOWED,                                        "LW_MMU_VER3_PDE_PCF_SPARSE_ATS_NOT_ALLOWED"                                    },
    };

    void RegisterPcf(py::enum_<PCF_SW> mmuPcf);
    Memory::Location ColwertMmuApertureToMemoryLocation
    (
        UtlMmu::APERTURE mmuAperture
    );
};

using namespace UtlMmu;

// This class represents a logical mmu segment from the point of view of a
// UTL script.  Lwrrently this class is a wrapper around MmuLevelSegment. 
// This class is an internal object which will not be explore to UTL API
// for user.
//
class UtlMmuSegment
{
public:
    UtlMmuSegment(UtlSurface * pSurface, MmuLevelSegment * pSegment, SUB_LEVEL_INDEX subLevelIdx);
    void WriteMmuEntries(UINT32 offset, UINT64 *size, 
            Channel * pChannel, UINT32 subchNum);
    vector<UtlMmuEntry *> GetEntryLists(UINT64 offset, UINT64 * size);
    RC GetMmuLevelSegmentInfo(MmuLevelSegmentInfo *pInfo) const 
    { return m_pSegment->GetMmuLevelSegmentInfo(pInfo); }
    MmuLevel::MmuSubLevelIndex GetSubLevelIndex() const 
    { return static_cast<MmuLevel::MmuSubLevelIndex>(m_SubLevelIdx); }
    MmuLevelSegment * GetRawSegment() const { return m_pSegment; }
    UtlMmuEntry * GetEntry(UINT64 offset);
    UtlSurface * GetSurface() const { return m_pSurface; }
private:
    UtlSurface * m_pSurface;
    MmuLevelSegment * m_pSegment;
    MmuLevelTree::LevelIndex m_LevelIndex;
    SUB_LEVEL_INDEX m_SubLevelIdx;

    map<UINT32, unique_ptr<UtlMmuEntry> > m_UtlEntryLists;
};

// This class represents a logical mmu entry from the point of view of a
// UTL script.  Lwrrently this class is a wrapper around MMU smallest unit. 
// All MMU opertation will base on this object.
// 
class UtlMmuEntry
{
public:
    UtlMmuEntry(UINT64 offset, UtlMmuSegment * pSegment);
    ~UtlMmuEntry();

    void InitEntry();
    vector<UINT08> GetData();
    UINT08 * GetDataPtr();
    void WriteMmuEntryPy(py::kwargs kwargs);
    void WriteMmuEntry(UtlChannel * pInbandChannel, UINT32 subchNum = 0);
    void SetAttributes(py::kwargs kwargs);

    string FieldGetAperturePy(); 
    APERTURE FieldGetAperture(); 
    void FieldSetAperture(APERTURE aperture); 

    UINT64 FieldGetPhysAddr(); 
    void FieldSetPhysAddr(UINT64 addr);

    bool FieldGetBoolPy(py::kwargs kwargs);
    bool FieldGetBool(FIELD field);
    void FieldSetBool(FIELD field, bool value);

    string FieldGetPcfPy();
    PCF_SW FieldGetPcf();
    void FieldSetPcf(PCF_SW pcf);

    UINT32 FieldGetKind();
    void FieldSetKind(UINT32 kind);

    void FieldChangePageSize(LEVEL targetLevel);
    void ChangePageEntryType(MmuLevel::MmuSubLevelIndex subLevelIdx,
            bool bToPte);
    void ConnectToLowerLevel(MmuLevel::MmuSubLevelIndex subLevelIdx); 
private:
    bool IsPteEntry();
    void SetActivePageSize(LEVEL level);
    MmuLevelTree * GetMmuLevelTree();
    void PrintMmuInfo(const string & message);
    void ResetPcf(bool valid);
    
    PCF_SW PcfHwToPcfSw(UINT32 pcfHw);
    UINT32 PcfSwToPcfHw(PCF_SW pcfSw);

    UINT64 m_Offset;
    UINT64 m_Size;
    UtlMmuSegment * m_pSegment;

    struct MMUInfo
    {
        PCF_SW PCF = PCF_SW::PCF_MAX_COUNT;
        bool valid = false;
        UINT64 physAddr = ~0x0;
        APERTURE aperture = APERTURE::APERTURE_ILWALID; 
        UINT64 pageSize = 0x0;

        MMUInfo() = default;
    };

    struct EntryOps
    {
        enum MmuEntryOpTypes
        {
            MMUENTRY_OP_SET_PCF             = 1 << 0,
            MMUENTRY_OP_SET_VALID           = 1 << 1,
            MMUENTRY_OP_SET_PA              = 1 << 2,
            MMUENTRY_OP_SET_APERTURE        = 1 << 3,

        };

        UINT32 MmuEntryOps = 0;
    };
    // Initial MMU entry information which need to be recovered at the end of test
    MMUInfo m_OrignalMmuInfo;
};
#endif
