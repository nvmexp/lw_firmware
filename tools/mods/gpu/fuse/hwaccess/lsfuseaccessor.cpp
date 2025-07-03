/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2021-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lsfuseaccessor.h"

#include "core/include/jscript.h"
#include "core/include/platform.h"
#include "core/include/script.h"
#include "core/include/tasker.h"
#include "core/include/version.h"
#include "core/tests/testconf.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/fuse/fuse.h"
#include "gpu/include/testdevice.h"
#include "gpu/reghal/reghal.h"

LsFuseAccessor::LsFuseAccessor(TestDevice* pDev)
    : LrFuseAccessor(pDev),
      m_pTestDev(pDev),
      m_pFuse(nullptr)
{
    MASSERT(pDev);
}

void LsFuseAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    LrFuseAccessor::SetParent(pFuse);
    m_pFuse = pFuse;
}

RC LsFuseAccessor::SetupFuseBlow()
{
    // Program the PS latch to switch on power supply to fuse macro
    RC rc = ProgramPSLatch(true);
    if (rc != RC::OK)
    {
        if (rc == RC::PRIV_LEVEL_VIOLATION)
        {
            Printf(Tee::PriLow, "MODS not setting PS latch\n");
        }
        else
        {
            return rc;
        }
    }
    return LrFuseAccessor::SetupFuseBlow();
}

RC LsFuseAccessor::CleanupFuseBlow()
{
    // Program the PS latch to switch off power supply to fuse macro
    RC rc = ProgramPSLatch(false);
    if (rc != RC::OK)
    {
        if (rc == RC::PRIV_LEVEL_VIOLATION)
        {
            Printf(Tee::PriLow, "MODS not clearing PS latch\n");
        }
        else
        {
            return rc;
        }
    }
    return LrFuseAccessor::CleanupFuseBlow();
}

RC LsFuseAccessor::CheckPSDefaultState()
{
    RegHal& regHal = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    if (regHal.Test32(MODS_FUSE_DEBUGCTRL_PS09_SET, 0) &&
        regHal.Test32(MODS_FUSE_DEBUGCTRL_PS09_RESET, 1))
    {
        return RC::OK;
    }

    Printf(Tee::PriError, "PS09 power switch is not in its default state\n");
    return RC::SOFTWARE_ERROR;
}

RC LsFuseAccessor::ProgramPSLatch(bool bSwitchOn)
{
    RC rc;
    RegHal& regHal = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    if (!regHal.HasRWAccess(0, MODS_FUSE_DEBUGCTRL))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }

    if (bSwitchOn)
    {
        CHECK_RC(CheckPSDefaultState());

        // Bring the power switch to hold state
        UINT32 regVal = regHal.Read32(0, MODS_FUSE_DEBUGCTRL);
        regHal.SetField(&regVal, MODS_FUSE_DEBUGCTRL_PS09_SET, 0);
        regHal.SetField(&regVal, MODS_FUSE_DEBUGCTRL_PS09_RESET, 0);
        regHal.Write32(0, MODS_FUSE_DEBUGCTRL, regVal);

        // Set the power switch to turn on power for the fuse macro
        regHal.Write32(0, MODS_FUSE_DEBUGCTRL_PS09_SET, 1);
    }
    else
    {
        // Bring the power switch to hold state
        UINT32 regVal = regHal.Read32(0, MODS_FUSE_DEBUGCTRL);
        regHal.SetField(&regVal, MODS_FUSE_DEBUGCTRL_PS09_SET, 0);
        regHal.SetField(&regVal, MODS_FUSE_DEBUGCTRL_PS09_RESET, 0);
        regHal.Write32(0, MODS_FUSE_DEBUGCTRL, regVal);

        // Reset the power switch to turn off power for the fuse macro
        regHal.Write32(0, MODS_FUSE_DEBUGCTRL_PS09_RESET, 1);
     }

     Utility::SleepUS(PS09_RESET_DELAY_US);
     return rc;
}

