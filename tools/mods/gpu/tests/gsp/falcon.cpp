/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "falcon.h"
#include "mods_reg_hal.h"
#include "gpu/tests/rm/dmemva/ucode/dmvacommon.h"

Falcon::Falcon(GpuSubdevice *pDev)
    : m_pSubdev(pDev)
{
    MASSERT(pDev);
}

void Falcon::UCodeBarrier()
{
    WaitForStop();
    Resume();
}

UINT32 Falcon::GetMailbox0()
{
    RegHal &regs = m_pSubdev->Regs();
    return m_pSubdev->RegRd32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_MAILBOX0));
}

UINT32 Falcon::GetMailbox1()
{
    RegHal &regs = m_pSubdev->Regs();
    return m_pSubdev->RegRd32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_MAILBOX1));
}

void Falcon::SetMailbox0(UINT32 val)
{
    RegHal &regs = m_pSubdev->Regs();
    m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_MAILBOX0), val);
}

void Falcon::SetMailbox1(UINT32 val)
{
    RegHal &regs = m_pSubdev->Regs();
    m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_MAILBOX1), val);
}

UINT32 Falcon::UCodeRead32()
{
    WaitForStop();
    UINT32 data = GetMailbox1();
    Resume();
    return data;
}

void Falcon::UCodeWrite32(UINT32 data)
{
    WaitForStop();
    SetMailbox1(data);
    Resume();
}

UINT32* Falcon::GetCode() const
{
    return &(GetUCodeData()[GetCodeOffset() / sizeof(UINT32)]);
}

UINT32* Falcon::GetData(void) const
{
    return &(GetUCodeData()[GetDataOffset() / sizeof(UINT32)]);
}

UINT32* Falcon::GetAppCode(UINT32 appId) const
{
    MASSERT(appId < GetNumApps());
    return &(GetUCodeData()[GetUCodeHeader()[5 + 2 * appId] / sizeof(UINT32)]);
}

UINT32 Falcon::GetAppCodeSize(UINT32 appId) const
{
    MASSERT(appId < GetNumApps());
    return GetUCodeHeader()[5 + 2 * appId + 1];
}

UINT32* Falcon::GetAppData(UINT32 appId) const
{
    MASSERT(appId < GetNumApps());
    return &(GetUCodeData()[GetUCodeHeader()[5 + 2 * GetNumApps() + 2 * appId] /
                                            sizeof(UINT32)]);
}

UINT32 Falcon::GetAppDataSize(UINT32 appId) const
{
     MASSERT(appId < GetNumApps());
     return GetUCodeHeader()[5 + 2 * GetNumApps() + 2 * appId + 1];
}

void Falcon::UCodePatchSignature()
{
    UINT32 *pImg, *pSig;
    UINT32 patchLoc = GetPatchLocation()/sizeof(UINT32);
    pImg = GetUCodeData();
    pSig = GetSignature(true);

    for (UINT32 i = 0; i < 16 / sizeof(UINT32); i++)
    {
        pImg[patchLoc + i] = pSig[i];
    }
}

UINT32 Falcon::ImemAccess
(
    UINT32 addr,
    UINT32 *codeArr,
    UINT32 codeSize,
    ReadWrite rw,
    UINT16 tag,
    bool isSelwre
)
{
    MASSERT((addr & 0x03) == 0);
    MASSERT((codeSize & 0x03) == 0);
    MASSERT(rw == DMEMRD || rw == DMEMWR);

    RegHal &regs = m_pSubdev->Regs();

    UINT32 imemc = 0;
    regs.SetField(&imemc, MODS_PFALCON_FALCON_IMEMC_OFFS,   addr & 0xFF);
    regs.SetField(&imemc, MODS_PFALCON_FALCON_IMEMC_BLK,    addr >> 8);
    regs.SetField(&imemc, MODS_PFALCON_FALCON_IMEMC_AINCW,  rw == DMEMWR);
    regs.SetField(&imemc, MODS_PFALCON_FALCON_IMEMC_AINCR,  rw == DMEMRD);
    regs.SetField(&imemc, MODS_PFALCON_FALCON_IMEMC_SELWRE, isSelwre ? 1 : 0);
    m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_IMEMC, 0), imemc);

    UINT32 lastDword = 0;
    for (UINT32 imemAddrOff = 0; imemAddrOff < codeSize; imemAddrOff += sizeof(UINT32))
    {
        if (((addr + imemAddrOff) & 0xFF) == 0 && rw == DMEMWR)
        {
            m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_IMEMT, 0),
                               (UINT32)tag++);
        }
        if (rw == DMEMRD)
        {
            lastDword = m_pSubdev->RegRd32(GetEngineBase() +
                                           regs.LookupAddress(MODS_PFALCON_FALCON_IMEMD, 0));
            if (codeArr != NULL)
            {
                codeArr[imemAddrOff / sizeof(UINT32)] = lastDword;
            }
        }
        else if (rw == DMEMWR)
        {
            m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_IMEMD, 0),
                               codeArr[imemAddrOff / sizeof(UINT32)]);
        }
    }
    return lastDword;
}

