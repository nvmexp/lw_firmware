/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2016,2019-2021 by LWPU Corporation. All rights reserved. All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation. Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gm20xfalcon.h"

#include "core/include/fileholder.h"
#include "core/include/utility.h"

static map<GM20xFalcon::FalconType, GM20xFalcon::FalconInfo> engineBases =
{
    { GM20xFalcon::FalconType::PMU,   {MODS_FALCON_PWR_BASE,     MODS_FALCON2_PWR_BASE,      MODS_PPWR_PMU_SCP_CTL0}},
    // LW_FALCON_SEC_BASE is not available through reghal since it is not defined in
    // dev_falcon_v4.h (and dev_falcon_v1.h has a couple of conflicting defines with v4)
    { GM20xFalcon::FalconType::SEC,   {MODS_PSEC_FALCON_IRQSSET, MODS_FALCON2_SEC_BASE,      MODS_PSEC_SCP_CTL0}},
    { GM20xFalcon::FalconType::LWDEC, {MODS_FALCON_LWDEC_BASE,   MODS_REGISTER_ADDRESS_NULL, MODS_REGISTER_ADDRESS_NULL}},
    { GM20xFalcon::FalconType::GSP,   {MODS_FALCON_GSP_BASE,     MODS_FALCON2_GSP_BASE,      MODS_PGSP_SCP_CTL0}}
};

//-----------------------------------------------------------------------------
GM20xFalcon::GM20xFalcon(GpuSubdevice* pSubdev, FalconType falconType)
    : m_pSubdev(pSubdev)
    , m_FalconType(falconType)
{
    MASSERT(m_pSubdev);
}

RC GM20xFalcon::Initialize()
{
    if (m_IsInitialized)
    {
        return RC::OK;
    }

    RegHal &regs           = m_pSubdev->Regs();
    ModsGpuRegAddress addr = engineBases[m_FalconType].engineBase;

    if (regs.IsSupported(addr))
    {
        m_EngineBase = regs.LookupAddress(addr);

        ModsGpuRegAddress addr2 = engineBases[m_FalconType].engineBase2;
        if (addr2 != MODS_REGISTER_ADDRESS_NULL && regs.IsSupported(addr2))
        {
            m_EngineBase2 = regs.LookupAddress(addr2);
        }
        ModsGpuRegAddress addr3 = engineBases[m_FalconType].scpBase;
        if (addr3 != MODS_REGISTER_ADDRESS_NULL && regs.IsSupported(addr3))
        {
            m_ScpBase = regs.LookupAddress(addr3);
        }
    }
    else
    {
        Printf(Tee::PriError, "Falcon type %u not supported on this chip\n",
                               static_cast<UINT32>(m_FalconType));
        return RC::UNSUPPORTED_FUNCTION;
    }
    m_IsInitialized = true;
    return RC::OK;
}

RC GM20xFalcon::IsInitialized()
{
    return m_IsInitialized;
}

RC GM20xFalcon::WaitForHalt(UINT32 timeoutMs)
{
    RC rc;
    CHECK_RC(Initialize());

    PollFalconArgs pollArgs;
    pollArgs.pSubdev    = m_pSubdev;
    pollArgs.engineBase = m_EngineBase;
    CHECK_RC(POLLWRAP_HW(&PollEngineHalt, &pollArgs, AdjustTimeout(timeoutMs)));

    return rc;
}

RC GM20xFalcon::Start(bool bWaitForHalt, UINT32 delayUs)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    CHECK_RC(Initialize());

    Printf(GetVerbosePriority(), "Falcon Start\n");

    // Set BOOTVEC to 0
    CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_BOOTVEC, 0x0));

    // Clear REQUIRE_CTX to start without a DMA
    CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_DMACTL, 0x0));

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

RC GM20xFalcon::Reset()
{
    RC rc;

    CHECK_RC(Initialize());

    CHECK_RC(WaitForKfuseSfkLoad(1000)); // Timeout is arbitrarily chosen
    CHECK_RC(ApplyEngineReset());
    CHECK_RC(WaitForKfuseSfkLoad(1000)); // Timeout is arbitrarily chosen

    PollFalconArgs pollArgs;
    pollArgs.pSubdev    = m_pSubdev;
    pollArgs.engineBase = m_EngineBase;

    // Poll for DMA to complete
    CHECK_RC(POLLWRAP_HW(&PollDmaDone, &pollArgs, AdjustTimeout(3000)));

    // Wait for the scrubbing to stop
    CHECK_RC(POLLWRAP_HW(&PollScrubbingDone, &pollArgs, AdjustTimeout(500)));

    return rc;
}

