/*
* LWIDIA_COPYRIGHT_BEGIN
*
* Copyright 2020-2022 by LWPU Corporation.  All rights reserved.  All
* information contained herein is proprietary and confidential to LWPU
* Corporation.  Any use, reproduction, or disclosure without the written
* permission of LWPU Corporation is prohibited.
*
* LWIDIA_COPYRIGHT_END
*/

#include "mmuutils.h"
#include "gpu/include/gpudev.h"
#include "mdiag/sysspec.h"
#include "mdiagsurf.h"
#include "ctrl/ctrl90f1.h"
#include "mdiag/advschd/pmsurf.h"
#include "g00x/g000/dev_mmu.h"
#include "core/include/channel.h"
#include "class/cla0b5.h"
#include "mmu/gmmu_fmt.h"
#include "mdiag/utl/utl.h"
#include "mdiag/utils/vaspaceutils.h"

unique_ptr<MmuLevelTreeManager> MmuLevelTreeManager::m_Instance;

// A note about PCFs:
//
// A PCF (Permission Control Flag) is a field in a PTE (Page Table Entry) that
// stores various access attributes for a given page of a surface (e.g.,
// read-only vs. read-write, atomic vs. no atomic, etc.)  The PCF field was
// introduced in page table version 3 and created to support Hopper.  In page
// tables prior to version 3, each access attribute is controlled by exactly
// one bit (with the exception of sparseness and cacheability, which are
// controlled with a combination of the valid and volatile bits).  In the
// version 3 page table, there isn't enough space to be able to assign one bit
// per attribute, so the attributes are instead encoded into a single integer
// called a PCF.  Because the PCF field has fewer bits than the number of
// attributes, not all combintations of attributes can be expressed with a
// valid PCF value, and so only the commonly used attribute combinations are
// represented.
//
// There is also a PCF field in the PDE (Page Directory Entry) which operates
// similarly to the PCF field in the PTE.  However, it has a different set of
// attributes that apply, so a diffent set of PCF values are used.
//
// Prior to page table version 3, changing a single attribute of a page simply
// required changing the associated bit in the PTE.  In version 3, the
// attributes are encoded in a single PCF, so changing an attribute requires
// colwersion from one PCF value to another.  In order to make this colwersion
// simpler, MODS and RM use the concept of a SW PCF.  A SW PCF is a decoded
// version of the actual PCF stored in the PTE (which is often referred to in
// the rest of this file as a "HW PCF").  Each bit of the SW PCF value
// corresponds to an attribute, and each SW PCF value corresponds to a HW PCF.

#define SW_PCF_VALID_FALSE_MASK       \
    ((1 << SW_MMU_PCF_ILWALID_IDX)    \
    | (1 << SW_MMU_PCF_LW4K_IDX)      \
    | (1 << SW_MMU_PCF_SPARSE_IDX)    \
    | (1 << SW_MMU_PCF_NOMAPPING_IDX))

#define SW_PCF_VALID_TRUE_MASK       \
    ((1 << SW_MMU_PCF_ACE_IDX)       \
    | (1 << SW_MMU_PCF_UNCACHED_IDX) \
    | (1 << SW_MMU_PCF_NOATOMIC_IDX) \
    | (1 << SW_MMU_PCF_RO_IDX)       \
    | (1 << SW_MMU_PCF_REGULAR_IDX))

namespace MmuUtils 
{
    // copy from the RM code gmmuTranslatePtePcfFromSw_GH100/ gmmuTranslatePtePcfFromHw_GH100 in gmmugh100.c
    // The reason why we need a duplicated code is caused by RM common layer can't access the per-GPU manuals
    // And those defined only existed in hopper
    RC GmmuTranslatePtePcfFromHw
    (
        UINT32 ptePcfHw,
        bool bPteValid,
        UINT32* pPtePcfSw
    )
    {
        if (!bPteValid)
        {
            switch (ptePcfHw)
            {
                case LW_MMU_VER3_PTE_PCF_ILWALID:
                    *pPtePcfSw = SW_MMU_PTE_PCF_ILWALID;
                    break;

                case LW_MMU_VER3_PTE_PCF_NO_VALID_4KB_PAGE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_NO_VALID_4KB_PAGE;
                    break;

                case LW_MMU_VER3_PTE_PCF_SPARSE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_SPARSE;
                    break;

                case LW_MMU_VER3_PTE_PCF_MAPPING_NOWHERE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_MAPPING_NOWHERE;
                    break;

                default:
                    return RC::SOFTWARE_ERROR;
            }
        }
        else
        {
            switch (ptePcfHw)
            {
                case LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACD;
                    break;

                case LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACE;
                    break;

                case LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACD;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACE;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_UNCACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_UNCACHED_ACE;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_CACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_CACHED_ACE;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_PRIVILEGE_RO_ATOMIC_UNCACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_PRIVILEGE_RO_ATOMIC_UNCACHED_ACE;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_UNCACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_UNCACHED_ACE;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_CACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_CACHED_ACE;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACD;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACE;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACD;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACE;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACD;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACE;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACD;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACE;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACD;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACE;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACD;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACE;
                    break;
                
                case LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACD;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACE;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACD:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACD;
                    break;

                case LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACE:
                    *pPtePcfSw = SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACE;
                    break;

                default:
                    return RC::SOFTWARE_ERROR;
            }
        }

        return OK;
    }

    RC GmmuTranslatePtePcfFromSw
    (
        UINT32 ptePcfSw, 
        UINT32* pPtePcfHw
    )
    {
        switch (ptePcfSw)
        {
            case SW_MMU_PTE_PCF_ILWALID:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_ILWALID;
                break;

            case SW_MMU_PTE_PCF_NO_VALID_4KB_PAGE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_NO_VALID_4KB_PAGE;
                break;

            case SW_MMU_PTE_PCF_SPARSE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_SPARSE;
                break;

            case SW_MMU_PTE_PCF_MAPPING_NOWHERE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_MAPPING_NOWHERE;
                break;

            case SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACD;
                break;

            case SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_CACHED_ACE;
                break;

            case SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACD;
                break;

            case SW_MMU_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_ATOMIC_UNCACHED_ACE;
                break;
            
            case SW_MMU_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_UNCACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_UNCACHED_ACE;
                break;
            
            case SW_MMU_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_CACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_PRIVILEGE_RW_NO_ATOMIC_CACHED_ACE;
                break;
            
            case SW_MMU_PTE_PCF_PRIVILEGE_RO_ATOMIC_UNCACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_PRIVILEGE_RO_ATOMIC_UNCACHED_ACE;
                break;
            
            case SW_MMU_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_UNCACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_UNCACHED_ACE;
                break;
            
            case SW_MMU_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_CACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_PRIVILEGE_RO_NO_ATOMIC_CACHED_ACE;
                break;
            
            case SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACD;
                break;
            
            case SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_CACHED_ACE;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACD;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RW_ATOMIC_UNCACHED_ACE;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACD;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_CACHED_ACE;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACD;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RW_NO_ATOMIC_UNCACHED_ACE;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACD;
                break;
            
            case SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RO_ATOMIC_CACHED_ACE;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACD;
                break;
            
            case SW_MMU_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RO_ATOMIC_UNCACHED_ACE;
                break;
            
            case SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACD;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_CACHED_ACE;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACD:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACD;
                break;

            case SW_MMU_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACE:
                *pPtePcfHw = LW_MMU_VER3_PTE_PCF_REGULAR_RO_NO_ATOMIC_UNCACHED_ACE;
                break;

            default:
                return RC::SOFTWARE_ERROR;
        }

        return OK;
    }

    //
    // Takes a SW PDE PCF and translates to HW PDE PCF
    // If a bit pattern is not supported by HW, return RC::SOFTWARE_ERROR.
    //
    RC GmmuTranslatePdePcfFromSw
    (
        UINT32  pdePcfSw,
        UINT32* pPdePcfHw
    )
    {
        switch (pdePcfSw)
        {
            case SW_MMU_PDE_PCF_ILWALID_ATS_ALLOWED:
                *pPdePcfHw = LW_MMU_VER3_PDE_PCF_ILWALID_ATS_ALLOWED;
                break;

            case SW_MMU_PDE_PCF_ILWALID_ATS_NOT_ALLOWED:
                *pPdePcfHw = LW_MMU_VER3_PDE_PCF_ILWALID_ATS_NOT_ALLOWED;
                break;

            case SW_MMU_PDE_PCF_SPARSE_ATS_ALLOWED:
                *pPdePcfHw = LW_MMU_VER3_PDE_PCF_SPARSE_ATS_ALLOWED;
                break;

            case SW_MMU_PDE_PCF_SPARSE_ATS_NOT_ALLOWED:
                *pPdePcfHw = LW_MMU_VER3_PDE_PCF_SPARSE_ATS_NOT_ALLOWED;
                break;

            case SW_MMU_PDE_PCF_VALID_CACHED_ATS_ALLOWED:
                *pPdePcfHw = LW_MMU_VER3_PDE_PCF_VALID_CACHED_ATS_ALLOWED;
                break;

            case SW_MMU_PDE_PCF_VALID_UNCACHED_ATS_ALLOWED:
                *pPdePcfHw = LW_MMU_VER3_PDE_PCF_VALID_UNCACHED_ATS_ALLOWED;
                break;

            case SW_MMU_PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED:
                *pPdePcfHw = LW_MMU_VER3_PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED;
                break;

            case SW_MMU_PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED:
                *pPdePcfHw = LW_MMU_VER3_PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED;
                break;

            default:
                return RC::SOFTWARE_ERROR;
        }

        return OK;
    }

    //
    // Takes a HW PDE PCF and translates to SW PDE PCF
    // If a bit pattern is not supported by SW, return RC::SOFTWARE_ERROR.
    //
    RC GmmuTranslatePdePcfFromHw
    (
        UINT32 pdePcfHw,
        GMMU_APERTURE aperture,
        UINT32* pPdePcfSw
    )
    {
        if (!aperture)
        {
            switch (pdePcfHw)
            {
                case LW_MMU_VER3_PDE_PCF_ILWALID_ATS_ALLOWED:
                    *pPdePcfSw = SW_MMU_PDE_PCF_ILWALID_ATS_ALLOWED;
                    break;

                case LW_MMU_VER3_PDE_PCF_ILWALID_ATS_NOT_ALLOWED:
                    *pPdePcfSw = SW_MMU_PDE_PCF_ILWALID_ATS_NOT_ALLOWED;
                    break;

                case LW_MMU_VER3_PDE_PCF_SPARSE_ATS_ALLOWED:
                    *pPdePcfSw = SW_MMU_PDE_PCF_SPARSE_ATS_ALLOWED;
                    break;

                case LW_MMU_VER3_PDE_PCF_SPARSE_ATS_NOT_ALLOWED:
                    *pPdePcfSw = SW_MMU_PDE_PCF_SPARSE_ATS_NOT_ALLOWED;
                    break;

                default:
                    return RC::SOFTWARE_ERROR;
            }
        }
        else
        {
            switch (pdePcfHw)
            {
                case LW_MMU_VER3_PDE_PCF_VALID_CACHED_ATS_ALLOWED:
                    *pPdePcfSw = SW_MMU_PDE_PCF_VALID_CACHED_ATS_ALLOWED;
                    break;

                case LW_MMU_VER3_PDE_PCF_VALID_UNCACHED_ATS_ALLOWED:
                    *pPdePcfSw = SW_MMU_PDE_PCF_VALID_UNCACHED_ATS_ALLOWED;
                    break;

                case LW_MMU_VER3_PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED:
                    *pPdePcfSw = SW_MMU_PDE_PCF_VALID_CACHED_ATS_NOT_ALLOWED;
                    break;

                case LW_MMU_VER3_PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED:
                    *pPdePcfSw = SW_MMU_PDE_PCF_VALID_UNCACHED_ATS_NOT_ALLOWED;
                    break;

                default:
                    return RC::SOFTWARE_ERROR;
            }
        }

        return OK;
    }

    // Remove unnecessary bits from the SW PTE PCF.  Some bits in the SW PTE
    // PCF only apply if the PTE's valid bit is true, while others only apply
    // if the PTE's valid bit is false.
    UINT32 TrimSwPtePcf(UINT32 oldSwPtePcf, bool bPteValid)
    {
        if (bPteValid)
        {
            return oldSwPtePcf & SW_PCF_VALID_TRUE_MASK;
        }
        else
        {
            UINT32 ret = oldSwPtePcf & SW_PCF_VALID_FALSE_MASK;

            // For invalid PTE_PCFs, the PTE_PCF itself can represent the invalid kind
            // Don't need invalid bit set unless it's INVALID itself. 
            if (ret & (~SW_MMU_PTE_PCF_ILWALID))
            {
                ret &= ~SW_MMU_PTE_PCF_ILWALID;
            }

            return ret;
        }
    }
}

using namespace MmuUtils;

//--------------------------------------------------------------------
//! \brief Instance method for the MmuLevelTreeManager singleton
//!
MmuLevelTreeManager *MmuLevelTreeManager::Instance()
{
    if (m_Instance == NULL)
    {
        m_Instance.reset(new MmuLevelTreeManager());
    }
    return m_Instance.get();
}

bool MmuLevelTreeManager::HasInstance()
{
    return (NULL != m_Instance);
}

