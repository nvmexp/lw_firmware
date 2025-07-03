/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpufb.h"

#ifndef MATS_STANDALONE
#   include "core/include/platform.h"
#   include "core/include/mle_protobuf.h"
#endif

GpuFrameBuffer::GpuFrameBuffer
(
    const char   * name,
    GpuSubdevice * pGpuSubdevice,
    LwRm         * pLwRm
)
:   FrameBuffer(name, pGpuSubdevice, pLwRm),
    m_VendorId(RamVendorUnknown),
    m_ConfigStrap(0),
    m_RamProtocol(RamUnknown),
    m_ChannelsPerFbio(0),
    m_PseudoChannelsPerChannel(0),
    m_PseudoChannelsPerSubpartition(0),
    m_IsEccOn(false),
    m_RankCount(0),
    m_BankCount(0),
    m_RowBits(0),
    m_RowSize(0),
    m_BeatSize(0),
    m_GraphicsRamAmount(0)
{
}

RC GpuFrameBuffer::InitCommon()
{
    RC rc;
    GpuSubdevice* pSubdev = GpuSub();
    const RegHal &regs  = pSubdev->Regs();
    const UINT32 subpartitionsPerFbio = GetSubpartitions();
    UINT32 tmpValue;

    // Init FBP/FBIO/LTC info
    //
    CHECK_RC(InitFbpInfo());

    // Find m_RamProtocol, m_VendorId, and m_ConfigStrap.
    //
#ifndef MATS_STANDALONE
    LwRm *pLwRm = GetRmClient();
    if (pLwRm)
    {
        Printf(Tee::PriDebug, "Initializing FB RAM info\n");
        LW2080_CTRL_FB_INFO ramInfo[] =
        {
            {LW2080_CTRL_FB_INFO_INDEX_RAM_TYPE},
            {LW2080_CTRL_FB_INFO_INDEX_MEMORYINFO_VENDOR_ID},
            {LW2080_CTRL_FB_INFO_INDEX_RAM_CFG},
            {LW2080_CTRL_FB_INFO_INDEX_PSEUDO_CHANNEL_MODE}
        };
        LW2080_CTRL_FB_GET_INFO_PARAMS ramInfoParams =
        {
            static_cast<LwU32>(NUMELEMS(ramInfo)),
            LW_PTR_TO_LwP64(&ramInfo)
        };

        CHECK_RC(pLwRm->ControlBySubdevice(pSubdev,
                                           LW2080_CTRL_CMD_FB_GET_INFO,
                                           &ramInfoParams,
                                           sizeof(ramInfoParams)));
        switch (ramInfo[0].data)
        {
            case LW2080_CTRL_FB_INFO_RAM_TYPE_UNKNOWN:
                m_RamProtocol = RamUnknown;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_SDRAM:
                m_RamProtocol = RamSdram;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_DDR1:
                m_RamProtocol = RamDDR1;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_DDR2:
                m_RamProtocol = RamDDR2;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR2:
                m_RamProtocol = RamGDDR2;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR3:
                m_RamProtocol = RamGDDR3;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR4:
                m_RamProtocol = RamGDDR4;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_DDR3:
                m_RamProtocol = RamDDR3;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5:
                m_RamProtocol = RamGDDR5;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR5X:
                m_RamProtocol = RamGDDR5X;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_SDDR4:
                m_RamProtocol = RamSDDR4;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_LPDDR2:
                m_RamProtocol = RamLPDDR2;
                break;
                // RM API does not support LPDDR3 yet
                //  m_RamProtocol = RamLPDDR3;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_LPDDR4:
                m_RamProtocol = RamLPDDR4;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_HBM1:
                m_RamProtocol = RamHBM1;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_HBM2:
                m_RamProtocol = RamHBM2;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_HBM3:
                m_RamProtocol = RamHBM3;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6:
                m_RamProtocol = RamGDDR6;
                break;
            case LW2080_CTRL_FB_INFO_RAM_TYPE_GDDR6X:
                m_RamProtocol = RamGDDR6X;
                break;
            default:
                Printf(Tee::PriWarn,
                       "Unknown FrameBuffer::RamProtocol %d!\n",
                       static_cast<int>(ramInfo[0].data));
                m_RamProtocol = RamUnknown;
                break;
        }
        m_RamProtocolString = FrameBuffer::GetRamProtocolString();
        m_VendorId = RamVendorId(ramInfo[1].data);
        m_ConfigStrap = ramInfo[2].data;

        if (m_RamProtocol == RamHBM2 &&
            ramInfo[3].data == LW2080_CTRL_FB_INFO_PSEUDO_CHANNEL_MODE_DISABLED)
        {
            // This is an HBM2 chip using HBM1 protocol.
            // Set RamProtocol to HBM1 internally, but say "HBM2" in
            // logfiles for marketing reasons.
            m_RamProtocol = RamHBM1;
            m_RamProtocolString = "HBM2 (nonpseudochannel mode)";
        }
    }
    else
#endif // MATS_STANDALONE
    {
        const UINT32 ddr = regs.Read32(MODS_PFB_FBPA_FBIO_BROADCAST);

        if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_DDR1))
            m_RamProtocol = RamDDR1;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_DDR2))
            m_RamProtocol = RamDDR2;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR3))
            m_RamProtocol = RamGDDR3;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_DDR4))
            m_RamProtocol = RamGDDR4;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5))
            m_RamProtocol = RamGDDR5;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_SDDR4))
            m_RamProtocol = RamSDDR4;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM2))
            m_RamProtocol = RamHBM2;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM3))
            m_RamProtocol = RamHBM3;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR5X))
            m_RamProtocol = RamGDDR5X;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_LPDDR4))
            m_RamProtocol = RamLPDDR4;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_HBM1))
            m_RamProtocol = RamHBM1;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6))
            m_RamProtocol = RamGDDR6;
        else if (regs.TestField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE_GDDR6X))
            m_RamProtocol = RamGDDR6X;
        else
            m_RamProtocol = RamUnknown;

        if (m_RamProtocol == RamUnknown)
        {
            Printf(Tee::PriWarn, "Unknown FrameBuffer::RamProtocol %d!\n",
                   regs.GetField(ddr, MODS_PFB_FBPA_FBIO_BROADCAST_DDR_MODE));
        }

        m_RamProtocolString = FrameBuffer::GetRamProtocolString();
        m_VendorId          = RamVendorUnknown;     // Normally read from VBIOS
        m_ConfigStrap       = 0;                    // Normally read from VBIOS
    }

    // Get channel & pseudoChannel config
    //
    switch (m_RamProtocol)
    {
       case RamHBM2:
       case RamHBM3:
       case RamGDDR5X:
       case RamGDDR6:
       case RamGDDR6X:
           m_PseudoChannelsPerChannel = 2;
           break;
       default:
           m_PseudoChannelsPerChannel = 1;
           break;
    }

    switch (m_RamProtocol)
    {
       case RamHBM3:
           m_ChannelsPerFbio = subpartitionsPerFbio * 2;
           break;
       default:
           m_ChannelsPerFbio = subpartitionsPerFbio;
           break;
    }

    m_PseudoChannelsPerSubpartition = (m_PseudoChannelsPerChannel *
                                       m_ChannelsPerFbio /
                                       subpartitionsPerFbio);
    MASSERT(m_PseudoChannelsPerChannel * m_ChannelsPerFbio ==
            m_PseudoChannelsPerSubpartition * subpartitionsPerFbio);

    // Is ECC enabled?
    //