RC GM20xFalcon::GetUCodeVersion(UINT32 ucodeId, UINT32 *pVersion)
{
    RC rc;
    MASSERT(pVersion);

    CHECK_RC(Initialize());

    RegHal &regs = m_pSubdev->Regs();
    if (regs.IsSupported(MODS_PSEC_FALCON_UCODE_VERSION, ucodeId))
    {
        UINT32 ucodeVersionOffset = regs.LookupAddress(MODS_PSEC_FALCON_UCODE_VERSION, ucodeId) -
                                    regs.LookupAddress(MODS_PSEC_FALCON_IRQSSET);
        UINT32 ucodeVersionReg    = m_EngineBase + ucodeVersionOffset;
        *pVersion = m_pSubdev->RegRd32(ucodeVersionReg);
    }
    else
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

RC GM20xFalcon::ReadMailbox(UINT32 num, UINT32 *pVal)
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

RC GM20xFalcon::WriteMailbox(UINT32 num, UINT32 val)
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

RC GM20xFalcon::ReadEMem
(
    UINT32 offset,
    UINT32 dwordCount,
    vector<UINT32>* pRetArray
)
{
    RC rc;
    MASSERT(pRetArray);
    RegHal &regs = m_pSubdev->Regs();

    CHECK_RC(Initialize());
    switch (m_FalconType)
    {
        case FalconType::SEC:
            // Load address and set auto-increment on read only
            regs.SetField(&offset, MODS_PSEC_EMEMC_AINCW_FALSE);
            regs.SetField(&offset, MODS_PSEC_EMEMC_AINCR_TRUE);
            regs.Write32(MODS_PSEC_EMEMC, 0, offset);
            for (UINT32 i = 0; i < dwordCount; i++)
            {
                pRetArray->push_back(regs.Read32(MODS_PSEC_EMEMD, 0));
            }
            break;
        default:
            return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

RC GM20xFalcon::WriteEMem
(
    const vector<UINT32>& binary,
    UINT32 offset
)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    CHECK_RC(Initialize());
    switch (m_FalconType)
    {
        case FalconType::SEC:
            // Load address and set auto-increment on write only
            regs.SetField(&offset, MODS_PSEC_EMEMC_AINCW_TRUE);
            regs.SetField(&offset, MODS_PSEC_EMEMC_AINCR_FALSE);
            regs.Write32(MODS_PSEC_EMEMC, 0, offset);

            for (const UINT32& dword : binary)
            {
                regs.Write32(MODS_PSEC_EMEMD, 0, dword);
            }
            break;
        default:
            return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

//-----------------------------------------------------------------------------
RC GM20xFalcon::ApplyEngineReset()
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    switch (m_FalconType)
    {
        case FalconType::PMU:
            CHECK_RC(ResetPmu());
            break;
        case FalconType::SEC:
            CHECK_RC(ResetSec());
            break;
        case FalconType::LWDEC:
            regs.Write32(MODS_PMC_ENABLE_LWDEC, 1);
            regs.Write32(MODS_PMC_ENABLE_LWDEC, 0);
            break;
        case FalconType::GSP:
            regs.Write32(MODS_PGSP_FALCON_ENGINE_RESET_TRUE);
            regs.Write32(MODS_PGSP_FALCON_ENGINE_RESET_FALSE);
            break;
        default:
            Printf(Tee::PriError, "Reset not implemented for falcon type %u\n",
                                   static_cast<UINT32>(m_FalconType));
            return RC::UNSUPPORTED_FUNCTION;
    }

    return rc;
}

RC GM20xFalcon::TriggerAndPollScrub()
{
    RC rc;

    // Trigger scrubbing
    UINT32 dummyWrite;
    CHECK_RC(FalconRegRd(MODS_PFALCON_FALCON_DMEMC, 0, &dummyWrite));
    CHECK_RC(FalconRegWr(MODS_PFALCON_FALCON_DMEMC, 0, dummyWrite));

    // Wait for the scrubbing to stop
    PollFalconArgs pollArgs;
    pollArgs.pSubdev    = m_pSubdev;
    pollArgs.engineBase = m_EngineBase;
    CHECK_RC(POLLWRAP_HW(&PollScrubbingDone, &pollArgs, AdjustTimeout(3000)));

    return rc;
}

RC GM20xFalcon::LoadProgram
(
    const vector<UINT32>& imemBinary,
    const vector<UINT32>& dmemBinary,
    const FalconUCode::UCodeInfo& ucodeInfo
)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    // Get IMEM/DMEM size
    UINT32 val;
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

RC GM20xFalcon::ReadDMem
(
    UINT32 offset,
    UINT32 dwordCount,
    vector<UINT32>* pRetArray
)
{
    RC rc;
    MASSERT(pRetArray);
    RegHal &regs = m_pSubdev->Regs();

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

RC GM20xFalcon::ReadIMem
(
    UINT32 offset,
    UINT32 dwordCount,
    vector<UINT32>* pRetArray
)
{
    RC rc;
    MASSERT(pRetArray);
    RegHal &regs = m_pSubdev->Regs();

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

RC GM20xFalcon::WriteDMem
(
    const vector<UINT32>& binary,
    UINT32 offset
)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

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

RC GM20xFalcon::WriteIMem
(
    const vector<UINT32>& binary,
    UINT32 offset,
    UINT32 secStart,
    UINT32 secEnd
)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

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

RC GM20xFalcon::ResetPmu()
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    // Disable PMU module
    m_pSubdev->DisablePwrBlock();
    // Wait 5ms for the reset
    Utility::Delay(static_cast<UINT32>(AdjustTimeout(5000)));
    // Enable PMU module, and PTimer
    m_pSubdev->EnablePwrBlock();
    if (regs.IsSupported(MODS_PMC_ENABLE_PPMI))
    {
        regs.Write32(MODS_PMC_ENABLE_PPMI_ENABLED);
    }

    // The above enable/disable functions should program the
    // FALCON_ENGINE_RESET registers
    // Only if the register isn't present (as on GM20x), do we need
    // to program the HRESET field in CPUCTL
    if (!regs.IsSupported(MODS_PPWR_FALCON_ENGINE))
    {
        // Hard reset CPUCTL and ilwalidate IMEM
        UINT32 val = 0;
        regs.SetField(&val, MODS_PPWR_FALCON_CPUCTL_IILWAL_TRUE);
        regs.SetField(&val, MODS_PPWR_FALCON_CPUCTL_HRESET_TRUE);
        regs.Write32(MODS_PPWR_FALCON_CPUCTL, val);
    }

    PollFalconArgs pollArgs;
    pollArgs.pSubdev    = m_pSubdev;
    pollArgs.engineBase = m_EngineBase;

    // Poll for DMA to complete
    CHECK_RC(POLLWRAP_HW(&PollDmaDone, &pollArgs, AdjustTimeout(3000)));

    // Disable SFTRESET_EXT to reset PMU sub-modules except FBIF.
    regs.Write32(MODS_PPWR_FALCON_SFTRESET_EXT_TRUE);

    // Add 10ms delay after SFTRESET, Bug 1508797.
    // TODO: SFTRESET will be covered by PMU engine reset after bug 200004537.  Remove SFTRESET after bug 200004537.
    Utility::Delay(static_cast<UINT32>(AdjustTimeout(10000)));

    return rc;
}

RC GM20xFalcon::ResetSec()
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    // Disable Sec module
    m_pSubdev->DisableSecBlock();
    // Wait 5ms for the reset
    Utility::Delay(static_cast<UINT32>(AdjustTimeout(5000)));
    // Enable SEC module
    m_pSubdev->EnableSecBlock();

    // The above enable/disable functions should program the
    // FALCON_ENGINE_RESET registers
    // Only if the register isn't present (as on GM20x), do we need
    // to program the HRESET field in CPUCTL
    if (!regs.IsSupported(MODS_PSEC_FALCON_ENGINE))
    {
        // Hard reset CPUCTL and invalid IMEM
        UINT32 val = 0;
        regs.SetField(&val, MODS_PSEC_FALCON_CPUCTL_IILWAL_TRUE);
        regs.SetField(&val, MODS_PSEC_FALCON_CPUCTL_HRESET_TRUE);
        regs.Write32(MODS_PSEC_FALCON_CPUCTL, val);
    }

    return rc;
}

RC GM20xFalcon::WaitForKfuseSfkLoad(UINT32 timeoutMs)
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    CHECK_RC(Initialize());

    // Ref - https://rmopengrok.lwpu.com/source/xref/sw/rel/gfw_ucode/r3/v1/src/falcon_loader.c
    if ((m_EngineBase2 != ILWALID_ENGINE_BASE) &&
        (m_ScpBase != ILWALID_ENGINE_BASE))
    {
        // Using SEC registers to determine offsets
        // Assuming same offset across falcons
        if (regs.IsSupported(MODS_PSEC_SCP_CTL_P2PRX) &&
            regs.IsSupported(MODS_PSEC_SCP_CTL0) &&
            regs.IsSupported(MODS_PSEC_FALCON_KFUSE_LOAD_CTL) &&
            regs.IsSupported(MODS_FALCON2_SEC_BASE))
        {
            UINT32 falconP2pRxOffset = (regs.LookupAddress(MODS_PSEC_SCP_CTL_P2PRX) -
                                        regs.LookupAddress(MODS_PSEC_SCP_CTL0));
            UINT32 falconKfuseOffset = (regs.LookupAddress(MODS_PSEC_FALCON_KFUSE_LOAD_CTL) -
                                        regs.LookupAddress(MODS_FALCON2_SEC_BASE));
            UINT32 p2pRxReg     = m_ScpBase     + falconP2pRxOffset;
            UINT32 kfuseLoadReg = m_EngineBase2 + falconKfuseOffset;

            // Read FALCON_KFUSE_LOAD_CTL to trigger P2P
            UINT32 kfuseRead = m_pSubdev->RegRd32(kfuseLoadReg);
            (void)kfuseRead;

            // Wait for P2P and Kfuse interfaces to quiesce
            PollKfuseSfkArgs pollArgs;
            pollArgs.pSubdev      = m_pSubdev;
            pollArgs.kfuseRegAddr = kfuseLoadReg;
            pollArgs.sfkRegAddr   = p2pRxReg;
            CHECK_RC(POLLWRAP_HW(&PollKfuseSfkLoaded, &pollArgs, AdjustTimeout(timeoutMs)));
        }
    }

    return rc;
}

bool GM20xFalcon::PollScrubbingDone(void * pArgs)
{
    PollFalconArgs *pPollArgs = (PollFalconArgs *)(pArgs);
    GpuSubdevice   *pSubdev   = pPollArgs->pSubdev;
    UINT32         baseAddr   = pPollArgs->engineBase;

    RegHal &regs = pSubdev->Regs();
    UINT32 status;
    bool bDone = false;
    if (FalconRegRdImpl(pSubdev, baseAddr, MODS_PFALCON_FALCON_DMACTL, &status) == RC::OK)
    {
        bDone = regs.TestField(status, MODS_PFALCON_FALCON_DMACTL_DMEM_SCRUBBING_DONE) &&
                regs.TestField(status, MODS_PFALCON_FALCON_DMACTL_IMEM_SCRUBBING_DONE);

    }
    return bDone;
}

bool GM20xFalcon::PollDmaDone(void *pArgs)
{
    PollFalconArgs *pPollArgs  = (PollFalconArgs *)(pArgs);
    GpuSubdevice   *pSubdev   = pPollArgs->pSubdev;
    UINT32         baseAddr   = pPollArgs->engineBase;

    RegHal &regs = pSubdev->Regs();
    UINT32 status;
    bool bDone = false;
    if (FalconRegRdImpl(pSubdev, baseAddr, MODS_PFALCON_FALCON_DMATRFCMD, &status) == RC::OK)
    {
        bDone = regs.TestField(status, MODS_PFALCON_FALCON_DMATRFCMD_FULL_FALSE) &&
                regs.TestField(status, MODS_PFALCON_FALCON_DMATRFCMD_IDLE_TRUE);
    }
    return bDone;
}

bool GM20xFalcon::PollEngineHalt(void *pArgs)
{
    PollFalconArgs *pPollArgs  = (PollFalconArgs *)(pArgs);
    GpuSubdevice   *pSubdev   = pPollArgs->pSubdev;
    UINT32         baseAddr   = pPollArgs->engineBase;

    RegHal &regs = pSubdev->Regs();
    UINT32 status;
    bool bDone = false;
    if (FalconRegRdImpl(pSubdev, baseAddr, MODS_PFALCON_FALCON_CPUCTL, &status) == RC::OK)
    {
        bDone = regs.TestField(status, MODS_PFALCON_FALCON_CPUCTL_HALTED_TRUE);
    }
    return bDone;
}

bool GM20xFalcon::PollKfuseSfkLoaded(void *pArgs)
{
    PollKfuseSfkArgs *pPollArgs = (PollKfuseSfkArgs *)(pArgs);
    GpuSubdevice     *pSubdev   = pPollArgs->pSubdev;
    UINT32           kfuseAddr  = pPollArgs->kfuseRegAddr;
    UINT32           sfkAddr    = pPollArgs->sfkRegAddr;

    RegHal &regs = pSubdev->Regs();
    UINT32 kfuseVal = pSubdev->RegRd32(kfuseAddr);
    UINT32 sfkVal   = pSubdev->RegRd32(sfkAddr);
    return (regs.Test32(sfkVal, MODS_PSEC_SCP_CTL_P2PRX_SFK_LOADED_TRUE) &&
            (regs.Test32(kfuseVal, MODS_PSEC_FALCON_KFUSE_LOAD_CTL_HWVER_BUSY_FALSE) &&
             regs.Test32(kfuseVal, MODS_PSEC_FALCON_KFUSE_LOAD_CTL_HWVER_VALID_TRUE)));
}

RC GM20xFalcon::FalconRegRd(ModsGpuRegAddress reg, UINT32 *pReadBack)
{
    return FalconRegRdImpl(m_pSubdev, m_EngineBase, reg, pReadBack);
}

RC GM20xFalcon::FalconRegRd(ModsGpuRegAddress reg, UINT32 idx, UINT32 *pReadBack)
{
    return FalconRegRdIdxImpl(m_pSubdev, m_EngineBase, reg, idx, pReadBack);
}

RC GM20xFalcon::FalconRegWr(ModsGpuRegAddress reg, UINT32 val)
{
    return FalconRegWrImpl(m_pSubdev, m_EngineBase, reg, val);
}

RC GM20xFalcon::FalconRegWr(ModsGpuRegAddress reg, UINT32 idx, UINT32 val)
{
    return FalconRegWrIdxImpl(m_pSubdev, m_EngineBase, reg, idx, val);
}

RC GM20xFalcon::Falcon2RegRd(ModsGpuRegAddress reg, UINT32 *pReadBack)
{
    return FalconRegRdImpl(m_pSubdev, m_EngineBase2, reg, pReadBack);
}

RC GM20xFalcon::Falcon2RegRd(ModsGpuRegAddress reg, UINT32 idx, UINT32 *pReadBack)
{
    return FalconRegRdIdxImpl(m_pSubdev, m_EngineBase2, reg, idx, pReadBack);
}

RC GM20xFalcon::Falcon2RegWr(ModsGpuRegAddress reg, UINT32 val)
{
    return FalconRegWrImpl(m_pSubdev, m_EngineBase2, reg, val);
}

RC GM20xFalcon::Falcon2RegWr(ModsGpuRegAddress reg, UINT32 idx, UINT32 val)
{
    return FalconRegWrIdxImpl(m_pSubdev, m_EngineBase2, reg, idx, val);
}

RC GM20xFalcon::FalconRegRdImpl
(
    GpuSubdevice *pSubdev,
    UINT32 engineBase,
    ModsGpuRegAddress reg,
    UINT32 *pReadBack
)
{
    RegHal &regs = pSubdev->Regs();
    *pReadBack = pSubdev->RegRd32(engineBase + regs.LookupAddress(reg));
    return RC::OK;
}

RC GM20xFalcon::FalconRegRdIdxImpl
(
    GpuSubdevice *pSubdev,
    UINT32 engineBase,
    ModsGpuRegAddress reg,
    UINT32 idx,
    UINT32 *pReadBack
)
{
    RegHal &regs = pSubdev->Regs();
    *pReadBack = pSubdev->RegRd32(engineBase + regs.LookupAddress(reg, idx));
    return RC::OK;
}

RC GM20xFalcon::GM20xFalcon::FalconRegWrImpl
(
    GpuSubdevice *pSubdev,
    UINT32 engineBase,
    ModsGpuRegAddress reg,
    UINT32 val
)
{
    RegHal &regs = pSubdev->Regs();
    pSubdev->RegWr32(engineBase + regs.LookupAddress(reg), val);
    return RC::OK;
}

RC GM20xFalcon::FalconRegWrIdxImpl
(
    GpuSubdevice *pSubdev,
    UINT32 engineBase,
    ModsGpuRegAddress reg,
    UINT32 idx,
    UINT32 val
)
{
    RegHal &regs = pSubdev->Regs();
    pSubdev->RegWr32(engineBase + regs.LookupAddress(reg, idx), val);
    return RC::OK;
}