UINT32 Falcon::DmemAccess
(
    UINT32 addr,
    UINT32 *dataArr,
    UINT32 dataSize,
    ReadWrite rw,
    UINT16 tag
)
{
    MASSERT((addr & 0x03) == 0);
    MASSERT((dataSize & 0x03) == 0);
    MASSERT(rw == DMEMRD || rw == DMEMWR);
    (void)tag;

    RegHal &regs = m_pSubdev->Regs();

    UINT32 dmemc = 0;
    regs.SetField(&dmemc, MODS_PFALCON_FALCON_DMEMC_OFFS,   addr & 0xFF);
    regs.SetField(&dmemc, MODS_PFALCON_FALCON_DMEMC_BLK,    addr >> 8);
    regs.SetField(&dmemc, MODS_PFALCON_FALCON_DMEMC_AINCW,  rw == DMEMWR);
    regs.SetField(&dmemc, MODS_PFALCON_FALCON_DMEMC_AINCR,  rw == DMEMRD);
    m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_DMEMC, 0), dmemc);

    UINT32 lastDword = 0;
    for (UINT32 dmemAddrOff = 0; dmemAddrOff < dataSize; dmemAddrOff += sizeof(UINT32))
    {
        if (rw == DMEMRD)
        {
            lastDword = m_pSubdev->RegRd32(GetEngineBase() +
                                           regs.LookupAddress(MODS_PFALCON_FALCON_DMEMD, 0));
            if (dataArr != NULL)
            {
                dataArr[dmemAddrOff / sizeof(UINT32)] = lastDword;
            }
        }
        else if (rw == DMEMWR)
        {
            m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_DMEMD, 0),
                               dataArr[dmemAddrOff / sizeof(UINT32)]);
        }
    }
    return lastDword;
}

bool Falcon::IsScrubbingDone()
{
    RegHal &regs = m_pSubdev->Regs();
    UINT32 falconDmaCtl = m_pSubdev->RegRd32(GetEngineBase() +
                                             regs.LookupAddress(MODS_PFALCON_FALCON_DMACTL));
    return (regs.TestField(falconDmaCtl, MODS_PFALCON_FALCON_DMACTL_DMEM_SCRUBBING_DONE) ||
            regs.TestField(falconDmaCtl, MODS_PFALCON_FALCON_DMACTL_IMEM_SCRUBBING_DONE));
}

bool Falcon::IsNotRunning()
{
    RegHal &regs = m_pSubdev->Regs();
    UINT32 cpuCtl = m_pSubdev->RegRd32(GetEngineBase() +
                                       regs.LookupAddress(MODS_PFALCON_FALCON_CPUCTL));
    return regs.TestField(cpuCtl, MODS_PFALCON_FALCON_CPUCTL_HALTED, 0x1) ||
           regs.TestField(cpuCtl, MODS_PFALCON_FALCON_CPUCTL_STOPPED, 0x1);
}

bool Falcon::IsHalted()
{
    RegHal &regs = m_pSubdev->Regs();
    UINT32 cpuCtl = m_pSubdev->RegRd32(GetEngineBase() +
                                       regs.LookupAddress(MODS_PFALCON_FALCON_CPUCTL));
    return (regs.TestField(cpuCtl, MODS_PFALCON_FALCON_CPUCTL_HALTED, 0x1));
}

bool Falcon::IsStopped()
{
    RegHal &regs = m_pSubdev->Regs();
    UINT32 cpuCtl = m_pSubdev->RegRd32(GetEngineBase() +
                                       regs.LookupAddress(MODS_PFALCON_FALCON_CPUCTL));
    return (regs.TestField(cpuCtl, MODS_PFALCON_FALCON_CPUCTL_STOPPED, 0x1));
}

RC Falcon::Poll(bool (Falcon::*PollFunction)(), UINT32 timeoutUS)
{
    const UINT32 SLEEP_US = 1000;
    MASSERT(timeoutUS >= SLEEP_US);
    for (UINT32 uSec = 0; uSec < timeoutUS; uSec += SLEEP_US)
    {
        Utility::SleepUS(SLEEP_US);
        if ((this->*PollFunction)())
            return OK;
    }
    Printf(Tee::PriError, "TIMEOUT\n");
    return RC::TIMEOUT_ERROR;
}