#ifndef MATS_STANDALONE
    if (pLwRm)
    {
        UINT32 eccMask = 0;
        bool isEccSupported = false;
        m_IsEccOn = (pSubdev->GetEccEnabled(&isEccSupported, &eccMask) == RC::OK &&
                     isEccSupported && Ecc::IsUnitEnabled(Ecc::Unit::DRAM, eccMask));
    }
    else
#endif
    {
        m_IsEccOn = regs.Test32(MODS_PFB_FBPA_ECC_CONTROL_MASTER_EN_ENABLED);
    }

    // Create row remapper
    //
    m_pRowRemapper = make_unique<RowRemapper>(GpuSub(), GetRmClient());
    CHECK_RC(m_pRowRemapper->Initialize());

    // Get rank and bank
    //
    CHECK_RC(CalibratedRead32(MODS_PFB_FBPA_CFG1_BANK_2, 2, &tmpValue));
    m_BankCount = 1 << tmpValue;

    switch (m_RamProtocol)
    {
       case RamHBM1:
       case RamHBM2:
       case RamHBM3:
           if (!pSubdev->HasBug(2355623))
           {
               if (!regs.HasReadAccess(MODS_PFB_FBPA_HBM_CFG0))
               {
                   Printf(Tee::PriError,
                          "Cannot determine whether HBM is dual rank\n");
                   return RC::PRIV_LEVEL_VIOLATION;
               }
           }
           if (regs.IsSupported(MODS_PFB_FBPA_HBM_CFG0_NUM_SIDS))
           {
               CHECK_RC(CalibratedRead32(MODS_PFB_FBPA_HBM_CFG0_NUM_SIDS_1,
                                         1, &tmpValue));
               m_RankCount = tmpValue;
           }
           else if (regs.Test32(MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_ENABLE))
           {
               CHECK_RC(CalibratedRead32(
                       MODS_PFB_FBPA_HBM_CFG0_DUAL_RANK_BANK_8, 3, &tmpValue));
               m_BankCount = 1 << tmpValue; // Override MODS_PFB_FBPA_CFG1_BANK
               m_RankCount = 2;
           }
           else
           {
               m_RankCount = 1;
           }
           break;
       default:
           CHECK_RC(CalibratedRead32(MODS_PFB_FBPA_CFG0_EXTBANK_0, 0,
                                     &tmpValue));
           m_RankCount = 1 << tmpValue;
           break;
    }

    // Get row, column, and beat size

    CHECK_RC(CalibratedRead32(MODS_PFB_FBPA_CFG1_ROWA_10, 10, &tmpValue));
    m_RowBits = tmpValue;
    CHECK_RC(CalibratedRead32(MODS_PFB_FBPA_CFG1_COL_8, 8, &tmpValue));
    m_RowSize = ((1 << tmpValue) *
                 GetAmapColumnSize() / m_PseudoChannelsPerChannel);

    switch (m_RamProtocol)
    {
       case RamHBM1:
           m_BeatSize = 16;
           break;
       case RamHBM2:
           m_BeatSize = 8;
           break;
       case RamGDDR5X:
       case RamGDDR6:
       case RamGDDR6X:
           m_BeatSize = 2;
           break;
       default:
           m_BeatSize = 4;
           break;
    }

    // Get m_GraphicsRamAmount, the total amount of FB DRAM
    //
    // Note: We can't get m_GraphicsRamAmount by summing the sizes in
    // m_FbRanges, because m_FbRanges omits RM carveouts.
    //
    m_GraphicsRamAmount = 0;
    UINT64 smallestFbioSize = _UINT64_MAX; // Smallest amount of memory on an FBIO

    const UINT32 fbioMask = GetFbioMask();
    UINT32 numActiveFbios = 0;
    for (INT32 fbio = Utility::BitScanForward(fbioMask); fbio >= 0;
         fbio = Utility::BitScanForward(fbioMask, fbio + 1))
    {
        const UINT32 fbioSizeMb = regs.Read32(MODS_PFB_FBPA_0_CSTATUS_RAMAMOUNT,
                                              fbio);
        const UINT64 fbioSize = static_cast<UINT64>(fbioSizeMb) << 20;

        // The CSTATUS a FBPA reports is based on the number of active L2 slices it's connected to.
        // If all the L2 slices connected are FS, it will report 0 in CSTATUS.
        if (fbioSize > 0)
        {
            smallestFbioSize      = min(smallestFbioSize, fbioSize);
            m_GraphicsRamAmount  += fbioSize;
            numActiveFbios++;
        }
    }
    MASSERT(m_GraphicsRamAmount > 0);

    // Get m_FbRanges, preferably from the RM
    //
    m_FbRanges.clear();

