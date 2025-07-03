/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2016-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "amapv1fb.h"

#ifndef MATS_STANDALONE
#include "core/include/script.h"
#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "gpu/include/floorsweepimpl.h"
#include "gpu/include/gpudev.h"
#endif

// Used to swizzle some values from AMAP that require post-processing
//
#define AMAP_BANK_RANK  3:3
#define AMAP_BANK_BANK  2:0

#ifndef MATS_STANDALONE
//! AmapV1FrameBuffer JS Class
JS_CLASS(AmapV1FrameBuffer);
static SObject AmapV1FrameBuffer_Object
(
    "AmapV1FrameBuffer",
    AmapV1FrameBufferClass,
    0,
    0,
    "AmapV1FrameBuffer JS Object"
);
PROP_READWRITE_NAMESPACE(AmapV1FrameBuffer, SkipParseRowRemapTable, bool,
                         "Skip parsing the row remapper table in amapinit");

#endif

//----------------- Static Functions and Variables -------------------
bool AmapV1FrameBuffer::s_SkipParseRowRemapTable = false;

bool AmapV1FrameBuffer::GetSkipParseRowRemapTable()
{
    return s_SkipParseRowRemapTable;
}

RC AmapV1FrameBuffer::SetSkipParseRowRemapTable(bool bEnable)
{
    s_SkipParseRowRemapTable = bEnable;
    return RC::OK;
}

//--------------------------------------------------------------------
AmapV1FrameBuffer::AmapV1FrameBuffer
(
    const char   *name,
    GpuSubdevice *pGpuSubdevice,
    LwRm         *pLwRm,
    AmapLitter    litter
) :
    GpuFrameBuffer(name, pGpuSubdevice, pLwRm)
{
    m_ChipConf.litter = litter;
}

//--------------------------------------------------------------------
AmapV1FrameBuffer::~AmapV1FrameBuffer()
{
    // Delete the DRAM HAL object that was allocated in Initialize()
    //
    if (m_pDramConf != nullptr)
    {
        freeDramConfV1(m_pDramConf);
        m_pDramConf = nullptr;
    }

    // Delete the AmapConf objects that were allocated in Initialize()
    //
    if (m_pAmapConfEccOff != nullptr)
    {
        freeAmapConfV1(m_pAmapConfEccOff);
    }
    if (m_pAmapConfEccOn != nullptr && m_pAmapConfEccOn != m_pAmapConfEccOff)
    {
        freeAmapConfV1(m_pAmapConfEccOn);
    }
    m_pAmapConfEccOff = nullptr;
    m_pAmapConfEccOn  = nullptr;
    m_pAmapConf       = nullptr;
}

//--------------------------------------------------------------------
//! \brief The common parts of Initialize() that can be inherited
//!
/* virtual */ RC AmapV1FrameBuffer::InitCommon()
{
    GpuSubdevice* pSubdev = GpuSub();
    RC rc;

    // Initialize the GPU & DRAM info
    //
    CHECK_RC(GpuFrameBuffer::InitCommon());

    // If the GPU has two LTCs per FBPA, and one LTC is floorswept on
    // one or more FBPAs, it can either be in half-FBPA or
    // balanced-LTC mode.
    // In half-FBPA mode, the half-FBPA mask should have a bit set for
    // each FBPA that has only one LTC.  The LTC drives a
    // subpartition, which is always subpartition #1.
    // In balanced-LTC mode, *all* non-floorswept FBPAs must have a
    // floorswept LTC, which drives both subpartitions, and the
    // half-FBPA mask must be 0.  Each LTC drives both subpartitions;
    // the subpartition number comes from the slice.
    //
    m_HalfFbpaMask = 0;
    m_BalancedLtcMode = false;
    if (GetMaxLtcsPerFbp() == 2 * GetFbiosPerFbp())
    {
        const UINT32 fbpaMask  = GetFbioMask();  // Assume iron grid
        UINT32 derivedHalfFbpaMask = 0;
        m_HalfFbpaMask = pSubdev->GetFsImpl()->HalfFbpaMask() & fbpaMask;
        m_HalfFbpaStride = pSubdev->GetFsImpl()->GetHalfFbpaWidth();
        const UINT32 ltcMask  = GetLtcMask();
        for (INT32 hwFbpa = Utility::BitScanForward(fbpaMask);
             hwFbpa >= 0;
             hwFbpa = Utility::BitScanForward(fbpaMask, hwFbpa + 1))
        {
            if (((ltcMask >> (2 * hwFbpa)) & 3) != 3)
            {
                derivedHalfFbpaMask |= 1 << hwFbpa;
            }
        }

        if (m_HalfFbpaMask == 0 && derivedHalfFbpaMask == fbpaMask)
        {
            // Fused half-fbpa mask is 0 and all FBPAs have just one LTC
            //
            m_BalancedLtcMode = true;
        }
        else if (m_HalfFbpaMask != derivedHalfFbpaMask)
        {
            // Fused half-fbpa mask does not match the derived mask
            //
            Printf(Tee::PriWarn, "Half-FBPA mask is 0x%x, but expected 0x%x\n",
                   m_HalfFbpaMask, derivedHalfFbpaMask);
            m_HalfFbpaMask = derivedHalfFbpaMask;
        }
    }

    // Sanity-check the floorsweeping masks
    //
#ifndef MATS_STANDALONE
    if (Platform::GetSimulationMode() == Platform::Hardware &&
        !pSubdev->IsEmulation())
#endif
    {
        const UINT32 fbpMask     = GetFbpMask();
        const UINT32 fbioMask    = GetFbioMask();
        const UINT32 fbiosPerFbp = GetFbiosPerFbp();
        for (INT32 hwFbio = Utility::BitScanForward(fbioMask); hwFbio >= 0;
             hwFbio = Utility::BitScanForward(fbioMask, hwFbio + 1))
        {
            const UINT32 hwFbp = hwFbio / fbiosPerFbp;
            if (((fbpMask >> hwFbp) & 1) == 0)
            {
                Printf(Tee::PriError,
                       "Floorswept FBP has a non-floorswept FBPA\n");
                return RC::ILWALID_RAM_AMOUNT;
            }
        }
    }

    // Initialize the AMAP and DRAM HAL libraries
    //
    CHECK_RC(InitAmap());
    CHECK_RC(InitDramHal());
    return OK;
}

