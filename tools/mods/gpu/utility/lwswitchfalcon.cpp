/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwswitchfalcon.h"

#include "core/include/fileholder.h"
#include "core/include/registry.h"
#include "core/include/tasker.h"
#include "core/include/utility.h"
#include "device/interface/lwlink/lwlregs.h"

static map<LwSwitchFalcon::FalconType, UINT32> engineBases =
{
    // Not defined in the manuals
    // SOE is a "discoverable" module, so we need to add the base (i.e, 0x840000)
    // to all the addresses even in the SOE manuals
    { LwSwitchFalcon::FalconType::SOE,   0x840000  }
};

//-----------------------------------------------------------------------------
LwSwitchFalcon::LwSwitchFalcon(TestDevice* pDev, FalconType falconType)
    : m_pDev(pDev)
    , m_FalconType(falconType)
{
    MASSERT(m_pDev);
}

RC LwSwitchFalcon::Initialize()
{
    if (m_IsInitialized)
    {
        return RC::OK;
    }

    m_EngineBase    = engineBases[m_FalconType];
    m_IsInitialized = true;
    return RC::OK;
}

RC LwSwitchFalcon::WaitForHalt(UINT32 timeoutMs)
{
    RC rc;
    CHECK_RC(Initialize());

    PollFalconArgs pollArgs;
    pollArgs.pDev       = m_pDev;
    pollArgs.engineBase = m_EngineBase;
    CHECK_RC(POLLWRAP_HW(&PollEngineHalt, &pollArgs, AdjustTimeout(timeoutMs)));

    return rc;
}

RC LwSwitchFalcon::Start(bool bWaitForHalt, UINT32 delayUs)
{
    RC rc;
    RegHal &regs = m_pDev->GetInterface<LwLinkRegs>()->Regs();

    CHECK_RC(Initialize());

    Printf(GetVerbosePriority(), "Falcon Start\n");

    // Set BOOTVEC to 0
    CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_BOOTVEC, 0U));
    // Clear REQUIRE_CTX to start without a DMA
    CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_DMACTL, 0U));

    // Start CPU
    UINT32 cpuCtl;
    CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_CPUCTL, &cpuCtl));
    regs.SetField(&cpuCtl, MODS_PFALCON_FALCON_CPUCTL_STARTCPU_TRUE);
    CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_CPUCTL, cpuCtl));

    // Throw in a delay to make sure the falcon has
    // started before exelwtion continues
    Utility::Delay(static_cast<UINT32>(AdjustTimeout(delayUs)));

    if (bWaitForHalt)
    {
        CHECK_RC(WaitForHalt(GetTimeoutMs()));
    }

    return rc;
}