#ifndef MATS_STANDALONE
    if (pLwRm)
    {
        LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO_PARAMS regionParams = {0};
        rc = pLwRm->ControlBySubdevice(pSubdev,
                                       LW2080_CTRL_CMD_FB_GET_FB_REGION_INFO,
                                       &regionParams, sizeof(regionParams));
        if (rc == OK)
        {
            FbRange fbRange = {0};
            for (UINT32 ii = 0; ii < regionParams.numFBRegions; ii++)
            {
                MASSERT(regionParams.fbRegion[ii].base <=
                        regionParams.fbRegion[ii].limit);
                fbRange.start = regionParams.fbRegion[ii].base;
                fbRange.size =
                    regionParams.fbRegion[ii].limit - fbRange.start + 1;
                m_FbRanges.push_back(fbRange);
            }
        }
        else if (rc == RC::LWRM_NOT_SUPPORTED)
        {
            rc.Clear();
        }
        CHECK_RC(rc);
    }

    if (m_FbRanges.empty())
#endif
    {
        m_FbRanges.resize(1);
        m_FbRanges[0].start = 0;
        m_FbRanges[0].size  = smallestFbioSize * numActiveFbios;
        if (m_FbRanges[0].size != m_GraphicsRamAmount)
        {
            // Bug 1515210
            Printf(Tee::PriWarn,
                   "RM unavailable - won't test %s upper memory\n",
                   GetName());
        }
    }

    return rc;
}

RC GpuFrameBuffer::BlacklistPage
(
    UINT64 Location,
    UINT32 PteKind,
    UINT32 PageSizeKB,
    BlacklistSource source,
    UINT32 rbcAddress
) const
{
#ifndef MATS_STANDALONE
    LwRm *pLwRm = GetRmClient();
    if (pLwRm)
    {
        RC rc;
        if (GpuSub()->HasBug(868191))
            return RC::UNSUPPORTED_FUNCTION;

        FbDecode decode;
        CHECK_RC(DecodeAddress(&decode, Location, PteKind, PageSizeKB));

        UINT64 eccOnAddr;
        UINT64 eccOffAddr;
        CHECK_RC(EncodeAddress(decode, PteKind, PageSizeKB,
                               &eccOnAddr, &eccOffAddr));

        // Blacklist 4KB Pages
        const UINT64 eccOnPage  = eccOnAddr >> 12;
        const UINT64 eccOffPage = eccOffAddr >> 12;

        Printf(Tee::PriNormal,
               "[Blacklisting PA: 0x%0llx, ECC: 0x%0llx, NECC: 0x%0llx]\n",
               Location, eccOnPage, eccOffPage);

        return BlacklistPageInternal(eccOnPage, eccOffPage, source, rbcAddress);
    }
    else
#endif
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
}

RC GpuFrameBuffer::BlacklistPage
(
    UINT64 eccOnPage,
    UINT64 eccOffPage,
    BlacklistSource source,
    UINT32 rbcAddress
) const
{
    Printf(Tee::PriNormal, "[Blacklisting ECC: 0x%0llx, NECC: 0x%0llx]\n",
                            eccOnPage, eccOffPage);
    return BlacklistPageInternal(eccOnPage, eccOffPage, source, rbcAddress);
}

RC GpuFrameBuffer::BlacklistPageInternal
(
    UINT64 eccOnPage,
    UINT64 eccOffPage,
    BlacklistSource source,
    UINT32 rbcAddress
) const
{
#ifndef MATS_STANDALONE
    GpuSubdevice* pSubdev = GpuSub();
    LwRm *pLwRm = GetRmClient();

    if (!pSubdev->HasFeature(Device::Feature::GPUSUB_SUPPORTS_PAGE_BLACKLISTING))
    {
        Printf(Tee::PriError, "GPU does not support page blacklisting\n");
        return RC::UNSUPPORTED_FUNCTION;
    }

    if (pLwRm)
    {
        RC rc;
        if (pSubdev->HasBug(868191))
            return RC::UNSUPPORTED_FUNCTION;

        JavaScriptPtr pJs;
        bool useDynamic;
        CHECK_RC(pJs->GetProperty(pJs->GetGlobalObject(), "g_BlacklistPagesDynamic",
                 &useDynamic));

        LW2080_CTRL_FB_OFFLINE_PAGES_PARAMS params;

        memset(&params, 0, sizeof(params));

        // RM control call works on page numbers, not page offsets
        params.offlined[0].pageAddressWithEccOn  = eccOnPage;
        params.offlined[0].pageAddressWithEccOff = eccOffPage;
        params.offlined[0].rbcAddress = rbcAddress;
        LwU32 *pSource = &params.offlined[0].source;
        Mle::PageBlacklist::Cause cause = Mle::PageBlacklist::unknown;
        switch (source)
        {
            case BlacklistSource::MemError:
                *pSource = useDynamic ? LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_DPR_DBE
                                      : LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_MODS_MEM_ERROR;
                cause = Mle::PageBlacklist::mem_error;
                break;
            case BlacklistSource::Sbe: /* There is no DPR_SBE. RM only retires on multiple SBE */
                *pSource = useDynamic ? LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_DPR_MULTIPLE_SBE
                                      : LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_MODS_SBE;
                cause = Mle::PageBlacklist::sbe;
                break;
            case BlacklistSource::Dbe:
                *pSource = useDynamic ? LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_DPR_DBE
                                      : LW2080_CTRL_FB_OFFLINED_PAGES_SOURCE_MODS_DBE;
                cause = Mle::PageBlacklist::dbe;
                break;
            default:
                MASSERT(!"Illegal source in GF100FrameBuffer::BlacklistPage");
                break;
        }

        params.pageSize = LW2080_CTRL_FB_OFFLINED_PAGES_PAGE_SIZE_4K;
        params.validEntries = 1;

        RC blRc = pLwRm->ControlBySubdevice(pSubdev,
                                            LW2080_CTRL_CMD_FB_OFFLINE_PAGES,
                                            &params,
                                            sizeof(params));

        // Record metadata
        std::string ecid;
        CHECK_RC(pSubdev->GetEcid(&ecid));

        DEFER
        {
            Mle::Print(Mle::Entry::page_blacklist)
                .status(blRc.Get())
                .ecid(ecid)
                .ecc_off_addr(eccOffPage)
                .ecc_on_addr(eccOnPage)
                .rbc_addr(rbcAddress)
                .cause(cause)
                .page_size(params.pageSize)
                .dynamic(useDynamic);
        };

        if (blRc != RC::OK)
        {
            return RC::BLACKLIST_FAILURE;
        }

        Printf(Tee::PriDebug, "[NA: %d, VE: %d]\n",
               params.numPagesAdded, params.validEntries);
        if (params.numPagesAdded < params.validEntries)
        {
            blRc = RC::LWRM_ERROR;

            Printf(Tee::PriError, "Less pages blacklisted than requested: "
                   "requested(%u), blacklisted(%u)\n",
                   params.validEntries, params.numPagesAdded);
            return blRc;
        }

        return RC::OK;
    }
    else
#endif
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
}