//--------------------------------------------------------------------
//! \brief Initialize the AMAP library; called from InitCommon().
//!
RC AmapV1FrameBuffer::InitAmap()
{
    RC rc;
    const UINT32 pseudoChannels = GetPseudoChannelsPerSubpartition();

    ConfParamsV1 confParams = {0};
    confParams.litter            = static_cast<AmapLitter>(
                                       static_cast<UINT32>(GetAmapLitter()));
    confParams.numPartitions     = GetLtcCount();
    confParams.numFBs            = GetFbpCount();
    confParams.numSlices         = GetMaxSlicesPerLtc();
    confParams.numBanks          = GetBankCount();
    confParams.numExtBanks       = GetAmapExtBanks();
    confParams.dramPageSize      = GetPseudoChannelsPerChannel() * GetRowSize();
    confParams.numPseudoChannels = pseudoChannels;
    confParams.b2PAsPerFb        = (GetFbiosPerFbp() == 2);
    if (SwizzleHbmRank())
    {
        MASSERT(confParams.numExtBanks == 1 && confParams.numBanks == 8);
        confParams.numExtBanks = 0;
        confParams.numBanks    = 16;
    }

    confParams.isECCOn = false;
    m_pAmapConfEccOff  = configNewAmapV1(&confParams);
    confParams.isECCOn = true;
    m_pAmapConfEccOn   = (IsHbm() ? m_pAmapConfEccOff
                                  : configNewAmapV1(&confParams));
    m_pAmapConf        = IsEccOn() ? m_pAmapConfEccOn : m_pAmapConfEccOff;

    void* allAmapConfs[] =
    {
        m_pAmapConfEccOff,
        m_pAmapConfEccOn != m_pAmapConfEccOff ? m_pAmapConfEccOn : nullptr
    };
    for (void* pAmapConf: allAmapConfs)
    {
        if (pAmapConf)
        {
            // configAmapDualRanksV1 should not be called for GH100+
            if (IsHbm() && m_ChipConf.litter != LITTER_GHLIT1)
            {
                configAmapDualRanksV1(pAmapConf,
                                      GetRankCount() == 2, GetBankCount());
            }
            configAmapBalancedLTCFS(pAmapConf, m_BalancedLtcMode);
            if (!s_SkipParseRowRemapTable && IsRowRemappingOn())
            {
                CHECK_RC(SetupAmapRowRemapping(pAmapConf));
            }
            configAmapSlicesFSV1(pAmapConf, GetL2FSSliceCount()); // floorswept slices
            printAmapConfV1(pAmapConf);
        }
    }

    return rc;
}

RC AmapV1FrameBuffer::SetupAmapRowRemapping(void* pAmapConf)
{
    RC rc;
    MASSERT(IsRowRemappingOn());

    GpuSubdevice* pSubdev = GpuSub();
    RegHal& regs = pSubdev->Regs();

    const UINT32 numActiveFbios = GetFbioCount();
    const UINT32 numSubps = GetSubpartitions();
    const UINT32 numPcs = GetPseudoChannelsPerSubpartition();
    const UINT32 numRanks = GetRankCount();
    const UINT32 numBanks = GetBankCount();
    const UINT32 numRowRemapTableEntries = pSubdev->NumRowRemapTableEntries();
    MASSERT(numRowRemapTableEntries != 0);

    UINT32 numRowsPow2;
    CHECK_RC(CalibratedRead32(MODS_PFB_FBPA_CFG1_ROWA_10, 10,
                              &numRowsPow2));

    // Read all the row remapping entries. Report any remapped rows to AMAP.
    //
    // NOTE: Below assumes 1:1 FBPA:FBIO
    for (UINT32 virtFbio = 0; virtFbio < numActiveFbios; ++virtFbio)
    {
        const UINT32 hwFbpa = HwFbioToHwFbpa(VirtualFbioToHwFbio(virtFbio));

        for (UINT32 subp = 0; subp < numSubps; ++subp)
        {
            for (UINT32 pc = 0; pc < numPcs; ++pc)
            {
                for (UINT32 rank = 0; rank < numRanks; ++rank)
                {
                    for (UINT32 bank = 0; bank < numBanks; ++bank)
                    {
                        for (UINT32 entry = 0; entry < numRowRemapTableEntries; ++entry)
                        {
                            // Write entry selection for table entry read
                            UINT32 entrySelVal = 0;
                            regs.SetField(&entrySelVal, MODS_PFB_FBPA_0_ROW_REMAP_RD_TRIGGER, 1);
                            regs.SetField(&entrySelVal, MODS_PFB_FBPA_0_ROW_REMAP_RD_EXT_BANK, rank);
                            regs.SetField(&entrySelVal, MODS_PFB_FBPA_0_ROW_REMAP_RD_BANK, bank);
                            regs.SetField(&entrySelVal, MODS_PFB_FBPA_0_ROW_REMAP_RD_ENTRY, entry);
                            regs.Write32(MODS_PFB_FBPA_0_ROW_REMAP_RD, {hwFbpa, subp}, entrySelVal);

                            // Read the entry
                            const UINT32 entryVal = regs.Read32(MODS_PFB_FBPA_0_ROW_REMAP_RD,
                                                                {hwFbpa, subp});
                            const bool isEntryValid
                                = regs.GetField(entryVal, MODS_PFB_FBPA_0_ROW_REMAP_RD_ENTRY_VLD,
                                            {hwFbpa, subp}) != 0;

                            if (isEntryValid)
                            {
                                // Found remapped row, tell AMAP
                                Printf(Tee::PriLow, "AMAP: setting row remap table "
                                    "on HW FBPA %u, subp %u, rank %u, bank %u, entry %u\n",
                                    entry, hwFbpa, subp, rank, bank);

                                const UINT32 logicalRow
                                    = regs.GetField(entryVal, MODS_PFB_FBPA_0_ROW_REMAP_RD_ROW,
                                                    {hwFbpa, subp});

                                RowRemappingInputParamsV1 rowRemapParams = {};
                                rowRemapParams.index      = entry;
                                rowRemapParams.logicalRow = logicalRow;
                                rowRemapParams.FBPA       = hwFbpa;
                                rowRemapParams.subp       = subp;
                                rowRemapParams.pc         = pc;
                                rowRemapParams.rank       = rank;
                                rowRemapParams.bank       = bank;

                                configAmapRowRemappingV1(pAmapConf,
                                                         numRowsPow2,
                                                         &rowRemapParams);
                            }
                        }
                    }
                }
            }
        }
    }

    return rc;
}

