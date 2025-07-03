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

#include "pmsurf.h"
#include "mdiag/utils/mdiagsurf.h"
#include "ctrl/ctrl0080.h"
#include "gpu/include/gpudev.h"
#include "lwos.h"
#include "core/include/platform.h"
#include "pmchan.h"
#include "pmgilder.h"
#include "pmtest.h"
#include "pmevntmn.h"
#include "gpu/utility/surf2d.h"
#include "core/include/utility.h"
#include "class/cla0b5.h"
#include "class/cl90f1.h"       // FERMI_VASPACE_A
#include "class/clc3b5.h"       // VOLTA_DMA_COPY_A
#include "class/clc5b5.h"       // TURING_DMA_COPY_A
#include "ctrl/ctrl90f1.h"
#include "ctrl/ctrl0080/ctrl0080fb.h"
#include "g00x/g000/dev_mmu.h"
#include <algorithm>
#include <memory>
#include "mdiag/sysspec.h"
#include "mdiag/utils/mdiagsurf.h"

//--------------------------------------------------------------------
//! \brief constructor
//!
PmMmuLevel::PmMmuLevel
(
    GpuDevice               *pGpuDevice,
    MdiagSurf               *pSurface,
    const struct GMMU_FMT   *pGmmuFmt,
    UINT32                   levelId,
    MmuLevelTree            *pMmuLevelTree
) :
    MmuLevel(pGpuDevice, pSurface, pGmmuFmt, levelId, pMmuLevelTree)
{}

