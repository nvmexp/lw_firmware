/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2005-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "azactrl.h"
#include "azactrlmgr.h"
#include "azacdc.h"
#include "azadmae.h"
#include "azafpci.h"
#include "azareg.h"
#include "azautil.h"
#include "device/include/memtrk.h"
#include "device/include/mcp.h"
#include "core/include/platform.h"
#include "core/include/xp.h"
#include "core/include/utility.h"
#include "core/include/tasker.h"
#include "core/include/pci.h"
#include "cheetah/include/devid.h"
#include "ada/ad102/dev_lw_xve1.h" // LW_XVE1_PRIV_MISC_1
#include "lwmisc.h" // FLD_TEST_DRF
#include <string.h>
#include <ctype.h>

#ifdef LWCPU_FAMILY_ARM
    #include "cheetah/include/tegrareg.h"
#endif
#include "cheetah/include/tegradrf.h"

#ifdef CLASS_NAME
    #undef CLASS_NAME
#endif
#define CLASS_NAME "AzaliaController"

#define CORB_SIZE 256
#define RIRB_SIZE 256
#define DMA_POS_BYTES_PER_STREAM 8
#define MAX_NUM_OSTREAMS 15
#define MAX_NUM_ISTREAMS 15

CONTROLLER_NAMED_ISR(0, AzaliaController, Common, Isr)
CONTROLLER_NAMED_ISR(1, AzaliaController, Common, Isr)
CONTROLLER_NAMED_ISR(2, AzaliaController, Common, Isr)
CONTROLLER_NAMED_ISR(3, AzaliaController, Common, Isr)
CONTROLLER_NAMED_ISR(4, AzaliaController, Common, Isr)
CONTROLLER_NAMED_ISR(5, AzaliaController, Common, Isr)

namespace
{
    UINT32 GetAddressBits(UINT32 deviceId)
    {
        // Only Fermi+ is supported lwrrently in MODS
        switch (deviceId)
        {
            case LW_DEVID_HDA:
                return 32;

            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK104:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK106:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK107:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK110:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK208:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM107:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM200:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM204:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM206:
                return 40;

            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP100:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP102:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP104:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP106:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP107:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP108:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GV100:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU102:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU104:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU106:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU108:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU116:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU117:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA100:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA102:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA103:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA104:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA106:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA107:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD102:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD103:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD104:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD106:
            case LW_XVE1_ID_DEVICE_CHIP_AZALIA_AD107:
                return 47;

            default:
                Printf(Tee::PriError, "Unable to determine address bits for devid 0x%08x\n", deviceId);
                break;
        }

        return 32;
    }
}
void AzaliaController::IntControllerStatus::Clear()
{
    corbMemErr = false;
    rirbOverrun = false;
    rirbIntFlag = false;
    statests = 0;
}

void AzaliaController::IntControllerStatus::Merge(const IntControllerStatus ToMerge)
{
    corbMemErr |= ToMerge.corbMemErr;
    rirbOverrun |= ToMerge.rirbOverrun;
    rirbIntFlag |= ToMerge.rirbIntFlag;
    statests |= ToMerge.statests;
}

void AzaliaController::IntStreamStatus::Clear()
{
    dese = false;
    fifoe = false;
    bcis = false;
}

void AzaliaController::IntStreamStatus::Merge(const IntStreamStatus ToMerge)
{
    dese = dese || ToMerge.dese;
    fifoe = fifoe || ToMerge.fifoe;
    bcis = bcis || ToMerge.bcis;
}

bool AzaliaController::IsIch6()
{
    return m_IsIch6Mode;
}

RC AzaliaController::SetEcrMode(bool IsEcrMode)
{
    RC rc = OK;
    if (!m_IsIch6Mode)
    {
        UINT32 value32 = 0;
        CHECK_RC( CfgRd32(AZA_DBG_CFG_2, &value32) );

        if (IsEcrMode)
            value32 = FLD_SET_RF_NUM(AZA_DBG_CFG_2_SPEC_REV_CYA, 1, value32);
        else
            value32 = FLD_SET_RF_NUM(AZA_DBG_CFG_2_SPEC_REV_CYA, 0, value32);

        CHECK_RC( CfgWr32(AZA_DBG_CFG_2, value32) );

        m_IsEcrMode = IsEcrMode;
        Printf(Tee::PriDebug, "Ecr mode = %d\n", m_IsEcrMode);
    }

    return rc;
}

RC AzaliaController::SetPowerState(PowerState Target)
{
    RC rc;
    UINT32 value32 = 0;
    UINT32 pmRegAddr = 0;

    pmRegAddr = AZA_PCI_CFG_18; // for LW Chips
    if (m_IsIch6Mode)
        pmRegAddr = AZA_MSI_ADDR1; // For Intel chip register meanings are different

    switch (Target)
    {
        case D3HOT:
            // This may need to change if we implement other power states (like
            // D1 and D2).  For now, we're just assuming that if we already have
            // some saved registers, we're in D3HOT.
            if (m_vPowStateConfigRegisters.size() != 0)
            {
                Printf(Tee::PriLow, "Already in D3HOT state\n");
                break;
            }

            // Clear PME status
            CHECK_RC(CfgWr32(pmRegAddr, RF_NUM(AZA_PCI_CFG_18_PME_STATUS,1)));

            Printf(Tee::PriLow, "Saving values in PCI Config Registers...\n");
            for (UINT32 i=0; i<=0xff; i+=4)
            {
                CHECK_RC(CfgRd32(i, &value32));
                m_vPowStateConfigRegisters.push_back(value32);
            }

            // Goto D3hot state
            Printf(Tee::PriLow, "Going to D3hot...\n");
            CHECK_RC(CfgWr08(pmRegAddr, AZA_PCI_CFG_18_PM_STATE_D3));
            break;

        case D0:
            // This may need to change if we implement other power states.  For
            // now, we're just assuming that if don't have any saved registers,
            // we're in D0.
            if (m_vPowStateConfigRegisters.size() == 0)
            {
                Printf(Tee::PriLow, "Already in D0 state\n");
                break;
            }

            // Report PME status
            CHECK_RC(CfgRd32(pmRegAddr, &value32));
            if (FLD_TEST_RF_NUM(AZA_PCI_CFG_18_PME_STATUS, 1, value32))//&0x00008000)
                Printf(Tee::PriLow, "PME assertion detected while in D3.\n");
            else
                Printf(Tee::PriLow, "No PME assertion detected while in D3.\n");

            // Returning to D0 state
            Printf(Tee::PriLow, "Returning to D0...\n");
            CHECK_RC(CfgWr08(pmRegAddr, AZA_PCI_CFG_18_PM_STATE_D0));
            AzaliaUtilities::Pause(4); // msec

            //Restore values in Config Space, so that we can access Mem space
            Printf(Tee::PriLow, "Restoring values in PCI Config Registers...\n");
            for (UINT32 i=0; i<=0xff; i+=4)
            {
                CHECK_RC(CfgWr32(i, m_vPowStateConfigRegisters[i/4]));
            }
            m_vPowStateConfigRegisters.clear();

            Printf(Tee::PriLow, "After PCI Config Space Restoration:\n");
            CHECK_RC(CfgRd32(PCI_BAR_OFFSET(0), &value32));
            Printf(Tee::PriLow, "    BAR 0 = 0x%08x\n", value32);
            CHECK_RC(CfgRd32(PCI_BAR_OFFSET(1), &value32));
            Printf(Tee::PriLow, "    BAR 1 = 0x%08x\n", value32);
            break;

        default:
            Printf(Tee::PriNormal, "Unknown target state\n");
    }
    return rc;
}

RC AzaliaController::ToggleRandomization(bool IsEnable)
{
    m_IsRandomEnabled = IsEnable;
    return OK;
}

RC AzaliaController::ToggleUnsolResponses(bool IsEnable)
{
    RC rc;
    UINT32 value = 0;
    CHECK_RC(RegRd(GCTL, &value));
    if (IsEnable)
        value = FLD_SET_RF_DEF(AZA_GCTL_UNSOL, _ACCEPTED, value);
    else
        value = FLD_SET_RF_DEF(AZA_GCTL_UNSOL, _NOT_ACCEPTED, value);
    CHECK_RC(RegWr(GCTL, value));
    return OK;
}

AzaliaController::AzaliaController()
    : Controller(true)
{
    m_IsRandomEnabled = false;
    m_pBar0 = 0;
    m_NInputStreamsSupported = 0;
    m_NOutputStreamsSupported = 0;
    m_NBidirStreamsSupported = 0;
    m_NCodecs = 0;
    m_vpStreamEngines.clear();
    for (UINT32 i=0; i<MAX_NUM_SDINS; i++)
    {
        m_pCodecTable[i] = NULL;
        m_SolResponses[i].clear();
        m_UnsolResponses[i].clear();
    }
    m_pCorbBaseAddr = 0;
    m_pRirbBaseAddr = 0;
    m_pDmaPosBaseAddr = 0;
    m_CorbSize = CORB_SIZE;
    m_RirbSize = RIRB_SIZE;
    m_RirbReadPtr = 0;
    m_IntStatus.Clear();
    m_IsIch6Mode = false;
    m_IsEcrMode = false;

    m_IsrCnt[0] = 0; //for IRQ_RAW
    m_IsrCnt[1] = 0; //for IRQ_REAL
    m_rirbSts = 0;
    m_rirbWp = 0;

    m_NumMsiLines = AZA_DEFAULT_MSILINES;
    m_AddrBits    = 32;

    m_vPowStateConfigRegisters.clear();

    memset(&m_IntEnables, 0, sizeof(m_IntEnables));
    m_IsAllocFromPcieNode = false;
    m_IsUseMappedPhysAddr = true;
}

AzaliaController::~AzaliaController()
{
    Uninit();
}

const char* AzaliaController::GetName()
{
    return "AzaliaController";
}

RC AzaliaController::DumpCorb(Tee::Priority Pri, UINT16 FirstEntry, UINT16 LastEntry)
{
    RC rc;
    UINT32 wp, rp;
    CHECK_RC(RegRd(CORBWP, &wp));
    wp = RF_VAL(AZA_CORBWP_CORBWP, wp);
    CHECK_RC(RegRd(CORBRP, &rp));
    rp = RF_VAL(AZA_CORBWP_CORBRP, rp);

    for (UINT16 i=FirstEntry; i<=LastEntry; i++)
    {
        string pointer = "";
        if (i == rp)
        {
            if (i == wp)
            {
                pointer = "<--- RP/WP";
            } else {
                pointer = "<--- RP";
            }
        }
        else if (i == wp)
        {
            pointer = "<--- WP";
        }

        Printf(Pri,
               "        [0x%04x] 0x%08x %s\n",
               i,
               m_pCorbBaseAddr[i],
               pointer.c_str());
    }

    return OK;
}

RC AzaliaController::CorbGetFreeSpace(UINT16 *pSpace)
{
    MASSERT(pSpace);

    RC rc;
    UINT32 wp, rp;
    CHECK_RC(RegRd(CORBWP, &wp));
    wp = RF_VAL(AZA_CORBWP_CORBWP, wp); // Last verb written by sw
    CHECK_RC(RegRd(CORBRP, &rp));
    rp = RF_VAL(AZA_CORBWP_CORBRP, rp); // Last verb fetched by controller
    if (wp >= rp)
    {
        *pSpace = m_CorbSize - (wp - rp);
    }
    else
    {
        *pSpace = rp - wp;
    }

    return OK;
}

RC AzaliaController::CorbGetTotalSize(UINT16 *pSize)
{
    MASSERT(pSize);

    *pSize = m_CorbSize;
    return OK;
}

RC AzaliaController::CorbInitialize(UINT16 Size, bool IsMemErrIntEnable)
{
    LOG_ENT();

    RC rc;
    UINT32 value;

    // Stop the CORB
    CHECK_RC(CorbToggleRunning(false));

    // Make sure CORB is allocated. CORB must be aligned on 128B boundary
    // (or is it 1024B boundary -- r0.7 spec is unclear).
    if (!m_pCorbBaseAddr)
    {
        CHECK_RC(AzaliaUtilities::AlignedMemAlloc(CORB_SIZE * sizeof(UINT32),
                                                  0x80, (void**) &m_pCorbBaseAddr, m_AddrBits,
                                                  GetMemoryType(),
                                                  m_IsRandomEnabled,
                                                  this));
        MASSERT(m_pCorbBaseAddr);
    }

    // Program CORB base address registers
    MASSERT(sizeof(PHYSADDR) == sizeof(UINT32)*2);
    PHYSADDR physCorbBaseAddr;
    if (GetIsUseMappedPhysAddr())
    {
        CHECK_RC(MemoryTracker::GetMappedPhysicalAddress(m_pCorbBaseAddr, this, &physCorbBaseAddr));
    }
    else
    {
        physCorbBaseAddr = Platform::VirtualToPhysical(m_pCorbBaseAddr);
    }
    CHECK_RC(RegWr(CORBLBASE, (UINT32) physCorbBaseAddr));
    CHECK_RC(RegWr(CORBUBASE, (UINT32) (physCorbBaseAddr >> 32)));

    // Program CORB size
    CHECK_RC(RegRd(CORBSIZE, &value));
    switch (Size)
    {
        case 2:
            if (!(value & RF_NUM(AZA_CORBCTL_CORBSZCAP, 0x1)))
            {
                Printf(Tee::PriLow, "Attempting to set CORB size to unsupported value\n");
            }
            value |= RF_DEF(AZA_CORBCTL_CORBSIZE, _2ENTRIES);
            break;
        case 16:
            if (!(value & RF_NUM(AZA_CORBCTL_CORBSZCAP, 0x2)))
            {
                Printf(Tee::PriLow, "Attempting to set CORB size to unsupported value\n");
            }
            value |= RF_DEF(AZA_CORBCTL_CORBSIZE, _16ENTRIES);
            break;
        case 256:
            if (!(value & RF_NUM(AZA_CORBCTL_CORBSZCAP, 0x4)))
            {
                Printf(Tee::PriLow, "Attempting to set CORB size to unsupported value\n");
            }
            value |= RF_DEF(AZA_CORBCTL_CORBSIZE, _256ENTRIES);
            break;
        default:
            Printf(Tee::PriError, "Invalid CORB size (%d) requested\n", Size);
            return RC::BAD_PARAMETER;
    }
    CHECK_RC(RegWr(CORBSIZE, value));

    // Verify programmed size
    CHECK_RC(RegRd(CORBSIZE, &value));
    switch (RF_VAL(AZA_CORBCTL_CORBSIZE, value))
    {
        case AZA_CORBCTL_CORBSIZE_2ENTRIES:   m_CorbSize = 2;   break;
        case AZA_CORBCTL_CORBSIZE_16ENTRIES:  m_CorbSize = 16;  break;
        case AZA_CORBCTL_CORBSIZE_256ENTRIES: m_CorbSize = 256; break;
        default:
            Printf(Tee::PriError, "Invalid CORB size reported!\n");
            return RC::HW_STATUS_ERROR;
    }

    // Reset CORB read and write pointers
    CHECK_RC(CorbReset());

    // Set CORB interrupt enables
    CHECK_RC(IntToggle(INT_CORB_MEM_ERR, IsMemErrIntEnable));

    // Start the CORB
    CHECK_RC(CorbToggleRunning(true));
    LOG_EXT();
    return OK;
}