//--------------------------------------------------------------------
//! \brief Create virtual <-> hw Ltc mapping and pass it onto DramHal
//!
RC AmapV1FrameBuffer::ConfigureLtcMapping()
{
    RegHal& regs = GpuSub()->Regs();

    // For HBM, there's a 1:1 LTC - FBPA mapping
    // For some GDDR chips (GA10x, TU10x), there are multiple LTC per FBPA
    // This function assumes a 1:1 mapping, so if this assumption is no longer
    // valid, or we want to extend this function to GDDR chips, this assertion
    // should be removed
    //
    // However, we can't assert on LtcCount==FbioCount since LTC floorsweeping on HBM3
    // does not require FBIO being floorswept as well
    vector<UINT32> virtToHwLtcMapping;
    for (UINT32 virtLtc = 0; virtLtc < GetLtcCount(); virtLtc++)
    {
        // Get hw FBPA from virtual LTC
        const UINT32 hwFbpaId =
            regs.Read32(MODS_PLTCG_LTCx_MISC_FB_ID_VAL, virtLtc);
        // 1:1 mapping of FBPA : LTC
        const UINT32 hwLtc = hwFbpaId;
        virtToHwLtcMapping.push_back(static_cast<UINT32>(hwLtc));
    }

    configLogicalLTC2Physical(m_pDramConf,
                              GetLtcCount(),
                              virtToHwLtcMapping.data());

    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Initialize the DRAM HAL library; called from InitCommon().
//!
//! If the DRAM HAL doesn't yet support the current memory type, then
//! set m_pDramConf to nullptr and return OK.  This isn't an error; it
//! just means that we'll have to decode the data without DRAM HAL.
//!
RC AmapV1FrameBuffer::InitDramHal()
{
    RC rc;

    const UINT32 fbioMask = GetFbioMask();
    const UINT32 ltcMask  = GetLtcMask();

    // Colwert the DRAM type (e.g. HBM2) from the enum that MODS uses
    // to the one the DRAM HAL uses.
    //
    static const map<RamProtocol, DramType> dramTypeTable =
    {
        // Not yet supported: { RamGDDR5,  DRAM_TYPE_GDDR5  },
        { RamHBM1,   DRAM_TYPE_HBM1   },
        { RamHBM2,   DRAM_TYPE_HBM2   },
        { RamHBM3,   DRAM_TYPE_HBM3   },
        { RamGDDR5X, DRAM_TYPE_GDDR5X },
        { RamGDDR6,  DRAM_TYPE_GDDR6  },
        { RamGDDR6X, DRAM_TYPE_GDDR6X },
    };

    auto pDramType = dramTypeTable.find(GetRamProtocol());
    if (pDramType == dramTypeTable.end())
    {
        m_pDramConf = nullptr;   // Unsupported memory type.
        return OK;
    }
    m_ChipConf.dramType = pDramType->second;

    // Initialize the DRAM HAL
    //
    DramConfParamsV1 confParams;
    memset(&confParams, 0, sizeof(confParams));
    confParams.type              = m_ChipConf.dramType;
    confParams.litter            = static_cast<AmapLitter>(
                                       static_cast<UINT32>(GetAmapLitter()));
    confParams.numPseudoChannels = GetPseudoChannelsPerSubpartition();
    confParams.burstLength       = GetBurstSize() / GetBeatSize();
    confParams.numRanks          = GetRankCount();
    confParams.numBanksPerRank   = GetBankCount();
    confParams.fsLtcMask         = ltcMask;
    confParams.fsFbioMask        = fbioMask;
    confParams.fsHalfFbpaEnMask  = GetHalfFbpaMask();

    m_pDramConf = configNewDramHalV1(&confParams);
    printDramHalConfigV1(m_pDramConf);

    // Starting GH100, we need to configure virt <-> hw ltc mapping manually by
    // reading registers
    //
    // Disable for Fmodel for now since it fails vdvs
    // (Registers might be broken in Fmodel)
    if (IsHbm()
        && GpuSub()->Regs().IsSupported(MODS_PLTCG_LTCx_MISC_FB_ID_VAL, 0)
#ifndef MATS_STANDALONE
        && Platform::GetSimulationMode() != Platform::Fmodel
#endif
        )
    {
        MASSERT(m_pDramConf);
        CHECK_RC(ConfigureLtcMapping());
    }

    return OK;
}

//--------------------------------------------------------------------
//! \brief Colwert the FBIO & subpartition to the HBM site & channel
//!
//! Return UNSUPPORTED_FUNCTION if this is not an HBM chip
//!
/* virtual */ RC AmapV1FrameBuffer::FbioSubpToHbmSiteChannel
(
    UINT32  hwFbio,
    UINT32  subpartition,
    UINT32 *pHbmSite,
    UINT32 *pHbmChannel
) const
{
    // Legacy function for HBM2/HBM2e chips
    if (!IsHbm() || IsHbm3())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Use DRAM_HAL to get the site & channel
    //
    uint32_t hbmSite    = 0;
    uint32_t hbmChannel = 0;
    mapHwFbioSubpToFbioBrickIfV1(&m_ChipConf, hwFbio, subpartition,
                                 &hbmSite, &hbmChannel);

    // Sanity-check the results
    //
    MASSERT(GetChannelsPerSite() > 0);
    MASSERT(hbmSite < (GetMaxFbioCount() * GetChannelsPerFbio() /
                       GetChannelsPerSite()));
    MASSERT(hbmChannel < GetChannelsPerSite());

    // Return
    //
    *pHbmSite    = hbmSite;
    *pHbmChannel = hbmChannel;
    return RC::OK;
}

//--------------------------------------------------------------------
//! \brief Colwert the FBIO & subpartition to the HBM site & channel
//! For HBM3 Only
//! Return UNSUPPORTED_FUNCTION if this is not HBM3
//!
/* virtual */ RC AmapV1FrameBuffer::FbioSubpToHbmSiteChannel
(
    UINT32  hwFbio,
    UINT32  subpartition,
    UINT32  pseudoChannel,
    UINT32 *pHbmSite,
    UINT32 *pHbmChannel
) const
{
    if (GetRamProtocol() != FrameBuffer::RamHBM3)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Use DRAM_HAL to get the site & channel
    //
    uint32_t hbmSite    = 0;
    uint32_t hbmChannel = 0;
    mapHwFbioSubpToFbioBrickIfV2(&m_ChipConf, hwFbio, subpartition, pseudoChannel,
                                 &hbmSite, &hbmChannel);

    // Sanity-check the results
    //
    MASSERT(GetChannelsPerSite() > 0);
    MASSERT(hbmSite < (GetMaxFbioCount() * GetChannelsPerFbio() /
                       GetChannelsPerSite()));
    MASSERT(hbmChannel < GetChannelsPerSite());

    // Return
    //
    *pHbmSite    = hbmSite;
    *pHbmChannel = hbmChannel;
    return RC::OK;
}

/* virtual */ RC AmapV1FrameBuffer::HbmSiteChannelToFbioSubp
(
    UINT32  hbmSite,
    UINT32  hbmChannel,
    UINT32 *pHwFbio,
    UINT32 *pSubpartition
) const
{
    if (!IsHbm())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // Use DRAM_HAL to get the FBIO & subpartition
    //
    uint32_t hwFbio       = 0;
    uint32_t subpartition = 0;
    mapFbioBrickIfToHwFbioSubpV1(&m_ChipConf, hbmSite, hbmChannel,
                                 &hwFbio, &subpartition);

    // Sanity-check the results
    //
    MASSERT(hwFbio < GetMaxFbioCount());
    MASSERT(subpartition < GetSubpartitions());

    // Return
    //
    *pHwFbio = hwFbio;
    *pSubpartition = subpartition;
    return OK;
}

//--------------------------------------------------------------------
//! \brief Colwert a virtual LTC number to HW LTC
//!
//! In GA100+ HBM, the virtual->HW LTC mapping isn't just a matter
//! unpacking the floorsweeping. They also swizzle the numbers,
//! requiring a dram_hal lookup table.
//!
UINT32 AmapV1FrameBuffer::VirtualLtcToHwLtc(UINT32 virtualLtc) const
{
    UINT32 hwLtc = 0;
    if (m_pDramConf && IsHbm())
    {
        mapLogicalLTCToPhysical(m_pDramConf, virtualLtc, &hwLtc);
    }
    else
    {
        hwLtc = GpuFrameBuffer::VirtualLtcToHwLtc(virtualLtc);
    }
    return hwLtc;
}

//--------------------------------------------------------------------
//! \brief Colwert a HW LTC number to virtual LTC
//!
//! In GA100+ HBM, the HW->virtual LTC mapping isn't just a matter
//! packing the floorsweeping. They also swizzle the numbers,
//! requiring a dram_hal lookup table.
//!
UINT32 AmapV1FrameBuffer::HwLtcToVirtualLtc(UINT32 hwLtc) const
{
    UINT32 virtualLtc = 0;
    if (m_pDramConf && IsHbm())
    {
        mapPhysicalLTCToLogical(m_pDramConf, hwLtc, &virtualLtc);
    }
    else
    {
        virtualLtc = GpuFrameBuffer::HwLtcToVirtualLtc(hwLtc);
    }
    return virtualLtc;
}

//--------------------------------------------------------------------
//! Colwert an RBC address to physical addresses
//!
/* virtual */ RC AmapV1FrameBuffer::EncodeAddress
(
    const FbDecode &decode,
    UINT32          pteKind,
    UINT32          pageSizeKB,
    UINT64         *pEccOnAddr,
    UINT64         *pEccOffAddr
) const
{
    MASSERT(pEccOnAddr);
    MASSERT(pEccOffAddr);
    const UINT32 amapColumnSize = GetAmapColumnSize();
    const UINT32 rowOffset      = GetRowOffset(decode.burst, decode.beat,
                                               decode.beatOffset);
    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    const UINT32 hwFbio       = VirtualFbioToHwFbio(decode.virtualFbio);
    const UINT32 subpartition = decode.subpartition;
    const UINT32 hwLtc        = HwFbioToHwLtc(hwFbio, subpartition);
    const UINT32 virtualLtc   = HwLtcToVirtualLtc(hwLtc);

    DramParamsV1 dramParams = {0};
    dramParams.partition     = virtualLtc;
    dramParams.subPartition  = EncodeAmapSubpartition(subpartition, virtualLtc);
    dramParams.pseudoChannel = decode.pseudoChannel;
    dramParams.extBank       = decode.rank;
    dramParams.bank          = decode.bank;
    dramParams.row           = decode.row;
    dramParams.column        = rowOffset / amapColumnSize;
    if (SwizzleHbmRank())
    {
        dramParams.bank = (REF_NUM(AMAP_BANK_RANK, dramParams.extBank) |
                           REF_NUM(AMAP_BANK_BANK, dramParams.bank));
        dramParams.extBank = 0;
    }

    // WAR bug 3481353
    // TODO: Have AMAP create an interface for MODS to pass in flip strap
    // register values instead of flipping subp/pc in MODS
    HbmFlipSubpPc(hwFbio, &dramParams.subPartition, &dramParams.pseudoChannel);

    // Unswizzle pseudochannels if necessary (e.g. block-linear on some GPUs)
    removeFBRawFromDRAMV1(m_pAmapConfEccOn, &memParams, &dramParams);

    uint64_t fbOffset = 0;
    mapDramToPaV1(m_pAmapConfEccOn, &memParams, &dramParams, &fbOffset);
    *pEccOnAddr = fbOffset + (rowOffset % amapColumnSize);
    if (m_pAmapConfEccOff == m_pAmapConfEccOn)
    {
        *pEccOffAddr = *pEccOnAddr;
    }
    else
    {
        mapDramToPaV1(m_pAmapConfEccOff, &memParams, &dramParams, &fbOffset);
        *pEccOffAddr = fbOffset + (rowOffset % amapColumnSize);
    }
    return OK;
}

//--------------------------------------------------------------------
//! Colwert a physical address to RBC
//!
/* virtual */ RC AmapV1FrameBuffer::DecodeAddress
(
    FbDecode* pDecode,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSizeKB
) const
{
    RC rc;
    const UINT64 alignedAddr = ALIGN_DOWN(fbOffset, GetAmapColumnSize());
    const UINT32 alignOffset = static_cast<UINT32>(fbOffset - alignedAddr);
    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    DramParamsV1 dramParams = {0};
    XbarParamsV1 xbarParams = {0};
    mapPaToDramV1(m_pAmapConf, &memParams, alignedAddr, &dramParams);
    mapPaToXbarV1(m_pAmapConf, &memParams, alignedAddr, &xbarParams);

    // Swizzle pseudochannels if necessary (e.g. for block-linear on some GPUs)
    addFBRawInToDRAMV1(m_pAmapConf, &memParams, &dramParams);

    const UINT32 hwLtc        = VirtualLtcToHwLtc(dramParams.partition);
    const UINT32 hwFbio       = HwLtcToHwFbio(hwLtc);
    const UINT32 virtualFbio  = HwFbioToVirtualFbio(hwFbio);
    const UINT32 rowOffset    = ((dramParams.column * GetAmapColumnSize()) +
                                 alignOffset);

    // WAR bug 3481353
    // TODO: Have AMAP create an interface for MODS to pass in flip strap
    // register values instead of flipping subp/pc in MODS
    HbmFlipSubpPc(hwFbio, &dramParams.subPartition, &dramParams.pseudoChannel);

    memset(pDecode, 0, sizeof(*pDecode));
    pDecode->virtualFbio    = virtualFbio;
    pDecode->subpartition   = DecodeAmapSubpartition(dramParams.subPartition,
                                                     hwLtc);
    pDecode->pseudoChannel  = dramParams.pseudoChannel;
    pDecode->rank           = dramParams.extBank;
    pDecode->bank           = dramParams.bank;
    pDecode->row            = static_cast<UINT32>(dramParams.row);
    MASSERT(pDecode->row == dramParams.row);
    pDecode->burst          = rowOffset / GetBurstSize();
    pDecode->beat           = (rowOffset % GetBurstSize()) / GetBeatSize();
    pDecode->beatOffset     = rowOffset % GetBeatSize();
    pDecode->channel        = CallwlateChannel(pDecode->subpartition,
                                               pDecode->pseudoChannel /
                                               GetPseudoChannelsPerChannel());
    pDecode->extColumn      = (IsHbm() && !IsHbm3() ?
                               pDecode->burst * GetPseudoChannelsPerChannel() :
                               pDecode->burst);
    pDecode->slice          = xbarParams.slice;
    pDecode->xbarRawAddr    = xbarParams.padr;

    if (SwizzleHbmRank())
    {
        pDecode->rank = REF_VAL(AMAP_BANK_RANK, pDecode->bank);
        pDecode->bank = REF_VAL(AMAP_BANK_BANK, pDecode->bank);
    }

    if (m_pDramConf)
    {
        HwParamsV1 hwParams = {0};
        const UINT32 dummyBitOffset = (pDecode->burst & 0x7); // pseudo-random
        getDramHalMappingV1(m_pDramConf, alignOffset * 8 + dummyBitOffset,
                            &dramParams, &hwParams);
        MASSERT(hwParams.ltcId         == hwLtc);
        MASSERT(hwParams.fbioId        == hwFbio);
        MASSERT(hwParams.rank          == pDecode->rank);
        MASSERT(hwParams.row           == pDecode->row);
        MASSERT(hwParams.bank          == pDecode->bank);
        MASSERT(hwParams.column        == pDecode->extColumn);
        MASSERT(hwParams.pseudoChannel == pDecode->pseudoChannel);
        MASSERT(hwParams.beat          == pDecode->beat);
        MASSERT(hwParams.pin    == (pDecode->pseudoChannel * GetBeatSize() * 8 +
                                    pDecode->beatOffset * 8 +
                                    dummyBitOffset));
        if (IsHbm())
        {
            // For HBM3, this function requires pseudoChannel as an input
            if (IsHbm3())
            {
                CHECK_RC(FbioSubpToHbmSiteChannel(hwFbio, pDecode->subpartition,
                                    pDecode->pseudoChannel,
                                    &pDecode->hbmSite,
                                    &pDecode->hbmChannel));
            }
            else
            {
                CHECK_RC(FbioSubpToHbmSiteChannel(hwFbio, pDecode->subpartition,
                                                &pDecode->hbmSite,
                                                &pDecode->hbmChannel));
            }
            UINT32 hwFbpa;
            mapAMAPLtcToHwFBPA(m_pDramConf, dramParams.partition, &hwFbpa);
            MASSERT(hwFbpa == HwFbioToHwFbpa(hwFbio));
        }
    }

    const Tee::Priority pri = m_VerbosePrintPri;
    Printf(pri, "FrameBuffer::Decode addr=0x%llx, kind=0x%x, pageSizeKB=%u\n",
           fbOffset, pteKind, pageSizeKB);
    Printf(pri, "    virtualFbio   = 0x%x\n", pDecode->virtualFbio);
    Printf(pri, "    subpartition  = 0x%x\n", pDecode->subpartition);
    Printf(pri, "    pseudoChannel = 0x%x\n", pDecode->pseudoChannel);
    Printf(pri, "    rank          = 0x%x\n", pDecode->rank);
    Printf(pri, "    bank          = 0x%x\n", pDecode->bank);
    Printf(pri, "    row           = 0x%x\n", pDecode->row);
    Printf(pri, "    burst         = 0x%x\n", pDecode->burst);
    Printf(pri, "    beat          = 0x%x\n", pDecode->beat);
    Printf(pri, "    beatOffset    = 0x%x\n", pDecode->beatOffset);
    Printf(pri, "    hbmSite       = 0x%x\n", pDecode->hbmSite);
    Printf(pri, "    extColumn     = 0x%x\n", pDecode->extColumn);
    Printf(pri, "    slice         = 0x%x\n", pDecode->slice);
    Printf(pri, "    xbarRawAddr   = 0x%llx\n", pDecode->xbarRawAddr);
    return rc;
}

//--------------------------------------------------------------------
//! Colwert a physical address to the bit-packed RBC format used by
//! the LW_PFB_FBPA_*_ECC_ADDR & LW_PFB_FBPA_*_ECC_ADDR_EXT registers
//! when the GPU reports an ECC fault.  Used by trace3d plugins.
//!
/* virtual */ void AmapV1FrameBuffer::GetRBCAddress
(
    EccAddrParams *pRbcAddr,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    UINT32 errCount,
    UINT32 errPos
)
{
    MASSERT(pRbcAddr);

    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccPaParamsV1 eccPaParams = {0};
    eccPaParams.pa = fbOffset;
    eccPaParams.EccErrInjection4BOffset = errPos;

    EccAddrParamsV1 eccAddrParams = {0};
    mapPaToEccAddrV1(m_pAmapConf, &memParams, &eccPaParams, &eccAddrParams);

    memset(pRbcAddr, 0, sizeof(*pRbcAddr));
    pRbcAddr->eccAddr      = eccAddrParams.ECC_ADDR;
    pRbcAddr->eccAddrExt   = eccAddrParams.ECC_ADDR_EXT;
    pRbcAddr->extBank      = eccAddrParams.extBank;
    pRbcAddr->subPartition = eccAddrParams.subPartition;
    pRbcAddr->partition    = eccAddrParams.partition;
}

//------------------------------------------------------------------------------
RC AmapV1FrameBuffer::EccAddrToPhysAddr
(
    const EccAddrParams & eccAddr,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    PHYSADDR *pPhysAddr
)
{
    MASSERT(pPhysAddr);

    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccAddrParamsV1 eccAddrParams = { };
    eccAddrParams.ECC_ADDR      = eccAddr.eccAddr;
    eccAddrParams.ECC_ADDR_EXT  = eccAddr.eccAddrExt;
    eccAddrParams.extBank       = eccAddr.extBank;
    eccAddrParams.subPartition  = eccAddr.subPartition;
    eccAddrParams.partition     = eccAddr.partition;

    EccPaParamsV1 eccPaParams = { };
    mapEccAddrToPaV1(m_pAmapConf, &memParams, &eccAddrParams, &eccPaParams);
    *pPhysAddr = static_cast<PHYSADDR>(eccPaParams.pa);
    return OK;
}

//--------------------------------------------------------------------
//! Colwert a physical address to the fields used by the
//! LW_PFB_FBPA_*_ECC_CONTROL and LW_PFB_FBPA_*_ECC_ERR_INJECTION_ADDR
//! registers to disable ECC checkbits at a specified address.
//!
/* virtual */ void AmapV1FrameBuffer::GetEccInjectRegVal
(
    EccErrInjectParams *pInjectParams,
    UINT64 fbOffset,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    UINT32 errCount,
    UINT32 errPos
)
{
    MASSERT(pInjectParams);

    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccPaParamsV1 paParams = {0};
    paParams.pa = fbOffset;
    paParams.EccErrInjection4BOffset = errPos;

    EccErrInjectParamsV1 injectParams = {0};
    mapPaToEccErrInjectV1(m_pAmapConf, &memParams, &paParams, &injectParams);

    memset(pInjectParams, 0, sizeof(*pInjectParams));
    pInjectParams->eccCtrlWrtKllPtr0 = injectParams.ECC_CONTROL_WRITE_KILL_PTR0;
    pInjectParams->eccErrInjectAddr  = injectParams.ECC_ERR_INJECTION_ADDR;
    pInjectParams->subPartition      = injectParams.subPartition;
    pInjectParams->partition         = injectParams.partition;
}

//--------------------------------------------------------------------
//! \brief Enable verbose dumping of address-decode info, for debugging
//!
/* virtual */ void AmapV1FrameBuffer::EnableDecodeDetails(bool enable)
{
    m_VerbosePrintPri = enable ? Tee::PriNormal : Tee::PriNone;
}

//--------------------------------------------------------------------
//! \brief Disable the FB ECC checkbits at just one address
//!
/* virtual */ RC AmapV1FrameBuffer::DisableFbEccCheckbitsAtAddress
(
    UINT64 physAddr,
    UINT32 pteKind,
    UINT32 pageSizeKB
)
{
    const RegHal &regs = GpuSub()->Regs();
    RC rc;

    // Use amaplib to find the value to write to
    // ECC_ERR_INJECTION_ADDR, as well as the FBIO & channel
    //
    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccPaParamsV1        paParams     = {0};
    EccErrInjectParamsV1 injectParams = {0};

    paParams.pa = physAddr;
    mapPaToEccErrInjectV1(m_pAmapConf, &memParams, &paParams, &injectParams);

    const UINT32 hwLtc   = VirtualLtcToHwLtc(injectParams.partition);
    const UINT32 hwFbio  = HwLtcToHwFbio(hwLtc);
    const UINT32 channel = DecodeAmapSubpartition(injectParams.subPartition,
                                                  hwLtc);

    // Write the registers
    //
    CHECK_RC(WriteEccRegisters(
                    regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL_ADR),
                    regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL),
                    injectParams.ECC_ERR_INJECTION_ADDR, _UINT32_MAX,
                    hwFbio, channel));
    return OK;
}