RC PmMmuLevel::SetAperture
(
    MmuSubLevelIndex subLevelIdx,
    PmChannel* pInbandChannel,
    PmMemMapping *pPmMemMapping
)
{
    RC rc;

    UINT64 offset = pPmMemMapping->GetOffset();
    UINT64 size = pPmMemMapping->GetSize();

    for (UINT64 batchOffset = offset; batchOffset < offset + size;)
    {
        MmuLevelSegment *pSegment = nullptr;
        CHECK_RC(GetMmuLevelSegment(batchOffset, &pSegment));

        UINT64 batchSize = offset + size - batchOffset;

        CHECK_RC(pSegment->SetAperture(batchOffset, &batchSize, subLevelIdx,
            pPmMemMapping->GetMdiagSurf()->GetLocation()));

        UINT32 subChannelNum = 0;
        Channel* pChannel = nullptr;
        if (pInbandChannel)
        {
            CHECK_RC(pInbandChannel->GetCESubchannelNum(&subChannelNum));
            pChannel = pInbandChannel->GetModsChannel();
        }

        CHECK_RC(pSegment->WriteMmuEntries(
            batchOffset, &batchSize, pChannel, subChannelNum));

        batchOffset += batchSize;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Modify Entry fields according to operations
//!        This function breaks the modification request of a range into
//!        different segments in MmuLevelSegment.
//! \param subLevelIdx - MmuSubLevelIndex: 4k or 64;
//!                      At present only valid for PDE0;
//!                      It will be ignored for other PDEs;
//! \param pInbandChannel: In-band operation mode
//! \param PmMemMapping:   Memory range to be modified
//! \param entryOps:       Operations to make
RC PmMmuLevel::ModifyMmuEntries
(
    MmuSubLevelIndex subLevelIdx,
    PmChannel *pInbandChannel,
    PmMemMapping *pPmMemMapping,
    const EntryOps& entryOps
)
{
    RC rc;

    UINT64 offset = pPmMemMapping->GetOffset();
    UINT64 size = pPmMemMapping->GetSize();

    MemAttrs *pMemAttrs = pPmMemMapping->GetMemAttrs();
    MmuLevelTree::LevelIndex levelNum =
        static_cast<MmuLevelTree::LevelIndex>(m_LevelId);

    for (UINT64 batchOffset = offset; batchOffset < offset + size; /*nop*/)
    {
        MmuLevelSegment *pSegment;
        CHECK_RC(GetMmuLevelSegment(batchOffset, &pSegment));

        UINT64 batchSize = offset + size - batchOffset;

        //
        // Modify local saved data according to request
        UINT32 mmuEntryOps = entryOps.MmuEntryOps;

        if (mmuEntryOps & EntryOps::MMUENTRY_OP_CHANGETYPE_TOPDE ||
            mmuEntryOps & EntryOps::MMUENTRY_OP_CHANGETYPE_TOPTE)
        {
            CHECK_RC(pSegment->ChangePageEntryType(
                batchOffset, &batchSize, subLevelIdx,
                (mmuEntryOps & EntryOps::MMUENTRY_OP_CHANGETYPE_TOPTE) != 0));

            // re-init the attributes of the level
            CHECK_RC(pMemAttrs->InitMemAttrsLevel(levelNum));

            mmuEntryOps &= ~(EntryOps::MMUENTRY_OP_CHANGETYPE_TOPDE |
                             EntryOps::MMUENTRY_OP_CHANGETYPE_TOPTE);
        }

        if (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_VALID)
        {
            // Background:
            // 1.If PDE is valid, the voliate bit means GPU cache or not
            // 2.If PDE is invalid, the voliate bit means sparse or not

            // SetPageAttrValid should be called before SetSegmentValid because
            // SetSegmentValid makes PDE entry become invalid. If it's called first,
            // The meaning of voliate bit will be parsed incorrectly in SetPageAttrValid
            CHECK_RC(pMemAttrs->SetPageAttrValid(levelNum, true));

            CHECK_RC(pSegment->SetSegmentValid(
                batchOffset, &batchSize, subLevelIdx, true)); // valid = true

            UINT64 physAddr = 0;
            GMMU_APERTURE aperture = GMMU_APERTURE_ILWALID;
            CHECK_RC(pSegment->GetEntryPhysAddr(batchOffset,
                MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT, &physAddr, &aperture));

#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (GetGmmuFmt()->version == GMMU_FMT_VERSION_3)
            {
                CHECK_RC(SetValidPcf(pMemAttrs, pSegment, batchOffset,
                    &batchSize, subLevelIdx));
            }
            else
#endif
            {
                bool bCachable = false;
                if (OK == pMemAttrs->IsPageAttrCache(levelNum, &bCachable))
                {
                    if (bCachable)
                    {
                        // re-enable cache bit
                        CHECK_RC(pSegment->SetSegmentVolatile(batchOffset,
                                &batchSize, subLevelIdx, false));
                    }
                    else
                    {
                        // re-disable cache bit
                        CHECK_RC(pSegment->SetSegmentVolatile(batchOffset,
                                &batchSize, subLevelIdx, true));
                    }
                }
            }

            mmuEntryOps &= ~EntryOps::MMUENTRY_OP_SET_VALID;
        }

        if (mmuEntryOps & EntryOps::MMUENTRY_OP_CLEAR_VALID)
        {
            // Background:
            // 1.If PDE is valid, the voliate bit means GPU cache or not
            // 2.If PDE is invalid, the voliate bit means sparse or not

            // SetPageAttrValid should be called before SetSegmentValid because
            // SetSegmentValid makes PDE entry become invalid. If it's called first,
            // The meaning of voliate bit will be parsed incorrectly in SetPageAttrValid
            CHECK_RC(pMemAttrs->SetPageAttrValid(levelNum, false));
            CHECK_RC(pSegment->SetSegmentValid(
                batchOffset, &batchSize, subLevelIdx, false));

#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (GetGmmuFmt()->version == GMMU_FMT_VERSION_3)
            {
                CHECK_RC(SetIlwalidPcf(pMemAttrs, pSegment, batchOffset,
                    &batchSize, subLevelIdx));
            }
            else
#endif
            {
                bool bSparse = false;
                if (OK == pMemAttrs->IsPageAttrSparse(levelNum, &bSparse))
                {
                    CHECK_RC(pSegment->SetSegmentSparse(batchOffset,
                            &batchSize, subLevelIdx, bSparse));
                }
            }

            mmuEntryOps &= ~EntryOps::MMUENTRY_OP_CLEAR_VALID;
        }
        
        // Accumulate PCF fields than set together.
        map<MmuLevelSegment::PtePcfField, bool> pteFields;
        map<MmuLevelSegment::PdePcfField, bool> pdeFields;
        if (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_CACHE ||
            mmuEntryOps & EntryOps::MMUENTRY_OP_CLEAR_CACHE)
        {
            // Gpu cache attribute only valid in case the pages are valid
            // Otherwise, just update the mem attribute
            bool bValid = false;
            bool bSetCache = (mmuEntryOps & EntryOps::MMUENTRY_OP_CLEAR_CACHE) != 0;
            CHECK_RC(pMemAttrs->IsPageAttrValid(levelNum, &bValid));
            if (bValid)
            {
#if LWCFG(GLOBAL_ARCH_HOPPER)
                if (GetGmmuFmt()->version == GMMU_FMT_VERSION_3)
                {
                    // clear => uncached => volatile = 1
                    pteFields[MmuLevelSegment::PtePcfField::Volatile] = bSetCache;
                    pdeFields[MmuLevelSegment::PdePcfField::Volatile] = bSetCache;
                }
                else
#endif
                {
                    // clear => uncached => volatile = 1
                    CHECK_RC(pSegment->SetSegmentVolatile(
                        batchOffset, &batchSize, subLevelIdx, bSetCache));
                }
                
            }

            CHECK_RC(pMemAttrs->SetPageAttrCache(levelNum, bSetCache));

            mmuEntryOps &= ~(EntryOps::MMUENTRY_OP_SET_CACHE |
                             EntryOps::MMUENTRY_OP_CLEAR_CACHE);
        }

        if (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_SPARSE ||
            mmuEntryOps & EntryOps::MMUENTRY_OP_CLEAR_SPARSE)
        {
            // Sparse only valid in case the pages are invalid
            // Otherwise, just update the mem attribute
            bool bValid = false;
            bool bSetSparse = (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_SPARSE) != 0;
            CHECK_RC(pMemAttrs->IsPageAttrValid(levelNum, &bValid));
            if (!bValid)
            {
#if LWCFG(GLOBAL_ARCH_HOPPER)
                if (GetGmmuFmt()->version == GMMU_FMT_VERSION_3)
                {
                    // set => sparse => volatile = 1
                    pteFields[MmuLevelSegment::PtePcfField::Sparse] = bSetSparse;
                    pdeFields[MmuLevelSegment::PdePcfField::Sparse] = bSetSparse;
                }
                else
#endif
                {
                    // set => sparse => volatile = 1
                    CHECK_RC(pSegment->SetSegmentSparse(
                        batchOffset, &batchSize, subLevelIdx, bSetSparse));
                }
            }

            CHECK_RC(pMemAttrs->SetPageAttrSparse(levelNum, bSetSparse));

            mmuEntryOps &= ~(EntryOps::MMUENTRY_OP_SET_SPARSE |
                             EntryOps::MMUENTRY_OP_CLEAR_SPARSE);
        }

        if (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_ATOMICDISABLE ||
            mmuEntryOps & EntryOps::MMUENTRY_OP_CLEAR_ATOMICDISABLE)
        {
            bool bSetAtomicDisable = (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_ATOMICDISABLE) != 0;
#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (GetGmmuFmt()->version == GMMU_FMT_VERSION_3)
            {
                pteFields[MmuLevelSegment::PtePcfField::AtomicDisable] = bSetAtomicDisable;
            }
            else
#endif
            {
                CHECK_RC(pSegment->SetSegmentAtomicDisable(
                    batchOffset, &batchSize, subLevelIdx, bSetAtomicDisable));
            }

            CHECK_RC(pMemAttrs->SetPageAttrAtomicDisable(levelNum, bSetAtomicDisable));

            mmuEntryOps &= ~(EntryOps::MMUENTRY_OP_SET_ATOMICDISABLE |
                             EntryOps::MMUENTRY_OP_CLEAR_ATOMICDISABLE);
        }

        if (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_ACCESSRO ||
            mmuEntryOps & EntryOps::MMUENTRY_OP_CLEAR_ACCESSRO)
        {
            bool bSetAccessRO = (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_ACCESSRO) != 0;
#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (GetGmmuFmt()->version == GMMU_FMT_VERSION_3)
            {
                pteFields[MmuLevelSegment::PtePcfField::ReadOnly] = bSetAccessRO;
            }
            else
#endif
            {
                CHECK_RC(pSegment->SetSegmentAccessRO(
                    batchOffset, &batchSize, subLevelIdx, bSetAccessRO));
            }

            CHECK_RC(pMemAttrs->SetPageAttrRO(levelNum, bSetAccessRO));

            mmuEntryOps &= ~(EntryOps::MMUENTRY_OP_SET_ACCESSRO |
                             EntryOps::MMUENTRY_OP_CLEAR_ACCESSRO);
        }

        if (mmuEntryOps & EntryOps::MMUENTRY_OP_CLEAR_PRIV ||
            mmuEntryOps & EntryOps::MMUENTRY_OP_SET_PRIV)
        {
            bool bSetPriv = (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_PRIV) != 0;
#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (GetGmmuFmt()->version == GMMU_FMT_VERSION_3)
            {
                pteFields[MmuLevelSegment::PtePcfField::Privilege] = bSetPriv;
            }
            else
#endif
            {
                CHECK_RC(pSegment->SetSegmentPriv(
                    batchOffset, &batchSize, subLevelIdx, bSetPriv));
            }

            CHECK_RC(pMemAttrs->SetPageAttrPriv(levelNum, bSetPriv));

            mmuEntryOps &= ~(EntryOps::MMUENTRY_OP_CLEAR_PRIV |
                             EntryOps::MMUENTRY_OP_SET_PRIV);
        }

#if LWCFG(GLOBAL_ARCH_HOPPER)
        if (GetGmmuFmt()->version == GMMU_FMT_VERSION_3)
        {
            // Handle PCF together
            CHECK_RC(pSegment->SetSegmentPcf(batchOffset, &batchSize, subLevelIdx,
                pteFields, pdeFields, MmuLevelSegment::FetchMode::FetchCache));
        }
#endif

        if (mmuEntryOps & EntryOps::MMUENTRY_OP_CONNECTLOWER)
        {
            CHECK_RC(pSegment->ConnectToLowerLevel(
                batchOffset, &batchSize, subLevelIdx));

            // no memory attributes change?
            mmuEntryOps &= ~EntryOps::MMUENTRY_OP_CONNECTLOWER;
        }

        if (mmuEntryOps & EntryOps::MMUENTRY_OP_SET_CONTI_PA)
        {
            CHECK_RC(pSegment->SetContiguousPA(
                batchOffset, &batchSize, subLevelIdx,
                entryOps.EntryPhysAddr + batchOffset - offset, entryOps.EntryAperture));

            mmuEntryOps &= ~EntryOps::MMUENTRY_OP_SET_CONTI_PA;
        }

        if (mmuEntryOps != 0)
        {
            MASSERT(!"Not supported function(s)");
            return RC::UNSUPPORTED_FUNCTION;
        }

        UINT32 subchNum = 0;
        Channel * pChannel = nullptr;
        if (pInbandChannel)
        {
            CHECK_RC(pInbandChannel->GetCESubchannelNum(&subchNum));
            pChannel = pInbandChannel->GetModsChannel();
        }

        //
        // Download to hw in the way of out-of-band or in-band
        CHECK_RC(pSegment->WriteMmuEntries(
            batchOffset, &batchSize, pChannel, subchNum));

        MASSERT(batchSize > 0);
        batchOffset += batchSize;
    }

    return rc;
}

// Set the PCF to the correct value after the valid bit has been changed to 1.
//
RC PmMmuLevel::SetValidPcf
(
    MemAttrs* pMemAttrs,
    MmuLevelSegment* pSegment,
    UINT64 offset,
    UINT64* pSize,
    MmuSubLevelIndex subLevelIdx
)
{
    RC rc;
    MmuLevelTree::LevelIndex levelNum =
        static_cast<MmuLevelTree::LevelIndex>(m_LevelId);
    map<MmuLevelSegment::PtePcfField, bool> pteFields;
    map<MmuLevelSegment::PdePcfField, bool> pdeFields;

    bool bCachable = false;
    if (OK == pMemAttrs->IsPageAttrCache(levelNum, &bCachable))
    {
        pteFields[MmuLevelSegment::PtePcfField::Volatile] = !bCachable;
        pdeFields[MmuLevelSegment::PdePcfField::Volatile] = !bCachable;
    }

    bool bAtomicDisable = false;
    if (OK == pMemAttrs->IsPageAttrAtomicDisable(levelNum, &bAtomicDisable))
    {
        pteFields[MmuLevelSegment::PtePcfField::AtomicDisable] = bAtomicDisable;
    }

    bool bReadOnly = false;
    if (OK == pMemAttrs->IsPageAttrRO(levelNum, &bReadOnly))
    {
        pteFields[MmuLevelSegment::PtePcfField::ReadOnly] = bReadOnly;
    }

    bool bPriv = false;
    if (OK == pMemAttrs->IsPageAttrPriv(levelNum, &bPriv))
    {
        pteFields[MmuLevelSegment::PtePcfField::Privilege] = bPriv;
    }

    // Lwrrently access counting is enabled by default and can't be changed
    // by the user, so always set to true.
    pteFields[MmuLevelSegment::PtePcfField::ACE] = true;

    pSegment->SetSegmentPcf(offset, pSize, subLevelIdx, pteFields, pdeFields,
        MmuLevelSegment::FetchMode::Reset);

    return rc;
}

// Set the PCF to the correct value after the valid bit has been changed to 0.
//
RC PmMmuLevel::SetIlwalidPcf
(
    MemAttrs* pMemAttrs,
    MmuLevelSegment* pSegment,
    UINT64 offset,
    UINT64* pSize,
    MmuSubLevelIndex subLevelIdx
)
{
    RC rc;
    MmuLevelTree::LevelIndex levelNum =
        static_cast<MmuLevelTree::LevelIndex>(m_LevelId);
    map<MmuLevelSegment::PtePcfField, bool> pteFields;
    map<MmuLevelSegment::PdePcfField, bool> pdeFields;

    bool bSparse = false;
    if (OK == pMemAttrs->IsPageAttrSparse(levelNum, &bSparse))
    {
        if (bSparse)
        {
            pteFields[MmuLevelSegment::PtePcfField::Sparse] = true;
            pdeFields[MmuLevelSegment::PdePcfField::Sparse] = true;
        }
        else
        {
            pteFields[MmuLevelSegment::PtePcfField::Invalid] = true;
            pdeFields[MmuLevelSegment::PdePcfField::Invalid] = true;
        }
    }

    pSegment->SetSegmentPcf(offset, pSize, subLevelIdx, 
        pteFields, pdeFields, MmuLevelSegment::FetchMode::Reset);

    return rc;
}

//--------------------------------------------------------------------
//! \brief constructor
//!
V1MmuLevel::V1MmuLevel
(
    GpuDevice               *pGpuDevice,
    MdiagSurf               *pSurface,
    const struct GMMU_FMT   *pGmmuFmt,
    UINT32                   levelId,
    MmuLevelTree            *pMmuLevelTree
) : 
    PmMmuLevel(pGpuDevice, pSurface, pGmmuFmt, levelId, pMmuLevelTree)
{
}

//--------------------------------------------------------------------
//! \brief A wrap function of LW0080_CTRL_DMA_FILL_PTE_MEM
//!        LW0080_CTRL_DMA_FILL_PTE_MEM only supports a range in same page table.
//!
//!        This function breaks the modification request of a range into
//!        different segments in MmuLevelSegment.
//!
//! \param hMem/MemOffset/PteFlags/PhysAddr:
//!        Please reference LW0080_CTRL_DMA_FILL_PTE_MEM for details
//!        Note: PhysAddr is the start PA of the range; must be contiguous
//! \param pInbandChannel: In-band operation mode
//! \param PmMemRange:     memory range to be modified
//!
RC V1MmuLevel::FillPteMem
(
    LwRm::Handle hMem,
    UINT64 MemOffset,
    UINT32 PteFlags,
    UINT64 PhysAddr,
    LwRm::Handle hSrcVASpace,
    LwRm::Handle hTgtVASpace,
    PmChannel *pInbandChannel,
    const PmMemRange* pPmMemRange
)
{
    RC rc;

    if ((m_LevelId != MmuLevelTree::GMMU_LEVEL_PTE_4K) &&
        (m_LevelId != MmuLevelTree::GMMU_LEVEL_PTE_64K) &&
        (m_LevelId != MmuLevelTree::GMMU_LEVEL_PTE_2M) &&
        (m_LevelId != MmuLevelTree::GMMU_LEVEL_PTE_512M))
    {
        ErrPrintf("%s only supports PTE modification\n", __FUNCTION__);
        return RC::ILWALID_ARGUMENT;
    }

    if (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _CONTIGUOUS, PteFlags)
        != LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_CONTIGUOUS_TRUE)
    {
        ErrPrintf("%s only supports contiguous physical address\n", __FUNCTION__);
        return RC::ILWALID_ARGUMENT;
    }

    UINT64 offset = pPmMemRange->GetOffset();
    UINT64 size = pPmMemRange->GetSize();

    for (UINT64 batchOffset = offset; batchOffset < offset + size; /*nop*/)
    {
        MmuLevelSegment *pSegment;
        CHECK_RC(GetMmuLevelSegment(batchOffset, &pSegment));

        UINT64 batchPhysAddr = PhysAddr + batchOffset - offset;
        UINT64 batchSize = offset + size - batchOffset;
        CHECK_RC(pSegment->FillPteMem(
            batchOffset, &batchSize, hMem, MemOffset, PteFlags,
            batchPhysAddr, hSrcVASpace, hTgtVASpace));

        UINT32 subchNum = 0;
        Channel * pChannel = nullptr;
        if (pInbandChannel)
        {
            CHECK_RC(pInbandChannel->GetCESubchannelNum(&subchNum));
            pChannel = pInbandChannel->GetModsChannel();
        }

        CHECK_RC(pSegment->WriteMmuEntries(
            batchOffset, &batchSize, pChannel, subchNum));

        MASSERT(batchSize > 0);
        batchOffset += batchSize;
    }

    // To be compatible with orignal behavior
    // Only out-of-band has defered tlbIlwalidate
    //
    if (!pInbandChannel &&
        (FLD_TEST_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                       _DEFER_ILWALIDATE, _FALSE, PteFlags)))
    {
        LwRm* pLwRm = m_pSurface->GetLwRmPtr();
        LW2080_CTRL_DMA_ILWALIDATE_TLB_PARAMS Params = {0};
        Params.hVASpace = hTgtVASpace;

        for (UINT32 subdev = 0; subdev < m_pGpuDevice->GetNumSubdevices(); ++subdev)
        {
            CHECK_RC(pLwRm->ControlBySubdevice(
                m_pGpuDevice->GetSubdevice(subdev),
                LW2080_CTRL_CMD_DMA_ILWALIDATE_TLB,
                &Params, sizeof(Params)));
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief constructor
//!
PmSurface::PmSurface
(
    PolicyManager *pPolicyManager,
    PmTest *pTest,
    MdiagSurf *pMdiagSurf,
    bool ExternalSurf2D
) :
    m_pPolicyManager(pPolicyManager),
    m_pTest(pTest),
    m_pMdiagSurf(pMdiagSurf),
    m_ExternalSurf2D(ExternalSurf2D),
    m_pDefaultChannel(NULL),
    m_Refs(1),
    m_ChildRefs(0),
    m_InDestructor(false),
    m_bSurfaceValidated(true),
    m_CpuMappingMode(PolicyManager::CpuMapDefault),
    m_pMemMappingsHelper(NULL),
    m_pOrigMemMappingsHelper(NULL),
    m_pHiddenTegraMappingsHelper(NULL),
    m_pOrigHiddenTegraMappingsHelper(NULL)
{
    MASSERT(pPolicyManager);
    MASSERT(pTest == NULL || pTest->GetPolicyManager() == pPolicyManager);
    MASSERT(pMdiagSurf);
#ifndef LW_VERIF_FEATURES
    m_bSurfaceValidated = false;
#endif

    // m_pMemMappingsHelper and m_pOrigMemMappingsHelper
    // are non-NULL pointer in whole PmSurface life cycle
    //
    UINT32 vaSpaceClass = 0;  // default
    Surface2D::VASpace vaSpace = m_pMdiagSurf->GetVASpace();
    if (vaSpace == Surface2D::GPUVASpace)
    {
        vaSpaceClass = FERMI_VASPACE_A;
    }

    m_pMemMappingsHelper = new PmMemMappingsHelper(this, NULL, vaSpaceClass);
    m_pOrigMemMappingsHelper = new PmMemMappingsHelper(this, NULL, vaSpaceClass);
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmSurface::~PmSurface()
{
    m_InDestructor = true;

    MASSERT(m_pMemMappingsHelper);
    MASSERT(m_pOrigMemMappingsHelper);
    delete m_pMemMappingsHelper;
    delete m_pOrigMemMappingsHelper;

    if (m_pHiddenTegraMappingsHelper)
    {
        delete m_pHiddenTegraMappingsHelper;
    }

    if (m_pOrigHiddenTegraMappingsHelper)
    {
        delete m_pOrigHiddenTegraMappingsHelper;
    }

    for (PmSubsurfaces::iterator ppSubsurface = m_Subsurfaces.begin();
         ppSubsurface != m_Subsurfaces.end(); ++ppSubsurface)
    {
        delete (*ppSubsurface);
    }
    m_Subsurfaces.clear();

    MASSERT(m_Refs == 0);
    MASSERT(m_ChildRefs == 0);

    if (!m_ExternalSurf2D)
    {
        delete m_pMdiagSurf;
        m_pMdiagSurf = nullptr;
    }
}

//--------------------------------------------------------------------
//! \brief Returns the surface size.
//!
//! Return the number of bytes in the surface, where the surface is
//! considered to start at GetMdiagSurf()->GetCtxDmaOffsetGpu() and
//! end where it's no longer mapped into memory.  This includes
//! ExtraAllocSize but not HiddenAllocSize; see
//! https://engwiki/index.php/Surface2D.
//!
UINT64 PmSurface::GetSize() const
{
    return m_pMdiagSurf->GetExtraAllocSize() + m_pMdiagSurf->GetSize();
}

//--------------------------------------------------------------------
//! \brief Add a named subsurface to the surface.
//!
//! This method creates a new PmSubsurface object in the surface.  If
//! a subsurface already exists with the indicated offset, size, name,
//! and typename (i.e. we already created the subsurface), then this
//! method does nothing.
//!
//! \param Offset The offset of the subsurface, relative to GetCtxDmaOffsetGpu
//! \param Size The size of the subsurface
//! \param Name The name of the subsurface
//! \param TypeName The subsurface's type
//!
RC PmSurface::AddSubsurface
(
    UINT64 Offset,
    UINT64 Size,
    const string &Name,
    const string &TypeName
)
{
    MASSERT(Size > 0);
    RC rc;

    // Check to see if this subsurface overlaps any other subsurface.
    // If so, we'll call it an error -- at least for now.  We may have
    // to remove this check if it turns out that trace3d TraceModules
    // can overlap, but it's easier on us if they can't, so we'll call
    // it an error for now.
    //
    for (PmSubsurfaces::iterator ppSubsurface = m_Subsurfaces.begin();
         ppSubsurface != m_Subsurfaces.end(); ++ppSubsurface)
    {
        if ((*ppSubsurface)->GetOffset() < Offset + Size &&
            Offset < (*ppSubsurface)->GetOffset() + (*ppSubsurface)->GetSize())
        {
            if (Offset == (*ppSubsurface)->GetOffset() &&
                Size ==  (*ppSubsurface)->GetSize() &&
                Name ==  (*ppSubsurface)->GetName() &&
                TypeName ==  (*ppSubsurface)->GetTypeName())
            {
                return rc;  // Subsurface was already added
            }
            else
            {
                ErrPrintf("Overlapping surfaces \"%s\" and \"%s\"\n",
                          (*ppSubsurface)->GetName().c_str(), Name.c_str());
                return RC::ILWALID_ARGUMENT;
            }
        }
    }

    // Add the new subsurface
    //
    DebugPrintf("PolicyManager: Add surface '%s', type '%s', %p[0x%llx-0x%llx]\n",
                Name.c_str(), TypeName.c_str(), m_pMdiagSurf, Offset, Size);
    m_Subsurfaces.push_back(new PmSubsurface(this, Offset, Size,
                                             Name, TypeName));

    // Whether this surface will be used in the policy file .
    // If this surface will be used, surfaceDesc will be created in parse processing.
    //
    // CreateMemMappings will do nothing if the mapping has been created.
    if (m_pPolicyManager->GetEventManager()->IsMatchSurfaceDesc(Name))
    {
        CHECK_RC(CreateMemMappings());
    }
    else
    {
        DebugPrintf("skip creating mappings for surface %s because it's not referenced in the policy file.\n",
                    Name.c_str());
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return all the allocated subsurfaces in the surface
//!
//! This method returns all the subsurfaces in the surface, if the
//! surface has been allocated.
//!
//! If the surface has not been allocated via Surface2D::Alloc yet,
//! then this method returns an empty array.
//!
//! Once a surface has been allocated, it *must* contain at least one
//! subsurface.  Either it contains a set of subsurfaces created by
//! AddSubsurface (typical of TraceModule surfaces), or it contains
//! one big subsurface that covers the entire surface (typical of
//! non-TraceModule surfaces).  If nobody has called AddSubsurface by
//! the time this method is called, then we assume the second case,
//! and automatically create a big subsurface to cover the surface.
//!
const PmSubsurfaces &PmSurface::GetSubsurfaces()
{
    // If we haven't allocated the surface yet, return an empty array
    //
    if (!m_pMdiagSurf->IsAllocated())
    {
        static const PmSubsurfaces emptySubsurfaces;
        return emptySubsurfaces;
    }

    // If this surface was created without subsurfaces, then create a
    // subsurface that spans the entire Surface2D.
    //
    if (m_Subsurfaces.empty())
    {
        if (OK != AddSubsurface(0, GetSize(), m_pMdiagSurf->GetName(), "Unknown"))
        {
            ErrPrintf("Cannot add subsurface %s.\n",  m_pMdiagSurf->GetName().c_str());
            MASSERT(0);   
        }
    }

    // Return
    //
    return m_Subsurfaces;
}

//--------------------------------------------------------------------
//! \brief Initialize m_MemMappings, if needed
//!
//! The first time this method is called after Surface2D::Alloc is
//! called, it stores the allocation data in m_MemMappings.  This
//! method is a no-op thereafter.
//!
//! This method is called from OnEvent, which should ensure that
//! m_MemMappings is initialized when needed, but it doesn't hurt to
//! call it right after creating a PmSurface just in case.
//!
RC PmSurface::CreateMemMappings()
{
    RC rc;

    // Abort if m_MemMappings is already set, or if we can't create it
    // because the surface isn't a paging surface and/or isn't
    // allocated yet.
    //
    if (!m_pMemMappingsHelper->GetMemMappings(NULL).empty() ||
        m_pMdiagSurf->GetMemHandle() == 0)
    {
        return rc;
    }
    else if (!m_pMdiagSurf->HasMap())
    {
        if (m_pMdiagSurf->IsAtsMapped())
        {
            CHECK_RC(m_pMemMappingsHelper->CreateAtsMappings());
            CHECK_RC(m_pOrigMemMappingsHelper->CloneMemMappings(
                 m_pMemMappingsHelper, NULL));
        }
        return rc;
    }

    // Expore mmu level info
    //
    CHECK_RC(ExploreMmuLevelTree());

    // Create memmappings
    //
    CHECK_RC(m_pMemMappingsHelper->CreateMemMappings());
    CHECK_RC(m_pOrigMemMappingsHelper->CloneMemMappings(
        m_pMemMappingsHelper, NULL));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get the original memory handle that was mapped at an offset
//!
//! This method returns the original mapping data at an offset in the
//! surface, before PolicyManager started moving pages around.
//!
//! \sa GetOrigMemOffset()
//! \sa GetOrigMemSize()
//!
LwRm::Handle PmSurface::GetOrigMemHandle(UINT64 Offset) const
{
    MASSERT(m_pMdiagSurf != NULL);
    if (m_pMdiagSurf->GetSplit() && Offset >= m_pMdiagSurf->GetPartOffset(1))
        return m_pMdiagSurf->GetSplitMemHandle();
    else
        return m_pMdiagSurf->GetMemHandle();
}

//--------------------------------------------------------------------
//! \brief Get the original offset into the mem returned by GetOrigMemHandle()
//!
//! \sa GetOrigMemHandle()
//! \sa GetOrigMemSize()
//!
UINT64 PmSurface::GetOrigMemOffset(UINT64 Offset) const
{
    MASSERT(m_pMdiagSurf != NULL);
    if (m_pMdiagSurf->GetSplit() && Offset >= m_pMdiagSurf->GetPartOffset(1))
        return Offset - m_pMdiagSurf->GetPartOffset(1);
    else
        return Offset + m_pMdiagSurf->GetHiddenAllocSize();
}

//--------------------------------------------------------------------
//! Get the remaining size, starting at Offset, of the mem returned by
//! GetOrigMemHandle()
//!
//! \sa GetOrigMemHandle()
//! \sa GetOrigMemOffset()
//!
UINT64 PmSurface::GetOrigMemSize(UINT64 Offset) const
{
    MASSERT(m_pMdiagSurf != NULL);
    if (m_pMdiagSurf->GetSplit() && Offset < m_pMdiagSurf->GetPartSize(0))
        return m_pMdiagSurf->GetPartSize(0) - Offset;
    else
        return GetSize() - Offset;
}

//--------------------------------------------------------------------
//! \brief Return an appropriate channel for inband operations
//!
//! This method is used when we need to do some inband operation on
//! the surface, and we don't care which channel we use.  Lwrrently,
//! it uses the trace3d DefaultChannel for the test when available, or
//! the first channel in the same MemSpace if not.
//!
//! This mechanism may be replaced by something more advanced if needs
//! be.  But since the current trend is to do out-of-band operations
//! when possible, I doubt anyone will care.
//!
PmChannel *PmSurface::GetDefaultChannel()
{
    if (m_pDefaultChannel == NULL)
    {
        GpuDevice *pGpuDevice = m_pMdiagSurf->GetGpuDev();
        PmChannels allChannels = m_pPolicyManager->GetActiveChannels();
        for (PmChannels::iterator ppChannel = allChannels.begin();
             ppChannel != allChannels.end(); ++ppChannel)
        {
            if (pGpuDevice == (*ppChannel)->GetGpuDevice())
            {
                m_pDefaultChannel = *ppChannel;
                break;
            }
        }
    }

    MASSERT(m_pDefaultChannel != NULL);
    return m_pDefaultChannel;
}

//--------------------------------------------------------------------
//! \brief This method should be called when an event oclwrs
//!
//! This method calls CreateMemMappings() to generate the initial
//! PmMemMappings structs, which keeps track of the current state of
//! the PTEs and remapped memory.
//!
RC PmSurface::OnEvent()
{
    RC rc;
    return rc;
}

//--------------------------------------------------------------------
//! \brief This method should be called when the test ends
//!
//! This method restores the surface to its original mappings, before
//! PolicyManager started modifying the PTEs and remapping memory.
//!
RC PmSurface::EndTest()
{
    RC rc;

    // If the mappings changed, update m_MemMappings and the PTEs to
    // match the original mapping.
    //
    if (m_pMemMappingsHelper->IsMemMappingsChanged(m_pOrigMemMappingsHelper))
    {
        // Not trigger tlbIlwalidate until page table update is done
        const bool deferTlbIlwalidate = true;

        // For BOTH(DUAL) type surface, current mappings may be in different pte/pde
        // as orignal mappings. Ilwalidate current memMappings before restoring
        // orignal mappings.
        //
        if (m_pMdiagSurf->IsDualPageSize())
        {
            PmMemMappings lwrMemMappings;
            lwrMemMappings = m_pMemMappingsHelper->GetMemMappings(NULL);
            for (UINT32 ii = 0; ii < lwrMemMappings.size(); ++ii)
            {
                CHECK_RC(lwrMemMappings[ii]->ModifyPtes(
                    DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, _FALSE),
                    DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID),
                    lwrMemMappings[ii], deferTlbIlwalidate, 0,
                    lwrMemMappings[ii]->GetMemAttrs()->GetActivePteLevel()));
            }
        }

        // Update memmappings to point original position
        //
        PmMemMappings origMemMappings;
        origMemMappings = m_pOrigMemMappingsHelper->GetMemMappings(NULL);

        CHECK_RC(m_pMemMappingsHelper->UpdateMemMappings(0, 0, 0, 0,
            &origMemMappings, 0, m_pOrigMemMappingsHelper->GetSize(),
            PmMemMappingsHelper::WritePtes_TRUE, deferTlbIlwalidate, 0));

        // Ilwalidate GMMU
        LwRm* pLwRm = GetLwRmPtr();
        LW2080_CTRL_DMA_ILWALIDATE_TLB_PARAMS Params = {0};
        GpuDevice *pGpuDevice = GetMdiagSurf()->GetGpuDev();
        for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
        {
            CHECK_RC(pLwRm->ControlBySubdevice(
                pGpuDevice->GetSubdevice(subdev),
                LW2080_CTRL_CMD_DMA_ILWALIDATE_TLB,
                &Params, sizeof(Params)));
        }
   }

    // restore cheetah part of shared surface
    //
    if (m_pHiddenTegraMappingsHelper)
    {
        if (m_pHiddenTegraMappingsHelper->IsMemMappingsChanged(
                            m_pOrigHiddenTegraMappingsHelper))
        {
            PmMemMappings origMemMappings =
                m_pOrigHiddenTegraMappingsHelper->GetMemMappings(NULL);

            CHECK_RC(m_pHiddenTegraMappingsHelper->UpdateMemMappings(0, 0, 0, 0,
                &origMemMappings, 0, m_pOrigHiddenTegraMappingsHelper->GetSize(),
                PmMemMappingsHelper::WritePtes_TRUE, false, 0));
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Increment the reference count
//!
//! Some PmSurfaces should be auto-deleted when nobody is using the
//! anymore.  The best (only?) example are the "hidden" surfaces that
//! were created to allocate physical memory for a page-movement
//! operation.  Once no other objects are using that physical memory
//! anymore, the hidden surface should be deleted.
//!
//! Thus, PmSurface has a modified form of the standard AddRef/Release
//! calls.  There are a few caveats:
//!
//! - PmSurfaces are created with a ref count of 1, under the
//!   assumption that the object that allocates the PmSurface will
//!   claim a reference.  If this is not the case, the allocating
//!   object should call Release() as soon as it has passed ownership
//!   to another object.
//! - Most objects that get a temporary PmSurface pointer don't need
//!   to call AddRef/Release.  Lwrrently, the only ones that need it
//!   are PmRange objects and subclasses (which keep hidden surfaces
//!   alive until they aren't needed) and the main PolicyManager
//!   object (which keeps non-hidden surfaces alive).
//! - The fact that PmRange objects & subclasses call AddRef creates a
//!   self-reference problem: PmSurface contains some PmRange objects
//!   in m_Subsurfaces and m_MemMappings.  The ref count can't drop to
//!   0 if those internal objects have called AddRef() on the
//!   PmSurface that contains them.  That's no good: we want to delete
//!   the PmSurface as soon as there are no more *external* references
//!   to it, but we don't care about the *internal* references.
//!   Rather than suppress the AddRef/Release calls in the child
//!   objects, it was easier to maintain a "child ref" count of all
//!   the child objects that called AddRef on their parent.  Once the
//!   ref count equals the child-ref count, we can delete the PmSurface.
//!
//! \sa Release()
//! \sa AddChildRef()
//! \sa ReleaseChild()
//! \sa GetRefs()
//! \sa GetChildRefs()
//!
void PmSurface::AddRef()
{
    ++m_Refs;
}

//--------------------------------------------------------------------
//! \brief Decrement the reference count.  See AddRef() for details.
//!
void PmSurface::Release()
{
    // Sanity check: If m_Refs drops below m_ChildRefs, something is
    // wrong.
    //
    MASSERT(m_Refs > m_ChildRefs);

    // Decrement the ref count.  Delete this object if m_Refs drops to
    // m_ChildRefs, unless we're already in the destructor.  (The
    // m_InDestructor relwrsion check is needed because, once in the
    // destructor, we'll probably hit the m_Refs == m_ChildRefs state
    // multiple times as we alternately call ReleaseChild() and
    // Release() on all the child objects' destructors).
    //
    --m_Refs;
    if (m_Refs == m_ChildRefs && !m_InDestructor)
    {
        m_InDestructor = true;
        delete this;
    }
}

//--------------------------------------------------------------------
//! \brief Increment the child-reference count.  See AddRef() for details.
//!
void PmSurface::AddChildRef()
{
    ++m_ChildRefs;

    // Sanity check: if calling AddChildRef() puts us in the "should
    // be deleted" state, we're probably calling the ref-count methods
    // in the wrong order
    //
    MASSERT(m_Refs > m_ChildRefs);
}

//--------------------------------------------------------------------
//! \brief Decrement the child-reference count.  See AddRef() for details.
//!
void PmSurface::ReleaseChild()
{
    MASSERT(m_ChildRefs > 0);
    --m_ChildRefs;
}

//--------------------------------------------------------------------
//! \brief Return whether specified range has smmu mappings.
//!
//! After page movement, it's possible that part of surface has gpu-
//! asid mappings but others not.
bool PmSurface::HasGpuSmmuMappings(const PmMemRange* pMemRange) const
{
    MASSERT(m_pMemMappingsHelper);
    return false;
}

//--------------------------------------------------------------------
//! \brief Fill a surface with specified value.
//!
//! \param Value     Given value to be filled
//! \param SubdevNum Gpu device number
//! \param pInbandChannel NULL means CPU out-band fill
RC PmSurface::Fill(UINT32 Value, UINT32 SubdevNum, PmChannel *pInbandChannel)
{
    RC rc;

    //
    // CPU fill
    if (pInbandChannel == NULL)
    {
        return GetMdiagSurf()->Fill(Value, SubdevNum);
    }

    //
    // GPU fill
    PmMemRange range(this, 0, GetSize());
    range.GpuInbandPhysFill(Value, SubdevNum, pInbandChannel);

    return rc;
}

//--------------------------------------------------------------------
//! \brief Get a MmuLevel via level index.
//!
//! \param index: Level index
//! \param ppMmuLevel: returned MmuLevel object pointer
//!
RC PmSurface::GetMmuLevel
(
    MmuLevelTree::LevelIndex index,
    MmuLevel** ppMmuLevel
)
{
    RC rc;
    MmuLevelTreeManager * pMmuMgr = MmuLevelTreeManager::Instance();
    CHECK_RC(pMmuMgr->GetMmuLevel(index, 
                                  m_pMdiagSurf->GetGpuDev(),
                                  m_pMdiagSurf,
                                  m_pMdiagSurf->GetCtxDmaOffsetGpu(),
                                  m_pMdiagSurf->GetVASpaceHandle(Surface2D::DefaultVASpace),
                                  ppMmuLevel));

    return OK;
}
//--------------------------------------------------------------------
//! \brief Get a PTE MmuLevel
//!
//! \param pageSize: page size of the PTE level
//! \param ppMmuLevel: returned MmuLevel object pointer
//!
RC PmSurface::GetMmmuPteLevel(UINT64 pageSize, MmuLevel** pMmuLevel)
{
    RC rc;

    if (pageSize == PolicyManager::BYTES_IN_SMALL_PAGE)
    {
        CHECK_RC(GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_4K, pMmuLevel));
    }
    else if (pageSize == PolicyManager::BYTES_IN_512MB_PAGE)
    {
        CHECK_RC(GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_512M, pMmuLevel));
    }
    else if (pageSize == PolicyManager::BYTES_IN_HUGE_PAGE)
    {
        CHECK_RC(GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_2M, pMmuLevel));
    }
    else
    {
        const UINT64 bigPageSize = GetMdiagSurf()->GetGpuDev()->GetBigPageSize();

        if (pageSize == bigPageSize)
        {
            CHECK_RC(GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_64K, pMmuLevel));
        }
        else
        {
            return RC::SOFTWARE_ERROR;
        }
    }

    return rc;
}

//------------------------------------------------------------------
//! \\brief Print MmuLevel inforamtion for gdb debug
//!
bool PmSurface::PrintMmuLevelSegmentInfo()
{
    return PrintMmuLevelSegmentInfo(NULL, false);
}

//--------------------------------------------------------------------
//! \brief Print MmuLevel information of a mmu tree.
//!
bool PmSurface::PrintMmuLevelSegmentInfo(string * pString = NULL, bool moreInfo = false)
{
    if (NULL == m_pMmuLevelTree)
    {
        return false;
    }

    MmuLevel* pMmuLevel = NULL;
    InfoPrintf("Surface Name = %s\n", GetMdiagSurf()->GetName().c_str());
    for (UINT32 level = 0; level < MmuLevelTree::GMMU_LEVEL_PDE_LAST; ++level)
    {
         if (OK != GetMmuLevel((MmuLevelTree::LevelIndex)level, &pMmuLevel))
         {
             continue;
         }


         UINT64 size = GetSize();
         for (UINT64 batchOffset = 0; batchOffset < size; /*nop*/)
         {
             MmuLevelSegment *pSegment = NULL;
             if (!pMmuLevel ||
                 (pMmuLevel->GetMmuLevelSegment(batchOffset, &pSegment) != OK))
             {
                 break;
             }

             RC rc;
             MmuLevelSegmentInfo info;
             rc = pSegment->GetMmuLevelSegmentInfo(&info);
             if (OK != rc)
             {
                 break;
             }

             UINT64 levelPageSize = info.levelPageSize;

            if(pString)
            {
                *pString += Utility::StrPrintf("Level = %d\n", level);
                *pString += Utility::StrPrintf("  PA = 0x%llx\n", info.physAddress);
                *pString += Utility::StrPrintf("  tableBasePA = 0x%llx\n", info.tableBasePhysAddress);
                *pString += Utility::StrPrintf("  levelPageSize = 0x%llx\n", levelPageSize);
                *pString += Utility::StrPrintf("  location = 0x%x\n", info.location);
                *pString += Utility::StrPrintf("  surfSegmentSize = 0x%llx\n", info.surfSegmentSize);
                *pString += Utility::StrPrintf("  surfaceStartOffset = 0x%llx\n", info.surfOffset);
                *pString += Utility::StrPrintf("  pCpuAddress = %p\n", info.pCpuAddress);
                *pString += Utility::StrPrintf("  pageCount = 0x%llx\n",
                  (info.surfSegmentSize + levelPageSize - 1)/levelPageSize);
            }
            else
            {
                InfoPrintf("Level = %d\n", level);
                InfoPrintf("  PA = 0x%llx\n", info.physAddress);
                InfoPrintf("  tableBasePA = 0x%llx\n", info.tableBasePhysAddress);
                InfoPrintf("  levelPageSize = 0x%llx\n", levelPageSize);
                InfoPrintf("  location = 0x%x\n", info.location);
                InfoPrintf("  surfSegmentSize = 0x%llx\n", info.surfSegmentSize);
                InfoPrintf("  surfaceStartOffset = 0x%llx\n", info.surfOffset);
                InfoPrintf("  pCpuAddress = %p\n", info.pCpuAddress);
                InfoPrintf("  pageCount = 0x%llx\n", (info.surfSegmentSize + levelPageSize - 1)/levelPageSize);
            }
            vector<UINT08> entryData;
            rc = pSegment->GetMmuEntriesData(batchOffset, &info.surfSegmentSize, &entryData);
            if (rc != OK)
            {
                break;
            }

            size_t sizeInDword = entryData.size() / sizeof(UINT32);
            UINT32 *pData = (UINT32*)(&entryData[0]);

#if LWCFG(GLOBAL_ARCH_HOPPER)
            if (pMmuLevel->GetGmmuFmt()->version == GMMU_FMT_VERSION_3 &&
                    moreInfo)
            {
                if(pString)
                {
                    *pString += Utility::StrPrintf("  mmuEntriesData[0x%x] = {\n", (UINT32)sizeInDword);
                    for (size_t i = 0; i < sizeInDword; i += 2)
                    {
                        // normal PTE/PDE entry is 64bit data
                        // PDE0 is 128bit
                        *pString += Utility::StrPrintf(" 0x%08x\n", pData[i]);
                        *pString += Utility::StrPrintf(" 0x%08x\n", pData[i + 1]);

                        bool isRO = false;
                        bool isNoAtomic = false;
                        bool isSparse = false;
                        bool isIlwalid = false;
                        bool isLW4K = false;
                        bool isNoMapping = false;
                        bool isACE = false;
                        bool isUnCached = false;
                        bool isPriv = false;
                        bool isAtsAllow = false;
                            
                        UINT08 *pRawData = (UINT08 *)(&pData[i]);
                        if (pSegment->IsPteEntry(pRawData))
                        {
                            CHECK_RC(pSegment->GetBoolFromPtePcf(pRawData,  
                                        MmuLevelSegment::PtePcfField::ReadOnly, &isRO));
                            CHECK_RC(pSegment->GetBoolFromPtePcf(pRawData,  
                                        MmuLevelSegment::PtePcfField::AtomicDisable, &isNoAtomic));
                            CHECK_RC(pSegment->GetBoolFromPtePcf(pRawData,  
                                        MmuLevelSegment::PtePcfField::Sparse, &isSparse));
                            CHECK_RC(pSegment->GetBoolFromPtePcf(pRawData,  
                                        MmuLevelSegment::PtePcfField::Invalid, &isIlwalid));
                            CHECK_RC(pSegment->GetBoolFromPtePcf(pRawData,  
                                        MmuLevelSegment::PtePcfField::Lw4K, &isLW4K));
                            CHECK_RC(pSegment->GetBoolFromPtePcf(pRawData,  
                                        MmuLevelSegment::PtePcfField::NoMapping, &isNoMapping));
                            CHECK_RC(pSegment->GetBoolFromPtePcf(pRawData,  
                                        MmuLevelSegment::PtePcfField::ACE, &isACE));
                            CHECK_RC(pSegment->GetBoolFromPtePcf(pRawData,  
                                        MmuLevelSegment::PtePcfField::Volatile, &isUnCached));
                            CHECK_RC(pSegment->GetBoolFromPtePcf(pRawData,  
                                        MmuLevelSegment::PtePcfField::Privilege, &isPriv));
                            *pString  += Utility::StrPrintf("MmuBits:\n RO = %d\n NoAtomic = %d\n Sparse = %d\n "
                                    "Invalid = %d\n LW4K = %d\n NoMapping = %d\n ACE = %d\n "
                                    "UnCached = %d\n Priv = %d\n\n", 
                                    isRO ? 1 : 0, isNoAtomic ? 1 : 0, isSparse ? 1 : 0, isIlwalid ? 1 : 0, 
                                    isLW4K ? 1 : 0, isNoMapping ? 1 : 0, isACE ? 1 : 0, isUnCached ? 1 : 0, 
                                    isPriv ? 1 : 0);
                        }
                        else
                        {
                            MmuLevel::MmuSubLevelIndex subLevelIdx = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT;
                            if (i % 4 == 0)
                            {
                                // normal PDE & PDE0 big sub level index
                                subLevelIdx = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_BIG;
                                if (sizeInDword == 4)
                                {
                                    *pString += "sub level Big: \n";
                                }
                            }
                            else if (i % 4 == 2)
                            {
                                // PDE0 small sub level index
                                subLevelIdx = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_SMALL;
                                *pString += "sub level Small: \n";
                            }

                            CHECK_RC(pSegment->GetBoolFromPdePcf(pRawData,  
                                        MmuLevelSegment::PdePcfField::Sparse, subLevelIdx, &isSparse));
                            CHECK_RC(pSegment->GetBoolFromPdePcf(pRawData,  
                                        MmuLevelSegment::PdePcfField::Invalid, subLevelIdx, &isIlwalid));
                            CHECK_RC(pSegment->GetBoolFromPdePcf(pRawData,  
                                        MmuLevelSegment::PdePcfField::Volatile, subLevelIdx, &isUnCached));
                            CHECK_RC(pSegment->GetBoolFromPdePcf(pRawData,  
                                        MmuLevelSegment::PdePcfField::AtsAllow, subLevelIdx, &isAtsAllow));
                            *pString  += Utility::StrPrintf("MmuBits:\n Sparse = %d\n "
                                    "Invalid = %d\n UnCached = %d\n AtsAllowed = %d\n\n", 
                                    isSparse ? 1 : 0, isIlwalid ? 1 : 0, 
                                    isUnCached ? 1 : 0, isAtsAllow ? 1 : 0);
                        }
                    }
                    *pString += Utility::StrPrintf(" }\n");
                }
                else
                {
                    InfoPrintf("  mmuEntriesData[0x%x] = {", (UINT32)sizeInDword);
                    for (size_t i = 0; i < sizeInDword; ++i)
                    {
                        InfoPrintf(" 0x%08x", pData[i]);
                    }
                    InfoPrintf(" }\n");
                }
            }
            else
#endif
            {
                if(pString)
                {
                    *pString += Utility::StrPrintf("  mmuEntriesData[0x%x] = {", (UINT32)sizeInDword);
                    for (size_t i = 0; i < sizeInDword; ++i)
                    {
                        *pString += Utility::StrPrintf(" 0x%08x", pData[i]);
                    }
                    *pString += Utility::StrPrintf(" }\n");
                }
                else
                {
                    InfoPrintf("  mmuEntriesData[0x%x] = {", (UINT32)sizeInDword);
                    for (size_t i = 0; i < sizeInDword; ++i)
                    {
                        InfoPrintf(" 0x%08x", pData[i]);
                    }
                    InfoPrintf(" }\n");
                }
            }
            
            batchOffset += info.surfSegmentSize;
         }
    }

    return true;
}

//--------------------------------------------------------------------
//! \brief Private function to explore mmu info of different levels.
//!
//! Notes:
//!  Policy manager may modify the content of mmu entries,
//!  but it will not allocate/free pde/pte table
RC PmSurface::ExploreMmuLevelTree()
{
    if (m_pMmuLevelTree)
    {
        // has been explored
        // PTE/PDE will not be freed/allocated
        // So mmu level info should not change
        // Note: PTE/PDE content may be modified
        return OK;
    }

    RC rc;
    GpuDevice *pGpuDevice = m_pMdiagSurf->GetGpuDev();
    LwRm::Handle hVASpace = m_pMdiagSurf->GetVASpaceHandle(Surface2D::DefaultVASpace);
    UINT64 virtAddress = m_pMdiagSurf->GetCtxDmaOffsetGpu();

    MmuLevelTreeManager * pMmuMgr = MmuLevelTreeManager::Instance();
    m_pMmuLevelTree = pMmuMgr->GetMmuLevelTree(pGpuDevice, m_pMdiagSurf, virtAddress, hVASpace);
    CHECK_RC(m_pMmuLevelTree->ExploreMmuLevels());

    return rc;
}

LwRm* PmSurface::GetLwRmPtr()
{
    return m_pMdiagSurf->GetLwRmPtr();
}

//--------------------------------------------------------------------
//! \brief constructor
//!
PmMemRange::PmMemRange
(
    PmSurface *pSurface,
    UINT64 Offset,
    UINT64 Size
) :
    m_pSurface(pSurface),
    m_Offset(Offset),
    m_Size(Size)
{
    if (m_pSurface != NULL)
        m_pSurface->AddRef();
}

//--------------------------------------------------------------------
//! \brief copy constructor
//!
PmMemRange::PmMemRange
(
    const PmMemRange &rhs
) :
    m_pSurface(rhs.m_pSurface),
    m_Offset(rhs.m_Offset),
    m_Size(rhs.m_Size)
{
    if (m_pSurface != NULL)
        m_pSurface->AddRef();
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmMemRange::~PmMemRange()
{
    if (m_pSurface != NULL)
        m_pSurface->Release();
}

//--------------------------------------------------------------------
//! \brief assignment operation
//!
PmMemRange &PmMemRange::operator=
(
    const PmMemRange &rhs
)
{
    Set(rhs.m_pSurface, rhs.m_Offset, rhs.m_Size);
    return *this;
}

//--------------------------------------------------------------------
//! \brief Define a sorting order for memory ranges.
//!
//! The sorting order is used to maintain STL sorted lists of memory
//! ranges.
//!
bool PmMemRange::operator<
(
    const PmMemRange &rhs
) const
{
    if (rhs.m_pSurface != m_pSurface)
        return m_pSurface < rhs.m_pSurface;
    else if (rhs.m_Offset != m_Offset)
        return m_Offset < rhs.m_Offset;
    else
        return m_Size < rhs.m_Size;
}

//--------------------------------------------------------------------
//! \brief Equivalent to *this = PmRange(pSurface, Offset, Size).
//!
void PmMemRange::Set(PmSurface *pSurface, UINT64 Offset, UINT64 Size)
{
    if (pSurface != NULL)
        pSurface->AddRef();
    if (m_pSurface != NULL)
        m_pSurface->Release();

    m_pSurface = pSurface;
    m_Offset   = Offset;
    m_Size     = Size;
}

//--------------------------------------------------------------------
//! \brief Return the GpuAddress of the memory range
//!
UINT64 PmMemRange::GetGpuAddr(GpuSubdevice *pLocalSubdev,
                              GpuSubdevice *pRemoteSubdev) const
{
    MASSERT(m_pSurface != NULL);
    MASSERT(((pLocalSubdev == NULL) && (pRemoteSubdev == NULL)) ||
            ((pLocalSubdev != NULL) && (pRemoteSubdev != NULL)));

    UINT64 Offset;

    if ((!pLocalSubdev) ||
        ((GetMdiagSurf()->GetLocation() != Memory::Fb) &&
         (pLocalSubdev->GetParentDevice() == pRemoteSubdev->GetParentDevice())))
    {
        Offset = GetMdiagSurf()->GetCtxDmaOffsetGpu();
    }
    else if (pLocalSubdev->GetParentDevice() == pRemoteSubdev->GetParentDevice())
    {
        // Loopback is not supported on SLI devices
        MASSERT(GetMdiagSurf()->IsPeerMapped());
        MASSERT((pLocalSubdev != pRemoteSubdev) ||
                pLocalSubdev->GetParentDevice()->GetNumSubdevices() == 1);

        // Otherwise for SLI type access pass in the local subdevice inst
        // since that is the surface where we want access
        Offset = GetMdiagSurf()->GetCtxDmaOffsetGpuPeer(
                                   pLocalSubdev->GetSubdeviceInst());
    }
    else if (GetMdiagSurf()->GetLocation() == Memory::Fb)
    {
        MASSERT(GetMdiagSurf()->IsPeerMapped(pRemoteSubdev->GetParentDevice()));
        Offset = GetMdiagSurf()->GetCtxDmaOffsetGpuPeer(
                                   pLocalSubdev->GetSubdeviceInst(),
                                   pRemoteSubdev->GetParentDevice(),
                                   pRemoteSubdev->GetSubdeviceInst());
    }
    else
    {
        MASSERT(GetMdiagSurf()->IsMapShared(pRemoteSubdev->GetParentDevice()));
        Offset = GetMdiagSurf()->GetCtxDmaOffsetGpuShared(
                                   pRemoteSubdev->GetParentDevice());
    }

    return Offset + m_Offset;
}

//--------------------------------------------------------------------
//! \brief Map the surface as required
//!
RC PmMemRange::MapPeer(GpuSubdevice *pLocalSubdev,
                       GpuSubdevice *pRemoteSubdev)
{
    RC rc;

    MASSERT(m_pSurface != NULL);
    MASSERT(((pLocalSubdev == NULL) && (pRemoteSubdev == NULL)) ||
            ((pLocalSubdev != NULL) && (pRemoteSubdev != NULL)));

    if (!pLocalSubdev)
        return OK;

    MdiagSurf *pMdiagSurf = const_cast<MdiagSurf *>(GetMdiagSurf());
    if (pLocalSubdev->GetParentDevice() == pRemoteSubdev->GetParentDevice())
    {
        if (pMdiagSurf->IsPeerMapped() ||
            GetMdiagSurf()->GetLocation() != Memory::Fb)
        {
            return OK;
        }
        else if (pLocalSubdev->GetParentDevice()->GetNumSubdevices() == 1)
        {
            CHECK_RC(pMdiagSurf->MapLoopback());
        }
        else
        {
            CHECK_RC(pMdiagSurf->MapPeer());
        }
    }
    else
    {
        if (((GetMdiagSurf()->GetLocation() == Memory::Fb) &&
             pMdiagSurf->IsPeerMapped(pRemoteSubdev->GetParentDevice())) ||
            ((GetMdiagSurf()->GetLocation() != Memory::Fb) &&
             pMdiagSurf->IsMapShared(pRemoteSubdev->GetParentDevice())))
        {
            return OK;
        }
        else if (GetMdiagSurf()->GetLocation() != Memory::Fb)
        {
            CHECK_RC(pMdiagSurf->MapShared(pRemoteSubdev->GetParentDevice()));
        }
        else
        {
            CHECK_RC(pMdiagSurf->MapPeer(pRemoteSubdev->GetParentDevice()));
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return true if the two memory ranges overlap
//!
bool PmMemRange::Overlaps(const PmMemRange *rhs) const
{
    MASSERT(rhs != NULL);

    return (m_pSurface != NULL &&
            m_pSurface == rhs->m_pSurface &&
            m_Offset < rhs->m_Offset + rhs->m_Size &&
            rhs->m_Offset < m_Offset + m_Size);
}

//--------------------------------------------------------------------
//! \brief Take two ranges, and return a range with the overlapping bytes
//!
//! If the two ranges do not overlap, then return an empty range
//!
PmMemRange PmMemRange::GetIntersection(const PmMemRange *rhs) const
{
    UINT64 Offset;
    UINT64 Size;

    if (Overlaps(rhs))
    {
        Offset = max(m_Offset, rhs->m_Offset);
        Size = min(m_Offset + m_Size, rhs->m_Offset + rhs->m_Size) - Offset;
    }
    else
    {
        Offset = m_Offset;
        Size = 0;
    }

    return PmMemRange(m_pSurface, Offset, Size);
}

//--------------------------------------------------------------------
//! \brief Modify the PTE flags in the memory range.
//!
//! Use LW0080_CTRL_CMD_DMA_FILL_PTE_MEM to modify the PTEs for this
//! range.  This can be used to unmap/remap the pages, set/clear the
//! priv bits, etc.
//!
//! \param Flags LW0080_CTRL_CMD_DMA_FILL_PTE_MEM flags to set in the
//!     PTEs.
//! \param Mask Mask of LW0080_CTRL_CMD_DMA_FILL_PTE_MEM flags that
//!     will be changed.  (Flags & ~Mask) must be 0.
//! \param TargetVaSpace target vaspace to be modified
//!
RC PmMemRange::ModifyPteFlags
(
    PolicyManager::PageSize PageSize,
    UINT32 PteFlags,
    UINT32 PteMask,
    PmChannel *inbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    PolicyManager::Level MmuLevelType,
    bool DeferTlbIlwalidate
) const
{
    MASSERT(m_pSurface != NULL);
    RC rc;


    // Case2: target is primary memmappings:
    // Modify top mmu vaspace or bottom mmu vaspace
    PmMemMappings memMappings;
    memMappings = m_pSurface->GetMemMappingsHelper()->SplitMemMappings(this);
    for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
         ppMemMapping != memMappings.end(); ++ppMemMapping)
    {
        PmMemMapping *pMemMapping = *ppMemMapping;
        
        bool UpdateLwrrentPte = false;
        bool UpdateIlwalidPte = false;
        UINT64 IlwalidPageSize = 0;

        if (pMemMapping->GetPageSize() == PolicyManager::BYTES_IN_SMALL_PAGE)
        {
            switch (PageSize)
            {
                case PolicyManager::LWRRENT_PAGE_SIZE:
                case PolicyManager::SMALL_PAGE_SIZE:
                    UpdateLwrrentPte = true;
                    break;
                case PolicyManager::BIG_PAGE_SIZE:
                    UpdateIlwalidPte = true;
                    break;
                case PolicyManager::HUGE_PAGE_SIZE:
                case PolicyManager::PAGE_SIZE_512MB:
                    {
                        ErrPrintf("Lwrent pagesize is SMALL, but the targeted PTE to be modified is HUGE/512MB. No plan to support combined actions implicitly. "
                                  "Please call Action.ChangePageSize() to change pagesize to 2M/512MB before modifying 2M/512MB PTE\n");
                        return RC::ILWALID_ARGUMENT;
                    }
                    break;
                case PolicyManager::ALL_PAGE_SIZES:
                    UpdateLwrrentPte = true;
                    UpdateIlwalidPte = true;
                default:
                    MASSERT(!"Illegal case");
            }
            GpuDevice *pGpuDevice = GetMdiagSurf()->GetGpuDev();
            IlwalidPageSize = pGpuDevice->GetBigPageSize();
        }
        else if ((pMemMapping->GetPageSize() != PolicyManager::BYTES_IN_HUGE_PAGE) &&
                 (pMemMapping->GetPageSize() != PolicyManager::BYTES_IN_512MB_PAGE))
        {
            switch (PageSize)
            {
                case PolicyManager::LWRRENT_PAGE_SIZE:
                case PolicyManager::BIG_PAGE_SIZE:
                    UpdateLwrrentPte = true;
                    break;
                case PolicyManager::SMALL_PAGE_SIZE:
                    UpdateIlwalidPte = true;
                    break;
                case PolicyManager::HUGE_PAGE_SIZE:
                case PolicyManager::PAGE_SIZE_512MB:
                    {
                        ErrPrintf("Lwrent pagesize is BIG, but the targeted PTE to be modified is HUGE/512MB. No plan to support combined actions implicitly. "
                                  "Please call Action.ChangePageSize() to change pagesize to 2M/512MB before modifying 2M/512MB PTE\n");
                        return RC::ILWALID_ARGUMENT;
                    }
                    break;
                case PolicyManager::ALL_PAGE_SIZES:
                    UpdateLwrrentPte = true;
                    UpdateIlwalidPte = true;
                default:
                    MASSERT(!"Illegal case");
            }
            IlwalidPageSize = PolicyManager::BYTES_IN_SMALL_PAGE;
        }
        else if (pMemMapping->GetPageSize() == PolicyManager::BYTES_IN_HUGE_PAGE)
        {
            // Current - HUGE pagesize
            switch (PageSize)
            {
                case PolicyManager::LWRRENT_PAGE_SIZE:
                case PolicyManager::HUGE_PAGE_SIZE:
                    UpdateLwrrentPte = true;
                    break;
                default:
                    {
                        ErrPrintf("Lwrent pagesize is HUGE, but the targeted PTE to be modified is SMALL/BIG/512MB. No plan to support combined actions implicitly. "
                                  "Please call Action.ChangePageSize() to change pagesize to SMALL/BIG/512MB before modifying SMALL/BIG/512MB PTE\n");
                        return RC::ILWALID_ARGUMENT;
                    }
            }
        }
        else
        {
            // Current - 512MB pagesize
            switch (PageSize)
            {
                case PolicyManager::LWRRENT_PAGE_SIZE:
                case PolicyManager::PAGE_SIZE_512MB:
                    UpdateLwrrentPte = true;
                    break;
                default:
                    {
                        ErrPrintf("Lwrent pagesize is 512MB, but the targeted PTE to be modified is SMALL/BIG/HUGE. No plan to support combined actions implicitly. "
                                  "Please call Action.ChangePageSize() to change pagesize to SMALL/BIG/HUGE before modifying SMALL/BIG/HUGE PTE\n");
                        return RC::ILWALID_ARGUMENT;
                    }
            }
        }

        if (UpdateLwrrentPte)
        {
            MmuLevelTree::LevelIndex LevelNumInt = this->GetLevelIndex(pMemMapping->GetPageSize(), MmuLevelType);
            CHECK_RC(pMemMapping->ModifyPtes(PteFlags, PteMask,
                                             *ppMemMapping, DeferTlbIlwalidate,
                                             inbandChannel, LevelNumInt));
        }
        if (UpdateIlwalidPte)
        {
            if(GetSurface()->IsMmuLevelFmtSupported())
            {
                // Call new MMU manipulation API
                MmuLevelTree::LevelIndex LevelNumInt = this->GetLevelIndex(IlwalidPageSize, MmuLevelType);
                CHECK_RC(pMemMapping->WriteMmuLevelEntries(PteFlags, PteMask,
                                                           FALSE, IlwalidPageSize,
                                                           DeferTlbIlwalidate,
                                                           inbandChannel, LevelNumInt));
            }
            else
            {
                CHECK_RC(pMemMapping->WritePtes(pMemMapping->GetMemHandle(),
                            pMemMapping->GetMemOffset(),
                            PteFlags,
                            pMemMapping->GetPhysAddr(),
                            IlwalidPageSize,
                            DeferTlbIlwalidate,
                            inbandChannel));
            }
        }
    }

    m_pSurface->GetMemMappingsHelper()->JoinMemMappings(this);

    return rc;
}

//--------------------------------------------------------------------
//! \brief Move the physical memory in a range to a newly-allocated location
//!
//!  "Dst" and "Src" in this function is different from the concept in Src=>Dst copy.
//!   DstMem: the memory to be modified to point to the new location
//!   SrcMem: the memory donating the physical memory to DstMem
//!
//!  pAllocatedSrcRange:
//!      pAllocatedSrcRange == NULL: Use newly allocated internal surface to donate physical address
//!      pAllocatedSrcRange != NULL: Use pre-allocated surface passed in to donate physical address
//!
//!   Two actions:
//!     1. Copy data of DstMem to new location(SrcMem) through in-band or out-of-band mode
//!     2. Modify DstMem PTE to point to physical memory of SrcMem
RC PmMemRange::MovePhysMem
(
    Memory::Location Location,
    PolicyManager::MovePolicy MovePolicy,
    PolicyManager::LoopBackPolicy Loopback,
    bool DeferTlbIlwalidate,
    PmChannel *pInbandChannel,
    PolicyManager::AddressSpaceType TargetVaSpace,
    Surface2D::VASpace SurfAllocType,
    Surface2D::GpuSmmuMode SurfGpuSmmuMode,
    const PmMemRange * pAllocatedSrcRange,
    bool DisablePostL2Compression
) const
{
    MASSERT(m_pSurface != NULL);

    RC rc;
    PolicyManager *pPolicyManager = m_pSurface->GetPolicyManager();
    GpuDevice     *pGpuDevice     = GetMdiagSurf()->GetGpuDev();
    bool bLoopback;

    if (m_Size == 0)
    {
        return rc;
    }

    string tgtVASpace = "Gmmu";

    DebugPrintf("%s: offset = 0x%llx size = 0x%llx TargetVaSpace = %s\n",
                __FUNCTION__, GetOffset(), GetSize(), tgtVASpace.c_str());

    // Determine whether to do loopback based on Loopback arg
    //
    switch (Loopback)
    {
        case PolicyManager::LOOPBACK:
            bLoopback = true;
            break;
        case PolicyManager::NO_LOOPBACK:
            bLoopback = false;
            break;
        default:
            MASSERT(!"Illegal Loopback arg in PmMemRange::MovePhysMem()");
            return RC::BAD_PARAMETER;
    }

    // Get the PmMemMappings we will update
    //
    PmMemMappings memMappings;
    memMappings = GetSurface()->GetMemMappingsHelper()->SplitMemMappings(this);
    Utility::CleanupOnReturnArg<PmMemMappingsHelper, void, const PmMemRange*>
        UndoSplitOnError(GetSurface()->GetMemMappingsHelper(),
        &PmMemMappingsHelper::JoinMemMappings, this);

    UINT64 dstGpuAddr = memMappings[0]->GetGpuAddr();
    UINT64 dstOffset = memMappings[0]->GetOffset();
    UINT64 dstSize = (memMappings.back()->GetOffset() +
                      memMappings.back()->GetSize() - dstOffset);

    PmMemRanges srcRanges;   // Candidates to donate the phys memory
    map<PmMemMapping*, PmMemRange> srcRangeMap;  // How srcRanges gets mapped

    for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
         ppMemMapping != memMappings.end(); ++ppMemMapping)
    {
          PmMemRange *pSrcRange = NULL;

         // Find the "full" range to donate the physcial address
         // This range doesn't include offset; the size is big enough to move whole dst range
         // The following "sub range"(srcSubRange) considers which offset of the "full" range is used.
         //
        if(pAllocatedSrcRange)
        {
            // Use pre - allocated surface passed in
            UINT64 pageSize = (*ppMemMapping)->GetPageSize();
            UINT64 alignedDstRangeSize = ALIGN_UP(GetSize(), pageSize);
            MdiagSurf * pSrcMdiagSurf = pAllocatedSrcRange->GetMdiagSurf();
            if (pSrcMdiagSurf->GetSize() - pAllocatedSrcRange->GetOffset()  < alignedDstRangeSize)
            {
                ErrPrintf("pre-created surface which remaining size 0x%llx is smaller than moved page size 0x%llx",
                          pSrcMdiagSurf->GetSize() - pAllocatedSrcRange->GetOffset(), alignedDstRangeSize);
                return RC::SOFTWARE_ERROR;
            }
            else if (pAllocatedSrcRange->GetGpuAddr() % pageSize != 0)
            {
                ErrPrintf("pre-created surface which va 0x%llx doesn't match the desired pagesize 0x%llx",
                          pAllocatedSrcRange->GetGpuAddr(), pageSize);
                return RC::SOFTWARE_ERROR;
            }
            else if (pSrcMdiagSurf->GetVASpace() != SurfAllocType)
            {
                ErrPrintf("pre-created surface which va space %d doesn't match the source surface va space %d",
                          pSrcMdiagSurf->GetVASpace(), SurfAllocType);
                return RC::SOFTWARE_ERROR;
            }

            // passing check, use the pre-allocated surface
            srcRanges.push_back(*pAllocatedSrcRange);
            pSrcRange = &srcRanges.back();
        }
        else
        {
            // Use internal surface
            //
            // Figure out the desired properties of the source surface
            UINT64 desiredPageSize = (*ppMemMapping)->GetPageSize();

            Memory::Location desiredLocation = Location;
            if (desiredLocation == Memory::Optimal)
            {
                switch (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                    _APERTURE, (*ppMemMapping)->GetPteFlags()))
                {
                case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_VIDEO_MEMORY:
                case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_PEER_MEMORY:
                    desiredLocation = Memory::Fb;
                    break;
                case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_COHERENT_MEMORY:
                    desiredLocation = Memory::Coherent;
                    break;
                case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_NON_COHERENT_MEMORY:
                    desiredLocation = Memory::NonCoherent;
                    break;
                }
            }

            // Re-use a surface that was allocated earlier in this loop if possible
            //     In 1st loop, srcRanges is empty and a new internal surface will be created for 1st mapping;
            //     In 2nd or later loop, it may re-use the surface created in 1st loop as long as desiredPageSize
            //      and desiredLocation keep unchanged; otherwise, another new internal surface will be created.
            //
            //     Note: the size of the surface created in 1st loop is big enough to hold the whole Dst surface,
            //                so no surface size check here.
            for (size_t ii = 0; ii < srcRanges.size(); ++ii)
            {
                bool bReuseAllocatedSurface =
                    (srcRanges[ii].GetMdiagSurf()->GetPageSize() * 1024 == desiredPageSize &&
                        srcRanges[ii].GetMdiagSurf()->GetLocation() == desiredLocation);

                if (bReuseAllocatedSurface)
                {
                    pSrcRange = &srcRanges[ii];
                    break;
                }
            }

            if (pSrcRange == NULL)
            {
                MdiagSurf *pDstMdiagSurf = m_pSurface->GetMdiagSurf();
                unique_ptr<MdiagSurf> pMdiagSurf(new MdiagSurf());

                if (Surface2D::BlockLinear == pDstMdiagSurf->GetLayout())
                {
                    // it is a new surface
                    pMdiagSurf->SetWidth(pDstMdiagSurf->GetWidth());
                    UINT32 extraHeight = ((UINT32)desiredPageSize - 1 +
                        pDstMdiagSurf->GetPitch()) / pDstMdiagSurf->GetPitch();
                    pMdiagSurf->SetHeight(pDstMdiagSurf->GetHeight() + extraHeight);
                    pMdiagSurf->SetColorFormat(pDstMdiagSurf->GetColorFormat());
                }
                else
                {
                    pMdiagSurf->SetWidth((UINT32)(dstSize + desiredPageSize - 1));
                    pMdiagSurf->SetHeight(1);
                    pMdiagSurf->SetColorFormat(ColorUtils::Y8);
                }

                PmSurface *pSrcSurface;
                pMdiagSurf->SetAlignment(pDstMdiagSurf->GetAlignment());
                pMdiagSurf->SetExtraAllocSize(pDstMdiagSurf->GetExtraAllocSize());
                pMdiagSurf->SetLayout(pDstMdiagSurf->GetLayout());
                pMdiagSurf->SetPteKind(pDstMdiagSurf->GetPteKind());
                pMdiagSurf->SetLocation(desiredLocation);
                pMdiagSurf->SetPageSize((UINT32)(desiredPageSize / 1024));
                pMdiagSurf->SetLoopBack(bLoopback &&
                    (desiredLocation == Memory::Fb));
                pMdiagSurf->SetPhysContig(true);

                if (SurfAllocType == Surface2D::DefaultVASpace)
                {
                    pMdiagSurf->SetVASpace(
                        m_pSurface->GetMdiagSurf()->GetVASpace());
                }
                else
                {
                    pMdiagSurf->SetVASpace(SurfAllocType);
                }


                CHECK_RC(pMdiagSurf->Alloc(pGpuDevice, pDstMdiagSurf->GetLwRmPtr()));

                pSrcSurface = new PmSurface(pPolicyManager,
                    m_pSurface->GetTest(),
                    pMdiagSurf.release(), false);
                CHECK_RC(pSrcSurface->CreateMemMappings());

                PmMemMappings srcMemMappings;
                srcMemMappings = pSrcSurface->GetMemMappingsHelper()->GetMemMappings(NULL);
                MASSERT(srcMemMappings.size() == 1);
                PmMemMapping *pSrcMemMapping = srcMemMappings[0];
                UINT64 srcPageSize = pSrcMemMapping->GetPageSize();
                MASSERT(srcPageSize <= desiredPageSize);

                srcRanges.push_back(PmMemRange(
                    pSrcSurface,
                    (dstGpuAddr - pSrcMemMapping->GetPhysAddr()) &
                    (srcPageSize - 1),
                    dstSize));
                pSrcSurface->Release();  // srcRanges[] has the ref now

                pSrcRange = &srcRanges.back();
            }
        }

        // Figure out the subRange;
        // Copy the data and update the PTEs
        //
        PmMemRange srcSubRange(pSrcRange->GetSurface(),
                               pSrcRange->GetOffset() +
                               (*ppMemMapping)->GetOffset() - dstOffset,
                               (*ppMemMapping)->GetSize());

        if (pInbandChannel != 0)
        {
            CHECK_RC(CopyPhysMemInband(*ppMemMapping, &srcSubRange, pGpuDevice,
                    pInbandChannel, DisablePostL2Compression));
        }
        else
        {
            for (UINT32 ii = 0; ii < pGpuDevice->GetNumSubdevices(); ++ii)
            {
                GpuSubdevice *pGpuSubdevice = pGpuDevice->GetSubdevice(ii);
                vector<UINT08> bytes;
                CHECK_RC((*ppMemMapping)->Read(&bytes, pGpuSubdevice));
                CHECK_RC(srcSubRange.Write(&bytes, pGpuSubdevice));
            }
        }

        // Associate the source (sub)surface with the PmMemMapping it
        // will be mapped to
        //
        srcRangeMap[*ppMemMapping] = srcSubRange;
    }

    // Call ApplyMovePolicy() just before updating the PTEs
    // In page movement, it's possible to only move part of surface.
    // So only apply move policy to intersection range.
    for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
         ppMemMapping != memMappings.end(); ++ppMemMapping)
    {
        PmMemRange range;
        range = (*ppMemMapping)->GetPhysRange()->GetIntersection(this);
        range.ApplyMovePolicy(MovePolicy,
            DeferTlbIlwalidate, (pInbandChannel != 0));
    }

    // Moving physical memory only inherits _APERTURE flag from src;
    // Other flags keep unchanged
    //
    UINT32 dstFlagsMask = ~DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE);

    // Should not set invalid PTE attributes (here we only checked atomic, add more if any)
    // Assumption that all kinds of PTE entry have same atomic support capability.
    // So only check 64k PTE of any surface to see whether atomic attribute is supported.
    MmuLevel *pMmuLevel;
    CHECK_RC(GetSurface()->GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_64K, &pMmuLevel));
    const GMMU_FMT_PTE* pFmtPte = pMmuLevel->GetGmmuFmt()->pPte;
    if (!lwFieldIsValid32(&pFmtPte->fldAtomicDisable.desc))
    {
        dstFlagsMask &= ~DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_ATOMIC_DISABLE);
    }

    // Update the PTEs of primary memmappings:
    //    case 1: modify Top mmu vaspace
    //
    for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
         ppMemMapping != memMappings.end(); ++ppMemMapping)
    {
        // No PTEs to set for an ATS mapping.
        if ((*ppMemMapping)->GetMappingType() == PmMemMapping::Ats_GvaSpa)
        {
            continue;
        }
        else
        {
            CHECK_RC((*ppMemMapping)->ModifyPtes(
                (*ppMemMapping)->GetPteFlags() & dstFlagsMask,
                dstFlagsMask,
                &srcRangeMap[*ppMemMapping],
                DeferTlbIlwalidate, pInbandChannel,
                (*ppMemMapping)->GetMemAttrs()->GetActivePteLevel()));
        }
    }

    UndoSplitOnError.Cancel();
    return rc;
}

