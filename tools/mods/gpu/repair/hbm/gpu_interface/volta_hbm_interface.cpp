/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "volta_hbm_interface.h"

#include "core/include/platform.h"
#include "gpu/include/hbmimpl.h"
#include "gpu/repair/repair_util.h"
#include "volta/gv100/dev_pri_ringstation_sys.h"

using namespace Memory;

RC Hbm::VoltaHbmInterface::CheckWirWriteCapable(const Wir& wir) const
{
    if (!wir.IsWrite())
    {
        Printf(Tee::PriError,
               "Attempt to write WDR when WIR does not support WDR writes:\n"
               "  WIR: (%s)\n", wir.ToString().c_str());
        MASSERT(!"Attempt to write WDR when WIR does not support WDR writes");
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC Hbm::VoltaHbmInterface::CheckWirReadCapable(const Wir& wir) const
{
    if (!wir.IsRead())
    {
        Printf(Tee::PriError,
               "Attempt to read WDR when WIR does not support WDR reads:\n"
               "  WIR: (%s)\n", wir.ToString().c_str());
        MASSERT(!"Attempt to read WDR when WIR does not support WDR reads");
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC Hbm::VoltaHbmInterface::CheckWdrLength
(
    const Wir& wir,
    const WdrData& data,
    bool allowEmpty
) const
{
    // TODO(stewarts): There is an exception made for wdrLength > 0 if wdr data
    // is not provided. This is done with the bypass instruction in the old
    // code. The question is, is this valid for all instructions, or specific
    // ones?

    // Check the provided data meets the WDR length requirements
    const UINT32 num32BitWrites = Repair::Num32BitChunks(wir.GetWdrLength());

    const bool acceptablyEmpty = (allowEmpty && data.empty());
    const bool sizeMatch = (data.size() == num32BitWrites);

    if (!sizeMatch && !acceptablyEmpty)
    {
        Printf(Tee::PriError, "Incorrect amount of WDR data provided: WIR "
               "requires %u 32-bit writes (%u bits of data), but %u were "
               "provided\n  WIR: %s\n  WDR: %s\n",
               num32BitWrites, wir.GetWdrLength(),
               static_cast<UINT32>(data.size()),
               wir.ToString().c_str(), data.ToString().c_str());
        return RC::SOFTWARE_ERROR;
    }

    return RC::OK;
}

RC Hbm::VoltaHbmInterface::WirWrite
(
    const Wir& wir,
    const Site& hbmSite,
    Wir::ChannelSelect chanSel,
    const WdrData& data
) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    CHECK_RC(CheckWirWriteCapable(wir));
    CHECK_RC(CheckWdrLength(wir, data, true));

    // Set WIR
    //
    CHECK_RC(ClearPrivError(&regs));

    // NOTE: HBM is assumed to expect normal mode!
    UINT32 normalMode = 0;
    CHECK_RC(GetIeee1500InstrNormalModeVal(hbmSite, &normalMode));

    Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR,
                    masterFbpa.Number(), normalMode | wir.Encode(chanSel));

    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 INSTR"));

    // Set WDR
    //
    if (data.empty())
    {
        return rc; // No WDR data, done
    }

    Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE,
                    masterFbpa.Number(), wir.GetWdrLength());

    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 MODE"));

    for (const auto& chunk : data)
    {
        Repair::Write32(m_Settings, regs,
                        MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA,
                        masterFbpa.Number(), chunk);
    }
    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 DATA"));

    return rc;
}

RC Hbm::VoltaHbmInterface::WirRead
(
    const Wir& wir,
    const Site& hbmSite,
    Wir::ChannelSelect chanSel,
    WdrData* pData
) const
{
    MASSERT(pData);
    RC rc;

    CHECK_RC(CheckWirReadCapable(wir));

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    // NOTE: HBM is assumed to expect normal mode!
    UINT32 normalMode = 0;
    CHECK_RC(GetIeee1500InstrNormalModeVal(hbmSite, &normalMode));

    CHECK_RC(ClearPrivError(&regs));

    Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR,
                    masterFbpa.Number(), normalMode | wir.Encode(chanSel));
    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 INSTR"));

    Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE,
                    masterFbpa.Number(), wir.GetWdrLength());
    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 MODE"));

    const UINT32 num32BitReads = Repair::Num32BitChunks(wir.GetWdrLength());
    pData->resize(num32BitReads, 0);

    // Read WDR
    //

    // NOTE: *_HBM_TEST_I1500_DATA is a shift register. Multiple writes means
    // adding more data to a buffer.
    for (auto& chunk : *pData)
    {
        chunk = Repair::Read32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA,
                               masterFbpa.Number());
    }
    CHECK_RC(CheckPrivViolation(&regs, "Read from HBM IEEE1500 DATA"));
    WdrReadNormalization(wir, pData);

    return rc;
}