//--------------------------------------------------------------------
//! \brief Apply FB write-kill at an address
//!
//! See GpuFrameBuffer::ApplyFbEccWriteKillPtr() for a full description
//! \see GpuFrameBuffer::ApplyFbEccWriteKillPtr()
//!
/* virtual */ RC AmapV1FrameBuffer::ApplyFbEccWriteKillPtr
(
    UINT64 physAddr,
    UINT32 pteKind,
    UINT32 pageSizeKB,
    INT32 kptr0,
    INT32 kptr1
)
{
    const RegHal &regs = GpuSub()->Regs();
    RC rc;

    if (!regs.IsSupported(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE_ADR))
        return RC::UNSUPPORTED_FUNCTION;

    // Use amaplib to find the values to write to ECC_CONTROL and
    // ECC_ERR_INJECTION_ADDR at the injection address, as well as the
    // corresponding FBIO & subpartition.
    //
    MemParamsV1 memParams;
    GetMemParams(&memParams, pteKind, pageSizeKB);

    EccPaParamsV1        paParams     = {0};
    EccErrInjectParamsV1 injectParams = {0};

    UINT32 controlValue =
        regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE_ADR);
    UINT32 controlMask =
        regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE);
    if (kptr0 >= 0)
    {
        paParams.pa = physAddr;
        paParams.EccErrInjection4BOffset = (physAddr & 0x3) * 8 + kptr0;
        mapPaToEccErrInjectV1(m_pAmapConf, &memParams,
                              &paParams, &injectParams);
        controlValue |=
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0,
                          injectParams.ECC_CONTROL_WRITE_KILL_PTR0) |
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0_EN_TRUE);
        controlMask |=
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0) |
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0_EN);
    }
    if (kptr1 >= 0)
    {
        paParams.pa = physAddr;
        paParams.EccErrInjection4BOffset = (physAddr & 0x3) * 8 + kptr1;
        mapPaToEccErrInjectV1(m_pAmapConf, &memParams,
                              &paParams, &injectParams);
        controlValue |=
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1,
                          (physAddr % GetFbEccSectorSize()) * 8 + kptr1) |
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1_EN_TRUE);
        controlMask |=
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1) |
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1_EN);
    }
    if (kptr0 < 0 && kptr1 < 0)
    {
        // Make sure we have a valid FBIO & subpart, even if the
        // caller sets both kptr0 & kptr1 to NO_KPTR for some reason.
        paParams.pa = physAddr;
        mapPaToEccErrInjectV1(m_pAmapConf, &memParams,
                              &paParams, &injectParams);
    }

    const UINT32 hwLtc   = VirtualLtcToHwLtc(injectParams.partition);
    const UINT32 hwFbio  = HwLtcToHwFbio(hwLtc);
    const UINT32 channel = DecodeAmapSubpartition(injectParams.subPartition,
                                                  hwLtc);

    // Write the registers
    //
    CHECK_RC(WriteEccRegisters(controlValue, controlMask,
                               injectParams.ECC_ERR_INJECTION_ADDR,
                               _UINT32_MAX, hwFbio, channel));
    return OK;
}