RC GpuFrameBuffer::DumpFailingPage
(
    UINT64 Location,
    UINT32 PteKind,
    UINT32 PageSizeKB,
    BlacklistSource source,
    UINT32 rbcAddress,
    INT32 originTestId
) const
{
#ifndef MATS_STANDALONE
    LwRm *pLwRm = GetRmClient();
    if (pLwRm)
    {
        RC rc;
        if (GpuSub()->HasBug(868191))
            return RC::UNSUPPORTED_FUNCTION;

        FbDecode decode;
        CHECK_RC(DecodeAddress(&decode, Location, PteKind, PageSizeKB));

        UINT64 EccOnAddr;
        UINT64 EccOffAddr;
        CHECK_RC(EncodeAddress(decode, PteKind, PageSizeKB,
                               &EccOnAddr, &EccOffAddr));

        // Use 4KB Pages
        const UINT64 eccOnPage  = EccOnAddr  >> 12;
        const UINT64 eccOffPage = EccOffAddr >> 12;

        const UINT32 hwFbio = VirtualFbioToHwFbio(decode.virtualFbio);
        const UINT32 hwFbp = hwFbio / GetFbiosPerFbp(); // TODO

        JavaScriptPtr pJs;
        JsArray Args(17);
        pJs->ToJsval(Utility::StrPrintf("ECC=0x%llx NECC=0x%llx",
                                        eccOnPage, eccOffPage),
                     &Args[0]);
        pJs->ToJsval(static_cast<UINT32>(eccOnPage),        &Args[1]);
        pJs->ToJsval(static_cast<UINT32>(eccOnPage >> 32),  &Args[2]);

        pJs->ToJsval(static_cast<UINT32>(eccOffPage),       &Args[3]);
        pJs->ToJsval(static_cast<UINT32>(eccOffPage >> 32), &Args[4]);
        pJs->ToJsval(static_cast<UINT32>(source),           &Args[5]);
        pJs->ToJsval(static_cast<UINT32>(rbcAddress),       &Args[6]);

        pJs->ToJsval(hwFbp,                &Args[7]);
        pJs->ToJsval(hwFbio,               &Args[8]);
        pJs->ToJsval(decode.subpartition,  &Args[9]);
        pJs->ToJsval(decode.rank,          &Args[10]);
        pJs->ToJsval(decode.bank,          &Args[11]);
        pJs->ToJsval(decode.row,           &Args[12]);
        pJs->ToJsval(decode.pseudoChannel, &Args[13]);
        pJs->ToJsval(originTestId,         &Args[14]);

        const UINT32 offsetMask = (1 << 12) - 1;
        pJs->ToJsval(static_cast<UINT32>(EccOnAddr  & offsetMask), &Args[15]);
        pJs->ToJsval(static_cast<UINT32>(EccOffAddr & offsetMask), &Args[16]);

        UINT32 ret;
        jsval retvalJs = JSVAL_NULL;
        CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(),
                                 "AddFailingPage", Args, &retvalJs));
        CHECK_RC(pJs->FromJsval(retvalJs, &ret));
        CHECK_RC(ret);
        return rc;
    }
    else
#endif
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
}