void MmuLevelTreeManager::FreeResource
(
    MdiagSurf * pSurf
)
{
    MASSERT(pSurf);
    if (pSurf->GetCtxDmaOffsetGpu() && pSurf->GetGpuDev())
    {
        for (auto it = m_MmuLevelTrees.begin(); it != m_MmuLevelTrees.end(); /*nop*/)
        {
            if (get<0>(it->first) == pSurf->GetGpuDev() &&
                get<1>(it->first) == pSurf &&
                get<2>(it->first) == pSurf->GetCtxDmaOffsetGpu() &&
                get<3>(it->first) == pSurf->GetVASpaceHandle(Surface2D::DefaultVASpace))
            {
                if (Utl::HasInstance())
                {
                    Utl::Instance()->FreeSurfaceMmuLevels(pSurf);
                }
                m_MmuLevelTrees.erase(it++);
            }
            else
            {
                it++;
            }
        }
    }
}

void MmuLevelTreeManager::ShutDown()
{
    if (m_Instance)
    {
        m_Instance.reset(nullptr);
    }
}

// This function is for PTE kind to get corrosponding PTE info
RC MmuLevelTreeManager::SetPteInfoStruct
(
    LwRm * pLwRm,
    UINT64 gpuAddr,
    GpuDevice * pGpuDevice,
    LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS *pSetParams
)
{
    RC rc;

    LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS getParams = {};
    getParams.gpuAddr = gpuAddr;
    CHECK_RC(pLwRm->ControlByDevice(pGpuDevice,
                LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                &getParams, sizeof(getParams)));

    *pSetParams = {};
    pSetParams->gpuAddr = getParams.gpuAddr;
    for (UINT32 ii = 0;
            ii < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++ii)
    {
        pSetParams->pteBlocks[ii] = getParams.pteBlocks[ii];
    }

    return rc;
}
//--------------------------------------------------------------------
//! \brief Create or Get according resource MmuLevelTree 
//!
MmuLevelTree * MmuLevelTreeManager::GetMmuLevelTree
(
    GpuDevice* pGpuDevice,
    MdiagSurf* pSurf,
    UINT64 virtAddress,
    LwRm::Handle hVASpace,
    bool isUtl
)
{
    auto mmuLevelTreeKey = make_tuple(pGpuDevice, pSurf, virtAddress, hVASpace, isUtl);
    if (m_MmuLevelTrees.find(mmuLevelTreeKey) == m_MmuLevelTrees.end())
    {
        m_MmuLevelTrees[mmuLevelTreeKey].reset(new MmuLevelTree(pGpuDevice, pSurf, virtAddress, hVASpace, isUtl));
    }

    return m_MmuLevelTrees[mmuLevelTreeKey].get();
}

RC MmuLevelTreeManager::GetMmuLevel
(
    MmuLevelTree::LevelIndex index,
    GpuDevice* pGpuDevice,
    MdiagSurf* pSurf,
    UINT64 virtAddress,
    LwRm::Handle hVASpace,
    MmuLevel** pMmuLevel
)
{
    RC rc;
    MmuLevelTreeManager * pMmuMgr = MmuLevelTreeManager::Instance();
    MmuLevelTree * pMmuLevelTree = pMmuMgr->GetMmuLevelTree(pGpuDevice,
                                pSurf, virtAddress, hVASpace);
    MASSERT(pMmuLevelTree);    
    CHECK_RC(pMmuLevelTree->GetMmuLevel(index, pMmuLevel));

    return OK;
}

RC MmuLevelTreeManager::GetMmuPteLevel
(
    UINT64 pageSize,
    GpuDevice* pGpuDevice,
    MdiagSurf* pSurf,
    UINT64 virtAddress,
    LwRm::Handle hVASpace,
    MmuLevel** pMmuLevel
)
{
    RC rc;

    MmuLevelTreeManager * pMmuMgr = MmuLevelTreeManager::Instance();
    MmuLevelTree * pMmuLevelTree = pMmuMgr->GetMmuLevelTree(pGpuDevice,
                                pSurf, virtAddress, hVASpace);
    if (pageSize == PolicyManager::BYTES_IN_SMALL_PAGE)
    {
        CHECK_RC(pMmuLevelTree->GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_4K, 
                                            pMmuLevel));
    }
    else if (pageSize == PolicyManager::BYTES_IN_512MB_PAGE)
    {
        CHECK_RC(pMmuLevelTree->GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_512M, 
                                            pMmuLevel));
    }
    else if (pageSize == PolicyManager::BYTES_IN_HUGE_PAGE)
    {
        CHECK_RC(pMmuLevelTree->GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_2M, 
                                            pMmuLevel));
    }
    else
    {
        const UINT64 bigPageSize = pGpuDevice->GetBigPageSize();

        if (pageSize == bigPageSize)
        {
            CHECK_RC(pMmuLevelTree->GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_64K, 
                                                pMmuLevel));
        }
        else
        {
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
} 

//--------------------------------------------------------------------
//! \brief constructor
//!
MmuLevelTree::MmuLevelTree
(
    GpuDevice *pGpuDevice,
    MdiagSurf *pSurface,
    UINT64 virtAddress,
    LwRm::Handle hVASpace,
    bool isUtl
) :
    m_pGpuDevice(pGpuDevice),
    m_BaseVirtAddr(virtAddress),
    m_hVASpace(hVASpace),
    m_pSurface(pSurface),
    m_pGmmuFmt(NULL),
    m_IsUtl(isUtl),
    m_ActivePageSize(0)
{
}

//--------------------------------------------------------------------
//! \brief Get a MmuLevel via level index
//!
RC MmuLevelTree::GetMmuLevel
(
    LevelIndex level,
    MmuLevel** ppMmuLevel
)
{
    RC rc;

    if (static_cast<UINT32>(level) >= m_MmuLevels.size())
    {
        return RC::ILWALID_ARGUMENT;
    }

    *ppMmuLevel = m_MmuLevels[level].get();
    return rc;
}