//--------------------------------------------------------------------
//! \brief Return the litter used by the AMAP library
//!
//! The return value uses the same enum as AmapLitter in amap_v1.h,
//! but colwerted to a typesafe GpuLitter because we'd get
//! name-collisions if we tried to include amap_v1.h in most mods
//! files.
//!
GpuLitter AmapV1FrameBuffer::GetAmapLitter() const
{
    return GpuLitter(m_ChipConf.litter);
}

//--------------------------------------------------------------------
//! \brief Fill a MemParamsV1 struct, used by most AMAP functions
//!
void AmapV1FrameBuffer::GetMemParams
(
    MemParamsV1 *pMemParams,
    UINT32 pteKind,
    UINT32 pageSizeKB
) const
{
    MASSERT(pMemParams);
    const RegHal &regs = GpuSub()->Regs();

    memset(pMemParams, 0, sizeof(*pMemParams));

    switch (m_ChipConf.litter)
    {
        case LITTER_GMLIT4:
        case LITTER_GPLIT3:
        case LITTER_GPLIT4:
        case LITTER_GVLIT1:
        case LITTER_GVLIT3:
        case LITTER_GVLIT5:
            if (pteKind == regs.LookupValue(MODS_MMU_PTE_KIND_PITCH))
            {
                pMemParams->pteKind = PTE_KIND_PITCH;
            }
            else if (pteKind ==
                     regs.LookupValue(MODS_MMU_PTE_KIND_PITCH_NO_SWIZZLE))
            {
                pMemParams->pteKind = PTE_KIND_PITCH_NO_SWIZZLE;
            }
            else if (pteKind ==
                     regs.LookupValue(MODS_MMU_PTE_KIND_C32_MS2_2C) ||
                     pteKind ==
                     regs.LookupValue(MODS_MMU_PTE_KIND_GENERIC_16BX2))
            {
                pMemParams->pteKind = PTE_KIND_BLOCKLINEAR_GENERIC; //Same as _Z64
            }
            else
            {
                MASSERT(!"Decode is unsupported for given PTE Kind");
                pMemParams->pteKind = PTE_KIND_ILWALID;
            }
            break;
        default:
            // Turing+ uses BLOCKLINEAR_Z64 for LW_MMU_PTE_KIND_ZF32_X24S8_*
            // (which mods doesn't use yet), and BLOCKLINEAR_GENERIC for
            // everything else.
            pMemParams->pteKind = PTE_KIND_BLOCKLINEAR_GENERIC;
            break;
    }

    switch (pageSizeKB)
    {
        case 4:
            pMemParams->pageSize = PAGESIZE_4K;
            break;
        case 64:
            pMemParams->pageSize = PAGESIZE_64K;
            break;
        case 128:
            pMemParams->pageSize = PAGESIZE_128K;
            break;
        case 2048:
            pMemParams->pageSize = PAGESIZE_2M;
            break;
        case 512 * 1024:
#ifdef PAGESIZE_512M
            pMemParams->pageSize = PAGESIZE_512M;
#else
            pMemParams->pageSize = PAGESIZE_2M;
#endif
            break;
        default:
            MASSERT(!"Decode is unsupported for given PTE size");
            pMemParams->pageSize = PAGESIZE_ILWALID;
            break;
    }

    pMemParams->aperture = APERTURE_LOCAL;
}

