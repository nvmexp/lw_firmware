/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2020-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "ga10xfuseaccessor.h"

#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/fuse/fuse.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghal.h"

GA10xFuseAccessor::GA10xFuseAccessor(TestDevice* pDev)
    : AmpereFuseAccessor(pDev),
      m_pDev(pDev),
      m_pFuse(nullptr)
{
    MASSERT(pDev);
    MASSERT(dynamic_cast<GpuSubdevice*>(pDev) != nullptr);
}

void GA10xFuseAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    AmpereFuseAccessor::SetParent(pFuse);
    m_pFuse = pFuse;
}

RC GA10xFuseAccessor::SetupFuseBlow()
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
    return AmpereFuseAccessor::SetupFuseBlow();
}

RC GA10xFuseAccessor::CleanupFuseBlow()
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
    return AmpereFuseAccessor::CleanupFuseBlow();
}

RC GA10xFuseAccessor::BlowFuses(const vector<UINT32>& fuseBinary)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    RegHal& regHal = m_pDev->Regs();

    CHECK_RC(SetupHwFusing());

    // Clear the fuse control register before starting
    regHal.Write32(MODS_FUSE_FUSECTRL, 0);

    if (IsFusePrivSelwrityEnabled())
    {
        CHECK_RC(LowerFuseSelwrity());
    }

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

    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    UINT32 gpioNum        = GetFuseEnableGpioNum();
    bool   isFuseEnSet    = false;

    for (UINT32 row = 0; row < fuseBinary.size(); row++)
    {
        // Only burn bits going from 0 to 1
        UINT32 valToBurn = fuseBinary[row] & ~cachedFuses[row];
        if (valToBurn == 0)
        {
            continue;
        }

        // Pull GPIO to high
        if (!isFuseEnSet)
        {
            pSubdev->GpioSetOutput(gpioNum, true);

            // Program the PS latch to switch on power supply to fuse macro
            CHECK_RC(ProgramPSLatch(true));

            isFuseEnSet = true;
        }

        regHal.Write32(MODS_FUSE_FUSEADDR, row);
        regHal.Write32(MODS_FUSE_FUSEWDATA, valToBurn);
        regHal.Write32(MODS_FUSE_FUSECTRL_CMD_WRITE);

        CHECK_RC(PollFusesIdle());
    }

    // Pull down GPIO
    if (isFuseEnSet)
    {
        // Program the PS latch to switch off power supply to fuse macro
        CHECK_RC(ProgramPSLatch(false));

        pSubdev->GpioSetOutput(gpioNum, false);
    }

    MarkFuseReadDirty();

    CHECK_RC(VerifyFusing(fuseBinary));

    return rc;
}

RC GA10xFuseAccessor::ReadFuseBlock(vector<UINT32>* pReadFuses)
{
    RC rc;
    MASSERT(pReadFuses);
    auto& regs = m_pDev->Regs();

    // Disable reading from IFF ram by setting startIndex > endIndex
    const UINT32 savedIffStart = regs.Read32(0, MODS_FUSE_IFF_RECORD_START);
    const UINT32 savedIffLast  = regs.Read32(0, MODS_FUSE_IFF_RECORD_LAST);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_START, 1);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_LAST, 0);

    rc = AmpereFuseAccessor::ReadFuseBlock(pReadFuses);

    // Restore IFF ram indices
    regs.Write32(0, MODS_FUSE_IFF_RECORD_START, savedIffStart);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_LAST,  savedIffLast);

    return rc;
}

RC GA10xFuseAccessor::LatchFusesToOptRegs()
{
    // If the register to couple the resets is already priv protected, we will
    // not be able to resense fuses
    // GA100 allows a resense trigger + hot reset backup approach ; but since
    // LW_FUSE_PRIV2INTFC is timing out while sensing records on GA10x, we don't
    // have that backup

    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    return pSubdev->HotReset(GpuSubdevice::FundamentalResetOptions::Enable);
}

UINT32 GA10xFuseAccessor::GetFuseEnableGpioNum()
{
    return FUSE_EN_GPIO_NUM;
}

RC GA10xFuseAccessor::CheckPSDefaultState()
{
    RegHal& regHal = m_pDev->Regs();

    if (regHal.Test32(MODS_PGC6_BSI_FUSEPS18_CTRL_SET, 0) &&
        regHal.Test32(MODS_PGC6_BSI_FUSEPS18_CTRL_RESET, 1))
    {
        return RC::OK;
    }

    Printf(Tee::PriError, "PS18 power switch is not in its default state\n");
    return RC::SOFTWARE_ERROR;
}

RC GA10xFuseAccessor::ProgramPSLatch(bool bSwitchOn)
{
    RC rc;
    RegHal& regHal = m_pDev->Regs();
    if (!regHal.IsSupported(MODS_PGC6_BSI_FUSEPS18_CTRL))
    {
        Printf(Tee::PriError, "PS18 power switch is not supported on this chip\n");
        return RC::SOFTWARE_ERROR;
    }

    if (!regHal.HasRWAccess(MODS_PGC6_BSI_FUSEPS18_CTRL))
    {
        return RC::PRIV_LEVEL_VIOLATION;
    }

    if (bSwitchOn)
    {
        CHECK_RC(CheckPSDefaultState());
        // Bring PS18 power switch to hold state
        regHal.Write32(MODS_PGC6_BSI_FUSEPS18_CTRL_SET, 0);
        regHal.Write32(MODS_PGC6_BSI_FUSEPS18_CTRL_RESET, 0);

        // Set the PS18 power switch to turn on power for the fuse macro
        regHal.Write32(MODS_PGC6_BSI_FUSEPS18_CTRL_SET, 1);
    }
    else
    {
        // Bring PS18 power switch to hold state
        regHal.Write32(MODS_PGC6_BSI_FUSEPS18_CTRL_SET, 0);
        regHal.Write32(MODS_PGC6_BSI_FUSEPS18_CTRL_RESET, 0);

        // Reset the PS18 power switch to turn off power for the fuse macro
        regHal.Write32(MODS_PGC6_BSI_FUSEPS18_CTRL_RESET, 1);
     }

     // Turning off PS18 and next turning on PS18 should be longer than 10us
     Utility::SleepUS(PS18_RESET_DELAY_US);
     return rc;
}

RC GA10xFuseAccessor::SetupHwFusing()
{
    RegHal& regs = m_pDev->Regs();

    // Reset IFF start / end and clear SW override
    regs.Write32(0, MODS_FUSE_EN_SW_OVERRIDE_VAL_DISABLE);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_START, 0);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_LAST,  0);

    // Clear any cached values from the SW fusing
    MarkFuseReadDirty();

    return RC::OK;
}