//--------------------------------------------------------------------
//! \brief Explore all MmuLevels of a surface
//!
RC MmuLevelTree::ExploreMmuLevels()
{
    //
    // Retry old API in case new API failed
    //  New API only works in case -enable_vmm
    //  Old API only has part of pte information.
    //
    if (OK != ExploreMmuLevelsGetPageLevelInfo())
    {
        return ExploreMmuLevelsGetPdeInfo();
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Get active page size in PTE
//!
RC MmuLevelTree::GetSurfaceActivePageSize(UINT64 offset, UINT64 *pageSize)
{
    RC rc;

    MmuLevel *pMmuLevel = NULL;
    MmuLevelSegment *pSegment = NULL;

    for (int i = GMMU_LEVEL_PTE_4K; i <= GMMU_LEVEL_PTE_512M; ++ i)
    {
        bool isPte = false;
        MmuLevelTree::LevelIndex level = static_cast<MmuLevelTree::LevelIndex>(i);
        if (OK != GetMmuLevel(level, &pMmuLevel))
        {
            continue; // does not exist; continue trying next level
        }

        if (OK != pMmuLevel->GetMmuLevelSegment(offset, &pSegment))
        {
            continue; // does not exist; continue trying next level
        }

        if (pSegment->IsEntryValid(offset,
            MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT, &isPte) && isPte)
        {
            switch(level)
            {
            case GMMU_LEVEL_PTE_512M:
                *pageSize = PolicyManager::BYTES_IN_512MB_PAGE;
                break;
            case GMMU_LEVEL_PTE_2M:
                *pageSize = PolicyManager::BYTES_IN_HUGE_PAGE;
                break;
            case GMMU_LEVEL_PTE_64K:
                *pageSize = m_pGpuDevice->GetBigPageSize();
                break;
            case GMMU_LEVEL_PTE_4K:
                *pageSize = PolicyManager::BYTES_IN_SMALL_PAGE;
                break;
            default:
                MASSERT(!"should NOT hit here");
            }

            m_ActivePageSize = *pageSize;
            return OK;
        }
    }

    ErrPrintf("Surface pages are invalid. So page size can't be identified due to no active pages!\n");
    return RC::SOFTWARE_ERROR;
}


RC MmuLevelTree::GetSurfacePhysAddr
(
    UINT64 offset,
    UINT64 pageSize,
    UINT64 *pPhysAddr,
    GMMU_APERTURE *pAperture
)
{
    RC rc;

    MmuLevel *pMmuLevel = NULL;
    MmuLevelSegment *pSegment = NULL;
    MmuLevelTreeManager * pMmuMgr = MmuLevelTreeManager::Instance();
    CHECK_RC(pMmuMgr->GetMmuPteLevel(pageSize, m_pSurface->GetGpuDev(),
            m_pSurface, m_pSurface->GetCtxDmaOffsetGpu(), 
            m_pSurface->GetVASpaceHandle(Surface2D::DefaultVASpace), &pMmuLevel));
    CHECK_RC(pMmuLevel->GetMmuLevelSegment(offset, &pSegment));

    // subLevel is meanless for PTE -> GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT
    CHECK_RC(pSegment->GetEntryPhysAddr(
        offset, MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT,
        pPhysAddr, pAperture));
    return rc;
}


//--------------------------------------------------------------------
//! \brief Private Function.
//!        Use old RM API to get PDEs info
//!        It can be removed after vmm mode is enabled permanently
RC MmuLevelTree::ExploreMmuLevelsGetPdeInfo()
{
    RC rc;

    //
    // old API only support PTE info
    //
    // Entries other than GMMU_LEVEL_PTE_4K/GMMU_LEVEL_PTE_64K/GMMU_LEVEL_PTE_2M
    // should be empty
    m_MmuLevels.clear();
    for (UINT32 level = GMMU_LEVEL_PTE_4K; level < GMMU_LEVEL_PDE_LAST; ++level)
    {
        m_MmuLevels.push_back(unique_ptr<V1MmuLevel>(new V1MmuLevel(m_pGpuDevice, m_pSurface,
                                       m_pGmmuFmt, level, this)));
    }

    for (UINT64 offset = 0; offset < m_pSurface->GetSize(); /*nop*/)
    {
        LwRm* pLwRm = m_pSurface->GetLwRmPtr();
        LW0080_CTRL_DMA_GET_PDE_INFO_PARAMS getParams = {0};
        LW0080_CTRL_DMA_ADV_SCHED_GET_VA_CAPS_PARAMS vasCaps = {0};

        UINT64 virtAddr = m_BaseVirtAddr + offset;
        getParams.gpuAddr = virtAddr;
        CHECK_RC(pLwRm->ControlByDevice(m_pGpuDevice,
            LW0080_CTRL_CMD_DMA_GET_PDE_INFO,
            &getParams, sizeof(getParams)));

        CHECK_RC(pLwRm->ControlByDevice(m_pGpuDevice,
            LW0080_CTRL_CMD_DMA_ADV_SCHED_GET_VA_CAPS,
            &vasCaps, sizeof(vasCaps)));

        UINT64 surfSegmentSize = 0;
        for (UINT32 i = 0; i < LW0080_CTRL_DMA_PDE_INFO_PTE_BLOCKS; ++i)
        {
            if (getParams.pteBlocks[i].pageSize != 0)
            {
                UINT64 pageSize = getParams.pteBlocks[i].pageSize;

                MmuLevelSegmentInfo info;
                switch(getParams.pteBlocks[i].pteAddrSpace)
                {
                case 0: info.location = Memory::Fb; break;
                case 1: info.location = Memory::Coherent; break;
                case 2: info.location = Memory::NonCoherent; break;
                default: MASSERT(!"Unsupported location!\n");
                }

                info.entrySize = getParams.pteBlocks[i].pteEntrySize;
                info.pFmtLevel = NULL;
                info.levelPageSize = pageSize;
                info.tableBasePhysAddress = 0;
                info.physAddress =
                    getParams.pteBlocks[i].ptePhysAddr +
                    (((virtAddr & (BIT64(vasCaps.pdeCoverageBitCount) - 1)) / pageSize) *
                    getParams.pteBlocks[i].pteEntrySize);
                info.pCpuAddress = 0;
                info.mappedHandle = 0;
                info.surfOffset = offset;
                surfSegmentSize = min(m_pSurface->GetSize() - offset,
                                  (UINT64)(1 << vasCaps.pdeCoverageBitCount));
                info.surfSegmentSize = surfSegmentSize;

                if (pageSize == PolicyManager::BYTES_IN_SMALL_PAGE)
                {
                    m_MmuLevels[GMMU_LEVEL_PTE_4K]->AddMmuLevelSegment(info);
                }
                else
                {
                    m_MmuLevels[GMMU_LEVEL_PTE_64K]->AddMmuLevelSegment(info);
                }
            }
        }

        MASSERT(surfSegmentSize > 0);
        offset += surfSegmentSize;
    }

    return rc;
}


//--------------------------------------------------------------------
//! \brief Private Function.
//!        Use new RM API to get PDEs/PTEs info in different levels
//!        It only works in vmm-enable mode.
RC MmuLevelTree::ExploreMmuLevelsGetPageLevelInfo()
{
    RC rc;

    //
    // API LW90F1_CTRL_CMD_VASPACE_GET_PAGE_LEVEL_INFO only works on -enable_vmm.
    //
    LwRm* pLwRm = m_pSurface->GetLwRmPtr();
    MASSERT(m_pGpuDevice->GetNumSubdevices() == 1);
    LwRm::Handle hSubDevice =
        pLwRm->GetSubdeviceHandle(m_pGpuDevice->GetSubdevice(0));

    // Use default VA space if it is not specified
    //
    if (!m_hVASpace)
    {
        CHECK_RC(VaSpaceUtils::GetFermiVASpaceHandle(
            LW_VASPACE_ALLOCATION_INDEX_GPU_DEVICE,
            pLwRm->GetDeviceHandle(m_pGpuDevice),
            &m_hVASpace,
            pLwRm));
    }

    // Get top level mmu format
    //
    CHECK_RC(VaSpaceUtils::GetVASpaceGmmuFmt(m_hVASpace,
        hSubDevice, &m_pGmmuFmt, pLwRm));

    if (NULL == m_pGmmuFmt)
    {
        ErrPrintf("%s vmm is not enabled! RM API GET_PAGE_LEVEL_INFO can't work! Trying old API\n",
                  __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    LevelIndex rootLevel;
    switch (m_pGmmuFmt->version)
    {
    case GMMU_FMT_VERSION_1:
        rootLevel = GMMU_LEVEL_PDE0;
        break;
    case GMMU_FMT_VERSION_2:
        rootLevel = GMMU_LEVEL_PDE3;
        break;
#if LWCFG(GLOBAL_ARCH_HOPPER)
    case GMMU_FMT_VERSION_3:
        // If a 57-bit virtual address space is used, then the root level
        // is PDE4, otherwise it's PDE3 (e.g., 49-bit virtual address space).
        if (m_pGmmuFmt->pRoot->virtAddrBitHi == 56)
        {
            rootLevel = GMMU_LEVEL_PDE4;
        }
        else
        {
            rootLevel = GMMU_LEVEL_PDE3;
        }
        break;
#endif
    default:
        ErrPrintf("%s: Unsupported GMMU format version %d! Trying old API\n",
                  __FUNCTION__, m_pGmmuFmt->version);
        return RC::SOFTWARE_ERROR;
        break;
    }

    //
    // Get information of all mmu levels
    //
    MASSERT(m_MmuLevels.size() == 0);
    for (UINT32 level = GMMU_LEVEL_PTE_4K; level < GMMU_LEVEL_PDE_LAST; ++level)
    {
        if (m_IsUtl)
        {
            m_MmuLevels.push_back(unique_ptr<MmuLevel>(new MmuLevel(m_pGpuDevice, m_pSurface,
                        m_pGmmuFmt, level, this)));
        }
        else
        {
            m_MmuLevels.push_back(unique_ptr<MmuLevel>(new PmMmuLevel(m_pGpuDevice, m_pSurface,
                        m_pGmmuFmt, level, this)));
        }
    }

    UINT64 pdbPhysAddr = ~0;
    UINT32 pdbAperture = ~0;
    for (UINT32 pageSizeIdx = PolicyManager::BIG_PAGE_SIZE;
         pageSizeIdx <= PolicyManager::SMALL_PAGE_SIZE; ++pageSizeIdx)
    {
        UINT64 pageSize = 0;
        if (pageSizeIdx == PolicyManager::BIG_PAGE_SIZE)
        {
            pageSize = m_pGpuDevice->GetBigPageSize();
        }
        else if (pageSizeIdx == PolicyManager::SMALL_PAGE_SIZE)
        {
            pageSize = PolicyManager::BYTES_IN_SMALL_PAGE;
        }
        else
        {
            MASSERT(!"No need to explore other page size!\n");
        }

        //
        // scan segments from the root PD to GMMU_LEVEL_PTE_64K
        for (UINT64 offset = 0; offset < m_pSurface->GetSize(); /*nop*/)
        {
            LW90F1_CTRL_VASPACE_GET_PAGE_LEVEL_INFO_PARAMS mmuLevelInfo = {0};
            mmuLevelInfo.hSubDevice = hSubDevice;
            mmuLevelInfo.virtAddress = m_BaseVirtAddr + offset;

            mmuLevelInfo.pageSize = pageSize;
            CHECK_RC(pLwRm->Control(m_hVASpace,
                LW90F1_CTRL_CMD_VASPACE_GET_PAGE_LEVEL_INFO,
                &mmuLevelInfo,
                sizeof(mmuLevelInfo)));

            if (mmuLevelInfo.numLevels == 0)
            {
                break;
            }

            pdbPhysAddr = mmuLevelInfo.levels[0].physAddress;
            pdbAperture = mmuLevelInfo.levels[0].aperture;

            UINT32 level = 0;
            if (pageSizeIdx == PolicyManager::BIG_PAGE_SIZE)
            {
                // Update root ~ GMMU_LEVEL_PTE_64K
                level = 0;
            }
            else if (pageSizeIdx == PolicyManager::SMALL_PAGE_SIZE)
            {
                // Only update last level (GMMU_LEVEL_PTE_4K)
                level = mmuLevelInfo.numLevels - 1;
                if ((0 == mmuLevelInfo.numLevels) ||
                    (4096 != mmuFmtLevelPageSize(&(mmuLevelInfo.levels[level].levelFmt))))
                {
                    // No GMMU_LEVEL_PTE_4k
                    break;
                }
            }
            else
            {
                MASSERT(!"No need to explore other page size!\n");
            }

            UINT64 surfSegmentSize = 0;
            for (; level < mmuLevelInfo.numLevels; ++ level)
            {
                UINT32 entryIndex = mmuFmtVirtAddrToEntryIndex(
                    &(mmuLevelInfo.levels[level].levelFmt), m_BaseVirtAddr + offset);

                MmuLevelSegmentInfo info = {0};
                MMU_FMT_LEVEL       *pSubLevels = NULL;

                info.fmtLevelVec.resize(sizeof(MMU_FMT_LEVEL));
                info.pFmtLevel = (MMU_FMT_LEVEL *)&(info.fmtLevelVec[0]);
                MASSERT(info.pFmtLevel);
                memcpy((info.pFmtLevel), &(mmuLevelInfo.levels[level].levelFmt), sizeof(MMU_FMT_LEVEL));

                info.fmtSubLevelVecs.resize(sizeof(MMU_FMT_LEVEL) * MMU_FMT_MAX_SUB_LEVELS);
                pSubLevels = (MMU_FMT_LEVEL *)&(info.fmtSubLevelVecs[0]);
                MASSERT(pSubLevels);
                memcpy(pSubLevels, &(mmuLevelInfo.levels[level].sublevelFmt), sizeof(MMU_FMT_LEVEL) * MMU_FMT_MAX_SUB_LEVELS);
                info.pFmtLevel->subLevels = pSubLevels;

                info.levelPageSize = mmuFmtLevelPageSize(info.pFmtLevel);
                info.tableBasePhysAddress = mmuLevelInfo.levels[level].physAddress;
                info.physAddress = mmuLevelInfo.levels[level].physAddress +
                    entryIndex * (info.pFmtLevel->entrySize);
                info.entrySize = info.pFmtLevel->entrySize;
                info.pCpuAddress = 0;
                info.mappedHandle = 0;
                info.surfOffset = offset;

                UINT64 entryCount = mmuFmtLevelEntryCount(info.pFmtLevel);
                surfSegmentSize = (entryCount - entryIndex) * info.levelPageSize;
                info.surfSegmentSize =
                    surfSegmentSize > m_pSurface->GetSize() - offset?
                    (m_pSurface->GetSize() - offset) : surfSegmentSize;
                surfSegmentSize = info.surfSegmentSize;

                //
                // Colwert the aperture to generic definition
                // PDE/PTE/CE class has different aperture definition
                //
                switch(mmuLevelInfo.levels[level].aperture)
                {
                case GMMU_APERTURE_VIDEO:
                    info.location = Memory::Fb;
                    break;

                case GMMU_APERTURE_SYS_COH:
                    info.location = Memory::Coherent;
                    break;

                case GMMU_APERTURE_SYS_NONCOH:
                    info.location = Memory::NonCoherent;
                    break;

                case GMMU_APERTURE_PEER:
                case GMMU_APERTURE_ILWALID:
                    MASSERT(!"Unsupported GMMU Aperture format!\n");
                    break;
                }

                if (pageSizeIdx == PolicyManager::SMALL_PAGE_SIZE)
                    m_MmuLevels[GMMU_LEVEL_PTE_4K]->AddMmuLevelSegment(info);
                else
                    m_MmuLevels[rootLevel - level]->AddMmuLevelSegment(info);
            }

            MASSERT(surfSegmentSize > 0);
            offset  += surfSegmentSize;
        }
    }

    // Save PDB informtion to avoid another query to RM
    CHECK_RC(VaSpaceUtils::SetVASpaceInfo(m_hVASpace,
        hSubDevice, pdbPhysAddr, pdbAperture, m_pGmmuFmt));

    return rc;
}


//--------------------------------------------------------------------
//! \brief constructor
//!
MmuLevel::MmuLevel
(
    GpuDevice               *pGpuDevice,
    MdiagSurf               *pSurface,
    const struct GMMU_FMT   *pGmmuFmt,
    UINT32                   levelId,
    MmuLevelTree            *pMmuLevelTree
):
    m_pGpuDevice(pGpuDevice),
    m_pSurface(pSurface),
    m_LevelId(levelId),
    m_pGmmuFmt(pGmmuFmt),
    m_pMmuLevelTree(pMmuLevelTree)
{
}

//--------------------------------------------------------------------
//! \brief Get the MmuLevel object which is one level below current one
//!
//! \param subLevelIdx - MmuSubLevelIndex: 4k or 64;
//!                      At present only valid for PDE0;
//!                      It will be ignored for other PDEs;
//!                      NULL will be returned if current level is leaf level.
RC MmuLevel::GetLowerMmuLevel
(
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    MmuLevel **ppMmuLevel
) const
{
    RC rc;

    MmuLevelTree::LevelIndex oneLevelBelow;
    switch(m_LevelId)
    {
    case MmuLevelTree::GMMU_LEVEL_PDE0:
        {
            if (subLevelIdx == GMMU_SUB_LEVEL_PAGE_TABLE_SMALL)
                oneLevelBelow = MmuLevelTree::GMMU_LEVEL_PTE_4K;
            else
                oneLevelBelow = MmuLevelTree::GMMU_LEVEL_PTE_64K;
        }
        break;

    case MmuLevelTree::GMMU_LEVEL_PDE1:
    case MmuLevelTree::GMMU_LEVEL_PDE2:
    case MmuLevelTree::GMMU_LEVEL_PDE3:
        oneLevelBelow = static_cast<MmuLevelTree::LevelIndex>(m_LevelId - 1);
        break;

    case MmuLevelTree::GMMU_LEVEL_PTE_4K:
    case MmuLevelTree::GMMU_LEVEL_PTE_64K:
    default:
        *ppMmuLevel = NULL;
        return RC::ILWALID_ARGUMENT;
    }

    CHECK_RC(m_pMmuLevelTree->GetMmuLevel(oneLevelBelow, ppMmuLevel));
    return rc;
}

//--------------------------------------------------------------------
//! \brief Get mmu info(MmuLevelSegmentInfo) of a level according to offset
//!
//! \param offset: offset in the surface; used to callwlate entry index
RC MmuLevel::GetMmuLevelSegmentInfo
(
    UINT64 offset,
    MmuLevelSegmentInfo *pInfo
) const
{
    RC rc;

    if (!pInfo)
    {
        return RC::ILWALID_ARGUMENT;
    }

    UINT32 segmentId = 0;
    CHECK_RC(GetSegmentId(offset, segmentId));
    const MmuLevelSegment* pSegment = m_MmuLevelSegments[segmentId].get();
    CHECK_RC(pSegment->GetMmuLevelSegmentInfo(pInfo));

    //
    // Addjust the physAddress according to offset
    UINT64 levelPageSize = pInfo->levelPageSize;
    pInfo->physAddress += (offset - pInfo->surfOffset +
         + levelPageSize - 1) / levelPageSize * pInfo->entrySize;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get MmuLevel segment of a level according to offset
//!
//! \param offset: offset in the surface; used to callwlate entry index
RC MmuLevel::GetMmuLevelSegment(UINT64 offset, MmuLevelSegment **ppSegment)
{
    RC rc;

    UINT32 segmentId = 0;
    CHECK_RC(GetSegmentId(offset, segmentId));
    *ppSegment = m_MmuLevelSegments[segmentId].get();

    return rc;
}


//--------------------------------------------------------------------
//! \brief Append a mmu segment to a level
//!
//!        Only append a new segment in case it doesn't exist
RC MmuLevel::AddMmuLevelSegment(const MmuLevelSegmentInfo& info)
{
    RC rc;

    UINT32 segmentId = 0;
    if(OK != GetSegmentId(info.surfOffset, segmentId))
    {
        // does not exist
        m_MmuLevelSegments.push_back(unique_ptr<MmuLevelSegment>
                (new MmuLevelSegment(this, info)));
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Private function
//!        One level may have multiple segments;
//!        multiple segment is referenced by segment index
RC MmuLevel::GetSegmentId(UINT64 offset, UINT32& segmentId) const
{
    RC rc;

    for (UINT32 id = 0; id < m_MmuLevelSegments.size(); ++ id)
    {
        MmuLevelSegmentInfo info;
        CHECK_RC(m_MmuLevelSegments[id]->GetMmuLevelSegmentInfo(&info));
        if ((offset >= info.surfOffset) &&
            (offset < info.surfOffset + info.surfSegmentSize))
        {
            segmentId = id;
            return rc;
        }
    }

    return RC::ILWALID_ARGUMENT;
}

//--------------------------------------------------------------------
//! \brief constructor
//!
MmuLevelSegment::MmuLevelSegment
(
    MmuLevel *pMmuLevel,
    const MmuLevelSegmentInfo &info
)
:
    m_pMmuLevel(pMmuLevel)
{
    m_SegmentInfo = info;
    m_SegmentInfo.pFmtLevel = (MMU_FMT_LEVEL *)&(m_SegmentInfo.fmtLevelVec[0]);
    m_SegmentInfo.pFmtLevel->subLevels = (MMU_FMT_LEVEL *)&(m_SegmentInfo.fmtSubLevelVecs[0]);
    m_UnmappedPdeAperture = GMMU_APERTURE_ILWALID;
}


//--------------------------------------------------------------------
//! \brief destructor
//!
MmuLevelSegment::~MmuLevelSegment()
{
    UnmapCpuPointer();
}


//--------------------------------------------------------------------
//! \brief Map CPU address of the segment
//!
RC MmuLevelSegment::MapCpuPointer()
{
    RC rc;

    if (NULL == m_SegmentInfo.pCpuAddress)
    {
        UINT64 levelPageSize = m_SegmentInfo.levelPageSize;
        UINT64 pageCount =
            (m_SegmentInfo.surfSegmentSize + levelPageSize - 1) / levelPageSize;

        if (Memory::Fb == m_SegmentInfo.location)
        {
            MASSERT(0 == m_SegmentInfo.mappedHandle);

            // Map the full size of the segment
            //
            LwRm* pLwRm = m_pMmuLevel->GetSurface()->GetLwRmPtr();
            LW0080_CTRL_FB_CREATE_FB_SEGMENT_PARAMS createFbSegParams = {0};
            GpuDevice *pGpuDev = m_pMmuLevel->GetGpuDevice();
            UINT64 pageSize = 0x1000;  // 4k pagesize specified in flags
            MASSERT((pageSize & (pageSize - 1)) == 0);
            UINT64 offsetMask = pageSize - 1;
            createFbSegParams.VidOffset       = m_SegmentInfo.physAddress & ~offsetMask;
            createFbSegParams.ValidLength     =
                ALIGN_UP(m_SegmentInfo.physAddress +
                         m_SegmentInfo.entrySize * pageCount, pageSize)
                - ALIGN_DOWN(m_SegmentInfo.physAddress, pageSize);
            createFbSegParams.Length          = createFbSegParams.ValidLength;
            createFbSegParams.AllocHintHandle = 0;
            createFbSegParams.hClient         = pLwRm->GetClientHandle();
            createFbSegParams.hDevice         = pLwRm->GetDeviceHandle(pGpuDev);
            createFbSegParams.Flags           =
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _FIXED_OFFSET, _NO) |
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _MAP_CPUVA, _YES) |
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _CONTIGUOUS, _TRUE) |
                DRF_DEF(0080_CTRL_CMD_FB, _CREATE_FB_SEGMENT, _PAGE_SIZE, _4K);
            createFbSegParams.subDeviceIDMask = 1; // Just enable subdevice 0

            CHECK_RC(pLwRm->CreateFbSegment(pGpuDev, &createFbSegParams));

            m_SegmentInfo.mappedHandle = createFbSegParams.hMemory;
            m_SegmentInfo.pCpuAddress = (UINT08 *)createFbSegParams.ppCpuAddress[0] +
                (m_SegmentInfo.physAddress & offsetMask);
        }
        else
        {
            MASSERT((Memory::Coherent == m_SegmentInfo.location) ||
                    (Memory::NonCoherent == m_SegmentInfo.location));

            UINT64 pageSize = static_cast<UINT64>(Platform::GetPageSize());
            MASSERT((pageSize & (pageSize - 1)) == 0);
            UINT64 offsetMask = pageSize - 1;

            PHYSADDR physAddr =
                m_SegmentInfo.physAddress & ~offsetMask;
            UINT64 size =
                ALIGN_UP(physAddr + m_SegmentInfo.entrySize * pageCount, pageSize)
                - ALIGN_DOWN(physAddr, pageSize);

            void* pMem = 0;
            CHECK_RC(Platform::MapDeviceMemory(&pMem, physAddr,
                static_cast<size_t>(size), Memory::UC, Memory::ReadWrite));
            m_SegmentInfo.pCpuAddress = static_cast<UINT08*>(pMem) +
                (m_SegmentInfo.physAddress & offsetMask);
        }
    }

    return rc;
}


//--------------------------------------------------------------------
//! \brief Unmap CPU address of the segment
//!
RC MmuLevelSegment::UnmapCpuPointer()
{
    RC rc;

    if (NULL != m_SegmentInfo.pCpuAddress)
    {
        if (Memory::Fb == m_SegmentInfo.location)
        {
            MASSERT(0 != m_SegmentInfo.mappedHandle);

            LwRm* pLwRm = m_pMmuLevel->GetSurface()->GetLwRmPtr();
            LW0080_CTRL_FB_DESTROY_FB_SEGMENT_PARAMS destroyFbSegParams = {0};
            destroyFbSegParams.hMemory = m_SegmentInfo.mappedHandle;
            CHECK_RC(pLwRm->DestroyFbSegment(
                m_pMmuLevel->GetGpuDevice(), &destroyFbSegParams));

            m_SegmentInfo.mappedHandle = 0;
            m_SegmentInfo.pCpuAddress = 0;
        }
        else
        {
            MASSERT((Memory::Coherent == m_SegmentInfo.location) ||
                    (Memory::NonCoherent == m_SegmentInfo.location));

            Platform::UnMapDeviceMemory((void*)m_SegmentInfo.pCpuAddress);
            m_SegmentInfo.pCpuAddress = 0;
        }
    }

    return rc;
}


//--------------------------------------------------------------------
//! \brief Get MmuLevelSegmentInfo held in MmuLevelSegment
//!
RC MmuLevelSegment::GetMmuLevelSegmentInfo(MmuLevelSegmentInfo *pInfo) const
{
    RC rc;

    if (!pInfo)
    {
        return RC::ILWALID_ARGUMENT;
    }

    *pInfo = m_SegmentInfo;
    return rc;
}


//--------------------------------------------------------------------
//! \brief  Modify physical address to point to lower level page table
//!         Modification happens on local saved data of the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//!
RC MmuLevelSegment::ConnectToLowerLevel
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT64 levelPageSize = m_SegmentInfo.levelPageSize;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    // Modify entry data page by page
    for (size_t i = 0; i < pageCnt; ++i)
    {
        // Get physical address of lower page table
        MmuLevel* pLowerLevel = NULL;
        MmuLevelSegmentInfo mmuLevelSegmentInfo;
        CHECK_RC(m_pMmuLevel->GetLowerMmuLevel(subLevelIdx, &pLowerLevel));
        CHECK_RC(pLowerLevel->GetMmuLevelSegmentInfo(offset + i * levelPageSize, &mmuLevelSegmentInfo));

        UINT64 physAddr = mmuLevelSegmentInfo.tableBasePhysAddress;
        Memory::Location location = mmuLevelSegmentInfo.location;

        const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
        const GMMU_FIELD_ADDRESS *pFmtFieldAddr = 0;
        const GMMU_FIELD_APERTURE *pFmtAperture = 0;
        GMMU_APERTURE aperture = GMMU_APERTURE_VIDEO;

        // Must to be a PDE to connect lower level
        const GMMU_FMT_PDE* pFmtPde =
            gmmuFmtGetPde(pFmt,
                          m_SegmentInfo.pFmtLevel,
                          subLevelIdx);
        pFmtAperture = &pFmtPde->fldAperture;

        switch (location)
        {
        case Memory::Fb:
            aperture = GMMU_APERTURE_VIDEO;
#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (pFmt->version == GMMU_FMT_VERSION_3)
            {
                pFmtFieldAddr = &pFmtPde->fldAddr;
            }
            else
#endif
            {
                pFmtFieldAddr = &pFmtPde->fldAddrVidmem;
            }
            break;
        case Memory::Coherent:
            aperture = GMMU_APERTURE_SYS_COH;
#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (pFmt->version == GMMU_FMT_VERSION_3)
            {
                pFmtFieldAddr = &pFmtPde->fldAddr;
            }
            else
#endif
            {
                pFmtFieldAddr = &pFmtPde->fldAddrSysmem;
            }
            break;
        case Memory::NonCoherent:
            aperture = GMMU_APERTURE_SYS_NONCOH;
#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (pFmt->version == GMMU_FMT_VERSION_3)
            {
                pFmtFieldAddr = &pFmtPde->fldAddr;
            }
            else
#endif
            {
                pFmtFieldAddr = &pFmtPde->fldAddrSysmem;
            }
            break;
        default:
            MASSERT(!"Unsupported location!\n");
        }

        // Only utility functions understand the entry format;
        // Call utility functions to modify the saved location data.
        gmmuFieldSetAddress(pFmtFieldAddr, physAddr, pEntryData + i * m_SegmentInfo.entrySize);
        gmmuFieldSetAperture(pFmtAperture, aperture, pEntryData + i * m_SegmentInfo.entrySize);
    }

    *pSize = realSize;
    return rc;
}

RC MmuLevelSegment::SetAperture
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    Memory::Location location
)
{
    RC rc;

    UINT64 pageCount = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = 
        GetEntryLocalData(offset, *pSize, &realSize, &pageCount);
    MASSERT(pEntryData);

    GMMU_APERTURE aperture = GMMU_APERTURE_VIDEO;
    switch(location)
    {
        case Memory::Fb:
            aperture = GMMU_APERTURE_VIDEO;
            break;
        case Memory::Coherent:
            aperture = GMMU_APERTURE_SYS_COH;
            break;
        case Memory::NonCoherent:
            aperture = GMMU_APERTURE_SYS_NONCOH;
            break;
        default:
            MASSERT(!"Unsupported location!\n");
    }

    for (UINT32 i = 0; i < pageCount; ++i)
    {
        if (IsPteEntry(pEntryData + i * m_SegmentInfo.entrySize))
        {
            const GMMU_FMT_PTE* pFmtPte = m_pMmuLevel->GetGmmuFmt()->pPte;
            gmmuFieldSetAperture(&pFmtPte->fldAperture, aperture,
                pEntryData + i * m_SegmentInfo.entrySize);
        }
        else
        {
            const GMMU_FMT_PDE* pFmtPde =
                gmmuFmtGetPde(m_pMmuLevel->GetGmmuFmt(), 
                    m_SegmentInfo.pFmtLevel, subLevelIdx);
            gmmuFieldSetAperture(&pFmtPde->fldAperture, aperture,
                pEntryData + i * m_SegmentInfo.entrySize);
        }
    }
    *pSize = realSize;
    return rc;
}
    

//--------------------------------------------------------------------
//! \brief Modify the valid bit(s) in saved PDEs/PTEs data of the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param valid: value of bit0 for PTE; value of bit1~2 for PDE
//!
RC MmuLevelSegment::SetSegmentValid
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool valid
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    for (size_t i = 0; i < pageCnt; ++ i)
    {
        if (IsPteEntry(pEntryData + i * m_SegmentInfo.entrySize))
        {
            // If last level of 512MB/2MB pages and remapping (PTE.Valid = TRUE)
            if (valid && 
                (m_pMmuLevel->GetLevelId() == MmuLevelTree::GMMU_LEVEL_PTE_2M ||
                m_pMmuLevel->GetLevelId() == MmuLevelTree::GMMU_LEVEL_PTE_512M))
            {
                // While unmapping (PTE.Valid=FALSE) PDE.Aperture will be set to INVALID
                // thereby corrupting PTE.Aperture (bits overlap in the two structs). 
                // Reset the PDE.Aperture so that PTE.Aperture returns to its original value
                const GMMU_FMT_PDE* pFmtPde =
                    gmmuFmtGetPde(m_pMmuLevel->GetGmmuFmt(),
                                  m_SegmentInfo.pFmtLevel,
                                  subLevelIdx);
                if (m_UnmappedPdeAperture != GMMU_APERTURE_ILWALID)
                {
                    gmmuFieldSetAperture(&pFmtPde->fldAperture, m_UnmappedPdeAperture,
                                     pEntryData + i * m_SegmentInfo.entrySize);
                }
            }

            const GMMU_FMT_PTE* pFmtPte = m_pMmuLevel->GetGmmuFmt()->pPte;
            lwFieldSetBool(&pFmtPte->fldValid, valid, pEntryData +
                           i * m_SegmentInfo.entrySize);

            // If last level of 512MB/2MB pages and unmapping (PTE.Valid = FALSE)
            if (!valid && 
                (m_pMmuLevel->GetLevelId() == MmuLevelTree::GMMU_LEVEL_PTE_2M ||
                m_pMmuLevel->GetLevelId() == MmuLevelTree::GMMU_LEVEL_PTE_512M))
            {
                // Setting PTE.Valid=FALSE for 512MB/2MB changed PTE to PDE1 
                // Set PDE1.Aperture=INVALID so that MMU does not access the next level 
                // when the surface is accessed 
                // Also, save the PDE.Aperture so that it can be used when remapping (PTE.Valid=TRUE)
                const GMMU_FMT_PDE* pFmtPde =
                    gmmuFmtGetPde(m_pMmuLevel->GetGmmuFmt(),
                                  m_SegmentInfo.pFmtLevel,
                                  subLevelIdx);
                GMMU_APERTURE lwrrUnmappedPdeAperture = gmmuFieldGetAperture(&pFmtPde->fldAperture, 
                                pEntryData + i * m_SegmentInfo.entrySize);
                if (lwrrUnmappedPdeAperture != GMMU_APERTURE_ILWALID)
                {
                    m_UnmappedPdeAperture = lwrrUnmappedPdeAperture;
                }
                gmmuFieldSetAperture(&pFmtPde->fldAperture, GMMU_APERTURE_ILWALID,
                                 pEntryData + i * m_SegmentInfo.entrySize);
            }
        }
        else
        {
            const GMMU_FMT_PDE* pFmtPde =
                gmmuFmtGetPde(m_pMmuLevel->GetGmmuFmt(),
                              m_SegmentInfo.pFmtLevel,
                              subLevelIdx);

            GMMU_APERTURE aperture = GMMU_APERTURE_VIDEO;
            if (valid)
            {
                MmuLevel *pBelowLevel = NULL;
                MmuLevelSegmentInfo info;
                CHECK_RC(m_pMmuLevel->GetLowerMmuLevel(subLevelIdx, &pBelowLevel));
                CHECK_RC(pBelowLevel->GetMmuLevelSegmentInfo(offset, &info));
                switch(info.location)
                {
                case Memory::Fb:
                    aperture = GMMU_APERTURE_VIDEO;
                    break;
                case Memory::Coherent:
                    aperture = GMMU_APERTURE_SYS_COH;
                    break;
                case Memory::NonCoherent:
                    aperture = GMMU_APERTURE_SYS_NONCOH;
                    break;
                default:
                    MASSERT(!"Unsupported location!\n");
                }
            }
            else
            {
                aperture = GMMU_APERTURE_ILWALID;
            }

            // Only utility functions understand the entry format;
            // Call utility functions to modify the saved location data.
            gmmuFieldSetAperture(&pFmtPde->fldAperture, aperture,
                                 pEntryData + i * m_SegmentInfo.entrySize);
        }
    }

    *pSize = realSize;
    return rc;
}


//--------------------------------------------------------------------
//! \brief Modify the volatile bit in saved PDEs/PTEs data of the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param sparse: 
//!   v2 true=>VOL_TRUE  false=>VOL_FALSE
//!   v3 true=>LW_MMU_VER3_PTE_PCF_SPARSE false=>skip
RC MmuLevelSegment::SetSegmentSparse
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool sparse
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    GetEntryLocalData(offset, *pSize, &realSize, &pageCnt);

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(m_pMmuLevel->GetGmmuFmt()->version < GMMU_FMT_VERSION_3);
#endif

    for (size_t i = 0; i < pageCnt; ++ i)
    {
        CHECK_RC(SetSegmentVolatile(offset, pSize, subLevelIdx, sparse)); 
    }

    return OK;
}
//--------------------------------------------------------------------
//! \brief Modify the volatile bit in saved PDEs/PTEs data of the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param vol: true=>VOL_TRUE  false=>VOL_FALSE
//!
RC MmuLevelSegment::SetSegmentVolatile
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool vol
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version < GMMU_FMT_VERSION_3);
#endif

    for (size_t i = 0; i < pageCnt; ++ i)
    {
        if (IsPteEntry(pEntryData + i * m_SegmentInfo.entrySize))
        {
            const GMMU_FMT_PTE* pFmtPte = m_pMmuLevel->GetGmmuFmt()->pPte;
            lwFieldSetBool(&pFmtPte->fldVolatile, vol, pEntryData +
                    i * m_SegmentInfo.entrySize);
        }
        else
        {
            const GMMU_FMT_PDE* pFmtPde =
                gmmuFmtGetPde(pFmt,
                        m_SegmentInfo.pFmtLevel,
                        subLevelIdx);
            lwFieldSetBool(&pFmtPde->fldVolatile, vol, pEntryData +
                    i * m_SegmentInfo.entrySize);
        }
    }

    *pSize = realSize;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Modify the atomic_disable bit in saved PDEs/PTEs data of the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param atomicDisable: true=>ATOMIC_DISABLE_TRUE  false=>ATOMIC_DISABLE_FALSE