//--------------------------------------------------------------------
//! \brief WAR for bug 1748287
//!
//! Pre-GH100, when AMAP encodes & decodes a dual-rank HBM chip with 8 banks,
//! they store the rank in the MSB of the bank (treating it as a
//! 16-bank part) instead of storing the rank in the extBank field.
//! This method returns true on such parts.
//!
bool AmapV1FrameBuffer::SwizzleHbmRank() const
{
    return (GpuSub()->HasBug(1748287)) &&
           (GetRankCount() == 2) &&
           (GetBankCount() == 8);
}

//--------------------------------------------------------------------
//! \brief Retrieve the numExtBanks field value to pass into AMAP
//!
//! Before GHLIT1
//!     RankCount=1 -> numExtBanks=0
//!     RankCount=2 -> numExtBanks=1
//! For GHLIT1+ 
//!     RankCount=1 -> numExtBanks=0
//!     RankCount=2 -> numExtBanks=1 or 2 (1 is for backwards compatibility)
//!     RankCount=3 -> numExtBanks=3
//!
//! Thus, if we keep the legacy behavior but set numExtBanks=3 when RankCount=3,
//! we should be able to support all litters without having to have multiple implementations.
//!
UINT32 AmapV1FrameBuffer::GetAmapExtBanks() const
{
    switch (GetRankCount())
    {
        case 1:
            return 0;
        case 2:
            return 1;
        case 3:
            return 3;
        default:
            MASSERT(!"Unsupported rank count in GetAmapExtBanks");
            return ~0;
    }
}