RC AzaliaController::CorbIsRunning(bool* pIsRunning)
{
    RC rc;
    UINT32 value;
    CHECK_RC(RegRd(CORBCTL, &value));
    *pIsRunning = FLD_TEST_RF_DEF(AZA_CORBCTL_CORBRUN, _RUN, value);
    return OK;
}

RC AzaliaController::CorbPush(UINT32 Verbs[], UINT16 lwerbs, UINT16* nPushed)
{
    RC rc;
    UINT32 value;

    // Check for memory error
    CHECK_RC(RegRd(CORBSTS, &value));
    if (FLD_TEST_RF_DEF(AZA_CORBCTL_CMEI, _ERROR, value))
    {
        Printf(Tee::PriError, "CORB memory error detected!\n");
        return RC::HW_STATUS_ERROR;
    }

    // Check that CORB is allocated
    if (!m_pCorbBaseAddr)
    {
        Printf(Tee::PriError, "CORB base address not programmed\n");
        return RC::SOFTWARE_ERROR;
    }

    UINT16 freeSpace;
    CHECK_RC(CorbGetFreeSpace(&freeSpace));
    UINT16 lwerbsToWrite = (freeSpace >= lwerbs) ? lwerbs : freeSpace;

    if (lwerbsToWrite)
    {
        // Get the write pointer
        UINT32 wp;
        CHECK_RC(RegRd(CORBWP, &wp));
        wp = RF_VAL(AZA_CORBWP_CORBWP, wp);

        // Write the verbs
        for (UINT32 i=0; i<lwerbsToWrite; i++)
        {
            wp++;
            MASSERT(wp <= m_CorbSize);
            if (wp == m_CorbSize)
            {
                wp -= m_CorbSize;
            }
            MEM_WR32(m_pCorbBaseAddr + wp, Verbs[i]);
            Printf(Tee::PriDebug, "  Pushed verb 0x%08x at slot %d (addr 0x%p)\n",
                   Verbs[i], wp, (m_pCorbBaseAddr+wp));
        }

        // Update the write pointer
        CHECK_RC(RegWr(CORBWP, RF_NUM(AZA_CORBWP_CORBWP, wp)));
        // Read to make sure that write-combine buffer is flushed.
        // Technically this only needs to be done on simulation, but it
        // doesn't hurt to have it here for real hw.
        // to remove clang error
        UINT32 dummy = MEM_RD32(m_pCorbBaseAddr+wp);
        dummy = dummy + 0;
    }

    *nPushed = lwerbsToWrite;
    return OK;
}

RC AzaliaController::PrintCorbState(Tee::Priority Pri) const
{
    RC rc;
    UINT32 value;
    Printf(Pri, "    CORB Register State:\n");
    CHECK_RC(RegRd(CORBCTL, &value));
    Printf(Pri, "        CORBCTL:         0x%08x\n", value);
    CHECK_RC(RegRd(CORBSTS, &value));
    Printf(Pri, "        CORBSTS:         0x%08x\n", value);
    CHECK_RC(RegRd(CORBSIZE, &value));
    Printf(Pri, "        CORBSIZE:        0x%08x\n", value);
    CHECK_RC(RegRd(CORBWP, &value));
    Printf(Pri, "        CORBWP:          0x%08x\n", value);
    CHECK_RC(RegRd(CORBRP, &value));
    Printf(Pri, "        CORBRP:          0x%08x\n", value);
    CHECK_RC(RegRd(AZA_CORBLBASE, &value));
    Printf(Pri, "        CORBLBASE:       0x%08x\n", value);
    CHECK_RC(RegRd(AZA_CORBUBASE, &value));
    Printf(Pri, "        CORBUBASE:       0x%08x\n", value);
    Printf(Pri, "        VirtualBase:     " PRINT_PHYS "\n", PRINT_FMT_PTR(m_pCorbBaseAddr));
    return OK;
}

RC AzaliaController::CorbReset()
{
    LOG_ENT();

    RC rc;
    UINT32 value;

    // Make sure CORB is stopped
    CHECK_RC(RegRd(CORBCTL, &value));
    if (FLD_TEST_RF_DEF(AZA_CORBCTL_CORBRUN, _RUN, value))
    {
        Printf(Tee::PriError, "CORB is running. Cannot reset pointers.\n");
        return RC::SOFTWARE_ERROR;
    }

    // Clear RP
    CHECK_RC(RegWr(CORBRP, RF_NUM(AZA_CORBWP_RST, 0x1)));

    // The Azalia spec states that the CORB Read Pointer Reset bit
    // is always read as a zero.  However, Intel's ICH6 implementation
    // is different.  Software must read back a 1 and then clear
    // the bit to return the CORB to operation.
    if (m_IsIch6Mode || m_IsEcrMode)
    {
        AzaRegPollArgs pollArgs;
        pollArgs.pAzalia = this;
        pollArgs.reg = CORBRP;
        pollArgs.mask = 0xffffffff;
        pollArgs.value = RF_NUM(AZA_CORBWP_RST, 0x1);
        rc = POLLWRAP_HW(AzaRegWaitPoll, &pollArgs, 10000);
        if (rc != OK)
        {
            Printf(Tee::PriError, "Timeout waiting for Crush19 Azalia CORB "
                                  "read pointer status change on Intel Chipset!\n");
            return RC::TIMEOUT_ERROR;
        }
        CHECK_RC(RegWr(CORBRP,0x0000)); // ICH6 requires that sw clear the bit
    }

    // Verify RP Reset
    CHECK_RC(RegRd(CORBRP, &value));
    if (value != 0)
    {
        Printf(Tee::PriError, "CORB RP did not reset! (0x%08x)\n", value);
        return RC::HW_STATUS_ERROR;
    }

    // Clear WP
    CHECK_RC(RegWr(CORBWP, 0));
    CHECK_RC(RegRd(CORBWP, &value));
    if (value != 0)
    {
        Printf(Tee::PriError, "CORB WP did not reset! (0x%08x)\n", value);
        return RC::HW_STATUS_ERROR;
    }

    LOG_EXT();
    return OK;
}

RC AzaliaController::CorbToggleCoherence(bool IsEnable)
{
    RC rc;

    if (!m_IsIch6Mode)
    {
        UINT32 value = 0;
        CHECK_RC(CfgRd32(AZA_CFG_19, &value));
        value = FLD_SET_RF_NUM(AZA_CFG_19_CORB_COH, (IsEnable?1:0), value);
        CHECK_RC(CfgWr32(AZA_CFG_19, value));
    }

    return OK;
}

RC AzaliaController::CorbToggleRunning(bool Running)
{
    RC rc;
    UINT32 value, valueToWrite;
    CHECK_RC(RegRd(CORBCTL, &value));
    valueToWrite = value & ~RF_SHIFTMASK(AZA_CORBCTL_CORBRUN);
    if (Running)
    {
        valueToWrite |= RF_DEF(AZA_CORBCTL_CORBRUN, _RUN);
    } else {
        valueToWrite |= RF_DEF(AZA_CORBCTL_CORBRUN, _STOP);
    }
    if (value != valueToWrite)
    {
        CHECK_RC(RegWr(CORBCTL, valueToWrite));
        AzaRegPollArgs pollArgs;
        pollArgs.pAzalia = this;
        pollArgs.reg = CORBCTL;
        pollArgs.mask = RF_SHIFTMASK(AZA_CORBCTL_CORBRUN);
        pollArgs.value = valueToWrite & RF_SHIFTMASK(AZA_CORBCTL_CORBRUN);
        rc = POLLWRAP_HW(AzaRegWaitPoll, &pollArgs, Tasker::GetDefaultTimeoutMs());
        if (rc != OK)
        {
            Printf(Tee::PriError, "Timeout waiting on CORB DMA state\n");
            return RC::TIMEOUT_ERROR;
        }
    }
    return OK;
}

RC AzaliaController::ToggleControllerInterrupt(bool IsEnable)
{
    RC rc;

    if (IsEnable)
    {
        CHECK_RC(IntToggle(INT_GLOBAL, true));
        CHECK_RC(IntToggle(INT_CONTROLLER, true));
    }
    else
    {
        CHECK_RC(IntToggle(INT_GLOBAL, false));
        CHECK_RC(IntToggle(INT_CONTROLLER, false));
    }
    return OK;
}

inline UINT08 AzaliaController::AzaMemRd08(UINT08* Address) const
{
    #ifndef LWCPU_FAMILY_ARM
        return MEM_RD08(Address);
    #endif

    UINT32 readWord = MEM_RD32( (size_t)Address & ~0x3);
    UINT32 loBit = ( (size_t)Address & 0x3) * 8;
    UINT32 hiBit = loBit + 7;
    return RF_VAL(hiBit:loBit,readWord);
}

inline UINT16 AzaliaController::AzaMemRd16(UINT08* Address) const
{
    #ifndef LWCPU_FAMILY_ARM
        return MEM_RD16(Address);
    #endif

    UINT32 readWord = MEM_RD32( (size_t)Address & ~0x2);
    UINT32 loBit = ( (size_t)Address & 0x2) * 8;
    UINT32 hiBit = loBit + 15;
    return RF_VAL(hiBit:loBit,readWord);
}

inline void AzaliaController::AzaMemWr08(UINT08* Address, UINT08 Data) const
{
    #ifdef LWCPU_FAMILY_ARM
        UINT32 byteEnable = 0;
        UINT32 word = Data << (((size_t)Address & 0x3) * 8);

        byteEnable = RF_SET(T30_HDA_DFPCI_BEN_0_EN_DFPCI_BEN);
        byteEnable |= ~(1<<((size_t)Address & 0x3)) & 0xf;

        MEM_WR32(reinterpret_cast<UINT08*>(m_pRegisterBase) + T30_HDA_DFPCI_BEN_0, byteEnable);
        MEM_WR32((size_t)Address & ~0x3, word);

        byteEnable &= ~(RF_SET(T30_HDA_DFPCI_BEN_0_EN_DFPCI_BEN));
        MEM_WR32(reinterpret_cast<UINT08*>(m_pRegisterBase) + T30_HDA_DFPCI_BEN_0, byteEnable);
        return;
    #endif

    MEM_WR08(Address,Data);
    return;

}
inline void AzaliaController::AzaMemWr16(UINT08* Address, UINT16 Data) const
{
    #ifdef LWCPU_FAMILY_ARM
        UINT32 byteEnable = 0;
        UINT32 word = Data << (((size_t)Address & 0x2) * 8);

        byteEnable = RF_SET(T30_HDA_DFPCI_BEN_0_EN_DFPCI_BEN);
        byteEnable |= ~(3<<((size_t)Address & 0x2)) & 0xf;

        MEM_WR32(reinterpret_cast<UINT08*>(m_pRegisterBase) + T30_HDA_DFPCI_BEN_0, byteEnable);
        MEM_WR32((size_t)Address & ~0x2, word);

        byteEnable &= ~(RF_SET(T30_HDA_DFPCI_BEN_0_EN_DFPCI_BEN));
        MEM_WR32(reinterpret_cast<UINT08*>(m_pRegisterBase) + T30_HDA_DFPCI_BEN_0, byteEnable);
        return;
    #endif

    MEM_WR16(Address,Data);
    return;
}

RC AzaliaController::RegRd(const UINT32 Offset,
                           UINT32 *pData,
                           const bool IsDebug) const
{
    RC rc = OK;
    CHECK_RC(DeviceRegRd(Offset, pData));
    return rc;
}

RC AzaliaController::RegWr(const UINT32 Offset,
                           const UINT32 Data,
                           const bool IsDebug) const
{
    RC rc = OK;
    CHECK_RC(DeviceRegWr(Offset, Data));
    return rc;
}

RC AzaliaController::DeviceRegRd(UINT32 Offset, UINT32 *pData) const
{
    // Get register code for SD(0), if appropriate. This simplifies
    // the switch statement below.
    UINT32 regCode = Offset;
    if (Offset >= SD_CTLX(0) && Offset < WALCLKA)
    {
        regCode = SD_CTLX(0) + ((regCode - SD_CTLX(0)) % (SD_CTLX(1) - SD_CTLX(0)));
    }
    else if (Offset >= SDLPIBA(0))
    {
        regCode = SDLPIBA(0) + ((regCode - SDLPIBA(0)) % (SDLPIBA(1) - SDLPIBA(0)));
    }

    UINT08 bitOffset = 0;
    switch (regCode)
    {
        // 8 bit registers
        case VMAJ:
        case SD_STS(0):
            if (!bitOffset) bitOffset = 24;
            // Fall through
        case VMIN:
        case CORBSIZE:
        case RIRBSIZE:
        case RINTCNT:
        case SD_CTLY(0):
            if (!bitOffset) bitOffset = 16;
            // Fall through
        case CORBSTS:
        case RIRBSTS:
            if (!bitOffset) bitOffset = 8;
            // Fall through
        case CORBCTL:
        case RIRBCTL:
            *pData = ((UINT32) AzaMemRd08(m_pBar0 + Offset)) << bitOffset;
            break;

        // 16 bit registers
        case INPAY:
        case STATESTS:
        case INSTRMPAY:
        case CORBRP:
        case SD_FMT(0):
            bitOffset = 16;
            // Fall through
        case GCAP:
        case OUTPAY:
        case WAKEEN:
        case GSTS:
        case OUTSTRMPAY:
        case CORBWP:
        case RIRBWP:
        case ICS:
        case SD_CTLX(0):
        case SD_LVI(0):
        case SD_FIFOD(0):
            *pData = ((UINT32) AzaMemRd16(m_pBar0 + Offset)) << bitOffset;
            break;

        // 32 bit registers
        case GCTL:
        case INTCTL:
        case INTSTS:
        case WALCLK:
        case SSYNC:
        case CORBLBASE:
        case CORBUBASE:
        case RIRBLBASE:
        case RIRBUBASE:
        case ICOI:
        case IRII:
        case DPIBLBASE:
        case DPIBUBASE:
        case WALCLKA:
        case SDLPIBA(0):
        case SD_PICB(0):
        case SD_CBL(0):
        case SD_BDPL(0):
        case SD_BDPU(0):
            *pData = MEM_RD32(m_pBar0 + Offset);
            break;

        default:
            Printf(Tee::PriError, "Unknown azalia register, offset = 0x%x\n", Offset);
            *pData = 0xdeaddead;
            return RC::ILWALID_REGISTER_NUMBER;
    }
    return OK;
}