//!
RC MmuLevelSegment::SetSegmentAtomicDisable
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool atomicDisable
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
    const GMMU_FMT_PTE* pFmtPte = pFmt->pPte;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version < GMMU_FMT_VERSION_3);
#endif

    if (IsFieldValid(MmuLevelSegment::PtePcfField::AtomicDisable))
    {
        for (size_t i = 0; i < pageCnt; ++ i)
        {
            lwFieldSetBool(&pFmtPte->fldAtomicDisable, atomicDisable,
                    pEntryData + i * m_SegmentInfo.entrySize);
        }
    }
    else
    {
        // HW does not support atomic disable.
        return RC::SOFTWARE_ERROR;
    }

    *pSize = realSize;
    return rc;
}


//--------------------------------------------------------------------
//! \brief Modify the priv bit in saved PDEs/PTEs data of the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param privEnable: true=>PRIV_TRUE  false=>PRIV_FALSE
//!
RC MmuLevelSegment::SetSegmentPriv
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool privEnable
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
    const GMMU_FMT_PTE* pFmtPte = pFmt->pPte;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version < GMMU_FMT_VERSION_3);
#endif

    for (size_t i = 0; i < pageCnt; ++ i)
    {
        lwFieldSetBool(&pFmtPte->fldPrivilege, privEnable,
                pEntryData + i * m_SegmentInfo.entrySize);
    }

    *pSize = realSize;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Modify the access_readonly bit in saved PDEs/PTEs data of the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param accessRO: true=>READ_ONLY_TRUE  false=>READ_ONLY_FALSE