//--------------------------------------------------------------------
//! \brief Decode the subpartition data returned by AMAP
//!
//! The "partition" returned by AMAP is the virtualLtc.  For the
//! subpartition, there are three cases we know how to deal with:
//!
//! (1) The chip has the same number of LTCs and FBIOs.  In this case,
//! each LTC maps directly to an FBIO, and the subpartition returned by
//! AMAP should be correct.
//!
//! (2) The chip has N LTCs per FBIO, where N is the number of
//! subpartitions per FBIO.  In this case, each LTC corresponds to a
//! subpartition: hwLtc == hwFbio * numSubpartitions + subpartition.
//! Ignore the "subpartition" value returned by AMAP; they return
//! virtualLtc%numSubpartitions, but we need hwLtc%numSubpartitions.
//!
//! (3) Exception for #2 above: If the chip has 2 LTCs per FBIO and
//! one LTC is floorswept, then it's either in "half FBPA" or
//! "balanced LTC" mode.  For half-FBPA mode, the subpartition is
//! always 1.  For balanced-LTC mode, the subpartition returned by
//! AMAP should be correct (it's the slice).
//!
UINT32 AmapV1FrameBuffer::DecodeAmapSubpartition
(
    uint32_t amapSubpartition,
    UINT32 hwLtc
) const
{
    MASSERT(GetMaxLtcsPerFbp() % GetFbiosPerFbp() == 0);
    const UINT32 maxLtcsPerFbio = GetMaxLtcsPerFbp() / GetFbiosPerFbp();
    if (maxLtcsPerFbio == 1 || m_BalancedLtcMode)
    {
        return amapSubpartition;
    }
    else
    {
        MASSERT(maxLtcsPerFbio == GetSubpartitions());
        if (maxLtcsPerFbio == 2 &&
            ((GetHalfFbpaMask() >> (hwLtc / 2)) & 1) != 0)
        {
            return 1; // Half-FBPA mode
        }
        return hwLtc % maxLtcsPerFbio;
    }
}

