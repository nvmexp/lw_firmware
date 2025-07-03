/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019,2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "dummyfuseaccessor.h"

#include "core/include/platform.h"
#include "core/include/tasker.h"
#include "gpu/include/gpusbdev.h"
#include "gpu/include/testdevice.h"
#include "gpu/fuse/fuse.h"
#include "mods_reg_hal.h"
#include "gpu/reghal/reghal.h"

DummyFuseAccessor::DummyFuseAccessor(TestDevice* pDev, UINT32 numRows)
{
    MASSERT(pDev);
    m_pFuse   = nullptr;
    m_pDev    = pDev;
    m_NumRows = numRows;
    m_HwFuses.resize(numRows);
    m_SwFusesInitialized = false;
}

void DummyFuseAccessor::SetParent(Fuse* pFuse)
{
    MASSERT(pFuse != nullptr);
    m_pFuse = pFuse;
}

RC DummyFuseAccessor::SetupFuseBlow()
{
    Printf(FuseUtils::GetVerbosePriority(), "Setting up fuseblow\n");
    return RC::OK;
}

RC DummyFuseAccessor::CleanupFuseBlow()
{
    Printf(FuseUtils::GetVerbosePriority(), "Cleaning up fuseblow\n");
    return RC::OK;
}

RC DummyFuseAccessor::BlowFuses(const vector<UINT32>& fuseBinary)
{
    RC rc;

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

    for (UINT32 row = 0; row < fuseBinary.size(); row++)
    {
        Printf(FuseUtils::GetVerbosePriority(), "Dummy HW Fuse write 0x%08x to row %d\n",
                                                 fuseBinary[row], row);
        m_HwFuses[row] = fuseBinary[row];
    }

    MarkFuseReadDirty();
    CHECK_RC(VerifyFusing(fuseBinary));

    return rc;
}

RC DummyFuseAccessor::SetSwFuses(const map<UINT32, UINT32>& swFuseWrites)
{
    RC rc;
    CHECK_RC(InitSwFuses());
    for (const auto& addrValPair : swFuseWrites)
    {
        Printf(FuseUtils::GetVerbosePriority(), "Dummy SW Fuse write val 0x%08x to addr 0x%08x\n",
                                                 addrValPair.second, addrValPair.first);
        m_SwFuses[addrValPair.first] = addrValPair.second;
    }
    return rc;
}

RC DummyFuseAccessor::SetSwIffFuses(const vector<UINT32>& iffRows, UINT32 iffCrc)
{
    RC rc;
    CHECK_RC(InitSwFuses());
    m_SwIffRam.clear();
    for (UINT32 row = 0; row < iffRows.size(); row++)
    {
        Printf(FuseUtils::GetVerbosePriority(), "Writing 0x%08x to SW IFF row %d\n",
                                                 iffRows[row], row);
        m_SwIffRam.push_back(iffRows[row]);
    }

    Printf(Tee::PriNormal, "IFF CRC value: 0x%x\n", iffCrc);

    return rc;
}

RC DummyFuseAccessor::VerifySwIffFusing(const vector<UINT32>& iffRows, UINT32 iffCrc)
{
    Printf(Tee::PriNormal, "In VerifySwIffFusing()\n");
    return RC::OK;
}

RC DummyFuseAccessor::GetFuseRow(UINT32 row, UINT32* pValue)
{
    RC rc;
    const vector<UINT32>* pFuseBlock = nullptr;
    CHECK_RC(GetFuseBlock(&pFuseBlock));
    if (row >= pFuseBlock->size())
    {
        Printf(Tee::PriError, "Requested row outside of fuse block: %d\n", row);
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    *pValue = m_HwFuses[row];
    return rc;
}

UINT32 DummyFuseAccessor::GetOptFuseValue(UINT32 address)
{
    RC rc;
    rc = InitSwFuses();
    if (rc != RC::OK)
    {
        Printf(Tee::PriWarn, "InitSwFuses() returned RC %u\n", static_cast<UINT32>(rc));
    }

    UINT32 val = 0;
    if (m_SwFuses.find(address) != m_SwFuses.end())
    {
        val = m_SwFuses[address];
    }
    return val;
}

RC DummyFuseAccessor::ReadFuseBlock(vector<UINT32>* pReadFuses)
{
    RC rc;
    MASSERT(pReadFuses);
    pReadFuses->clear();

    UINT32 numFuseRows;
    CHECK_RC(GetNumFuseRows(&numFuseRows));
    for (UINT32 row = 0; row < numFuseRows; row++)
    {
        pReadFuses->push_back(m_HwFuses[row]);
    }
    return rc;
}

void DummyFuseAccessor::DisableWaitForIdle()
{
    // Do nothing;
}

bool DummyFuseAccessor::IsFusePrivSelwrityEnabled()
{
    return false;
}

bool DummyFuseAccessor::IsSwFusingAllowed()
{
    return true;
}

RC DummyFuseAccessor::VerifyFusing(const vector<UINT32>& expectedBinary)
{
    StickyRC rc;
    const vector<UINT32>* pLwrrentFuses = nullptr;
    CHECK_RC(GetFuseBlock(&pLwrrentFuses));
    MASSERT(expectedBinary.size() == pLwrrentFuses->size());

    for (UINT32 row = 0; row < pLwrrentFuses->size(); row++)
    {
        UINT32 lwrrentRowVal = (*pLwrrentFuses)[row];
        Printf(Tee::PriNormal, "Final Fuse value at row %d: 0x%x\n", row, lwrrentRowVal);
        if (lwrrentRowVal != expectedBinary[row])
        {
            rc = RC::VECTORDATA_VALUE_MISMATCH;
            Printf(Tee::PriNormal,
                   "  Fuse mismatch at row %d: Expected 0x%08x != Actual 0x%08x\n",
                   row, expectedBinary[row], lwrrentRowVal);
        }
    }
    return rc;
}

RC DummyFuseAccessor::LatchFusesToOptRegs()
{
    return RC::OK;
}

RC DummyFuseAccessor::SwReset()
{
    return RC::OK;
}

RC DummyFuseAccessor::GetNumFuseRows(UINT32 *pNumFuseRows) const
{
    MASSERT(pNumFuseRows);
    *pNumFuseRows = m_NumRows;
    return RC::OK;
}

RC DummyFuseAccessor::InitSwFuses()
{
    if (m_SwFusesInitialized)
    {
        return RC::OK;
    }

    RC rc;
    const FuseUtil::SkuList* pSkuList = nullptr;
    const string fuseFile = FuseUtils::GetFuseFilename(m_pDev->GetDeviceId());
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fuseFile));
    CHECK_RC(pParser->ParseFile(fuseFile, nullptr, &pSkuList, nullptr));
    // Get all the fuses using the first SKU in the SKU list
    for (const auto& fuseInfo : pSkuList->begin()->second.FuseList)
    {
        UINT32 offset = fuseInfo.pFuseDef->PrimaryOffset.Offset;
        if (offset != FuseUtil::UNKNOWN_OFFSET)
        {
            // For disable fuses, a 1 in the raw fuses is translated to a 0 in the OPT register
            // So setting the OPT value such that raw fuse's reset value is 0
            m_SwFuses[offset] = fuseInfo.pFuseDef->Type == "dis" ? 1 : 0;
        }
    }
    m_SwFusesInitialized = true;
    return rc;
}

RC DummyFuseAccessor::SetFusingVoltage(UINT32 vddMv)
{
    Printf(Tee::PriNormal, "Dummy setting voltage to %u mV\n", vddMv);
    return RC::OK;
}