//!
RC MmuLevelSegment::SetSegmentAccessRO
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool accessRO
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
    const GMMU_FMT_PTE* pFmtPte = pFmt->pPte;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version < GMMU_FMT_VERSION_3);
#endif

    for (size_t i = 0; i < pageCnt; ++ i)
    {
        lwFieldSetBool(&pFmtPte->fldReadOnly, accessRO,
                pEntryData + i * m_SegmentInfo.entrySize);
    }

    *pSize = realSize;
    return rc;
}
//--------------------------------------------------------------------
//! \brief Modify the PCF in saved PDEs/PTEs data of the entry
//!
//! \param lwrrentData: Entry that's going to modify
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param pteFields: a map of PtePcfField enums to bools.  Used to determine
//!                   how to construct the PCF for PTE data.
//! \param pdeFields: a map of PdePcfField enums to bools.  Used to determine
//!                   how to construct the PCF for PDE data.
//! \param fetchMode: an option to determine the source of initial pcf value 
//!
RC MmuLevelSegment::SetEntryPcf
(
    UINT08* lwrrentData, 
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    const map<PtePcfField, bool>& pteFields,
    const map<PdePcfField, bool>& pdeFields,
    FetchMode fetchMode
)
{
    RC rc;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(m_pMmuLevel->GetGmmuFmt()->version == GMMU_FMT_VERSION_3);
#endif
    
    if (IsPteEntry(lwrrentData))
    {
        CHECK_RC(UpdateAllPtePcf(lwrrentData, pteFields, fetchMode));
    }
    // PDE entry
    else
    {
        CHECK_RC(UpdateAllPdePcf(lwrrentData, pdeFields, subLevelIdx, fetchMode));
    }
    
    return OK;
}
//--------------------------------------------------------------------
//! \brief Modify the PCF in saved PDEs/PTEs data of the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param pteFields: a map of PtePcfField enums to bools.  Used to determine
//!                   how to construct the PCF for PTE data.
//! \param pdeFields: a map of PdePcfField enums to bools.  Used to determine
//!                   how to construct the PCF for PDE data.
//! \param fetchMode: an option to determine the source of initial pcf value 
//!
RC MmuLevelSegment::SetSegmentPcf
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    const map<PtePcfField, bool>& pteFields,
    const map<PdePcfField, bool>& pdeFields,
    FetchMode fetchMode
)
{
    RC rc;

    UINT64 pageCount = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize, &realSize, &pageCount);
    MASSERT(pEntryData);

    // For each page, set all of it's PCFs in one time according to the map.
    for (UINT64 i = 0; i < pageCount; ++i)
    {
        UINT08* lwrrentData = pEntryData + i * m_SegmentInfo.entrySize;
        CHECK_RC(SetEntryPcf(lwrrentData, subLevelIdx, pteFields, pdeFields, fetchMode));        
    }
    *pSize = realSize;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Modify the PA and aperture bit in saved PDEs/PTEs data of the segment
//!        Now only PTE supported because PDE allocation is static
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param physAddr: Base physAddress, must be contiguous
//! \param EntryAperture: aperture bound to the PA
RC MmuLevelSegment::SetContiguousPA
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    UINT64 physAddr,
    GMMU_APERTURE aperture
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    for (size_t i = 0; i < pageCnt; ++ i)
    {
        UINT64 entryPhysAddr = physAddr + i * m_SegmentInfo.levelPageSize;
        const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
        const GMMU_FMT_PTE* pFmtPte = pFmt->pPte;
        const GMMU_FIELD_APERTURE *pFmtAperture = &pFmtPte->fldAperture;
        const GMMU_FIELD_ADDRESS *pFmtFieldAddr = 0;

        switch(aperture)
        {
        case GMMU_APERTURE_VIDEO:
            pFmtFieldAddr = &pFmtPte->fldAddrVidmem;
            break;
        case GMMU_APERTURE_SYS_COH:
        case GMMU_APERTURE_SYS_NONCOH:
            pFmtFieldAddr = &pFmtPte->fldAddrSysmem;
            break;
        case GMMU_APERTURE_PEER:
            pFmtFieldAddr = &pFmtPte->fldAddrPeer;
            break;
        case GMMU_APERTURE_ILWALID:
            {
                const GMMU_FMT_PDE* pFmtPde =
                    gmmuFmtGetPde(m_pMmuLevel->GetGmmuFmt(),
                                  m_SegmentInfo.pFmtLevel,
                                  subLevelIdx);
                pFmtAperture = &pFmtPde->fldAperture;
#if LWCFG(GLOBAL_ARCH_HOPPER)
                if (pFmt->version == GMMU_FMT_VERSION_3)
                {
                    // For v3 mmu, RM just init fldAddr
                    pFmtFieldAddr = &pFmtPde->fldAddr;
                }
                else
#endif
                {
                    pFmtFieldAddr = &pFmtPde->fldAddrVidmem;
                }
            }
            break;
        default:
            MASSERT(!"Unsupported aperture!\n");
        }

        gmmuFieldSetAddress(pFmtFieldAddr, entryPhysAddr,
            pEntryData + i * m_SegmentInfo.entrySize);
        gmmuFieldSetAperture(pFmtAperture, aperture,
            pEntryData + i * m_SegmentInfo.entrySize);
    }

    *pSize = realSize;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Modify the valid (bit0) in saved PDEs/PTEs data of the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param bToPte: true: change to PTE - LW_MMU_NEW_PDE_IS_PTE_TRUE(1)
//!                false: change to PDE - LW_MMU_NEW_PDE_IS_PDE_TRUE(0)
//!
RC MmuLevelSegment::ChangePageEntryType
(
    UINT64 offset,
    UINT64 *pSize,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool bToPte
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    for (size_t i = 0; i < pageCnt; ++ i)
    {
        //
        // Bug 1611401:
        // PTE bits may overlap with PDE bits. For example,
        //   #define LW_MMU_NEW_PTE_ENCRYPTED                              4:4 /* RWXVF */
        //   #define LW_MMU_NEW_PTE_PRIVILEGE                              5:5 /* RWXVF */
        //   #define LW_MMU_VER2_DUAL_PDE_ADDRESS_BIG_SYS                  53:(8-4) /* RWXVF */
        // bit4~5 means address for PDE. While colwerting the entry to PTE, they should be
        // cleared because they mean totally different things.
        //
        size_t memsetSize =  m_SegmentInfo.entrySize;
        size_t memsetOffset = i * m_SegmentInfo.entrySize;

        // PDE0/PTE_2M => Dual_PDE => 16 bytes
        // Colwert to PTE: clear all 16 bytes; bit64 ~ bit127 useless
        // colwert to PDE: BIG => clear bit0~bit63; SMALL => clear bit64 ~ bit127
        if (!bToPte &&
            m_pMmuLevel->GetLevelId() == MmuLevelTree::GMMU_LEVEL_PTE_2M)
        {
            memsetSize = m_SegmentInfo.entrySize / 2; // half size of Dual PDE
            if (subLevelIdx == MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_SMALL)
            {
                memsetOffset += memsetSize;           // add BIG offset
            }
        }

        // Clear the entry to avoid dirty bits
        memset(pEntryData + memsetOffset, 0, memsetSize);

        // PDE shares the first bit with PTE to identify whether it's PTE of PDE.
        //
        const GMMU_FMT_PTE* pFmtPte = m_pMmuLevel->GetGmmuFmt()->pPte;
        lwFieldSetBool(&pFmtPte->fldValid, bToPte, pEntryData +
                       i * m_SegmentInfo.entrySize);
    }

    *pSize = realSize;
    return rc;
}

