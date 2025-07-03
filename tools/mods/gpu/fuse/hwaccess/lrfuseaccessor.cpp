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

#include "lrfuseaccessor.h"

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

LrFuseAccessor::LrFuseAccessor(TestDevice* pDev)
    : LwSwitchFuseAccessor(pDev),
      m_pTestDev(pDev),
      m_pFuse(nullptr)
{
    MASSERT(pDev);
}

void LrFuseAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    LwSwitchFuseAccessor::SetParent(pFuse);
    m_pFuse = pFuse;
}

RC LrFuseAccessor::SetupFuseBlow()
{
    GpioSetOutput(GetFuseEnableGpioNum(), true);
    return RC::OK;
}

RC LrFuseAccessor::CleanupFuseBlow()
{
    GpioSetOutput(GetFuseEnableGpioNum(), false);
    return RC::OK;
}

RC LrFuseAccessor::BlowFuses(const vector<UINT32>& fuseBinary)
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
            SetupFuseBlow();
            isFuseEnSet = true;
        }

        regs.Write32(0, MODS_FUSE_FUSEADDR, row);
        regs.Write32(0, MODS_FUSE_FUSEWDATA, valToBurn);
        regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_WRITE);

        CHECK_RC(PollFusesIdle());
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

RC LrFuseAccessor::ReadFuseBlock(vector<UINT32>* pReadFuses)
{
    RC rc;
    MASSERT(pReadFuses);
    auto& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    // Disable reading from IFF ram by setting startIndex > endIndex
    const UINT32 savedIffStart = regs.Read32(0, MODS_FUSE_IFF_RECORD_START);
    const UINT32 savedIffLast  = regs.Read32(0, MODS_FUSE_IFF_RECORD_LAST);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_START, 1);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_LAST, 0);

    rc = LwSwitchFuseAccessor::ReadFuseBlock(pReadFuses);

    // Restore IFF ram indices
    regs.Write32(0, MODS_FUSE_IFF_RECORD_START, savedIffStart);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_LAST,  savedIffLast);

    return rc;
}


RC LrFuseAccessor::SetSwFuses(const map<UINT32, UINT32>& swFuseWrites)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    StickyRC rc;
    RegHal& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();
    regs.Write32(0, MODS_FUSE_EN_SW_OVERRIDE_VAL_ENABLE);

    for (const auto& addrValPair : swFuseWrites)
    {
        UINT32 readBack = 0;
        m_pTestDev->GetInterface<LwLinkRegs>()->RegWr(0, RegHalDomain::FUSE,
                                                  addrValPair.first, addrValPair.second);
        m_pTestDev->GetInterface<LwLinkRegs>()->RegRd(0, RegHalDomain::FUSE,
                                                  addrValPair.first, &readBack);
        if (readBack != addrValPair.second)
        {
            Printf(Tee::PriError, "SW Fuse write [0x%08x] = 0x%08x failed: 0x%08x\n",
                                   addrValPair.first, addrValPair.second, readBack);
            rc = RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }
    return rc;
}