RC GpuFrameBuffer::DumpMBISTFailingPage
(
    UINT08 site,
    string rowName,
    UINT32 hwFbio,
    UINT32 fbpaSubp,
    UINT08 rank,
    UINT08 bank,
    UINT16 ra,
    UINT08 pseudoChannel
) const
{
#ifndef MATS_STANDALONE
    RC rc;

    JavaScriptPtr pJs;
    JsArray Args(17);
    const UINT08 ilwalidVal = -1;
    const UINT32 hwFbp = hwFbio / GetFbiosPerFbp();

    pJs->ToJsval(Utility::StrPrintf("SOURCE=MBIST SITE=%d", site), &Args[0]);
    pJs->ToJsval(ilwalidVal,    &Args[1]);
    pJs->ToJsval(ilwalidVal,    &Args[2]);

    pJs->ToJsval(ilwalidVal,    &Args[3]);
    pJs->ToJsval(ilwalidVal,    &Args[4]);
    pJs->ToJsval(static_cast<UINT32>(Memory::BlacklistSource::MemError), &Args[5]);
    pJs->ToJsval(ilwalidVal,    &Args[6]);

    pJs->ToJsval(hwFbp,         &Args[7]);
    pJs->ToJsval(hwFbio,        &Args[8]);
    pJs->ToJsval(fbpaSubp,      &Args[9]);
    pJs->ToJsval(rank,          &Args[10]);
    pJs->ToJsval(bank,          &Args[11]);
    pJs->ToJsval(ra,            &Args[12]);
    pJs->ToJsval(pseudoChannel, &Args[13]);
    pJs->ToJsval(ilwalidVal,    &Args[14]);

    pJs->ToJsval(ilwalidVal,    &Args[15]);
    pJs->ToJsval(ilwalidVal,    &Args[16]);

    UINT32 ret;
    jsval retvalJs = JSVAL_NULL;
    CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(),
            "AddFailingPage", Args, &retvalJs));
    CHECK_RC(pJs->FromJsval(retvalJs, &ret));
    CHECK_RC(ret);
    return rc;
#endif
    return RC::UNSUPPORTED_FUNCTION;
}

/* virtual */ RC GpuFrameBuffer::GetFbRanges(FbRanges *pRanges) const
{
    *pRanges = m_FbRanges;
    return OK;
}

/* virtual */ bool GpuFrameBuffer::IsSOC() const
{
    return GpuSub()->IsSOC();
}

/* virtual */ UINT32 GpuFrameBuffer::GetBusWidth() const
{
    return
        GetFbioCount() *                // Num FBIOs
        GetSubpartitions() *            // Subpartitions per FBIO
        GetSubpartitionBusWidth();
}

/* virtual */ UINT32 GpuFrameBuffer::GetBurstSize() const
{
    switch (m_RamProtocol)
    {
        case RamGDDR5X:
        {
            // Burst length depends on data rate:
            // QDR mode (pstate 0):      32 bytes per pseudochannel
            // DDR mode (other pstates): 16 bytes per pseudochannel
            const bool isQdr = (GetDataRate() == 4);
            return isQdr ? 32 : 16;
        }
        default:
        {
            // HBM1: 2 beats of 16 bytes each
            // HBM2: 4 beats of 8 bytes each, per pseudochannel
            // HBM3: 8 beats of 4 bytes each, per pseudochannel
            // GDDR6: 16 beats of 2 bytes each, per pseudochannel
            // GDDR6X: 8 beats of 4 bytes each, per pseudochannel
            // GDDR5 et al: 8 beats of 4 bytes each
            return 32;
        }
    }
}

//! Return the number of reads or writes per clock cycle
UINT32 GpuFrameBuffer::GetDataRate() const
{
    switch (m_RamProtocol)
    {
        case RamGDDR5X:
        {
            // QDR (Quad Data Rate, pstate 0):        4 per clock cycle
            // DDR (Double Data Rate, other pstates): 2 per clock cycle
            const bool isQdr =
                GpuSub()->Regs().Test32(MODS_PFB_FBPA_CFG0_BL_BL16);
            return isQdr ? 4 : 2;
        }
        case RamGDDR6X:
        case RamGDDR6:
        {
            return 4;
        }
        default:
        {
            // All other GPUs use DDR
            return 2;
        }
    }
}

namespace
{
    //! \brief RAII class returned by SaveEccCheckbits
    //!
    //! This class saves the FB ECC checkbits registers in the
    //! constructor, and restores them to the previous value in the
    //! destructor
    class MyCheckbitsHolder : public FrameBuffer::CheckbitsHolder
    {
    public:
        MyCheckbitsHolder(GpuSubdevice *pSubdevice);
        virtual ~MyCheckbitsHolder();
    private:
        GpuSubdevice   *m_pGpuSubdevice;
        vector<UINT32>  m_Registers;
        vector<UINT32>  m_Values;
    };

    MyCheckbitsHolder::MyCheckbitsHolder(GpuSubdevice *pGpuSubdevice) :
        m_pGpuSubdevice(pGpuSubdevice)
    {
        const RegHal &regs        = pGpuSubdevice->Regs();
        const UINT32  fbioMask    = pGpuSubdevice->GetFB()->GetFbioMask();
        const UINT32  numChannels = pGpuSubdevice->GetFB()->GetChannelsPerFbio();
        for (INT32 ii = Utility::BitScanForward(fbioMask); ii >= 0;
             ii = Utility::BitScanForward(fbioMask, ii + 1))
        {
            const UINT32 hwFbio = static_cast<UINT32>(ii);

            m_Registers.push_back(regs.LookupAddress(
                            MODS_PFB_FBPA_0_ECC_CONTROL, hwFbio));
            if (regs.IsSupported(MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR))
            {
                for (UINT32 channel = 0; channel < numChannels; ++channel)
                {
                    m_Registers.push_back(regs.LookupAddress(
                                    MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR,
                                    {hwFbio, channel}));
                }
            }
            if (regs.IsSupported(MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR_EXT))
            {
                for (UINT32 channel = 0; channel < numChannels; ++channel)
                {
                    m_Registers.push_back(regs.LookupAddress(
                                    MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR_EXT,
                                    {hwFbio, channel}));
                }
            }
        }

        for (UINT32 regAddr: m_Registers)
        {
            m_Values.push_back(pGpuSubdevice->RegRd32(regAddr));
        }
    }

    /* virtual */ MyCheckbitsHolder::~MyCheckbitsHolder()
    {
        for (UINT32 ii = 0; ii < m_Registers.size(); ++ii)
        {
            m_pGpuSubdevice->RegWr32(m_Registers[ii], m_Values[ii]);
            m_pGpuSubdevice->RegRd32(m_Registers[ii]);
        }
    }
}