//--------------------------------------------------------------------
//! \brief A wrap function of LW0080_CTRL_DMA_FILL_PTE_MEM
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param hMem/MemOffset/PteFlags/PhysAddr:
//!        Please reference LW0080_CTRL_DMA_FILL_PTE_MEM for details
//!        Note: PhysAddr is the start PA of the range; must be contiguous
//!
RC MmuLevelSegment::FillPteMem
(
    UINT64 offset,
    UINT64 *pSize,
    LwRm::Handle hMem,
    UINT64 MemOffset,
    UINT32 PteFlags,
    UINT64 PhysAddr,
    LwRm::Handle hSrcVASpace,
    LwRm::Handle hTgtVASpace
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT64 levelPageSize = m_SegmentInfo.levelPageSize;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    // Call RM API LW0080_CTRL_DMA_FILL_PTE_MEM
    //
    // Note: This RM API is fading out. We'll have our own PTE modification function
    //       in the future.
    //
    LwRm* pLwRm = m_pMmuLevel->GetSurface()->GetLwRmPtr();
    LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS params = {0};
    UINT64 pageArray = PhysAddr & ~(levelPageSize - 1);
    GpuDevice *pGpuDevice = m_pMmuLevel->GetGpuDevice();
    params.pageCount = static_cast<UINT32>(pageCnt);
    params.hwResource.hClient = pLwRm->GetClientHandle();
    params.hwResource.hDevice = pLwRm->GetDeviceHandle(pGpuDevice);
    params.hwResource.hMemory = hMem;
    params.offset = MemOffset;
    params.gpuAddr = m_pMmuLevel->GetMmuLevelTree()->GetBaseVirtAddr() + offset;
    params.pageArray = LW_PTR_TO_LwP64(&pageArray);
    params.hSrcVASpace = hSrcVASpace;
    params.hTgtVASpace = hTgtVASpace;
    params.pageSize = (LwU32)levelPageSize;
    params.flags = PteFlags;
    params.pteMem = (LwP64)pEntryData;

    MASSERT(pageCnt * m_SegmentInfo.entrySize <= m_MmuEntriesData.size() -
        ((offset - m_SegmentInfo.surfOffset) / levelPageSize) * m_SegmentInfo.entrySize);

    CHECK_RC(pLwRm->ControlByDevice(pGpuDevice,
        LW0080_CTRL_CMD_DMA_FILL_PTE_MEM,
        &params, sizeof(params)));

    *pSize = realSize;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Get saved PDEs/PTEs data in the segment
//!
//! \param offset: Surface offset
//! \param pSize: [In] requested surface size; [Out] real surface size used
//! \param pData: [Out] returned PDEs/PTEs data
//!
RC MmuLevelSegment::GetMmuEntriesData
(
    UINT64 offset,
    UINT64 *pSize,
    vector<UINT08> *pData
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT08* pEntryData = GetEntryLocalData(offset, *pSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);

    MASSERT(pData);
    size_t sizeInBytes = static_cast<size_t>(pageCnt * m_SegmentInfo.entrySize);
    pData->clear(); // to output modified data
    pData->resize(sizeInBytes);
    UINT08* pOutput = &((*pData)[0]);

    memcpy(pOutput, pEntryData, sizeInBytes);

    *pSize = realSize;
    return rc;
}

//--------------------------------------------------------------------
//! \brief Update saved entry data in specified range
//!
//! \param offset: Surface offset
//! \param pSize:  [In] requested surface size; [Out] real surface size used
//! \param pData:  NULL - updated data comes from hw; non-NULL: given updated data
//!
RC MmuLevelSegment::UpdateLocalEntriesData
(
    UINT64 offset,
    UINT64 *pSize,
    vector<UINT08> *pData
)
{
    RC rc;

    UINT64 realSize =
        min(m_SegmentInfo.surfOffset + m_SegmentInfo.surfSegmentSize - offset,
            *pSize);
    UINT64 levelPageSize = m_SegmentInfo.levelPageSize;
    UINT32 entryIndex = static_cast<UINT32>
        ((offset - m_SegmentInfo.surfOffset) / levelPageSize);

    UINT64 pageCount = (realSize + levelPageSize - 1) / levelPageSize;
    UINT64 dataSizeInDword = pageCount * m_SegmentInfo.entrySize / sizeof(UINT32);

    if (m_MmuEntriesData.size() == 0)
    {
        if (realSize == m_SegmentInfo.surfSegmentSize)
        {
            // Update all data in the segment
            // Just allocate the size for the following code
            m_MmuEntriesData.resize((size_t)(pageCount * m_SegmentInfo.entrySize));
        }
        else
        {
            // Update part of data entries in the segment
            // Read whole segment data from hw first
            CHECK_RC(UpdateLocalEntriesData(m_SegmentInfo.surfOffset,
                &m_SegmentInfo.surfSegmentSize, NULL));
        }
    }

    CHECK_RC(MapCpuPointer());

    MASSERT(m_MmuEntriesData.size()/m_SegmentInfo.entrySize > entryIndex);
    UINT32 *pDataAddress = reinterpret_cast<UINT32*>
        (&m_MmuEntriesData[entryIndex*m_SegmentInfo.entrySize]);

    if (NULL == pData)
    {
        //
        // Update data from hw
        UINT32 *pSrcAddress = reinterpret_cast<UINT32*>
            (m_SegmentInfo.pCpuAddress + entryIndex * m_SegmentInfo.entrySize);

        for (UINT32 i = 0; i < dataSizeInDword; ++ i)
        {
            pDataAddress[i] = Platform::VirtualRd32(pSrcAddress+i);
        }
    }
    else
    {
        //
        // Update data from given data
        // It's not allowed to pass in less data
        MASSERT(pData->size() >= dataSizeInDword * sizeof(UINT32));
        UINT32 *pSrcAddress = reinterpret_cast<UINT32*>(&((*pData)[0]));

        memcpy(pDataAddress, pSrcAddress, (size_t)(dataSizeInDword * sizeof(UINT32)));

        // erase consumed src data for next segment
        pData->erase(pData->begin(),
            pData->begin() + (size_t)(dataSizeInDword * sizeof(UINT32)));
    }

    *pSize = realSize;
    return rc;
}

//--------------------------------------------------------------------
//! \brief download saved entry data in specified range to hw
//!
//! \param offset: Surface offset
//! \param pSize:  [In] requested surface size; [Out] real surface size used
//!
RC MmuLevelSegment::WriteMmuEntries
(
    UINT64 offset,
    UINT64 *pSize,
    Channel *pInbandChannel,
    UINT32 subchNum
)
{
    RC rc;

    vector<UINT08> entryData;
    UINT64 size = *pSize;
    CHECK_RC(GetMmuEntriesData(offset, &size, &entryData));

    UINT64 levelPageSize = m_SegmentInfo.levelPageSize;
    UINT32 entryIndex = static_cast<UINT32>
        ((offset - m_SegmentInfo.surfOffset) / levelPageSize);

    const UINT32 *pDataAddress = reinterpret_cast<const UINT32*>(&(entryData[0]));

    if (pInbandChannel)
    {
        UINT64 physAddress = m_SegmentInfo.physAddress +
            entryIndex * m_SegmentInfo.entrySize;

        CHECK_RC(WriteMmuEntriesInband(pInbandChannel, subchNum,
            physAddress, m_SegmentInfo.location, entryData));
    }
    else
    {
        UINT32 *pCpuAddress = reinterpret_cast<UINT32*>(
            m_SegmentInfo.pCpuAddress + entryIndex * m_SegmentInfo.entrySize);

        for (UINT32 i = 0; i < entryData.size() / sizeof(UINT32); ++ i)
        {
            Platform::VirtualWr32(pCpuAddress+i, pDataAddress[i]);
        }

        Platform::FlushCpuWriteCombineBuffer();
    }

    *pSize = size; // return real size comparing to requested size
    return rc;
}

bool MmuLevelSegment::IsPteValid
(
    const UINT08 * pEntryData
)
{
    MASSERT(pEntryData);
    
    if (IsPteEntry(pEntryData))
    {
        const GMMU_FMT_PTE* pFmtPte = m_pMmuLevel->GetGmmuFmt()->pPte;

        return lwFieldGetBool(&pFmtPte->fldValid, pEntryData) == LW_TRUE;
    }

    return false;
}

bool MmuLevelSegment::IsEntryValid
(
    UINT64 offset,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool *isPte
)
{
    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT64 levelPageSize = m_SegmentInfo.levelPageSize;
    UINT08* pEntryData = GetEntryLocalData(offset, levelPageSize,
                                           &realSize, &pageCnt);
    MASSERT(pEntryData);
    MASSERT(pageCnt == 1);

    if (!m_pMmuLevel->GetGmmuFmt())
    {
        // mmu level APIs are not enabled
        *isPte = true;
        const UINT32* pDataInDwords = reinterpret_cast<const UINT32*>(pEntryData);
        return FLD_TEST_DRF(_MMU, _PTE, _VALID, _TRUE,
                            pDataInDwords[(0?LW_MMU_PTE_VALID)/32]);
    }

    if (IsPteEntry(pEntryData))
    {
        const GMMU_FMT_PTE* pFmtPte = m_pMmuLevel->GetGmmuFmt()->pPte;

        *isPte = true;
        return lwFieldGetBool(&pFmtPte->fldValid, pEntryData) == LW_TRUE;
    }
    else
    {
        const GMMU_FMT_PDE* pFmtPde =
            gmmuFmtGetPde(m_pMmuLevel->GetGmmuFmt(),
                          m_SegmentInfo.pFmtLevel,
                          subLevelIdx);

        GMMU_APERTURE pdeAperture = gmmuFieldGetAperture(
            &pFmtPde->fldAperture, pEntryData);

        *isPte = false;
        return pdeAperture != GMMU_APERTURE_ILWALID;
    }

    MASSERT(!"Should NOT hit here");
    return false;
}

RC MmuLevelSegment::GetEntryPhysAddr
(
    UINT64 offset,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    UINT64 *pPhysAddr,
    GMMU_APERTURE *pAperture
)
{
    RC rc;

    UINT64 pageCnt = 0;
    UINT64 realSize = 0;
    UINT64 levelPageSize = m_SegmentInfo.levelPageSize;
    const UINT08* pEntryData = GetEntryLocalData(offset, levelPageSize,
                                                &realSize, &pageCnt);
    UINT64 base = 0;
    MASSERT(pEntryData);
    MASSERT(pageCnt == 1);

    // Don't forget to specify subdevice id when ready in MmuLevelSegment class.
    CHECK_RC(PolicyManager::Instance()->GetSysMemBaseAddrPerDev(m_pMmuLevel->GetGpuDevice(), &base));

    if (!m_pMmuLevel->GetGmmuFmt())
    {
        // mmu level APIs are not enabled
        const UINT32* pDataInDwords = reinterpret_cast<const UINT32*>(pEntryData);
        switch (DRF_VAL(_MMU, _PTE, _APERTURE,
                        pDataInDwords[(0?LW_MMU_PTE_APERTURE)/32]))
        {
        case LW_MMU_PTE_APERTURE_VIDEO_MEMORY:
            *pAperture = GMMU_APERTURE_VIDEO;
            *pPhysAddr = DRF_VAL(_MMU, _PTE, _ADDRESS_VID,
                         pDataInDwords[(0?LW_MMU_PTE_ADDRESS_VID)/32]);
            break;
        case LW_MMU_PTE_APERTURE_PEER_MEMORY:
            *pAperture = GMMU_APERTURE_PEER;
            *pPhysAddr = DRF_VAL(_MMU, _PTE, _ADDRESS_VID,
                         pDataInDwords[(0?LW_MMU_PTE_ADDRESS_VID)/32]);
            break;
        case LW_MMU_PTE_APERTURE_SYSTEM_COHERENT_MEMORY:
            *pAperture = GMMU_APERTURE_SYS_NONCOH;
            *pPhysAddr = DRF_VAL(_MMU, _PTE, _ADDRESS_SYS,
                    pDataInDwords[(0?LW_MMU_PTE_ADDRESS_SYS)/32]);
            break;
        default:
            MASSERT(!"Should NOT hit here");
            return RC::SOFTWARE_ERROR;
        }

        *pPhysAddr <<= LW_MMU_PTE_ADDRESS_SHIFT;
        *pPhysAddr += base;
        return rc;
    }

    GMMU_APERTURE aperture = GMMU_APERTURE_ILWALID;
    const GMMU_FIELD_ADDRESS *pFldAddr = NULL;
    if (IsPteEntry(pEntryData))
    {
        const GMMU_FMT_PTE* pFmtPte = m_pMmuLevel->GetGmmuFmt()->pPte;
        aperture = gmmuFieldGetAperture(&pFmtPte->fldAperture,
                pEntryData);
        pFldAddr =gmmuFmtPtePhysAddrFld(pFmtPte, aperture);
    }
    else
    {
        const GMMU_FMT_PDE* pFmtPde =
            gmmuFmtGetPde(m_pMmuLevel->GetGmmuFmt(),
                    m_SegmentInfo.pFmtLevel,
                    subLevelIdx);

        aperture = gmmuFieldGetAperture(&pFmtPde->fldAperture,
                                        pEntryData);
        pFldAddr = gmmuFmtPdePhysAddrFld(pFmtPde, aperture);
    }

    *pPhysAddr = gmmuFieldGetAddress(pFldAddr, pEntryData);
    *pPhysAddr += base;
    *pAperture = aperture;

    return rc;
}

RC MmuLevelSegment::WriteMmuEntriesInband
(
    Channel *pInbandChannel,
    UINT32 subchNum,
    UINT64 ptePhysAddr,
    Memory::Location location,
    const vector<UINT08>& pteBytes
) const
{
    RC rc;
    GpuDevice *pGpuDevice = m_pMmuLevel->GetGpuDevice();

    PolicyManager *pPolicyManager = PolicyManager::Instance();
    PolicyManager::PreAllocSurface * pPreAllocSurface = pPolicyManager->GetPreAllocSurface(pInbandChannel->GetRmClient());
    CHECK_RC(pPreAllocSurface->AllocSmallChunk(pteBytes.size()));

    for (UINT32 ii = 0; ii < pGpuDevice->GetNumSubdevices(); ++ii)
    {
        CHECK_RC(pPreAllocSurface->WritePreAllocSurface(&pteBytes[0],
                                                        pteBytes.size(), ii,
                                                        pInbandChannel));
    }

    UINT32 srcPhysMode = 0;
    UINT64 srcPhysAddr = pPreAllocSurface->GetPhysAddress();
    Memory::Location srcLocation = pPreAllocSurface->GetLocation();
    switch(srcLocation)
    {
    case Memory::Fb:
        srcPhysMode = DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _LOCAL_FB);
        break;

    case Memory::Coherent:
        srcPhysMode = DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _COHERENT_SYSMEM);
        break;

    case Memory::NonCoherent:
        srcPhysMode = DRF_DEF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM);
        break;

    default:
        MASSERT(!"Unsupported location!\n");
    }

    UINT32 dstPhysMode = 0;
    switch(location)
    {
    case Memory::Fb:
        dstPhysMode = DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB);
        break;

    case Memory::Coherent:
        dstPhysMode = DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _COHERENT_SYSMEM);
        break;

    case Memory::NonCoherent:
        dstPhysMode = DRF_DEF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM);
        break;

    default:
        MASSERT(!"Unsupported location!\n");
    }

    CHECK_RC(pInbandChannel->BeginInsertedMethods());
    Utility::CleanupOnReturn<Channel> cleanupInsertedMethods(
                        pInbandChannel, &Channel::CancelInsertedMethods);

    pInbandChannel->Write(subchNum, LWA0B5_PITCH_IN, 0);
    pInbandChannel->Write(subchNum, LWA0B5_PITCH_OUT, 0);
    pInbandChannel->Write(subchNum, LWA0B5_OFFSET_IN_UPPER,
        static_cast<UINT32>(srcPhysAddr >> 32));
    pInbandChannel->Write(subchNum, LWA0B5_OFFSET_IN_LOWER,
        static_cast<UINT32>(srcPhysAddr & 0xFFFFFFFF));
    pInbandChannel->Write(subchNum, LWA0B5_SET_SRC_PHYS_MODE, srcPhysMode);
    pInbandChannel->Write(subchNum, LWA0B5_OFFSET_OUT_UPPER,
        static_cast<UINT32>(ptePhysAddr >> 32));
    pInbandChannel->Write(subchNum, LWA0B5_OFFSET_OUT_LOWER,
        static_cast<UINT32>(ptePhysAddr & 0xFFFFFFFF));
    pInbandChannel->Write(subchNum, LWA0B5_SET_DST_PHYS_MODE, dstPhysMode);
    pInbandChannel->Write(subchNum, LWA0B5_LINE_LENGTH_IN, (UINT32)pteBytes.size());
    pInbandChannel->Write(subchNum, LWA0B5_LINE_COUNT, 1);

    UINT32 data = 0;

    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _PIPELINED, data);
    if(PolicyManager::Instance()->GetInbandCELaunchFlushENable())
    {
        data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE, data);
    }
    else
    {
        data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _FALSE, data);
    }
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DST_TYPE, _PHYSICAL, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _PHYSICAL, data);

    pInbandChannel->Write(subchNum, LWA0B5_LAUNCH_DMA, data);

    cleanupInsertedMethods.Cancel();
    CHECK_RC(pInbandChannel->EndInsertedMethods());

    return OK;
}

