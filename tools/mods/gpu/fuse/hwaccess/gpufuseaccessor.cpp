/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "gpufuseaccessor.h"

#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/fuse/fuse.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghal.h"

GpuFuseAccessor::GpuFuseAccessor(TestDevice* pDev)
    : m_pDev(pDev),
      m_pFuse(nullptr)
{
    MASSERT(pDev);
    MASSERT(dynamic_cast<GpuSubdevice*>(pDev) != nullptr);
}

void GpuFuseAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    m_pFuse = pFuse;
}

RC GpuFuseAccessor::SetupFuseBlow()
{
    RC rc;
    FuseSource fuseSrc;
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    CHECK_RC(fuseSrc.Initialize(pSubdev, "jsfunc_FuseSrcToggle"));
    CHECK_RC(fuseSrc.Toggle(FuseSource::TO_HIGH));
    return rc;
}

RC GpuFuseAccessor::CleanupFuseBlow()
{
    RC rc;
    FuseSource fuseSrc;
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    CHECK_RC(fuseSrc.Initialize(pSubdev, "jsfunc_FuseSrcToggle"));
    CHECK_RC(fuseSrc.Toggle(FuseSource::TO_LOW));
    return rc;
}

RC GpuFuseAccessor::BlowFuses(const vector<UINT32>& fuseBinary)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC rc;
    RegHal& regs = m_pDev->Regs();

    // Clear the fuse control register before starting
    regs.Write32(MODS_FUSE_FUSECTRL, 0);

    // Set up the function to toggle the daughter card
    FuseSource fuseSrc;
    GpuSubdevice *pSubdev = static_cast<GpuSubdevice*>(m_pDev);
    CHECK_RC(fuseSrc.Initialize(pSubdev, "jsfunc_FuseSrcToggle"));

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
    bool fuseSrcHigh = false;

    for (UINT32 row = 0; row < fuseBinary.size(); row++)
    {
        // Only burn bits going from 0 to 1
        UINT32 valToBurn = fuseBinary[row] & ~cachedFuses[row];
        if (valToBurn == 0)
        {
            continue;
        }

        if (!fuseSrcHigh)
        {
            CHECK_RC(fuseSrc.Toggle(FuseSource::TO_HIGH));
            fuseSrcHigh = true;
        }

        regs.Write32(MODS_FUSE_FUSEADDR, row);
        regs.Write32(MODS_FUSE_FUSEWDATA, valToBurn);
        regs.Write32(MODS_FUSE_FUSECTRL_CMD_WRITE);

        CHECK_RC(PollFusesIdle());
    }

    if (fuseSrcHigh)
    {
        CHECK_RC(fuseSrc.Toggle(FuseSource::TO_LOW));
        fuseSrcHigh = false;
    }

    MarkFuseReadDirty();

    CHECK_RC(VerifyFusing(fuseBinary));

    return rc;
}

RC GpuFuseAccessor::SetSwFuses(const map<UINT32, UINT32>& swFuseWrites)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    StickyRC rc;
    m_pDev->Regs().Write32(MODS_FUSE_EN_SW_OVERRIDE_VAL_ENABLE);

    for (const auto& addrValPair : swFuseWrites)
    {
        m_pDev->RegWr32(addrValPair.first, addrValPair.second);
        
        UINT32 readBack = m_pDev->RegRd32(addrValPair.first);
        if (readBack == 0xBADF510C)
        {
            // In case burning some other fuse prior to this one caused raised read
            // level protections. Eg. http://lwbugs.lwpu.com/3522799
            Printf(Tee::PriWarn, "Priv-level violation while verifying [0x%08x]\n",
                                  addrValPair.first);
        }
        else if (readBack != addrValPair.second)
        {
            Printf(Tee::PriError, "SW Fuse write [0x%08x] = 0x%08x failed: 0x%08x\n",
                addrValPair.first, addrValPair.second, readBack);
            rc = RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }
    return rc;
}