//--------------------------------------------------------------------
//! \brief Method to save & restore the FB ECC checkbits registers
//!
//! This method allows us to call other functions that write the FB
//! ECC checkbits registers, such as DisableFbEccCheckbits, and
//! restore the registers to their previous values when we're done.
//! It returns an abstract RAII object that restores the checkbits in
//! the destructor.
//!
//! Typical usage is to store the return value in a unique_ptr so that
//! the destructor gets called when it goes out of scope.
//!
/* virtual */
unique_ptr<FrameBuffer::CheckbitsHolder> GpuFrameBuffer::SaveFbEccCheckbits()
{
    return unique_ptr<CheckbitsHolder>(new MyCheckbitsHolder(GpuSub()));
}

//--------------------------------------------------------------------
//! \brief Check whether we can control ECC checkbits
//!
//! Attempt to write the ECC checkbit-control fields.  Return true if
//! the writes succeed.  A failure typically means that we need a HULK
//! license.
//!
/* virtual */ bool GpuFrameBuffer::CanToggleFbEccCheckbits()
{
    unique_ptr<CheckbitsHolder> checkbitsHolder(SaveFbEccCheckbits());
    RegHal &regs = GpuSub()->Regs();
    const ModsGpuRegValue checkbitRegValues[] =
    {
        MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL_ADR,
        MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL_ENABLED,
        MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL_DISABLED,
        MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE_ADR,
        MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE_ALL,
        MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE_OFF
    };

    EnableFbEccCheckbits();
    for (const ModsGpuRegValue value: checkbitRegValues)
    {
        if (regs.IsSupported(value))
        {
            regs.Write32(value);
            if (!regs.Test32(value))
            {
                return false;
            }
        }
    }
    return true;
}

//--------------------------------------------------------------------
//! \brief Enable the FB ECC checkbits
//!
/* virtual */ RC GpuFrameBuffer::EnableFbEccCheckbits()
{
    RC rc;
    CHECK_RC(WriteEccRegisters(0, 0, _UINT32_MAX, _UINT32_MAX,
                               _UINT32_MAX, _UINT32_MAX));
    return OK;
}

//--------------------------------------------------------------------
//! \brief Disable the ECC checkbits for the entire FB
//!
/* virtual */ RC GpuFrameBuffer::DisableFbEccCheckbits()
{
    const RegHal &regs = GpuSub()->Regs();
    RC rc;
    CHECK_RC(WriteEccRegisters(
                regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL_ENABLED),
                regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL),
                _UINT32_MAX, _UINT32_MAX, _UINT32_MAX, _UINT32_MAX));
    return OK;
}

//--------------------------------------------------------------------
//! \brief Disable the FB ECC checkbits at just one address
//!
//! If the GPU doesn't support disabling the checkbits at just one
//! address (which was the case for Fermi and early Kepler), then this
//! method disables the checkbits for the entire FB.
//!
/* virtual */ RC GpuFrameBuffer::DisableFbEccCheckbitsAtAddress
(
    UINT64 physAddr,
    UINT32 pteKind,
    UINT32 pageSizeKB
)
{
    const RegHal &regs = GpuSub()->Regs();
    RC rc;

    // Disable the entire FB if we can't disable a single address
    //
    if (!regs.IsSupported(MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL_ADR))
        return DisableFbEccCheckbits();

    // Use DecodeAddress() to find the value to write to
    // ECC_ERR_INJECTION_ADDR at the injection address, as well as the
    // corresponding FBIO & channel.
    //
    FbDecode decode;
    CHECK_RC(DecodeAddress(&decode, physAddr, pteKind, pageSizeKB));
    const UINT32 rowOffset = GetRowOffset(decode.burst, decode.beat,
                                          decode.beatOffset);
    const UINT32 injectionAddr =
        regs.SetField(MODS_PFB_FBPA_ECC_ERR_INJECTION_ADDR_COL,
                      rowOffset / GetFbEccSectorSize()) |
        regs.SetField(MODS_PFB_FBPA_ECC_ERR_INJECTION_ADDR_ROW,
                      decode.row) |
        regs.SetField(MODS_PFB_FBPA_ECC_ERR_INJECTION_ADDR_BANK,
                      decode.bank) |
        regs.SetField(MODS_PFB_FBPA_ECC_ERR_INJECTION_ADDR_EXTBANK,
                      decode.rank);
    const UINT32 hwFbio = VirtualFbioToHwFbio(decode.virtualFbio);

    // Write the registers
    //
    CHECK_RC(WriteEccRegisters(
                    regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL_ADR),
                    regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL),
                    injectionAddr, _UINT32_MAX, hwFbio, decode.channel));
    return OK;
}