RC AzaliaController::DeviceRegWr(UINT32 Offset, UINT32 data) const
{
    // Get register code for SD(0), if appropriate. This simplifies
    // the switch statement below.
    UINT32 regCode = Offset;
    if (Offset >= SD_CTLX(0) && Offset < AZA_WALCLKA)
    {
        regCode = SD_CTLX(0) + ((regCode - SD_CTLX(0)) % (SD_CTLX(1) - SD_CTLX(0)));
    }
    else if (Offset >= SDLPIBA(0))
    {
        regCode = SDLPIBA(0) + ((regCode - SDLPIBA(0)) % (SDLPIBA(1) - SDLPIBA(0)));
    }

    UINT08 bitOffset = 0;
    switch (regCode)
    {
        // 8 bit registers
        case VMAJ:
        case SD_STS(0):
            if (!bitOffset) bitOffset = 24;
            // Fall through
        case VMIN:
        case CORBSIZE:
        case RIRBSIZE:
        case RINTCNT:
        case SD_CTLY(0):
            if (!bitOffset) bitOffset = 16;
            // Fall through
        case CORBSTS:
        case RIRBSTS:
            if (!bitOffset) bitOffset = 8;
            // Fall through
        case CORBCTL:
        case RIRBCTL:
            if (data & ~(0xff << bitOffset))
            {
                Printf(Tee::PriWarn, "Value to be written is not 8 bits! Truncating...\n");
            }

            AzaMemWr08(m_pBar0 + Offset, (UINT08) (data >> bitOffset));
            break;

        // 16 bit registers
        case INPAY:
        case STATESTS:
        case INSTRMPAY:
        case CORBRP:
        case SD_FMT(0):
            bitOffset = 16;
            // Fall through
        case GCAP:
        case OUTPAY:
        case WAKEEN:
        case GSTS:
        case OUTSTRMPAY:
        case CORBWP:
        case RIRBWP:
        case ICS:
        case SD_CTLX(0):
        case SD_LVI(0):
        case SD_FIFOD(0):
            if (data & ~(0xffff << bitOffset))
            {
                Printf(Tee::PriWarn, "Value to be written is not 16 bits! Truncating...\n");
            }
            AzaMemWr16(m_pBar0 + Offset, (UINT16) (data >> bitOffset));
            break;

        // 32 bit registers
        case GCTL:
        case INTCTL:
        case INTSTS:
        case WALCLK:
        case SSYNC:
        case CORBLBASE:
        case CORBUBASE:
        case RIRBLBASE:
        case RIRBUBASE:
        case ICOI:
        case IRII:
        case DPIBLBASE:
        case DPIBUBASE:
        case WALCLKA:
        case SDLPIBA(0):
        case SD_PICB(0):
        case SD_CBL(0):
        case SD_BDPL(0):
        case SD_BDPU(0):
            // 32 bit registers
            MEM_WR32(m_pBar0 + Offset, data);
            break;
        default:
            Printf(Tee::PriError, "Unknown Azalia register, offset = 0x%x\n", Offset);
            return RC::ILWALID_REGISTER_NUMBER;
    }
    return OK;
}

RC AzaliaController::DmaPosClear(UINT08 StreamNumber)
{
    if (StreamNumber >= MAX_NUM_ENGINES)
    {
        Printf(Tee::PriError, "Invalid stream number: %d\n", StreamNumber);
        return RC::SOFTWARE_ERROR;
    }
    if (!m_pDmaPosBaseAddr)
    {
        Printf(Tee::PriError, "DMA position buffer not initialized!\n");
        return RC::SOFTWARE_ERROR;
    }
    MEM_WR32(((UINT08*) m_pDmaPosBaseAddr) +
             (StreamNumber * DMA_POS_BYTES_PER_STREAM), 0);
    return OK;
}

RC AzaliaController::DmaPosGet(UINT08 StreamNumber, UINT32 *pPos)
{
    if (StreamNumber >= MAX_NUM_ENGINES)
    {
        Printf(Tee::PriError, "Invalid stream number: %d\n", StreamNumber);
        return RC::SOFTWARE_ERROR;
    }
    if (!m_pDmaPosBaseAddr)
    {
        Printf(Tee::PriError, "DMA position buffer not initialized!\n");
        return RC::SOFTWARE_ERROR;
    }
    *pPos = MEM_RD32(((UINT08*) m_pDmaPosBaseAddr) +
                     (StreamNumber * DMA_POS_BYTES_PER_STREAM));
    return OK;
}

RC AzaliaController::DmaPosInitialize()
{
    RC rc;

    for (UINT08 i=0; i<m_vpStreamEngines.size(); i++)
    {
        AzaliaDmaEngine::STATE state;
        CHECK_RC(m_vpStreamEngines[i]->GetEngineState(&state));
        if (state == AzaliaDmaEngine::STATE_RUN)
        {
            Printf(Tee::PriError, "Attempting to enable DmaPos while stream is running!\n");
            return RC::SOFTWARE_ERROR;
        }
    }

    // Allocate DMA position buffer. Must be 128B-aligned
    if (m_pDmaPosBaseAddr)
        CHECK_RC(AzaliaUtilities::AlignedMemFree(m_pDmaPosBaseAddr));

     CHECK_RC(AzaliaUtilities::AlignedMemAlloc(MAX_NUM_ENGINES * DMA_POS_BYTES_PER_STREAM,
                                              0x80, &m_pDmaPosBaseAddr, m_AddrBits,
                                              GetMemoryType(),
                                              m_IsRandomEnabled,
                                              this));

    MASSERT(m_pDmaPosBaseAddr);

    // Program hw with base address
    PHYSADDR physDmaPosBaseAddr;
    if (GetIsUseMappedPhysAddr())
    {
        CHECK_RC(MemoryTracker::GetMappedPhysicalAddress(m_pDmaPosBaseAddr, this, &physDmaPosBaseAddr));
    }
    else
    {
        physDmaPosBaseAddr = Platform::VirtualToPhysical(m_pDmaPosBaseAddr);
    }
    CHECK_RC(RegWr(DPIBLBASE, (UINT32) physDmaPosBaseAddr));
    CHECK_RC(RegWr(DPIBUBASE, (UINT32) (physDmaPosBaseAddr >> 32)));

    // Initialize the buffer. Use non-zero values as that is more
    // likely to expose problems.
    for (UINT32 i=0; i<(MAX_NUM_ENGINES*DMA_POS_BYTES_PER_STREAM); i+=4)
    {
            MEM_WR32(((UINT08*)m_pDmaPosBaseAddr)+i, 0xdeadbeef);
    }

    return OK;
}

RC AzaliaController::DmaPosToggle(bool IsEnable)
{
    RC rc;
    UINT32 value;
    if (!m_pDmaPosBaseAddr)
        CHECK_RC(DmaPosInitialize());

    CHECK_RC(RegRd(DPIBLBASE, &value));
    value = FLD_SET_RF_NUM(AZA_DPLBASE_ENABLE, IsEnable ? 1:0, value);
    CHECK_RC(RegWr(DPIBLBASE, value));
    return OK;
}

RC AzaliaController::DmaPosToggleCoherence(bool IsEnable)
{
    RC rc;
    if (!m_IsIch6Mode)
    {
        UINT32 value = 0;
        CHECK_RC(CfgRd32(AZA_CFG_19, &value));
        value = FLD_SET_RF_NUM(AZA_CFG_19_DMAPOS_COH, (IsEnable?1:0), value);
        CHECK_RC(CfgWr32(AZA_CFG_19, value));
    }
    return OK;
}

RC AzaliaController::GetCodec(UINT08 Sdin, AzaliaCodec **ppCodec)
{
    if (Sdin >= MAX_NUM_SDINS)
    {
        Printf(Tee::PriError, "Invalid codec number requested\n");
        return RC::SOFTWARE_ERROR;
    }

    if (ppCodec == NULL)
    {
        Printf(Tee::PriError, "Invalid parameter passed to function\n");
        return RC::BAD_PARAMETER;
    }

    if (m_pCodecTable[Sdin] == NULL)
    {
        return RC::UNSUPPORTED_HARDWARE_FEATURE;
    }

    *ppCodec = m_pCodecTable[Sdin];
    return OK;
}

RC AzaliaController::GetNumCodecs(UINT08 *pNumCodecs) const
{
    *pNumCodecs = m_NCodecs;
    return OK;
}

RC AzaliaController::GetStream(UINT16 index, AzaStream **ppStream)
{
    if (index >= m_vpStreams.size())
    {
        Printf(Tee::PriError, "Invalid stream number requested\n");
        return RC::SOFTWARE_ERROR;
    }
    if (ppStream == NULL)
    {
        Printf(Tee::PriError, "Invalid parameter passed to function\n");
        return RC::BAD_PARAMETER;
    }

    *ppStream = m_vpStreams[index];
    return OK;
}

RC AzaliaController::EnableStream(UINT32 Index)
{
    if (Index >= m_vpStreams.size())
        return RC::BAD_PARAMETER;

    m_vpStreams[Index]->Reserve(true);

    return OK;
}

RC AzaliaController::DisableStream(UINT32 Index)
{
    if (Index >= m_vpStreams.size())
        return RC::BAD_PARAMETER;

    m_vpStreams[Index]->Reserve(false);

    return OK;
}

RC AzaliaController::ReleaseAllStreams()
{
    for (UINT32 i = 0; i < m_vpStreams.size(); ++i)
        m_vpStreams[i]->Reserve(false);

    return OK;
}

RC AzaliaController::GetNextAvailableStream(AzaliaDmaEngine::DIR Dir,
                                            UINT32 *pStreamIndex) const
{
    RC rc = OK;

    UINT32 i;
    for (i=0; i<m_vpStreams.size(); i++)
    {
        if (!m_vpStreams[i]->IsReserved())
        {
            AzaliaDmaEngine::DIR engineType;
            CHECK_RC(m_vpStreams[i]->GetEngine()->GetType(&engineType));
            if ((Dir == engineType) || (engineType == AzaliaDmaEngine::DIR_BIDIR))
            {
                *pStreamIndex = i;
                m_vpStreams[i]->Reserve(true);
                break;
            }
        }
    }

    if (i == m_vpStreams.size())
    {
        Printf(Tee::PriError, "No suitable Azalia Streams available!\n");
        return RC::ILWALID_INPUT;
    }

    return rc;
}

RC AzaliaController::InitDataStructures()
{
    LOG_ENT();
    RC rc;

    CHECK_RC(CfgRd16(LW_PCI_VENDOR_ID, &m_VendorId));
    CHECK_RC(CfgRd16(LW_PCI_DEVICE_ID, &m_DeviceId));

    m_AddrBits = GetAddressBits(m_DeviceId);

    if (m_VendorId == Pci::VendorIds::Intel)
        m_IsIch6Mode = true;

    UINT32 value = 0;

    struct SysUtil::PciInfo pciInfo;
    if ((m_IsUseMappedPhysAddr == true)  && (GetPciInfo(&pciInfo) == OK))
    {
        if(Xp::CanEnableIova(pciInfo.DomainNumber, pciInfo.BusNumber, pciInfo.DeviceNumber, pciInfo.FunctionNumber))
        {
            CHECK_RC(Xp::SetDmaMask(pciInfo.DomainNumber, pciInfo.BusNumber, pciInfo.DeviceNumber, pciInfo.FunctionNumber, m_AddrBits));
        }
        else
        {
            m_IsUseMappedPhysAddr = false;
        }
    }
    else
    {
        m_IsUseMappedPhysAddr = false;
    }

    // Enumerate codecs
    // We'll use the STATESTS register to detect codecs, but first we
    // must wait at least 4 frames (120us) to make sure that state change
    // requests get through.
    AzaliaUtilities::Pause(1);
    bool codecFound = false;
    CHECK_RC(RegRd(STATESTS, &value));
    value = value >> 16;

    m_NCodecs = 0;
    for (UINT32 i=0; i<MAX_NUM_SDINS; i++)
    {
        MASSERT(m_pCodecTable[i] == NULL);
        if (value & 0x1)
        {
            Printf(Tee::PriLow, "Found codec on sdin %d\n", i);
            m_pCodecTable[i] = new AzaliaCodec(this, i);
            m_NCodecs++;
            codecFound = true;
        }
        else
        {
            m_pCodecTable[i] = NULL;
        }
        m_SolResponses[i].clear();
        m_UnsolResponses[i].clear();
        value = value >> 1;
    }
    if (!codecFound)
    {
        Printf(Tee::PriNormal, "  No codecs found!\n");
    }

    // Get the number of streams supported and create the stream and
    // DMA engine data objects
    MASSERT(!m_vpStreamEngines.size());

    CHECK_RC(RegRd(GCAP, &value));
    m_NInputStreamsSupported = RF_VAL(AZA_GCAP_ISS, value);
    m_NOutputStreamsSupported = RF_VAL(AZA_GCAP_OSS, value);
    m_NBidirStreamsSupported = RF_VAL(AZA_GCAP_BSS, value);

    AzaliaDmaEngine* pEngine;
    AzaStream* pStream;
    UINT32 swStrmNum = 0;

    for (UINT08 i=0; i<m_NInputStreamsSupported; i++, swStrmNum++)
    {
        pEngine = new AzaliaDmaEngine(this, AzaliaDmaEngine::DIR_INPUT,
                                      m_vpStreamEngines.size());
        m_vpStreamEngines.push_back(pEngine);
        pStream = new AzaStream(this, pEngine, AzaliaDmaEngine::DIR_INPUT, swStrmNum);
        m_vpStreams.push_back(pStream);
    }

    for (UINT08 i=0; i<m_NOutputStreamsSupported; i++, swStrmNum++)
    {
        pEngine = new AzaliaDmaEngine(this, AzaliaDmaEngine::DIR_OUTPUT,
                                      m_vpStreamEngines.size());
        m_vpStreamEngines.push_back(pEngine);
        pStream = new AzaStream(this, pEngine, AzaliaDmaEngine::DIR_OUTPUT, swStrmNum);
        m_vpStreams.push_back(pStream);
    }
    for (UINT08 i=0; i<m_NBidirStreamsSupported; i++)
    {
        pEngine = new AzaliaDmaEngine(this, AzaliaDmaEngine::DIR_BIDIR,
                                      m_vpStreamEngines.size());
        m_vpStreamEngines.push_back(pEngine);
        pStream = new AzaStream(this, pEngine, AzaliaDmaEngine::DIR_BIDIR, swStrmNum);
        m_vpStreams.push_back(pStream);
    }

    if (m_vpStreamEngines.size() > MAX_NUM_ENGINES)
    {
        Printf(Tee::PriError, "New hardware has more DMA engines than expected!\n");
        return RC::SOFTWARE_ERROR;
    }

    CHECK_RC(CorbInitialize(m_CorbSize));
    CHECK_RC(RirbInitialize(m_RirbSize));
    CHECK_RC(DmaPosInitialize());

    CHECK_RC(IntControllerClearStatus());
    for (UINT08 i=0; i<MAX_NUM_ENGINES; i++)
        CHECK_RC(IntStreamClearStatus(i));

    // Check for spurious interrupts
    IntControllerStatus sts;
    CHECK_RC(IntControllerGetStatus(&sts));
    CHECK_RC(IntControllerClearStatus());
    if (sts.corbMemErr)
    {
        Printf(Tee::PriError, "CORB error during reset\n");
        return RC::CMD_INTERRUPT_ERROR;
    }
    if (sts.rirbOverrun)
    {
        Printf(Tee::PriError, "RIRB error during reset\n");
        return RC::CMD_INTERRUPT_ERROR;
    }
    if (sts.rirbIntFlag)
    {
        Printf(Tee::PriError, "RIRBINT error during reset\n");
        return RC::CMD_INTERRUPT_ERROR;
    }

    LOG_EXT();
    return OK;
}