//--------------------------------------------------------------------
//! \brief Read the data in the range, as it's lwrrently mapped.
//!
//! This method reads data from the surface.  If the surface has been
//! remapped in the PTEs, then this method reads the memory that the
//! PTEs point at, which may not be the same physical memory that the
//! surface was originally allocated with.
//!
//! \sa ReadPhys()
//!
//! \param[out] pBytes On success, the bytes are stored here
//! \param pGpuSubdevice The subdevice to read.
//!
RC PmMemRange::Read(vector<UINT08> *pBytes, GpuSubdevice *pGpuSubdevice) const
{
    MASSERT(pBytes != NULL);
    RC rc;

    pBytes->clear();
    pBytes->reserve((size_t)m_Size);

    if (m_Size > 0)
    {
        MASSERT(m_pSurface != NULL);
        PmMemMappings memMappings;
        memMappings = m_pSurface->GetMemMappingsHelper()->GetMemMappings(this);

        if (memMappings.size() == 0)
        {
            pBytes->resize((size_t)m_Size);
            return SurfaceUtils::ReadSurface(*(m_pSurface->GetMdiagSurf()->GetSurf2D()), 0,
                &((*pBytes)[0]), pBytes->size(), pGpuSubdevice->GetSubdeviceInst());
        }

        for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
             ppMemMapping != memMappings.end(); ++ppMemMapping)
        {
            PmMemRange subRange = GetIntersection(*ppMemMapping);
            void *pAddress;

            CHECK_RC((*ppMemMapping)->GetAddress(pGpuSubdevice, &pAddress));
            ReadData(pBytes, pAddress,
                     subRange.GetOffset() - (*ppMemMapping)->GetOffset(),
                     subRange.GetSize());
        }
    }

    MASSERT(pBytes->size() == m_Size);
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write data to the range, as it's lwrrently mapped.
//!
//! This method writes data to the surface.  If the surface has been
//! remapped in the PTEs, then this method writes to the memory that
//! the PTEs point at, which may not be the same physical memory that
//! the surface was originally allocated with.
//!
//! \sa WritePhys()
//!
//! \param bytes The data to write to the range.  If the range is
//!     bigger than bytes.size(), then this routine starts over at the
//!     beginning of bytes[].
//! \param pGpuSubdevice The subdevice to write.
//!
RC PmMemRange::Write
(
    const vector<UINT08> *pBytes,
    GpuSubdevice *pGpuSubdevice
) const
{
    MASSERT(pBytes != NULL);
    MASSERT(pBytes->size() > 0 || m_Size == 0);
    RC rc;

    if (m_Size > 0)
    {
        MASSERT(m_pSurface != NULL);
        PmMemMappings memMappings;
        memMappings = m_pSurface->GetMemMappingsHelper()->GetMemMappings(this);
        size_t byteOffset = 0;

        if (memMappings.size() == 0)
        {
            return SurfaceUtils::WriteSurface(*(m_pSurface->GetMdiagSurf()->GetSurf2D()), 0,
                &((*pBytes)[0]), pBytes->size(), pGpuSubdevice->GetSubdeviceInst());
        }

        for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
             ppMemMapping != memMappings.end(); ++ppMemMapping)
        {
            PmMemRange subRange = GetIntersection(*ppMemMapping);
            void      *pAddress;

            CHECK_RC((*ppMemMapping)->GetAddress(pGpuSubdevice, &pAddress));
            WriteData(pBytes, &byteOffset, pAddress,
                      subRange.GetOffset() - (*ppMemMapping)->GetOffset(),
                      subRange.GetSize());
        }
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Read the data in the range, from the original physical memory
//!
//! This method reads data from the surface.  If the surface has been
//! remapped in the PTEs, then this method reads the physical memory
//! that the surface was originally allocated with, which may not be
//! the same memory that the PTEs lwrrently point at in virtual-memory
//! space.
//!
//! \sa Read()
//!
//! \param[out] pBytes On success, the bytes are stored here
//! \param pGpuSubdevice The subdevice to read.
//!
RC PmMemRange::ReadPhys
(
    vector<UINT08> *pBytes,
    GpuSubdevice *pGpuSubdevice
) const
{
    MASSERT(pBytes != NULL);
    MASSERT(m_pSurface != NULL || m_Size == 0);
    RC      rc;
    LwRm* pLwRm = GetSurface()->GetLwRmPtr();

    pBytes->clear();
    pBytes->reserve((size_t)GetSize());

    UINT32 flags = 0;
    switch(GetSurface()->GetCpuMappingMode())
    {
    case PolicyManager::CpuMapDirect:
        flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DIRECT, flags);
        break;

    case PolicyManager::CpuMapReflected:
        flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _REFLECTED, flags);
        break;

    case PolicyManager::CpuMapDefault:
    default:
         flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DEFAULT, flags);
         break;
    }

    UINT64 ReadSize = 0;
    for (UINT64 ReadOffset = 0; ReadOffset < m_Size; ReadOffset += ReadSize)
    {
        MASSERT(ReadOffset == pBytes->size());
        ReadSize = min(m_Size - ReadOffset,
                       m_pSurface->GetOrigMemSize(ReadOffset));

        UINT08 *ptr;
        CHECK_RC(pLwRm->MapMemory(m_pSurface->GetOrigMemHandle(ReadOffset),
                                  m_pSurface->GetOrigMemOffset(ReadOffset),
                                  ReadSize, (void**)(&ptr), flags,
                                  pGpuSubdevice));
        ReadData(pBytes, ptr, 0, ReadSize);
        pLwRm->UnmapMemory(m_pSurface->GetOrigMemHandle(ReadOffset),
                           ptr, flags, pGpuSubdevice);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Write the data in the range, to the original physical memory
//!
//! This method writes data to the surface.  If the surface has been
//! remapped in the PTEs, then this method writes the physical memory
//! that the surface was originally allocated with, which may not be
//! the same memory that the PTEs lwrrently point at in virtual-memory
//! space.
//!
//! \sa Write()
//!
//! \param bytes The data to write to the range.  If the range is
//!     bigger than bytes.size(), then this routine starts over at the
//!     beginning of bytes[].
//! \param pGpuSubdevice The subdevice to write.
//!
RC PmMemRange::WritePhys
(
    const vector<UINT08> *pBytes,
    GpuSubdevice *pGpuSubdevice
) const
{
    MASSERT(pBytes != NULL);
    MASSERT(pBytes->size() > 0 || m_Size == 0);
    MASSERT(m_pSurface != NULL || m_Size == 0);
    RC      rc;
    LwRm* pLwRm = GetSurface()->GetLwRmPtr();

    UINT32 flags = 0;
    switch(GetSurface()->GetCpuMappingMode())
    {
    case PolicyManager::CpuMapDirect:
        flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DIRECT, flags);
        break;

    case PolicyManager::CpuMapReflected:
        flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _REFLECTED, flags);
        break;

    case PolicyManager::CpuMapDefault:
    default:
         flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DEFAULT, flags);
         break;
    }

    size_t byteOffset = 0;
    UINT64 WriteSize = 0;
    for (UINT64 WriteOffset = 0; WriteOffset < m_Size;
         WriteOffset += WriteSize)
    {
        MASSERT((WriteOffset % pBytes->size()) == byteOffset);
        WriteSize = min(m_Size - WriteOffset,
                        m_pSurface->GetOrigMemSize(WriteOffset));

        UINT08 *ptr;
        CHECK_RC(pLwRm->MapMemory(m_pSurface->GetOrigMemHandle(WriteOffset),
                                  m_pSurface->GetOrigMemOffset(WriteOffset),
                                  WriteSize, (void**)(&ptr), flags,
                                  pGpuSubdevice));
        WriteData(pBytes, &byteOffset, ptr, 0, WriteSize);
        pLwRm->UnmapMemory(m_pSurface->GetOrigMemHandle(WriteOffset),
                           ptr, flags, pGpuSubdevice);
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Use Gpu CE engine to fill a given data in the range
//!
//! This method use a given data to fill a range of surface. If channel
//! does not include CE object, failure will be returned.
//!
//! \param Value UINT32 data to fill into a surface
//!
//! \param subdev Subdevice number we are working on.
//
//! \param pInbandChannel Channel used to send CE methods
//!
RC PmMemRange::GpuInbandPhysFill
(
    UINT32 Value,
    UINT32 SubdevNum,
    PmChannel *pInbandChannel
)
{
    RC rc;

    if (!m_Size)
    {
        return rc; // nothing to do
    }

    //
    // Call private function to fill the range
    PmMemMappings memMappings;
    memMappings = m_pSurface->GetMemMappingsHelper()->GetMemMappings(this);

    if (memMappings.size() == 0)
    {
        CHECK_RC(m_pSurface->CreateMemMappings());
        memMappings = m_pSurface->GetMemMappingsHelper()->GetMemMappings(this);
        MASSERT(memMappings.size() != 0);
    }

    for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
         ppMemMapping != memMappings.end(); ++ppMemMapping)
    {
        PmMemRange subRange = GetIntersection(*ppMemMapping);
        UINT64 physAddr = (*ppMemMapping)->GetPhysAddr() +
                          subRange.GetOffset() - (*ppMemMapping)->GetOffset();
        UINT64 size = subRange.GetSize();
        if (!size)
        {
            continue;
        }

        UINT32 aperture = 0;
        switch (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                    _APERTURE, (*ppMemMapping)->GetPteFlags()))
        {
        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_VIDEO_MEMORY:
            aperture = FLD_SET_DRF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB, aperture);
            break;

        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_COHERENT_MEMORY:
            aperture = FLD_SET_DRF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _COHERENT_SYSMEM, aperture);
            break;

        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_NON_COHERENT_MEMORY:
            aperture = FLD_SET_DRF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM, aperture);
            break;

        default:
            MASSERT(!"Illegal location in PmMemRange::CopyPhysMemInband()");
        }

        CHECK_RC(FillPhysMemInband(Value, SubdevNum, pInbandChannel,
                                   physAddr, aperture, size));
    }

    return rc;
}