RC LsFuseAccessor::BlowFuses(const vector<UINT32>& fuseBinary)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    RegHal& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    // Clear the fuse control register before starting
    regs.Write32(0, MODS_FUSE_FUSECTRL, 0);

    CHECK_RC(SetupHwFusing());

    // Get the cached fuses for comparison
    const vector<UINT32>* pCachedFuses = nullptr;
    CHECK_RC(GetFuseBlock(&pCachedFuses));
    const vector<UINT32>& cachedFuses = *pCachedFuses;

    if (cachedFuses.size() != fuseBinary.size())
    {
        Printf(Tee::PriError, "Fuse binary is not the correct size!\n");
        return RC::CANNOT_MEET_FS_REQUIREMENTS;
    }

    PrintNewFuses(*pCachedFuses, fuseBinary);

    CHECK_RC(PollFusesIdle());

    bool   isFuseEnSet    = false;

    UINT32 readProtectedStart, readProtectedEnd;
    GetReadProtectedRows(&readProtectedStart, &readProtectedEnd);
    for (UINT32 row = 0; row < fuseBinary.size(); row++)
    {
        bool bIsProtectedRow = ((row >= readProtectedStart) && (row <= readProtectedEnd));

        UINT32 valToBurn = fuseBinary[row];
        if (!bIsProtectedRow)
        {
            // Only burn bits going from 0 to 1
            valToBurn = fuseBinary[row] & ~cachedFuses[row];
        }

        if (valToBurn == 0)
        {
            continue;
        }

        // Pull GPIO to high
        if (!isFuseEnSet)
        {
            CHECK_RC(SetupFuseBlow());
            isFuseEnSet = true;
        }

        if (!bIsProtectedRow)
        {
            regs.Write32(0, MODS_FUSE_FUSEADDR, row);
            regs.Write32(0, MODS_FUSE_FUSEWDATA, valToBurn);
            regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_WRITE);
            CHECK_RC(PollFusesIdle());
        }
        else
        {
            regs.Write32(0, MODS_FUSE_FUSECTRL_BURN_AND_CHECK_ENABLE);

            regs.Write32(0, MODS_FUSE_FUSEADDR, row);
            regs.Write32(0, MODS_FUSE_FUSEWDATA, valToBurn);
            regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_WRITE);

            CHECK_RC(PollFusesIdle());

            if (regs.Test32(0, MODS_FUSE_FUSECTRL_BURN_AND_CHECK_STATUS_FAIL))
            {
                Printf(FuseUtils::GetPrintPriority(), "  Fuse mismatch at row %u\n", row);
                return RC::VECTORDATA_VALUE_MISMATCH;
            }
        }
    }

    // Pull down GPIO
    if (isFuseEnSet)
    {
        CleanupFuseBlow();
    }

    MarkFuseReadDirty();

    CHECK_RC(VerifyFusing(fuseBinary));

    return rc;
}

RC LsFuseAccessor::VerifyFusing(const vector<UINT32>& expectedBinary)
{
    StickyRC rc;
    const vector<UINT32>* pLwrrentFuses = nullptr;
    CHECK_RC(GetFuseBlock(&pLwrrentFuses));
    MASSERT(expectedBinary.size() == pLwrrentFuses->size());

    UINT32 readProtectedStart, readProtectedEnd;
    GetReadProtectedRows(&readProtectedStart, &readProtectedEnd);
    for (UINT32 row = 0; row < pLwrrentFuses->size(); row++)
    {
        if (row < readProtectedStart || row > readProtectedEnd)
        {
            UINT32 lwrrentRowVal = (*pLwrrentFuses)[row];
            Printf(FuseUtils::GetPrintPriority(), "Final Fuse value at row %d: 0x%x\n", row, lwrrentRowVal);
            if (lwrrentRowVal != expectedBinary[row])
            {
                rc = RC::VECTORDATA_VALUE_MISMATCH;
                Printf(FuseUtils::GetPrintPriority(),
                    "  Fuse mismatch at row %d: Expected 0x%08x != Actual 0x%08x\n",
                    row, expectedBinary[row], lwrrentRowVal);
            }
        }
    }
    return rc;
}