RC AzaliaController::IsSupport64bit(bool *pIsSupport64)
{
    LOG_ENT();
    RC rc;
    UINT32 hccparams;

    MASSERT(pIsSupport64);

    CHECK_RC(RegRd(AZA_GCAP, &hccparams));

    if (FLD_TEST_RF_DEF(AZA_GCAP_64BIT_OK, _YES, hccparams))
        *pIsSupport64 = true;
    else
        *pIsSupport64 = false;

    LOG_EXT();
    return OK;
}

RC AzaliaController::InitRegisters()
{
    LOG_ENT();
    RC rc = OK;

    #ifdef LWCPU_FAMILY_ARM
        // This is specific to T30 to get the right FIFO size
        UINT32 reg = 0;
        CHECK_RC(CfgRd32(AZA_OBCYA_STR_FIFO_MONITOR_EN, &reg));
        reg |= RF_SET(AZA_OBCYA_STR_FIFO_MONITOR_EN_STR5);
        CHECK_RC(CfgWr32(AZA_OBCYA_STR_FIFO_MONITOR_EN, reg));
    #endif

    // Enable bus-mastering in Azalia
    CHECK_RC(CfgWr08(AZA_CFG_1, 0x07));

    CHECK_RC(IntToggle(INT_WAKE, false, 0x7fff));
    CHECK_RC(Reset(true));

    // While in reset, clear STATESTS so that we can properly detect codecs.
    // STATESTS is one of the few registers that can be written in reset.
    CHECK_RC(RegWr(STATESTS, 0xffff0000));
    CHECK_RC(Reset(false));

    LOG_EXT();
    return OK;
}

RC AzaliaController::IntClear(AzaIntType Type, UINT32 Index)
{
    RC rc;

    switch (Type)
    {
        case INT_CORB_MEM_ERR:
            CHECK_RC(RegWr(CORBSTS, RF_DEF(AZA_CORBCTL_CMEI, _W1C)));
            break;
        case INT_RIRB_OVERRUN:
            CHECK_RC(RegWr(RIRBSTS, RF_DEF(AZA_RIRBCTL_RIRBOIS, _W1C)));
            break;
        case INT_RIRB:
            CHECK_RC(RegWr(RIRBSTS, RF_DEF(AZA_RIRBCTL_RINTFL, _W1C)));
            break;
        case INT_WAKE:
            MASSERT(Index < 8);
            CHECK_RC(RegWr(STATESTS, 1 << (Index + 16)));
            break;
        case INT_STREAM_DESC_ERR:
            MASSERT(Index < MAX_NUM_ENGINES);
            CHECK_RC(RegWr(SD_STS(Index), RF_DEF(AZA_SDCTL_DESE, _W1C)));
            break;
        case INT_STREAM_FIFO_ERR:
            MASSERT(Index < MAX_NUM_ENGINES);
            CHECK_RC(RegWr(SD_STS(Index), RF_DEF(AZA_SDCTL_FIFOE, _W1C)));
            break;
        case INT_STREAM_IOC:
            MASSERT(Index < MAX_NUM_ENGINES);
            CHECK_RC(RegWr(SD_STS(Index), RF_DEF(AZA_SDCTL_BCIS, _W1C)));
            break;
        case INT_CONTROLLER:
        case INT_STREAM:
        case INT_GLOBAL:
        case INT_PCI:
            Printf(Tee::PriError, "Cannot clear interrupt of type %d, it is RO!\n", Type);
            MASSERT(!"IntClear RO Interrupt Type");
            return RC::SOFTWARE_ERROR;
            break;
        default:
            Printf(Tee::PriError, "Unknown interrupt type: %d\n", Type);
            MASSERT(!"IntClear unknown type");
            return RC::SOFTWARE_ERROR;
    }

    return OK;
}

RC AzaliaController::IntControllerClearStatus()
{
    m_IntStatus.Clear();
    return OK;
}

RC AzaliaController::IntControllerGetStatus(IntControllerStatus *pStatus)
{
    RC rc;
    if (m_IntrMode == POLL || !m_IntStatus.rirbIntFlag)
    {
        CHECK_RC(IntControllerUpdateStatus());
    }
    pStatus->Clear();
    pStatus->Merge(m_IntStatus);
    return OK;
}

RC AzaliaController::IntControllerUpdateStatus()
{
    RC rc;

    // Get new status from hw
    IntControllerStatus newStatus;
    newStatus.Clear();

    // Read the master interrupt status register
    UINT32 intSts = 0;
    CHECK_RC(RegRd(INTSTS, &intSts));
    bool gis   = RF_VAL(AZA_INTSTS_GIS, intSts)!=0;
    bool cis   = RF_VAL(AZA_INTSTS_CIS, intSts)!=0;
    UINT32 sis = RF_VAL((MAX_NUM_STREAMS_RARELY_USED-1):0, intSts);

    // Read the PCI status register
    UINT16 pciSts = 0;
    CHECK_RC(CfgRd16(LW_PCI_CONTROL_STATUS, &pciSts));
    bool pci_is = pciSts&0x0008;

    // Get CORB status and clear hw
    UINT32 corbsts = 0;
    CHECK_RC(RegRd(CORBSTS, &corbsts));
    newStatus.corbMemErr = FLD_TEST_RF_DEF(AZA_CORBCTL_CMEI, _ERROR, corbsts);
    if (newStatus.corbMemErr)
    {
        corbsts = FLD_SET_RF_DEF(AZA_CORBCTL_CMEI, _W1C, corbsts);
        CHECK_RC(RegWr(CORBSTS, corbsts));
    }
    // Get RIRB status and clear hw
    UINT32 rirbsts = 0;

    CHECK_RC(RegRd(RIRBSTS, &rirbsts));
    newStatus.rirbOverrun = FLD_TEST_RF_DEF(AZA_RIRBCTL_RIRBOIS, _OVERRUN, rirbsts);
    newStatus.rirbIntFlag = FLD_TEST_RF_DEF(AZA_RIRBCTL_RINTFL, _RESPONSE, rirbsts);

    // RF_DEF could be used instead of FLD_SET_RF_DEF
    if (newStatus.rirbOverrun || newStatus.rirbIntFlag)
    {
        rirbsts = (newStatus.rirbOverrun
                        ? FLD_SET_RF_DEF(AZA_RIRBCTL_RIRBOIS, _W1C, rirbsts): 0)
                       | (newStatus.rirbIntFlag
                          ? FLD_SET_RF_DEF(AZA_RIRBCTL_RINTFL, _W1C, rirbsts): 0);

        CHECK_RC(RegWr(RIRBSTS, rirbsts));
    }
    // Get STATESTS and clear hw
    CHECK_RC(RegRd(STATESTS, &newStatus.statests));
    if (newStatus.statests)
    {
        CHECK_RC(RegWr(STATESTS, newStatus.statests));
    }

    // Check the GIS bit
    if (((cis && m_IntEnables.ci) || (sis & m_IntEnables.si)) && !gis)
    {
        Printf(Tee::PriError, "GIS bit should have been set!\n");
        return RC::CMD_INTERRUPT_ERROR;
    }
    else if (!(cis && m_IntEnables.ci) && !(sis & m_IntEnables.si) && gis)
    {
        Printf(Tee::PriError, "GIS bit is improperly set!\n");
        return RC::CMD_INTERRUPT_ERROR;
    }

    // Check the PCI IS bit
    if ((gis && m_IntEnables.gi && !pci_is && !IsGpuAza()))
    {
        // For MCP89 Dont check this since the PCI interrupt status bit is not
        // expected to be set in MSI mode
        /*if ((m_IntrMode == BusMasterDevice::INTR_MSI) &&
            (m_ChipVersion != Chipset::MCP89))
        {
            Printf(Tee::PriError, "PCI IS bit should have been set!\n");
            return RC::CMD_INTERRUPT_ERROR;
        }*/
    }
    else if (gis && !m_IntEnables.gi && pci_is)
    {
        Printf(Tee::PriError, "PCI IS bit is improperly set!\n");
        return RC::CMD_INTERRUPT_ERROR;
    }

    // Check the CIS bit
    if (cis && !((newStatus.corbMemErr  && m_IntEnables.cmei)
                 || (newStatus.rirbOverrun && m_IntEnables.rirboic)
                 || (newStatus.rirbIntFlag && m_IntEnables.rirbctl)
                 || (newStatus.statests & m_IntEnables.wakeen)))
    {
        Printf(Tee::PriError, "CIS should not have been set!\n");
        return RC::CMD_INTERRUPT_ERROR;
    }
    else if (!cis && ((newStatus.corbMemErr  && m_IntEnables.cmei)
                      || (newStatus.rirbOverrun && m_IntEnables.rirboic)
                      || (newStatus.rirbIntFlag && m_IntEnables.rirbctl)
                      || (newStatus.statests & m_IntEnables.wakeen)))
    {
        // Reread the CIS bit
        CHECK_RC(RegRd(INTSTS, &intSts));
        cis = RF_VAL(AZA_INTSTS_CIS,intSts)!=0;

        if (!cis)
        {
            Printf(Tee::PriError, "CIS should be set!\n");
            return RC::CMD_INTERRUPT_ERROR;
        }
    }

    m_IntStatus.Merge(newStatus);

    return OK;
}