RC LrFuseAccessor::SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc /* unused */)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    RC rc;
    RegHal& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    // Set the redirection bit for SW IFF fusing
    regs.Write32(0, MODS_FUSE_EN_SW_OVERRIDE_VAL_ENABLE);

    FuseUtil::FuselessFuseInfo rcdInfo;
    const UINT32 numIffRows = static_cast<UINT32>(iffRows.size());
    CHECK_RC(FuseUtil::GetFuseMacroInfo(m_pFuse->GetFuseFilename(), &rcdInfo, nullptr,
                       m_pTestDev->HasFeature(Device::GPUSUB_HAS_MERGED_FUSE_MACROS)));
    // The IFF records are a part of the fuseless fuse section, hence the size
    // must not exceed the entire fuseless fuse section
    MASSERT(numIffRows <= (rcdInfo.fuselessEndRow - rcdInfo.fuselessStartRow + 1));

    // Set IFF record start and end values
    const UINT32 lastIndex  = rcdInfo.fuselessEndRow;
    const UINT32 startIndex = lastIndex - numIffRows + 1;
    regs.Write32(0, MODS_FUSE_IFF_RECORD_START, startIndex);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_LAST,  lastIndex);

    // Write IFF records
    CHECK_RC(PollSwIffIdle());
    for (UINT32 row = startIndex; row <= lastIndex; row++)
    {
        regs.Write32(0, MODS_FUSE_FUSEADDR,  row);
        regs.Write32(0, MODS_FUSE_FUSEWDATA, iffRows[row - startIndex]);
        regs.Write32(0, MODS_FUSE_IFF_SW_FUSING_VAL_ENABLE);
        CHECK_RC(PollSwIffIdle());
    }

    // Read back data to verify
    for (UINT32 row = startIndex; row <= lastIndex; row++)
    {
        regs.Write32(0, MODS_FUSE_FUSEADDR, row);
        regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_READ);
        CHECK_RC(PollFusesIdle());
        const UINT32 readBack = regs.Read32(0, MODS_FUSE_FUSERDATA);
        if (readBack != iffRows[row - startIndex])
        {
            Printf(Tee::PriError, "SW IFF write [%u] = 0x%x failed: 0x%x\n",
                                   row, iffRows[row - startIndex], readBack);
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    return rc;
}

bool LrFuseAccessor::IsSwFusingAllowed()
{
    UINT32 swOvrVal;
    // Fuse sensing on GA100 requires a different LWVDD, so just read the OPT fuse value instead
    RC rc = m_pFuse->GetFuseValue("disable_sw_override", FuseFilter::OPT_FUSES_ONLY, &swOvrVal);
    return rc == OK && swOvrVal == 0;
}

UINT32 LrFuseAccessor::GetOptFuseValue(UINT32 address)
{
    UINT32 fuseVal = 0;
    m_pTestDev->GetInterface<LwLinkRegs>()->RegRd(0, RegHalDomain::FUSE, address, &fuseVal);
    return fuseVal;
}

RC LrFuseAccessor::SwReset()
{
    // We use a hot reset (without a fundamental reset) to reset the chip
    // to SW fused settings
    return m_pTestDev->HotReset(TestDevice::FundamentalResetOptions::Disable);
}

UINT32 LrFuseAccessor::GetFuseEnableGpioNum()
{
    return FUSE_EN_GPIO_NUM;
}

void LrFuseAccessor::GpioSetOutput(UINT32 gpioNum, bool toHigh)
{
    RegHal& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    // First set the direction to output
    regs.Write32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES, gpioNum);
    // Then set the output
    regs.Write32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUTPUT, gpioNum, toHigh ? 1 : 0);
    // Trigger the GPIO change
    regs.Write32(0, MODS_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER);
}

RC LrFuseAccessor::PollSwIffIdle()
{
    RC rc;
    rc = Tasker::PollHw(
            [](void *pRegs) { return static_cast<RegHal*>(pRegs)->Test32(
                              MODS_FUSE_IFF_SW_FUSING_VAL_DISABLE); },
            &m_pTestDev->GetInterface<LwLinkRegs>()->Regs(),
            Tasker::FixTimeout(FuseUtils::GetTimeoutMs()),
            __FUNCTION__,
            "IFF SWFUSE IDLE");
    if (rc == RC::TIMEOUT_ERROR &&
        Platform::GetSimulationMode() != Platform::Hardware)
    {
            // Ignore timeouts in simulation, which may not support
            // LW_FUSE_IFF_SW_FUSING
            rc.Clear();
    }
    return rc;
}

RC LrFuseAccessor::SetupHwFusing()
{
    RegHal& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    // Reset IFF start / end and clear SW override
    regs.Write32(0, MODS_FUSE_EN_SW_OVERRIDE_VAL_DISABLE);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_START, 0);
    regs.Write32(0, MODS_FUSE_IFF_RECORD_LAST,  0);

    // Clear any cached values from the SW fusing
    MarkFuseReadDirty();

    return RC::OK;
}