RC Falcon::WaitForHalt()
{
    RC rc;
    CHECK_RC(Poll(&Falcon::IsNotRunning, FALC_RESET_TIMEOUT_US));
    if (!IsHalted())
    {
        Printf(Tee::PriError, "Falcon waited when it should have halted.\n");
        return RC::INCORRECT_MODE;
    }
    return OK;
}

RC Falcon::WaitForStop()
{
    RC rc;
    CHECK_RC(Poll(&Falcon::IsNotRunning, FALC_RESET_TIMEOUT_US));
    if (!IsStopped())
    {
        Printf(Tee::PriError, "Falcon halted when it should have waited.\n");
        return RC::INCORRECT_MODE;
    }
    return OK;
}

RC Falcon::WaitForScrub()
{
    RC rc;
    CHECK_RC(Poll(&Falcon::IsScrubbingDone, FALC_RESET_TIMEOUT_US));
    return rc;
}

void Falcon::ClearInterruptOnHalt()
{
    RegHal &regs = m_pSubdev->Regs();
    m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_IRQMCLR),
                       regs.LookupValue(MODS_PFALCON_FALCON_IRQMCLR_HALT_SET));
}

void Falcon::Bootstrap(UINT32 bootvector)
{
    RegHal &regs = m_pSubdev->Regs();

    // Clear DMACTL
     m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_DMACTL), 0);

    // Set Bootvec
    UINT32 bootVec = regs.SetField(MODS_PFALCON_FALCON_BOOTVEC_VEC, bootvector);
    m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_BOOTVEC),
                       bootVec);

    UINT32 cpuCtl = regs.SetField(MODS_PFALCON_FALCON_CPUCTL_STARTCPU_TRUE);
    m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_CPUCTL),
                       cpuCtl);
    if (IsNotRunning())
    {
        Printf(Tee::PriWarn, "DETECTED SUSPICIOUS WAIT/HALT IMMEDIATELY AFTER BOOT."
                             "FALCON POSSIBLY IGNORED STARTCPU\n");
    }
}

void Falcon::Resume()
{
    RegHal &regs = m_pSubdev->Regs();

    UINT32 irqsset = regs.SetField(MODS_PFALCON_FALCON_IRQSSET_SWGEN0_SET);
    m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_IRQSSET),
                       irqsset);
    if (IsNotRunning())
    {
        Printf(Tee::PriWarn, "DETECTED SUSPICIOUS WAIT/HALT IMMEDIATELY AFTER RESUME."
                             "FALCON POSSIBLY IGNORED INTERRUPT\n");
    }
}

RC Falcon::Reset()
{
    RC rc;

    RegHal &regs = m_pSubdev->Regs();
    EngineReset();

    // Disable context DMA support
    m_pSubdev->RegWr32(GetEngineBase() + regs.LookupAddress(MODS_PFALCON_FALCON_DMACTL),
                       regs.LookupValue(MODS_PFALCON_FALCON_DMACTL_REQUIRE_CTX_FALSE));

    UINT32 pmc = regs.Read32(MODS_PMC_ENABLE);
    pmc = SetPMC(pmc, 0);
    regs.Write32(MODS_PMC_ENABLE, pmc);
    pmc = SetPMC(pmc, 1);
    regs.Write32(MODS_PMC_ENABLE, pmc);

    // Wait for the scrubbing to stop
    CHECK_RC(WaitForScrub());

    return rc;
}

void Falcon::HandleRegRWRequest()
{
    UINT32 addr = UCodeRead32();
    UINT32 isRead = UCodeRead32();

    if (isRead)
    {
        UCodeWrite32(m_pSubdev->RegRd32(addr));
    }
    else
    {
        UINT32 data = UCodeRead32();
        m_pSubdev->RegWr32(addr, data);
        UCodeBarrier();
    }
}