RC Hbm::VoltaHbmInterface::WirWriteRaw
(
    const Wir& wir,
    const Site& hbmSite,
    Wir::ChannelSelect chanSel
) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    CHECK_RC(ClearPrivError(&regs));

    // NOTE: HBM is assumed to expect normal mode!
    UINT32 normalMode = 0;
    CHECK_RC(GetIeee1500InstrNormalModeVal(hbmSite, &normalMode));

    Repair::Write32(m_Settings, regs, 
                    MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR, masterFbpa.Number(),
                    normalMode | wir.Encode(chanSel));

    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 INSTR"));

    return rc;
}

RC Hbm::VoltaHbmInterface::WirWriteRaw
(
    const Site& hbmSite,
    const UINT32 data
) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    CHECK_RC(ClearPrivError(&regs));

    Repair::Write32(m_Settings, regs,
                    MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR, masterFbpa.Number(),
                    data);

    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 INSTR"));

    return rc;
}

RC Hbm::VoltaHbmInterface::ModeWriteRaw
(
    const Wir& wir,
    const Site& hbmSite
) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    CHECK_RC(ClearPrivError(&regs));

    Repair::Write32(m_Settings, regs, 
                    MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE, masterFbpa.Number(),
                    wir.GetWdrLength());

    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 MODE"));

    return rc;
}

RC Hbm::VoltaHbmInterface::ModeWriteRaw
(
    const Site& hbmSite,
    const UINT32 data
) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    CHECK_RC(ClearPrivError(&regs));

    Repair::Write32(m_Settings, regs,
            MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE, masterFbpa.Number(),
            data);

    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 MODE"));

    return rc;
}

RC Hbm::VoltaHbmInterface::WdrReadRaw
(
    const Wir& wir,
    const Site& hbmSite,
    Wir::ChannelSelect chanSel,
    WdrData* pData
) const
{
    MASSERT(pData);
    RC rc;

    CHECK_RC(CheckWirReadCapable(wir));

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    const UINT32 num32BitReads = Repair::Num32BitChunks(wir.GetWdrLength());
    pData->resize(num32BitReads, 0);

    // Read WDR
    //
    CHECK_RC(ClearPrivError(&regs));

    // NOTE: *_HBM_TEST_I1500_DATA is a shift register. Multiple writes means
    // adding more data to a buffer.
    for (auto& chunk : *pData)
    {
        chunk = Repair::Read32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA,
                               masterFbpa.Number());
    }
    CHECK_RC(CheckPrivViolation(&regs, "Read from HBM IEEE1500 DATA"));
    WdrReadNormalization(wir, pData);

    return rc;
}

RC Hbm::VoltaHbmInterface::WdrReadRaw
(
    const Site& hbmSite,
    UINT32* pData
) const
{
    MASSERT(pData);
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    // Read WDR
    //
    CHECK_RC(ClearPrivError(&regs));

    // NOTE: *_HBM_TEST_I1500_DATA is a shift register.
    *pData = Repair::Read32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA,
            masterFbpa.Number());
    CHECK_RC(CheckPrivViolation(&regs, "Read from HBM IEEE1500 DATA"));

    return rc;
}

RC Hbm::VoltaHbmInterface::WdrReadCompareRaw
(
    const Site& hbmSite,
    UINT32 data
) const
{
    RC rc;
    UINT32 readData = 0;

    CHECK_RC(WdrReadRaw(hbmSite, &readData));
    if (readData != data)
    {
        Printf(Tee::PriError, "Invalid WDR value to compare! found 0x%X, expected 0x%X\n",
                readData, data);
        rc = RC::SOFTWARE_ERROR;
    }
    return rc;
}

RC Hbm::VoltaHbmInterface::WdrWrite
(
    const Wir& wir,
    const Site& hbmSite,
    Wir::ChannelSelect chanSel,
    const WdrData& data
) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    CHECK_RC(CheckWirWriteCapable(wir));
    CHECK_RC(CheckWdrLength(wir, data));

    CHECK_RC(ClearPrivError(&regs));

    Repair::Write32(m_Settings, regs, 
                    MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_MODE, masterFbpa.Number(),
                    wir.GetWdrLength());
    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 MODE"));

    for (const auto& chunk : data)
    {
        Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA,
                        masterFbpa.Number(), chunk);
    }
    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 DATA"));

    return RC::OK;
}