RC LwSwitchFalcon::ShutdownUCode()
{
    // Nothing to do for non-SOE falcon (lwrrently the only falcon)
    if (m_FalconType != FalconType::SOE)
        return RC::OK;

    RC rc;

    CHECK_RC(Initialize());

    // If SOE is disabled it is required to wait for GFW boot to be complete and then
    // request that GFW halt
    RegHal &regs = m_pDev->GetInterface<LwLinkRegs>()->Regs();
    UINT32 soeDisable = 0;
    if ((RC::OK == Registry::Read("LwSwitch", "SoeDisable", &soeDisable)) && (soeDisable == 1))
    {
        rc = Tasker::Poll(AdjustTimeout(1000), [&]()->bool
                          {
                              return regs.Test32(MODS_GFW_SOE_BOOT_PROGRESS_COMPLETED) &&
                                     !regs.Test32(MODS_GFW_SOE_BOOT_VALIDATION_STATUS_IN_PROGRESS);
                          });
        if (rc != RC::OK)
        {
            Printf(Tee::PriError, "Failed to wait for SOE GFW to complete\n");
            return rc;
        }

        if (!regs.Test32(MODS_GFW_SOE_BOOT_VALIDATION_STATUS_PASS_TRUSTED))
        {
            Printf(Tee::PriError, "SOE GFW firmware failed validation\n");
            return RC::HW_STATUS_ERROR;
        }

        regs.Write32(MODS_GFW_SOE_EXIT_AND_HALT_REQUESTED_YES);

        PollFalconArgs pollArgs;
        pollArgs.pDev       = m_pDev;
        pollArgs.engineBase = m_EngineBase;
        rc = POLLWRAP_HW(&PollEngineHalt, &pollArgs, AdjustTimeout(1000));
        if (rc != RC::OK)
        {
            Printf(Tee::PriError, "Failed to wait for SOE GFW to halt\n");
            return rc;
        }
    }
    else
    {
        // This *should* lower the PLMs so MODS can apply the HULK once the driver firmware
        // is loaded, however it lwrrently does not function correctly (see http://lwbugs/3209680)
        regs.Write32(MODS_SOE_RESET_SEQUENCE_REQUESTED_YES);

        StickyRC pollRc;
        rc = Tasker::Poll(AdjustTimeout(1000), [&]()->bool
                          {
                              UINT32 resetPLM;
                              UINT32 engctlPLM;

                              pollRc = FalconRegRd(MODS_SOE_FALCON_RESET_PRIV_LEVEL_MASK, &resetPLM);
                              pollRc = FalconRegRd(MODS_SOE_FALCON_ENGCTL_PRIV_LEVEL_MASK, &engctlPLM);

                              return (pollRc != RC::OK) ||
                                     (regs.TestField(resetPLM, MODS_SOE_FALCON_RESET_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_ENABLE) &&
                                      regs.TestField(engctlPLM, MODS_SOE_FALCON_ENGCTL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_ENABLE));
                          });
        if ((rc != RC::OK) || (pollRc != RC::OK))
        {
            Printf(Tee::PriError, "Failed to reset SOE driver firmware\n");
            return rc;
        }
    }
    return rc;
}

RC LwSwitchFalcon::Reset()
{
    RC rc;

    CHECK_RC(Initialize());
    switch (m_FalconType)
    {
        case FalconType::SOE:
            CHECK_RC(FalconRegWr(MODS_SOE_FALCON_ENGINE, 1U));
            CHECK_RC(FalconRegWr(MODS_SOE_FALCON_ENGINE, 0U));
            break;
        default:
            Printf(Tee::PriError, "Reset not implemented for falcon type %u\n",
                                   static_cast<UINT32>(m_FalconType));
            return RC::UNSUPPORTED_FUNCTION;
    }

    PollFalconArgs pollArgs;
    pollArgs.pDev       = m_pDev;
    pollArgs.engineBase = m_EngineBase;

    // Poll for DMA to complete
    CHECK_RC(POLLWRAP_HW(&PollDmaDone, &pollArgs, AdjustTimeout(3000)));

    // Wait for the scrubbing to stop
    CHECK_RC(POLLWRAP_HW(&PollScrubbingDone, &pollArgs, AdjustTimeout(500)));

    return rc;
}

RC LwSwitchFalcon::ReadMailbox(UINT32 num, UINT32 *pVal)
{
    RC rc;
    MASSERT(pVal);

    CHECK_RC(Initialize());
    ModsGpuRegAddress mailboxReg;
    switch (num)
    {
        case 0:
            mailboxReg = MODS_PFALCON_FALCON_MAILBOX0;
            break;
        case 1:
            mailboxReg = MODS_PFALCON_FALCON_MAILBOX1;
            break;
        default:
            Printf(Tee::PriError, "Invalid mailbox register\n");
            return RC::ILWALID_ARGUMENT;
    }

    CHECK_RC(FalconRegRd(mailboxReg, pVal));
    return rc;
}

RC LwSwitchFalcon::WriteMailbox(UINT32 num, UINT32 val)
{
    RC rc;

    CHECK_RC(Initialize());
    ModsGpuRegAddress mailboxReg;
    switch (num)
    {
        case 0:
            mailboxReg = MODS_PFALCON_FALCON_MAILBOX0;
            break;
        case 1:
            mailboxReg = MODS_PFALCON_FALCON_MAILBOX1;
            break;
        default:
            Printf(Tee::PriError, "Invalid mailbox register\n");
            return RC::ILWALID_ARGUMENT;
    }

    CHECK_RC(FalconRegWr(mailboxReg, val));
    return rc;
}