bool MmuLevelSegment::IsPteEntry(const UINT08 *pEntry)
{
    const struct GMMU_FMT *pGmmuFmt = m_pMmuLevel->GetGmmuFmt();
    bool isPte = false;
    if (pGmmuFmt)
    {
        //
        // For PTE_2M/PDE0, gmmuFmtEntryIsPte() can not be trusted.
        isPte =
            LW_TRUE == gmmuFmtEntryIsPte(pGmmuFmt, m_SegmentInfo.pFmtLevel, pEntry);

        if (isPte ||
            (m_pMmuLevel->GetLevelId() != MmuLevelTree::GMMU_LEVEL_PTE_2M &&
             m_pMmuLevel->GetLevelId() != MmuLevelTree::GMMU_LEVEL_PTE_512M))
        {
            return isPte;
        }
    }

    // it's still can be PTE even valid bit is false - a unmapped 2M / 512M page
    // Check active pagesize again
    // User case - UVM 
    // 1. force 2m fault via unmap 2M page 
    // 2. on the replayable fault handle flow, Mods change page size to split the 2m to 64k
    // 3. At the change page size flow, Mods need to get the 2m physical address via the PTE inforamtion
    // User case - Volta super page - 512M PTE
    // 512M can be a PTE even if the valid bit is false

    UINT64 pageSize = m_pMmuLevel->GetMmuLevelTree()->GetActivePageSize();
    if (pageSize == GpuDevice::PAGESIZE_2MB ||
            pageSize == GpuDevice::PAGESIZE_512MB)
    {
        if (!isPte && (m_SegmentInfo.levelPageSize == pageSize ||
                    m_SegmentInfo.levelPageSize == pageSize))
        {
            return true;
        }
    }

    return false;
}

UINT08* MmuLevelSegment::GetEntryLocalData
(
    UINT64 offset,
    UINT64 size,
    UINT64 *pRealSize,
    UINT64 *pPageCnt
)
{
    if (m_MmuEntriesData.size() == 0)
    {
        // Read init data from hw
        if(OK != UpdateLocalEntriesData(m_SegmentInfo.surfOffset,
                                       &m_SegmentInfo.surfSegmentSize, NULL))
        {
            return NULL;
        }
    }

    UINT64 realSize =
        min(m_SegmentInfo.surfOffset + m_SegmentInfo.surfSegmentSize - offset,
            size);
    UINT64 levelPageSize = m_SegmentInfo.levelPageSize;
    UINT64 pageCnt = (realSize + levelPageSize - 1) / levelPageSize;

    // surfOffset is index 0 in save entry list
    MASSERT(offset >= m_SegmentInfo.surfOffset); // otherwise it's in wrong segment
    UINT32 entryIndex = static_cast<UINT32>
        ((offset - m_SegmentInfo.surfOffset) / levelPageSize);

    *pRealSize = realSize;
    *pPageCnt = pageCnt;
    MASSERT(m_MmuEntriesData.size()/m_SegmentInfo.entrySize > entryIndex);
    return &(m_MmuEntriesData[entryIndex*m_SegmentInfo.entrySize]);
}

#define TO_FIELD(pGmmuFmt, field) &(pGmmuFmt->pPte)->fld##field
bool MmuLevelSegment::GetFieldBool
(
    PtePcfField ptePcfField, 
    const UINT08 * pEntry
)
{
    auto getFieldBoolV2 = [this, pEntry](auto field)->bool 
    {
        return lwFieldGetBool(field, &pEntry[0]);
    };

#if LWCFG(GLOBAL_ARCH_HOPPER)
    auto getFieldBoolV3 = [this, pEntry](auto field)->bool
    {
        bool value = false;                                       
        this->GetBoolFromPtePcf(pEntry,                                   
                field, &value);     
        return value;
    };
#endif

    const GMMU_FMT* pGmmuFmt = m_pMmuLevel->GetGmmuFmt();
    switch(ptePcfField)
    {
    case PtePcfField::ReadOnly:
#if LWCFG(GLOBAL_ARCH_HOPPER)
        if (pGmmuFmt->version == GMMU_FMT_VERSION_3)
        {
            return getFieldBoolV3(ptePcfField);
        }
        else
#endif
        {
            return getFieldBoolV2(TO_FIELD(pGmmuFmt, ReadOnly));
        }
        break;
    case PtePcfField::AtomicDisable:
#if LWCFG(GLOBAL_ARCH_HOPPER)
        if (pGmmuFmt->version == GMMU_FMT_VERSION_3)
        {
            return getFieldBoolV3(ptePcfField);
        }
        else
#endif
        {
            return getFieldBoolV2(TO_FIELD(pGmmuFmt, AtomicDisable));
        }
        break;
    case PtePcfField::Volatile:
#if LWCFG(GLOBAL_ARCH_HOPPER)
        if (pGmmuFmt->version == GMMU_FMT_VERSION_3)
        {
            return getFieldBoolV3(ptePcfField);
        }
        else
#endif
        {
            return getFieldBoolV2(TO_FIELD(pGmmuFmt, Volatile));
        }
        break;
    case PtePcfField::Privilege:
#if LWCFG(GLOBAL_ARCH_HOPPER)
        if (pGmmuFmt->version == GMMU_FMT_VERSION_3)
        {
            return getFieldBoolV3(ptePcfField);
        }
        else
#endif
        {
            return getFieldBoolV2(TO_FIELD(pGmmuFmt, Privilege));
        }
        break;
    case PtePcfField::Sparse:
#if LWCFG(GLOBAL_ARCH_HOPPER)
        if (pGmmuFmt->version == GMMU_FMT_VERSION_3)                  
        {                                                           
            return getFieldBoolV3(ptePcfField);
        }                                                             
        else                                                          
#endif
        {                                                           
            return getFieldBoolV2(TO_FIELD(pGmmuFmt, Volatile));
        }                                                                         
        break;
    default:
        MASSERT(!"Not support field");
        return false;
    }

}

bool MmuLevelSegment::GetFieldBool
(
    PdePcfField pdePcfField, 
    const UINT08 * pEntry,
    MmuLevel::MmuSubLevelIndex subLevelIdx
)
{
    const GMMU_FMT* pGmmuFmt = m_pMmuLevel->GetGmmuFmt();
    const GMMU_FMT_PDE* pFmtPde =
        gmmuFmtGetPde(pGmmuFmt,
                m_SegmentInfo.pFmtLevel,
                subLevelIdx);
#if LWCFG(GLOBAL_ARCH_HOPPER)
    bool value = false;
#endif

    switch(pdePcfField)
    {
    case PdePcfField::Volatile:
#if LWCFG(GLOBAL_ARCH_HOPPER)
        if (pGmmuFmt->version == GMMU_FMT_VERSION_3)
        {
            GetBoolFromPdePcf(pEntry, 
                    MmuLevelSegment::PdePcfField::Volatile, subLevelIdx,
                    &value);
            return value;
        }
        else
#endif
        {
            return lwFieldGetBool(&pFmtPde->fldVolatile, &pEntry[0]);       
        }
        break;
    case PdePcfField::AtsAllow:
#if LWCFG(GLOBAL_ARCH_HOPPER)
        if (pGmmuFmt->version == GMMU_FMT_VERSION_3)                  
        {
            // Only support at the v3
            // ToDo: AtsAllow not support at the RM now
            // GET_FIELD_BOOL(pGmmuFmt, pFmtPde, AtsAllow, pEntry, subLevelIdx);
        }
#endif
        return false;
        break;
    case PdePcfField::Sparse:
#if LWCFG(GLOBAL_ARCH_HOPPER)
        if (pGmmuFmt->version == GMMU_FMT_VERSION_3)                  
        {                                                           
            GetBoolFromPdePcf(pEntry,                         
                    MmuLevelSegment::PdePcfField::Sparse, subLevelIdx,
                    &value);
            return value;                                             
        }                                                             
        else                                                          
#endif
        {                                                           
            return lwFieldGetBool(&pFmtPde->fldVolatile, &pEntry[0]);       
        }                                                                         
        break;
    default:
        MASSERT("Not support field");
        return false;
    }
}
 
// In MMU V3: Field bit which in pcf don't support desc valid check
#define TO_FIELD_DESC(pGmmuFmt, field) &(pGmmuFmt->pPte)->fld##field.desc
bool MmuLevelSegment::IsFieldValid
(
    PtePcfField ptePcfField 
)
{
    auto isFieldValid = [this](auto field)->bool 
    {
#if LWCFG(GLOBAL_ARCH_HOPPER)
        const GMMU_FMT* pGmmuFmt = m_pMmuLevel->GetGmmuFmt();
        if (pGmmuFmt->version == GMMU_FMT_VERSION_3)
        {
            return true;
        }
        else
#endif
        {
            return lwFieldIsValid32(field);
        }
    };

    const GMMU_FMT* pGmmuFmt = m_pMmuLevel->GetGmmuFmt();
    switch(ptePcfField)
    {
    case PtePcfField::ReadOnly:
        return isFieldValid(TO_FIELD_DESC(pGmmuFmt, ReadOnly));
        break;
    case PtePcfField::AtomicDisable:
        return isFieldValid(TO_FIELD_DESC(pGmmuFmt, AtomicDisable));
        break;
    case PtePcfField::Volatile:
        return isFieldValid(TO_FIELD_DESC(pGmmuFmt, Volatile));
        break;
    case PtePcfField::Privilege:
        return isFieldValid(TO_FIELD_DESC(pGmmuFmt, Privilege));
        break;
    default:
        MASSERT(!"Not support field");
        return false;
    }
}

RC MmuLevelSegment::GetBoolFromPtePcf
(
    const UINT08 * pEntryData,
    PtePcfField ptePcfField,
    bool * pPtePcfValue
)
{
    RC rc;
    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
    UINT32 ptePcfSw = 0;
    UINT32 ptePcfHw = 0;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version == GMMU_FMT_VERSION_3);