RC Hbm::VoltaHbmInterface::WdrWriteRaw
(
    const Site& hbmSite,
    Wir::ChannelSelect chanSel,
    const WdrData& data
) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    CHECK_RC(ClearPrivError(&regs));

    for (const auto& chunk : data)
    {
        Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA,
                        masterFbpa.Number(), chunk);
    }
    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 DATA"));

    return rc;
}

RC Hbm::VoltaHbmInterface::WdrWriteRaw
(
    const Site& hbmSite,
    const UINT32 data
) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    CHECK_RC(ClearPrivError(&regs));


    Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_DATA,
            masterFbpa.Number(), data);
    CHECK_RC(CheckPrivViolation(&regs, "Write to HBM IEEE1500 DATA"));

    return rc;
}

RC Hbm::VoltaHbmInterface::ResetSite(const Site& hbmSite) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    UINT32 regVal = 0;

    // Put HBM into reset mode
    //
    regVal = Repair::Read32(m_Settings, regs,
                            MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                            masterFbpa.Number());
    regs.SetField(&regVal,
                  MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO_DRAM_RESET_ENABLED,
                  masterFbpa.Number());
    regs.SetField(&regVal,
                  MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO_DRAM_WRST_RESET_ENABLED,
                  masterFbpa.Number());
    Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                    masterFbpa.Number(), regVal);

    SleepUs(200);

    // Pull HBM out of reset mode
    //
    regVal = Repair::Read32(m_Settings, regs,
                            MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                            masterFbpa.Number());
    regs.SetField(&regVal,
                  MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO_DRAM_RESET_DISABLED,
                  masterFbpa.Number());
    regs.SetField(&regVal,
                  MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO_DRAM_WRST_RESET_DISABLED,
                  masterFbpa.Number());
    Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                    masterFbpa.Number(), regVal);

    SleepUs(1000);

    return rc;
}

RC Hbm::VoltaHbmInterface::ToggleWrstN(const Site& hbmSite) const
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    // NOTE: HBM_TEST_*_DISABLED means driving signal high. HBM_TEST_*_ENABLED
    // means driving signal low.

    // Keep RESET_n high
    MASSERT(regs.Test32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO_DRAM_RESET_DISABLED,
            masterFbpa.Number()));

    UINT32 regVal = 0;

    // Drive WRST_n low
    //
    regVal = Repair::Read32(m_Settings, regs,
                            MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                            masterFbpa.Number());
    regs.SetField(&regVal,
                  MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO_DRAM_WRST_RESET_ENABLED,
                  masterFbpa.Number());
    Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                    masterFbpa.Number(), regVal);

    SleepUs(1); // Wait 500ns

    // Drive WRST_n high
    //
    regVal = Repair::Read32(m_Settings, regs,
                            MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                            masterFbpa.Number());
    regs.SetField(&regVal,
                  MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO_DRAM_WRST_RESET_DISABLED,
                  masterFbpa.Number());
    Repair::Write32(m_Settings, regs, MODS_PFB_FBPA_0_FBIO_HBM_TEST_MACRO,
                    masterFbpa.Number(), regVal);

    SleepUs(1000);

    return rc;
}

FbioSubp Hbm::VoltaHbmInterface::FbpaSubpToFbioSubp(const FbpaSubp& fbpaSubp) const
{
    // 1:1 mapping on Volta
    return FbioSubp(fbpaSubp.Number());
}

FbpaSubp Hbm::VoltaHbmInterface::FbioSubpToFbpaSubp(const FbioSubp& fbioSubp) const
{
    // 1:1 mapping on Volta
    return FbpaSubp(fbioSubp.Number());
}

RC Hbm::VoltaHbmInterface::GetHbmLane
(
    const FbpaLane& fbpaLane,
    HbmLane* pHbmLane
) const
{
    MASSERT(pHbmLane);

    // There is a direct mapping from the FBPA to the HBM lanes on Volta
    RC rc;

    Site hbmSite;
    Channel hbmChannel;
    CHECK_RC(HwFbpaSubpToHbmSiteChannel(fbpaLane.hwFbpa,
                                        GetSubpartition(fbpaLane),
                                        &hbmSite, &hbmChannel));
    *pHbmLane = HbmLane(hbmSite, hbmChannel, fbpaLane.laneBit,
                        fbpaLane.laneType);

    return rc;
}