RC AzaliaController::IntQuery(AzaIntType Type, UINT32 Index, bool* pResult)
{
    RC rc;
    UINT32 value = 0;
    bool pciis = false;
    bool gis = false;
    bool cis = false;
    bool sis = false;
    UINT32 deis = 0;
    UINT32 feis = 0;
    UINT32 iocs = 0;
    bool cmeis = false;
    bool rirboics = false;
    bool rirbctls = false;
    UINT32 wakeens = 0;
    bool shouldBeSet = false;

    *pResult = false;

    // Check PCI interrupt status
    // This is the same as reading the LW_PCI_CONTROL_STATUS
    // and checking bit 3
    CHECK_RC(CfgRd32(AZA_CFG_1, &value));
    pciis = RF_VAL(AZA_CFG_1_INTR_STS, value);

    // Get high-level interrupt status
    CHECK_RC(RegRd(INTSTS, &value));
    gis = FLD_TEST_RF_DEF(AZA_INTSTS_GIS, _INTR, value);
    cis = FLD_TEST_RF_DEF(AZA_INTSTS_CIS, _INTR, value);
    sis = RF_VAL((MAX_NUM_STREAMS_RARELY_USED-1):0, value);
    MASSERT(!RF_VAL(RF_SHIFT(AZA_INTSTS_CIS):MAX_NUM_STREAMS_RARELY_USED, value));

    // Check that PCI IS bit is correct
    shouldBeSet = (gis && m_IntEnables.gi);
    if (pciis != shouldBeSet)
    {
        Printf(Tee::PriError, "PCI interrupt status (%d) incorrect!\n", pciis);
        Printf(Tee::PriError, "gis = %d, gie = %d\n", gis, m_IntEnables.gi);
        return RC::CMD_INTERRUPT_ERROR;
    }

    if (Type < INT_PCI)
    {
        // Check that GIS IS bit is correct
        // Note that sis and m_IntEnables.si are both bit fields,
        // so & is the correct operator to find bits with both status and
        // enable bits set.
        shouldBeSet = (cis && m_IntEnables.ci) || (sis & m_IntEnables.si);
        if (gis != shouldBeSet)
        {
            Printf(Tee::PriError, "GIS bit (%d) incorrect!\n", gis);
            Printf(Tee::PriError, "cis & e %d & %d; sis & e %d &%d\n",
                   cis, m_IntEnables.ci, sis, m_IntEnables.si);
            return RC::CMD_INTERRUPT_ERROR;
        }
        if (Type == INT_STREAM_DESC_ERR ||
            Type == INT_STREAM_FIFO_ERR ||
            Type == INT_STREAM_IOC ||
            Type == INT_STREAM)
        {
            // Stream interrupt
            MASSERT(Index < MAX_NUM_ENGINES);
            for (UINT32 i=0; i < m_vpStreamEngines.size(); i++)
            {
                CHECK_RC(RegRd(SD_STS(i), &value));
                deis |= RF_VAL(AZA_SDCTL_DESE, value) << i;
                feis |= RF_VAL(AZA_SDCTL_FIFOE, value) << i;
                iocs |= RF_VAL(AZA_SDCTL_BCIS, value) << i;
            }

            // Check that SIS IS is set correctly
            shouldBeSet = (deis & m_IntEnables.dei) ||
                          (feis & m_IntEnables.fei) ||
                          (iocs & m_IntEnables.ioc);
            if (sis != shouldBeSet)
            {
                Printf(Tee::PriError, "SIS bit (%d) incorrect!\n", sis);
                Printf(Tee::PriError, "(0x%x & 0x%x) (0x%x & 0x%x) (0x%x & 0x%x)\n",
                       deis, m_IntEnables.dei,
                       feis, m_IntEnables.fei,
                       iocs, m_IntEnables.ioc);

                return RC::CMD_INTERRUPT_ERROR;
            }
        }
        else if (Type == INT_CORB_MEM_ERR ||
                 Type == INT_RIRB_OVERRUN ||
                 Type == INT_RIRB ||
                 Type == INT_WAKE ||
                 Type == INT_CONTROLLER)
        {
            // Controller interrupt
            CHECK_RC(RegRd(CORBSTS, &value));
            cmeis = FLD_TEST_RF_DEF(AZA_CORBCTL_CMEI, _ERROR, value);
            CHECK_RC(RegRd(RIRBSTS, &value));
            rirboics = FLD_TEST_RF_DEF(AZA_RIRBCTL_RIRBOIS, _OVERRUN, value);
            rirbctls = FLD_TEST_RF_DEF(AZA_RIRBCTL_RINTFL, _RESPONSE, value);
            CHECK_RC(RegRd(STATESTS, &wakeens));
            wakeens = wakeens >> 16;
            MASSERT(Index < MAX_NUM_SDINS);

            // Check that the CIS bit is set correctly
            shouldBeSet = (cmeis && m_IntEnables.cmei) ||
                          (rirboics && m_IntEnables.rirboic) ||
                          (rirbctls && m_IntEnables.rirbctl) ||
                          (wakeens & m_IntEnables.wakeen);
            if (cis != shouldBeSet)
            {
                Printf(Tee::PriError, "CIS bit (%d) incorrect!\n", cis);
                Printf(Tee::PriError, "(%d && %d) (%d && %d) (%d && %d) (0x%x & 0x%x)\n",
                       cmeis, m_IntEnables.cmei,
                       rirboics, m_IntEnables.rirboic,
                       rirbctls, m_IntEnables.rirbctl,
                       wakeens, m_IntEnables.wakeen);

                return RC::CMD_INTERRUPT_ERROR;
            }
        }
    }

    switch (Type)
    {
        case INT_CORB_MEM_ERR:    *pResult = cmeis;                   break;
        case INT_RIRB_OVERRUN:    *pResult = rirboics;                break;
        case INT_RIRB:            *pResult = rirbctls;                break;
        case INT_WAKE:            *pResult = (wakeens >> Index) &0x1; break;
        case INT_STREAM_DESC_ERR: *pResult = (deis >> Index) & 0x1;   break;
        case INT_STREAM_FIFO_ERR: *pResult = (feis >> Index) & 0x1;   break;
        case INT_STREAM_IOC:      *pResult = (iocs >> Index) & 0x1;   break;
        case INT_CONTROLLER:      *pResult = cis;                     break;
        case INT_STREAM:          *pResult = sis;                     break;
        case INT_GLOBAL:          *pResult = gis;                     break;
        case INT_PCI:             *pResult = pciis;                   break;
        default:
            Printf(Tee::PriError, "Unknown interrupt type: %d\n", Type);
            MASSERT(!"IntClear unknown type");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC AzaliaController::IntStreamClearStatus(UINT32 StreamNumber)
{
    MASSERT(StreamNumber < MAX_NUM_ENGINES);
    m_IntStreamStatus[StreamNumber].Clear();
    return OK;
}

RC AzaliaController::IntStreamGetStatus(UINT32 StreamNumber, IntStreamStatus *pStatus)
{
    MASSERT(StreamNumber < MAX_NUM_ENGINES);
    RC rc;

    if (m_IntrMode == POLL)
    {
        CHECK_RC(IntStreamUpdateStatus(StreamNumber));
    }

    pStatus->Clear();
    pStatus->Merge(m_IntStreamStatus[StreamNumber]);
    return OK;
}

RC AzaliaController::IntStreamUpdateStatus(UINT32 StreamNumber)
{
    MASSERT(StreamNumber < MAX_NUM_ENGINES);
    if (StreamNumber >= MAX_NUM_ENGINES)
    {
        Printf(Tee::PriError, "StreamNumber=%d >= MAX_NUM_ENGINES\n", StreamNumber);
        return RC::SOFTWARE_ERROR;
    }
    RC rc;
    IntStreamStatus newStatus;
    newStatus.Clear();
    // Read the master interrupt status register. Note that CIS and SIS
    // should be RO, not RW1C 1.0 spec states. Microsoft and Intel
    // changed this.
    UINT32 intsts = 0;
    CHECK_RC(RegRd(INTSTS, &intsts));

    // Read the stream status register
    UINT32 strmsts = 0;
    CHECK_RC(RegRd(SD_STS(StreamNumber), &strmsts));
    newStatus.dese = FLD_TEST_RF_DEF(AZA_SDCTL_DESE, _ERROR, strmsts);
    newStatus.fifoe = FLD_TEST_RF_DEF(AZA_SDCTL_FIFOE, _ERROR, strmsts);
    newStatus.bcis = FLD_TEST_RF_DEF(AZA_SDCTL_BCIS, _COMPLETE, strmsts);

    // Clear the stream status register
    if (newStatus.dese || newStatus.fifoe || newStatus.bcis)
    {
        CHECK_RC(RegWr(SD_STS(StreamNumber),
                       RF_DEF(AZA_SDCTL_DESE, _W1C)
                       | RF_DEF(AZA_SDCTL_FIFOE, _W1C)
                       | RF_DEF(AZA_SDCTL_BCIS, _W1C)));

        // For the fifo error test we need to enable bus-mastering to prevent
        // the system from hanging after an interrup is triggered
        // Enable bus-mastering here if we see the fifo error flag set
        if (newStatus.fifoe)
        {
            // Pause a little while to make sure the error oclwrs on all streams
            // Enabling bus-mastering too quickly can cause input streams to not
            // see the error
            AzaliaUtilities::Pause(100);

            // Enable Bus-mastering
            Printf(Tee::PriLow, "Enabling Bus Mastering in StreamUpdateStatus "
                                "for Fifo Error Test\n");
            CHECK_RC(CfgWr08(AZA_CFG_1, 0x07));
        }
    }

    m_IntStreamStatus[StreamNumber].Merge(newStatus);
    return OK;
}

RC AzaliaController::IntToggle(AzaIntType Type, bool IsEnable, UINT32 Mask)
{
    RC rc;
    UINT32 value = 0;
    UINT16 value16 = 0;
    UINT32 reg = 0;
    UINT32 drfBase = 0;
    UINT32 drfExtent = 0;
    UINT32 enableValue = 0;
    UINT32 disableValue = 0;
    bool isSingle = false;
    bool isMultiple = false;
    void* pEnableCache = NULL;

    switch (Type)
    {
        case INT_CORB_MEM_ERR:
            reg = CORBCTL;
            drfBase = RF_BASE(AZA_CORBCTL_CMEIE);
            drfExtent = RF_EXTENT(AZA_CORBCTL_CMEIE);
            enableValue = AZA_CORBCTL_CMEIE_ENABLE;
            disableValue = AZA_CORBCTL_CMEIE_DISABLE;
            isSingle = true;
            pEnableCache = &m_IntEnables.cmei;
            break;
        case INT_RIRB_OVERRUN:
            reg = RIRBCTL;
            drfBase = RF_BASE(AZA_RIRBCTL_RIRBOIC);
            drfExtent = RF_EXTENT(AZA_RIRBCTL_RIRBOIC);
            enableValue = AZA_RIRBCTL_RIRBOIC_INTR;
            disableValue = AZA_RIRBCTL_RIRBOIC_NOINTR;
            isSingle = true;
            pEnableCache = &m_IntEnables.rirboic;
            break;
        case INT_RIRB:
            reg = RIRBCTL;
            drfBase = RF_BASE(AZA_RIRBCTL_RINTCTL);
            drfExtent = RF_EXTENT(AZA_RIRBCTL_RINTCTL);
            enableValue = AZA_RIRBCTL_RINTCTL_ENABLE;
            disableValue = AZA_RIRBCTL_RINTCTL_DISABLE;
            isSingle = true;
            pEnableCache = &m_IntEnables.rirbctl;
            break;
        case INT_WAKE:
            CHECK_RC(RegRd(WAKEEN, &value));
            if (IsEnable)
            {
                value |= (Mask & 0x7fff);
            } else {
                value &= ~(Mask & 0x7fff);
            }
            CHECK_RC(RegWr(WAKEEN, value));
            m_IntEnables.wakeen = value;
            break;
        case INT_STREAM_DESC_ERR:
            drfBase = RF_BASE(AZA_SDCTL_DEIE);
            drfExtent = RF_EXTENT(AZA_SDCTL_DEIE);
            enableValue = AZA_SDCTL_DEIE_ENABLE;
            disableValue = AZA_SDCTL_DEIE_DISABLE;
            isMultiple = true;
            pEnableCache = &m_IntEnables.dei;
            break;
        case INT_STREAM_FIFO_ERR:
            drfBase = RF_BASE(AZA_SDCTL_FEIE);
            drfExtent = RF_EXTENT(AZA_SDCTL_FEIE);
            enableValue = AZA_SDCTL_FEIE_ENABLE;
            disableValue = AZA_SDCTL_FEIE_DISABLE;
            isMultiple = true;
            pEnableCache = &m_IntEnables.fei;
            break;
        case INT_STREAM_IOC:
            drfBase = RF_BASE(AZA_SDCTL_IOCE);
            drfExtent = RF_EXTENT(AZA_SDCTL_IOCE);
            enableValue = AZA_SDCTL_IOCE_ENABLE;
            disableValue = AZA_SDCTL_IOCE_DISABLE;
            isMultiple = true;
            pEnableCache = &m_IntEnables.ioc;
            break;
        case INT_CONTROLLER:
            reg = INTCTL;
            drfBase = RF_BASE(AZA_INTCTL_CIE);
            drfExtent = RF_EXTENT(AZA_INTCTL_CIE);
            enableValue = AZA_INTCTL_CIE_ENABLE;
            disableValue = AZA_INTCTL_CIE_DISABLE;
            isSingle = true;
            pEnableCache = &m_IntEnables.ci;
            break;
        case INT_STREAM:
            CHECK_RC(RegRd(INTCTL, &value));
            if (IsEnable)
            {
                value |= (Mask & 0x3fffffff);
            } else {
                value &= ~(Mask & 0x3fffffff);
            }
            CHECK_RC(RegWr(INTCTL, value));
            m_IntEnables.si = value;
            break;
        case INT_GLOBAL:
            reg = INTCTL;
            drfBase = RF_BASE(AZA_INTCTL_GIE);
            drfExtent = RF_EXTENT(AZA_INTCTL_GIE);
            enableValue = AZA_INTCTL_GIE_ENABLE;
            disableValue = AZA_INTCTL_GIE_DISABLE;
            isSingle = true;
            pEnableCache = &m_IntEnables.gi;
            break;
        case INT_PCI:
            CHECK_RC(CfgRd16(LW_PCI_COMMAND, &value16));
            if (IsEnable)
            {
                value16 &= ~(0x0400); // a disable bit
            } else {
                value16 |= 0x0400;
            }
            MASSERT(!(value & 0xffff0000));
            CHECK_RC(CfgWr16(LW_PCI_COMMAND, value));
            m_IntEnables.pcii = IsEnable;
            break;
        default:
            MASSERT(!"Unknown interrupt type");
            return RC::SOFTWARE_ERROR;
    }

    MASSERT(!(isSingle && isMultiple));
    if (isSingle)
    {
        CHECK_RC(RegRd(reg, &value));
        value &= ~(RF_SHIFTMASK(drfExtent : drfBase));
        if (IsEnable)
        {
            value |= RF_NUM(drfExtent : drfBase, enableValue);
        } else {
            value |= RF_NUM(drfExtent : drfBase, disableValue);
        }
        CHECK_RC(RegWr(reg, value));
        *((bool*) pEnableCache) = IsEnable;
    }
    if (isMultiple)
    {
        UINT32 tempMask = 0x1;
        for (UINT32 i=0; i<m_vpStreamEngines.size(); i++)
        {
            if (Mask & tempMask)
            {
                CHECK_RC(RegRd(AZA_SDCTL(i), &value));
                value &= ~(RF_SHIFTMASK(drfExtent : drfBase));
                if (IsEnable)
                {
                    value |= RF_NUM(drfExtent : drfBase, enableValue);
                } else {
                    value |= RF_NUM(drfExtent : drfBase, disableValue);
                }
                CHECK_RC(RegWr(AZA_SDCTL(i), value));
            }
            tempMask = tempMask << 1;
        }
        if (IsEnable)
        {
            *((UINT32*) pEnableCache) = *((UINT32*) pEnableCache) | Mask;
        } else {
            *((UINT32*) pEnableCache) = *((UINT32*) pEnableCache) & ~Mask;
        }
    }
    return OK;
}

RC AzaliaController::IsInReset(bool* pIsInReset)
{
    RC rc;
    UINT32 value;
    CHECK_RC(RegRd(GCTL, &value));
    *pIsInReset = FLD_TEST_RF_DEF(AZA_GCTL_CRST, _ASSERT, value);
    return OK;
}

long AzaliaController::Isr()
{
    RC rc;
    LOG_ENT();

    UINT32 intSts = 0;
    CHECK_RC(RegRd(INTSTS, &intSts));

    m_IsrCnt[0]++;

    if(intSts == 0)
    {
        // This interrupt was not generated by Azalia.  Let someone else handle it.
        return Xp::ISR_RESULT_NOT_SERVICED;
    }

    m_IsrCnt[1]++;

    UINT32 rirbWp, rirbSts;
    UINT16 cmdSts = 0;
    CHECK_RC(RegRd(RIRBWP, &rirbWp));
    CHECK_RC(RegRd(RIRBSTS, &rirbSts));
    m_rirbWp = rirbWp;
    m_rirbSts = rirbSts;

    CHECK_RC(CfgRd16(LW_PCI_COMMAND, &cmdSts));

    if((intSts>>30) == 0x3)
    {
        // This is required for the RIRB Overrun test when running it in
        // interrupt mode.
        if (FLD_TEST_RF_DEF(AZA_RIRBCTL_RIRBOIS, _OVERRUN, rirbSts))
        {
            Printf(Tee::PriDebug, "Enabling bus-mastering in ISR\n");
            CHECK_RC(CfgWr08(AZA_CFG_1, 0x07));
        }

        // controller interrupt
        CHECK_RC(IntControllerUpdateStatus());
    }
    if((intSts<<2) > 0)
    {
        // Stream Interrupt
        INT32 mask = 0x1;
        for(UINT32 i = 0; i < (MAX_NUM_OSTREAMS + MAX_NUM_ISTREAMS) ; ++i)
        {
            if((mask & intSts) != 0)
            {
                CHECK_RC(IntStreamUpdateStatus(i));
            }
            mask = mask << 1;
        }
    }

    CHECK_RC(RegWr(INTSTS, intSts));

    return Xp::ISR_RESULT_SERVICED;
}

RC AzaliaController::PrintReg(Tee::Priority Pri) const
{
    RC rc = OK;
    UINT32 value = 0;

    Printf(Pri, "Global Registers\n");
    Printf(Pri, "----------------\n");
    PRETTY_PRINT_REG(Pri, "AZA_GCAP     ", AZA_GCAP, value);
    PRETTY_PRINT_REG(Pri, "AZA_GCTL     ", AZA_GCTL, value);
    PRETTY_PRINT_REG(Pri, "AZA_WAKEEN   ", AZA_WAKEEN, value);
    PRETTY_PRINT_REG(Pri, "AZA_GSTS     ", AZA_GSTS, value);
    PRETTY_PRINT_REG(Pri, "AZA_INTCTL   ", AZA_INTCTL, value);
    PRETTY_PRINT_REG(Pri, "AZA_INTSTS   ", AZA_INTSTS, value);
    PRETTY_PRINT_REG(Pri, "AZA_WALCLK   ", AZA_WALCLK, value);
    PRETTY_PRINT_REG(Pri, "AZA_WALCLKA  ", AZA_WALCLKA, value);
    PRETTY_PRINT_REG(Pri, "AZA_ICOI     ", AZA_ICOI, value);
    PRETTY_PRINT_REG(Pri, "AZA_SSYNC    ", AZA_SSYNC, value);

    Printf(Pri, "\nCORB Registers\n");
    Printf(Pri, "--------------\n");
    PRETTY_PRINT_REG(Pri, "AZA_CORBLBASE ", AZA_CORBLBASE, value);
    PRETTY_PRINT_REG(Pri, "AZA_CORBUBASE ", AZA_CORBUBASE, value);
    PRETTY_PRINT_REG(Pri, "AZA_CORBWP    ", AZA_CORBWP, value);
    PRETTY_PRINT_REG(Pri, "AZA_CORBCTL   ", AZA_CORBCTL, value);

    Printf(Pri, "\nRIRB Registers\n");
    Printf(Pri, "--------------\n");
    PRETTY_PRINT_REG(Pri, "AZA_RIRBLBASE ", AZA_RIRBLBASE, value);
    PRETTY_PRINT_REG(Pri, "AZA_RIRBUBASE ", AZA_RIRBUBASE, value);
    PRETTY_PRINT_REG(Pri, "AZA_RIRBWP    ", AZA_RIRBWP, value);
    PRETTY_PRINT_REG(Pri, "AZA_RIRBCTL   ", AZA_RIRBCTL, value);

    Printf(Pri, "\nStream Registers\n");
    Printf(Pri, "----------------\n");
    UINT32 maxNumStreams = 0;
    UINT32 value2 = 0;
    CHECK_RC(GetMaxNumStreams(&maxNumStreams));
    for (UINT32 i = 0; i < maxNumStreams; ++i)
    {
        Printf(Pri, "Stream %d : ", i);
        CHECK_RC(RegRd(AZA_SDCTL(i), &value));
        CHECK_RC(RegRd(AZA_SDFMT(i), &value2));
        Printf(Pri,"SDCTL: 0x%08x\tSDFMT: 0x%08x\n", value, value2);
    }

    return rc;
}

RC AzaliaController::PrintState(Tee::Priority Pri) const
{
    RC rc;
    UINT32 value = 0;

    Printf(Pri, "Azalia controller state:\n");
    Printf(Pri, "------------------------\n");
    Printf(Pri, "ISR Counter RAW: %d, REAL: %d\n", m_IsrCnt[0], m_IsrCnt[1]);
    PRETTY_PRINT_REG(Pri, "GCTL ", GCTL, value);
    PRETTY_PRINT_REG(Pri, "GSTS ", GSTS, value);
    CHECK_RC(PrintCorbState(Pri));
    CHECK_RC(PrintRirbState(Pri));

    return OK;
}

RC AzaliaController::PrintExtDevInfo(Tee::Priority Priority) const
{
    RC rc;
    Printf(Tee::PriNormal, "\nCodec Information\n");
    Printf(Tee::PriNormal, "-----------------\n");
    CHECK_RC(MapPins());
    return OK;
}

RC AzaliaController::DumpRirb(Tee::Priority Pri, UINT16 FirstEntry, UINT16 LastEntry)
{
    MASSERT(FirstEntry <= LastEntry);

    if (LastEntry >= m_RirbSize)
    {
        Printf(Tee::PriWarn, "Index out of bounds. Resetting\n");
        LastEntry = m_RirbSize - 1;
    }
    RC rc;
    UINT32 wp, rp;
    UINT64 response = 0;
    CHECK_RC(RegRd(RIRBWP, &wp));
    wp = RF_VAL(AZA_RIRBWP_RIRBWP, wp);
    rp = m_RirbReadPtr;
    for (UINT16 i=FirstEntry; i<=LastEntry; i++)
    {
        const char* pointer = "";
        if (i == rp)
        {
            if (i == wp)
            {
                pointer = "<--- RP/WP";
            } else {
                pointer = "<--- RP";
            }
        }
        else if (i == wp)
        {
            pointer = "<--- WP";
        }
        response = MEM_RD64((m_pRirbBaseAddr + i));
        Printf(Pri, "        [0x%04x] 0x%08x%08x %s\n",
               i,
               (UINT32) (response >> 32),
               (UINT32) response,
               pointer);
    }
    return OK;
}

RC AzaliaController::RirbInitialize(UINT16 Size, UINT16 RespIntCnt,
                                bool IsRespIntEnable, bool IsRespOverrunIntEnable)
{
    LOG_ENT();

    RC rc;
    UINT32 value;

    // Stop the RIRB
    CHECK_RC(RirbToggleRunning(false));

    // Make sure RIRB is allocated. RIRB must be aligned on 128B boundary
    // (or is it 1024B boundary -- r0.7 spec is unclear).
    if (!m_pRirbBaseAddr)
    {

        CHECK_RC(AzaliaUtilities::AlignedMemAlloc(RIRB_SIZE * sizeof(UINT64),
                                                  0x80, (void**) &m_pRirbBaseAddr, m_AddrBits,
                                                  GetMemoryType(),
                                                  m_IsRandomEnabled,
                                                  this));
        MASSERT(m_pRirbBaseAddr);

    }

    // Initialize RIRB entries
    for (UINT32 i=0; i<RIRB_SIZE; i++)
    {
         MEM_WR64((m_pRirbBaseAddr + i), 0);
    }

    // Program RIRB base address registers
    MASSERT(sizeof(PHYSADDR) == sizeof(UINT32)*2);
    PHYSADDR physRirbBaseAddr;
    if (GetIsUseMappedPhysAddr())
    {
        CHECK_RC(MemoryTracker::GetMappedPhysicalAddress(m_pRirbBaseAddr, this, &physRirbBaseAddr));
    }
    else
    {
        physRirbBaseAddr = Platform::VirtualToPhysical(m_pRirbBaseAddr);
    }
    CHECK_RC(RegWr(RIRBLBASE, (UINT32) physRirbBaseAddr));
    CHECK_RC(RegWr(RIRBUBASE, (UINT32) (physRirbBaseAddr >> 32)));

    // Program RIRB size
    CHECK_RC(RegRd(RIRBSIZE, &value));
    switch (Size)
    {
        case 2:
            if (!(value & RF_NUM(AZA_RIRBCTL_RIRBSZCAP, 0x1)))
            {
                Printf(Tee::PriLow, "Attemping to set RIRB size to unsupported value\n");
            }
            value |= RF_DEF(AZA_RIRBCTL_RIRBSIZE, _2ENTRIES);
            break;
        case 16:
            if (!(value & RF_NUM(AZA_RIRBCTL_RIRBSZCAP, 0x2)))
            {
                Printf(Tee::PriLow, "Attemping to set RIRB size to unsupported value\n");
            }
            value |= RF_DEF(AZA_RIRBCTL_RIRBSIZE, _16ENTRIES);
            break;
        case 256:
            if (!(value & RF_NUM(AZA_RIRBCTL_RIRBSZCAP, 0x4)))
            {
                Printf(Tee::PriLow, "Attemping to set RIRB size to unsupported value\n");
            }
            value |= RF_DEF(AZA_RIRBCTL_RIRBSIZE, _256ENTRIES);
            break;
        default:
            Printf(Tee::PriError, "Invalid RIRB size (%d) requested\n", Size);
            return RC::BAD_PARAMETER;
    }
    CHECK_RC(RegWr(RIRBSIZE, value));

    // Verify programmed size
    CHECK_RC(RegRd(RIRBSIZE, &value));
    switch (RF_VAL(AZA_RIRBCTL_RIRBSIZE, value))
    {
        case AZA_RIRBCTL_RIRBSIZE_2ENTRIES:   m_RirbSize = 2;   break;
        case AZA_RIRBCTL_RIRBSIZE_16ENTRIES:  m_RirbSize = 16;  break;
        case AZA_RIRBCTL_RIRBSIZE_256ENTRIES: m_RirbSize = 256; break;
        default:
            Printf(Tee::PriError, "Invalid RIRB size reported\n");
            return RC::HW_STATUS_ERROR;
    }

    // Reset RIRB read and write pointers
    CHECK_RC(RirbReset());

    // Set RespIntCnt. Note that RINTCNT of 0 is actually 256
    if (((RespIntCnt == 0) && (m_RirbSize < 256)) || (RespIntCnt > m_RirbSize))
    {
        Printf(Tee::PriError, "RINTCNT (%d) is too large!\n", RespIntCnt);
        return RC::BAD_PARAMETER;
    }

    CHECK_RC(RegWr(RINTCNT, RF_NUM(AZA_RIRBWP_RINTCNT, RespIntCnt)));

    // Clear RIRB status
    value = 0;
    value = RF_DEF(AZA_RIRBCTL_RIRBOIS, _W1C) |
        RF_DEF(AZA_RIRBCTL_RINTFL, _W1C);
    CHECK_RC(RegWr(RIRBSTS, value));

    // Set RIRB interrupt enables
    CHECK_RC(IntToggle(INT_RIRB_OVERRUN, IsRespOverrunIntEnable));
    CHECK_RC(IntToggle(INT_RIRB, IsRespIntEnable));

    // Start the RIRB
    CHECK_RC(RirbToggleRunning(true));
    LOG_EXT();
    return OK;
}

RC AzaliaController::RirbIsRunning(bool* pIsRunning)
{
    RC rc;
    UINT32 value;
    CHECK_RC(RegRd(RIRBCTL, &value));
    *pIsRunning = FLD_TEST_RF_DEF(AZA_RIRBCTL_RIRBDMAEN, _RUN, value);
    return OK;
}

RC AzaliaController::RirbGetIntCnt(UINT16 *pIntCnt)
{
    RC rc;
    UINT32 value = 0;
    CHECK_RC(RegRd(RINTCNT, &value));
    *pIntCnt = RF_VAL(AZA_RIRBWP_RINTCNT, value);
    return OK;
}

RC AzaliaController::RirbGetNResponses(UINT16 *pNResponses)
{
    RC rc;
    UINT32 wp;
    CHECK_RC(RegRd(RIRBWP, &wp));
    wp = RF_VAL(AZA_RIRBWP_RIRBWP, wp); // Last entry written by controller
                                             // Read pointer is in sw
    if (wp >= m_RirbReadPtr)
    {
        *pNResponses = wp - m_RirbReadPtr;
    }
    else
    {
        *pNResponses = m_RirbSize - (m_RirbReadPtr - wp);
    }

    return OK;
}

RC AzaliaController::RirbGetNSolicitedResponses(UINT08 CodecAddr, UINT16 *pNResponses)
{
    MASSERT(CodecAddr < MAX_NUM_SDINS);

    *pNResponses = m_SolResponses[CodecAddr].size();
    return OK;
}

RC AzaliaController::RirbGetNUnsolicitedResponses(UINT08 CodecAddr, UINT16 *pNResponses)
{
    MASSERT(CodecAddr < MAX_NUM_SDINS);

    *pNResponses = m_UnsolResponses[CodecAddr].size();
    return OK;
}

RC AzaliaController::RirbGetSolicitedResponse(UINT08 CodecAddr, UINT32 *pResponse)
{
    MASSERT(CodecAddr < MAX_NUM_SDINS);

    if (!m_pCodecTable[CodecAddr])
    {
        Printf(Tee::PriError, "No codec on sdin %d!\n", CodecAddr);
        return RC::AZA_CODEC_ERROR;
    }
    if (m_SolResponses[CodecAddr].empty())
    {
        Printf(Tee::PriError, "No responses on codec %d!\n", CodecAddr);
        return RC::AZA_CODEC_ERROR;
    }

    *pResponse = m_SolResponses[CodecAddr].front();
    m_SolResponses[CodecAddr].pop_front();
    return OK;
}

RC AzaliaController::RirbGetTotalSize(UINT16 *pSize)
{
    *pSize = m_RirbSize;
    return OK;
}

RC AzaliaController::RirbGetUnsolictedResponse(UINT08 CodecAddr, UINT32 *pResponse)
{
    MASSERT(CodecAddr < MAX_NUM_SDINS);

    if (!m_pCodecTable[CodecAddr])
    {
        Printf(Tee::PriError, "No codec on sdin %d!\n", CodecAddr);
        return RC::AZA_CODEC_ERROR;
    }
    if (m_UnsolResponses[CodecAddr].empty())
    {
        Printf(Tee::PriError, "No responses on codec %d!\n", CodecAddr);
        return RC::AZA_CODEC_ERROR;
    }
    *pResponse = m_UnsolResponses[CodecAddr].front();
    m_UnsolResponses[CodecAddr].pop_front();
    return OK;
}

RC AzaliaController::RirbPop(UINT64 Responses[], UINT16 nToPop, UINT16 *pNumPopped)
{
    RC rc;
    *pNumPopped = 0;

    if (!m_pRirbBaseAddr)
    {
        Printf(Tee::PriError, "RIRB base address not programmed\n");
        return RC::SOFTWARE_ERROR;
    }

    // Get RIRB pointers
    UINT32 wp, rp;
    CHECK_RC(RegRd(RIRBWP, &wp));
    wp = RF_VAL(AZA_RIRBWP_RIRBWP, wp); // Last entry written by controller
    rp = m_RirbReadPtr;                      // Read pointer is in sw

    UINT16 cnt = 0;
    while ((wp != rp) && (cnt < nToPop))
    {
        rp++;
        MASSERT(rp <= m_RirbSize);
        if (rp == m_RirbSize)
        {
            rp -= m_RirbSize;
        }
        Responses[cnt] = MEM_RD64(m_pRirbBaseAddr + rp);
        cnt++;
    }
    m_RirbReadPtr = rp;
    *pNumPopped = cnt;
    return OK;
}

RC AzaliaController::PrintRirbState(Tee::Priority Pri) const
{
    RC rc;
    UINT32 value;
    Printf(Pri, "    RIRB Register State:\n");
    CHECK_RC(RegRd(RIRBCTL, &value));
    Printf(Pri, "        RIRBCTL:         0x%08x\n", value);
    CHECK_RC(RegRd(RIRBSTS, &value));
    Printf(Pri, "        RIRBSTS:         0x%08x\n", value);
    CHECK_RC(RegRd(RIRBSIZE, &value));
    Printf(Pri, "        RIRBSIZE:        0x%08x\n", value);
    CHECK_RC(RegRd(RINTCNT, &value));
    Printf(Pri, "        RINTCNT:         0x%08x\n", value);
    CHECK_RC(RegRd(RIRBWP, &value));
    Printf(Pri, "        RIRBWP:          0x%08x\n", value);
    Printf(Pri, "        ReadPtr (SW):    0x%04x\n", m_RirbReadPtr);
    CHECK_RC(RegRd(AZA_RIRBLBASE, &value));
    Printf(Pri, "        RIRBLBASE:       0x%08x\n", value);
    CHECK_RC(RegRd(AZA_RIRBUBASE, &value));
    Printf(Pri, "        RIRBUBASE:       0x%08x\n", value);
    Printf(Pri, "        VirtualBase:     " PRINT_PHYS "\n", PRINT_FMT_PTR(m_pRirbBaseAddr));
    return OK;
}

RC AzaliaController::RirbProcess()
{
    RC rc;
    UINT16 nRirbEntries = 0;
    UINT16 nPopped = 0;

    CHECK_RC(RirbGetNResponses(&nRirbEntries));
    vector<UINT64> responses(nRirbEntries);
    CHECK_RC(RirbPop(&responses[0], nRirbEntries, &nPopped));
    if (nPopped != nRirbEntries)
    {
        Printf(Tee::PriError, "Expected to get %d RIRB entries, only got %d!\n",
               nRirbEntries, nPopped);

        rc = RC::SOFTWARE_ERROR;
    }
    for (UINT32 i=0; i<nPopped; i++)
    {
        UINT32 upperBits = responses[i] >> 32;
        UINT16 codecAddr = RF_VAL(AZA_CDC_RESPONSE_CAD, upperBits);
        bool isUnsol = RF_VAL(AZA_CDC_RESPONSE_UNSOL, upperBits) == AZA_CDC_RESPONSE_UNSOL_ISUNSOL;
        if (!m_pCodecTable[codecAddr])
        {
            Printf(Tee::PriError, "RIRB response with invalid codec address (%d 0x%08x%08x)!\n",
                   codecAddr,
                   (UINT32) (responses[i] >> 32),
                   (UINT32) (responses[i]));

            rc = RC::SOFTWARE_ERROR;
        }
        if (isUnsol)
        {
            m_UnsolResponses[codecAddr].push_back(RF_VAL(AZA_CDC_RESPONSE_RESPONSE,
                                                          responses[i]));
        }
        else
        {
            m_SolResponses[codecAddr].push_back(RF_VAL(AZA_CDC_RESPONSE_RESPONSE,
                                                        responses[i]));
        }
    }
    return rc;
}

RC AzaliaController::RirbReset()
{
    RC rc;
    UINT32 value;
    CHECK_RC(RegRd(RIRBCTL, &value));
    if (value & RF_DEF(AZA_RIRBCTL_RIRBDMAEN, _RUN))
    {
        Printf(Tee::PriError, "RIRB is running. Cannot reset pointers\n");
        return RC::SOFTWARE_ERROR;
    }
    // Reset write pointer
    CHECK_RC(RegWr(RIRBWP, RF_DEF(AZA_RIRBWP_RIRBWPRST, _RESET_WP)));
    // Verify reset
    CHECK_RC(RegRd(RIRBWP, &value));
    if (value)
    {
        Printf(Tee::PriError, "RIRB write pointer did not reset\n");
        return RC::HW_STATUS_ERROR;
    }
    // Reset read pointer
    m_RirbReadPtr = 0;
    return OK;
}

RC AzaliaController::RirbToggleCoherence(bool IsEnable)
{
    RC rc;
    if (!m_IsIch6Mode)
    {
        UINT32 value = 0;
        CHECK_RC(CfgRd32(AZA_CFG_19, &value));
        if (IsEnable)
            value |= 0x00040000;
        else
            value &= ~(0x00040000);
        CHECK_RC(CfgWr32(AZA_CFG_19, value));
    }
    return OK;
}

RC AzaliaController::RirbToggleRunning(bool IsRunning)
{
    RC rc;
    UINT32 value, valueToWrite;
    CHECK_RC(RegRd(RIRBCTL, &value));
    valueToWrite = value & ~RF_SHIFTMASK(AZA_RIRBCTL_RIRBDMAEN);
    if (IsRunning)
    {
        valueToWrite |= RF_DEF(AZA_RIRBCTL_RIRBDMAEN, _RUN);
    } else {
        valueToWrite |= RF_DEF(AZA_RIRBCTL_RIRBDMAEN, _STOP);
    }
    if (value != valueToWrite)
    {
        CHECK_RC(RegWr(RIRBCTL, valueToWrite));
        AzaRegPollArgs pollArgs;
        pollArgs.pAzalia = this;
        pollArgs.reg = RIRBCTL;
        pollArgs.mask = RF_SHIFTMASK(AZA_RIRBCTL_RIRBDMAEN);
        pollArgs.value = valueToWrite & RF_SHIFTMASK(AZA_RIRBCTL_RIRBDMAEN);
        rc = POLLWRAP_HW(AzaRegWaitPoll, &pollArgs, Tasker::GetDefaultTimeoutMs());
        if (rc != OK)
        {
            Printf(Tee::PriError, "Timeout waiting on RIRB DMA status\n");
            return RC::TIMEOUT_ERROR;
        }
    }
    return OK;
}

RC AzaliaController::GenerateStreamNumber(AzaliaDmaEngine::DIR Dir, UINT08* pStreamNumber,
                                      UINT08 DesiredStreamNumber)
{
    *pStreamNumber = 0;

    if (DesiredStreamNumber == 0)
    {
        Printf(Tee::PriError, "Stream number cannot be 0\n");
        return RC::BAD_PARAMETER;
    }

    // Select a stream number
    if (DesiredStreamNumber == RANDOM_STRM_NUMBER ||
        DesiredStreamNumber == RANDOM_NONZERO_STRM_NUMBER)
    {
        // Select a random available stream number. If there was an open
        // DMA engine, there should be an open stream number. By convention,
        // stream numbers cannot be 0. We had formerly allowed this for
        // some codecs, but not anymore.
        *pStreamNumber = 1;
        if (m_IsRandomEnabled)
            *pStreamNumber = AzaliaUtilities::GetRandom(1, m_vpStreams.size()-1);

        while (!IsStreamNumberUnique(Dir, *pStreamNumber))
        {
            *pStreamNumber = (*pStreamNumber+1) % m_vpStreams.size();
        }
    }
    else
    {
        // A specific stream number was requested. Make sure that the index
        // is not already in use.
        if (!IsStreamNumberUnique(Dir, DesiredStreamNumber))
        {
            Printf(Tee::PriError, "%s stream %d already in use!\n",
                   (Dir == AzaliaDmaEngine::DIR_INPUT ? "Input" : "Output"),
                   DesiredStreamNumber);

            return RC::AZA_STREAM_ERROR;
        }
        *pStreamNumber = DesiredStreamNumber;
    }
    MASSERT(*pStreamNumber);

    return OK;
}

RC AzaliaController::StreamToggleInputCoherence(bool IsEnable)
{
    RC rc;
    if (!m_IsIch6Mode)
    {
        UINT32 value = 0;
        CHECK_RC(CfgRd32(AZA_CFG_19, &value));
        if (IsEnable)
            value |= 0x00000100;
        else
            value &= ~(0x00000100);
        CHECK_RC(CfgWr32(AZA_CFG_19, value));
    }
    return OK;
}

RC AzaliaController::StreamToggleOutputCoherence(bool IsEnable)
{
    RC rc;
    if (!m_IsIch6Mode)
    {
        UINT32 value = 0;
        CHECK_RC(CfgRd32(AZA_CFG_19, &value));
        if (IsEnable)
            value |= 0x00000001;
        else
            value &= ~(0x00000001);
        CHECK_RC(CfgWr32(AZA_CFG_19, value));
    }
    return OK;
}

bool AzaliaController::IsStreamNumberUnique(AzaliaDmaEngine::DIR Dir, UINT08 StreamNumber)
{
    AzaliaDmaEngine::DIR engineDir;
    UINT08 engineStreamNumber;
    for (UINT08 i=0; i<m_vpStreams.size(); i++)
    {
        m_vpStreams[i]->GetEngine()->GetDir(&engineDir);
        m_vpStreams[i]->GetEngine()->GetStreamNumber(&engineStreamNumber);
        if ((engineDir == Dir) &&
            (engineStreamNumber == StreamNumber) &&
            (m_vpStreams[i]->IsReserved()))
        {
            return false;
        }
    }
    return true;
}

RC AzaliaController::ToggleBdlCoherence(bool IsEnable)
{
    RC rc;
    if (!m_IsIch6Mode)
    {
        UINT32 value = 0;
        CHECK_RC(CfgRd32(AZA_CFG_19, &value));
        if (IsEnable)
            value |= 0x00010000;
        else
            value &= ~(0x00010000);
        CHECK_RC(CfgWr32(AZA_CFG_19, value));
    }
    return OK;
}

RC AzaliaController::Reset(bool putIntoReset)
{
    RC rc;
    UINT32 value = 0;
    AzaRegPollArgs pollArgs;
    CHECK_RC(RegRd(GCTL, &value));
    switch (RF_VAL(AZA_GCTL_CRST, value))
    {
        case AZA_GCTL_CRST_ASSERT:
            if (putIntoReset) return OK; // already in RESET
            pollArgs.pAzalia = this;
            pollArgs.reg = GCTL;
            pollArgs.mask = RF_SHIFTMASK(AZA_GCTL_CRST);
            pollArgs.value = AZA_GCTL_CRST_DEASSERT;
            value = FLD_SET_RF_DEF(AZA_GCTL_CRST, _DEASSERT, value);
            CHECK_RC(RegWr(GCTL, value));
            rc = POLLWRAP_HW(AzaRegWaitPoll, &pollArgs, Tasker::GetDefaultTimeoutMs());
            if (rc != OK)
            {
                Printf(Tee::PriError, "Timeout waiting for Azalia to enter RESET\n");
                return RC::TIMEOUT_ERROR;
            }
            // All interrupt enables are clear when coming out of reset.
            memset(&m_IntEnables, 0, sizeof(m_IntEnables));
            CHECK_RC(IntToggle(INT_GLOBAL, false));
            CHECK_RC(IntToggle(INT_CONTROLLER, false));
            CHECK_RC(IntToggle(INT_CORB_MEM_ERR, false));
            CHECK_RC(IntToggle(INT_RIRB_OVERRUN, false));
            CHECK_RC(IntToggle(INT_RIRB, false));
            break;
        case AZA_GCTL_CRST_DEASSERT:
            if (!putIntoReset) return OK; // already running
            pollArgs.pAzalia = this;
            pollArgs.reg = GCTL;
            pollArgs.mask = RF_SHIFTMASK(AZA_GCTL_CRST);
            pollArgs.value = AZA_GCTL_CRST_ASSERT;
            value = FLD_SET_RF_DEF(AZA_GCTL_CRST, _ASSERT, value);
            CHECK_RC(RegWr(GCTL, value));
            rc = POLLWRAP_HW(AzaRegWaitPoll, &pollArgs, Tasker::GetDefaultTimeoutMs());
            if (rc != OK)
            {
                Printf(Tee::PriError, "Timeout waiting for Azalia to exit RESET\n");
                return RC::TIMEOUT_ERROR;
            }

            for (UINT08 i=0; i<m_vpStreamEngines.size(); i++)
                CHECK_RC(m_vpStreamEngines[i]->SetEngineState(AzaliaDmaEngine::STATE_STOP));
            CHECK_RC(CorbToggleRunning(false));
            CHECK_RC(RirbToggleRunning(false));
            // Wait long enough to ensure minimum link timing
            // requirements (at least 100us).
            AzaliaUtilities::Pause(1);
            // Reset RIRB pointer, since RIRB write pointer was also just reset
            m_RirbReadPtr = 0;
            break;
        default:
            MASSERT(!"Unknown GCTL value");
            return RC::SOFTWARE_ERROR;
    }
    return OK;
}

RC AzaliaController::UninitDataStructures()
{
    RC rc;
    if (m_pCorbBaseAddr)
    {
        CHECK_RC(AzaliaUtilities::AlignedMemFree(m_pCorbBaseAddr));
        m_pCorbBaseAddr = NULL;
    }
    if (m_pRirbBaseAddr)
    {
        CHECK_RC(AzaliaUtilities::AlignedMemFree(m_pRirbBaseAddr));
        m_pRirbBaseAddr = NULL;
    }
    if (m_pDmaPosBaseAddr)
    {
        CHECK_RC(AzaliaUtilities::AlignedMemFree(m_pDmaPosBaseAddr));
        m_pDmaPosBaseAddr = NULL;
    }

    while (m_vpStreamEngines.size() > 0)
    {
        AzaliaDmaEngine* streamPtr = m_vpStreamEngines.back();
        m_vpStreamEngines.pop_back();
        delete streamPtr;
    }

    while (m_vpStreams.size() > 0)
    {
        AzaStream* strmPtr = m_vpStreams.back();
        m_vpStreams.pop_back();
        delete strmPtr;
    }

    return OK;
}

RC AzaliaController::UninitRegisters()
{
    for (UINT32 i=0; i<MAX_NUM_SDINS; i++)
    {
        if (m_pCodecTable[i])
        {
            delete m_pCodecTable[i];
            m_pCodecTable[i] = NULL;
        }
        m_SolResponses[i].clear();
        m_UnsolResponses[i].clear();
    }

    return OK;
}

RC AzaliaController::MapPins() const
{
    RC rc;
    for (UINT32 i=0; i<MAX_NUM_SDINS; i++)
    {
        if (m_pCodecTable[i])
        {
            CHECK_RC(m_pCodecTable[i]->MapPins());
        }
    }

    return OK;
}

RC AzaliaController::GetWallClockCount(UINT32 *pCount)
{
    RC rc;
    CHECK_RC(RegRd(WALCLK, pCount));
    return rc;
}

RC AzaliaController::GetWallClockCountAlias(UINT32 *pCount)
{
    RC rc;
    CHECK_RC(RegRd(WALCLKA, pCount));
    return rc;
}

RC AzaliaController::GetMaxNumInputStreams(UINT32 *pNumInputStreams) const
{
    *pNumInputStreams = m_NInputStreamsSupported;
    return OK;
}

RC AzaliaController::GetMaxNumOutputStreams(UINT32 *pNumOutputStreams) const
{
    *pNumOutputStreams = m_NOutputStreamsSupported;
    return OK;
}

RC AzaliaController::GetMaxNumStreams(UINT32 *pNumStreams) const
{
    *pNumStreams = m_vpStreams.size();
    return OK;
}

RC AzaliaController::SendICommand(UINT32 CodecNum, UINT32 NodeId, UINT32 Verb,
                                  UINT32 Payload, UINT32 *pResp)
{
    RC rc = OK;
    UINT32 command;

    bool azaWasInReset = false;
    CHECK_RC(IsInReset(&azaWasInReset));
    if(azaWasInReset)
        CHECK_RC(Reset(false));

    // Turn off the Corb and Rirb temporarily so we don't leave
    // unexpected solicited responses in the rirb queue.
    bool corbWasRunning = false;
    CHECK_RC(CorbIsRunning(&corbWasRunning));
    if (corbWasRunning)
        CHECK_RC(CorbToggleRunning(false));

    bool rirbWasRunning = false;
    CHECK_RC(RirbIsRunning(&rirbWasRunning));
    if (rirbWasRunning)
        CHECK_RC(RirbToggleRunning(false));

    Printf(Tee::PriDebug, "Immediate Command will be sent to codec %d, node %d\n",
           CodecNum&0x0F, NodeId&0x00FF);

    Printf(Tee::PriDebug, "  verb    = 0x%05x\n" ,Verb&0x000FFFFF);

    command = (CodecNum&0x0F)<<28 | (NodeId&0x00FF)<<20 | (Verb&0x000FFFFF);
    CHECK_RC_CLEANUP(SendICommand(command, pResp));

    // Verify that it came from the correct codec
    if (!m_IsIch6Mode) // ICH6 doesn't implement IRRADD register field
    {
        UINT32 retCodec;
        CHECK_RC_CLEANUP(RegRd(ICS, &retCodec));
        retCodec = (retCodec>>4)&0xF;
        if (retCodec!=CodecNum)
        {
            Printf(Tee::PriError, "Unexpected codec address received, %d\n", retCodec);
            rc = RC::SOFTWARE_ERROR;
            goto Cleanup;
        }
    }

    Cleanup:
    // Return link to whatever state it started from.
    if (rirbWasRunning)
        CHECK_RC(RirbToggleRunning(true));
    if (corbWasRunning)
        CHECK_RC(CorbToggleRunning(true));
    if (azaWasInReset)
        CHECK_RC(Reset(true));

    return rc;
}

RC AzaliaController::SendICommand(UINT32 Command, UINT32 *pResponse)
{

    RC rc = OK;
    UINT32 response;

    // Clear the Immediate Result Valid bit so we can determine
    // when a new response has arrived.
    // --this operation also writes a 0 to the ICB register which
    //   is required by Intel ICH6's out-of-spec implementation.
    CHECK_RC(RegWr(ICS, RF_NUM(AZA_ICS_IRV,1))); // IRV is W1C

    // Wait until controller reports IRV is clear and ICH says it can
    // accept an immediate command.
    AzaRegPollArgs pollArgs;
    pollArgs.pAzalia = this;
    pollArgs.reg = ICS;
    pollArgs.mask = 0x3;
    pollArgs.value = 0x0;
    rc = POLLWRAP_HW(AzaRegWaitPoll, &pollArgs, 1000);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Timeout waiting for Immediate Command Busy bit to clear!\n"
                              "Unable to send immediate command!");

        return RC::TIMEOUT_ERROR;
    }

    // Send the command
    CHECK_RC(RegWr(ICOI,Command));

     // Once again, Intel doesn't follow their own spec.  ICB register
    // must be written with 1 to initiate command transfer.
    if (m_IsIch6Mode)
    {
        CHECK_RC(RegWr(ICS, RF_NUM(AZA_ICS_ICB, 1)));
    }

    // Wait for response to arrive.
    pollArgs.reg = ICS;
    pollArgs.mask = 0x0002;
    pollArgs.value = 0x0002;
    rc = POLLWRAP_HW(AzaRegWaitPoll, &pollArgs, 1000);
    if (rc != OK)
    {
        Printf(Tee::PriError, "Timeout waiting for Immediate Command response to arrive!\n");
        if (m_IsIch6Mode)
        {
            // We must clear the ICB or the RirbDmaState cannot be changed.
            RegWr(ICS,0);
        }
        return RC::TIMEOUT_ERROR;
    }

    // Get the response
    CHECK_RC(RegRd(IRII, &response));
    *pResponse = response;

    // Report the result
    UINT32 ics;
    CHECK_RC(RegRd(ICS, &ics));
    Printf(Tee::PriDebug, "  response = 0x%08x (%s)\n",
           response,
           (ics&0x8)?"unsolicited":"solicited");

    return rc;
}