//--------------------------------------------------------------------
//! Read data from the provided address.
//!
//! \param[inout] pBytes On success, the bytes are appended to here.
//! \param pAddress The address to read.
//! \param MemOffset The offset from pAddress to read
//! \param MemSize The number of bytes to read
//!
/* static */ void PmMemRange::ReadData
(
    vector<UINT08> *pBytes,
    void *pAddress,
    UINT64 MemOffset,
    UINT64 MemSize
)
{
    MASSERT(pBytes != NULL);
    MASSERT(pAddress != 0);

    // Read the bytes
    //
    size_t byteOffset = pBytes->size();
    size_t bytesRead = 0;

    pBytes->resize(pBytes->size() + (size_t)MemSize);
    while (bytesRead < MemSize)
    {
        UINT32 readSize = (UINT32)min(MemSize - bytesRead, 0xffffffffULL);
        Platform::VirtualRd((UINT08 *)pAddress + MemOffset + bytesRead,
                            &(*pBytes)[byteOffset],
                            readSize);
        bytesRead += readSize;
        byteOffset += readSize;
    }
}

//--------------------------------------------------------------------
//! Write data to the provided address.
//!
//! \param pBytes The data to write
//! \param pByteOffset[inout] On entry, the offset in pBytes to start
//!     writing from.  On exit, the next offset in pBytes to write, if
//!     more data needs to be written.
//! \param pAddress The address to write.
//! \param MemOffset The offset into hMem to write
//! \param MemSize The number of bytes to write
//!
/* static */ void PmMemRange::WriteData
(
    const vector<UINT08> *pBytes,
    size_t *pByteOffset,
    void *pAddress,
    UINT64 MemOffset,
    UINT64 MemSize)
{
    MASSERT(pBytes != NULL);
    MASSERT(pByteOffset != NULL);
    MASSERT(*pByteOffset < pBytes->size());
    MASSERT(pAddress != 0);

    // Write the bytes
    //
    size_t bytesWritten = 0;
    while (bytesWritten < MemSize)
    {
        UINT32 writeSize =
            (UINT32)min((UINT64)(MemSize - bytesWritten),
                        (UINT64)(pBytes->size() - *pByteOffset));
        Platform::VirtualWr((UINT08 *)pAddress +MemOffset + bytesWritten,
                            &(*pBytes)[*pByteOffset],
                            writeSize);
        bytesWritten += writeSize;
        *pByteOffset = (*pByteOffset + writeSize) % pBytes->size();
    }
}

//--------------------------------------------------------------------
//! \brief Write data to the provided address.
//!
//! \param pSourceMapping The mapping for the source memory of the copy
//! \param pDestRange The memory range for the destination of the copy
//! \param pGpuDevice The GPU device of the surface being copied
//! \param pInbandChannel The channel to which the copy engine methods
//!        will be written
//!
/* static */ RC PmMemRange::CopyPhysMemInband
(
    PmMemMapping *pSourceMapping,
    PmMemRange *pDestRange,
    GpuDevice *pGpuDevice,
    PmChannel *pInbandChannel,
    bool DisablePostL2Compression
)
{
    RC rc;
    MASSERT(pInbandChannel != 0);

    PmMemMappingsHelper *pMappingsHelper;
    pMappingsHelper = pDestRange->GetSurface()->GetMemMappingsHelper();
    PmMemMappings memMappings = pMappingsHelper->GetMemMappings(pDestRange);
    UINT64 sourcePhysAddr = pSourceMapping->GetPhysAddr();

    for (PmMemMappings::iterator ppMemMapping = memMappings.begin();
        ppMemMapping != memMappings.end();
        ++ppMemMapping)
    {
        MASSERT(sourcePhysAddr < pSourceMapping->GetPhysAddr() + pDestRange->GetSize());

        PmMemRange subRange = pDestRange->GetIntersection(*ppMemMapping);
        UINT64 destPhysAddr = (*ppMemMapping)->GetPhysAddr() + subRange.GetOffset() - (*ppMemMapping)->GetOffset();
        UINT64 MemSize = subRange.GetSize();

        PM_BEGIN_INSERTED_METHODS(pInbandChannel);

        UINT32 data = 0;

        switch (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                        _APERTURE, pSourceMapping->GetPteFlags()))
        {
            case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_VIDEO_MEMORY:
                data = FLD_SET_DRF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _LOCAL_FB, data);
                break;

            case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_COHERENT_MEMORY:
                data = FLD_SET_DRF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _COHERENT_SYSMEM, data);
                break;

            case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_NON_COHERENT_MEMORY:
                data = FLD_SET_DRF(A0B5, _SET_SRC_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM, data);
                break;

            default:
                MASSERT(!"Illegal location in PmMemRange::CopyPhysMemInband()");
        }

        UINT32 subchNum = 0;
        CHECK_RC(pInbandChannel->GetCESubchannelNum(&subchNum));

        pInbandChannel->Write(subchNum, LWA0B5_SET_SRC_PHYS_MODE, data);

        data = 0;
        switch (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                        _APERTURE, (*ppMemMapping)->GetPteFlags()))
        {
            case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_VIDEO_MEMORY:
                data = FLD_SET_DRF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _LOCAL_FB, data);
                break;

            case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_COHERENT_MEMORY:
                data = FLD_SET_DRF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _COHERENT_SYSMEM, data);
                break;

            case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_NON_COHERENT_MEMORY:
                data = FLD_SET_DRF(A0B5, _SET_DST_PHYS_MODE, _TARGET, _NONCOHERENT_SYSMEM, data);
                break;

            default:
                MASSERT(!"Illegal location in PmMemRange::CopyPhysMemInband()");
        }

        pInbandChannel->Write(subchNum, LWA0B5_SET_DST_PHYS_MODE, data);

        pInbandChannel->Write(subchNum, LWA0B5_PITCH_IN, 0);
        pInbandChannel->Write(subchNum, LWA0B5_PITCH_OUT, 0);
        pInbandChannel->Write(subchNum, LWA0B5_OFFSET_IN_UPPER,
            static_cast<UINT32>(sourcePhysAddr >> 32));
        pInbandChannel->Write(subchNum, LWA0B5_OFFSET_IN_LOWER,
            static_cast<UINT32>(sourcePhysAddr & 0xFFFFFFFF));
        pInbandChannel->Write(subchNum, LWA0B5_OFFSET_OUT_UPPER,
            static_cast<UINT32>(destPhysAddr >> 32));
        pInbandChannel->Write(subchNum, LWA0B5_OFFSET_OUT_LOWER,
            static_cast<UINT32>(destPhysAddr & 0xFFFFFFFF));
        pInbandChannel->Write(subchNum, LWA0B5_LINE_LENGTH_IN, (UINT32)MemSize);
        pInbandChannel->Write(subchNum, LWA0B5_LINE_COUNT, 1);

        data = 0;
        data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _PIPELINED, data);
        if (PolicyManager::Instance()->GetInbandCELaunchFlushENable())
        {
            data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE, data);
        }
        else
        {
            data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _FALSE, data);
        }
        data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _SRC_TYPE, _PHYSICAL, data);
        data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DST_TYPE, _PHYSICAL, data);
        data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH, data);
        data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH, data);

        if (pInbandChannel->GetSubchannelClass(subchNum) >= TURING_DMA_COPY_A)
        {
            if (DisablePostL2Compression)
            {
                data = FLD_SET_DRF(C5B5, _LAUNCH_DMA, _DISABLE_PLC, _TRUE, data);
            }
            else
            {
                data = FLD_SET_DRF(C5B5, _LAUNCH_DMA, _DISABLE_PLC, _FALSE, data);
            }
        }

        pInbandChannel->Write(subchNum, LWA0B5_LAUNCH_DMA, data);

        PM_END_INSERTED_METHODS();

        sourcePhysAddr += subRange.GetSize();
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Private function to fill a block of contiguous memory
//!        Raw fill function called by GpuInbandPhysFill()
//!
//! \param Value Give UINT32 to fill into surface
//! \param SubdevNum Subdevice number we are working on
//! \param pCh The channel to which the copy engine methods
//!        will be written
//! \param PhysAddr Dst Address
//! \param Size Dst size
//!
/*static*/ RC PmMemRange::FillPhysMemInband
(
    UINT32 Value,
    UINT32 SubdevNum,
    PmChannel *pCh,
    UINT64 PhysAddr,
    UINT32 Aperture,
    UINT64 Size
)
{
    RC rc;
    MASSERT(pCh);

    UINT32 subChannel = 0;
    CHECK_RC(pCh->GetCESubchannelNum(&subChannel));

    PM_BEGIN_INSERTED_METHODS(pCh);

    // Set GPU subdevice mask for SLI system
    UINT32 gpuSubdeviceNum = pCh->GetGpuDevice()->GetNumSubdevices();
    if(gpuSubdeviceNum > 1)
    {
        MASSERT(SubdevNum < gpuSubdeviceNum);
        UINT32 subdevMask = 1 << SubdevNum;
        CHECK_RC(pCh->WriteSetSubdevice(subdevMask));
    }

    //
    // Send CE methods to fill a 32bit const value
    //
    // set linelength, send launch dma
    const bool use4BytesCopy = (Size & 0x3) == 0;
    const UINT32 LineCount = 1;
    const UINT64 LineLength = use4BytesCopy? Size >> 2 : Size;
    if (!use4BytesCopy)
    {
        // make sure 4bytes are identical for byte copy
        // Note: the following code depends on the assumption that Value is
        //       a 32bit type.
        bool validValue = ((Value & 0xFFFF) == ((Value >> 16) & 0xFFFF)) &&
                          ((Value & 0xFF) == ((Value >> 8) & 0xFF));
        if (!validValue)
        {
            ErrPrintf("Each byte of the 32-bit fill value must be identical if the fill size is not a multiple of four."
                      "Fill value = 0x%x, Fill size = 0x%llx\n",
                      Value, Size);
            return RC::BAD_PARAMETER;
        }
    }

    MASSERT(LineLength >> 32 == 0);
    CHECK_RC(pCh->Write(subChannel, LWA0B5_LINE_COUNT, LineCount));
    CHECK_RC(pCh->Write(subChannel, LWA0B5_LINE_LENGTH_IN, static_cast<UINT32>(LineLength)));
    CHECK_RC(pCh->Write(subChannel, LWA0B5_SET_DST_PHYS_MODE, Aperture));
    CHECK_RC(pCh->Write(subChannel, LWA0B5_OFFSET_OUT_UPPER, LwU64_HI32(PhysAddr)));
    CHECK_RC(pCh->Write(subChannel, LWA0B5_OFFSET_OUT_LOWER, LwU64_LO32(PhysAddr)));

    // set remapping
    CHECK_RC(pCh->Write(subChannel, LWA0B5_SET_REMAP_CONST_A, Value));
    CHECK_RC(pCh->Write(subChannel, LWA0B5_SET_REMAP_COMPONENTS,
        REF_DEF(LWA0B5_SET_REMAP_COMPONENTS_DST_X, _CONST_A) |
        (use4BytesCopy?
            REF_DEF(LWA0B5_SET_REMAP_COMPONENTS_COMPONENT_SIZE, _FOUR):
            REF_DEF(LWA0B5_SET_REMAP_COMPONENTS_COMPONENT_SIZE, _ONE))
        ));

    UINT32 data = 0x0;
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DATA_TRANSFER_TYPE, _NON_PIPELINED, data);
    if (PolicyManager::Instance()->GetInbandCELaunchFlushENable())
    {
        data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _TRUE, data);
    }
    else
    {
        data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _FLUSH_ENABLE, _FALSE, data);
    }
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DST_TYPE, _PHYSICAL, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _SRC_MEMORY_LAYOUT, _PITCH, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _DST_MEMORY_LAYOUT, _PITCH, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _MULTI_LINE_ENABLE, _FALSE, data);
    data = FLD_SET_DRF(A0B5, _LAUNCH_DMA, _REMAP_ENABLE, _TRUE, data);

    CHECK_RC(pCh->Write(subChannel, LWA0B5_LAUNCH_DMA, data));

    // Renable all the subdevices for SLI system
    if (gpuSubdeviceNum > 1)
    {
        CHECK_RC(pCh->WriteSetSubdevice(Channel::AllSubdevicesMask));
    }

    PM_END_INSERTED_METHODS();

    return rc;
}

//--------------------------------------------------------------------
//! \brief Apply MovePolicy to a range of old physical memory
//!
//! Called by MovePhysMem to determine what to do with physical memory
//! that will no longer be in use after the move.
//!
//! Note that this must be called *before* doing the move, because
//! some MovePolicies implicitly cause the RM to ilwalidate the TLB
//! (e.g. scrambling, when it maps & unmaps the surfaces).  Doing it
//! after the move would defeat the DeferTlbIlwalidate option.
//
RC PmMemRange::ApplyMovePolicy
(
    PolicyManager::MovePolicy MovePolicy,
    bool DeferTlbIlwalidate,
    bool isInband
) const
{
    PmGilder *pGilder = GetSurface()->GetPolicyManager()->GetGilder();
    RC rc;

    switch (MovePolicy)
    {
        case PolicyManager::DUMP_MEM:
            pGilder->DumpRange(*this);
            break;
        case PolicyManager::CRC_MEM:
            pGilder->CrcRange(*this);
            break;
        case PolicyManager::SCRAMBLE_MEM:
        {
            // Can't scramble memory when using inband copying because
            // the copy won't have happened yet.
            if (isInband)
            {
                ErrPrintf("Policy.SetScrambleMovedSurfaces can't be used with Policy.SetInband.\n");
                return RC::SOFTWARE_ERROR;
            }

            // Use static data for now, and maybe a fancypicker later
            //
            GpuDevice *pGpuDevice = GetMdiagSurf()->GetGpuDev();
            vector<UINT08> scrambleData(4);
            scrambleData[0] = 0x0b;
            scrambleData[1] = 0xad;
            scrambleData[2] = 0xbe;
            scrambleData[3] = 0xef;
            for (UINT32 ii = 0; ii < pGpuDevice->GetNumSubdevices(); ++ii)
            {
                CHECK_RC(WritePhys(&scrambleData,
                                   pGpuDevice->GetSubdevice(ii)));
            }
            if (DeferTlbIlwalidate)
            {
                pGilder->KeepRange(*this);
            }
            break;
        }
        default:
            if (DeferTlbIlwalidate || isInband)
            {
                // Preserve the old phys mem until the end of test.
                // Otherwise, we'll delete it right after the move if the
                // ref-count drops to 0, which makes RM ilwalidate the TLBs.
                //
                pGilder->KeepRange(*this);
            }
            break;
    }
    return rc;
}

//--------------------------------------------------------------------
//! \brief Remap the page table entries for the alias destination
// to point to the physical memory lwrrently mapped to the alias source.
//!
//! \param sourceSubsurface The subsurface of the alias source
//! \param srcRange The mem range of the alias source
//! \param dstRange The mem range of the alias destination
//! \param size The size of memory to be aliased
//!
//! This function is called by one of actions that aliases surfaces.
//! The function will modify the PTEs of the destination memory range
//! so that they point to the same physical memory addresses that the PTEs
//! of the source memory range point to.
//!
//! Note that the source and destination memory ranges can come from the
//! same surface.
//
/* static */RC PmMemRange::AliasPhysMem
(
    const PmSubsurface *sourceSubsurface,
    const PmSubsurface *destSubsurface,
    UINT64 aliasSourceOffset,
    UINT64 aliasDestOffset,
    UINT64 size,
    bool deferTlbIlwalidate
)
{
    RC rc;

    // If either offset is not within the corresponding subsurface, there is
    // nothing to alias.  This is not an error, so just return.
    if (aliasSourceOffset >= sourceSubsurface->GetSize())
    {
        return rc;
    }
    else if (aliasDestOffset >= destSubsurface->GetSize())
    {
        return rc;
    }

    // Check to confirm range is inside surface
    if (aliasDestOffset + size > destSubsurface->GetSize())
    {
        size = destSubsurface->GetSize() - aliasDestOffset;
    }
    if (aliasSourceOffset + size > sourceSubsurface->GetSize())
    {
        size = sourceSubsurface->GetSize() - aliasSourceOffset;
    }

    PmMemRange dstRange(destSubsurface->GetSurface(),
        destSubsurface->GetOffset() + aliasDestOffset, size);
    PmMemRange srcRange(sourceSubsurface->GetSurface(),
        sourceSubsurface->GetOffset() + aliasSourceOffset, size);

    PmMemMappingsHelper *pDstMappingsHelper;
    PmMemMappingsHelper *pSrcMappingsHelper;

    pDstMappingsHelper = dstRange.GetSurface()->GetMemMappingsHelper();
    pSrcMappingsHelper = srcRange.GetSurface()->GetMemMappingsHelper();
    PmMemMappings dstMappings = pDstMappingsHelper->SplitMemMappings(&dstRange);
    PmMemMappings srcMappings = pSrcMappingsHelper->SplitMemMappings(&srcRange);

    // determine page aligned range boundaries
    MASSERT( dstMappings[0]->GetPageSize() == srcMappings[0]->GetPageSize() &&
        dstMappings.back()->GetPageSize() == srcMappings.back()->GetPageSize() );

    UINT64 dstPgAlignedStart = ALIGN_DOWN(dstRange.GetGpuAddr(), dstMappings[0]->GetPageSize());
    UINT64 dstPgAlignedEnd = ALIGN_UP(dstRange.GetGpuAddr() + size, dstMappings.back()->GetPageSize());
    UINT64 srcPgAlignedStart = ALIGN_DOWN(srcRange.GetGpuAddr(), srcMappings[0]->GetPageSize());
    UINT64 srcPgAlignedEnd = ALIGN_UP(srcRange.GetGpuAddr() + size, srcMappings.back()->GetPageSize());

    MdiagSurf* pDstMdiagSurf = dstRange.GetSurface()->GetMdiagSurf();
    MdiagSurf* pSrcMdiagSurf = srcRange.GetSurface()->GetMdiagSurf();

    // Check src&dst address range in case they are in same vas
    if (pDstMdiagSurf->GetGpuVASpace() == pSrcMdiagSurf->GetGpuVASpace())
    {
        // check for partial source pages at start or end of surface
        if (srcPgAlignedStart < sourceSubsurface->GetGpuAddr())
        {
            srcPgAlignedStart += srcMappings[0]->GetPageSize();
            dstPgAlignedStart += dstMappings[0]->GetPageSize();
        }
        if (srcPgAlignedEnd > sourceSubsurface->GetGpuAddr() + sourceSubsurface->GetSize())
        {
            srcPgAlignedEnd -= srcMappings.back()->GetPageSize();
            dstPgAlignedEnd -= dstMappings.back()->GetPageSize();
        }

        // check for overlapping src & dst
        if (dstPgAlignedStart >= srcPgAlignedStart && dstPgAlignedStart < srcPgAlignedEnd)
        {
            // dst starts inside src
            dstPgAlignedEnd -= srcPgAlignedEnd - dstPgAlignedStart;
            srcPgAlignedEnd -= srcPgAlignedEnd - dstPgAlignedStart;
        }
        else if (srcPgAlignedStart >= dstPgAlignedStart && srcPgAlignedStart < dstPgAlignedEnd)
        {
            // src starts inside dst
            srcPgAlignedEnd -= dstPgAlignedEnd - srcPgAlignedStart;
            dstPgAlignedEnd -= dstPgAlignedEnd - srcPgAlignedStart;
        }
        if (srcPgAlignedStart == srcPgAlignedEnd)
        {
            // the source and destination already match
            return rc;
        }
    }

    // Get potentially modified mappings
    dstRange.Set(dstRange.GetSurface(), dstRange.GetOffset() - (dstRange.GetGpuAddr() - dstPgAlignedStart),
        dstPgAlignedEnd - dstPgAlignedStart);
    srcRange.Set(srcRange.GetSurface(), srcRange.GetOffset() - (srcRange.GetGpuAddr() - srcPgAlignedStart),
        srcPgAlignedEnd - srcPgAlignedStart);

    pDstMappingsHelper = dstRange.GetSurface()->GetMemMappingsHelper();
    pSrcMappingsHelper = srcRange.GetSurface()->GetMemMappingsHelper();
    dstMappings = pDstMappingsHelper->SplitMemMappings(&dstRange);
    srcMappings = pSrcMappingsHelper->SplitMemMappings(&srcRange);

    PmMemMappings::iterator pDstMapping = dstMappings.begin();
    PmMemMappings::iterator pSrcMapping = srcMappings.begin();
    UINT64 dstBaseOffset = (*pDstMapping)->GetOffset();
    UINT64 srcBaseOffset = (*pSrcMapping)->GetOffset();
    // add the indent to take into the src to account for a partial destination page
    srcBaseOffset += (*pDstMapping)->GetGpuAddr() - ALIGN_DOWN((*pDstMapping)->GetGpuAddr(), (*pDstMapping)->GetPageSize());
    while( pSrcMapping != srcMappings.end() &&
        pDstMapping != dstMappings.end() )
    {
        // TODO: Add support for differing page sizes
        MASSERT( (*pSrcMapping)->GetPageSize() == (*pDstMapping)->GetPageSize() );

        UINT64 nxtSrcOffset = ((*pSrcMapping)->GetOffset() + (*pSrcMapping)->GetSize());
        UINT64 nxtDstOffset = ((*pDstMapping)->GetOffset() + (*pDstMapping)->GetSize());

        // If a boundary in the srcMapping falls before the next dstMapping boundary,
        // we must split the dstMapping in order to remap it properly
        if( nxtDstOffset - dstBaseOffset > nxtSrcOffset - srcBaseOffset )
        {
            // Split dest mapping at the next src offset
            size_t dstMappingIdx = pDstMapping - dstMappings.begin();
            PmMemMappingsHelper *pPmMemMappingsHelper;
            pPmMemMappingsHelper = (*pDstMapping)->GetSurface()->GetMemMappingsHelper();
            dstMappings.insert(pDstMapping, pPmMemMappingsHelper->
                SplitMapping(dstBaseOffset + (nxtSrcOffset - srcBaseOffset)));
            pDstMapping = dstMappings.begin() + dstMappingIdx;
            return rc;
        }

        // remap page table
        //
        PmMemRange srcSubRange( (*pSrcMapping)->GetSurface(),
            srcBaseOffset + ( (*pDstMapping)->GetOffset() - dstBaseOffset ),
            (*pDstMapping)->GetSize() );
        (*pDstMapping)->ModifyPtes((*pSrcMapping)->GetPteFlags(), 0xFFFFFFFF,
            &srcSubRange, deferTlbIlwalidate, 0,
            (*pDstMapping)->GetMemAttrs()->GetActivePteLevel());

        // increment one or both iterators to continue in contiguous order
        pDstMapping++;
        if( nxtDstOffset - dstBaseOffset >= nxtSrcOffset - srcBaseOffset )
        {
            pSrcMapping++;
        }
    }

    // Clean up remapped mappings by joining where possible
    PmMemMappingsHelper *pPmMemMappingsHelper;
    pPmMemMappingsHelper = dstMappings[0]->GetSurface()->GetMemMappingsHelper();
    pPmMemMappingsHelper->JoinMemMappings(&dstRange);

    return rc;
}

//--------------------------------------------------------------------
//! \brief print PmMemRange
//!
ostream& operator<< (ostream& os, const PmMemRange& pmMemRange)
{
    os << "PmMemRange(surface name = " << pmMemRange.GetSurface()->GetMdiagSurf()->GetName();

    // if surface is created by policy manager, the test will be null
    if (pmMemRange.GetSurface()->GetTest())
    {
        os << ", test id = " << pmMemRange.GetSurface()->GetTest()->GetTestId()
           << ", test name = " << pmMemRange.GetSurface()->GetTest()->GetName();
    }

    os  << ", surface base VA(local) = 0x" << hex << pmMemRange.GetMdiagSurf()->GetCtxDmaOffsetGpu()
        << ", offset = 0x" << pmMemRange.GetOffset()
        << ", size = 0x" << pmMemRange.GetSize() << ")" << dec;

    return os;
}