//-----------------------------------------------------------------------------
RC LwSwitchFalcon::LoadProgram
(
    const vector<UINT32>& imemBinary,
    const vector<UINT32>& dmemBinary,
    const FalconUCode::UCodeInfo& ucodeInfo
)
{
    RC rc;
    RegHal &regs = m_pDev->GetInterface<LwLinkRegs>()->Regs();

    // Get IMEM/DMEM size
    UINT32 val = 0;
    CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_HWCFG, &val));
    // IMEM in units of 256 bytes.
    UINT32 falconIMEMSize = regs.GetField(val, MODS_PFALCON_FALCON_HWCFG_IMEM_SIZE);
    // DMEM in units of 256 bytes.
    UINT32 falconDMEMSize = regs.GetField(val, MODS_PFALCON_FALCON_HWCFG_DMEM_SIZE);
    falconIMEMSize *= 256;
    falconDMEMSize *= 256;

    Tee::Priority pri = GetVerbosePriority();
    MASSERT(falconIMEMSize / 4 >= imemBinary.size());
    MASSERT(falconDMEMSize / 4 >= dmemBinary.size());
    Printf(pri, "Max MEM Size - I: 0x%x, D: 0x%x\n", falconIMEMSize, falconDMEMSize);

    Printf(pri, "Loading IMEM/DMEM\n");
    CHECK_RC(WriteIMem(imemBinary, ucodeInfo.imemBase, ucodeInfo.secStart, ucodeInfo.secEnd));
    CHECK_RC(WriteDMem(dmemBinary, ucodeInfo.dmemBase));

    return rc;
}

RC LwSwitchFalcon::ReadDMem
(
    UINT32 offset,
    UINT32 dwordCount,
    vector<UINT32>* pRetArray
)
{
    RC rc;
    MASSERT(pRetArray);
    RegHal &regs = m_pDev->GetInterface<LwLinkRegs>()->Regs();

    // Load address and set auto-increment on read only
    regs.SetField(&offset, MODS_PFALCON_FALCON_DMEMC_AINCW_FALSE);
    regs.SetField(&offset, MODS_PFALCON_FALCON_DMEMC_AINCR_TRUE);
    CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_DMEMC, 0, offset));

    UINT32 dword;
    for (UINT32 i = 0; i < dwordCount; i++)
    {
        CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_DMEMD, 0, &dword));
        pRetArray->push_back(dword);
    }
    return rc;
}

RC LwSwitchFalcon::ReadIMem
(
    UINT32 offset,
    UINT32 dwordCount,
    vector<UINT32>* pRetArray
)
{
    RC rc;
    MASSERT(pRetArray);
    RegHal &regs = m_pDev->GetInterface<LwLinkRegs>()->Regs();

    // Load address and set auto-increment on read only
    regs.SetField(&offset, MODS_PFALCON_FALCON_IMEMC_AINCW_FALSE);
    regs.SetField(&offset, MODS_PFALCON_FALCON_IMEMC_AINCR_TRUE);
    CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_IMEMC, 0, offset));

    UINT32 dword;
    for (UINT32 i = 0; i < dwordCount; i++)
    {
        CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_IMEMD, 0, &dword));
        pRetArray->push_back(dword);
    }
    return rc;
}

RC LwSwitchFalcon::WriteDMem
(
    const vector<UINT32>& binary,
    UINT32 offset
)
{
    RC rc;
    RegHal &regs = m_pDev->GetInterface<LwLinkRegs>()->Regs();

    Tee::Priority pri = GetVerbosePriority();
    Printf(pri, "DMEM:");
    for (UINT32 i = 0; i < binary.size(); i++)
    {
        if (i % 4 == 0)
        {
            // Print the byte address
            Printf(pri, "\n%04x:  ", i * 4);
        }
        Printf(pri, "%08x ", binary[i]);
    }
    Printf(pri, "\n\n");

    // Load address and set auto-increment on write only
    regs.SetField(&offset, MODS_PFALCON_FALCON_DMEMC_AINCW_TRUE);
    regs.SetField(&offset, MODS_PFALCON_FALCON_DMEMC_AINCR_FALSE);
    CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_DMEMC, 0, offset));

    for (const UINT32& dword : binary)
    {
        CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_DMEMD, 0, dword));
    }

    return rc;
}