bool AzaliaController::IsFormatSupported(UINT32 Dir, UINT32 Rate, UINT32 Size, UINT32 Sdin)
{
    INT32 startSdin = Sdin;
    INT32 endSdin = Sdin;
    if (Sdin >= MAX_NUM_SDINS)
    {
        startSdin = 0;
        endSdin = MAX_NUM_SDINS - 1;
    }

    for (INT32 sdinCnt = startSdin; sdinCnt <= endSdin; ++sdinCnt)
    {
        if (m_pCodecTable[sdinCnt])
        {
            if (m_pCodecTable[sdinCnt]->IsFormatSupported(Dir, Rate, Size))
                return true;
        }
    }

    return false;
}

bool AzaliaController::IsGpuAza()
{
    switch (m_DeviceId)
    {
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK104:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK106:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK107:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK110:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GK208:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM107:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM200:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM204:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GM206:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP100:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP102:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP104:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP106:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP107:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GP108:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GV100:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU102:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU104:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU106:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU108:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU116:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_TU117:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA100:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA102:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA103:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA104:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA106:
        case LW_XVE1_ID_DEVICE_CHIP_AZALIA_GA107:
            return true;
    }

    return false;
}

RC AzaliaController::InitBar()
{
    RC rc = OK;
    CHECK_RC(Controller::InitBar());

    UINT32 bar0Offset = 0;
    CHECK_RC(SysUtil::GetBarOffset(m_BasicInfo.DeviceId, 0, &bar0Offset));
    m_pBar0 = reinterpret_cast<UINT08*>(m_pRegisterBase) + bar0Offset;

    return rc;
}

