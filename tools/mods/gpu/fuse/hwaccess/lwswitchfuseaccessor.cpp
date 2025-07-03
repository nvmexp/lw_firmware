/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2018-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwswitchfuseaccessor.h"

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

LwSwitchFuseAccessor::LwSwitchFuseAccessor(TestDevice* pDev)
: m_pTestDev(pDev)
{
    MASSERT(pDev);
}

void LwSwitchFuseAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    m_pFuse = pFuse;
}

RC LwSwitchFuseAccessor::SetupFuseBlow()
{
    return EnableFuseSrc(FuseSource::TO_HIGH);
}

RC LwSwitchFuseAccessor::CleanupFuseBlow()
{
    return EnableFuseSrc(FuseSource::TO_LOW);
}

RC LwSwitchFuseAccessor::BlowFuses(const vector<UINT32>& fuseBinary)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
    {
        return RC::UNSUPPORTED_FUNCTION;
    }
    RC rc;
    auto& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    // Clear the fuse control register before starting
    regs.Write32(0, MODS_FUSE_FUSECTRL, 0);

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
            CHECK_RC(EnableFuseSrc(FuseSource::TO_HIGH));
        }

        regs.Write32(0, MODS_FUSE_FUSEADDR, row);
        regs.Write32(0, MODS_FUSE_FUSEWDATA, valToBurn);
        regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_WRITE);

        CHECK_RC(PollFusesIdle());
    }

    if (fuseSrcHigh)
    {
        CHECK_RC(EnableFuseSrc(FuseSource::TO_LOW));
    }

    MarkFuseReadDirty();

    CHECK_RC(VerifyFusing(fuseBinary));

    return rc;
}

RC LwSwitchFuseAccessor::SetSwFuses(const map<UINT32, UINT32>& swFuseWrites)
{
    // LwSwitch doesn't support SW Fusing
    return RC::UNSUPPORTED_FUNCTION;
}

RC LwSwitchFuseAccessor::GetFuseRow(UINT32 row, UINT32* pValue)
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

UINT32 LwSwitchFuseAccessor::GetOptFuseValue(UINT32 address)
{
    UINT32 fuseVal = 0;
    m_pTestDev->GetInterface<LwLinkRegs>()->RegRd(0, RegHalDomain::FUSE, address, &fuseVal);
    return fuseVal;
}

RC LwSwitchFuseAccessor::ReadFuseBlock(vector<UINT32>* pReadFuses)
{
    RC rc;
    MASSERT(pReadFuses);
    pReadFuses->clear();
    auto& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    UINT32 numFuseRows;
    CHECK_RC(GetNumFuseRows(&numFuseRows));
    for (UINT32 row = 0; row < numFuseRows; row++)
    {
        regs.Write32(0, MODS_FUSE_FUSEADDR, row);
        regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_READ);
        CHECK_RC(PollFusesIdle());
        pReadFuses->push_back(regs.Read32(0, MODS_FUSE_FUSERDATA));
    }
    return OK;
}

void LwSwitchFuseAccessor::DisableWaitForIdle()
{
    m_WaitForIdle = false;
}

bool LwSwitchFuseAccessor::IsFusePrivSelwrityEnabled()
{
    // We don't enable Fuse Priv Security on LwSwitch
    return false;
}

bool LwSwitchFuseAccessor::IsSwFusingAllowed()
{
    // We don't allow SW Fusing on LwSwitch
    return false;
}

RC LwSwitchFuseAccessor::EnableFuseSrc(bool setHigh)
{
    RC rc;
    JavaScriptPtr pJs;
    jsval retval;

    JsArray arg(1);
    CHECK_RC(pJs->ToJsval(setHigh, &arg[0]));

    CHECK_RC(pJs->CallMethod(pJs->GetGlobalObject(), "FuseSrcToggle", arg, &retval));

    UINT32 rcNum;
    CHECK_RC(pJs->FromJsval(retval, &rcNum));
    return RC(rcNum);
}

RC LwSwitchFuseAccessor::PollFusesIdle()
{
    RC rc;
    if (m_WaitForIdle)
    {
        CHECK_RC(Tasker::PollHw(
            Tasker::FixTimeout(FuseUtils::GetTimeoutMs()),
            [=]() -> bool
            {
                return m_pTestDev->GetInterface<LwLinkRegs>()->Regs().Test32(0,
                                                            MODS_FUSE_FUSECTRL_STATE_IDLE);
            },
            __FUNCTION__));
    }
    return rc;
}

RC LwSwitchFuseAccessor::VerifyFusing(const vector<UINT32>& expectedBinary)
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

RC LwSwitchFuseAccessor::LatchFusesToOptRegs()
{
    auto& regs = m_pTestDev->GetInterface<LwLinkRegs>()->Regs();

    regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_SENSE_CTRL);

    // Reset control registers
    regs.Write32(0, MODS_FUSE_PRIV2INTFC_START_DATA, 1);
    Platform::Delay(1);
    regs.Write32(0, MODS_FUSE_PRIV2INTFC_START_DATA, 0);
    Platform::Delay(4);
    return Tasker::PollHw(
        Tasker::FixTimeout(FuseUtils::GetTimeoutMs()),
        [=]() -> bool
        {
            return m_pTestDev->GetInterface<LwLinkRegs>()->Regs().Test32(0,
                                                        MODS_FUSE_FUSECTRL_FUSE_SENSE_DONE_YES);
        },
        __FUNCTION__);
}

RC LwSwitchFuseAccessor::GetNumFuseRows(UINT32 *pNumFuseRows) const
{
    MASSERT(pNumFuseRows);
    RC rc;

    auto fuseFile = m_pFuse->GetFuseFilename();
    auto pParser = unique_ptr<FuseParser>(FuseParser::CreateFuseParser(fuseFile));
    const FuseUtil::MiscInfo* pMiscInfo = nullptr;
    CHECK_RC(pParser->ParseFile(fuseFile, nullptr, nullptr, &pMiscInfo));
    
    *pNumFuseRows = pMiscInfo->MacroInfo.FuseRows; 
    return RC::OK;
}