RC LwSwitchFalcon::WriteIMem
(
    const vector<UINT32>& binary,
    UINT32 offset,
    UINT32 secStart,
    UINT32 secEnd
)
{
    RC rc;
    RegHal &regs = m_pDev->GetInterface<LwLinkRegs>()->Regs();

    Tee::Priority pri = GetVerbosePriority();
    Printf(pri, "IMEM:\n");
    for (UINT32 i = 0; i < binary.size(); i++)
    {
        Printf(pri, "%08x ", binary[i]);
        if (i % 4 == 3)
        {
            Printf(pri, "\n");
        }
    }
    UINT32 lwrrBlock = 0;
    const UINT32 dwordsPerBlock = 256 / 4;

    // pad the remaining space in the last block with 0's
    UINT32 paddedSize = UNSIGNED_CAST(UINT32,
                        ((binary.size() - 1) / dwordsPerBlock + 1) * dwordsPerBlock);
    vector<UINT32> paddedBinary(binary);
    paddedBinary.resize(paddedSize, 0);

    for (UINT32 i = 0; i < paddedBinary.size() / dwordsPerBlock; i++)
    {
        // Load address and auto-increment on write only
        regs.SetField(&offset, MODS_PFALCON_FALCON_IMEMC_AINCW_TRUE);
        regs.SetField(&offset, MODS_PFALCON_FALCON_IMEMC_AINCR_FALSE);

        // Mark the appropriate blocks as secure
        UINT32 addr = offset &
                      (regs.LookupMask(MODS_PFALCON_FALCON_IMEMC_OFFS) |
                       regs.LookupMask(MODS_PFALCON_FALCON_IMEMC_BLK));
        if (addr >= secStart && addr < secEnd)
        {
            Printf(pri, "Block %u Selwred\n", i);
            regs.SetField(&offset, MODS_PFALCON_FALCON_IMEMC_SELWRE, 1);
        }
        else
        {
            Printf(pri, "Block %u Unselwred\n", i);
            regs.SetField(&offset, MODS_PFALCON_FALCON_IMEMC_SELWRE, 0);
        }
        CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_IMEMC, 0, offset));

        // Setup tag
        UINT32 tag = regs.GetField(offset, MODS_PFALCON_FALCON_IMEMC_BLK);
        CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_IMEMT, 0, tag));

        // Write the IMEM data
        lwrrBlock = i * dwordsPerBlock;
        for (UINT32 j = 0; j < dwordsPerBlock; j++)
        {
            CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_IMEMD, 0, paddedBinary[lwrrBlock + j]));
        }

        offset += 256;
    }

    return rc;
}

bool LwSwitchFalcon::PollScrubbingDone(void * pArgs)
{
    PollFalconArgs *pPollArgs = (PollFalconArgs *)(pArgs);
    TestDevice     *pDev      = pPollArgs->pDev;
    UINT32         baseAddr   = pPollArgs->engineBase;

    RegHal &regs = pDev->GetInterface<LwLinkRegs>()->Regs();
    UINT32 status = 0;
    bool bDone = false;
    if (FalconRegRdImpl(pDev, baseAddr, MODS_PFALCON_FALCON_DMACTL, &status) == RC::OK)
    {
        bDone = regs.TestField(status, MODS_PFALCON_FALCON_DMACTL_DMEM_SCRUBBING_DONE) &&
                regs.TestField(status, MODS_PFALCON_FALCON_DMACTL_IMEM_SCRUBBING_DONE);
    }
    return bDone;
}

bool LwSwitchFalcon::PollDmaDone(void *pArgs)
{
    PollFalconArgs *pPollArgs = (PollFalconArgs *)(pArgs);
    TestDevice     *pDev      = pPollArgs->pDev;
    UINT32         baseAddr   = pPollArgs->engineBase;

    RegHal &regs = pDev->GetInterface<LwLinkRegs>()->Regs();
    UINT32 status = 0;
    bool bDone = false;
    if (FalconRegRdImpl(pDev, baseAddr, MODS_PFALCON_FALCON_DMATRFCMD, &status) == RC::OK)
    {
        bDone = regs.TestField(status, MODS_PFALCON_FALCON_DMATRFCMD_FULL_FALSE) &&
                regs.TestField(status, MODS_PFALCON_FALCON_DMATRFCMD_IDLE_TRUE);
    }
    return bDone;
}