RC AzaliaController::InitExtDev()
{
    LOG_ENT();
    RC rc;

    for (UINT32 i=0; i<MAX_NUM_SDINS; i++)
    {
        if (m_pCodecTable[i])
        {
            // Bug 737561: MODS not able to initialize codec 3 of Intel azalia device
            //             on P55 chipset. Hence, we fail only if we are working with
            //             LWPU azalia device, else just print a warning.
            if ((rc = m_pCodecTable[i]->Initialize()) != OK)
            {
                if (m_VendorId == Pci::VendorIds::Lwpu)
                {
                    return rc;
                }
                else
                {
                    rc.Clear();
                    Printf(Tee::PriWarn, "Could not initialize codec %d of azalia device "
                                         "with vendor id: 0x%04x.\n",
                           i, m_VendorId);

                    Printf(Tee::PriWarn, "Azalia test will fail if run on this codec.\n");
                }
            }
        }
    }

    LOG_EXT();
    return rc;
}

RC AzaliaController::ReadIResponse(UINT32 *pResponse)
{
    RC rc = OK;

    // Get the response
    UINT32 resp = 0;
    CHECK_RC(RegRd(IRII, &resp));

    UINT32 ics = 0;
    CHECK_RC(RegRd(ICS, &ics));
    UINT32 retCodec = (ics >> 4)& 0xF;

    // Report the result
    Printf(Tee::PriNormal,
           "Response = 0x%08x (%s) codec address 0x%02x ICB 0x%x, IRV 0x%x\n",
           resp, (ics & 0x8)?"unsolicited":"solicited", retCodec,
           ics & 0x1, (ics >> 1) & 0x1);

    *pResponse = resp;

    return rc;
}