//--------------------------------------------------------------------
//! \brief constructor
//!
PmSubsurface::PmSubsurface
(
    PmSurface *pSurface,
    UINT64 Offset,
    UINT64 Size,
    const string &Name,
    const string &TypeName
) :
    PmMemRange(pSurface, Offset, Size),
    m_Name(Name),
    m_TypeName(TypeName)
{
    MASSERT(pSurface != NULL);
    GetSurface()->AddChildRef();
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmSubsurface::~PmSubsurface()
{
    GetSurface()->ReleaseChild();
}

//--------------------------------------------------------------------
//! \brief constructor
//!
PmMemMapping::PmMemMapping
(
    PmSurface *pSurface,
    UINT64 Offset,
    UINT64 Size,
    LwRm::Handle hMem,
    UINT64 MemOffset,
    UINT64 PhysAddr,
    MappingType Type,
    PmMemMappingsHelper *pOwnerMemMappingsHelper,
    const MemAttrs* pMemAttrs
) :
    PmMemRange(pSurface, Offset, Size),
    m_PhysRange(pSurface, Offset, Size),
    m_hMem(hMem),
    m_MemOffset(MemOffset),
    m_PhysAddr(PhysAddr),
    m_MappingType(Type),
    m_MemMappingsHelper(NULL),
    m_OwnerMemMappingsHelper(pOwnerMemMappingsHelper)
{
    MASSERT(pSurface != NULL);
    MASSERT(hMem != 0);
    MASSERT(pMemAttrs->GetActivePageSize() != 0);
    MASSERT((pMemAttrs->GetActivePageSize() &
            (pMemAttrs->GetActivePageSize() - 1)) == 0);
    MASSERT(pMemAttrs != NULL);
    GetSurface()->AddChildRef();
    m_PhysRange.GetSurface()->AddChildRef();

    MASSERT(pMemAttrs);
    m_MemAttrs.reset(pMemAttrs->Clone(Offset));
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmMemMapping::~PmMemMapping()
{
    UnmapAll();
    if (m_PhysRange.GetSurface() == GetSurface())
        m_PhysRange.GetSurface()->ReleaseChild();
    GetSurface()->ReleaseChild();

    if (m_MemMappingsHelper)
    {
        m_MemMappingsHelper->Release();

        if (!m_MemMappingsHelper->GetRefs())
            delete m_MemMappingsHelper;
    }
}

//--------------------------------------------------------------------
//! \brief MemAttr will be used to replace PteFlags
//!        This function will be removed finally
UINT32 PmMemMapping::GetPteFlags()  const
{
    MemAttrs *pMemAttrs = GetMemAttrs();

    return pMemAttrs? pMemAttrs->GetPteFlags() : 0;
}

//--------------------------------------------------------------------
//! \brief Get Gpu virtual address for this mapping
//!
UINT64 PmMemMapping::GetVirtAddr()  const
{
    return m_OwnerMemMappingsHelper->GetVirtAddr(GetOffset());
}

//--------------------------------------------------------------------
//! \brief Get active page size in user view
//!
UINT64 PmMemMapping::GetPageSize()  const
{
    MemAttrs *pMemAttrs = GetMemAttrs();

    return pMemAttrs? pMemAttrs->GetActivePageSize() : 0;
}

//--------------------------------------------------------------------
//! \brief Get a CPU address for this mapping on the specified subdevice
//!
RC PmMemMapping::GetAddress(GpuSubdevice *pSubdev, void **ppAddress)
{
    RC rc;

    UINT32 flags = 0;
    UINT32 flagsMask = DRF_SHIFTMASK(LWOS33_FLAGS_MAPPING);
    LwRm* pLwRm = GetSurface()->GetLwRmPtr();

    switch(GetSurface()->GetCpuMappingMode())
    {
    case PolicyManager::CpuMapDirect:
        flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DIRECT, flags);
        break;

    case PolicyManager::CpuMapReflected:
        flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _REFLECTED, flags);
        break;

    case PolicyManager::CpuMapDefault:
    default:
         flags = FLD_SET_DRF(OS33, _FLAGS, _MAPPING, _DEFAULT, flags);
         break;
    }

    if (m_SubdevAddrs.count(pSubdev) != 0)
    {
        UINT32 lwrrMappingFlags = m_SubdevAddrs[pSubdev].CpuMappingFlags;
        if ((lwrrMappingFlags & flagsMask) == flags)
        {
            // Existed mapping has the required flags
            *ppAddress = m_SubdevAddrs[pSubdev].CpuAddress;

            return OK;
        }
        else
        {
            // Unmap existed mapping and map a new one below
            pLwRm->UnmapMemory(m_hMem,
                                m_SubdevAddrs[pSubdev].CpuAddress,
                                m_SubdevAddrs[pSubdev].CpuMappingFlags,
                                pSubdev);

            // erase it to avoid stale data in case MapMemory fails.
            m_SubdevAddrs.erase(pSubdev);
        }
    }

    CHECK_RC(pLwRm->MapMemory(m_hMem,
                                m_MemOffset,
                                GetSize(),
                                ppAddress,
                                flags,
                                pSubdev));

    m_SubdevAddrs[pSubdev].CpuAddress = *ppAddress;
    m_SubdevAddrs[pSubdev].CpuMappingFlags = flags;

    return rc;

}

//--------------------------------------------------------------------
//! \brief Change PageSize of the mapping;
//!        2m -> 4k/64k: modify pte to pde0 to point to 5th level
//!        4k/64k -> 2m: modify pde0 to pte, save 5th level
RC PmMemMapping::ChangePageSize
(
    UINT64 pageSize,
    PmChannel *pInbandChannel,
    bool deferTlbIlwalidate
)
{
    RC rc;

    if (GetSize() % pageSize != 0)
    {
        // split does not happen on page boundary
        return RC::SOFTWARE_ERROR;
    }

    UINT64 targetPageSize = pageSize;
    UINT64 sourcePageSize = GetPageSize();

    if (sourcePageSize == targetPageSize)
    {
        return rc; // nothing to do
    }

    MmuLevel::EntryOps entryOps;
    entryOps.MmuEntryOps = 0;
    MmuLevel* pTargetMmuPte = NULL;
    CHECK_RC(GetSurface()->GetMmmuPteLevel(targetPageSize, &pTargetMmuPte));

    // 1. copy phys address from current active pte
    MmuLevelTree *pMmuLevelTree = GetSurface()->GetMmuLevelTree();
    CHECK_RC(pMmuLevelTree->GetSurfacePhysAddr(GetOffset(),
        GetPageSize(), &entryOps.EntryPhysAddr, &entryOps.EntryAperture));
    entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_CONTI_PA;

    // 2. copy src attrs to dst
    MmuLevelTree::LevelIndex srcLevel = m_MemAttrs->GetActivePteLevel();

    bool bValid = false;
    if (OK == m_MemAttrs->IsPageAttrValid(srcLevel, &bValid))
    {
        if (bValid)
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_VALID;
        }
        else
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_VALID;
        }
    }

    bool bCachable = false;
    if (OK == m_MemAttrs->IsPageAttrCache(srcLevel, &bCachable))
    {
        if (bCachable)
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_CACHE;
        }
        else
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_CACHE;
        }
    }

    bool bSparse = false;
    if (OK == m_MemAttrs->IsPageAttrSparse(srcLevel, &bSparse))
    {
        if (bSparse)
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_SPARSE;
        }
        else
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_SPARSE;
        }
    }

    bool readOnly = false;
    if (OK == m_MemAttrs->IsPageAttrRO(srcLevel, &readOnly))
    {
        if (readOnly)
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_ACCESSRO;
        }
        else
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_ACCESSRO;
        }
    }

    bool atomicDisable = false;
    if (OK == m_MemAttrs->IsPageAttrAtomicDisable(srcLevel, &atomicDisable))
    {
        if (atomicDisable)
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_ATOMICDISABLE;
        }
        else
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_ATOMICDISABLE;
        }
    }

    bool bPrivEnable = false;
    if (OK == m_MemAttrs->IsPageAttrPriv(srcLevel, &bPrivEnable))
    {
        if (bPrivEnable)
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_PRIV;
        }
        else
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_PRIV;
        }
    }

    //
    // 3. Activate targeted PageSize PTE
    m_MemAttrs->SetActivePageSize(targetPageSize);

    //
    // 4. Connect/disconnect last level
    bool deactiveSrcLevel = false;
    if (targetPageSize == PolicyManager::BYTES_IN_HUGE_PAGE)
    {
        //
        // 4k/64k -> 2m: 5 levels -> 4 levels
        deactiveSrcLevel = true; // deactive 4k/64k

        // Change PDE0 to PTE type
        entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_CHANGETYPE_TOPTE;
    }
    else if (sourcePageSize == PolicyManager::BYTES_IN_HUGE_PAGE)
    {
        //
        // 2m -> 4k/64k: 4 levels -> 5 levels
        // Connect both 4k and 64k PTE
        for (UINT32 type = PolicyManager::BIG_PAGE_SIZE;
             type <= PolicyManager::SMALL_PAGE_SIZE; ++type)
        {
            MmuLevel::MmuSubLevelIndex subLevel =
                MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_SMALL;

            if (type == PolicyManager::BIG_PAGE_SIZE)
            {
                subLevel = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_BIG;
            }

            MmuLevel* pPTE2M = NULL;
            CHECK_RC(GetSurface()->GetMmuLevel(MmuLevelTree::GMMU_LEVEL_PTE_2M, &pPTE2M));

            MmuLevel::EntryOps connectOp;
            connectOp.MmuEntryOps =
                MmuLevel::EntryOps::MMUENTRY_OP_CHANGETYPE_TOPDE |
                MmuLevel::EntryOps::MMUENTRY_OP_CONNECTLOWER;
            CHECK_RC(dynamic_cast<PmMmuLevel *>(pPTE2M)->ModifyMmuEntries(
                subLevel, pInbandChannel, this, connectOp));
        }
    }
    else
    {
        // 4k <-> 64k:  5 levels unchanged
        deactiveSrcLevel = true; // deactive opposite page size
    }

    //
    // Activate the PTE for targeted PageSize
    // 5. modify mmu entris and download
    CHECK_RC(dynamic_cast<PmMmuLevel *>(pTargetMmuPte)->ModifyMmuEntries(
        MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT,
        pInbandChannel, this, entryOps));

    // 6. Set Aperture
    CHECK_RC(dynamic_cast<PmMmuLevel *>(pTargetMmuPte)->SetAperture(
        MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT, pInbandChannel, this));

    // 7. deactive orignal src level if it's mapped
    if (deactiveSrcLevel && bValid)
    {
        MmuLevel::EntryOps unmapOps;
        MmuLevel* pMmuLevel = NULL;
        CHECK_RC(pMmuLevelTree->GetMmuLevel(srcLevel, &pMmuLevel));

        unmapOps.MmuEntryOps = MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_VALID;

        CHECK_RC(dynamic_cast<PmMmuLevel *>(pMmuLevel)->ModifyMmuEntries(
            MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_DEFAULT,
            pInbandChannel, this, unmapOps));
    }

    // To be compatible with other mmu entry actions
    // Only out-of-band has defered tlbIlwalidate
    //
    if (!pInbandChannel && !deferTlbIlwalidate)
    {
        LwRm* pLwRm = GetSurface()->GetLwRmPtr();
        LW2080_CTRL_DMA_ILWALIDATE_TLB_PARAMS params = {};
        params.hVASpace = GetFillPteTgtHandle();
        GpuDevice *pGpuDevice = GetMdiagSurf()->GetGpuDev();

        for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
        {
            CHECK_RC(pLwRm->ControlBySubdevice(
                pGpuDevice->GetSubdevice(subdev),
                LW2080_CTRL_CMD_DMA_ILWALIDATE_TLB,
                &params, sizeof(params)));
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Split a PmMemMapping at a page boundary
//!
//! This method takes a PmMemMapping, and splits it in two at VirtAddr.
//! VirtAddr must occur at a page boundary within the range of the
//! PmMemMapping.  After the split, this object will contain the part
//! of the PmMemMapping that came after VirtAddr, and the return value
//! will by a new PmMemMapping (allocated with "new") that contains
//! the part of the PmMemMapping that came before VirtAddr.
//!
PmMemMapping *PmMemMapping::Split(UINT64 VirtAddr)
{
    MASSERT(VirtAddr > GetVirtAddr());
    MASSERT(VirtAddr < GetVirtAddr() + GetSize());
    MASSERT((VirtAddr & (GetPageSize() - 1)) == 0);

    // Callwlate the offsets, sizes, etc. of the two new PmMemMappings
    //
    UINT64 offset1 = GetOffset();
    UINT64 size1   = VirtAddr - GetVirtAddr();
    UINT64 offset2 = offset1 + size1;
    UINT64 size2   = GetSize() - size1;

    // Unmap any existing CPU addresses for this PmMemMapping since splitting
    // will ilwalidate these mappings
    UnmapAll();

    // Create the return value
    //
    PmMemMapping *pNewMapping = new PmMemMapping(GetSurface(), offset1, size1,
                                                 m_hMem, m_MemOffset,
                                                 m_PhysAddr, m_MappingType,
                                                 m_OwnerMemMappingsHelper,
                                                 m_MemAttrs.get());
    pNewMapping->SetPhysRange(m_PhysRange.GetSurface(),
                              m_PhysRange.GetOffset(), size1);

    // Modify this object to not include the region before the split
    //
    Set(GetSurface(), offset2, size2);
    SetPhysRange(m_PhysRange.GetSurface(),
                 m_PhysRange.GetOffset() + size1, size2);
    m_MemOffset += size1;
    m_PhysAddr += size1;
    m_MemAttrs->SetOffset(GetOffset()); // update offset in MemAttrs

    // Split bottom memmappings if it exists
    //
    if (m_MemMappingsHelper)
    {
        PmMemMappingsHelper *pSubMappingHelper;
        pSubMappingHelper = m_MemMappingsHelper->Split(offset1, size1,
                                                       pNewMapping);
        pNewMapping->SetMemMappingsHelper(pSubMappingHelper);

        // up layer phys address is the virtual address of sublayer
        m_MemMappingsHelper->SetBaseVirtAddr(m_PhysAddr);
    }

    return pNewMapping;
}

//--------------------------------------------------------------------
//! \brief Join two PmMemMappings, if possible
//!
//! This method takes two PmMemMappings, and joins them into one if
//! they are contiguous and have the same memory handle, pte flags,
//! page size, and so on.
//!
//! This method is usually used to undo the Split() method.
//!
//! \param pMemMapping The PmMemMapping that we will attempt to join
//!     with this object.  *pMemMapping and *this should be
//!     contiguous, but they can occur in either order.  On success,
//!     this method calls "delete" on pMemMapping.
//! \return true if the join succeeded, false if not.
//!
bool PmMemMapping::Join(PmMemMapping *pMemMapping)
{
    MASSERT(pMemMapping != NULL);

    // Set pMap1 and pMap2 to the objects that come first and second,
    // respectively.
    //
    bool thisComesFirst = (GetOffset() < pMemMapping->GetOffset());
    PmMemMapping *pMap1 = (thisComesFirst ? this : pMemMapping);
    PmMemMapping *pMap2 = (thisComesFirst ? pMemMapping : this);

    // Return false if the two objects cannot be joined
    //
    if (pMap1->GetSurface() != pMap2->GetSurface() ||
        pMap1->GetOffset() + pMap1->GetSize() != pMap2->GetOffset() ||
        pMap1->m_hMem != pMap2->m_hMem ||
        pMap1->m_MemOffset + pMap1->GetSize() != pMap2->m_MemOffset ||
        pMap1->m_PhysAddr + pMap1->GetSize() != pMap2->m_PhysAddr)
    {
        return false;
    }

    if (pMap1->GetMemAttrs() &&
        pMap2->GetMemAttrs())
    {
        if (!pMap1->GetMemAttrs()->IsSameMemAttrs(pMap2->GetMemAttrs()))
            return false;
    }
    else if (pMap1->GetMemAttrs() || pMap2->GetMemAttrs())
    {
        return false;
    }

    // If we got past the above check, then the virtual and physical
    // memory sohuld be contiguous, and the following assertions
    // should automatically succeed.
    //
    MASSERT(pMap1->m_PhysRange.GetSurface() ==
            pMap2->m_PhysRange.GetSurface());
    MASSERT(pMap1->m_PhysRange.GetOffset() + pMap1->m_PhysRange.GetSize() ==
            pMap2->m_PhysRange.GetOffset());

    // Update this object.  Be careful during the update, since
    // changing this object will also change either pMap1 or pMap2.
    //
    UINT64 newSize = pMap1->GetSize() + pMap2->GetSize();

    // Unmap any existing CPU addresses for this PmMemMapping since joining
    // will ilwalidate these mappings
    UnmapAll();

    Set(pMap1->GetSurface(), pMap1->GetOffset(), newSize);
    SetPhysRange(pMap1->m_PhysRange.GetSurface(),
                 pMap1->m_PhysRange.GetOffset(), newSize);
    m_MemOffset = pMap1->m_MemOffset;
    m_PhysAddr = pMap1->m_PhysAddr;

    // Join bottom memmappings if it exists
    //
    if (m_MemMappingsHelper)
    {
        m_MemMappingsHelper->Join(pMemMapping->GetMemMappingsHelper());
    }

    return true;
}

//--------------------------------------------------------------------
//! \brief Use LW0080_CTRL_CMD_DMA_FILL_PTE_MEM to modify the PTEs
//!
//! \param PteFlags LW0080_CTRL_CMD_DMA_FILL_PTE_MEM flags to set in the
//!     PTEs.
//! \param PteMask Mask of LW0080_CTRL_CMD_DMA_FILL_PTE_MEM flags that
//!     1 means using PTE bit from dst, and 0 means from src.
//!     (PteFlags & ~PteMask) must be 0.
//! \param pSrcRange Used to get the physical pages for the PTEs.  The
//!     PTEs will be pointed at whatever pages pSrcRange is lwrrently
//!     mapped to.  The caller is required to make sure that pSrcRange
//!     corresponds to a single PmMemMapping in the donor surface, and
//!     that pSrcRange has the same size as this PmMemMapping.
//! \param DeferTlbIlwalidate adds
//!     LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_DEFER_ILWALIDATE_TRUE
//!     to flags when calling LW0080_CTRL_CMD_DMA_FILL_PTE_MEM.
//!
RC PmMemMapping::ModifyPtes
(
    UINT32 PteFlags,
    UINT32 PteMask,
    const PmMemRange *pSrcRange,
    bool DeferTlbIlwalidate,
    PmChannel *pInbandChannel,
    MmuLevelTree::LevelIndex LevelNum
)
{
    MASSERT((PteFlags & ~PteMask) == 0);
    MASSERT(pSrcRange != NULL);
    MASSERT(pSrcRange->GetSize() == GetSize());

    RC rc;

    // Get the src PmMemMapping
    //
    PmMemMappingsHelper *pSrcMappingsHelper;
    PmMemMappings srcMemMappings;
    pSrcMappingsHelper = pSrcRange->GetSurface()->GetMemMappingsHelper();
    srcMemMappings = pSrcMappingsHelper->GetMemMappings(pSrcRange);
    PmMemMapping *pSrcMemMapping = srcMemMappings[0];

    MASSERT(srcMemMappings.size() == 1);
    MASSERT(pSrcRange->GetOffset() >= pSrcMemMapping->GetOffset());
    MASSERT(pSrcRange->GetOffset() + pSrcRange->GetSize() <=
            pSrcMemMapping->GetOffset() + pSrcMemMapping->GetSize());

    // Modifying the PTES will ilwalidate any existing CPU addresses
    // for this PmMemMapping
    UnmapAll();

    const MemAttrs *pMemAttrs = GetMemAttrs();
    MASSERT(pMemAttrs);

    // If the page-size will change, and the current PTE is valid,
    // then call LW0080_CTRL_CMD_DMA_FILL_PTE_MEM to ilwalidate it
    // before we write the new PTE.
    //
    bool bValid = false;
    if (GetPageSize() != pSrcMemMapping->GetPageSize() &&
        (OK == pMemAttrs->IsPageAttrValid(LevelNum, &bValid)) &&
        bValid)
    {
        UINT32 IlwalidPteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                                             _FLAGS, _VALID, _FALSE,
                                             GetPteFlags());
        UINT32 PteMask =
            DRF_SHIFTMASK(LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID);

        if (GetSurface()->IsMmuLevelFmtSupported())
        {
            CHECK_RC(WriteMmuLevelEntries(IlwalidPteFlags, PteMask, false,
                         GetPageSize(), DeferTlbIlwalidate,
                         pInbandChannel, LevelNum));
        }
        else
        {
            CHECK_RC(WritePtes(m_hMem, m_MemOffset, IlwalidPteFlags,
                         m_PhysAddr, GetPageSize(), DeferTlbIlwalidate,
                         pInbandChannel));
        }
    }

    UINT64 srcOffsetInMapping =
           pSrcRange->GetOffset() - pSrcMemMapping->GetOffset();

    bool bCopySrcPhysAddr = false;
    UINT32 dstPteFlags = PteFlags;
    MmuLevelTree::LevelIndex targetLevel = LevelNum;
    if (bModifyPteEntries(LevelNum))
    {
        // Copy all physical-memory fields from pSrcRange to this object.
        //
        SetPhysRange(pSrcMemMapping->m_PhysRange.GetSurface(),
                     pSrcMemMapping->m_PhysRange.GetOffset() + srcOffsetInMapping,
                     pSrcRange->GetSize());
        m_hMem      = pSrcMemMapping->m_hMem;
        m_MemOffset = pSrcMemMapping->m_MemOffset + srcOffsetInMapping;
        dstPteFlags = (pSrcMemMapping->GetPteFlags() & ~PteMask) | PteFlags;
        m_MappingType = pSrcMemMapping->m_MappingType;
        if (pSrcMemMapping->m_PhysAddr + srcOffsetInMapping != m_PhysAddr)
        {
            m_PhysAddr  = pSrcMemMapping->m_PhysAddr + srcOffsetInMapping;
            bCopySrcPhysAddr = true;
        }

        if (GetPageSize() != pSrcMemMapping->GetPageSize())
        {
            m_MemAttrs->SetActivePageSize(pSrcMemMapping->GetPageSize());
            targetLevel = m_MemAttrs->GetActivePteLevel();
        }

        MASSERT((m_PhysAddr & (GetPageSize() - 1)) ==
            (GetVirtAddr() & (GetPageSize() - 1)));
    }

    if (GetSurface()->IsMmuLevelFmtSupported())
    {
        // Call new MMU manipulation API
        CHECK_RC(WriteMmuLevelEntries(dstPteFlags, PteMask,
            bCopySrcPhysAddr, GetPageSize(), DeferTlbIlwalidate,
            pInbandChannel, targetLevel));
    }
    else
    {
        // Call LW0080_CTRL_CMD_DMA_FILL_PTE_MEM to set the mapping.
        //
        CHECK_RC(WritePtes(m_hMem, m_MemOffset, dstPteFlags,
                 m_PhysAddr, GetPageSize(), DeferTlbIlwalidate, pInbandChannel));
    }

    return rc;
}

//--------------------------------------------------------------------
//! Public method to set m_PhysRange and update the surface's ref counts.
//!
void PmMemMapping::SetPhysRange
(
    PmSurface *pSurface,
    UINT64 Offset,
    UINT64 Size
)
{
    MASSERT(pSurface != NULL);

    if (m_PhysRange.GetSurface() == GetSurface())
        m_PhysRange.GetSurface()->ReleaseChild();

    m_PhysRange.Set(pSurface, Offset, Size);

    if (m_PhysRange.GetSurface() == GetSurface())
        m_PhysRange.GetSurface()->AddChildRef();
}

RC PmMemMapping::WriteMmuLevelEntries
(
    UINT32 PdeFlags,
    UINT32 dstPdeMask,
    bool bCopySrcPhysAddr,
    UINT64 PageSize,
    bool deferTlbIlwalidate,
    PmChannel *pInbandChannel,
    MmuLevelTree::LevelIndex LevelNum
)
{
    RC rc;
    MmuLevel *pMmuLevel;
    CHECK_RC(GetSurface()->GetMmuLevel(LevelNum, &pMmuLevel));

    MmuLevel::MmuSubLevelIndex subLevelIndex;
    if(LevelNum == MmuLevelTree::GMMU_LEVEL_PDE0 &&
       PageSize == PolicyManager::BYTES_IN_SMALL_PAGE)
    {
        subLevelIndex = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_SMALL;
    }
    else
    {
        subLevelIndex = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_BIG;
    }

    // To do:
    //   Use new Action flags to replace the FILL_PTE_MEM flags
    // set valid bit
    MmuLevel::EntryOps entryOps;
    entryOps.MmuEntryOps = 0;

    if (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID, dstPdeMask) != 0)
    {
        if (FLD_TEST_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                         _FLAGS, _VALID, _TRUE, PdeFlags))
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_VALID;
        }
        else
        {
            entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_VALID;
        }
    }

    // set volatile bit
    if (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _SPARSE, dstPdeMask) != 0)
    {
        if (FLD_TEST_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                         _FLAGS, _SPARSE, _TRUE, PdeFlags))
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_SET_SPARSE;
        }
        else
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_SPARSE;
        }
    }

    if (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _GPU_CACHED, dstPdeMask) != 0)
    {
        if (FLD_TEST_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                         _FLAGS, _GPU_CACHED, _TRUE, PdeFlags))
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_SET_CACHE;
        }
        else
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_CACHE;
        }
    }

    if (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _ATOMIC_DISABLE, dstPdeMask) != 0)
    {
        if (FLD_TEST_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                         _FLAGS, _ATOMIC_DISABLE, _TRUE, PdeFlags))
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_SET_ATOMICDISABLE;
        }
        else
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_ATOMICDISABLE;
        }
    }

    if (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _READ_ONLY, dstPdeMask) != 0)
    {
        if (FLD_TEST_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                         _FLAGS, _READ_ONLY, _TRUE, PdeFlags))
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_SET_ACCESSRO;
        }
        else
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_ACCESSRO;
        }
    }

    if (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _PRIV, dstPdeMask) != 0)
    {
        if (FLD_TEST_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                         _FLAGS, _PRIV, _TRUE, PdeFlags))
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_SET_PRIV;
        }
        else
        {
            entryOps.MmuEntryOps |=  MmuLevel::EntryOps::MMUENTRY_OP_CLEAR_PRIV;
        }
    }

    // set src aperture - physaddr
    if (bCopySrcPhysAddr ||
        DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _APERTURE, dstPdeMask) != 0)
    {
        entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_CONTI_PA;
        entryOps.EntryPhysAddr = m_PhysAddr;

        switch (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _APERTURE, PdeFlags))
        {
        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_VIDEO_MEMORY:
            entryOps.EntryAperture = GMMU_APERTURE_VIDEO;
            break;
        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_PEER_MEMORY:
            entryOps.EntryAperture = GMMU_APERTURE_PEER;
            break;
        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_COHERENT_MEMORY:
            entryOps.EntryAperture = GMMU_APERTURE_SYS_COH;
            break;
        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_NON_COHERENT_MEMORY:
            entryOps.EntryAperture = GMMU_APERTURE_SYS_NONCOH;
            break;
        default:
            MASSERT(!"Should not hit here");
        }
    }

    CHECK_RC(dynamic_cast<PmMmuLevel *>(pMmuLevel)->ModifyMmuEntries(subLevelIndex,
             pInbandChannel, this, entryOps));

    // To be compatible with orignal behavior
    // Only out-of-band has defered tlbIlwalidate
    //
    if (!pInbandChannel && !deferTlbIlwalidate)
    {
        LwRm* pLwRm = GetSurface()->GetLwRmPtr();
        LW2080_CTRL_DMA_ILWALIDATE_TLB_PARAMS Params = {0};
        Params.hVASpace = GetFillPteTgtHandle();
        GpuDevice *pGpuDevice = GetMdiagSurf()->GetGpuDev();

        for (UINT32 subdev = 0; subdev < pGpuDevice->GetNumSubdevices(); ++subdev)
        {
            CHECK_RC(pLwRm->ControlBySubdevice(
                pGpuDevice->GetSubdevice(subdev),
                LW2080_CTRL_CMD_DMA_ILWALIDATE_TLB,
                &Params, sizeof(Params)));
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Thin wrapper around LW0080_CTRL_CMD_DMA_FILL_PTE_MEM
//!
//! This method uses LW0080_CTRL_CMD_DMA_FILL_PTE_MEM to modify the
//! PTEs.  Unlike ModifyPtes (which calls this method), this method
//! does not attempt to record the new information in the
//! PolicyManager data structs, or modify only certain bits, or
//! anything like that; it just writes the data verbatim.
//!
//! The virtual address and size are derived from *this.  Other
//! fields, which tend to change in ModifyPtes, are set in the arg
//! list.
//!
//! \param hMem Memory handle of the physical memory allocation
//!        from which to extract the kind and partition stride attributes.
//! \param MemOffset Offset into hMem for the first PTE we'll write
//! \param PteFlags New PTE flags
//! \param PhysAddr Physical address to write into the first PTE.
//!     This routine rounds down to the nearest page.  Subsequent
//!     pages are assumed to be contiguous.
//! \param PageSize The page size, in bytes, of the PTE to modify.  In
//!     DualPageSize surfaces, ay be used to modify the invalid PTE.
//! \param DeferTlbIlwalidate adds
//!     LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_DEFER_ILWALIDATE_TRUE
//!     to flags when calling LW0080_CTRL_CMD_DMA_FILL_PTE_MEM.
//!
RC PmMemMapping::WritePtes
(
    LwRm::Handle hMem,
    UINT64 MemOffset,
    UINT32 PteFlags,
    UINT64 PhysAddr,
    UINT64 PageSize,
    bool DeferTlbIlwalidate,
    PmChannel *pInbandChannel
) const
{
    RC rc;

    UINT32 targetPteFlags = PteFlags;
    targetPteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _CONTIGUOUS, _TRUE, targetPteFlags);
    targetPteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _OVERRIDE_PAGE_SIZE, _TRUE, targetPteFlags);
    if (DeferTlbIlwalidate || (pInbandChannel != 0))
    {
        targetPteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                                   _DEFER_ILWALIDATE, _TRUE, targetPteFlags);
    }

    // Gmmu manipulation
    MmuLevel *pMmuLevel = NULL;
    CHECK_RC(GetSurface()->GetMmmuPteLevel(PageSize, &pMmuLevel));

    CHECK_RC(dynamic_cast<V1MmuLevel *>(pMmuLevel)->FillPteMem(hMem, MemOffset, targetPteFlags, PhysAddr,
                GetFillPteSrcHandle(), GetFillPteTgtHandle(), pInbandChannel, this));

    // Save PteFlags
    // Now it's only needed for SMMU
    CHECK_RC(m_MemAttrs->UpdateMemAttrs(PteFlags));

    return rc;
}