bool LwSwitchFalcon::PollEngineHalt(void *pArgs)
{
    PollFalconArgs *pPollArgs = (PollFalconArgs *)(pArgs);
    TestDevice     *pDev      = pPollArgs->pDev;
    UINT32         baseAddr   = pPollArgs->engineBase;

    RegHal &regs = pDev->GetInterface<LwLinkRegs>()->Regs();
    UINT32 status = 0;
    bool bDone = false;
    if (FalconRegRdImpl(pDev, baseAddr, MODS_PFALCON_FALCON_CPUCTL, &status) == RC::OK)
    {
        bDone = regs.TestField(status, MODS_PFALCON_FALCON_CPUCTL_HALTED_TRUE);
    }
    return bDone;
}

RC LwSwitchFalcon::FalconRegRd(ModsGpuRegAddress reg, UINT32 *pReadBack)
{
    return FalconRegRdImpl(m_pDev, m_EngineBase, reg, pReadBack);
}

RC LwSwitchFalcon::FalconRegRd(ModsGpuRegAddress reg, UINT32 idx, UINT32 *pReadBack)
{
    return FalconRegRdIdxImpl(m_pDev, m_EngineBase, reg, idx, pReadBack);
}

RC LwSwitchFalcon::FalconRegWr(ModsGpuRegAddress reg, UINT32 val)
{
    return FalconRegWrImpl(m_pDev, m_EngineBase, reg, val);
}

RC LwSwitchFalcon::FalconRegWr(ModsGpuRegAddress reg, UINT32 idx, UINT32 val)
{
    return FalconRegWrIdxImpl(m_pDev, m_EngineBase, reg, idx, val);
}

RC LwSwitchFalcon::FalconRegRdImpl
(
    TestDevice *pDev,
    UINT32 engineBase,
    ModsGpuRegAddress reg,
    UINT32 *pReadBack
)
{
    RegHal &regs = pDev->GetInterface<LwLinkRegs>()->Regs();
    return pDev->GetInterface<LwLinkRegs>()->RegRd(0,
                                                   RegHalDomain::RAW,
                                                   engineBase + regs.LookupAddress(reg),
                                                   pReadBack);
}

RC LwSwitchFalcon::FalconRegRdIdxImpl
(
    TestDevice *pDev,
    UINT32 engineBase,
    ModsGpuRegAddress reg,
    UINT32 idx,
    UINT32 *pReadBack
)
{
    RegHal &regs = pDev->GetInterface<LwLinkRegs>()->Regs();
    return pDev->GetInterface<LwLinkRegs>()->RegRd(0,
                                                   RegHalDomain::RAW,
                                                   engineBase + regs.LookupAddress(reg, idx),
                                                   pReadBack);
}

RC LwSwitchFalcon::FalconRegWrImpl
(
    TestDevice *pDev,
    UINT32 engineBase,
    ModsGpuRegAddress reg,
    UINT32 val
)
{
    RegHal &regs = pDev->GetInterface<LwLinkRegs>()->Regs();
    return pDev->GetInterface<LwLinkRegs>()->RegWr(0,
                                                   RegHalDomain::RAW,
                                                   engineBase + regs.LookupAddress(reg),
                                                   val);
}

RC LwSwitchFalcon::FalconRegWrIdxImpl
(
    TestDevice *pDev,
    UINT32 engineBase,
    ModsGpuRegAddress reg,
    UINT32 idx,
    UINT32 val
)
{
    RegHal &regs = pDev->GetInterface<LwLinkRegs>()->Regs();
    return pDev->GetInterface<LwLinkRegs>()->RegWr(0,
                                                   RegHalDomain::RAW,
                                                   engineBase + regs.LookupAddress(reg, idx),
                                                   val);
}