RC GpuFuseAccessor::GetFuseRow(UINT32 row, UINT32* pValue)
{
    MASSERT(pValue);
    RC rc;
    const vector<UINT32>* pFuseBlock = nullptr;
    CHECK_RC(GetFuseBlock(&pFuseBlock));
    if (row >= pFuseBlock->size())
    {
        Printf(Tee::PriError, "Requested row outside of fuse block: %d\n", row);
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    *pValue = (*pFuseBlock)[row];
    return rc;
}

UINT32 GpuFuseAccessor::GetOptFuseValue(UINT32 address)
{
    return m_pDev->RegRd32(address);
}

RC GpuFuseAccessor::ReadFuseBlock(vector<UINT32>* pReadFuses)
{
    RC rc;
    MASSERT(pReadFuses);
    pReadFuses->clear();
    RegHal& regs = m_pDev->Regs();

    UINT32 numFuseRows;
    CHECK_RC(GetNumFuseRows(&numFuseRows));

    for (UINT32 row = 0; row < numFuseRows; row++)
    {
        regs.Write32(MODS_FUSE_FUSEADDR, row);
        regs.Write32(MODS_FUSE_FUSECTRL_CMD_READ);
        CHECK_RC(PollFusesIdle());
        pReadFuses->push_back(regs.Read32(MODS_FUSE_FUSERDATA));
    }
    return OK;
}

RC GpuFuseAccessor::PollFusesIdle()
{
    RC rc;
    if (m_WaitForIdle)
    {
        rc = Tasker::PollHw(
                [](void *pRegs) { return static_cast<RegHal*>(pRegs)->Test32(
                                MODS_FUSE_FUSECTRL_STATE_IDLE); },
                &m_pDev->Regs(), Tasker::FixTimeout(FuseUtils::GetTimeoutMs()),
                __FUNCTION__, "FUSECTRL IDLE");
        if (rc == RC::TIMEOUT_ERROR &&
            Platform::GetSimulationMode() != Platform::Hardware)
        {
            // Ignore timeouts in simulation, which may not support
            // LW_FUSECTRL_STATE_IDLE
            rc.Clear();
        }
    }
    return rc;
}

void GpuFuseAccessor::DisableWaitForIdle()
{
    m_WaitForIdle = false;
}

bool GpuFuseAccessor::IsFusePrivSelwrityEnabled()
{
    RegHal& regs = m_pDev->Regs();
    // Fuse security is enabled if Priv Security is on and either
    // FUSEWDATA or FUSECTRL is locked down
    return regs.Test32(MODS_FUSE_OPT_PRIV_SEC_EN_DATA_YES) &&
              (regs.Test32(MODS_FUSE_WDATA_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_DISABLE) ||
               regs.Test32(MODS_FUSE_FUSECTRL_PRIV_LEVEL_MASK_WRITE_PROTECTION_LEVEL0_DISABLE));
}

bool GpuFuseAccessor::IsSwFusingAllowed()
{
    UINT32 swOvrVal;
    RC rc = m_pFuse->GetFuseValue("sw_override_fuse", FuseFilter::ALL_FUSES, &swOvrVal);
    return rc == OK && swOvrVal == 0;
}

RC GpuFuseAccessor::VerifyFusing(const vector<UINT32>& expectedBinary)
{
    StickyRC rc;
    const vector<UINT32>* pLwrrentFuses = nullptr;
    CHECK_RC(GetFuseBlock(&pLwrrentFuses));
    MASSERT(expectedBinary.size() == pLwrrentFuses->size());

    for (UINT32 row = 0; row < pLwrrentFuses->size(); row++)
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
    return rc;
}

RC GpuFuseAccessor::LatchFusesToOptRegs()
{
    RegHal& regs = m_pDev->Regs();
    // Make sure sw fusing isn't enabled
    regs.Write32(MODS_FUSE_EN_SW_OVERRIDE_VAL_DISABLE);

    regs.Write32(MODS_FUSE_FUSECTRL_CMD_SENSE_CTRL);

    // Reset control registers
    regs.Write32(MODS_FUSE_PRIV2INTFC_START_DATA, 1);
    Platform::Delay(1);
    regs.Write32(MODS_FUSE_PRIV2INTFC_START_DATA, 0);
    Platform::Delay(4);
    return Tasker::PollHw(
        Tasker::FixTimeout(FuseUtils::GetTimeoutMs()),
        [=]() -> bool { return m_pDev->Regs().Test32(MODS_FUSE_FUSECTRL_FUSE_SENSE_DONE_YES); },
        __FUNCTION__);
}

RC GpuFuseAccessor::SwReset()
{
    return static_cast<GpuSubdevice*>(m_pDev)->GetFsImpl()->SwReset(false);
}

RC GpuFuseAccessor::LowerFuseSelwrity()
{
    Printf(Tee::PriWarn, "Lowering fuse security is not implemented yet.\n");
    return OK;
}

RC GpuFuseAccessor::GetNumFuseRows(UINT32 *pNumFuseRows) const
{
    MASSERT(pNumFuseRows);
    RC rc;

    auto fuseFile = m_pFuse->GetFuseFilename();
    auto pParser = unique_ptr<FuseParser>(FuseParser::CreateFuseParser(fuseFile));
    const FuseUtil::MiscInfo* pMiscInfo = nullptr;
    CHECK_RC(pParser->ParseFile(fuseFile, nullptr, nullptr, &pMiscInfo));

    switch (static_cast<GpuSubdevice*>(m_pDev)->DeviceId())
    {
        case Gpu::GM108:
        case Gpu::GM107:
        case Gpu::GM200:
        case Gpu::GM204:
        case Gpu::GM206:
            *pNumFuseRows = (pMiscInfo->FuselessEnd + 1) / 32;
            break;
        default:
            *pNumFuseRows = pMiscInfo->MacroInfo.FuseRows;
    }

    return RC::OK;
}

RC GpuFuseAccessor::SetFusingVoltage(UINT32 vddMv)
{
    RC rc;
    auto& regs = m_pDev->Regs();

    if (vddMv > 900 || vddMv < 675)
    {
        Printf(Tee::PriError, "Requested lwvdd not supported.\n");
        return RC::ILWALID_ARGUMENT;
    }

    // We can only write these registers when security is disabled
    if (IsFusePrivSelwrityEnabled())
    {
        return RC::UNSUPPORTED_FUNCTION;
    }

    // These should be taken from the vbios, but is (historically) constant
    const UINT32 baseMv = 300;
    const UINT32 rangeMv = 1000;

    // Step 1: Set up GPIO that drives LWVDD (should be taken from DCB but is always GPIO 0)
    if (regs.IsSupported(MODS_PMGR_GPIO_OUTPUT_CNTL))
    {
        regs.Write32(MODS_PMGR_GPIO_OUTPUT_CNTL_SEL_VID_PWM, 0);
        regs.Write32(MODS_PMGR_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES, 0);
    }
    else
    {
        regs.Write32(MODS_GPIO_OUTPUT_CNTL_SEL_VID_PWM, 0);
        regs.Write32(MODS_GPIO_OUTPUT_CNTL_IO_OUT_EN_YES, 0);
    }
    regs.Write32(MODS_PMGR_GPIO_OUTPUT_CNTL_TRIGGER_UPDATE_TRIGGER);

    // Step 2: Configure period
    // - The period is in units of utilsclk.
    // - It just needs to be a number that works with the voltage callwlation,
    //   but this specific one was chosen based on the vbios value
    // - The vbios has a field called "PWM Input Frequency" which is set to 675 khz.
    //   At boot, this is 108MhZ = 9.259ns.
    //   So we program this field to 675khz as well, which is 108Mhz / 160.
    const UINT32 periodUtilsClk = 160;
    regs.Write32(MODS_THERM_VID0_PWM_PERIOD, 0, periodUtilsClk);

    // Step 3: Callwlate HI value
    // HI = period * (target - base) / range
    // Note that the step values will not be truncated, but no guarantees for other voltages
    UINT32 hi = periodUtilsClk * (vddMv - baseMv) / rangeMv;
    UINT32 actualvddMv = (hi * rangeMv) / periodUtilsClk + baseMv;
    regs.Write32(MODS_THERM_VID1_PWM_HI, 0, hi);

    Printf(FuseUtils::GetPrintPriority(),
           "Setting lwvdd to %d mV via direct writes\n", actualvddMv);

    // Step 4: Trigger and wait for complete
    regs.Write32(MODS_THERM_VID_PWM_TRIGGER_MASK_0_SETTING_NEW_TRIGGER);
    CHECK_RC(Tasker::PollHw(
        FuseUtils::GetTimeoutMs(),
        [&regs]() -> bool {
        return regs.Test32(MODS_THERM_VID_PWM_TRIGGER_MASK_0_SETTING_NEW_DONE); },
        __FUNCTION__));

    return RC::OK;
}