//--------------------------------------------------------------------
//! \brief Set PmMemMappingsHelper point to this object
//!        For smmu enabled gpu/shared surface, top layer PmMemMapping
//!        (in gmmu) holds another group of PmMemMappings in gpu-asid smmu
//! \param pMemMappingsHelper New PmMemMappingsHelper point to be set
void PmMemMapping::SetMemMappingsHelper
(
    PmMemMappingsHelper *pMemMappingsHelper
)
{
    if (m_MemMappingsHelper == pMemMappingsHelper)
    {
        return;
    }

    if (m_MemMappingsHelper)
    {
        m_MemMappingsHelper->Release();

        if (!m_MemMappingsHelper->GetRefs())
        {
            delete m_MemMappingsHelper;
            m_MemMappingsHelper = NULL;
        }
    }

    m_MemMappingsHelper = pMemMappingsHelper;
    if (m_MemMappingsHelper)
    {
        m_MemMappingsHelper->AddRef();
    }
}


//--------------------------------------------------------------------
//! Private method to unmap any existing CPU mappings.
//!
void PmMemMapping::UnmapAll()
{
    map<GpuSubdevice *, CpuMappingInfo>::iterator pMapping = m_SubdevAddrs.begin();
    LwRm* pLwRm = GetSurface()->GetLwRmPtr();

    for ( ; pMapping != m_SubdevAddrs.end(); pMapping++)
    {
        pLwRm->UnmapMemory(m_hMem,
                           pMapping->second.CpuAddress,
                           pMapping->second.CpuMappingFlags,
                           pMapping->first);
    }
    m_SubdevAddrs.clear();
}

//--------------------------------------------------------------------
//! Private method to manipulate SMMU entires through LW0080_CTRL_CMD_DMA_FILL_PTE_MEM.
//!     GMMU is using MmuLevelTree class to manipulate mmu entries
//!     SMMU still need to use old RM API LW0080_CTRL_CMD_DMA_FILL_PTE_MEM
//!
RC PmMemMapping::SmmuOutOfBandWritePtes
(
    LwRm::Handle hMem,
    UINT64 MemOffset,
    UINT32 PteFlags,
    UINT64 PhysAddr,
    UINT64 PageSize,
    PmChannel *pInbandChannel
) const
{
    RC rc;
    LwRm* pLwRm = GetSurface()->GetLwRmPtr();
    GpuDevice *pGpuDevice = GetMdiagSurf()->GetGpuDev();

    // SMMU does not support in-band tlb ilwalidate
    // So it's unreasonable to manipulate mmu entries through in-band methods
    //
    MASSERT(pInbandChannel == NULL);

    LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS params = {0};
    UINT64 pageArray = PhysAddr & ~(PageSize - 1);
    params.pageCount = (LwU32)((ALIGN_UP(GetVirtAddr() + GetSize(), PageSize) -
                                ALIGN_DOWN(GetVirtAddr(), PageSize))
                               / PageSize);
    params.hwResource.hClient = pLwRm->GetClientHandle();
    params.hwResource.hDevice = pLwRm->GetDeviceHandle(pGpuDevice);
    params.hwResource.hMemory = hMem;
    params.offset = MemOffset;
    params.gpuAddr = GetVirtAddr();
    params.pageArray = LW_PTR_TO_LwP64(&pageArray);
    params.hSrcVASpace = GetFillPteSrcHandle();
    params.hTgtVASpace = GetFillPteTgtHandle();
    params.pageSize = (LwU32)PageSize;
    params.flags = PteFlags;
    params.pteMem = 0;

    CHECK_RC(pLwRm->ControlByDevice(pGpuDevice,
                 LW0080_CTRL_CMD_DMA_FILL_PTE_MEM,
                 &params, sizeof(params)));

    return rc;
}

//--------------------------------------------------------------------
//! Private method to return src handle for LW0080_CTRL_CMD_DMA_FILL_PTE_MEM.
//! to identify src address type
//!     return 0 for sysmem phys/carveout address
//!     return gpu-asid smmu handle for smmu-va
LwRm::Handle PmMemMapping::GetFillPteSrcHandle() const
{
    switch (m_MappingType)
    {
    case Gmmu_Default:
    case Gmmu_SysmemPa:
        return 0;
    default:
        MASSERT(!"Invalid memory mapping type");
    }

    return 0;
}

//--------------------------------------------------------------------
//! Private method to return target handle for LW0080_CTRL_CMD_DMA_FILL_PTE_MEM
//! call to identify the target mmu type
//!     gmmu: return 0
//!     smmu: return gpu-asid or cheetah-asid smmu handle
LwRm::Handle PmMemMapping::GetFillPteTgtHandle() const
{
    switch (m_MappingType)
    {
    case Gmmu_Default:
    case Gmmu_SysmemPa:
        return 0;
    default:
        MASSERT(!"Invalid memory mapping type");
    }

    return 0;
}

bool PmMemMapping::bModifyPteEntries
(
    MmuLevelTree::LevelIndex LevelNum
) const
{
    // Target Level is the bottom PTE level
    if(LevelNum == MmuLevelTree::GMMU_LEVEL_PTE_4K  ||
       LevelNum == MmuLevelTree::GMMU_LEVEL_PTE_64K ||
       (LevelNum == MmuLevelTree::GMMU_LEVEL_PDE0
       && GetPageSize() == PolicyManager::BYTES_IN_HUGE_PAGE) ||
       (LevelNum == MmuLevelTree::GMMU_LEVEL_PDE1
       && GetPageSize() == PolicyManager::BYTES_IN_512MB_PAGE))
    {
        return true;
    }

    return false;
}

RC PmMemMapping::SetPhysAddrBits
(
    UINT64 addrValue,
    UINT64 addrMask,
    bool deferTlbIlwalidate,
    PmChannel *inbandChannel
)
{
    RC rc;
    MmuLevel *mmuLevel;
    MmuLevelTree::LevelIndex levelNum = GetMemAttrs()->GetActivePteLevel();
    CHECK_RC(GetSurface()->GetMmuLevel(levelNum, &mmuLevel));

    MmuLevel::MmuSubLevelIndex subLevelIndex;
    if ((levelNum == MmuLevelTree::GMMU_LEVEL_PDE0) &&
        (GetPageSize() == PolicyManager::BYTES_IN_SMALL_PAGE))
    {
        subLevelIndex = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_SMALL;
    }
    else
    {
        subLevelIndex = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_BIG;
    }

    m_PhysAddr = (m_PhysAddr & ~addrMask) | (addrValue & addrMask);

    MmuLevel::EntryOps entryOps;
    entryOps.MmuEntryOps = 0;

    entryOps.MmuEntryOps |= MmuLevel::EntryOps::MMUENTRY_OP_SET_CONTI_PA;
    entryOps.EntryPhysAddr = m_PhysAddr;

    switch (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _APERTURE,
            GetPteFlags()))
    {
        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_VIDEO_MEMORY:
            entryOps.EntryAperture = GMMU_APERTURE_VIDEO;
            break;
        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_PEER_MEMORY:
            entryOps.EntryAperture = GMMU_APERTURE_PEER;
            break;
        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_COHERENT_MEMORY:
            entryOps.EntryAperture = GMMU_APERTURE_SYS_COH;
            break;
        case LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_APERTURE_SYSTEM_NON_COHERENT_MEMORY:
            entryOps.EntryAperture = GMMU_APERTURE_SYS_NONCOH;
            break;
        default:
            MASSERT(!"Illegal PTE aperture flag");
            return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(dynamic_cast<PmMmuLevel *>(mmuLevel)->ModifyMmuEntries(subLevelIndex, inbandChannel,
        this, entryOps));

    if ((inbandChannel == 0) && !deferTlbIlwalidate)
    {
        LwRm* pLwRm = GetSurface()->GetLwRmPtr();
        LW2080_CTRL_DMA_ILWALIDATE_TLB_PARAMS params = {0};
        params.hVASpace = GetFillPteTgtHandle();
        GpuDevice *gpuDevice = GetMdiagSurf()->GetGpuDev();

        for (UINT32 subdev = 0;
             subdev < gpuDevice->GetNumSubdevices();
             ++subdev)
        {
            CHECK_RC(pLwRm->ControlBySubdevice(
                gpuDevice->GetSubdevice(subdev),
                LW2080_CTRL_CMD_DMA_ILWALIDATE_TLB,
                &params, sizeof(params)));
        }
    }

    DebugPrintf("PolicyManager: modified PA bits for surface %s, mask=0x%llx, value=0x%llx\n", 
        GetSurface()->GetMdiagSurf()->GetName().c_str(), addrMask, addrValue);

    return rc;
}

//--------------------------------------------------------------------
//! \brief constructor
//!
MemAttrs::MemAttrs
(
    PmSurface* pPmSurface,
    UINT64 offset,
    UINT64 pageSize
) :
    m_pPmSurface(pPmSurface),
    m_Offset(offset),
    m_PageSize(pageSize)
{
    MASSERT(pPmSurface != NULL);
}

RC MemAttrs::IsPageAttrValid
(
    MmuLevelTree::LevelIndex levelNum,
    bool *pValid
) const
{
    const PageAttrs* pPageAttr = GetPageAttrs(levelNum);

    if (pPageAttr && pPageAttr->m_AttrVal != MEMATTR_ILWALID)
    {
        *pValid = pPageAttr->m_AttrVal == MEMATTR_TRUE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::IsPageAttrCache
(
    MmuLevelTree::LevelIndex levelNum,
     bool *pCachable
) const
{
    const PageAttrs* pPageAttr = GetPageAttrs(levelNum);

    if (pPageAttr && pPageAttr->m_AttrCache != MEMATTR_ILWALID)
    {
        *pCachable = pPageAttr->m_AttrCache == MEMATTR_TRUE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::IsPageAttrSparse
(
    MmuLevelTree::LevelIndex levelNum,
    bool *pSparse
) const
{
    const PageAttrs* pPageAttr = GetPageAttrs(levelNum);

    if (pPageAttr && pPageAttr->m_AttrSparse != MEMATTR_ILWALID)
    {
        *pSparse = pPageAttr->m_AttrSparse == MEMATTR_TRUE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::IsPageAttrRO
(
    MmuLevelTree::LevelIndex levelNum,
    bool *pReadOnly
) const
{
    const PageAttrs* pPageAttr = GetPageAttrs(levelNum);

    if (pPageAttr && pPageAttr->m_AttrRo != MEMATTR_ILWALID)
    {
        *pReadOnly = pPageAttr->m_AttrRo == MEMATTR_TRUE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::IsPageAttrAtomicDisable
(
    MmuLevelTree::LevelIndex levelNum,
    bool *pAtomicDisable
) const
{
    const PageAttrs* pPageAttr = GetPageAttrs(levelNum);

    if (pPageAttr && pPageAttr->m_AttrAtomicDisable != MEMATTR_ILWALID)
    {
        *pAtomicDisable = pPageAttr->m_AttrAtomicDisable == MEMATTR_TRUE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::IsPageAttrPriv
(
    MmuLevelTree::LevelIndex levelNum,
    bool *pPriv
) const
{
    const PageAttrs* pPageAttr = GetPageAttrs(levelNum);

    if (pPageAttr && pPageAttr->m_AttrPriv != MEMATTR_ILWALID)
    {
        *pPriv = pPageAttr->m_AttrPriv == MEMATTR_TRUE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::SetPageAttrValid
(
    MmuLevelTree::LevelIndex levelNum,
    bool valid
)
{
    PageAttrs* pPageAttr = GetPageAttrs(levelNum);
    if (pPageAttr && pPageAttr->m_AttrVal != MEMATTR_ILWALID)
    {
        pPageAttr->m_AttrVal = valid? MEMATTR_TRUE : MEMATTR_FALSE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::SetPageAttrCache
(
    MmuLevelTree::LevelIndex levelNum,
    bool cachable
)
{
    PageAttrs* pPageAttr = GetPageAttrs(levelNum);
    if (pPageAttr && pPageAttr->m_AttrCache != MEMATTR_ILWALID)
    {
        pPageAttr->m_AttrCache = cachable? MEMATTR_TRUE : MEMATTR_FALSE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::SetPageAttrSparse
(
    MmuLevelTree::LevelIndex levelNum,
    bool sparse)
{
    PageAttrs* pPageAttr = GetPageAttrs(levelNum);
    if (pPageAttr && pPageAttr->m_AttrSparse != MEMATTR_ILWALID)
    {
        pPageAttr->m_AttrSparse = sparse? MEMATTR_TRUE : MEMATTR_FALSE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::SetPageAttrRO
(
    MmuLevelTree::LevelIndex levelNum,
    bool readOnly
)
{
    PageAttrs* pPageAttr = GetPageAttrs(levelNum);
    if (pPageAttr && pPageAttr->m_AttrRo != MEMATTR_ILWALID)
    {
        pPageAttr->m_AttrRo = readOnly? MEMATTR_TRUE : MEMATTR_FALSE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::SetPageAttrAtomicDisable
(
    MmuLevelTree::LevelIndex levelNum,
    bool atomicDisable
)
{
    PageAttrs* pPageAttr = GetPageAttrs(levelNum);
    if (pPageAttr && pPageAttr->m_AttrAtomicDisable != MEMATTR_ILWALID)
    {
        pPageAttr->m_AttrAtomicDisable = atomicDisable? MEMATTR_TRUE : MEMATTR_FALSE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

RC MemAttrs::SetPageAttrPriv
(
    MmuLevelTree::LevelIndex levelNum,
    bool priv
)
{
    PageAttrs* pPageAttr = GetPageAttrs(levelNum);
    if (pPageAttr && pPageAttr->m_AttrPriv != MEMATTR_ILWALID)
    {
        pPageAttr->m_AttrPriv = priv? MEMATTR_TRUE : MEMATTR_FALSE;
        return OK;
    }

    return RC::UNSUPPORTED_FUNCTION;
}

const MemAttrs::PageAttrs* MemAttrs::GetPageAttrs
(
    MmuLevelTree::LevelIndex levelNum
) const
{
    map<UINT32, PageAttrs>::const_iterator cit;
    cit = m_MmuLevelsPageAttrs.find(levelNum);

    if (cit == m_MmuLevelsPageAttrs.end())
    {
        return NULL;
    }

    return &cit->second;
}

MemAttrs::PageAttrs* MemAttrs::GetPageAttrs
(
    MmuLevelTree::LevelIndex levelNum
)
{
    map<UINT32, PageAttrs>::iterator it;
    it = m_MmuLevelsPageAttrs.find(levelNum);

    if (it == m_MmuLevelsPageAttrs.end())
    {
        // init it on first get
        if (OK != InitMemAttrsLevel(levelNum))
        {
            return NULL;
        }

        it = m_MmuLevelsPageAttrs.find(levelNum);
        MASSERT(it != m_MmuLevelsPageAttrs.end());
    }

    return &it->second;
}

bool MemAttrs::IsSameMemAttrs(const MemAttrs *pMemAttrs) const
{
    if (GetActivePageSize() != pMemAttrs->GetActivePageSize())
    {
        return false;
    }

    for (UINT32 i = MmuLevelTree::GMMU_LEVEL_PTE_4K;
         i < MmuLevelTree::GMMU_LEVEL_PDE_LAST; ++ i)
    {
        MmuLevelTree::LevelIndex levelNum =
            static_cast<MmuLevelTree::LevelIndex>(i);

        //
        // RM returns invalid PTE entry data in case of sysmem surface;
        // Skip comparing inactive 4K/64K PTE level
        // PTE_2M is always valid no matter it's PTE or PDE0
        if (levelNum == MmuLevelTree::GMMU_LEVEL_PTE_4K  ||
            levelNum == MmuLevelTree::GMMU_LEVEL_PTE_64K)
        {
            if (levelNum != GetActivePteLevel())
            {
                continue; // inactive pte, skip comparing
            }
        }

        //
        // Comparing page attributes
        const PageAttrs* dstPageAttrs = GetPageAttrs(levelNum);
        const PageAttrs* srcPageAttrs = pMemAttrs->GetPageAttrs(levelNum);

        if ((dstPageAttrs == NULL) &&
            (srcPageAttrs == NULL))
       {
           continue;
       }
       else if ((dstPageAttrs == NULL) ||
                (srcPageAttrs == NULL))
       {
           return false;
       }

       if (memcmp(dstPageAttrs, srcPageAttrs, sizeof(PageAttrs)))
       {
           return false;
       }
    }

    return true;
}

//
// GmmuMemAttrs con's
GmmuMemAttrs::GmmuMemAttrs
(
    PmSurface* pPmSurface,
    UINT64 offset,
    UINT64 pageSize
) :
    MemAttrs(pPmSurface, offset, pageSize)
{
}

/*virtual*/ MmuLevelTree::LevelIndex GmmuMemAttrs::GetActivePteLevel() const
{
    MmuLevelTree::LevelIndex levelNum;
    if (m_PageSize == PolicyManager::BYTES_IN_512MB_PAGE)
    {
        levelNum = MmuLevelTree::GMMU_LEVEL_PTE_512M;
    }
    else if (m_PageSize == PolicyManager::BYTES_IN_HUGE_PAGE)
    {
        levelNum = MmuLevelTree::GMMU_LEVEL_PTE_2M;
    }
    else if (m_PageSize == PolicyManager::BYTES_IN_SMALL_PAGE)
    {
        levelNum = MmuLevelTree::GMMU_LEVEL_PTE_4K;
    }
    else
    {
        levelNum = MmuLevelTree::GMMU_LEVEL_PTE_64K;
    }

    return levelNum;
}

/*virtual*/ UINT32 GmmuMemAttrs::GetPteFlags() const
{
    return UpdatePteFlagsFromMemAttrs();
}

/*virtual*/ GmmuMemAttrs *GmmuMemAttrs::Clone(UINT64 Offset) const
{
    GmmuMemAttrs* cloned = new GmmuMemAttrs(m_pPmSurface, Offset, m_PageSize);

    map<UINT32, PageAttrs>::const_iterator constIt = m_MmuLevelsPageAttrs.begin();
    for (; constIt != m_MmuLevelsPageAttrs.end(); ++ constIt)
    {
        memcpy(&cloned->m_MmuLevelsPageAttrs[constIt->first],
           &constIt->second, sizeof(PageAttrs));
    }

    return cloned;
}

// To Do: remove this function finally
/*virtual*/ RC GmmuMemAttrs::UpdateMemAttrs(UINT32 fillPteFlags)
{
    // pte flags is droped in GmmuMemAttrs
    // Nothing to do here
    return OK;
}

/*virtual*/ RC GmmuMemAttrs::InitMemAttrsLevel(MmuLevelTree::LevelIndex levelNum)
{
    RC rc;
    MmuLevel *pMmuLevel = NULL;
    MmuLevelSegment *pSegment = NULL;
    MmuLevelSegmentInfo segmentInfo;

    CHECK_RC(m_pPmSurface->GetMmuLevel(levelNum, &pMmuLevel));
    CHECK_RC(pMmuLevel->GetMmuLevelSegment(m_Offset, &pSegment));
    CHECK_RC(pSegment->GetMmuLevelSegmentInfo(&segmentInfo));

    // Get entry data of one entry
    vector<UINT08> entryData;
    UINT64 onePage = m_PageSize;
    CHECK_RC(pSegment->GetMmuEntriesData(m_Offset, &onePage, &entryData));

    // Parse entry attributes
    const GMMU_FMT *pGmmuFmt = pMmuLevel->GetGmmuFmt();
    const MMU_FMT_LEVEL *pLevel = segmentInfo.pFmtLevel;
    const UINT08 *pEntry = &entryData[0];
    PageAttrs* pPageAttrs = &m_MmuLevelsPageAttrs[levelNum];

    // Clear all attrs to MEMATTR_ILWALID
    *pPageAttrs = PageAttrs();

    // Set valid attrs to _FALSE or _TRUE
    // v1 MMU
    if (!pGmmuFmt)
    {
        const UINT32* pDataInDwords = reinterpret_cast<const UINT32*>(pEntry);
        if (FLD_TEST_DRF(_MMU, _PTE, _VALID, _TRUE,
                         pDataInDwords[(0?LW_MMU_PTE_VALID)/32]))
        {
                        pPageAttrs->m_AttrVal = MEMATTR_TRUE;

            if (FLD_TEST_DRF(_MMU, _PTE, _VOL, _TRUE,
                    pDataInDwords[(0?LW_MMU_PTE_VOL)/32]))
            {
                pPageAttrs->m_AttrCache = MEMATTR_FALSE;
            }
            else
            {
                pPageAttrs->m_AttrCache = MEMATTR_TRUE;
            }

            pPageAttrs->m_AttrSparse = MEMATTR_FALSE;
        }
        else
        {
            m_MmuLevelsPageAttrs[levelNum].m_AttrVal = MEMATTR_FALSE;

            if (FLD_TEST_DRF(_MMU, _PTE, _VOL, _TRUE,
                    pDataInDwords[(0?LW_MMU_PTE_VOL)/32]))
            {
                pPageAttrs->m_AttrSparse = MEMATTR_TRUE;
            }
            else
            {
                pPageAttrs->m_AttrSparse = MEMATTR_FALSE;
            }

            pPageAttrs->m_AttrCache = MEMATTR_FALSE;
        }

        // readonly
        if (FLD_TEST_DRF(_MMU, _PTE, _READ_ONLY, _TRUE,
            pDataInDwords[(0?LW_MMU_PTE_READ_ONLY)/32]))
        {
            pPageAttrs->m_AttrRo = MEMATTR_TRUE;
        }
        else
        {
            pPageAttrs->m_AttrRo = MEMATTR_FALSE;
        }

        // atomic_disable
        if (FLD_TEST_DRF(_MMU, _PTE, _ATOMIC_DISABLE, _TRUE,
                pDataInDwords[(0?LW_MMU_PTE_ATOMIC_DISABLE)/32]))
        {
            pPageAttrs->m_AttrAtomicDisable = MEMATTR_TRUE;
        }
        else
        {
            pPageAttrs->m_AttrAtomicDisable = MEMATTR_FALSE;
        }

        // priv
        if (FLD_TEST_DRF(_MMU, _PTE, _PRIVILEGE, _TRUE,
                pDataInDwords[(0?LW_MMU_PTE_PRIVILEGE)/32]))
        {
            pPageAttrs->m_AttrPriv = MEMATTR_TRUE;
        }
        else
        {
            pPageAttrs->m_AttrPriv = MEMATTR_FALSE;
        }

        return rc;
    }

    // v2 & v3 MMU
    if (pSegment->IsPteEntry(pEntry))
    {
        // PTE
        const GMMU_FMT_PTE* pFmtPte = pGmmuFmt->pPte;

        // valid/sparse/cache bit
        if (lwFieldGetBool(&pFmtPte->fldValid, pEntry))
        {
            pPageAttrs->m_AttrVal = MEMATTR_TRUE;

            if (pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::Volatile, pEntry))
            {
                pPageAttrs->m_AttrCache = MEMATTR_FALSE;
            }
            else
            {
                pPageAttrs->m_AttrCache = MEMATTR_TRUE;
            }

            pPageAttrs->m_AttrSparse = MEMATTR_FALSE;
        }
        else
        {
            m_MmuLevelsPageAttrs[levelNum].m_AttrVal = MEMATTR_FALSE;

            if (pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::Sparse, pEntry))
            {
                pPageAttrs->m_AttrSparse = MEMATTR_TRUE;
            }
            else
            {
                pPageAttrs->m_AttrSparse = MEMATTR_FALSE;
            }

            pPageAttrs->m_AttrCache = MEMATTR_FALSE;
        }

        // readonly
        if (pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::ReadOnly, pEntry))
        {
            pPageAttrs->m_AttrRo = MEMATTR_TRUE;
        }
        else
        {
            pPageAttrs->m_AttrRo = MEMATTR_FALSE;
        }

        // atomic_disable
        if (pSegment->IsFieldValid(MmuLevelSegment::PtePcfField::AtomicDisable))
        {
            if (pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::AtomicDisable, pEntry))
            {
                pPageAttrs->m_AttrAtomicDisable = MEMATTR_TRUE;
            }
            else
            {
                pPageAttrs->m_AttrAtomicDisable = MEMATTR_FALSE;
            }
        }

        // priv
        if (pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::Privilege, pEntry))
        {
            pPageAttrs->m_AttrPriv = MEMATTR_TRUE;
        }
        else
        {
            pPageAttrs->m_AttrPriv = MEMATTR_FALSE;
        }
    }
    else
    {
        // PDE
        MmuLevel::MmuSubLevelIndex subLevelIndex;
        if(levelNum == MmuLevelTree::GMMU_LEVEL_PDE0 &&
           m_PageSize == PolicyManager::BYTES_IN_SMALL_PAGE)
        {
            subLevelIndex = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_SMALL;
        }
        else
        {
            subLevelIndex = MmuLevel::GMMU_SUB_LEVEL_PAGE_TABLE_BIG;
        }

        const GMMU_FMT_PDE* pFmtPde =
                gmmuFmtGetPde(pGmmuFmt, pLevel, subLevelIndex);

        GMMU_APERTURE aperture =
            gmmuFieldGetAperture(&pFmtPde->fldAperture, pEntry);
        if (aperture == GMMU_APERTURE_ILWALID)
        {
            pPageAttrs->m_AttrVal = MEMATTR_FALSE;

            if (pSegment->GetFieldBool(MmuLevelSegment::PtePcfField::Volatile, pEntry))
            {
                pPageAttrs->m_AttrSparse = MEMATTR_TRUE;
            }
            else
            {
                pPageAttrs->m_AttrSparse = MEMATTR_FALSE;
            }
        }
        else
        {
            pPageAttrs->m_AttrVal = MEMATTR_TRUE;
            pPageAttrs->m_AttrSparse = MEMATTR_FALSE;
        }
    }

    return rc;
}

UINT32 GmmuMemAttrs::UpdatePteFlagsFromMemAttrs() const
{
    MmuLevelTree::LevelIndex levelNum = GetActivePteLevel();

    map<UINT32, PageAttrs>::const_iterator constIt;
    constIt = m_MmuLevelsPageAttrs.find(levelNum);
    if (constIt == m_MmuLevelsPageAttrs.end())
    {
        return 0;
    }

    UINT32 pteFlags =
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                _CONTIGUOUS, _TRUE) |
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                _OVERRIDE_PAGE_SIZE, _TRUE);

    const PageAttrs* pPageAttrs = &constIt->second;

    if (pPageAttrs->m_AttrVal == MEMATTR_TRUE)
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                               _FLAGS, _VALID, _TRUE, pteFlags);
    }
    else
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                               _FLAGS, _VALID, _FALSE, pteFlags);
    }

    if (FLD_TEST_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                     _FLAGS, _APERTURE, _VIDEO_MEMORY, pteFlags))
    {
        if (pPageAttrs->m_AttrCache == MEMATTR_TRUE)
        {
            pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                                   _FLAGS, _GPU_CACHED, _TRUE, pteFlags);
        }
        else
        {
            pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                                   _FLAGS, _GPU_CACHED, _FALSE, pteFlags);
        }
    }
    else
    {
        // Gpu cache is not enabled for memory other than vidmem
        // We might need a better way to select the flag ???
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                               _FLAGS, _GPU_CACHED, _FALSE, pteFlags);
    }

    if (pPageAttrs->m_AttrSparse == MEMATTR_TRUE)
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _SPARSE, _TRUE, pteFlags);
    }
    else
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _SPARSE, _FALSE, pteFlags);
    }

    if (pPageAttrs->m_AttrRo == MEMATTR_TRUE)
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _READ_ONLY, _TRUE, pteFlags);
    }
    else
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _READ_ONLY, _FALSE, pteFlags);
    }

    if (pPageAttrs->m_AttrAtomicDisable == MEMATTR_TRUE)
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _ATOMIC_DISABLE, _TRUE, pteFlags);
    }
    else
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _ATOMIC_DISABLE, _FALSE, pteFlags);
    }

    if (pPageAttrs->m_AttrPriv == MEMATTR_TRUE)
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _PRIV, _TRUE, pteFlags);
    }
    else
    {
        pteFlags = FLD_SET_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                               _PRIV, _FALSE, pteFlags);
    }

    UINT64 physAddr = 0;
    GMMU_APERTURE aperture = GMMU_APERTURE_ILWALID;
    MmuLevelTree *pMmuLevelTree = m_pPmSurface->GetMmuLevelTree();
    if (OK != pMmuLevelTree->GetSurfacePhysAddr(GetOffset(),
            GetActivePageSize(), &physAddr, &aperture))
    {
        return 0;
    }

    switch(aperture)
    {
    case GMMU_APERTURE_VIDEO:
        pteFlags |= DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                            _FLAGS, _APERTURE, _VIDEO_MEMORY);
        break;

    case GMMU_APERTURE_SYS_COH:
        pteFlags |= DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                            _FLAGS, _APERTURE,
                            _SYSTEM_COHERENT_MEMORY);
        break;

    case GMMU_APERTURE_SYS_NONCOH:
        pteFlags |= DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                            _FLAGS, _APERTURE,
                            _SYSTEM_NON_COHERENT_MEMORY);
        break;

    case GMMU_APERTURE_PEER:
        pteFlags |= DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                            _FLAGS, _APERTURE, _PEER_MEMORY);
        pteFlags |= DRF_NUM(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                            _FLAGS, _PEER, 1);
        //pGpuSubdevice->GetSubdeviceInst() + 1);
        break;

    case GMMU_APERTURE_ILWALID:
    default:
        InfoPrintf("%s: No corresponding FILL_PTE_MEM_PARAMS_FLAGS for the aperture = 0x%x\n",
                   __FUNCTION__, aperture);
        return 0;
    }

    return pteFlags;
}