// Same as GH100
RC LsFuseAccessor::SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc)
{
    RC rc;
    RegHal& regHal = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    const UINT32 numIffRows = static_cast<UINT32>(iffRows.size());
    if (numIffRows > MAX_IFF_ROWS)
    {
        Printf(Tee::PriError,
               "Number of IFF rows greater than max allowed!. Max: %d, Current: %d\n",
               MAX_IFF_ROWS, numIffRows);
        return RC::SOFTWARE_ERROR;
    }

    // Writing the sequence RAM automatically sets ECC bits correctly.
    regHal.Write32(0, MODS_PMC_IFF_SEQUENCE_SIZE_ROW_COUNT, 0);

    UINT32 regVal = regHal.Read32(0, MODS_PMC_IFF_SEQUENCE_RAM_WRITE_ADDR);
    regHal.SetField(&regVal, MODS_PMC_IFF_SEQUENCE_RAM_WRITE_ADDR_POSITION, 0);
    regHal.SetField(&regVal, MODS_PMC_IFF_SEQUENCE_RAM_WRITE_ADDR_AUTOINC, 1);
    regHal.Write32(0, MODS_PMC_IFF_SEQUENCE_RAM_WRITE_ADDR, regVal);

    for (UINT32 row = 0; row < numIffRows; row++)
    {
        regHal.Write32(0, MODS_PMC_IFF_SEQUENCE_RAM_WRITE_DATA, iffRows[row]);
    }
    // Do error checking here in case of overflow
    if (!regHal.Test32(0, MODS_PMC_IFF_ERROR_IFF_SEQUENCE_RAM_WRITE_OVERFLOW, 0))
    {
        Printf(Tee::PriError,
               "IFF SW fusing write overflow reported\n");
        return RC::HW_STATUS_ERROR;
    }
    regHal.Write32(0, MODS_PMC_IFF_SEQUENCE_SIZE_ROW_COUNT, numIffRows);

    regVal = regHal.Read32(0, MODS_PMC_IFF_EXPECTED_CRC);
    regHal.SetField(&regVal, MODS_PMC_IFF_EXPECTED_CRC_VALUE, iffCrc);
    regHal.SetField(&regVal, MODS_PMC_IFF_EXPECTED_CRC_MISMATCH_STOP_IFF, 0);
    regHal.Write32(0, MODS_PMC_IFF_EXPECTED_CRC, regVal);

    return rc;
}

// Same as GH100
RC LsFuseAccessor::VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc)
{
    RegHal& regHal = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();
    const UINT32 numIffRows = static_cast<UINT32>(iffRows.size());

    if (!regHal.Test32(0, MODS_PMC_IFF_DEBUG_PRIV_LEVEL_MASK_READ_PROTECTION_LEVEL0_ENABLE))
    {
         Printf(Tee::PriDebug, "No permissions to check IFF status registers\n");
         return RC::OK;
    }

    UINT32 regVal = regHal.Read32(0, MODS_PMC_IFF_ACLWMULATED_CRC_POST_IFF_VALUE);
    if (regVal != iffCrc)
    {
        Printf(Tee::PriError,
               "Aclwmulated CRC post IFF: %u, does not equal callwlated CRC: %u\n",
               regVal, iffCrc);
        return RC::HW_STATUS_ERROR;
    }

    if (!(regHal.Test32(0, MODS_PMC_IFF_EXELWTION_FILTER_EXELWTE_ON_COLD_RESET, 1) &&
          regHal.Test32(0, MODS_PMC_IFF_EXELWTION_FILTER_EXELWTE_ON_HOT_RESET, 1) &&
          regHal.Test32(0, MODS_PMC_IFF_EXELWTION_FILTER_EXELWTE_ON_PF_FLR_RESET, 1)))
    {
        Printf(Tee::PriWarn,
               "All IFF commands may not be exelwted because of the exelwtion filter\n");
    }

    regVal = regHal.Read32(0, MODS_PMC_IFF_SEQUENCE_RAM_STATUS_ROW_INDEX);
    if (regVal != numIffRows)
    {
        Printf(Tee::PriError,
               "IFF sequence RAM status row index: %u, does not equal IFF row size: %u\n",
               regVal, numIffRows);
        return RC::HW_STATUS_ERROR;
    }

    regVal = regHal.Read32(0, MODS_PMC_IFF_SEQUENCE_RAM_STATUS_EXELWTION_STATE);
    if (!regHal.TestField(regVal, MODS_PMC_IFF_SEQUENCE_RAM_STATUS_EXELWTION_STATE_DONE))
    {
        Printf(Tee::PriError,
               "IFF sequence RAM status exelwtion state: %u, does not equal DONE\n",
               regVal);
        return RC::HW_STATUS_ERROR;
    }

    regVal = regHal.Read32(0, MODS_PMC_IFF_FSM_STATUS_PARSING_STATE);
    if (!regHal.TestField(regVal, MODS_PMC_IFF_FSM_STATUS_PARSING_STATE_FINISH))
    {
        Printf(Tee::PriError,
               "IFF FSM status parsing state: %u, does not equal FINISH\n",
               regVal);
        return RC::HW_STATUS_ERROR;
    }

    regVal = regHal.Read32(0, MODS_PMC_IFF_FSM_STATUS_DECODE_STATE);
    if (!regHal.TestField(regVal, MODS_PMC_IFF_FSM_STATUS_DECODE_STATE_IDLE))
    {
        Printf(Tee::PriError,
               "IFF FSM status decode state: %u, does not equal IDLE\n",
               regVal);
        return RC::HW_STATUS_ERROR;
    }

    regVal = regHal.Read32(0, MODS_PMC_SYSCTRL_PRI_STATUS_REQUEST_INFO_FSM_STATE);
    if (!regHal.TestField(regVal, MODS_PMC_SYSCTRL_PRI_STATUS_REQUEST_INFO_FSM_STATE_IDLE))
    {
        Printf(Tee::PriError,
               "Sysctrl PRI Status Request Info FSM state : %u, does not equal IDLE\n",
               regVal);
        return RC::HW_STATUS_ERROR;
    }

    return RC::OK;
}

