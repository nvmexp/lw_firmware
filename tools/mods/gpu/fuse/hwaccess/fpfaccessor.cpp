/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2019 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "fpfaccessor.h"

#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "gpu/include/testdevice.h"
#include "gpu/fuse/fuse.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghal.h"

FpfAccessor::FpfAccessor(TestDevice* pDev)
    : m_pDev(pDev)
    , m_pFuse(nullptr)
{
    MASSERT(pDev != nullptr);
}

void FpfAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    m_pFuse = pFuse;
}

RC FpfAccessor::SetupFuseBlow()
{
    m_pDev->Regs().Write32(MODS_FPF_DEBUGCTRL_FUSE_SRC_EN_ENABLE);
    // Sleep for 100ms to let the voltage shift
    Tasker::Sleep(100);
    return RC::OK;
}

RC FpfAccessor::CleanupFuseBlow()
{
    m_pDev->Regs().Write32(MODS_FPF_DEBUGCTRL_FUSE_SRC_EN_DISABLE);
    return RC::OK;
}

RC FpfAccessor::BlowFuses(const vector<UINT32>& fuseBinary)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC rc;
    RegHal& regs = m_pDev->Regs();

    // Clear the fuse control register before starting
    regs.Write32(MODS_FPF_FUSECTRL, 0);

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
            regs.Write32(MODS_FPF_DEBUGCTRL_FUSE_SRC_EN_ENABLE);
            // Sleep for 100ms to let the voltage shift
            Tasker::Sleep(100);
            CHECK_RC(PollFusesIdle());
            fuseSrcHigh = true;
        }

        regs.Write32(MODS_FPF_FUSEADDR, row);
        regs.Write32(MODS_FPF_FUSEWDATA, valToBurn);
        regs.Write32(MODS_FPF_FUSECTRL_CMD_WRITE);

        CHECK_RC(PollFusesIdle());
    }

    regs.Write32(MODS_FPF_DEBUGCTRL_FUSE_SRC_EN_DISABLE);
    fuseSrcHigh = false;

    MarkFuseReadDirty();

    CHECK_RC(VerifyFusing(fuseBinary));

    return rc;
}

RC FpfAccessor::SetSwFuses(const map<UINT32, UINT32>& swFuseWrites)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    StickyRC rc;
    m_pDev->Regs().Write32(MODS_FPF_EN_SW_OVERRIDE_VAL_ENABLE);

    for (const auto& addrValPair : swFuseWrites)
    {
        m_pDev->RegWr32(addrValPair.first, addrValPair.second);
        if (m_pDev->RegRd32(addrValPair.first) != addrValPair.second)
        {
            Printf(Tee::PriError, "SW Fuse write [0x%08x] = 0x%08x failed\n",
                addrValPair.first, addrValPair.second);
            rc = RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }
    return rc;
}

RC FpfAccessor::GetFuseRow(UINT32 row, UINT32* pValue)
{
    MASSERT(pValue != nullptr);
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

UINT32 FpfAccessor::GetOptFuseValue(UINT32 address)
{
    return m_pDev->RegRd32(address);
}

RC FpfAccessor::ReadFuseBlock(vector<UINT32>* pReadFuses)
{
    RC rc;
    MASSERT(pReadFuses != nullptr);
    pReadFuses->clear();
    RegHal& regs = m_pDev->Regs();

    UINT32 numRows;
    CHECK_RC(GetNumFuseRows(&numRows));
    for (UINT32 row = 0; row < numRows; row++)
    {
        regs.Write32(MODS_FPF_FUSEADDR, row);
        regs.Write32(MODS_FPF_FUSECTRL_CMD_READ);
        CHECK_RC(PollFusesIdle());
        pReadFuses->push_back(regs.Read32(MODS_FPF_FUSERDATA));
    }
    return OK;
}

RC FpfAccessor::PollFusesIdle()
{
    RC rc;
    if (m_WaitForIdle)
    {
        rc = Tasker::PollHw(
                [](void *pRegs) { return static_cast<RegHal*>(pRegs)->Test32(
                                MODS_FPF_FUSECTRL_STATE_IDLE); },
                &m_pDev->Regs(), Tasker::FixTimeout(FuseUtils::GetTimeoutMs()),
                __FUNCTION__, "FPF FUSECTRL IDLE");
        if (rc == RC::TIMEOUT_ERROR &&
            Platform::GetSimulationMode() != Platform::Hardware)
        {
            // Ignore timeouts in simulation, which may not support
            // LW_FPF_FUSECTRL_STATE_IDLE
            rc.Clear();
        }
    }
    return rc;
}

void FpfAccessor::DisableWaitForIdle()
{
    m_WaitForIdle = false;
}

bool FpfAccessor::IsSwFusingAllowed()
{
    UINT32 swOvrVal;
    RC rc = m_pFuse->GetFuseValue("fpf_sw_override_fuse", FuseFilter::ALL_FUSES, &swOvrVal);
    return rc == OK && swOvrVal == 0;
}

RC FpfAccessor::VerifyFusing(const vector<UINT32>& expectedBinary)
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

RC FpfAccessor::LatchFusesToOptRegs()
{
    RegHal& regs = m_pDev->Regs();
    // Make sure sw fusing isn't enabled
    regs.Write32(MODS_FPF_EN_SW_OVERRIDE_VAL_DISABLE);

    regs.Write32(MODS_FPF_FUSECTRL_CMD_SENSE_CTRL);

    // Reset control registers
    regs.Write32(MODS_FPF_PRIV2INTFC_START_DATA, 1);
    Platform::Delay(1);
    regs.Write32(MODS_FPF_PRIV2INTFC_START_DATA, 0);
    Platform::Delay(4);
    return Tasker::PollHw(
        Tasker::FixTimeout(FuseUtils::GetTimeoutMs()),
        [=]() -> bool { return m_pDev->Regs().Test32(MODS_FPF_FUSECTRL_FUSE_SENSE_DONE_YES); },
        __FUNCTION__);
}
    
RC FpfAccessor::GetNumFuseRows(UINT32 *pNumFuseRows) const
{
    MASSERT(pNumFuseRows);
    RC rc;

    auto fuseFile = m_pFuse->GetFuseFilename();
    auto pParser = unique_ptr<FuseParser>(FuseParser::CreateFuseParser(fuseFile));
    const FuseUtil::MiscInfo* pMiscInfo = nullptr;
    CHECK_RC(pParser->ParseFile(fuseFile, nullptr, nullptr, &pMiscInfo));
            
    *pNumFuseRows = pMiscInfo->fpfMacroInfo.FuseRows;
    return RC::OK;
}
