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

#include "amperefuseaccessor.h"

#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/fuse/fuse.h"
#include "gpu/reghal/reghal.h"

AmpereFuseAccessor::AmpereFuseAccessor(TestDevice* pDev)
    : GpuFuseAccessor(pDev),
      m_pDev(pDev),
      m_pFuse(nullptr)
{
    MASSERT(pDev);
    MASSERT(dynamic_cast<GpuSubdevice*>(pDev) != nullptr);
}

void AmpereFuseAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    GpuFuseAccessor::SetParent(pFuse);
    m_pFuse = pFuse;
}

RC AmpereFuseAccessor::SetupFuseBlow()
{
    RC rc;
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    pSubdev->GpioSetOutput(GetFuseEnableGpioNum(), true);
    return rc;
}

RC AmpereFuseAccessor::CleanupFuseBlow()
{
    RC rc;
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    pSubdev->GpioSetOutput(GetFuseEnableGpioNum(), false);
    return rc;
}

RC AmpereFuseAccessor::BlowFuses(const vector<UINT32>& fuseBinary)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    RegHal& regs = m_pDev->Regs();

    CHECK_RC(SetupHwFusing());

    // Clear the fuse control register before starting
    regs.Write32(MODS_FUSE_FUSECTRL, 0);

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
            isFuseEnSet = true;
        }

        regs.Write32(MODS_FUSE_FUSEADDR, row);
        regs.Write32(MODS_FUSE_FUSEWDATA, valToBurn);
        regs.Write32(MODS_FUSE_FUSECTRL_CMD_WRITE);

        CHECK_RC(PollFusesIdle());
    }

    // Pull down GPIO
    if (isFuseEnSet)
    {
        pSubdev->GpioSetOutput(gpioNum, false);
    }

    MarkFuseReadDirty();

    CHECK_RC(VerifyFusing(fuseBinary));

    return rc;
}

RC AmpereFuseAccessor::SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc /* unused */)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    RegHal& regs = m_pDev->Regs();

    // Set the redirection bit for SW IFF fusing
    regs.Write32(MODS_FUSE_EN_SW_OVERRIDE_VAL_ENABLE);

    const UINT32 numIffRows = static_cast<UINT32>(iffRows.size());

    const auto fuseFile = m_pFuse->GetFuseFilename();
    FuseUtil::FuselessFuseInfo rcdInfo;

    CHECK_RC(FuseUtil::GetFuseMacroInfo(fuseFile, &rcdInfo, nullptr,
                m_pDev->HasFeature(Device::GPUSUB_HAS_MERGED_FUSE_MACROS)));
    MASSERT(numIffRows <= (rcdInfo.fuselessEndRow - rcdInfo.fuselessStartRow + 1));

    // Set IFF record start and end values
    const UINT32 lastIndex  = rcdInfo.fuselessEndRow;
    const UINT32 startIndex = lastIndex - numIffRows + 1;
    regs.Write32(MODS_FUSE_IFF_RECORD_START, startIndex);
    regs.Write32(MODS_FUSE_IFF_RECORD_LAST,  lastIndex);

    // Write IFF records
    CHECK_RC(PollSwIffIdle());
    for (UINT32 row = startIndex; row <= lastIndex; row++)
    {
        regs.Write32(MODS_FUSE_FUSEADDR,  row);
        regs.Write32(MODS_FUSE_FUSEWDATA, iffRows[row - startIndex]);
        regs.Write32(MODS_FUSE_IFF_SW_FUSING_VAL_ENABLE);
        CHECK_RC(PollSwIffIdle());
    }

    // Read back data to verify
    for (UINT32 row = startIndex; row <= lastIndex; row++)
    {
        regs.Write32(MODS_FUSE_FUSEADDR, row);
        regs.Write32(MODS_FUSE_FUSECTRL_CMD_READ);
        CHECK_RC(PollFusesIdle());
        const UINT32 readBack = regs.Read32(MODS_FUSE_FUSERDATA);
        if (readBack != iffRows[row - startIndex])
        {
            Printf(Tee::PriError, "SW IFF write [%u] = 0x%x failed: 0x%x\n",
                                   row, iffRows[row - startIndex], readBack);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    return rc;
}

bool AmpereFuseAccessor::IsSwFusingAllowed()
{
    UINT32 swOvrVal;
    RC rc = m_pFuse->GetFuseValue("disable_sw_override", FuseFilter::ALL_FUSES, &swOvrVal);
    return rc == OK && swOvrVal == 0;
}

RC AmpereFuseAccessor::LatchFusesToOptRegs()
{
    RC rc;
    RegHal& regs = m_pDev->Regs();

    // To resense the fuse macro, we need to trigger a fundamental reset
    // Ampere onwards, hot reset and fundamental reset are being decoupled by default
    // and the related register is priv protected
    // However, the PLM of the register would be raised only after the fuse
    // opt_selwre_xve_reset_ctrl_wr_selwre is sensed
    // So we can couple the bits before the resensing to trigger a fundamental reset
    // with the next hot reset
    if (regs.HasRWAccess(MODS_XTL_EP_PRI_RESET))
    {
        regs.Write32(MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_RESET);
    }
    else if (regs.Test32(MODS_XTL_EP_PRI_RESET_ASSERT_FUNDAMENTAL_RESET_ON_HOT_RESET_NORESET))
    {
        Printf(Tee::PriWarn, "Can't couple hot and fundamental reset for resensing\n");
    }

    // Technically we need a fundamental reset to cleanly resense fuses + set HW state
    // In the (highly unlikely) event of the coupling of the resets being protected with
    // L0 HW fusing still being allowed on the chip, resense fuses + trigger a hot reset
    // instead

    CHECK_RC(GpuFuseAccessor::LatchFusesToOptRegs());

    // We can't depend on this function to couple the bits since PLMs might be raised
    // after the fuses were sensed in the above step
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    CHECK_RC(pSubdev->HotReset(GpuSubdevice::FundamentalResetOptions::Default));

    return rc;
}

UINT32 AmpereFuseAccessor::GetFuseEnableGpioNum()
{
    return FUSE_EN_GPIO_NUM;
}

RC AmpereFuseAccessor::PollSwIffIdle()
{
    RC rc;
    rc = Tasker::PollHw(
            [](void *pRegs) { return static_cast<RegHal*>(pRegs)->Test32(
                              MODS_FUSE_IFF_SW_FUSING_VAL_DISABLE); },
            &m_pDev->Regs(), Tasker::FixTimeout(FuseUtils::GetTimeoutMs()),
            __FUNCTION__, "IFF SWFUSE IDLE");
    if (rc == RC::TIMEOUT_ERROR &&
        Platform::GetSimulationMode() != Platform::Hardware)
    {
            // Ignore timeouts in simulation, which may not support
            // LW_FUSE_IFF_SW_FUSING
            rc.Clear();
    }
    return rc;
}

RC AmpereFuseAccessor::SetupHwFusing()
{
    RC rc;
    RegHal& regs = m_pDev->Regs();

    // Reset IFF start / end and clear SW override
    regs.Write32(MODS_FUSE_EN_SW_OVERRIDE_VAL_DISABLE);
    regs.Write32(MODS_FUSE_IFF_RECORD_START, 0);
    regs.Write32(MODS_FUSE_IFF_RECORD_LAST,  0);

    return rc;
}