//--------------------------------------------------------------------
//! \brief Encode the subpartition data passed to AMAP
//!
//! This function does the opposite of DecodeAmapSubpartition().
//!
uint32_t AmapV1FrameBuffer::EncodeAmapSubpartition
(
    UINT32 subpartition,
    UINT32 virtualLtc
) const
{
    MASSERT(GetMaxLtcsPerFbp() % GetFbiosPerFbp() == 0);
    const UINT32 maxLtcsPerFbio = GetMaxLtcsPerFbp() / GetFbiosPerFbp();
    MASSERT(maxLtcsPerFbio == 1 || maxLtcsPerFbio == GetSubpartitions());
    return (maxLtcsPerFbio == 1) ? subpartition : (virtualLtc % maxLtcsPerFbio);
}

//--------------------------------------------------------------------
//! \brief Callwlate the channel from the subpartition & pseudoChannel
//!
//! Amaplib has faulty subpartition callwlations (see
//! DecodeAmapSubpartition), and since the channel number is
//! callwlated from the subpartition number, any channel returned by
//! amaplib has the same problem.  However, we should be able to get
//! the correct channel info if we discard the bits that were derived
//! from the subpartition, and replace them with the correct
//! subpartition.
//!
UINT32 AmapV1FrameBuffer::CallwlateChannel
(
    UINT32 subpartition,
    UINT32 amapChannel
) const
{
    MASSERT(GetChannelsPerFbio() % GetSubpartitions() == 0);
    const UINT32 channelsPerSubpartition = (GetChannelsPerFbio() /
                                            GetSubpartitions());
    return ((subpartition * channelsPerSubpartition) +
            (amapChannel % channelsPerSubpartition));
}

// For GH100, flip strap registers must be taken into account to get the
// true Subpartition/Pseudochannel address.
void AmapV1FrameBuffer::HbmFlipSubpPc(UINT32 hwFbio, UINT32 *pSubp, UINT32 *pPc) const
{
    RegHal& regs = GpuSub()->Regs();
    if (m_ChipConf.litter == LITTER_GHLIT1)
    {
        if (GetRamProtocol() == FrameBuffer::RamHBM2)
        {
            if (regs.IsSupported(MODS_PFB_FBPA_0_DEBUG_2_HBM2_PC_FLIP_STRAP) &&
                regs.Read32(MODS_PFB_FBPA_0_DEBUG_2_HBM2_PC_FLIP_STRAP, hwFbio))
            {
                // For HBM2, flip the subp and the pc
                *pSubp ^= 1;
                *pPc ^= 1;
            }
        }
        else if (GetRamProtocol() == FrameBuffer::RamHBM3)
        {
            if (regs.IsSupported(MODS_PFB_FBPA_0_DEBUG_2_HBM3_PC_FLIP_STRAP) &&
                regs.Read32(MODS_PFB_FBPA_0_DEBUG_2_HBM3_PC_FLIP_STRAP, hwFbio))
            {
                // For HBM3, the subpartition, the channel within each subp,
                // and the pc within each channel needs to be flipped
                //
                // The pseudoChannel in this struct here is the local pc index
                // within the subpartition
                // 0 = Channel0, PC0
                // 1 = Channel0, PC1
                // 2 = Channel1, PC0
                // 3 = Channel1, PC1
                //
                // Flip all relevant bits
                *pSubp ^= 1;
                *pPc ^= 0x3;
            }
        }
        else
        {
            MASSERT(!"Invalid Ram Protocol!");
        }
    }
}

//------------------------------------------------------------------------------
FrameBuffer* CreateGP100FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV1FrameBuffer("GP100 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_GMLIT4);
}
FrameBuffer* CreateGPLIT3FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV1FrameBuffer("GPLIT3 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_GPLIT3);
}
FrameBuffer* CreateGPLIT4FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV1FrameBuffer("GPLIT4 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_GPLIT4);
}
FrameBuffer* CreateGVLIT1FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV1FrameBuffer("GVLIT1 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_GVLIT1);
}
FrameBuffer* CreateGVLIT3FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV1FrameBuffer("GVLIT3 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_GVLIT3);
}
FrameBuffer* CreateGVLIT5FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV1FrameBuffer("GVLIT5 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_GVLIT5);
}
FrameBuffer* CreateTULIT2FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV1FrameBuffer("TULIT2 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_TULIT2);
}
FrameBuffer* CreateGALIT1FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV1FrameBuffer("GALIT1 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_GALIT1);
}
FrameBuffer* CreateGALIT2FrameBuffer(GpuSubdevice* pGpuSubdevice, LwRm* pLwRm)
{
    return new AmapV1FrameBuffer("GALIT2 FrameBuffer", pGpuSubdevice, pLwRm,
                                 LITTER_GALIT2);
}