RC Falcon::GetTestRC()
{
    UINT32 m0 = GetMailbox0();
    UINT32 m1 = GetMailbox1();;
    switch (m0)
    {
        case DMVA_NULL:
            Printf(Tee::PriError, "The return code was not set by falcon.\n");
            return RC::HW_ERROR;
        case DMVA_FAIL:
            Printf(Tee::PriError, "Generic failure.\n");
            return RC::HW_ERROR;
        case DMVA_ASSERT_FAIL:
            Printf(Tee::PriError, "Ucode assert failed on line %u.\n", m1);
            return RC::HW_ERROR;
        case DMVA_BAD_TAG:
            Printf(Tee::PriError, "Bad tag. Expected %04x, but got %04x\n",
                                   m1 >> 16, m1 & 0xffff);
            return RC::HW_ERROR;
        case DMVA_BAD_STATUS:
            Printf(Tee::PriError, "Bad status. Expected %u, but got %u\n",
                                   (m1 >> 16), (m1 & 0xffff));
            return RC::HW_ERROR;
        case DMVA_BAD_MASK:
            Printf(Tee::PriError, "Bad mask. Expected %01x, but got %01x\n",
                                   m1 >> 16, m1 & 0xffff);
            return RC::HW_ERROR;
        case DMVA_DMEMC_BAD_DATA_READ:
            Printf(Tee::PriError, "DMEMC: bad data read.\n");
            return RC::HW_ERROR;
        case DMVA_DMEMC_BAD_DATA_WRITTEN:
            Printf(Tee::PriError, "DMEMC: bad data written.\n");
            return RC::HW_ERROR;
        case DMVA_DMEMC_BAD_LVLERR:
            Printf(Tee::PriError, "Bad DMEMC.lvlerr\n");
            return RC::HW_ERROR;
        case DMVA_DMLVL_SHOULD_EXCEPT:
            Printf(Tee::PriError, "Read/Write should have caused exception, but it didn't\n");
            return RC::HW_ERROR;
        case DMVA_EMEM_BAD_DATA:
            Printf(Tee::PriError, "Bad data at address %08x\n", m1);
            return RC::HW_ERROR;
        case DMVA_BAD_DATA:
            Printf(Tee::PriError, "Bad data\n");
            return RC::HW_ERROR;
        case DMVA_DMEMC_BAD_MISS:
            Printf(Tee::PriError, "Bad DMEMC.miss\n");
            return RC::HW_ERROR;
        case DMVA_BAD_ZEROS:
            Printf(Tee::PriError, "Bad dmblk.zeros\n");
            return RC::HW_ERROR;
        case DMVA_BAD_VALID:
            Printf(Tee::PriError, "Bad dm{blk,tag}.valid\n");
            return RC::HW_ERROR;
        case DMVA_BAD_PENDING:
            Printf(Tee::PriError, "Bad dm{blk,tag}.pending\n");
            return RC::HW_ERROR;
        case DMVA_BAD_DIRTY:
            Printf(Tee::PriError, "Bad dm{blk,tag}.dirty\n");
            return RC::HW_ERROR;
        case DMVA_DMA_TAG_BAD_NEXT_TAG:
            Printf(Tee::PriError, "Bad DMA tag: expected increment\n");
            return RC::HW_ERROR;
        case DMVA_DMA_TAG_HANDLER_WAS_NOT_CALLED:
            Printf(Tee::PriError, "Interrupt handler was not called after DMA2\n");
            return RC::HW_ERROR;
        case DMVA_UNEXPECTED_EXCEPTION:
            Printf(Tee::PriError, "Exception PC: %05x\n",
                                   m_pSubdev->Regs().GetField(m1, MODS_PFALCON_FALCON_EXCI_EXPC));
            return RC::HW_ERROR;
        case DMVA_PLATFORM_NOT_SUPPORTED:
            Printf(Tee::PriError, "This test is not supported on this engine\n");
            return RC::HW_ERROR;
        case DMVA_OK:
            return OK;
        default:
            Printf(Tee::PriError, "Corrupted return code. (MAILBOX0, MAILBOX1) = (%08x, %08x)\n",
                                   m0, m1);
            return RC::HW_ERROR;
    }
}

RC Falcon::VerifyDmlvlExceptionAccess()
{
    RC rc;
    RegHal &regs = m_pSubdev->Regs();

    UINT32 bShouldExcept = UCodeRead32();
    UINT32 dmemAddr = UCodeRead32();
    CHECK_RC(WaitForHalt());

    UINT32 exci    = m_pSubdev->RegRd32(GetEngineBase() +
                                        regs.LookupAddress(MODS_PFALCON_FALCON_EXCI));
    UINT32 exci2   = m_pSubdev->RegRd32(GetEngineBase() +
                                        regs.LookupAddress(MODS_PFALCON_FALCON_EXCI2));
    UINT32 exCause = regs.GetField(exci, MODS_PFALCON_FALCON_EXCI_EXCAUSE);

    UINT32 expectedCause = bShouldExcept ?
                           regs.LookupValue(MODS_PFALCON_FALCON_EXCI_EXCAUSE_DMEM_PERMISSION_INS):
                           0;
    UINT32 expectedAddr  = bShouldExcept ? dmemAddr : 0;

    bool badCause = exCause != expectedCause;
    bool badAddr = exci2 != dmemAddr && exci2 != (dmemAddr - 4);
    if (badCause || badAddr)
    {
        Printf(Tee::PriError, "Expected (EXCI.excause, EXCI2) to be (%02x, %08x)"
                              "but it was (%02x, %08x). EXCI = %08x\n",
                              expectedCause, expectedAddr, exCause, exci2, exci);
        rc = RC::HW_ERROR;
    }

    return rc;
}