//
// SmmuMemAttrs con's
SmmuMemAttrs::SmmuMemAttrs
(
    PmSurface* pPmSurface,
    UINT64 offset,
    UINT64 pageSize
):
    MemAttrs(pPmSurface, offset, pageSize)
{
}

/*virtual*/ SmmuMemAttrs *SmmuMemAttrs::Clone(UINT64 Offset) const
{
    SmmuMemAttrs* cloned = new SmmuMemAttrs(m_pPmSurface, Offset, m_PageSize);

    map<UINT32, PageAttrs>::const_iterator constIt = m_MmuLevelsPageAttrs.begin();
    for (; constIt != m_MmuLevelsPageAttrs.end(); ++ constIt)
    {
        memcpy(&cloned->m_MmuLevelsPageAttrs[constIt->first],
           &constIt->second, sizeof(PageAttrs));

        cloned->m_PteFlags = m_PteFlags;
    }

    return cloned;
}

// Might be removed after RM API fillPteMem is totally droped for GPU
/*virtual*/ RC SmmuMemAttrs::UpdateMemAttrs(UINT32 fillPteFlags)
{
    MmuLevelTree::LevelIndex levelNum = GetActivePteLevel();
    PageAttrs* pPageAttrs = &m_MmuLevelsPageAttrs[levelNum];

    // valid bit
    if (FLD_TEST_DRF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
                     _FLAGS, _VALID, _TRUE, fillPteFlags))
    {
        pPageAttrs->m_AttrVal = MEMATTR_TRUE;
    }
    else
    {
        pPageAttrs->m_AttrVal = MEMATTR_FALSE;
    }

    // To do: remove this
    m_PteFlags = fillPteFlags;

    return OK;
}

/*virtual*/ RC SmmuMemAttrs::InitMemAttrsLevel(MmuLevelTree::LevelIndex levelNum)
{
    if (GetActivePteLevel() != levelNum)
    {
        // Only support pte level for smmu now.
        return RC::SOFTWARE_ERROR;
    }

    PageAttrs* pPageAttrs = &m_MmuLevelsPageAttrs[levelNum];
    pPageAttrs->m_AttrVal = MEMATTR_TRUE;
    pPageAttrs->m_AttrCache = MEMATTR_FALSE;
    pPageAttrs->m_AttrSparse = MEMATTR_ILWALID;
    pPageAttrs->m_AttrRo = MEMATTR_ILWALID;
    pPageAttrs->m_AttrAtomicDisable = MEMATTR_ILWALID;
    pPageAttrs->m_AttrPriv = MEMATTR_ILWALID;

    m_PteFlags =
        DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                _CONTIGUOUS, _TRUE);
    if (pPageAttrs->m_AttrVal == MEMATTR_TRUE)
    {
        m_PteFlags |=
            DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS,
                    _VALID, _TRUE);
    }

    return OK;
}

//
// AtsMemAttrs con's
AtsMemAttrs::AtsMemAttrs
(
    PmSurface* pPmSurface,
    UINT64 offset,
    UINT64 pageSize
)
: MemAttrs(pPmSurface, offset, pageSize)
{
}

/*virtual*/ MmuLevelTree::LevelIndex AtsMemAttrs::GetActivePteLevel() const
{
    MASSERT(!"AtsMemAttrs::GetActivePteLevel called illegally");
    return MmuLevelTree::GMMU_LEVEL_PTE_64K;
}

// The AtsMemAttrs class is only used for surfaces that are created without a
// GMMU mapping, so there are no page table entries that can be used to
// construct the PTE flags.  However, PmAction_UpdateAtsMapping::Execute() uses
// PTE flags to determine the correct aperture, so at least the aperture
// PTE flags need to be set here.  The MdiagSurf location field is used because
// the aperture for a surface without a GMMU mapping should not change.
// 
/*virtual*/ UINT32 AtsMemAttrs::GetPteFlags() const
{
    UINT32 pteFlags = 0;
    MdiagSurf *mdiagSurface = m_pPmSurface->GetMdiagSurf();

    switch (mdiagSurface->GetLocation())
    {
    case Memory::Fb:
        pteFlags |= DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
            _FLAGS, _APERTURE, _VIDEO_MEMORY);
        break;

    case Memory::Coherent:
        pteFlags |= DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
            _FLAGS, _APERTURE, _SYSTEM_COHERENT_MEMORY);
        break;

    case Memory::NonCoherent:
        pteFlags |= DRF_DEF(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS,
            _FLAGS, _APERTURE, _SYSTEM_NON_COHERENT_MEMORY);
        break;

    default:
        MASSERT(!"unrecognized surface location");
    }

    return pteFlags;
}

/*virtual*/ AtsMemAttrs *AtsMemAttrs::Clone(UINT64 Offset) const
{
    return new AtsMemAttrs(m_pPmSurface, Offset, m_PageSize);
}

/*virtual*/ RC AtsMemAttrs::UpdateMemAttrs(UINT32 fillPteFlags)
{
    // pte flags is droped in AtsMemAttrs
    // Nothing to do here
    return OK;
}

/*virtual*/ RC AtsMemAttrs::InitMemAttrsLevel(MmuLevelTree::LevelIndex levelNum)
{
    MASSERT(!"AtsMemAttrs::InitMemAttrsLevel called illegally");
    return RC::SOFTWARE_ERROR;
}

UINT32 AtsMemAttrs::UpdatePteFlagsFromMemAttrs() const
{
    MASSERT(!"AtsMemAttrs::UpdatePteFlagsFromMemAttrs called illegally");
    return 0;
}

//--------------------------------------------------------------------
//! \brief constructor
//!
PmMemMappingsHelper::PmMemMappingsHelper
(
    PmSurface *pSurface,
    PmMemMapping *pParentMapping,
    UINT32 vaSpaceClass
) :
    m_pSurface(pSurface),
    m_pParentMapping(pParentMapping),
    m_Refs(0),
    m_VASpaceClass(vaSpaceClass),
    m_VirtAddr(0)
{
    MASSERT(m_pSurface != 0);
}

