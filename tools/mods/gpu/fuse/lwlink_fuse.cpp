/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2017-2022 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "lwlink_fuse.h"

#include "core/include/gpu.h"
#include "core/include/platform.h"
#include "core/include/version.h"
#include "core/tests/testconf.h"
#include "device/interface/lwlink/lwlregs.h"
#include "gpu/fuse/fusesrc.h"
#include "gpu/include/testdevice.h"
#include "gpu/utility/scopereg.h"
#include "lwswitch/svnp01/dev_host.h" // For IFF
#include "lwswitch/svnp01/dev_lw_xp.h"
#include "lwswitch/svnp01/dev_lw_xve.h"
#include "lwswitch/svnp01/dev_lws.h"
#include "lwswitch/svnp01/dev_trim.h"

//-----------------------------------------------------------------------------
LwLinkFuse::LwLinkFuse(Device *pDev) : OldFuse(pDev)
{
    m_FuseReadDirty = true;

    const FuseUtil::FuseDefMap* pFuseDefMap = nullptr;
    const FuseUtil::SkuList* pSkuList = nullptr;
    const FuseUtil::MiscInfo* pMiscInfo = nullptr;
    const FuseDataSet* pFuseDataset = nullptr;

    if (OK == ParseXmlInt(&pFuseDefMap, &pSkuList, &pMiscInfo, &pFuseDataset))
    {
        m_FuseSpec.numOfFuseReg  = pMiscInfo->MacroInfo.FuseRows;

        MASSERT(pMiscInfo->MacroInfo.FuseRecordStart * pMiscInfo->MacroInfo.FuseCols
            == pMiscInfo->FuselessStart);
        m_FuseSpec.fuselessStart = pMiscInfo->FuselessStart;    //ramRepairFuseBlockBegin

        MASSERT(m_FuseSpec.numOfFuseReg * pMiscInfo->MacroInfo.FuseCols - 1
            == pMiscInfo->FuselessEnd);
        m_FuseSpec.fuselessEnd   = pMiscInfo->FuselessEnd - 32;    //ramRepairFuseBlockEnd - 32
    }

    // TODO: Find missing header defines
    m_SetFieldSpaceName[IFF_SPACE(PMC_0)] = "LW_PMC";
    m_SetFieldSpaceAddr[IFF_SPACE(PMC_0)] = 0x0; //DRF_BASE(LW_PMC);

    m_SetFieldSpaceName[IFF_SPACE(PBUS_0)] = "LW_PBUS";
    m_SetFieldSpaceAddr[IFF_SPACE(PBUS_0)] = DRF_BASE(LW_PBUS);

    // LW_PEXTDEV does not exist in the .h or .ref, but dev_lws.ref lists it
    m_SetFieldSpaceName[IFF_SPACE(PEXTDEV_0)] = "LW_PEXTDEV";
    m_SetFieldSpaceAddr[IFF_SPACE(PEXTDEV_0)] = 0x101000; // DRF_BASE(LW_PEXTDEV);

    m_SetFieldSpaceName[IFF_SPACE(PCFG_0)] = "LW_PCFG";
    m_SetFieldSpaceAddr[IFF_SPACE(PCFG_0)] = DRF_BASE(LW_PCFG);

    // LW_PCFG1 does not exist in the .h or .ref, but dev_lws.ref lists it
    m_SetFieldSpaceName[IFF_SPACE(PCFG1_0)] = "LW_PCFG1";
    m_SetFieldSpaceAddr[IFF_SPACE(PCFG1_0)] = 0x8A000; // DRF_BASE(LW_PCFG1);

    // LW_PCFG2 does not exist in the .h or .ref, and dev_lws.ref gives 0x0??000
    // Assuming it matches GPUs and using 0x00083000
    m_SetFieldSpaceName[IFF_SPACE(PCFG2_0)] = "LW_PCFG2";
    m_SetFieldSpaceAddr[IFF_SPACE(PCFG2_0)] = 0x00083000;

    // LW_XP3G does not exist in the .h, but the .ref says it matches LW_XP
    m_SetFieldSpaceName[IFF_SPACE(XP3G_0)] = "LW_XP3G_0";
    m_SetFieldSpaceAddr[IFF_SPACE(XP3G_0)] = DRF_BASE(LW_XP) + 0x0000;

    m_SetFieldSpaceName[IFF_SPACE(XP3G_1)] = "LW_XP3G_1";
    m_SetFieldSpaceAddr[IFF_SPACE(XP3G_1)] = DRF_BASE(LW_XP) + 0x1000;

    m_SetFieldSpaceName[IFF_SPACE(XP3G_2)] = "LW_XP3G_2";
    m_SetFieldSpaceAddr[IFF_SPACE(XP3G_2)] = DRF_BASE(LW_XP) + 0x2000;

    m_SetFieldSpaceName[IFF_SPACE(XP3G_3)] = "LW_XP3G_3";
    m_SetFieldSpaceAddr[IFF_SPACE(XP3G_3)] = DRF_BASE(LW_XP) + 0x3000;

    m_SetFieldSpaceName[IFF_SPACE(PTRIM_7)] = "LW_PTRIM_7";
    m_SetFieldSpaceAddr[IFF_SPACE(PTRIM_7)] = DRF_BASE(LW_PTRIM) + 0x7000;

    // LW_PVTRIM isn't 0x1000 aligned, thus HW masks out the last 12 bits.
    m_SetFieldSpaceName[IFF_SPACE(PVTRIM_0)] = "LW_PVTRIM";
    m_SetFieldSpaceAddr[IFF_SPACE(PVTRIM_0)] = DRF_BASE(LW_PVTRIM) & ~0xFFF;

    m_SetFieldSpaceName[IFF_SPACE(PTRIM_8)] = "LW_PTRIM_8";
    m_SetFieldSpaceAddr[IFF_SPACE(PTRIM_8)] = DRF_BASE(LW_PTRIM) + 0x8000;

    // LW_PGC6 does not exist in the .h or .ref, but dev_lws.ref lists it
    // LW_PGC6 isn't 0x1000 aligned, thus HW masks out the last 12 bits.
    m_SetFieldSpaceName[IFF_SPACE(PGC6_0)] = "LW_PGC6";
    m_SetFieldSpaceAddr[IFF_SPACE(PGC6_0)] = 0x10E000; // DRF_BASE(LW_PGC6) & ~0xFFF;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::CacheFuseReg()
{
    // Is a fuse re-read needed?
    if (!m_FuseReadDirty)
        return OK;

    Tasker::MutexHolder mh(GetMutex());

    if (!m_FuseReadDirty)
        return OK;

    RC rc;
    m_FuseReg.clear();
    TestConfiguration lwrrTestConfig;
    CHECK_RC(lwrrTestConfig.InitFromJs());
    FLOAT64 timeoutMs = lwrrTestConfig.TimeoutMs();

    for (UINT32 i=0; i < m_FuseSpec.numOfFuseReg; i++)
    {
        UINT32 fuseReg;
        CHECK_RC(GetFuseWord(i, timeoutMs, &fuseReg));
        m_FuseReg.push_back(fuseReg);

        // lower priority
        if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
            Printf(Tee::PriDebug, "Fuse %02d : = 0x%08x\n", i, fuseReg);
    }
    CHECK_RC(FindConsistentRead(&m_FuseReg, timeoutMs));

    m_FuseReadDirty = false;
    return OK;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::GetCachedFuseReg(vector<UINT32> *pCachedReg)
{
    MASSERT(pCachedReg);
    RC rc;
    CHECK_RC(CacheFuseReg());
    *pCachedReg = m_FuseReg;
    return OK;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::GetFuseWord(UINT32 fuseRow, FLOAT64 timeoutMs, UINT32 *pRetData)
{
    MASSERT(pRetData);
    auto* pLwsDev = dynamic_cast<TestDevice*>(GetDevice());
    auto& regs = pLwsDev->GetInterface<LwLinkRegs>()->Regs();

    // set the Row of the fuse
    regs.Write32(0, MODS_FUSE_FUSEADDR_VLDFLD, fuseRow);
    // set fuse control to 'read'
    regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_READ);

    // wait for state to become idle
    RC rc = POLLWRAP_HW(PollFusesIdle, this, timeoutMs);

    *pRetData = regs.Read32(0, MODS_FUSE_FUSERDATA);

    return rc;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::LatchFuseToOptReg(FLOAT64 timeoutMs)
{
    auto* pLwsDev = dynamic_cast<TestDevice*>(GetDevice());
    auto& regs = pLwsDev->GetInterface<LwLinkRegs>()->Regs();

    // Resense control fuses
    regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_SENSE_CTRL);

    // Resense standard fuses
    regs.Write32(0, MODS_FUSE_PRIV2INTFC_START_DATA, 1);
    Platform::Delay(1);
    regs.Write32(0, MODS_FUSE_PRIV2INTFC, 0);   // reset _START
    Platform::Delay(4);
    return POLLWRAP_HW(PollFuseSenseDone, pLwsDev, timeoutMs);
}
//-----------------------------------------------------------------------------
UINT32 LwLinkFuse::ExtractFuseVal(const FuseUtil::FuseDef* pDef, Tolerance strictness)
{
    if (pDef->Type == FuseUtil::TYPE_PSEUDO)
    {
        Printf(Tee::PriError, "%s is a pseudo-fuse! Cannot extract value!\n",
            pDef->Name.c_str());
        return 0;
    }

    if (pDef->Type == "fuseless")
    {
        return ExtractFuselessFuseVal(pDef, strictness);
    }
    MASSERT(pDef);
    UINT32 fuseVal  = 0;
    list<FuseUtil::FuseLoc>::const_reverse_iterator fuseIter =
        pDef->FuseNum.rbegin();

    for (; fuseIter != pDef->FuseNum.rend(); fuseIter++)
    {
        GetPartialFuseVal(fuseIter->Number,
                          fuseIter->Lsb,
                          fuseIter->Msb,
                         &fuseVal);
    }

    // Get the Redundant fuse value
    UINT32 rdFuseVal = 0;
    fuseIter = pDef->RedundantFuse.rbegin();
    for (; fuseIter != pDef->RedundantFuse.rend(); fuseIter++)
    {
        GetPartialFuseVal(fuseIter->Number,
                          fuseIter->Lsb,
                          fuseIter->Msb,
                         &rdFuseVal);
    }

    if (strictness & OldFuse::REDUNDANT_MATCH)
        return (fuseVal | rdFuseVal);
    else
        return fuseVal;
    return 0;
}
//-----------------------------------------------------------------------------
UINT32 LwLinkFuse::ExtractFuselessFuseVal(const FuseUtil::FuseDef* pDef, Tolerance strictness)
{
    if (0 == m_Records.size())
    {
        if (OK != ReadInRecords())
        {
            Printf(Tee::PriNormal, "Cannot read in fuse records\n");
            return 0;
        }
    }

    Printf(Tee::PriDebug, "Extract %s\n", pDef->Name.c_str());
    UINT32 fuseVal = 0;
    // need to reconstruct the fuse from subfuses. Go from MSB subfuses to LSB
    list<FuseUtil::FuseLoc>::const_reverse_iterator subFuseIter;
    subFuseIter = pDef->FuseNum.rbegin();
    for (; subFuseIter != pDef->FuseNum.rend(); subFuseIter++)
    {
        Printf(Tee::PriDebug, " ChainId;0x%x, Offset:0x%x, "
                               "DataShift:0x%x, Msb:0x%x, Lsb:0x%x||",
               subFuseIter->ChainId, subFuseIter->ChainOffset,
               subFuseIter->DataShift, subFuseIter->Msb, subFuseIter->Lsb);

        // just like DRF_VAL:
        UINT32 numBits = subFuseIter->Msb - subFuseIter->Lsb + 1;
        UINT32 mask    = 0xFFFFFFFF >> (32 - numBits);
        // get the subfuse value - if it's set, it'd be in the Jtag Chain
        UINT32 subFuse = GetJtagChainData(subFuseIter->ChainId,
                                          subFuseIter->Msb,
                                          subFuseIter->Lsb);
        fuseVal        = (fuseVal << numBits) | subFuse;
        Printf(Tee::PriDebug,
               "NumBits %d, Mask:%d, SubFuse:0x%0x, FuseVal:0x%x",
               numBits, mask, subFuse, fuseVal);

        Printf(Tee::PriDebug, "\n");
    }

    return fuseVal;
}
//-----------------------------------------------------------------------------
UINT32 LwLinkFuse::ExtractOptFuseVal(const FuseUtil::FuseDef* pDef)
{
    auto* pLwsDev = dynamic_cast<TestDevice*>(GetDevice());
    UINT32 offset = pDef->PrimaryOffset.Offset;
    UINT32 fuseVal = 0;
    pLwsDev->GetInterface<LwLinkRegs>()->RegRd(0, RegHalDomain::FUSE, offset, &fuseVal);
    return fuseVal;
}
//-----------------------------------------------------------------------------
//  Get a portion of the fuse value from one register
// With '2D' fuses, a 'fuse' can be a collection of bits at different
// fuse rows. For example the OPT_DAC_CRT_MAP fuse on GT218 is on
// bit 31:31 of row 12, and bit 6:0 of row 14. In FuseXml parsing,
// the FuseLoc is stored by 'push_back' with LEAST SIGNIFICANT BITS FIRST
// In ExtractFuseVal, this function can be called multiple times in order to
// construct the value of a Fuse.
// This function will act on pFuseVal; it will be updated for each interaction
void LwLinkFuse::GetPartialFuseVal(UINT32 rowNum,
                                     UINT32 lsb,
                                     UINT32 msb,
                                     UINT32 *pFuseVal)
{
    if (rowNum >= m_FuseReg.size())
    {
        Printf(Tee::PriNormal, "XML error... fuse number too large\n");
        MASSERT(!"XML ERROR\n");
        return;
    }
    UINT32 rowVal = m_FuseReg[rowNum];
    // this is kind of like doing a DRF_VAL()
    UINT32 numBits = msb - lsb + 1;
    MASSERT(numBits <= 32);
    UINT32 mask    = static_cast<UINT32>((1ULL<<numBits) - 1);
    UINT32 partial = (rowVal >> lsb) & mask;

    *pFuseVal = (*pFuseVal << numBits) | partial;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::SetPartialFuseVal(const list<FuseUtil::FuseLoc> &fuseLocList,
                                   UINT32 desiredValue,
                                   vector<UINT32> *pRegsToBlow)
{
    list<FuseUtil::FuseLoc>::const_iterator fuseLocIter = fuseLocList.begin();
    // don't do anything - if there are no fuses (eg redundant fuses)
    if (fuseLocIter == fuseLocList.end())
        return OK;

    // Example: OPT_DAC_CRT_MAP row 12, bit 31:31, row 14, bit 6:0
    // MSB is row 12 31:31, then row 14 6:0
    // Construct the LSB of desired Value first, then right shift the desired
    // value by (MSB-LSB+1) each iteraction until we marked all the bits
    // in the given fuse
    for (; fuseLocIter != fuseLocList.end(); ++fuseLocIter)
    {
        UINT32 numBits = fuseLocIter->Msb - fuseLocIter->Lsb + 1;
        MASSERT(numBits <= 32);
        UINT32 mask    = static_cast<UINT32>((1ULL<<numBits) - 1);
        UINT32 row     = fuseLocIter->Number;

        UINT32 valInRow = (desiredValue & mask);
        (*pRegsToBlow)[row] |= valInRow << fuseLocIter->Lsb;
        if (valInRow)
        {
            Printf(GetPrintPriority(), "Row, Val = (%d, 0x%x)",
                   row, (*pRegsToBlow)[row]);
        }
        desiredValue >>= numBits;
    }
    return OK;
}

RC LwLinkFuse::ParseXmlInt(const FuseUtil::FuseDefMap **ppFuseDefMap,
                             const FuseUtil::SkuList    **ppSkuList,
                             const FuseUtil::MiscInfo   **ppMiscInfo,
                             const FuseDataSet          **ppFuseDataSet)
{
    RC rc;
    string fuseFileName = GetFuseFilename();
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fuseFileName));
    CHECK_RC(pParser->ParseFile(fuseFileName,
                                ppFuseDefMap,
                                ppSkuList,
                                ppMiscInfo,
                                ppFuseDataSet));
    return rc;
}
//-----------------------------------------------------------------------------
bool LwLinkFuse::IsFuseSupported(const FuseUtil::FuseInfo &spec, Tolerance strictness)
{
    if (!OldFuse::IsFuseSupported(spec, strictness))
        return false;

    CheckPriority fuseLevel = (OldFuse::CheckPriority)spec.pFuseDef->Visibility;
    if (strictness & OldFuse::LWSTOMER_MATCH)
        return (OldFuse::LEVEL_LWSTOMER <= fuseLevel);
    return (GetCheckLevel() <= fuseLevel);
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::BlowArray(const vector<UINT32>  &regsToBlow,
                           string                method,
                           UINT32                durationNs,
                           UINT32                lwVddMv)
{
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        return OK;

    RC rc;
    vector<UINT32> fuseReg;

    CHECK_RC(GetCachedFuseReg(&fuseReg));
    if (fuseReg.size() != regsToBlow.size())
    {
        Printf(Tee::PriNormal, "Input fuse array size mismatch.\n");
        return RC::BAD_PARAMETER;
    }

    // OR the current value and desired values together
    for (UINT32 i = 0; i < fuseReg.size(); i++)
    {
        fuseReg[i] |= regsToBlow[i];
    }

    // blow
    bool reblow = false;
    UINT32 attemptsLeft = GetReblowAttempts();
    do
    {
        rc = BlowFusesInt(fuseReg);
        reblow = ((OK != rc) && (attemptsLeft > 0));
        attemptsLeft--;
    } while (reblow);

    // Note : SLT does not want to restore fuse register (LW_FUSE_FUSETIME2)!
    return rc;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::BlowFusesInt(const vector<UINT32> &regsToBlow)
{
    RC rc;
    // mark dirty regardless of whether this funciton succeeds. Even if it
    // fails mid way through, some bits might have been blown
    MarkFuseReadDirty();

    // get the original fuse values:
    vector<UINT32> orignalValues;   // read the fuse
    vector<UINT32> newBitsArray;    // new bits to be blown

    Printf(GetPrintPriority(), "New Bits   | Expected Final Value \n");
    Printf(GetPrintPriority(), "----------   ----------\n");

    TestConfiguration lwrrTestConfig;
    CHECK_RC(lwrrTestConfig.InitFromJs());
    FLOAT64 timeoutMs = lwrrTestConfig.TimeoutMs();

    for (UINT32 i = 0; i < m_FuseSpec.numOfFuseReg; i++)
    {
        UINT32 fuseReg;
        CHECK_RC(GetFuseWord(i, timeoutMs, &fuseReg));
        orignalValues.push_back(fuseReg);
        UINT32 newBits = regsToBlow[i] & ~fuseReg;
        newBitsArray.push_back(newBits);

        Printf(GetPrintPriority(), "0x%08x : 0x%08x\n", newBits, regsToBlow[i]);
    }

    rc = BlowFuseRows(newBitsArray, timeoutMs);

    CHECK_RC(EnableFuseSrc(false));
    CHECK_RC(rc);
    Tasker::Sleep(m_FuseSpec.sleepTime[FuseSpec::SLEEP_AFTER_GPIO_LOW]);

    CHECK_RC(POLLWRAP_HW(PollFusesIdle, this, timeoutMs));
    Printf(Tee::PriNormal, "Fuse blow completed.\n");

    MarkFuseReadDirty();
    // verify whether we have the written the right data:
    vector<UINT32> reRead;
    for (UINT32 i = 0; i < m_FuseSpec.numOfFuseReg; i++)
    {
        UINT32 fuseReg;
        CHECK_RC(GetFuseWord(i, timeoutMs, &fuseReg));
        reRead.push_back(fuseReg);
    }
    CHECK_RC(FindConsistentRead(&reRead, timeoutMs));
    bool failFuseVerif = false;
    for (UINT32 i = 0; i < m_FuseSpec.numOfFuseReg; i++)
    {
        Printf(Tee::PriNormal, "Final Fuse value at row %d: 0x%x\n", i, reRead[i]);
        if (reRead[i] != regsToBlow[i])
        {
            Printf(Tee::PriNormal, "  Expected data 0x%08x != 0x%08x at row%d\n",
                   regsToBlow[i], reRead[i], i);
            failFuseVerif = true;
        }
    }
    if (failFuseVerif)
    {
        CHECK_RC(RC::VECTORDATA_VALUE_MISMATCH);
    }

    // Make fuse visible after fuse blow: details in bug 569056
    CHECK_RC(LatchFuseToOptReg(timeoutMs));
    return rc;
}
//-----------------------------------------------------------------------------
/* override */ RC LwLinkFuse::GetIffRecords
(
    vector<FuseUtil::IFFRecord>* pRecords,
    bool keepDeadRecords
)
{
    RC rc;
    vector<UINT32> fuseBlock;
    CHECK_RC(GetCachedFuseReg(&fuseBlock));

    UINT32 iff = static_cast<UINT32>(fuseBlock.size());
    for (UINT32 i = m_FuseSpec.fuselessStart / REPLACE_RECORD_SIZE;
        i < m_FuseSpec.numOfFuseReg; i++)
    {
        UINT32 row = fuseBlock[i];

        // Check the IFF bit to see if this row is an IFF record
        // Anything at or past an IFF record is IFF space
        if ((row & (1 << CHAINID_SIZE)) != 0)
        {
            iff = i;
            break;
        }
    }

    for (; iff < fuseBlock.size(); iff++)
    {
        pRecords->push_back(fuseBlock[iff]);
    }

    if (!keepDeadRecords)
    {
        for (iff = 0; iff < pRecords->size(); iff++)
        {
            UINT32 opCode = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD, _OP, (*pRecords)[iff].data);
            switch (opCode)
            {
                case IFF_OPCODE(MODIFY_DW): iff += 2; break;
                case IFF_OPCODE(WRITE_DW): iff++; break;
                case IFF_OPCODE(SET_FIELD): break;

                case IFF_OPCODE(NULL):
                case IFF_OPCODE(INVALID):
                    pRecords->erase(pRecords->begin() + iff);
                    iff--;
                    break;

                default:
                    Printf(Tee::PriDebug, "Unknown IFF record: %#x\n", (*pRecords)[iff].data);
                    break;
            }
        }
    }
    return OK;
}
/* virtual */ RC LwLinkFuse::DecodeIff()
{
    // Only supported on lwpu network or equivalent
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        return RC::UNSUPPORTED_FUNCTION;

    RC rc;
    vector<FuseUtil::IFFRecord> iffRecords;

    CHECK_RC(GetIffRecords(&iffRecords, true));
    CHECK_RC(DecodeIffRecords(iffRecords));

    return rc;
}

RC LwLinkFuse::DecodeSkuIff(string skuName)
{
    // Only supported on lwpu network or equivalent
    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        return RC::UNSUPPORTED_FUNCTION;

    const FuseUtil::SkuConfig* skuConfig = GetSkuSpecFromName(skuName);

    if (!skuConfig)
    {
        Printf(Tee::PriError, "Cannot find sku %s\n", skuName.c_str());
        return RC::BAD_PARAMETER;
    }

    vector<FuseUtil::IFFRecord> iffRecords;
    if (skuConfig->iffPatches.empty())
    {
        // Legacy IFF from FuseXML
        list<FuseUtil::FuseInfo>::const_iterator fuseInfo;
        for (const auto& fuseInfo : skuConfig->FuseList)
        {
            string fuseName = Utility::ToLowerCase(fuseInfo.pFuseDef->Name);
            if (fuseName.compare(0, 4, "iff_") == 0)
            {
                UINT64 index = Utility::Strtoull(fuseName.substr(4).c_str(), nullptr, 10);
                if (index >= iffRecords.size())
                {
                    iffRecords.resize(index + 1);
                }
                iffRecords[index] = fuseInfo.Value;
            }
        }
        // Reverse the order (IFF_0 is the last IFF record)
        for (UINT32 i = 0; i < iffRecords.size() / 2; i++)
        {
            auto temp = iffRecords[i];
            iffRecords[i] = iffRecords[iffRecords.size() - i - 1];
            iffRecords[iffRecords.size() - i - 1] = temp;
        }
    }
    else
    {
        list<FuseUtil::IFFRecord> records;

        // Combine the patches
        for (const auto& patch : skuConfig->iffPatches)
        {
            records.insert(records.end(),
                patch.rows.begin(), patch.rows.end());
        }

        // insert in reverse order since iff patches are specified bottom up
        for (auto rIter = records.rbegin();
             rIter != records.rend();
             rIter++)
        {
            iffRecords.push_back(*rIter);
        }
    }

    return DecodeIffRecords(iffRecords);
}

/* virtual */ RC LwLinkFuse::DecodeIffRecords(const vector<FuseUtil::IFFRecord>& iffRecords)
{
    // Decode information found in dev_bus.ref (most up-to-date) and
    // //hw/doc/gpu/maxwell/Ilwestigation_phase/Host/Maxwell_Host_IFF_uArch.doc
    // (a bit easier to read (MS Word formatting instead of ASCII),
    // but not the authoritative reference)

    Printf(Tee::PriNormal, "IFF Commands:\n");

    UINT32 commandNum = 0;
    for (UINT32 row = 0; row < iffRecords.size(); row++)
    {
        UINT32 record = iffRecords[row].data;
        if (record == 0 || record == 0xffffffff)
        {
            Printf(Tee::PriDebug, "Dead row. Continuing...\n");
            continue;
        }
        UINT32 opCode = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD, _OP, record);
        switch (opCode)
        {
            case IFF_OPCODE(MODIFY_DW):
            {
                if (row > iffRecords.size() - 3)
                {
                    Printf(Tee::PriError,
                        "Not enough rows left for record type %x!\n", opCode);
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }
                UINT32 addr = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_MODIFY_DW, _ADDR, record);
                addr <<= IFF_RECORD(CMD_MODIFY_DW_ADDR_BYTESHIFT);
                auto andMask = iffRecords[++row];
                auto orMask = iffRecords[++row];

                string decode = Utility::StrPrintf("%2d - Modify: [0x%06x] =", commandNum++, addr);

                if (andMask.dontCare || orMask.dontCare)
                {
                    Printf(Tee::PriNormal, "%s <ATE value>\n", decode.c_str());
                }
                else
                {
                    Printf(Tee::PriNormal, "%s ([0x%06x] & 0x%08x) | 0x%08x\n",
                           decode.c_str(), addr, andMask.data, orMask.data);
                }
            }
            break;
            case IFF_OPCODE(WRITE_DW):
            {
                if (row > iffRecords.size() - 2)
                {
                    Printf(Tee::PriError,
                        "Not enough rows left for record type %x!\n", opCode);
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }

                UINT32 addr = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_WRITE_DW, _ADDR, record);
                addr <<= IFF_RECORD(CMD_WRITE_DW_ADDR_BYTESHIFT);
                auto record = iffRecords[++row];

                string decode = Utility::StrPrintf("%2d - Write:  [0x%06x] =", commandNum++, addr);

                if (record.dontCare)
                {
                    Printf(Tee::PriNormal, "%s <ATE value>\n", decode.c_str());
                }
                else
                {
                    Printf(Tee::PriNormal, "%s 0x%08x\n", decode.c_str(), record.data);
                }
            }
            break;
            case IFF_OPCODE(SET_FIELD):
            {
                UINT32 space = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _SPACE, record);

                if (m_SetFieldSpaceAddr.find(space) == m_SetFieldSpaceAddr.end())
                {
                    Printf(Tee::PriError, "Invalid IFF Space value: %u\n", space);
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }

                UINT32 addr = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _ADDR, record);
                addr <<= IFF_RECORD(CMD_SET_FIELD_ADDR_BYTESHIFT);
                UINT32 bitIndex = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _BITINDEX, record);
                UINT32 fieldSpec = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD_SET_FIELD, _FIELDSPEC, record);

                // Most significant set bit of the field spec determines the size
                // Ex: 0b0100110 - the MSB set is bit 5, thus the data size is 5 bits (data = 0b00110)
                UINT32 fieldWidth = 6;
                for (; fieldWidth > 0; fieldWidth--)
                {
                    if (fieldSpec & (1 << fieldWidth))
                    {
                        break;
                    }
                }

                UINT32 orMask = (fieldSpec & ~(1 << fieldWidth)) << bitIndex;
                UINT32 andMask = ~(((1 << fieldWidth) - 1) << bitIndex);

                string spaceName = m_SetFieldSpaceName[space];
                addr |= m_SetFieldSpaceAddr[space];

                Printf(Tee::PriNormal,
                    "%2d - Set Field: (%s) [0x%06x] = ([0x%06x] & 0x%08x) | 0x%08x\n",
                    commandNum++, spaceName.c_str(), addr, addr, andMask, orMask);

            }
            break;
            case IFF_OPCODE(NULL):
            case IFF_OPCODE(INVALID):
                Printf(Tee::PriDebug, "Null/Ilwalidated IFF record\n");
                break;
            default:
                Printf(Tee::PriError, "Unknown IFF type: 0x%02x!\n", opCode);
                return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }
    Printf(Tee::PriNormal, "\n");
    return OK;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::ApplyIffPatches(string chipSku, vector<UINT32>* pRegsToBlow)
{
    RC rc;
    const FuseUtil::SkuConfig* pConfig = GetSkuSpecFromName(chipSku);

    if (pConfig == nullptr)
    {
        if (!chipSku.empty())
        {
            Printf(GetPrintPriority(), "Chip sku: %s not in XML\n", chipSku.c_str());
        }
        return OK;
    }

    if (pConfig->iffPatches.size() == 0)
    {
        Printf(GetPrintPriority(), "No IFF rows found for SKU %s\n", chipSku.c_str());
        return OK;
    }

    vector<UINT32> fuseBlock;
    CHECK_RC(GetCachedFuseReg(&fuseBlock));
    MASSERT(pRegsToBlow->size() == fuseBlock.size());

    // Find how many rows have been used by fuseless and column fuses
    // Also find the first of the existing IFF records (if any)

    // Assume all column-based fuses are used and start at fuseless fuses
    UINT32 usedRows = m_FuseSpec.fuselessStart / REPLACE_RECORD_SIZE;
    UINT32 firstIff = static_cast<UINT32>(pRegsToBlow->size());
    for (UINT32 i = m_FuseSpec.fuselessStart / REPLACE_RECORD_SIZE;
        i < m_FuseSpec.numOfFuseReg; i++)
    {
        UINT32 row = (*pRegsToBlow)[i] | fuseBlock[i];

        // Anything at or past an IFF record is IFF space
        if ((row & (1 << CHAINID_SIZE)) != 0)
        {
            firstIff = i;
            break;
        }

        if (row != 0)
        {
            usedRows = i + 1;
        }
    }

    // Group the needed IFF rows for simplicity
    list<FuseUtil::IFFRecord> iffRows;

    for (auto listIter = pConfig->iffPatches.begin();
         listIter != pConfig->iffPatches.end();
         listIter++)
    {
        iffRows.insert(iffRows.end(),
            listIter->rows.begin(), listIter->rows.end());
    }

    // Some IFF commands are 2 or 3 rows in length and need to be conselwtive. Find
    // the command size by iterating through in exelwtion order (reverse of fusing)
    list<UINT32> commandSizes;
    for (auto iffRecord = iffRows.rbegin(); iffRecord != iffRows.rend(); iffRecord++)
    {
        // The command type is specified in bits 4:0
        UINT32 opCode = DRF_VAL(_PBUS_FUSE_FMT_IFF_RECORD, _CMD, _OP, iffRecord->data);
        switch (opCode)
        {
            case IFF_OPCODE(MODIFY_DW):
                commandSizes.push_front(3);
                iffRecord++;
                MASSERT(iffRecord != iffRows.rend());
                iffRecord++;
                MASSERT(iffRecord != iffRows.rend());
                break;
            case IFF_OPCODE(WRITE_DW):
                commandSizes.push_front(2);
                iffRecord++;
                MASSERT(iffRecord != iffRows.rend());
                break;
            case IFF_OPCODE(SET_FIELD):
                commandSizes.push_front(1);
                break;
            default:
                Printf(Tee::PriError, "Malformed IFF Patch: 0x%08x\n", iffRecord->data);
                Printf(Tee::PriError, "Trying to blow Null/Ilwalidated/Unknown IFF\n");
                return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    // IFF rows go at the end of the fuse block. If it's possible to colwert
    // some of the existing rows, then do that, otherwise ilwalidate the rows
    // that don't match the IFF patches to burn, and add them at the end.
    //
    // Note: IFF commands can't change order, so we have to find a match for
    // command N before N+1. Even if command N+1 is already in the fuses,
    // if command N hasn't been written yet, we have to ilwalidate those rows.
    const UINT32 ilwalidatedRow = 0xffffffff;

    UINT32 newRows = 0;
    UINT32 lwrrentRow = static_cast<UINT32>(fuseBlock.size()) - 1;

    auto iffRow = iffRows.begin();
    auto cmdSize = commandSizes.begin();
    while (iffRow != iffRows.end() && lwrrentRow > 0)
    {
        bool hasOneToZero = false;

        // Check for 1 -> 0 transistions
        auto temp = iffRow;
        for (UINT32 i = 0; i < *cmdSize; i++, temp++)
        {
            // Fuse burning doesn't support ATE in IFF yet
            if (!temp->dontCare)
            {
                MASSERT(!"IFF fuse burning doesn't support ATE!");
                return RC::SOFTWARE_ERROR;
            }
            if ((~temp->data & fuseBlock[lwrrentRow - i]) != 0)
            {
                hasOneToZero = true;
                break;
            }
        }

        if (hasOneToZero)
        {
            // Ilwalidate this row and try the next one(s)
            // If the row to ilwalidate is 0, we don't need to change it
            // Keep it in case the IFFs change again and we can reuse this row
            if (fuseBlock[lwrrentRow] != 0)
            {
                (*pRegsToBlow)[lwrrentRow] = ilwalidatedRow;
            }
            lwrrentRow--;
            newRows++;
            continue;
        }

        for (UINT32 i = 0; i < *cmdSize; i++)
        {
            (*pRegsToBlow)[lwrrentRow--] = iffRow->data;
            newRows++;
            iffRow++;
        }
        cmdSize++;
    }

    // Make sure we finished and didn't overlap into fuseless fuses (this is a
    // very unlikely scenario, but it would cause major issues if it happened)
    UINT32 rowsNeeded = newRows + usedRows;
    if (iffRow != iffRows.end() || rowsNeeded > fuseBlock.size())
    {
        Printf(Tee::PriError, "Too many fuse records!\n");
        Printf(Tee::PriError, "Needed: %u;  Available: %u\n",
            rowsNeeded, static_cast<UINT32>(fuseBlock.size()));
        return RC::SOFTWARE_ERROR;
    }

    // Lastly, ilwalidate any extra IFF rows that we don't want anymore
    for (UINT32 i = firstIff; i < fuseBlock.size() - newRows; i++)
    {
        // Don't blow rows if they're blank or if they're already ilwalidated
        if (fuseBlock[i] != 0 && fuseBlock[i] != ilwalidatedRow)
        {
            (*pRegsToBlow)[i] = ilwalidatedRow;
        }
    }

    return OK;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::BlowFuseRows
(
    const vector<UINT32> &newBitsArray,
    FLOAT64               timeoutMs
)
{
    RC rc;
    auto* pLwsDev = dynamic_cast<TestDevice*>(GetDevice());
    auto& regs = pLwsDev->GetInterface<LwLinkRegs>()->Regs();

    regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_SENSE_CTRL);

    CHECK_RC(POLLWRAP_HW(PollFusesIdle, this, timeoutMs));

    bool fuseSrcHigh = false;

    for (UINT32 row = 0; row < m_FuseSpec.numOfFuseReg; row++)
    {
        if (Utility::CountBits(newBitsArray[row]) == 0)
        {
            // don't bother attempting to blow this row if there aren't new bits
            continue;
        }

        // wait until state is idle
        CHECK_RC(POLLWRAP_HW(PollFusesIdle, this, timeoutMs));

        // set the Row of the fuse
        regs.Write32(0, MODS_FUSE_FUSEADDR_VLDFLD, row);

        // write the new fuse bits in
        regs.Write32(0, MODS_FUSE_FUSEWDATA, newBitsArray[row]);

        if (!fuseSrcHigh)
        {
            // Flick FUSE_SRC to high
            CHECK_RC(EnableFuseSrc(true));
            Tasker::Sleep(m_FuseSpec.sleepTime[FuseSpec::SLEEP_AFTER_GPIO_HIGH]);
            fuseSrcHigh = true;
        }

        // issue the Write command
        regs.Write32(0, MODS_FUSE_FUSECTRL_CMD_WRITE);
        CHECK_RC(POLLWRAP_HW(PollFusesIdle, this, timeoutMs));
    }
    return rc;
}

//! Set the daughter card to output high/low
//  FuseSrcToggle function defined in fuseutil.js
//  Copy of the ByJsFunc methods in the FuseSrc class
RC LwLinkFuse::EnableFuseSrc(bool setHigh)
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

// bug 622250: we sometimes read back 0xFFFFFFFF/0xFFFF0000/0x0000FFFF
// after we do a fuse write. This is to ensure that we have 3 consistent
// reads first. Find the most common one. If all 3 are different, fail!
RC LwLinkFuse::FindConsistentRead(vector<UINT32> *pFuseRegs, FLOAT64 timeoutMs)
{
    RC rc;

    vector<UINT32> reReads[2];
    for (UINT32 j = 0; j < 2; j++)
    {
        // each loop
        reReads[j].resize(m_FuseReg.size());
        for (UINT32 i = 0; i < m_FuseReg.size(); i++)
        {
            UINT32 fuseReg;
            CHECK_RC(GetFuseWord(i, timeoutMs, &fuseReg));
            reReads[j][i]= fuseReg;
        }
        Tasker::Sleep(5);
    }

    // find the most frequent one.
    for (UINT32 i = 0; i < m_FuseReg.size(); i++)
    {
        if (((*pFuseRegs)[i] == reReads[0][i]) &&
            ((*pFuseRegs)[i] == reReads[1][i]))
            continue;
        // there is a mismatch pick the most common one and store it
        Printf(Tee::PriNormal, "re-read got different results: 0x%x, 0x%x, 0x%x\n",
               (*pFuseRegs)[i],
               reReads[0][i],
               reReads[1][i]);

        if (reReads[0][i] == reReads[1][i])
        {
            (*pFuseRegs)[i] = reReads[0][i];
        }
        else if (((*pFuseRegs)[i] == reReads[0][i]) ||
                 ((*pFuseRegs)[i] == reReads[1][i]))
        {
            // we have something that agrees with original
        }
        else
        {
            Printf(Tee::PriNormal, "all 3 reads are unique... bad chip!\n");
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }
    }
    return OK;
}
//-----------------------------------------------------------------------------
// This ensures that OPT fuses agrees with RawFuses
bool LwLinkFuse::DoesOptAndRawFuseMatch(const FuseUtil::FuseDef* pDef,
                                          UINT32 rawFuseVal,
                                          UINT32 optFuseVal)
{
    bool retVal = false;
    if (pDef->Type == "dis")
    {
        // for disable fuse types: 0 in RawFuse = 1 in Opt
        // create a mask first - it can be multi bits!
        UINT32 mask = (1 << pDef->NumBits) - 1;
        UINT32 ilwertedRawFuseVal = (~rawFuseVal) & mask;
        retVal = (optFuseVal == ilwertedRawFuseVal);
    }
    else if (pDef->Type == "en/dis")
    {
        // If the undo bit (bit 1) is set, we expect the OPT value to be 0
        bool validUndo = (rawFuseVal & (1 << pDef->NumBits)) != 0 && optFuseVal == 0;

        retVal = rawFuseVal == optFuseVal || validUndo;
    }
    else
    {
        retVal = (rawFuseVal == optFuseVal);
    }
    return retVal;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::DumpFuseRecToRegister(vector<UINT32> *pFuseReg)
{
    RC rc;
    MASSERT(pFuseReg);

    // LwSwitch fuseless fuses have to be ordered by [ChainId, Offset]
    // They also have to write to unique locations (i.e. they can't
    // write to [ChainId X, Offset Y] more than once)
    auto fuseRecComparator =
        [](const FuseRecord& first, const FuseRecord& second) -> bool
    {
        return first.chainId < second.chainId ||
            (first.chainId == second.chainId && first.offset < second.offset);
    };
    bool outOfOrder = false;
    UINT32 lastRecordFound = 0;
    UINT32 prevRecIdx = 0;
    for (UINT32 i = 1; i < m_Records.size(); i++)
    {
        auto& record = m_Records[i];
        auto& prevRec = m_Records[prevRecIdx];
        // Null/Ilwalidated records can be skipped
        if (record.chainId == 0 || record.chainId == ILWALID_CHAIN_ID)
        {
            continue;
        }
        // Mark the index of the last valid record we encounter
        lastRecordFound = i;
        prevRecIdx = i;

        // Check order
        // If this is the first valid record we've found, don't bother comparing
        if (!(prevRec.chainId == 0 || prevRec.chainId == ILWALID_CHAIN_ID) &&
            !fuseRecComparator(prevRec, record))
        {
            outOfOrder = true;
        }
    }

    if (outOfOrder)
    {
        // We could try to be clever here to save fuse rows, but LwSwitch
        // has so few fuses that there isn't much point. Ilwalidate all
        // previous fuselss records and rewrite them in sorted order.
        vector<FuseRecord> newRecords;
        for (auto& record : m_Records)
        {
            if (record.chainId == 0 || record.chainId == ILWALID_CHAIN_ID)
                continue;

            newRecords.push_back(record);
            record.chainId = ILWALID_CHAIN_ID;
        }
        stable_sort(newRecords.begin(), newRecords.end(), fuseRecComparator);
        // Remove duplicates (keeping the last of the duplicate records)
        for (INT32 i = newRecords.size() - 2; i >= 0; i--)
        {
            if (newRecords[i].chainId == newRecords[i + 1].chainId &&
                newRecords[i].offset == newRecords[i + 1].offset)
            {
                newRecords.erase(newRecords.begin() + i);
            }
        }
        for (UINT32 i = 0; i < newRecords.size(); i++)
        {
            // Make sure we don't run out of records
            if (lastRecordFound + 1 + i >= m_Records.size())
            {
                return RC::FUSE_VALUE_OUT_OF_RANGE;
            }
            m_Records[lastRecordFound + 1 + i] = newRecords[i];
        }
    }
    Printf(GetPrintPriority(), "Dump Fuse Records to Regiser\n");
    UINT32 lwrrOffset = m_FuseSpec.fuselessStart;
    for (UINT32 i = 0;
         (i < m_Records.size()) || (lwrrOffset < m_FuseSpec.fuselessEnd); i++)
    {
        if (m_Records[i].chainId == 0)
        {
            lwrrOffset += m_Records[i].totalSize;
            continue;
        }
        // write ChainId
        CHECK_RC(WriteFuseSnippet(pFuseReg,
                                  m_Records[i].chainId,
                                  CHAINID_SIZE,
                                  &lwrrOffset));
        // write type
        CHECK_RC(WriteFuseSnippet(pFuseReg,
                                  m_Records[i].type,
                                  GetRecordTypeSize(),
                                  &lwrrOffset));
        // write Offset
        CHECK_RC(WriteFuseSnippet(pFuseReg,
                                  m_Records[i].offset,
                                  m_Records[i].offsetSize,
                                  &lwrrOffset));
        // write Data
        CHECK_RC(WriteFuseSnippet(pFuseReg,
                                  m_Records[i].data,
                                  m_Records[i].dataSize,
                                  &lwrrOffset));
        Printf(Tee::PriLow,
               "Rec %d|ChainId:0x%x|Type:0x%x|Offset:%d|Data:0x%x\n",
               i,
               m_Records[i].chainId,
               m_Records[i].type,
               GetBitOffsetInChain(m_Records[i].offset),
               m_Records[i].dataSize);
    }
    return OK;
}
//-----------------------------------------------------------------------------
void LwLinkFuse::PrintRecords()
{
    Printf(GetPrintPriority(), "Fuse Records:\n"
                           "Rec  Data    Offset   type  Chain\n"
                           "---  -----   ------   ---   ------\n");
    for (UINT32 i = 0; i < m_Records.size(); i++)
    {
        if ((m_Records[i].chainId == 0) &&
            (m_Records[i].data == 0) &&
            (m_Records[i].type == 0) &&
            (m_Records[i].offset == 0))
        {
            // don't print records that is completely empty
            continue;
        }

        Printf(GetPrintPriority(), "%03d  0x%03x   0x%04x    %d    0x%02x\n",
               i, m_Records[i].data,
               GetBitOffsetInChain(m_Records[i].offset),
               m_Records[i].type,
               m_Records[i].chainId);
    }
}
//-----------------------------------------------------------------------------
// Assumption with WriteFuseSnippet: We assume that GenerateFuseRecords was
// created fuse records such that there will never be a transition from 1->0
// So we ALWAYS just OR the actual value into the
RC LwLinkFuse::WriteFuseSnippet(vector<UINT32> *pFuseRegs,
                                  UINT32 valToOR,
                                  UINT32 numBits,
                                  UINT32 *pLwrrOffset)
{
    UINT32 lsb = *pLwrrOffset % 32;
    UINT32 msb = lsb + numBits - 1;
    UINT32 row = *pLwrrOffset / 32;

    if (row >= pFuseRegs->size())
    {
        Printf(Tee::PriNormal, "fuse write out of bounds\n");
        Printf(Tee::PriNormal, "Row = %d, LwrrOffset = %d pFuseRegs->size=%d\n",
               row, *pLwrrOffset, (UINT32)pFuseRegs->size());
        return RC::SOFTWARE_ERROR;
    }

    // OR the lower bits first
    (*pFuseRegs)[row] |= (valToOR << lsb);
    Printf(Tee::PriLow, "  row %d = 0x%x", row, (*pFuseRegs)[row]);
    if (msb >= 32)
    {
        // set the upper bits
        UINT32 shiftForUpperBits = numBits - (32 - lsb);
        row++;
        (*pFuseRegs)[row] |= (valToOR >> shiftForUpperBits);
        Printf(Tee::PriLow, " |  row %d = 0x%x", row, (*pFuseRegs)[row]);
    }
    *pLwrrOffset += numBits;
    Printf(Tee::PriLow, " | offset = %d\n", *pLwrrOffset);
    return OK;
}
//-----------------------------------------------------------------------------
// The new 'fuseless' fuse allows 'instructions' to be blown into the fuses.
// The instructions (aka Fuse records) can encode one or more logical fuse. The
// fuse space consist of the traditional fuses and then fuse instructions:
//     |-----------------------------| <== Bit 0
//     |  traditional fuses          |
//     |  eg. SPARE_BITS, COMPUTE_EN |
//     |                             |
//     |_____________________________|
//     |  Fuse instructions          | <== START_BIT
//     | (eg. TPC_DISABLE  )         |
//     |   RECORD 1  |  RECORD 2     |
//     |-----------------------------|
//     |Record 3 (rec can be 32 bits)|
//     |-----------------------------|
//     |                             | <== END_BIT
RC LwLinkFuse::ReadInRecords()
{
    StickyRC rc;
    m_Records.clear();
    m_AvailableRecIdx = 0;

    vector<UINT32> fuseRegs;
    CHECK_RC(GetCachedFuseReg(&fuseRegs));

    // Find the beginning of IFF first
    for (UINT32 lwrrOffset = m_FuseSpec.fuselessStart;
         lwrrOffset < m_FuseSpec.fuselessEnd;
         lwrrOffset += REPLACE_RECORD_SIZE)
    {
        UINT32 tempOffset = lwrrOffset + CHAINID_SIZE;
        UINT32 type = GetFuseSnippet(fuseRegs,
                                     GetRecordTypeSize(),
                                     &tempOffset);

        if (type == INIT_FROM_FUSE)
        {
            // Found the start of IFF, and thus the end of fuseless fuses
            m_FuseSpec.fuselessEnd = lwrrOffset;
            break;
        }
    }

    UINT32 lwrrOffset = m_FuseSpec.fuselessStart;
    while (lwrrOffset < m_FuseSpec.fuselessEnd)
    {
        FuseRecord newRec = {0};
        // get ChainID
        newRec.chainId = GetFuseSnippet(fuseRegs, CHAINID_SIZE, &lwrrOffset);
        if (newRec.chainId == 0)
        {
            // NULL instruction (empty location)
            // add 11 bits to the location of next instruction. All instruction
            // needs to be 16 bits aligned.

            // When there are leading NULL records make sure we increment fuse
            // by 32 bits each time
            lwrrOffset += REPLACE_RECORD_SIZE - CHAINID_SIZE;

            // use replace records only for now
            newRec.totalSize = REPLACE_RECORD_SIZE;
            // Here's why we don't increment m_AvailableRecIdx:
            // This has to do with the way HW scans the fuse records -> from top
            // to bottom, and if there are overlapping window, the bottom wins.
            // Imagine this:
            //
            // 3  Record 3
            // 4   NULL   ---> bubble (bad bad bad) should never happen
            // 5   Record 4
            // 6   Record 5
            // 7   NULL
            // 8   NULL
            // 9   NULL
            // It is unsafe to put anything in slot (4) because if any future
            // record that has an overlapping window with slot 4, the new settings
            // in slot 4 is not going to take any effect! So skip 4.
            m_Records.push_back(newRec);
            continue;
        }
        // get instruction type
        // Assumption: there are no 'bubble instrucitons' - case where we have
        // empty 32 bits of fuse cells between two records (can't think of a
        // reason SLT / ATE would ever do this)
        newRec.type = GetFuseSnippet(fuseRegs,
                                     GetRecordTypeSize(),
                                     &lwrrOffset);
        if (newRec.type == GetReplaceRecordBitPattern())
        {
            newRec.offsetSize = GetRecordAddressSize();

            // The offset we program into a fuse record may have to be cooked
            // depending on the chip family
            newRec.offset = GetFuseSnippet(fuseRegs,
                                           GetRecordAddressSize(),
                                           &lwrrOffset);
            newRec.dataSize = GetRecordDataSize();
            newRec.totalSize = REPLACE_RECORD_SIZE;
            newRec.data = GetFuseSnippet(fuseRegs,
                                         GetRecordDataSize(),
                                         &lwrrOffset);
            m_Records.push_back(newRec);
            m_AvailableRecIdx = static_cast<UINT32>(m_Records.size());
        }
        else // this is not a replace record, handle it by chip family
        {
            CHECK_RC(ProcessNonRRFuseRecord(newRec.type,
                                            newRec.chainId,
                                            fuseRegs,
                                            &lwrrOffset));
        }
    }

    rc = RereadJtagChain();
    return rc;
}
//-----------------------------------------------------------------------------
// This is kind of like DRF_VAL for multiple rows of fuses
UINT32 LwLinkFuse::GetFuseSnippet(const vector<UINT32> &fuseRegs,
                                    UINT32 numBits,
                                    UINT32 *pLwrrOffset)
{
    MASSERT((numBits > 0) && pLwrrOffset);

    UINT32 retVal = 0;
    UINT32 lsb = *pLwrrOffset % 32;
    UINT32 msb = lsb + numBits - 1;
    UINT32 row = *pLwrrOffset / 32;

    if ((row >= fuseRegs.size()) ||
        (msb >= 64))
    {
        Printf(Tee::PriNormal, "fuse read out of bounds\n");
        return 0;
    }

    UINT32 mask     = 0;
    if (msb < 32)
    {
        // most cases
        mask = 0xFFFFFFFF >> (31 - (msb - lsb));
        retVal = (fuseRegs[row] >> lsb) & mask;
    }
    else
    {
        // when Snippet spans across two words - OR them together
        UINT32 lowerBits = fuseRegs[row] >> lsb;
        UINT32 numLowerBits = 31 - lsb + 1;
        UINT32 upperMsb  = numBits - numLowerBits - 1;
        mask = 0xFFFFFFFF >> (31 - upperMsb);
        UINT32 upperBits = fuseRegs[row + 1] & mask;
        retVal = lowerBits | (upperBits << (numBits - upperMsb + 1));
    }

    *pLwrrOffset += numBits;
    return retVal;
}
//------------------------------------------------------------------------------
// Common method to handle ALL types of fuse records EXCEPT replace records
// (Fermi/Kepler style fuse records)
//------------------------------------------------------------------------------
/* virtual */ RC LwLinkFuse::ProcessNonRRFuseRecord
(
    UINT32 recordType,
    UINT32 chainId,
    const vector<UINT32> &fuseRegs,
    UINT32 *pLwrrOffset
)
{
    RC rc;
    switch (recordType)
    {
        case REPLACE_REC:
            Printf(Tee::PriNormal,
                   "Unable to process replace records\n");
            rc = RC::BAD_PARAMETER;
            break;
        case INIT_FROM_FUSE:
            // Ignore IFF fuses and but don't fail MODS
            // They are handled as column-based fuses in gt21x_fuse.cpp
            if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
            {
                Printf(Tee::PriNormal,
                    "IFF fuse detected @ row %d\n", *pLwrrOffset / 32);
            }

            // If we hit an IFF fuse record that means no more fuseless
            // fuses can be encountered further
            // Seek to end of previous fuse record
            *pLwrrOffset -= (CHAINID_SIZE + GetRecordTypeSize());

            // Update the fuseless end marker
            m_FuseSpec.fuselessEnd = *pLwrrOffset;
            break;
        default:
            Printf(Tee::PriNormal,
                   "unknown record type %d\n", recordType);
            rc = RC::SOFTWARE_ERROR;
            break;
    }
    return rc;
}
//-----------------------------------------------------------------------------
// This takes one fuse that we need to burn and update the fuse records. Each
// fuse can have one or more subfuses.
// Note: multiple fuses can share one fuse record (different location of data
// bits in the data field:
// Example:  format of Replace Record
// |------------------------------------------------------------------------|
// | Data (11 bits)     | Offset (13 bits)  |Type (3 bits)| ChainId (5 bits)|
// | 11110000101 (0x785)|0000000100110(0x26)|  111        |  0x1            |
// |------------------------------------------------------------------------|
// The example above is one fuse record, but can represent 2 sub fuses. See the
// data field:
// 1) bit 4-7 of OPT_TPC_DISABLE (second subfuse)
// 2) bit 0-3 of OPT_ZLWLL_DISABLE (first subfuse)
RC LwLinkFuse::UpdateRecord(const FuseUtil::FuseDef &def, UINT32 value)
{
    if (def.Type != "fuseless")
        return OK;

    RC rc;
    list<FuseUtil::FuseLoc>::const_iterator subFuseIter = def.FuseNum.begin();
    Printf(GetPrintPriority(), "----- update %s -----:user needs 0x%x\n",
           def.Name.c_str(), value);

    // What is a subfuse?? (got to love how HW folks break down fuse into subfuses)
    // A feature fuse (eg. OPT_TPC_DISABLE) can be composed of multiple subfuses.
    // On GF100, OPT_TPC_DISABLE, can be broken down into 4 subfuses, each of them 4 bits long
    // The location of the subfuse in the JTAG chain is described in the XML. The
    // XML also have a 'suggested' way of telling SLT/MODS how to put the subfuse
    // into a fuse record.
    // The Offset field describe where the LSB of Data in the record relative to
    // a JTAG chain (specified by ChainId)
    // Each fuse record has a 11 bit data window. The subfuse may or may not be
    // in the entire record window; so a subfuse can potentially spam across
    // multiple records (YUCK!)
    //
    // For each subfuse in each fuse
    // 1) See if there already is an existing record on the subfuse
    // 2) Modify the record if necessary
    // 3) Create new record if the original records are insufficient
    //
    // So how do we put subfuses together to become a 'feature fuse'?
    // The subfuses are specified in least signficant bits first format in the XML
    // So for OPT_TPC_DISABLE Value of 0x1234, the first subfuse value would be 4,
    // second would be 3, then 2 and 1.
    // User specifies 'Value', so for each subfuse, we shift down a portion of the
    // subfuse we need to update
    //
    for (; subFuseIter != def.FuseNum.end(); subFuseIter++)
    {
        const UINT32 numBitsInSubFuse = subFuseIter->Msb - subFuseIter->Lsb + 1;
        const UINT32 recordDataSize = GetRecordDataSize();

        if (numBitsInSubFuse > recordDataSize)
        {
            //split this subfuse into sub-records each of size recordDataSize
            UINT32 numSubRecords = (numBitsInSubFuse + recordDataSize - 1)/recordDataSize;

            for (UINT32 i = 0; i < numSubRecords; i++)
            {
                FuseUtil::FuseLoc subRecord = *subFuseIter;
                subRecord.Lsb = subFuseIter->Lsb + i * recordDataSize;
                subRecord.Msb = min(subRecord.Lsb + recordDataSize - 1, subFuseIter->Msb);

                subRecord.ChainOffset = (subRecord.Lsb / recordDataSize) * recordDataSize;
                subRecord.DataShift = subRecord.Lsb - subRecord.ChainOffset;

                const UINT32 numBits = subRecord.Msb - subRecord.Lsb + 1;
                const UINT32 newDataField = value & ((1<<numBits) - 1);
                CHECK_RC(UpdateSubFuseRecord(subRecord, newDataField));
                value >>= numBits;
            }
        }
        else
        {
            const UINT32 newDataField = value & ((1<<numBitsInSubFuse) - 1);
            CHECK_RC(UpdateSubFuseRecord(*subFuseIter, newDataField));
            value >>= numBitsInSubFuse;
        }

    } // end for each subfuse
    return OK;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::UpdateSubFuseRecord(FuseUtil::FuseLoc subFuse, UINT32 newDataField)
{
    RC rc;

    UINT32 numBitsInSubFuse = subFuse.Msb - subFuse.Lsb + 1;
    // SubFuseMask: this is the bits that each loop works on. Should be 0
    // by the end of this loop
    UINT32 subFuseMask  = (1<<numBitsInSubFuse) - 1;
    UINT32 chainMsb  = subFuse.Msb;
    UINT32 chainLsb  = subFuse.Lsb;
    UINT32 oldChainData = GetJtagChainData(subFuse.ChainId, chainMsb, chainLsb);

    Printf(GetPrintPriority(),
            "Chain=%d|Offset=%d|DataShift=%d|NewDataField=0x%x|Msb=%d|Lsb=%d\n",
            subFuse.ChainId,
            subFuse.ChainOffset,
            subFuse.DataShift,
            newDataField,
            chainMsb,
            chainLsb);

    if (oldChainData == newDataField)
    {
        // sub fuse was already blown correctly
        return OK;
    }
    // Scan from the back of the records - JTAG scans the record forward,
    // so in case of duplicates (overwrites), the last record is the one
    // that the hardware sees.
    // 1) For each record, find  the overlap window of the JTAG chain in
    // the record
    // 2) based on overlap, determine whether there is a 0 -> 1 transition
    //    0 -> 1 transition means a new record needs to be added
    for (INT32 i = static_cast<INT32>(m_Records.size())-1; i >= 0; i--)
    {
        if (0 == subFuseMask)
        {
            // no more bits to worry about - done
            break;
        }

        // only Replace record is supported
        if (m_Records[i].type != GetReplaceRecordBitPattern())
        {
            continue;
        }

        // Note:
        // In Fermi/Kepler, the offset is directly programmed into raw fuses
        // In Maxwell, the raw fuse record contains a block ID to a 16-bit
        // block in the JTAG chain
        UINT32 offset = GetBitOffsetInChain(m_Records[i].offset);

        // record window
        UINT32 recWindowMsb = offset + GetRecordDataSize() - 1;
        UINT32 recWindowLsb = offset;

        // make sure the record exists and fits inside the JTAG window:
        if ((m_Records[i].chainId != subFuse.ChainId) ||
                (recWindowLsb > chainMsb) ||
                (recWindowMsb < chainLsb))
        {
            continue;
        }

        UINT32 oldRecData = GetJtagChainData(m_Records[i].chainId,
                                             recWindowMsb,
                                             recWindowLsb);
        Printf(GetPrintPriority(),
            "  OldData@record[%d] = 0x%x. SubfuseMask=0x%x Rec(Msb,Lsb)=%d,%d\n",
            i, oldRecData, subFuseMask, recWindowMsb, recWindowLsb);

        UINT32 caseNum           = 0;
        UINT32 numOverlapBits    = 0;
        UINT32 numNonOverlapBits = 0;
        UINT32 newRecData        = oldRecData;
        UINT32 overlapMask       = 0;
        UINT32 shiftedSubfuseMask;
        if ((recWindowLsb <= chainMsb) && (recWindowLsb >= chainLsb))
        {
            // Example
            // Suppose NewData field = 0xC3
            //            case 1 Overlap like this
            //                     ChainMsb              chainLsb
            //         NewDataField:  |----------------------|
            //                        |24|23|22|       |18|17|
            //                                 |<---NumNonOverlapBits
            //                                             -->
            //                             RecWinLsb
            // |32|31|30              |24|23|22|
            // |-------------------------------|   Record offset = 22
            //
            // NumOverlapBits = 24 - 22 + 1 = 3
            // NumNonOverlapBits = 22 - 17 = 5
            // OverlapMask = 0x7
            numOverlapBits    = chainMsb - recWindowLsb + 1;
            numNonOverlapBits = recWindowLsb - chainLsb;
            // suppose SubFuseMask was b'1010 1111
            // ShiftedSubfuseMask becomse b'101 (chain# 24-22)
            shiftedSubfuseMask = subFuseMask >> numNonOverlapBits;
            overlapMask = (1<<numOverlapBits) - 1;
            // Clear the overlap bits - make sure we clear only the bits
            // that hasn't been worked on (indicated by ShiftedSubfuseMask)
            newRecData &= ~(overlapMask & shiftedSubfuseMask);
            newRecData |= (newDataField >> numNonOverlapBits) & shiftedSubfuseMask;
            // clear fuse 24 to 22 on the subfuse mask
            // SubFuseMask = b'0000 1111
            subFuseMask &= ~(overlapMask << numNonOverlapBits);
            caseNum = 1;
        }
        else if ((recWindowMsb >= chainLsb) && (recWindowMsb <= chainMsb))
        {
            // suppose NewDataField = 0x00E
            //
            //            case 2 Overlap like this
            //
            //           ChainMsb                  chainLsb
            //NewDataField |--------------------------|
            //             |20|19|18|17|16|15|14|13|12|      (9 bits total in this subfuse)
            //                                        | <--NumNonOverlapBits --->
            //                         RecWindowMsb                        RecWindowLsb
            //                               |14|13|12|                 |06|05|04|
            //      record offset = 4        |-----------------------------------|
            //
            // NumOverlapBits = 14 - 12 + 1 = 3
            // NumNonOverlapBits = 12 - 4 = 8
            // OverlapMask = 0x7
            numOverlapBits = recWindowMsb - chainLsb + 1;
            numNonOverlapBits = chainLsb - recWindowLsb;
            overlapMask = (1<<numOverlapBits) - 1;
            // suppose SubFuseMask was b' 1 1100 1011
            // ShiftedSubfuseMask is   b'011
            shiftedSubfuseMask = subFuseMask & overlapMask;

            // Suppose record was originally b'111 1110 1010
            //                now it becomes b'100 1110 1010
            newRecData &= ~((overlapMask & shiftedSubfuseMask) << numNonOverlapBits);
            // NewRecData = NewRecData | ((0xE & 0x3)<<8
            //                             = b'110 1110 1010
            newRecData |= (newDataField & shiftedSubfuseMask) << numNonOverlapBits;
            // clear fuse 12-14 on the SubfuseMask -> b' 1 1100 1000
            subFuseMask &= ~shiftedSubfuseMask;
            caseNum = 2;
        }
        else
        {
            //                case3:   subfuse is entirely in the record
            // suppose NewDataField = 0x3A
            //
            //            case 3 Overlap like this
            //                        ChainMsb                  chainLsb
            //    NewDataField            |-----------------------|
            //                            |18|17|16|15|14|13|12|11|  (8 bits total in this subfuse)
            //
            //                     RecWindowMsb                    RecWindowLsb
            //                         |19|18|                 |11|10|09|
            //   record offset = 4     |--------------------------------|
            //
            //
            // ShiftUp 11 - 9 = 2
            UINT32 shiftUp = chainLsb - recWindowLsb;
            newRecData &= ~(subFuseMask << shiftUp);
            newRecData |= (newDataField & subFuseMask) << shiftUp;
            // should be no more subfuse bits to set since it is already in the record
            subFuseMask = 0;
            caseNum = 3;
        }
        Printf(GetPrintPriority(), "  **case %d **, SubFuseMask = 0x%x\n",
        caseNum, subFuseMask);

        // determine if we can use the existing record by just OR the
        // data field of current record
        UINT32 oneToZeroMask = oldRecData & (~newRecData);
        if (oneToZeroMask)
        {
            // we have a 1->0 transition in one of the bits, so need
            // to overwrite this record and create a new one. Also,
            // make sure we don't lose the other fuse bits that are not
            // part of the current subfuse - store this in OldData
            // clear the bits for the new field
            Printf(GetPrintPriority(), "  discard record #%d :"
                                        "offset:%d, ChaindId %d\n",
                                        i, offset, m_Records[i].chainId);

            Printf(GetPrintPriority(), "   overwrite old record required\n");
            CHECK_RC(AppendRecord(subFuse.ChainId,
                                  offset,
                                  newRecData));
        }
        else if (oldRecData != newRecData)
        {
            // Update the existing fuse record (no change in data offset)
            Printf(GetPrintPriority(), "  old record %d (old, new) = (0x%x, 0x%x)\n",
                                    i, oldRecData, newRecData);
            m_Records[i].data = newRecData;
        }

        // reread the entire JTAG chain from m_Records
        CHECK_RC(RereadJtagChain());
    }   // end search all records

    // SubFuseMask remains non zero -> so more fuses to blow!
    // Just create a new record using default location (in XML). The default
    // location will guarantee that the subfuse will fit in the new records.
    if ((newDataField & subFuseMask) != 0)
    {
        UINT32 dataField;
        // overflow will happens when:
        if ((subFuse.ChainOffset+GetRecordDataSize()-1) < subFuse.Msb)
        {
            // example: GK107 OPT_TPC_DISABLE
            //          <key>blockIndex</key>
            //          <str>49</str>            -> record only supports 49-59
            //          <key>blockOffset</key>
            //          <str>10</str>
            //          <key>chainId</key>
            //          <str>1</str>
            //          <key>lsbIndex</key>
            //          <str>59</str>
            //          <key>msbIndex</key>
            //          <str>60</str>            -> bit 60 overflows and needs its own record
            //          <key>name</key>
            //          <str>opt_tpc_disable_0</str>
            UINT32 newLsb = subFuse.ChainOffset + GetRecordDataSize();
            dataField = GetJtagChainData(subFuse.ChainId,
                                         newLsb + GetRecordDataSize() - 1,
                                         newLsb);
            UINT32 numShifts       = newLsb - subFuse.Lsb;
            UINT32 overflowBits    = dataField;
            overflowBits &= ~(subFuseMask >> numShifts);
            overflowBits |= (newDataField >> numShifts);

            Printf(GetPrintPriority(),
                "   brand new record required for overflow OldData=0x%x, New=0x%x\n",
                dataField, overflowBits);
            CHECK_RC(AppendRecord(subFuse.ChainId,
                                    newLsb,
                                    overflowBits));
            CHECK_RC(RereadJtagChain());

            // clear the bits that have been updated
            UINT32 numOverflow = subFuse.Msb - subFuse.Lsb + 1 - numShifts;
            UINT32 bitsToClear = (1 << numOverflow) - 1;
            subFuseMask &= ~(bitsToClear << numShifts);
            newDataField &= ~(bitsToClear << numShifts);
        }

        // handle normal cases - if still required
        if ((newDataField & subFuseMask) != 0)
        {
            dataField = GetJtagChainData(subFuse.ChainId,
                                         subFuse.ChainOffset + GetRecordDataSize() - 1,
                                         subFuse.ChainOffset);
            Printf(GetPrintPriority(),
                "   brand new record required. SubFuseMask=0x%x, OldData=0x%x\n",
                subFuseMask, dataField);

            UINT32 bitsToClear = (1 << numBitsInSubFuse) - 1;
            dataField &= ~(bitsToClear << subFuse.DataShift);
            dataField |= newDataField << subFuse.DataShift;

            CHECK_RC(AppendRecord(subFuse.ChainId,
                                  subFuse.ChainOffset,
                                  dataField));
            CHECK_RC(RereadJtagChain());
        }
    }
    return OK;
}
//-----------------------------------------------------------------------------
// Append fuse records based on the chain offset, data, and chain ID
// defined in fuse XML
RC LwLinkFuse::AppendRecord(UINT32 chainId, UINT32 offset, UINT32 data)
{
    if (m_AvailableRecIdx == m_Records.size())
    {
        Printf(Tee::PriNormal, "No more space to burn fuse records\n");
        Printf(Tee::PriNormal, "m_AvailableRecIdx = %d m_Records.size=%d\n",
               m_AvailableRecIdx, (UINT32)m_Records.size());
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    // What is a record address ?
    // A record address tells the fuse engine how to locate the data payload
    // in a fuse record within the JTAG chain.
    //
    // Record addressing differs across chip families:
    // Fermi/Kepler:
    //     Fuse records can be addressed using any offset in the JTAG chain
    // Maxwell:
    //     Fuse records are always addressed as 16 bit blocks in the JTAG chain
    UINT32 recordAddress = GetRecordAddress(offset);
    UINT32 offsetInChain = GetBitOffsetInChain(recordAddress);

    m_Records[m_AvailableRecIdx].chainId    = chainId;
    m_Records[m_AvailableRecIdx].type       = GetReplaceRecordBitPattern();
    m_Records[m_AvailableRecIdx].offset     = recordAddress;
    m_Records[m_AvailableRecIdx].offsetSize = GetRecordAddressSize();

    if (offset != offsetInChain)
    {
        // In Maxwell if the actual offset is not aligned to the 16-bit block
        // we need to shift the data relative to a 16-bit boundary
        m_Records[m_AvailableRecIdx].data   = data << (offset - offsetInChain);
    }
    else
    {
        // in Fermi/Kepler we write to the offset in chain directly
        // (no concept of 16-bit block addressing in fuse records)
        m_Records[m_AvailableRecIdx].data   = data;
    }

    // Quick check to see if we are writing more than the data payload
    // to a fuse record
    UINT32 recordDataMask = ((1 << GetRecordDataSize()) - 1);
    if (m_Records[m_AvailableRecIdx].data > recordDataMask)
    {
        Printf(Tee::PriNormal,
               "Error: Detected overflow in data in fuse record\n");
        Printf(Tee::PriNormal,
               "m_AvailableRecIdx = %d m_Records.Data =0x%08x\n",
               m_AvailableRecIdx, m_Records[m_AvailableRecIdx].data);
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    m_Records[m_AvailableRecIdx].dataSize   = GetRecordDataSize();

    Printf(GetPrintPriority(), "  add new record(%d) offset,data = %d,0x%x\n",
           m_AvailableRecIdx, offset, data);

    m_AvailableRecIdx++;
    return OK;
}
//-----------------------------------------------------------------------------
UINT32 LwLinkFuse::GetJtagChainData(UINT32 chainId, UINT32 msb, UINT32 lsb)
{
    UINT32 retVal = 0;
    if (m_JtagChain.find(chainId) != m_JtagChain.end())
    {
        for (UINT32 i = lsb; i <= msb; i++)
        {
            if (m_JtagChain[chainId].find(i) != m_JtagChain[chainId].end())
            {
                if (m_JtagChain[chainId][i])
                {
                    retVal |= (1 << (i - lsb));
                }
            }
        }
    }
    return retVal;
}
//-----------------------------------------------------------------------------
// This creates a virtual Jtag chain for our own records
RC LwLinkFuse::UpdateJtagChain(const FuseRecord &record)
{
    // only support replace reocrds -> just skip over if it's empty
    if ((record.type != GetReplaceRecordBitPattern()) ||
        (record.chainId == ILWALID_CHAIN_ID))
    {
        return OK;
    }

    UINT32 chainId = record.chainId;
    UINT32 dataToShift = record.data;

    // using the offset and data size, mark each bit on the jtag chain
    for (UINT32 j = 0; j < GetRecordDataSize(); j++)
    {
        // each record has an offset from the base of the chain
        UINT32 bitOffsetInChain = j + GetBitOffsetInChain(record.offset);
        if (dataToShift & (1<<j))
        {
            m_JtagChain[chainId][bitOffsetInChain] = true;
        }
        else
        {
            m_JtagChain[chainId][bitOffsetInChain] = false;
        }
    }
    return OK;
}
//-----------------------------------------------------------------------------
RC LwLinkFuse::RereadJtagChain()
{
    RC rc;
    // update our virtual JTAG chain
    m_JtagChain.clear();
    for (UINT32 i = 0; i < m_Records.size(); i++)
    {
        CHECK_RC(UpdateJtagChain(m_Records[i]));
    }
    return rc;
}
//-----------------------------------------------------------------------------
string LwLinkFuse::GetFuseFilename()
{
    return "svnp01_f.json";
}
//-----------------------------------------------------------------------------
bool LwLinkFuse::PollFusesIdle(void *pWrapperParam)
{
    MASSERT(pWrapperParam);
    LwLinkFuse* pThis = static_cast<LwLinkFuse*>(pWrapperParam);
    TestDevice* pLwsDev = static_cast<TestDevice*>(pThis->GetDevice());

    if (!pThis->IsWaitForIdleEnabled())
        return true;

    return pLwsDev->GetInterface<LwLinkRegs>()->Regs().Test32(0, MODS_FUSE_FUSECTRL_STATE_IDLE);
}
//-----------------------------------------------------------------------------
bool LwLinkFuse::PollFuseSenseDone(void *pParam)
{
    MASSERT(pParam);
    auto* pLwsDev = static_cast<TestDevice*>(pParam);
    Platform::Delay(1);
    bool retval = pLwsDev->GetInterface<LwLinkRegs>()->Regs().Test32(0, MODS_FUSE_FUSECTRL_FUSE_SENSE_DONE_YES);
    return retval;
}