RC Hbm::VoltaHbmInterface::GetFbpaLane(const HbmLane& hbmLane, FbpaLane* pFbpaLane) const
{
    MASSERT(pFbpaLane);
    RC rc;

    // There is a direct mapping from the FBPA to the HBM lanes on Volta
    HwFbpa hwFbpa;
    FbpaSubp fbpaSubp;
    CHECK_RC(HbmSiteChannelToHwFbpaSubp(hbmLane.site, hbmLane.channel,
                                        &hwFbpa, &fbpaSubp));
    *pFbpaLane = FbpaLane(hwFbpa, hbmLane.laneBit, hbmLane.laneType);

    return rc;
}

RC Hbm::VoltaHbmInterface::GetHbmDwordAndByte
(
    const HbmLane& lane,
    UINT32* pHbmDword,
    UINT32* pHbmByte
) const
{
    MASSERT(pHbmDword || pHbmByte);

    // 1:1 mapping on Volta
    return GetDwordAndByte(lane, pHbmDword, pHbmByte);
}

RC Hbm::VoltaHbmInterface::GetFbpaDwordAndByte
(
    const FbpaLane& lane,
    UINT32* pFbpaDword,
    UINT32* pFbpaByte
) const
{
    MASSERT(pFbpaDword || pFbpaByte);

    // 1:1 mapping on Volta
    return GetDwordAndByte(lane, pFbpaDword, pFbpaByte);
}

RC Hbm::VoltaHbmInterface::Ieee1500WaitForIdle
(
    Site hbmSite,
    WaitMethod waitMethod,
    FLOAT64 timeoutMs
) const
{
    constexpr FLOAT64 defaultTimeoutMs = 1.0f;

    RC rc;
    const RegHal &regs = m_pSubdev->Regs();

    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(hbmSite, &masterFbpa));

    if (timeoutMs < 0)
    {
        timeoutMs = defaultTimeoutMs;
    }

    // Poll for idle bridge
    //
    MASSERT(timeoutMs >= 0);

    const UINT32 pollValue = regs.LookupValue(
        MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_I1500_STATUS_BRIDGE_STATE_IDLE);

    switch (waitMethod)
    {
        case WaitMethod::POLL:
        {
            // Don't allow the calling thread to yield while waiting for
            // IEEE1500 to go idle
            const UINT64 timeoutUs = static_cast<UINT64>(timeoutMs * 1000.0f);
            const UINT64 startTimeUs = Xp::GetWallTimeUS();
            const UINT64 endTimeUs = startTimeUs + timeoutUs;
            do
            {
                if (regs.Test32(MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_STATUS_BRIDGE_STATE,
                                masterFbpa.Number(), pollValue))
                {
                    return rc;
                }

                Platform::Pause();
            } while (Xp::GetWallTimeUS() < endTimeUs);

            rc = RC::TIMEOUT_ERROR;
        } break;

        case WaitMethod::DEFAULT:
        {
            const UINT32 pollAddress = regs.LookupAddress(
                MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_STATUS,
                masterFbpa.Number());

            const UINT32 pollMask = regs.LookupMask(
                MODS_PFB_FBPA_MC_2_FBIO_HBM_TEST_I1500_STATUS_BRIDGE_STATE);

            CHECK_RC(m_pSubdev->PollRegValue(pollAddress, pollValue,
                                             pollMask, timeoutMs));
        } break;

        default:
            Printf(Tee::PriError, "Unknown wait method: %d\n", static_cast<INT32>(waitMethod));
            MASSERT(!"Unknown wait method");
    }

    return rc;
}

RC Hbm::VoltaHbmInterface::GetLinkRepairRegRemapValue
(
    const HwFbpa& hwFbpa,
    const FbpaSubp& fbpaSubp,
    UINT32 fbpaDword,
    UINT16* pRemapValue,
    bool* pIsBypassSet
) const
{
    MASSERT(pRemapValue);
    MASSERT(pIsBypassSet);
    RC rc;

    RegHal& regs = m_pSubdev->Regs();
    const UINT32 subpBitWidth = GetFb()->GetSubpartitionBusWidth();

    // Pick any lane in given dword
    FbpaLane lane(hwFbpa, fbpaSubp, subpBitWidth, fbpaDword);

    UINT32 regAddr = 0;
    CHECK_RC(m_pSubdev->GetHbmLinkRepairRegister(lane, &regAddr));
    const UINT32 linkRepairRegVal = m_pSubdev->RegRd32(regAddr);

    *pRemapValue = static_cast<UINT16>(linkRepairRegVal);
    *pIsBypassSet = regs.GetField(linkRepairRegVal,
                                  MODS_PFB_FBPA_0_FBIO_HBM_LINK_REPAIR_CHANNELx_DWORDy_BYPASS) != 0;

    return rc;
}