RC AzaliaController::CodecLoad(string &FileName, bool IsOnlyPrint)
{
    RC rc;
    LOG_ENT();
    vector<UINT32> vNodeVerbCfg;
    if (FileName.empty())
    {
        Printf(Tee::PriError, "Please input correct file name\n");
        return RC::ILWALID_INPUT;
    }

    Printf(Tee::PriDebug, "File Name:%s\n", FileName.c_str());

    FILE *pFile;
    CHECK_RC(Utility::OpenFile(FileName, &pFile, "r"));
    UINT32 fileSize = 0;
    CHECK_RC(Utility::FileSize(pFile, (long int *)&fileSize));
    string str;
    str.resize(fileSize);
    UINT32 readNum =  fread(&str[0], 1, fileSize, pFile);
    if (readNum != fileSize)
    {
        Printf(Tee::PriError, "File reading error, expect 0x%x, get 0x%x\n",
               (UINT32)fileSize, readNum);

        return RC::ILWALID_INPUT;
    }
    fclose(pFile);

    string::size_type pos = 0;
    // Find consective hexadecimal char;
    while (pos < str.size())
    {
        if (str[pos] == ';')
        {
            pos = str.find('\n', pos + 1);
            if (pos == string::npos)
                break;

            pos++;
            continue;
        }
        else if (str[pos] == '\n' || str[pos] == '\r')
        {
            pos++;
            continue;
        }

        string::size_type start = pos;
        string::size_type end = str.find_first_of('\n', start + 1);
        if (end == string::npos)
            break;

        string lineStr = str.substr(start, end - start);
        Printf(Tee::PriDebug, "Line to be parsed:%s\n", lineStr.c_str());
        pos = end + 1;

        if (lineStr.size() < 8)
            continue;

        string::size_type tmpPos = lineStr.find_first_of("01");
        if (tmpPos == string::npos)
            continue;

        if (tmpPos + 8 > lineStr.size())
            continue;

        if (lineStr[tmpPos + 1] == 'x' || lineStr[tmpPos + 1] == 'X')
        {
            // begin with 0x
            tmpPos = tmpPos + 2;
        }

        string digitStr = lineStr.substr(tmpPos, 8);

        UINT32 i = 0;
        for (i = 0; i < 8; i++)
        {
            if (!isxdigit(digitStr[i]))
                break;
        }

        if (i != 8)
            continue;

        UINT32 cmd = strtoul(digitStr.c_str(), NULL, 16);
        vNodeVerbCfg.push_back(cmd);
        Printf(Tee::PriDebug, "cmd :%08x\n", cmd);
    }

    for (UINT32 i = 0; i < vNodeVerbCfg.size(); i++)
    {
        UINT32 command = vNodeVerbCfg[i];

        if (IsOnlyPrint)
        {
            Printf(Tee::PriNormal, "Index 0x%x command: 0x%08x\n", i, command);
        }
        else
        {
            Printf(Tee::PriNormal, "Send index 0x%x command: 0x%08x\n", i, command);
            UINT32 resp = 0;
            CHECK_RC(SendICommand(command, &resp));
        }
    }
    LOG_EXT();
    return OK;
}

RC AzaliaController::GetIsr(ISR *Func) const
{
    switch (m_BasicInfo.DeviceIndex)
    {
        case 0: *Func = &AzaliaControllerMgr::IsrCommon0; break;
        case 1: *Func = &AzaliaControllerMgr::IsrCommon1; break;
        case 2: *Func = &AzaliaControllerMgr::IsrCommon2; break;
        case 3: *Func = &AzaliaControllerMgr::IsrCommon3; break;
        case 4: *Func = &AzaliaControllerMgr::IsrCommon4; break;
        case 5: *Func = &AzaliaControllerMgr::IsrCommon5; break;

        default:
            Printf(Tee::PriError, "Interrupts unsupported on controller %d\n",
                   m_BasicInfo.DeviceIndex);
            return RC::BAD_PARAMETER;
    }

    return OK;
}

RC AzaliaController::GetInterruptMode(IntrModes *pMode) const
{
    *pMode = m_IntrMode;
    return OK;
}
RC AzaliaController::GetCapPtr(UINT32 *CapBaseOffset)
{
    RC rc;
    UINT32 capPtr = 0;
    UINT32 capWord1 = 0;
    CHECK_RC(CfgRd32(PCI_CAP_PTR_OFFSET, &capPtr));
    while(capPtr!=0)
    {
       CHECK_RC(CfgRd32(capPtr, &capWord1) );

       // Check the CapId:0x10
       if( (capWord1&0xff) == PCI_CAP_ID_PCIE )
       {
          *CapBaseOffset = capPtr;
          return OK;
       }

       // Update Cap Pointer for next Cap
       capPtr = ((capWord1&0xffff)>>8);
    }
    return RC::UNSUPPORTED_FUNCTION;
}

RC AzaliaController::GetAspmState(UINT32 *aspm)
{
    RC rc;
    UINT32 capPtr;
    UINT32 linkCtrl = 0;
    CHECK_RC(GetCapPtr(&capPtr));
    // Read the Link Control register
    CHECK_RC(CfgRd32(capPtr+LW_PCI_CAP_PCIE_LINK_CTRL,&linkCtrl));
    *aspm = RF_VAL(LW_PCI_CAP_PCIE_LINK_CTRL_ASPM,linkCtrl);
    return OK;
}

RC AzaliaController::SetAspmState(UINT32 Aspm)
{
    RC rc;
    UINT32 capPtr;
    UINT32 linkCtrl = 0;
    CHECK_RC(GetCapPtr(&capPtr));
    // Read the Link Control register
    CHECK_RC(CfgRd32(capPtr+LW_PCI_CAP_PCIE_LINK_CTRL,&linkCtrl));
    linkCtrl = FLD_SET_RF_NUM(LW_PCI_CAP_PCIE_LINK_CTRL_ASPM,Aspm,linkCtrl);
    CHECK_RC(CfgWr32(capPtr+LW_PCI_CAP_PCIE_LINK_CTRL,linkCtrl));
    return OK;
}

Memory::Attrib AzaliaController::GetMemoryType()
{
#ifdef LWCPU_FAMILY_ARM
    return Memory::UC;
#endif
    return Memory::WC;
}

UINT16 AzaliaController::GetAerOffset() const
{
    return LW_XVE1_AER_CAP_HDR;
}

bool AzaliaController::AerEnabled() const
{
    RC rc;
    UINT32 xve1Misc1;
    CHECK_RC(CfgRd32(LW_XVE1_PRIV_MISC_1, &xve1Misc1));
    return FLD_TEST_DRF(_XVE1_PRIV, _MISC_1, _CYA_AER, _ENABLE, xve1Misc1);
}

RC AzaliaController::SetIsAllocFromPcieNode(bool IsPcie)
{
    m_IsAllocFromPcieNode = IsPcie;
    return OK;
}

RC AzaliaController::SetIsUseMappedPhysAddr(bool IsMapped)
{
    struct SysUtil::PciInfo pciInfo;
    if ((IsMapped == true) && (GetPciInfo(&pciInfo) == OK))
    {
        if(Xp::CanEnableIova(pciInfo.DomainNumber, pciInfo.BusNumber, pciInfo.DeviceNumber, pciInfo.FunctionNumber))
        {
            m_IsUseMappedPhysAddr = true;
        }
        else
        {
            m_IsUseMappedPhysAddr = false;
        }
    }
    else
    {
        m_IsUseMappedPhysAddr = false;
    }
    return OK;
}