//--------------------------------------------------------------------
//! \brief Apply FB write-kill at an address
//!
//! Some memories (e.g. HBM) don't allow you to disable ECC checkbits,
//! so we take a different approach to induce ECC errors.  We set one
//! or two "write kill" bits in an ECC sector.  When we write that
//! sector, the write-kill bits get ilwerted in the DRAM, but the
//! checkbits represent the original data.
//!
//! kptr0 and kptr1 are bit-offsets relative to physAddr, telling
//! which bit(s) to kill.  kptr0 or kptr1 can also be NO_KPTR, which
//! says not to set the corresponding write-kill.  Setting them both
//! should induce a double-bit error, while setting just one should
//! induce a single-bit error.
//!
//! Write-kill uses the same registers as DisableEccCheckbits et al,
//! so they are restored by SaveFbEccCheckbits().
//!
/* virtual */ RC GpuFrameBuffer::ApplyFbEccWriteKillPtr
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

    // Find the values to write to ECC_CONTROL at the injection address
    //
    UINT32 controlValue =
        regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE_ADR);
    UINT32 controlMask =
        regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE);
    if (kptr0 >= 0)
    {
        UINT32 sectorKptr = physAddr % GetFbEccSectorSize() * 8 + kptr0;
        MASSERT(sectorKptr < GetFbEccSectorSize() * 8);
        controlValue |=
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0,
                          sectorKptr) |
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0_EN_TRUE);
        controlMask |=
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0) |
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0_EN);
    }
    if (kptr1 >= 0)
    {
        UINT32 sectorKptr = physAddr % GetFbEccSectorSize() * 8 + kptr1;
        MASSERT(sectorKptr < GetFbEccSectorSize() * 8);
        controlValue |=
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1,
                          sectorKptr) |
            regs.SetField(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1_EN_TRUE);
        controlMask |=
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1) |
            regs.LookupMask(MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1_EN);
    }

    // Use DecodeAddress() to find the value to write to
    // ECC_ERR_INJECTION_ADDR at the injection address, as well as the
    // corresponding FBIO & channel.
    //
    FbDecode decode;
    CHECK_RC(DecodeAddress(&decode, physAddr, pteKind, pageSizeKB));
    const UINT32 rowOffset = GetRowOffset(decode.burst, decode.beat,
                                          decode.beatOffset);
    const UINT32 injectionAddr =
        regs.SetField(MODS_PFB_FBPA_ECC_ERR_INJECTION_ADDR_COL,
                      rowOffset / GetFbEccSectorSize()) |
        regs.SetField(MODS_PFB_FBPA_ECC_ERR_INJECTION_ADDR_ROW,
                      decode.row) |
        regs.SetField(MODS_PFB_FBPA_ECC_ERR_INJECTION_ADDR_BANK,
                      decode.bank) |
        regs.SetField(MODS_PFB_FBPA_ECC_ERR_INJECTION_ADDR_EXTBANK,
                      decode.rank);
    const UINT32 hwFbio = VirtualFbioToHwFbio(decode.virtualFbio);

    // Write the registers
    //
    CHECK_RC(WriteEccRegisters(controlValue, controlMask, injectionAddr,
                               _UINT32_MAX, hwFbio, decode.channel));
    return OK;
}