PmMemMappingsHelper::PmMemMappingsHelper
(
    PmSurface *pSurface,
    PmMemMapping *pParentMapping,
    UINT32 vaSpaceClass,
    UINT64 virtAddr
) :
    m_pSurface(pSurface),
    m_pParentMapping(pParentMapping),
    m_Refs(0),
    m_VASpaceClass(vaSpaceClass),
    m_VirtAddr(virtAddr)
{
    MASSERT(m_pSurface != 0);
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmMemMappingsHelper::~PmMemMappingsHelper()
{
    PmMemMappings::iterator ppMemMapping = m_MemMappings.begin();
    for (; ppMemMapping != m_MemMappings.end(); ++ppMemMapping)
    {
        delete (*ppMemMapping);
    }
    m_MemMappings.clear();
}

//--------------------------------------------------------------------
//! \brief Create Top and/or Bottom layer PmMemMappings;
//!
//! For gpu surface:
//!   Top layer mmu means gmmu;
//!   Bottom layer mmu means gpu-asid smmu;
//! For cheetah surface:
//!   Top layer mmu means smmu;
//!   No bottom layer mmu;
RC PmMemMappingsHelper::CreateMemMappings()
{
    RC rc;
    MdiagSurf *pMdiagSurf = m_pSurface->GetMdiagSurf();

#ifndef LW_VERIF_FEATURES
    GpuDevice *pGpuDevice = pMdiagSurf->GetGpuDev();
    // In non-verif configurations, in order to determine the physical
    // address of a memory allocation, a CPU virtual address is passed
    // to Platform::VirtualToPhysical.  For certain configurations of
    // Surface2D objects in sysmem using VirtualToPhysical on the CPU
    // mapped pointer will actually return a GPU BAR1 address rather
    // than the system memory physical address.  In this case policy
    // manager cannot support the surface because if it modifies the
    // PTEs using the GPU BAR1 address, it will corrupt them
    if (!m_pSurface->IsSurfaceValidated())
    {
        m_pSurface->SetSurfaceValidated(true);
        if (pMdiagSurf->GetLocation() != Memory::Fb)
        {
            for (UINT32 i=0; i < pGpuDevice->GetNumSubdevices(); i++)
            {
                GpuSubdevice* const pGpuSubdev = pGpuDevice->GetSubdevice(i);

                UINT64 PhysAddr = 0;
                CHECK_RC(GetSysmemPhysAddr(m_pSurface->GetOrigMemHandle(0), 0,
                    m_VASpaceClass, pGpuSubdev, &PhysAddr));

                if ((PhysAddr >= pGpuSubdev->GetPhysFbBase()) &&
                    (PhysAddr <  pGpuSubdev->GetPhysFbBase()
                                 + pGpuSubdev->FbApertureSize()))
                {
                    InfoPrintf("PolicyManager : Surface %s not supported and is unavailable for Policy Manager actions\n",
                               pMdiagSurf->GetName().c_str());
                    return rc;
                }
            }
        }
    }
    // If the Surface was already validated and the mem mappings are still
    // still empty, then the surface is not valid
    else if (m_MemMappings.empty())
    {
        return rc;
    }
#endif

    // Prepare parameters to populate memmappings
    //
    //
    UINT64 mappingSize = 0;
    LwRm::Handle hTargetVASpace = 0;
    PmMemMapping::MappingType mappingType = PmMemMapping::Gmmu_Default;
    switch (m_VASpaceClass)
    {
        case 0:                          // default va space
        case FERMI_VASPACE_A:            // gmmu va space
            {
                mappingType = PmMemMapping::Gmmu_SysmemPa;

                m_VirtAddr = pMdiagSurf->GetCtxDmaOffsetGpu();
                hTargetVASpace = 0;
            }
            break;
        default:
            MASSERT(!"Invalid va space class!");
    }

    mappingSize = m_pSurface->GetSize();

    // Explore mapping parameters
    //
    CHECK_RC(ExploreMemMappings(hTargetVASpace, m_VirtAddr,
                                mappingSize, mappingType));

    return rc;
}

RC PmMemMappingsHelper::CreateAtsMappings()
{
    RC rc;
    UINT64 RangeSize = m_pSurface->GetSize();

    // Loop that initializes m_MemMappings.  The first iteration
    // creates the PmMemMapping at offset 0, and subsequent iterations
    // create elements at successive offsets.
    //
    for (UINT64 Offset = 0;  Offset < RangeSize;
         Offset += m_MemMappings.back()->GetSize())
    {
        UINT64 Size = 0;
        UINT64 PhysAddr = 0;
        LwRm::Handle hMem = m_pSurface->GetOrigMemHandle(Offset);
        UINT64 PageSize = m_pSurface->GetMdiagSurf()->GetAtsPageSize() << 10;
        MASSERT(PageSize != 0);
        unique_ptr<AtsMemAttrs> pAtsMemAttrs;
        pAtsMemAttrs.reset(new AtsMemAttrs(m_pSurface, Offset, PageSize));

        while (Offset + Size < RangeSize)
        {
            UINT64 NextOffset = Offset + Size;
            UINT64 NextPhysAddr = 0;

            CHECK_RC(m_pSurface->GetMdiagSurf()->GetPhysAddress(NextOffset,
                &NextPhysAddr));

            if (Size == 0)
            {
                // init round
                PhysAddr = NextPhysAddr;
            }
            else if (PhysAddr + Size != NextPhysAddr)
            {
                // Each PmMemMapping should have a contiguous
                // physical address range.
                break;
            }
            Size = min(RangeSize, ALIGN_UP(NextPhysAddr+1, PageSize) - PhysAddr);
        }

        m_MemMappings.push_back(
            new PmMemMapping(m_pSurface, Offset, Size,
            hMem, Offset, PhysAddr, PmMemMapping::Ats_GvaSpa, this,
            pAtsMemAttrs.release()));
    }

    m_VirtAddr = m_pSurface->GetMdiagSurf()->GetCtxDmaOffsetGpu();

    return OK;
}

//!--------------------------------------------------------------------
//! \brief Colone PmMemMappings from a specified PmMemMappingsHelper
//!
//! \param pMemMappingsHelper:
//!        src PmMemMappings to be cloned; if it's same pointer
//!        as "this", copy the memmapping in pMemRange and drop others;
//! \param pMemRange:
//!        Range to be cloned
RC PmMemMappingsHelper::CloneMemMappings
(
    const PmMemMappingsHelper *pMemMappingsHelper,
    const PmMemRange *pMemRange
)
{
    RC rc;

    // Create new PmMemMappings
    //
    PmMemMappings dstMemMappings;
    PmMemMappings srcMemMappings;
    srcMemMappings = pMemMappingsHelper->GetMemMappings(pMemRange);
    for (PmMemMappings::iterator ppMemMapping = srcMemMappings.begin();
         ppMemMapping != srcMemMappings.end(); ++ppMemMapping)
    {
        PmMemMapping *pDstMemMapping;
        pDstMemMapping = new PmMemMapping((*ppMemMapping)->GetSurface(),
                                          (*ppMemMapping)->GetOffset(),
                                          (*ppMemMapping)->GetSize(),
                                          (*ppMemMapping)->GetMemHandle(),
                                          (*ppMemMapping)->GetMemOffset(),
                                          (*ppMemMapping)->GetPhysAddr(),
                                          (*ppMemMapping)->GetMappingType(),
                                          this,
                                          (*ppMemMapping)->GetMemAttrs());

        PmMemMappingsHelper *pSrcSubHelper;
        pSrcSubHelper = (*ppMemMapping)->GetMemMappingsHelper();
        if (pSrcSubHelper)
        {
            PmMemMappingsHelper *pDstSubHelper;
            pDstSubHelper = new PmMemMappingsHelper(
                                    pSrcSubHelper->GetSurface(),
                                    pDstMemMapping,
                                    pSrcSubHelper->GetVASpaceClass(),
                                    pDstMemMapping->GetPhysAddr());

            pDstSubHelper->CloneMemMappings(pSrcSubHelper, NULL);
            pDstMemMapping->SetMemMappingsHelper(pDstSubHelper);
        }

        dstMemMappings.push_back(pDstMemMapping);
    }

    m_VirtAddr = pMemMappingsHelper->GetVirtAddr(
        pMemMappingsHelper->GetBaseOffset());

    // Delete old PmMemMapping
    //
    for (PmMemMappings::iterator ppMemMapping = m_MemMappings.begin();
         ppMemMapping != m_MemMappings.end(); ++ppMemMapping)
    {
        delete (*ppMemMapping);
    }
    m_MemMappings.clear();

    // Save new PmMemMappings
    //
    m_MemMappings = dstMemMappings;

    return rc;
}

//--------------------------------------------------------------------
//! \brief Return array of all MemMappings that overlap pMemRange
//!
//! Return all MemMappings objects in the surface that overlap
//! pMemRange.  The returned objects should be in order and
//! contiguous; traversing the returned array from beginning to end
//! should hit all the memory in pMemRange, in order.
//!
//! Note that the returned MemMappings may include some pages at the
//! beginning and end that aren't in pMemRange.  Use
//! SplitMemMappings() if that's not what you want.
//!
PmMemMappings PmMemMappingsHelper::GetMemMappings(const PmMemRange *pMemRange) const
{
    if ((pMemRange == NULL) || (m_MemMappings.size() == 0))
    {
        return m_MemMappings;
    }
    else
    {
        MASSERT(pMemRange != NULL);
        MASSERT(pMemRange->GetSurface() == m_pSurface);
        MASSERT(pMemRange->GetOffset() + pMemRange->GetSize() <=
                GetBaseOffset() + GetSize());

        PmMemMappings Returlwalue;
        if (pMemRange->GetSize() == 0 ||
            m_MemMappings.size() == 0 ||
            pMemRange->GetOffset() < GetBaseOffset() ||
            pMemRange->GetOffset() + pMemRange->GetSize() >
            GetBaseOffset() + GetSize())
        {
            return Returlwalue;
        }

        UINT32 first = FindMemMapping(pMemRange->GetOffset());
        UINT32 last  = FindMemMapping(pMemRange->GetOffset() +
                                      pMemRange->GetSize() - 1);
        for (UINT32 ii = first; ii <= last; ++ii)
            Returlwalue.push_back(m_MemMappings[ii]);

        return Returlwalue;
    }
}

//--------------------------------------------------------------------
//! \brief Return array of all MemMappings that overlap pMemRange
//!
//! Return all MemMappings objects in the surface that overlap
//! pMemRange.  The returned objects should be in order and
//! contiguous; traversing the returned array from beginning to end
//! should hit all the memory in pMemRange, in order.
//!
//! Unlike GetMemMappings, the MemMappings returned by this method do
//! not contain any extra pages at the beginning or end.  If needed,
//! the first and last PmMemMappings structs are split into two, in
//! order to only include pages that are part of pMemRange.  The
//! splits are guaranteed to occur at page boundaries.
//!
PmMemMappings PmMemMappingsHelper::SplitMemMappings(const PmMemRange *pMemRange)
{
    MASSERT(pMemRange != NULL);
    MASSERT(pMemRange->GetSurface() == m_pSurface);
    MASSERT(pMemRange->GetOffset() + pMemRange->GetSize() <=
            GetBaseOffset() + GetSize());

    if (pMemRange->GetSize() > 0)
    {
        UINT64 splitVirtAddr = GetVirtAddr(pMemRange->GetOffset());

        UINT32 index;
        PmMemMapping *pMemMapping;
        UINT64 alignedSplitAddr;

        // Split the PmMemMappings at the start of pMemRange on a page
        // boundary, if needed.
        //
        index = FindMemMapping(pMemRange->GetOffset());
        pMemMapping = m_MemMappings[index];
        alignedSplitAddr = ALIGN_DOWN(splitVirtAddr,
                                      pMemMapping->GetPageSize());
        if (alignedSplitAddr > pMemMapping->GetVirtAddr())
        {
            UINT64 SplitOffset = pMemRange->GetOffset() -
                                 (splitVirtAddr - alignedSplitAddr);
            SplitMapping(SplitOffset);
        }
        else if (pMemMapping->GetMemMappingsHelper() &&
                 (splitVirtAddr > pMemMapping->GetVirtAddr()))
        {
            // sub memmappings may be on page boundary because it may
            // have different page size; split sub memmappings
            pMemMapping->GetMemMappingsHelper()->SplitMemMappings(pMemRange);
        }

        // Split the PmMemMappings at the end of pMemRange on a page
        // boundary, if needed.
        //
        index = FindMemMapping(pMemRange->GetOffset() +
                               pMemRange->GetSize() - 1);
        pMemMapping = m_MemMappings[index];
        alignedSplitAddr = ALIGN_UP(splitVirtAddr + pMemRange->GetSize(),
                                    pMemMapping->GetPageSize());
        if (alignedSplitAddr < pMemMapping->GetVirtAddr() + pMemMapping->GetSize())
        {
            UINT64 SplitOffset = (pMemRange->GetOffset() + pMemRange->GetSize()) +
                          (alignedSplitAddr - (splitVirtAddr + pMemRange->GetSize()));
            SplitMapping(SplitOffset);
        }
        else if (pMemMapping->GetMemMappingsHelper() &&
                 (splitVirtAddr + pMemRange->GetSize() <
                  pMemMapping->GetVirtAddr() + pMemMapping->GetSize()))
        {
            // sub memmappings may be on page boundary because it may
            // have different page size; split sub memmappings
            pMemMapping->GetMemMappingsHelper()->SplitMemMappings(pMemRange);
        }
    }

    // Now that we've done the splits, we can use GetMemMappings() to
    // get the return value.
    //
    return GetMemMappings(pMemRange);
}

//--------------------------------------------------------------------
//! \brief Join adjacent PmMemMapping elements where possible
//!
//! This method searches m_MemMappings over the range specified by
//! pMemRange, finds any adjacent elements that can be combined into
//! one, and combines them.
//!
//! Adjacent elements can be combined if they were allocated
//! contiguously from the same memory handle, and have the same
//! page-size and PTE flags.
//!
//! This method is generally called at the end of a subroutine that
//! called SplitMemMappings().
//!
void PmMemMappingsHelper::JoinMemMappings(const PmMemRange *pMemRange)
{
    UINT64 JoinOffset;
    UINT64 JoinSize;

    if (pMemRange == NULL)
    {
        JoinOffset = 0;
        JoinSize   = GetSize();
    }
    else
    {
        JoinOffset = pMemRange->GetOffset();
        JoinSize   = pMemRange->GetSize();
        MASSERT(pMemRange->GetSurface() == m_pSurface);
        MASSERT(JoinOffset + JoinSize <= GetSize());
    }

    if (JoinSize > 0)
    {
        UINT32 first = FindMemMapping(JoinOffset);
        UINT32 last  = FindMemMapping(JoinOffset + JoinSize - 1);
        UINT32 dst = (first > 0) ? (first - 1) : 0;
        UINT32 src = dst + 1;

        vector<UINT32> merged;

        while (src <= last)
        {
            if (m_MemMappings[dst]->Join(m_MemMappings[src]))
            {
                merged.insert(merged.begin(), src);
                ++src;
            }
            else
            {
                dst = src;
                ++src;
            }
        }
        for (vector<UINT32>::iterator i = merged.begin(), end = merged.end();
             i != end; ++i)
        {
            delete m_MemMappings[*i];
            m_MemMappings.erase(m_MemMappings.begin() + (*i));
        }
    }
}

//--------------------------------------------------------------------
//! \brief Split a memory mapping and update the
//! surface's memory mappings
//!
//! Returns a pointer to the mapping containing the
//! portion of the original mapping after splitOffset
PmMemMapping* PmMemMappingsHelper::SplitMapping(UINT64 splitOffset)
{
    UINT32 index;
    PmMemMapping *pMemMapping;
    PmMemRange splitLoc(m_pSurface, splitOffset, 1);

    index = FindMemMapping(splitOffset);
    pMemMapping = m_MemMappings[index];
    UINT64 SplitAddr = GetVirtAddr(splitOffset);

    m_MemMappings.insert(m_MemMappings.begin() + index,
                         pMemMapping->Split(SplitAddr));

    return m_MemMappings[index];
}

//--------------------------------------------------------------------
//! \brief Update PmMemMappings wrapped in this object
//!
//! \param PteFlags New PTE flags to be updated
//! \param PteMask  Mask of PTE flags to be updated
//!                 (PteFlags & ~PteMask) must be 0.
//! \param DstPteMask Mask of PTE flags to remain unchanged.
//!                 (PteMask & DstPteMask) must be 0.
//! Note:
//!     ~(PteMask | DstPteMask) is the mask of flags copied from pSrcMemMappings.
//!
//! \param pSrcMemMappings Src memmappings
//! \param Offset Dst starting offset to be updated
//! \param Size Mapping size to be updated; srcMemMappings size >= Size
//! \param WritePtes Write PTEs or not
//!        For smmu enabled gpu surface, mapping change in top layer mmu
//!        will make bottom mmu mapping changed automatically. Just update
//!        data for this case; no need to write hw PTEs.
//! \param DeferTlbIlwalidate PmMemMapping::WritePtes() params
//! \param pInbandChannel     PmMemMapping::WritePtes() params
RC PmMemMappingsHelper::UpdateMemMappings
(
    UINT32 PteFlags,
    UINT32 PteMask,
    UINT32 DstMask,
    UINT64 SrcRangeOffset,
    PmMemMappings *pSrcMemMappings,
    UINT64 Offset,
    UINT64 Size,
    WritePtesMode WritePtes,
    bool DeferTlbIlwalidate,
    PmChannel *pInbandChannel
)
{
    RC rc;

    MASSERT((PteFlags & ~PteMask) == 0);
    MASSERT((PteMask & DstMask) == 0);
    MASSERT(pSrcMemMappings != NULL);

    // Select MappingType:
    //  case 1: dst has same type as src
    //          Common case. Use new mapping position to replace old one.
    PmMemMapping::MappingType dstMappingType = PmMemMapping::Gmmu_Default;
    PmMemMapping::MappingType srcMappingType = PmMemMapping::Gmmu_Default;
    if (m_MemMappings.size() > 0)
    {
        dstMappingType = m_MemMappings[0]->GetMappingType();
    }
    if (pSrcMemMappings->size() > 0)
    {
        srcMappingType = (*pSrcMemMappings)[0]->GetMappingType();
    }

    bool bOnlyUpdateSysmemPhysAddr = false;
    dstMappingType = srcMappingType;

    // Create dst memmappings according to src mapping information
    //
    UINT64 dstSize = 0;
    PmMemMappings newMemMappings;
    for (PmMemMappings::iterator ppMemMapping = pSrcMemMappings->begin();
         ppMemMapping != pSrcMemMappings->end(); ++ppMemMapping)
    {
        MemAttrs *pMemAttrs = bOnlyUpdateSysmemPhysAddr?
                    m_MemMappings[0]->GetMemAttrs()
                   :(*ppMemMapping)->GetMemAttrs();

        UINT32 dstPteFlags = 0;
        if (bOnlyUpdateSysmemPhysAddr)
        {
            // Existed dst flags + flags to be set
            dstPteFlags = (m_MemMappings[0]->GetPteFlags() & ~PteMask) | PteFlags;
        }
        else if (m_MemMappings.size() > 0)
        {
            // Existed dst flags + flags to be set + existed src flags
            dstPteFlags =
                ((*ppMemMapping)->GetPteFlags() & ~(PteMask|DstMask)) |
                PteFlags |
                (m_MemMappings[0]->GetPteFlags() & DstMask);
        }
        else
        {
            // Existed src flags + flags to be set
            dstPteFlags =
                ((*ppMemMapping)->GetPteFlags() & ~PteMask) | PteFlags;
        }

        unique_ptr<MemAttrs> pDstMemAttrs(pMemAttrs->Clone(pMemAttrs->GetOffset()));
        pDstMemAttrs->UpdateMemAttrs(dstPteFlags);

        UINT64 dstMemMappingSize =
            min((*ppMemMapping)->GetSize(), Size - dstSize);
        UINT64 offsetInSrc =
            SrcRangeOffset + dstSize - (*ppMemMapping)->GetOffset();

        PmMemMapping* pDstMemMapping = new PmMemMapping(
                                m_pSurface,
                                Offset + dstSize,
                                dstMemMappingSize,
                                (*ppMemMapping)->GetMemHandle(),
                                (*ppMemMapping)->GetMemOffset(),
                                (*ppMemMapping)->GetPhysAddr() + offsetInSrc,
                                dstMappingType,
                                this,
                                pDstMemAttrs.get());
        pDstMemMapping->SetPhysRange(
            (*ppMemMapping)->GetPhysRange()->GetSurface(),
            (*ppMemMapping)->GetPhysRange()->GetOffset(),
            Size);

        if (WritePtes == WritePtes_TRUE)
        {
            if (GetSurface()->IsMmuLevelFmtSupported())
            {
                 // Update local PTE entry data directly
                 CHECK_RC(pDstMemMapping->WriteMmuLevelEntries(pDstMemMapping->GetPteFlags(), ~0,
                                         FALSE, pDstMemMapping->GetPageSize(), DeferTlbIlwalidate,
                                         pInbandChannel, pDstMemAttrs->GetActivePteLevel()));
            }
            else
            {
                // Call LW0080_CTRL_CMD_DMA_FILL_PTE_MEM to set the mapping.
                CHECK_RC(pDstMemMapping->WritePtes(pDstMemMapping->GetMemHandle(),
                            pDstMemMapping->GetMemOffset(),
                            pDstMemMapping->GetPteFlags(),
                            pDstMemMapping->GetPhysAddr(),
                            pDstMemMapping->GetPageSize(),
                            DeferTlbIlwalidate, NULL));
            }
        }

        newMemMappings.push_back(pDstMemMapping);

        dstSize += pDstMemMapping->GetSize();
        if (dstSize >= Size)
        {
            break;
        }
    }

    // SrcMemMappings should have big enough memory for requested size
    //
    MASSERT(dstSize >= Size);

    // Remove stale old memmapping and update to new dst memmapping
    //
    if (m_MemMappings.size() > 0)
    {
        // split and update partial memmappings
        //
        MASSERT(Offset >= GetBaseOffset());
        MASSERT(Offset + Size <= m_MemMappings.back()->GetOffset() +
                                 m_MemMappings.back()->GetSize());

        PmMemRange splitRange(m_pSurface, Offset, Size);
        SplitMemMappings(&splitRange);
        UINT32 first = FindMemMapping(splitRange.GetOffset());
        UINT32 last  = FindMemMapping(splitRange.GetOffset() +
                                      splitRange.GetSize() - 1);

        for (UINT32 ii = first; ii <= last; ++ii)
        {
            delete m_MemMappings[ii];
        }
        m_MemMappings.erase(m_MemMappings.begin() + first,
                            m_MemMappings.begin() + last + 1);
        m_MemMappings.insert(m_MemMappings.begin() + first,
                             newMemMappings.begin(),
                             newMemMappings.end());
    }
    else
    {
        m_MemMappings = newMemMappings;
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Compare to return whehter PmMemMappings are changed
//!
//! \param pSrcMemMappingsHelper Src PmMemMapings to be compared
//! \return true - same memmapping attributes as src
//!         false - different memmapping attributes
bool PmMemMappingsHelper::IsMemMappingsChanged
(
    const PmMemMappingsHelper *pSrcMemMappingsHelper
) const
{
    if (pSrcMemMappingsHelper == NULL)
    {
        return true;
    }

    PmMemMappings origMemMappings = pSrcMemMappingsHelper->GetMemMappings(NULL);
    if (origMemMappings.size() != m_MemMappings.size())
    {
        return true;
    }

    for (UINT32 ii = 0; ii < m_MemMappings.size(); ++ii)
    {
        PmMemMapping *pOld = origMemMappings[ii];
        PmMemMapping *pNew = m_MemMappings[ii];

        if (pOld->GetOffset() != pNew->GetOffset() ||
            pOld->GetSize() != pNew->GetSize() ||
            pOld->GetPteFlags() != pNew->GetPteFlags() ||
            pOld->GetPhysAddr() != pNew->GetPhysAddr() ||
            pOld->GetPageSize() != pNew->GetPageSize())
        {
            return true;
        }

        PmMemMappingsHelper *pOldSubMappingsHelper = pOld->GetMemMappingsHelper();
        PmMemMappingsHelper *pNewSubMappingsHelper = pNew->GetMemMappingsHelper();
        if (pOldSubMappingsHelper && pNewSubMappingsHelper)
        {
            if (pNewSubMappingsHelper->IsMemMappingsChanged(pOldSubMappingsHelper))
            {
                return true;
            }
        }
        else  if (pOldSubMappingsHelper || pNewSubMappingsHelper)
        {
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------
//! \brief Check LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID_FALSE
//!
//! \param pMemRange Range to check
bool PmMemMappingsHelper::IsMemMappingsValid(const PmMemRange *pMemRange) const
{
    PmMemMappings memMappings = GetMemMappings(pMemRange);
    for (PmMemMappings::const_iterator ppMemMapping = memMappings.begin();
         ppMemMapping != memMappings.end(); ++ ppMemMapping)
    {
        if (DRF_VAL(0080_CTRL_DMA_FILL_PTE_MEM_PARAMS, _FLAGS, _VALID,
            (*ppMemMapping)->GetPteFlags())
            == LW0080_CTRL_DMA_FILL_PTE_MEM_PARAMS_FLAGS_VALID_FALSE)
        {
            return false;
        }

        PmMemMappingsHelper *pSubMappingsHelper;
        pSubMappingsHelper = (*ppMemMapping)->GetMemMappingsHelper();
        if (pSubMappingsHelper)
        {
            if (!pSubMappingsHelper->IsMemMappingsValid(NULL))
            {
                return false;
            }
        }
    }

    return true;
}

//--------------------------------------------------------------------
//! \brief return memmappings size
//!
UINT64 PmMemMappingsHelper::GetSize() const
{
    UINT64 totalSize = 0;
    for (UINT32 ii = 0; ii < m_MemMappings.size(); ++ii)
    {
        totalSize += m_MemMappings[ii]->GetSize();
    }

    return totalSize;
}

//--------------------------------------------------------------------
//! \brief Split PmMemMappings of this object at a page boundary
//! \param splitOffset Requested split offset
//! \param pNewParentMapping Parent PmMemMapping for new PmMemMappingsHelper
//! \return:
//!          success - new PmMemMappingsHelper pointer
//!          fail    - NULL pointer
PmMemMappingsHelper *PmMemMappingsHelper::Split
(
    UINT64 offset,
    UINT64 size,
    PmMemMapping *pNewParentMapping
)
{
    PmMemRange splitRange(m_pSurface, offset, size);
    PmMemMappings newMemMappings = SplitMemMappings(&splitRange);

    PmMemMappingsHelper *pRtn = new PmMemMappingsHelper(m_pSurface,
                                                        pNewParentMapping,
                                                        m_VASpaceClass,
                                                        GetVirtAddr(offset));
    for (UINT32 ii = 0; ii < newMemMappings.size(); ++ ii)
    {
        PmMemMapping *pMemMapping = newMemMappings[ii];
        pRtn->m_MemMappings.push_back(new PmMemMapping(
            pMemMapping->GetSurface(), pMemMapping->GetOffset(),
            pMemMapping->GetSize(), pMemMapping->GetMemHandle(),
            pMemMapping->GetMemOffset(),
            pMemMapping->GetPhysAddr(),
            pMemMapping->GetMappingType(), pRtn,
            pMemMapping->GetMemAttrs()));
    }

    // Update current memmappings
    UINT32 first = FindMemMapping(splitRange.GetOffset());
    UINT32 last  = FindMemMapping(splitRange.GetOffset() +
                                      splitRange.GetSize() - 1);

    for (UINT32 ii = first; ii <= last; ++ii)
    {
        delete m_MemMappings[ii];
    }
    m_MemMappings.erase(m_MemMappings.begin() + first,
                        m_MemMappings.begin() + last + 1);

    return pRtn;
}

//--------------------------------------------------------------------
//! \brief Join two adjacent PmMemMappings hold by this object
//!
void PmMemMappingsHelper::Join(PmMemMappingsHelper *pSrcMemMappingHelper)
{
    if (pSrcMemMappingHelper == NULL ||
        this == pSrcMemMappingHelper)
    {
        return; // nothing to do
    }

    // copy all source memmappings to this object
    //
    PmMemMappings srcMappings = pSrcMemMappingHelper->m_MemMappings;
    for (PmMemMappings::iterator ppMemMapping = srcMappings.begin();
         ppMemMapping != srcMappings.end(); ++ppMemMapping)
    {
        m_MemMappings.push_back((*ppMemMapping));
    }

    return;
}

//--------------------------------------------------------------------
//! Private method to explore contiguous memmaping
//! \param hTargetVASpace Target VASpace to be explored
//! \param TargetVirtAddrBase Start virtual address to be explored
//! \param RangeSize Range size to be explored
//! \param ParamsList Return value including contiguous memmaping information
RC PmMemMappingsHelper::ExploreMemMappings(LwRm::Handle hTargetVASpace,
                                           UINT64 TargetVirtAddrBase,
                                           UINT64 RangeSize,
                                           PmMemMapping::MappingType mappingType)
{
    MdiagSurf *pMdiagSurf = m_pSurface->GetMdiagSurf();
    GpuDevice *pGpuDevice = pMdiagSurf->GetGpuDev();
    UINT32 TargetVAClass = pGpuDevice->GetVASpaceClassByHandle(hTargetVASpace);

    switch(TargetVAClass)
    {
    case 0:
    case FERMI_VASPACE_A:
        return ExploreGmmuMemMappings(RangeSize, mappingType);
    }

    MASSERT(!"Unknown MMU class\n");
    return RC::SOFTWARE_ERROR;
}

RC PmMemMappingsHelper::ExploreSmmuMemMappings
(
    LwRm::Handle hTargetVASpace,
    UINT64 TargetVirtAddrBase,
    UINT64 RangeSize,
    PmMemMapping::MappingType mappingType
)
{
    RC rc;
    MdiagSurf *pMdiagSurf = m_pSurface->GetMdiagSurf();
    GpuDevice *pGpuDevice = pMdiagSurf->GetGpuDev();
    UINT32 TargetVAClass = pGpuDevice->GetVASpaceClassByHandle(hTargetVASpace);
    LwRm* pLwRm = GetSurface()->GetLwRmPtr();

    // Loop that initializes m_MemMappings.  The first iteration
    // creates the PmMemMapping at offset 0, and subsequent iterations
    // create elements at successive offsets.
    //
    for (UINT64 Offset = 0;  Offset < RangeSize;
         Offset += m_MemMappings.back()->GetSize())
    {
        // Find the memory-handle-related info for this offset
        //
        LwRm::Handle hMem;
        UINT64       MemOffset;
        UINT64       MemSize;
        Memory::Location Location;
        if (m_pParentMapping)
        {
            // No split situation for sub memmappings
            //
            hMem      = m_pParentMapping->GetMemHandle();
            MemOffset = Offset;
            MemSize   = m_pParentMapping->GetSize();
            Location  = Memory::Coherent;
        }
        else
        {
            hMem      = m_pSurface->GetOrigMemHandle(Offset);
            MemOffset = m_pSurface->GetOrigMemOffset(Offset);
            MemSize   = m_pSurface->GetOrigMemSize(Offset);
            Location = (hMem == pMdiagSurf->GetMemHandle() ?
                                         pMdiagSurf->GetLocation() :
                                         pMdiagSurf->GetSplitLocation());
        }
        MASSERT(hMem != 0);
        MASSERT(MemSize != 0);

        // Query the RM for the page size and phys addr, and find how
        // many contiguous bytes have the same page size & phys addr.
        //
        UINT64 Size = 0;
        UINT64 PageSize = 0;
        UINT64 PhysAddr = 0;

        while (Size < MemSize)
        {
            UINT64 VirtAddr = TargetVirtAddrBase + Offset + Size;
            UINT64 NextPageSize = 0;
            UINT64 NextPhysAddr;

            LW0080_CTRL_DMA_GET_PTE_INFO_PARAMS params = { 0 };
            params.gpuAddr = VirtAddr;
            params.hVASpace = hTargetVASpace;
            CHECK_RC(pLwRm->ControlByDevice(
                         pGpuDevice, LW0080_CTRL_CMD_DMA_GET_PTE_INFO,
                         &params, sizeof(params)));
            for (UINT32 ii = 0;
                 ii < LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS; ++ii)
            {
                if (params.pteBlocks[ii].pageSize != 0 &&
                    DRF_VAL(0080_CTRL_DMA_PTE_INFO_PARAMS, _FLAGS, _VALID,
                            params.pteBlocks[ii].pteFlags) ==
                    LW0080_CTRL_DMA_PTE_INFO_PARAMS_FLAGS_VALID_TRUE)
                {
                    MASSERT(NextPageSize == 0); // Warn if 2 PTEs map VirtAddr
                                                // to 2 physAddrs. If so, need
                                                // to add code to choose one.
                    NextPageSize = params.pteBlocks[ii].pageSize;
                }
            }
            MASSERT(NextPageSize != 0);

            if ((Location == Memory::Fb))
            {
                if (hMem == pMdiagSurf->GetMemHandle())
                {
                    NextPhysAddr = (pMdiagSurf->GetVidMemOffset() +
                                    pMdiagSurf->GetHiddenAllocSize() +
                                    Offset + Size);
                }
                else
                {
                    NextPhysAddr = (pMdiagSurf->GetSplitVidMemOffset() +
                                    MemOffset + Size);
                }
            }
            else
            {
                CHECK_RC(GetSysmemPhysAddr(hMem, MemOffset + Size,
                                           TargetVAClass, pGpuDevice->GetSubdevice(0),
                                           &NextPhysAddr));
            }

            MASSERT(NextPageSize != 0);
            MASSERT((NextPageSize & (NextPageSize - 1)) == 0);
            MASSERT((NextPhysAddr % NextPageSize) == (VirtAddr % NextPageSize));

            if (Size == 0)
            {
                PageSize = NextPageSize;
                PhysAddr = NextPhysAddr;
            }
            else if (PageSize != NextPageSize ||
                     PhysAddr + Size != NextPhysAddr)
            {
                break;
            }
            Size = min(MemSize, ALIGN_UP(NextPhysAddr+1, PageSize) - PhysAddr);
        }

        // Create MemAttrs object
        unique_ptr<SmmuMemAttrs> pSmmuMemAttrs(
            new SmmuMemAttrs(m_pSurface, Offset, PageSize));
        CHECK_RC(pSmmuMemAttrs->InitMemAttrsLevel(
            pSmmuMemAttrs->GetActivePteLevel()));

        //
        // create MemMapping object(s)
        //
        m_MemMappings.push_back(
            new PmMemMapping(m_pSurface, Offset, Size,
            hMem, MemOffset, PhysAddr, mappingType, this,
            pSmmuMemAttrs.release()));
    }

    return rc;
}

RC PmMemMappingsHelper::ExploreGmmuMemMappings
(
    UINT64 RangeSize,
    PmMemMapping::MappingType mappingType
)
{
    RC rc;

    MmuLevelTree *pMmuLevelTree = m_pSurface->GetMmuLevelTree();
    if (!pMmuLevelTree)
    {
        ErrPrintf("%s: MmuLevelTree has not been created.\n", __FUNCTION__);
        return RC::SOFTWARE_ERROR;
    }

    // Loop that initializes m_MemMappings.  The first iteration
    // creates the PmMemMapping at offset 0, and subsequent iterations
    // create elements at successive offsets.
    //
    for (UINT64 Offset = 0;  Offset < RangeSize;
         Offset += m_MemMappings.back()->GetSize())
    {
        UINT64 Size = 0;
        UINT64 PageSize = 0;
        UINT64 PhysAddr = 0;
        GMMU_APERTURE Aperture = GMMU_APERTURE_ILWALID;
        LwRm::Handle hMem = m_pSurface->GetOrigMemHandle(Offset);
        unique_ptr<GmmuMemAttrs> pGmmuMemAttrs;

        while (Offset + Size < RangeSize)
        {
            UINT64 NextOffset = Offset + Size;
            UINT64 NextPageSize = 0;
            UINT64 NextPhysAddr = 0;
            GMMU_APERTURE NextAperture = GMMU_APERTURE_ILWALID;

            CHECK_RC(pMmuLevelTree->GetSurfaceActivePageSize(
                NextOffset, &NextPageSize));
            CHECK_RC(pMmuLevelTree->GetSurfacePhysAddr(
                NextOffset, NextPageSize, &NextPhysAddr, &NextAperture));
            MASSERT(NextPageSize != 0);

            GmmuMemAttrs NextGmmuMemAttrs(m_pSurface, NextOffset, NextPageSize);
            CHECK_RC(NextGmmuMemAttrs.InitMemAttrsLevel(
                NextGmmuMemAttrs.GetActivePteLevel()));

            if (Size == 0)
            {
                // init round
                PageSize = NextPageSize;
                PhysAddr = NextPhysAddr;
                Aperture = NextAperture;
                pGmmuMemAttrs.reset(new GmmuMemAttrs(m_pSurface, Offset, PageSize));
                CHECK_RC(pGmmuMemAttrs->InitMemAttrsLevel(
                    pGmmuMemAttrs->GetActivePteLevel()));
            }
            else if (PageSize != NextPageSize ||
                     PhysAddr + Size != NextPhysAddr ||
                     Aperture != NextAperture ||
                     !pGmmuMemAttrs->IsSameMemAttrs(&NextGmmuMemAttrs))
            {
                // Each PmMemMapping has
                //  1. same active page sie
                //  2. contiguous PA with same aperutre
                //  3. same mem attributes
                break;
            }
            Size = min(RangeSize, ALIGN_UP(NextPhysAddr+1, PageSize) - PhysAddr);
        }

        //
        // create MemMapping object(s)
        //
        if (mappingType == PmMemMapping::Gmmu_Default)
        {
            // Correct mapping type according to PA
            // If PA in GMMU == memory PA allocated by RM => Gmmu_SysmemPa.
            mappingType = PmMemMapping::Gmmu_SysmemPa;
        }

        m_MemMappings.push_back(
            new PmMemMapping(m_pSurface, Offset, Size,
            hMem, Offset, PhysAddr, mappingType, this,
            pGmmuMemAttrs.release()));
    }

    return OK;
}

//--------------------------------------------------------------------
//! Private method that does a binary search of m_MemMappings for a
//! requested offset.
//!
//! \param Offset The offset to search for.  This This method MASSERTs
//!     if the offset isn't found.
//! \return Index into m_MemMappings of the PmMemMapping object that
//!     contains Offset
//!
UINT32 PmMemMappingsHelper::FindMemMapping(UINT64 Offset) const
{
    MASSERT(m_MemMappings.size() > 0);

    UINT32 lo = 0;
    UINT32 hi = static_cast<UINT32>(m_MemMappings.size() - 1);

    while (lo < hi)
    {
        UINT32 mid = (lo + hi) / 2;
        if (Offset < m_MemMappings[mid]->GetOffset())
        {
            hi = mid - 1;
        }
        else if (Offset >= m_MemMappings[mid]->GetOffset() +
                           m_MemMappings[mid]->GetSize())
        {
            lo = mid + 1;
        }
        else
        {
            lo = mid;
            hi = mid;
        }
    }

    if (Offset < m_MemMappings[lo]->GetOffset() ||
        Offset >= m_MemMappings[lo]->GetOffset() +
                  m_MemMappings[lo]->GetSize())
    {
        MASSERT(!"Cannot find desired offset in PmSurface::FindMemMapping()");
    }
    return lo;
}

//--------------------------------------------------------------------
//! Private method to get the physical address of of a system memory
//! allocation.
//!
//! \param hMem The handle to the memory allocation.
//! \param Offset The offset within the memory allocation.
//! \param pGpuDev Pointer to the GpuDevice where the surface resides
//! \param pPhysAddr Pointer to the returned physical address.
//!
//! \return OK if successful, not OK otherwise
//!
RC PmMemMappingsHelper::GetSysmemPhysAddr
(
    LwRm::Handle hMem,
    UINT64       Offset,
    UINT32       TargetVASpaceClass,
    GpuSubdevice *pGpuSubdevice,
    UINT64      *pPhysAddr
) const
{
    RC      rc;
    LwRm* pLwRm = GetSurface()->GetLwRmPtr();
#ifdef LW_VERIF_FEATURES
    const MdiagSurf *pMdiagSurf = m_pSurface->GetMdiagSurf();
    if (pMdiagSurf->IsUseVidHeapAlloc() && TargetVASpaceClass)
    {
        LW0041_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
        params.memOffset  = Offset;
        params.mmuContext = TargetVASpaceClass;

        CHECK_RC(pLwRm->Control(hMem,
                                LW0041_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                                &params, sizeof(params)));
        *pPhysAddr = params.memOffset;
    }
    else
    {
        LW003E_CTRL_GET_SURFACE_PHYS_ATTR_PARAMS params = {0};
        params.memOffset = Offset;

        CHECK_RC(pLwRm->Control(hMem,
                                LW003E_CTRL_CMD_GET_SURFACE_PHYS_ATTR,
                                &params, sizeof(params)));
        *pPhysAddr = params.memOffset;
    }
#else
    void        *VirtualAddr;
    CHECK_RC(pLwRm->MapMemory(hMem, Offset, 1, &VirtualAddr,
                              DRF_DEF(OS33, _FLAGS, _MAPPING, _DIRECT),
                              pGpuSubdevice));
    *pPhysAddr = Platform::VirtualToPhysical(VirtualAddr);
    pLwRm->UnmapMemory(hMem, VirtualAddr,
                       DRF_DEF(OS33, _FLAGS, _MAPPING, _DIRECT),
                       pGpuSubdevice);
#endif
    return rc;
}

//--------------------------------------------------------------------
//! debug function to print information
//!
void PmMemMappingsHelper::PrintMemMappingsInfo() const
{
    InfoPrintf("%s: Print information of memmappings\n", __FUNCTION__);

    for (UINT32 ii = 0; ii < m_MemMappings.size(); ++ii)
    {
        PmMemMapping *pMemMapping = m_MemMappings[ii];

        InfoPrintf("pMemMapping = %p --- idx(%d): "
                   "hMemHandle = 0x%x VirtAddr = 0x%08x_%08x"
                   "PhysAddr = 0x%08x_%08x Offset = 0x%llx"
                   "Size = 0x%llx   Flags = 0x%x PageSize = 0x%llx\n",
                   pMemMapping, ii,
                   pMemMapping->GetMemHandle(),
                   (UINT32)(pMemMapping->GetVirtAddr() >> 32),
                   (UINT32)pMemMapping->GetVirtAddr(),
                   (UINT32)(pMemMapping->GetPhysAddr() >> 32),
                   (UINT32)pMemMapping->GetPhysAddr(),
                   pMemMapping->GetOffset(),
                   pMemMapping->GetSize(),
                   pMemMapping->GetPteFlags(),
                   pMemMapping->GetPageSize());
    }

    for (UINT32 ii = 0; ii < m_MemMappings.size(); ++ii)
    {
        PmMemMapping *pMemMapping = m_MemMappings[ii];

        if (pMemMapping->GetMemMappingsHelper())
        {
            InfoPrintf("Parent pMemMapping = %p\n", pMemMapping);
            pMemMapping->GetMemMappingsHelper()->PrintMemMappingsInfo();
        }
    }
}

MmuLevelTree::LevelIndex PmMemRange::GetLevelIndex(UINT64 pagesize, PolicyManager::Level mmuLevelType) const
{
    MmuLevelTree::LevelIndex LevelNumInt = MmuLevelTree::GMMU_LEVEL_PDE_LAST;
    if(PolicyManager::LEVEL_PTE == mmuLevelType)
    {
        switch(pagesize)
        {
            case PolicyManager::BYTES_IN_SMALL_PAGE:
                LevelNumInt = MmuLevelTree::GMMU_LEVEL_PTE_4K;
                break;
            case PolicyManager::BYTES_IN_512MB_PAGE:
                LevelNumInt = MmuLevelTree::GMMU_LEVEL_PTE_512M;
                break;
            case PolicyManager::BYTES_IN_HUGE_PAGE:
                LevelNumInt = MmuLevelTree::GMMU_LEVEL_PTE_2M;
                break;
            default:
                LevelNumInt = MmuLevelTree::GMMU_LEVEL_PTE_64K;
                break;
        }
    }
    else if (PolicyManager::LEVEL_PDE0 == mmuLevelType)
    {
        LevelNumInt = MmuLevelTree::GMMU_LEVEL_PDE0;
    }
    else if (PolicyManager::LEVEL_PDE1 == mmuLevelType)
    {
        LevelNumInt = MmuLevelTree::GMMU_LEVEL_PDE1;
    }
    else if (PolicyManager::LEVEL_PDE2 == mmuLevelType)
    {
        LevelNumInt = MmuLevelTree::GMMU_LEVEL_PDE2;
    }
    else if (PolicyManager::LEVEL_PDE3 == mmuLevelType)
    {
        LevelNumInt = MmuLevelTree::GMMU_LEVEL_PDE3;
    }
    else
    {
        MASSERT(!"Unsupported LevelNum");
    }

    return LevelNumInt;
}

//--------------------------------------------------------------------
//! \brief Create SET_PTE_INFO structs for read-modify-write operations
//!
//! This method is used by actions that do read-modify-write
//! operations on PTEs, using LW0080_CTRL_CMD_DMA_GET_PTE_INFO to read
//! and LW0080_CTRL_CMD_DMA_SET_PTE_INFO to write.
//!
//! Given a set of PmMemRanges, this function creates a set of
//! SET_PTE_INFO structs in which the fields are all set to the
//! current values in the PTEs.  It's up to the caller to modify any
//! fields he wants to change before sending the structs to the RM.
//!
//! Note that the values written by this operation will tend to be
//! erased by the next LW0080_CTRL_DMA_FILL_PTE_MEM
//! (PmMemMapping::ModifyPtes).
//!
//! \param pRanges The ranges that the caller wants to modify the PTEs
//!     for.
//! \param[out] pParams The SET_PTE_INFO structs returned to the caller.
//! \param[out] pGpuDevices The GpuDevice for each struct in pParams.
//!
/* static */ RC PmMemRange::CreateSetPteInfoStructs
(
    const PmMemRanges *pRanges,
    vector<LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS> *pParams,
    GpuDevices *pGpuDevices,
    vector<LwRm*> *pLwRms
)
{
    MASSERT(pRanges != NULL);
    MASSERT(pParams != NULL);
    MASSERT(pGpuDevices != NULL);
    MASSERT(LW0080_CTRL_DMA_SET_PTE_INFO_PTE_BLOCKS >=
            LW0080_CTRL_DMA_GET_PTE_INFO_PTE_BLOCKS);
    RC rc;

    pParams->clear();
    pGpuDevices->clear();

    for (PmMemRanges::const_iterator pRange = pRanges->begin();
         pRange != pRanges->end(); ++pRange)
    {
        GpuDevice *pGpuDevice = pRange->GetMdiagSurf()->GetGpuDev();
        PmMemMappingsHelper *pMappingsHelper;
        pMappingsHelper = pRange->GetSurface()->GetMemMappingsHelper();
        PmMemMappings mappings = pMappingsHelper->GetMemMappings(&(*pRange));

        for (UINT64 gpuAddr = pRange->GetGpuAddr();
             gpuAddr < pRange->GetGpuAddr() + pRange->GetSize();
             gpuAddr = ALIGN_UP(gpuAddr + 1, mappings[0]->GetPageSize()))
        {
            while (gpuAddr >=
                   mappings[0]->GetGpuAddr() + mappings[0]->GetSize())
            {
                mappings.erase(mappings.begin());
                MASSERT(!mappings.empty());
            }

            LwRm* pLwRm = pRange->GetSurface()->GetLwRmPtr();
            LW0080_CTRL_DMA_SET_PTE_INFO_PARAMS setParams = {0};
            MmuLevelTreeManager * pMmuLevelMgr = MmuLevelTreeManager::Instance();
            CHECK_RC(pMmuLevelMgr->SetPteInfoStruct(pLwRm, gpuAddr, pGpuDevice, &setParams));

            pParams->push_back(setParams);
            pGpuDevices->push_back(pGpuDevice);
            pLwRms->push_back(pLwRm);
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief constructor
//!
PmAtsRange::PmAtsRange
(
    PmSurface *surface,
    UINT64 offset,
    UINT64 size
) :
    m_Surface(surface),
    m_Offset(offset),
    m_Size(size)
{
    if (m_Surface != 0)
    {
        m_Surface->AddRef();
    }
}

//--------------------------------------------------------------------
//! \brief destructor
//!
PmAtsRange::~PmAtsRange()
{
    if (m_Surface != 0)
    {
        m_Surface->Release();
    }
}