void Hbm::VoltaHbmInterface::WdrReadNormalization
(
    const Wir& wir,
    WdrData* pWdrData
)
{
    MASSERT(pWdrData);

    // The only partial 32-bit value will be the last one. The upper byte
    // response comes back in the MSB portion of the register, so we shift down.
    const UINT32 shamt = (32 - (wir.GetWdrLength() % 32));
    pWdrData->back() >>= shamt;
}

RC Hbm::VoltaHbmInterface::ClearPrivError(RegHal* pRegs) const
{
    MASSERT(pRegs);
    MASSERT(pRegs->IsSupported(MODS_PPRIV_SYS_PRIV_ERROR_CODE));

    pRegs->Write32(MODS_PPRIV_SYS_PRIV_ERROR_CODE, 0);

    // NOTE(stewarts): Ideally this would be an assert. It is possible to have a
    // correct run where no PRIV errors are generated but we are unable to set
    // the PRIV error code. However, MODS Volta GPU initialization code seems to
    // be generating a PRIV error. This must be cleared before doing MemRepair's
    // first PRIV check to avoid a false positive.
    return CheckPrivViolation(pRegs, "Write to PRIV ERROR CODE");
}

RC Hbm::VoltaHbmInterface::CheckPrivViolation
(
    RegHal* pRegs,
    const char* pUserFriendlyRegisterName
) const
{
    // The mask is the prefix for all PRIV related errors which can be found at
    // https://wiki.lwpu.com/gpuhwvolta/index.php/Volta_PRIV#New_Table_of_PRIV_error_types

    MASSERT(pRegs);
    MASSERT(pUserFriendlyRegisterName);
    MASSERT(pRegs->IsSupported(MODS_PPRIV_SYS_PRIV_ERROR_CODE));

    const UINT32 errorCode = pRegs->Read32(MODS_PPRIV_SYS_PRIV_ERROR_CODE);
    // Use DRF_VAL since these defines don't correspond to actual registers
    const bool isPrivViolation = ((DRF_VAL(_PPRIV_SYS_PRI, _ERROR_CODE, _MASTER, errorCode))
                                  == LW_PPRIV_SYS_PRI_ERROR_CODE_MASTER_PRI_CLIENT_ERR);
    if (isPrivViolation)
    {
        Printf(Tee::PriError,
               "%s register is PRIV protected. Please acquire HULK license.\n",
               pUserFriendlyRegisterName);
        Printf(Tee::PriError, "For more information, please refer to "
               "https://confluence.lwpu.com/x/vjy2Ag\n");
        Printf(Tee::PriLow, "%s register failed with error: 0x%x\n",
               pUserFriendlyRegisterName, errorCode);
        return RC::PRIV_LEVEL_VIOLATION;
    }

    return RC::OK;
}

RC Hbm::VoltaHbmInterface::GetIeee1500InstrNormalModeVal(const Site& site, UINT32* pVal) const
{
    MASSERT(pVal);
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    HwFbpa masterFbpa;
    CHECK_RC(GetHbmSiteMasterFbpa(site, &masterFbpa));

    UINT32 normalMode = 0;
    regs.SetField(&normalMode,
                 MODS_PFB_FBPA_0_FBIO_HBM_TEST_I1500_INSTR_HBM_NORMAL_MODE_INIT,
                 masterFbpa.Number());
    MASSERT(normalMode == 0x1000);

    *pVal = normalMode;
    return rc;
}

RC Hbm::VoltaHbmInterface::GetDwordAndByte(const Lane& lane, UINT32* pDword, UINT32* pByte) const
{
    MASSERT(pDword || pByte);
    RC rc;

    const UINT32 subpBitWidth = GetFb()->GetSubpartitionBusWidth();
    constexpr UINT32 dwordBitWidth = 32;
    constexpr UINT32 byteBitWidth = 8;

    if (pDword)
    {
        *pDword = (lane.laneBit % subpBitWidth) / dwordBitWidth;
    }

    if (pByte)
    {
        *pByte = (lane.laneBit % subpBitWidth) / byteBitWidth;
    }

    return rc;
}