RC GpuFrameBuffer::RemapRow(const vector<RowRemapper::Request>& requests) const
{
#ifndef MATS_STANDALONE
    MASSERT(m_pRowRemapper.get());
    return m_pRowRemapper->Remap(requests);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC GpuFrameBuffer::CheckMaxRemappedRows(const RowRemapper::MaxRemapsPolicy& policy) const
{
#ifndef MATS_STANDALONE
    MASSERT(m_pRowRemapper.get());
    return m_pRowRemapper->CheckMaxRemappedRows(policy);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC GpuFrameBuffer::CheckRemapError() const
{
#ifndef MATS_STANDALONE
    MASSERT(m_pRowRemapper.get());
    return m_pRowRemapper->CheckRemapError();
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC GpuFrameBuffer::GetNumInfoRomRemappedRows(UINT32* pNumRows) const
{
    MASSERT(pNumRows);

#ifndef MATS_STANDALONE
    MASSERT(m_pRowRemapper.get());
    return m_pRowRemapper->GetNumInfoRomRemappedRows(pNumRows);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC GpuFrameBuffer::PrintInfoRomRemappedRows() const
{
#ifndef MATS_STANDALONE
    MASSERT(m_pRowRemapper.get());
    return m_pRowRemapper->PrintInfoRomRemappedRows();
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

RC GpuFrameBuffer::ClearRemappedRows
(
    vector<RowRemapper::ClearSelection> clearSelections
) const
{
#ifndef MATS_STANDALONE
    MASSERT(m_pRowRemapper.get());
    return m_pRowRemapper->ClearRemappedRows(clearSelections);
#else
    return RC::UNSUPPORTED_FUNCTION;
#endif
}

UINT64 GpuFrameBuffer::GetRowRemapReservedRamAmount() const
{
#ifndef MATS_STANDALONE
    MASSERT(m_pRowRemapper.get());
    return m_pRowRemapper->GetRowRemapReservedRamAmount();
#else
    return 0;
#endif
}

//--------------------------------------------------------------------
//! \brief Internal method used by the FB ECC checkbits functions
//!
//! This method consolidates much of the common register-writing logic
//! used by methods that control ECC checkbits.  It writes the
//! LW_PFB_FBPA_*_ECC_CONTROL & LW_PFB_FBPA_*_ECC_ERR_INJECTION_ADDR
//! registers.
//!
//! Conceptually, this method does the following, except that it
//! consolidates steps 1-2 so that each register is only written once:
//! (1) Write quiescent values to the registers, which enables ECC for
//!     the entire FB.
//! (2a) If injectHwFbio is a valid FBIO, then write special values to
//!      the registers corresponding to the FBIO/channel specified by
//!      injectHwFbio and injectChannel:
//!      * Write injectAddr to the corresponding ECC_ERR_INJECTION_ADDR
//!      * Write injectAddrExt to the corresponding ECC_ERR_INJECTION_ADDR_EXT
//!      * Update the corresponding ECC_CONTROL so that the bits in
//!        ctrlMask are set to ctrlValue
//! (2b) If injectHwFbio is not a valid FBIO, this method broadcasts
//!      ctrlValue and ctrlMask to all ECC_CONTROL registers, instead
//!      of just the ones for a single FBIO/channel.  For broadcast
//!      mode, the caller should set injectAddr == injectAddrExt ==
//!      injectHwFbio == injectChannel == _UINT32_MAX.
//! (3) Read the ECC_CONTROL registers back, as a WAR for bug 649430
//!
RC GpuFrameBuffer::WriteEccRegisters
(
    UINT32 ctrlValue,
    UINT32 ctrlMask,
    UINT32 injectAddr,
    UINT32 injectAddrExt,
    UINT32 injectHwFbio,
    UINT32 injectChannel
)
{
    RegHal&       regs                     = GpuSub()->Regs();
    const UINT32  ILWALID_INJECTION_ADDR   = _UINT32_MAX;
    const bool    broadcastMode            = (injectHwFbio == _UINT32_MAX);
    const bool    supportsInjectionAddr    = regs.IsSupported(
                                    MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR);
    const bool    supportsInjectionAddrExt = regs.IsSupported(
                                    MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR_EXT);

    if (!broadcastMode && !supportsInjectionAddr)
    {
        return RC::BAD_PARAMETER;
    }

    // Find the value & mask to write to the ECC_CONTROL registers:
    // * usualControlValue/Mask contains the quiescent ECC-enabled
    //   data to write to most of the ECC_CONTROL registers.
    // * injectControlValue/Mask contains the data to write to the
    //   injection address at injectHwFbio.
    //
    UINT32 usualControlValue = 0;
    UINT32 usualControlMask  = 0;
    for (ModsGpuRegValue ii: {
                 MODS_PFB_FBPA_ECC_CONTROL_ECC_WRITE_KILL_DISABLED,
                 MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR_MODE_OFF,
                 MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0_EN_FALSE,
                 MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1_EN_FALSE,
                 MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR0_INIT,
                 MODS_PFB_FBPA_ECC_CONTROL_WRITE_KILL_PTR1_INIT})
    {
        if (regs.IsSupported(ii))
        {
            usualControlValue |= regs.SetField(ii);
            usualControlMask  |= regs.LookupMask(regs.ColwertToField(ii));
        }
    }
    UINT32 injectControlValue = (usualControlValue & ~ctrlMask) | ctrlValue;
    UINT32 injectControlMask  = usualControlMask | ctrlMask;

    // In broadcast mode, apply injectControlValue/Mask to all
    // ECC_CONTROL registers
    //
    if (broadcastMode)
    {
        usualControlValue = injectControlValue;
        usualControlMask  = injectControlMask;
    }

    // Loop through the FBIOs and channels, and write the register
    // values we derived above.
    //
    const UINT32 fbioMask    = GetFbioMask();
    const UINT32 numChannels = GetChannelsPerFbio();
    for (INT32 ii = Utility::BitScanForward(fbioMask); ii >= 0;
         ii = Utility::BitScanForward(fbioMask, ii + 1))
    {
        const UINT32 hwFbio = static_cast<UINT32>(ii);

        // Write ECC_ERR_INJECTION_ADDR & ECC_ERR_INJECTION_ADDR_EXT
        //
        if (supportsInjectionAddr)
        {
            for (UINT32 channel = 0; channel < numChannels; ++channel)
            {
                if (hwFbio == injectHwFbio && channel == injectChannel)
                {
                    regs.Write32(MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR,
                                 {hwFbio, channel}, injectAddr);
                }
                else
                {
                    regs.Write32(MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR,
                                 {hwFbio, channel}, ILWALID_INJECTION_ADDR);
                }
            }
        }
        if (supportsInjectionAddrExt)
        {
            for (UINT32 channel = 0; channel < numChannels; ++channel)
            {
                if (hwFbio == injectHwFbio && channel == injectChannel)
                {
                    regs.Write32(MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR_EXT,
                                 {hwFbio, channel}, injectAddrExt);
                }
                else
                {
                    regs.Write32(MODS_PFB_FBPA_0_ECC_ERR_INJECTION_ADDR_EXT,
                                 {hwFbio, channel}, ILWALID_INJECTION_ADDR);
                }
            }
        }


        // Write ECC_CONTROL
        //
        const UINT32 oldCtrlValue = regs.Read32(MODS_PFB_FBPA_0_ECC_CONTROL,
                                                hwFbio);
        const UINT32 newCtrlValue =
            (hwFbio == injectHwFbio) ?
            ((oldCtrlValue & ~injectControlMask) | injectControlValue) :
            ((oldCtrlValue & ~usualControlMask)  | usualControlValue);
        if (newCtrlValue != oldCtrlValue)
        {
            regs.Write32(MODS_PFB_FBPA_0_ECC_CONTROL, hwFbio, newCtrlValue);
            regs.Read32(MODS_PFB_FBPA_0_ECC_CONTROL, hwFbio);
        }
    }
    return OK;
}

//--------------------------------------------------------------------
//! \brief Colwenience function for reading a register field where its values
//! are a fixed constant away from their implied value.
//!
//! Given one register field and its desired value, the difference will be
//! callwlated and added to the value read from the register. In this sense the
//! register read is 'calibrated'.
//!
//! For example, take LW_PFB_FBPA_CFG1_ROWA. It contains a series of values
//! labelled *_ROWA_10, *_ROWA_11, *_ROWA_12, and so on. These represent 10, 11,
//! and 12 bits where as the actual value read from the register would be 2, 3,
//! and 4 respectively (at least on GF100). The actual register values are 8
//! away from the desired value. This value is used to calibrate reads so they
//! correspond to the desired value rather than the actual value in the register.
//!
//! \param regValue Register value used for calibration. The corresponding
//! register will be read.
//! \param desiredValue The value we would like to have returned when the
//! register has value of \a regValue.
//! \param pOutput[out] Calibrated value read from the register
//! corresponding to \a regValue.
//!
RC GpuFrameBuffer::CalibratedRead32
(
    ModsGpuRegValue regValue,
    UINT32  desiredValue,
    UINT32* pOutput
) const
{
    MASSERT(pOutput);
    const RegHal &regs = GpuSub()->Regs();
    UINT32 uncalibratedValue;
    RC rc;

    if (GpuSub()->HasBug(2355623))
    {
        uncalibratedValue = regs.Read32(regs.ColwertToField(regValue));
    }
    else
    {
        CHECK_RC(regs.Read32Priv(regs.ColwertToField(regValue),
                                 &uncalibratedValue));
    }
    *pOutput = desiredValue + uncalibratedValue - regs.LookupValue(regValue);
    return rc;
}