RC LsFuseAccessor::SetupHwFusing()
{
    // Clear any cached values from the SW fusing
    MarkFuseReadDirty();

    return RC::OK;
}

RC LsFuseAccessor::LatchFusesToOptRegs()
{
    return m_pTestDev->HotReset(GpuSubdevice::FundamentalResetOptions::Enable);
}

RC LsFuseAccessor::SetFusingVoltage(UINT32 vddMv /*unused*/)
{
    // Fixed fusing voltage supported by programming the FSP

    // Refer bug https://lwbugswb.lwpu.com/LWBugs5/redir.aspx?url=/3415120
    // We need to set up a scratch to indicate fusing mode to the FSP and issue a hot reset
    static constexpr UINT32 LW_PRIV_OFFSET_LWLSAW = 0x00028000;
    UINT32 fspScratch = LW_PRIV_OFFSET_LWLSAW +
                        m_pTestDev->GetInterface<LwLinkRegs>()->Regs().LookupAddress(MODS_LWLSAW_SELWRE_SCRATCH_COLD_GROUP_0, 0);

    UINT32 regVal = 0;
    m_pTestDev->GetInterface<LwLinkRegs>()->RegRd(0,
            RegHalDomain::RAW,
            fspScratch,
            &regVal);
    regVal |= 0x1;
    m_pTestDev->GetInterface<LwLinkRegs>()->RegWr(0,
        RegHalDomain::RAW,
        fspScratch,
        regVal);
    return m_pTestDev->HotReset(GpuSubdevice::FundamentalResetOptions::Disable);
}

UINT32 LsFuseAccessor::GetFuseEnableGpioNum()
{
    return FUSE_EN_GPIO_NUM;
}

void LsFuseAccessor::GpioSetOutput(UINT32 gpioNum, bool toHigh)
{
    RegHal& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    // First set the direction to output
    regs.Write32(0, MODS_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES, gpioNum);
    // Then set the output
    regs.Write32(0, MODS_GPIO_OUTPUT_CNTL_IO_OUTPUT, gpioNum, toHigh ? 1 : 0);
    // Trigger the GPIO change
    regs.Write32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER);
}

void LsFuseAccessor::GetReadProtectedRows(UINT32 *pStart, UINT32* pEnd)
{
    MASSERT(pStart);
    MASSERT(pEnd);

    // TODO
    // Hard-coded for now ; will be added to the fuse file for parsing

    static constexpr UINT32 s_FspStart = 412;
    static constexpr UINT32 s_FspEnd   = 511;

    *pStart = s_FspStart;
    *pEnd   = s_FspEnd;
}

RC LsFuseAccessor::ReadFuseBlock(vector<UINT32>* pReadFuses)
{
    RC rc;
    MASSERT(pReadFuses);
    pReadFuses->clear();
    auto& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    UINT32 numFuseRows;
    CHECK_RC(GetNumFuseRows(&numFuseRows));
    UINT32 rpStart = 0;
    UINT32 rpEnd = 0;
    GetReadProtectedRows(&rpStart, &rpEnd);
    for (UINT32 row = 0; row < numFuseRows; row++)
    {
        regs.Write32(0, MODS_FUSE_FUSEADDR, row);
        regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_READ);
        CHECK_RC(PollFusesIdle());
        if (row >= rpStart && row <= rpEnd)
        {
            // For read protected fuses, we don't care about the original fuse values
            // Adding them as 0 to escape failures due to possbile 1->0 transitions
            pReadFuses->push_back(0);
        }
        else
        {
            pReadFuses->push_back(regs.Read32(0, MODS_FUSE_FUSERDATA));
        }
    }
    // Don't need to modify IFF RAM indices
    // We have a sequence RAM for IFF on Hopper which is accessed through
    // a different set of registers
    return RC::OK;
}