#endif

    // 1. Decode current cache hw pcf value
    ptePcfHw = lwFieldGet32(&pFmt->pPte->fldPtePcf, 
            pEntryData);

    // 2. Read back the sw pcf from the hw pcf
    rc = MmuUtils::GmmuTranslatePtePcfFromHw(
                ptePcfHw,
                IsPteValid(pEntryData),
                &ptePcfSw);


    // 3. Get the given bit from the pcf
    switch(ptePcfField)
    {
    case PtePcfField::ReadOnly:
        *pPtePcfValue = ptePcfSw & (1 << SW_MMU_PCF_RO_IDX);
        break;
    case PtePcfField::AtomicDisable:
        *pPtePcfValue = ptePcfSw & (1 << SW_MMU_PCF_NOATOMIC_IDX);
        break;
    case PtePcfField::Sparse: 
        // Current 0x00000004 is only used by Spares. 
        // So the index SW_MMU_PCF_SPARSE_IDX = 1 can deduce it is sparse
        *pPtePcfValue = ptePcfSw & (1 << SW_MMU_PCF_SPARSE_IDX);
        break;
    case PtePcfField::Invalid:
        *pPtePcfValue = ptePcfSw & (1 << SW_MMU_PCF_ILWALID_IDX);
        break;
    case PtePcfField::Lw4K:
        // Current 0x00000002 is only used by 4K. 
        // So the index SW_MMU_PCF_LW4K_IDX = 1 can deduce it is 4k
        *pPtePcfValue = ptePcfSw & (1 << SW_MMU_PCF_LW4K_IDX);
        break;
    case PtePcfField::NoMapping:
        // Current 0x00000008 is only used by No Map. 
        // So the index SW_MMU_PCF_NOMAPPING_IDX = 1 can deduce it is No Map
        *pPtePcfValue = ptePcfSw & (1 << SW_MMU_PCF_NOMAPPING_IDX);
        break;
    case PtePcfField::ACE:
        *pPtePcfValue = ptePcfSw & (1 << SW_MMU_PCF_ACE_IDX);
        break;
    case PtePcfField::Volatile:
        *pPtePcfValue = ptePcfSw & (1 << SW_MMU_PCF_UNCACHED_IDX);
        break;
    case PtePcfField::Privilege:
        // Regular is the opposite of Priv. 
        *pPtePcfValue = !(ptePcfSw & (1 << SW_MMU_PCF_REGULAR_IDX));
        break;
    default:
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC MmuLevelSegment::GetBoolFromPdePcf
(
    const UINT08 * pEntryData,
    PdePcfField pdePcfField,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool * pPdePcfValue
)
{
    RC rc;
    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
    const GMMU_FMT_PDE* pFmtPde = gmmuFmtGetPde(pFmt,
                                                m_SegmentInfo.pFmtLevel,
                                                subLevelIdx);
    UINT32 pdePcfSw = 0;
    UINT32 pdePcfHw = 0;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version == GMMU_FMT_VERSION_3);
#endif

    // 1. Decode current cache hw pcf value
    pdePcfHw = lwFieldGet32(&pFmtPde->fldPdePcf, 
            pEntryData);
    
    GMMU_APERTURE aperture = gmmuFieldGetAperture(&pFmtPde->fldAperture, pEntryData);

    // 2. Read back the sw pcf from the hw pcf
    rc = MmuUtils::GmmuTranslatePdePcfFromHw(
                pdePcfHw,
                aperture,
                &pdePcfSw);

    // 3. Get the given bit from the pcf
    switch(pdePcfField)
    {
    case PdePcfField::Sparse: 
        // Current 0x00000204/0x00000004 is only used by Spares. 
        // So the index SW_MMU_PCF_SPARSE_IDX = 1 can deduce it is sparse
        *pPdePcfValue = pdePcfSw & (1 << SW_MMU_PCF_SPARSE_IDX);
        break;
    case PdePcfField::Invalid:
        *pPdePcfValue = pdePcfSw & (1 << SW_MMU_PCF_ILWALID_IDX);
        break;
    case PdePcfField::Volatile:
        *pPdePcfValue = pdePcfSw & (1 << SW_MMU_PCF_UNCACHED_IDX);
        break;
    case PdePcfField::AtsAllow:
        *pPdePcfValue = pdePcfSw & (1 << SW_MMU_PCF_ATS_ALLOWED_IDX);
        break;
    default:
        return RC::SOFTWARE_ERROR;
    }

    return OK;
}

#define FIELD_SET_CLEAR(ptePcfSw, isSet, field)      \
    do{                                              \
          if (isSet)                                 \
          {                                          \
              ptePcfSw |= 1 << SW_MMU_PCF_##field##_IDX; \
          }                                          \
          else                                       \
          {                                          \
              ptePcfSw &= (~(1 << SW_MMU_PCF_##field##_IDX)); \
          }                                          \
    }while(0);

//--------------------------------------------------------------------
//! \brief Batch modify PCF of PTE entry for version 3
//!
//! \param pEntryData: Entry that's going to modify
//! \param pteFields: a map of PtePcfField enums to bools.  Used to determine
//!                   how to construct the PCF for PTE data.
//! \param fetchMode: an option to determine the source of initial pcf value 
//!
RC MmuLevelSegment::UpdateAllPtePcf
(
    UINT08* pEntryData,
    const map<PtePcfField, bool>& pteFields,
    FetchMode fetchMode
)
{
    RC rc;
    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
    UINT32 swPtePcf = 0;
    UINT32 hwPtePcf = 0;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version == GMMU_FMT_VERSION_3);
#endif

    if (fetchMode == FetchMode::FetchCache)
    {
        // 1. Decode current cache hw pcf value
        hwPtePcf = lwFieldGet32(&pFmt->pPte->fldPtePcf, 
        pEntryData);

        // 2. Read back the sw pcf from the hw pcf
        CHECK_RC(MmuUtils::GmmuTranslatePtePcfFromHw(
                    hwPtePcf,
                    IsPteValid(pEntryData),
                    &swPtePcf));
    }
    
    // 3. Modify it
    for (auto& iter : pteFields)
    {
        // Volatile bit represent CACHE and only legal to set when VALID.
        if (iter.first == MmuLevelSegment::PtePcfField::Volatile 
            && !IsPteValid(pEntryData))
        {
            MASSERT(!"Setting CACHE bit to an INVALID entry.\n");
        }
        SetSwPtePcfField(&swPtePcf, iter.first, iter.second);
    }

    // 4. Remove SW PTE PCF bits that don't apply based on how
    //    the PTE's valid bit is set.
    swPtePcf = MmuUtils::TrimSwPtePcf(swPtePcf, IsPteValid(pEntryData));

    // 5. Update the sw pcf into hw pcf
    CHECK_RC(MmuUtils::GmmuTranslatePtePcfFromSw(swPtePcf, &hwPtePcf));
    // 6. RM new API to set the pcf
    lwFieldSet32(&pFmt->pPte->fldPtePcf, hwPtePcf, pEntryData);

    return OK;
}

RC MmuLevelSegment::UpdatePtePcf
(
    UINT08 * pEntryData,
    PtePcfField ptePcfField,
    bool ptePcfValue
)
{
    RC rc;
    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
    UINT32 ptePcfSw = 0;
    UINT32 ptePcfHw = 0;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version == GMMU_FMT_VERSION_3);
#endif

    // 1. Decode current cache hw pcf value
    ptePcfHw = lwFieldGet32(&pFmt->pPte->fldPtePcf, 
            pEntryData);

    // 2. Read back the sw pcf from the hw pcf
    CHECK_RC(MmuUtils::GmmuTranslatePtePcfFromHw(
                ptePcfHw,
                IsPteValid(pEntryData),
                &ptePcfSw));

    // 3. Modify it
    CHECK_RC(SetSwPtePcfField(&ptePcfSw, ptePcfField, ptePcfValue));

    // 4. Remove SW PTE PCF bits that don't apply based on how
    //    the PTE's valid bit is set.
    ptePcfSw = MmuUtils::TrimSwPtePcf(ptePcfSw, IsPteValid(pEntryData));

    // 5. Update the sw pcf into hw pcf
    CHECK_RC(MmuUtils::GmmuTranslatePtePcfFromSw(
                ptePcfSw,
                &ptePcfHw));

    // 6. RM new API to set the pcf
    lwFieldSet32(&pFmt->pPte->fldPtePcf, ptePcfHw, 
            pEntryData);

    return OK;
}

// Modify an existing SW PCF based on the PTE PCF field passed.
//
RC MmuLevelSegment::SetSwPtePcfField
(
    UINT32* swPtePcf,
    PtePcfField ptePcfField,
    bool ptePcfValue
)
{
    RC rc;

    switch (ptePcfField)
    {
    case PtePcfField::ReadOnly:
        FIELD_SET_CLEAR(*swPtePcf, ptePcfValue, RO);
        break;
    case PtePcfField::AtomicDisable:
        FIELD_SET_CLEAR(*swPtePcf, ptePcfValue, NOATOMIC);
        break;
    case PtePcfField::Sparse: 
        FIELD_SET_CLEAR(*swPtePcf, ptePcfValue, SPARSE);
        break;
    case PtePcfField::Invalid:
        FIELD_SET_CLEAR(*swPtePcf, ptePcfValue, INVALID);
        break;
    case PtePcfField::Lw4K:
        FIELD_SET_CLEAR(*swPtePcf, ptePcfValue, LW4K);
        break;
    case PtePcfField::NoMapping:
        FIELD_SET_CLEAR(*swPtePcf, ptePcfValue, NOMAPPING);
        break;
    case PtePcfField::ACE:
        FIELD_SET_CLEAR(*swPtePcf, ptePcfValue, ACE);
        break;
    case PtePcfField::Volatile:
        FIELD_SET_CLEAR(*swPtePcf, ptePcfValue, UNCACHED);
        break;
    case PtePcfField::Privilege:
        // Regular is the opposite of Priv
        FIELD_SET_CLEAR(*swPtePcf, !ptePcfValue, REGULAR);
        break;
    default:
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Batch modify PCF of PDE entry for version 3
//!
//! \param pEntryData: Entry that's going to modify
//! \param pdeFields: a map of PdePcfField enums to bools.  Used to determine
//!                   how to construct the PCF for PTE data.
//! \param subLevelIdx: MmuSubLevelIndex: 4k or 64; Only valid for PDE0
//! \param fetchMode: an option to determine the source of initial pcf value 
//!
RC MmuLevelSegment::UpdateAllPdePcf
(
    UINT08 * pEntryData,
    const map<PdePcfField, bool>& pdeFields,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    FetchMode fetchMode
)
{
    RC rc;
    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
    UINT32 swPdePcf = 0;
    UINT32 hwPdePcf = 0;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version == GMMU_FMT_VERSION_3);
#endif

    if (fetchMode == FetchMode::FetchCache)
    {
        const GMMU_FMT_PDE* pFmtPde = gmmuFmtGetPde(pFmt,
                                            m_SegmentInfo.pFmtLevel,
                                            subLevelIdx);
        // 1. Decode current cache hw pcf value
        hwPdePcf = lwFieldGet32(&pFmtPde->fldPdePcf, 
                pEntryData);

        GMMU_APERTURE aperture = gmmuFieldGetAperture(&pFmtPde->fldAperture, pEntryData);


        // 2. Read back the sw pcf from the hw pcf
        CHECK_RC(MmuUtils::GmmuTranslatePdePcfFromHw(
                    hwPdePcf,
                    aperture,
                    &hwPdePcf));
    }

    // 3. Modify it
    for (auto& iter : pdeFields)
    {
        SetSwPdePcfField(&swPdePcf, iter.first, iter.second);
    }

    // 4. Update the sw pcf into hw pcf
    CHECK_RC(MmuUtils::GmmuTranslatePdePcfFromSw(swPdePcf, &hwPdePcf));
    // 5. RM new API to set the pcf
    lwFieldSet32(&pFmt->pPde->fldPdePcf, hwPdePcf, pEntryData);

    return OK;
}

RC MmuLevelSegment::UpdatePdePcf
(
    UINT08 * pEntryData,
    PdePcfField pdePcfField,
    MmuLevel::MmuSubLevelIndex subLevelIdx,
    bool pdePcfValue
)
{
    RC rc;
    const GMMU_FMT* pFmt = m_pMmuLevel->GetGmmuFmt();
    const GMMU_FMT_PDE* pFmtPde = gmmuFmtGetPde(pFmt,
                                                m_SegmentInfo.pFmtLevel,
                                                subLevelIdx);
    UINT32 pdePcfSw = 0;
    UINT32 pdePcfHw = 0;

#if LWCFG(GLOBAL_ARCH_HOPPER)
    MASSERT(pFmt->version == GMMU_FMT_VERSION_3);
#endif

    // 1. Decode current cache hw pcf value
    pdePcfHw = lwFieldGet32(&pFmtPde->fldPdePcf, 
            pEntryData);

    GMMU_APERTURE aperture = gmmuFieldGetAperture(&pFmtPde->fldAperture, pEntryData);

    // 2. Read back the sw pcf from the hw pcf
    CHECK_RC(MmuUtils::GmmuTranslatePdePcfFromHw(
                pdePcfHw,
                aperture,
                &pdePcfSw));

    // 3. Modify it
    CHECK_RC(SetSwPdePcfField(&pdePcfSw, pdePcfField, pdePcfValue));

    // 4. Update the sw pcf into hw pcf
    CHECK_RC(MmuUtils::GmmuTranslatePdePcfFromSw(
                pdePcfSw,
                &pdePcfHw));

    // 5. RM new API to set the pcf
    lwFieldSet32(&pFmtPde->fldPdePcf, pdePcfHw, 
            pEntryData);

    return OK;
}

// Modify an existing SW PCF based on the PDE PCF field passed.
//
RC MmuLevelSegment::SetSwPdePcfField
(
    UINT32* swPdePcf,
    PdePcfField pdePcfField,
    bool pdePcfValue
)
{
    RC rc;

    switch (pdePcfField)
    {
    case PdePcfField::Sparse:
        FIELD_SET_CLEAR(*swPdePcf, pdePcfValue, SPARSE);
        break;
    case PdePcfField::Invalid:
        FIELD_SET_CLEAR(*swPdePcf, pdePcfValue, INVALID);
        break;
    case PdePcfField::Volatile:
        FIELD_SET_CLEAR(*swPdePcf, pdePcfValue, UNCACHED);
        break;
    case PdePcfField::AtsAllow:
        FIELD_SET_CLEAR(*swPdePcf, pdePcfValue, ATS_ALLOWED);
        break;
    default:
        return RC::SOFTWARE_ERROR;
    }

    return rc;
}
