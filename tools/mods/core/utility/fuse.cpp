/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2009-2013,2015-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/jscript.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include "core/include/massert.h"
#include "fuse.h"
#include "core/include/device.h"
#include "core/include/version.h"
#include "core/include/tasker.h"

#include <algorithm>
#include <list>

bool OldFuse::s_CheckInternalFuses = false;

OldFuse::OldFuse(Device *pDevice)
: m_pDevice(pDevice),
  m_UseUndoFuse(false),
  m_Parsed(false),
  m_pFuseDefMap(nullptr),
  m_pSkuList(nullptr),
  m_pMiscInfo(nullptr),
  m_pFuseDataSet(nullptr),
  m_PrintPri(Tee::PriNormal)
{
    MASSERT(pDevice);
    if ((Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK) &&
        s_CheckInternalFuses)
    {
        m_CheckLevel = LEVEL_ALWAYS;
    }
    else
    {
        m_CheckLevel = LEVEL_LWSTOMER;
    }

    m_Mutex = Tasker::AllocMutex("OldFuse::m_Mutex", Tasker::mtxUnchecked);
}

OldFuse::~OldFuse()
{
    Tasker::FreeMutex(m_Mutex);
}

//-----------------------------------------------------------------------------
RC OldFuse::FindSkuMatch(vector<string>* pFoundSkus, Tolerance Strictness)
{
    RC rc;
    if (!m_Parsed)
        CHECK_RC(ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet));

    bool optFusesOnly = CanOnlyUseOptFuses();

    if (!optFusesOnly)
    {
        CHECK_RC(CacheFuseReg());
    }

    FuseUtil::SkuList::const_iterator SkuIter = m_pSkuList->begin();
    for (; SkuIter != m_pSkuList->end(); SkuIter++)
    {
        if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
            Printf(Tee::PriDebug, "SKU %s\n", SkuIter->second.SkuName.c_str());

        bool anyFuseSupported = false;

        bool SkuMatch = true;
        list<FuseUtil::FuseInfo>::const_iterator FuseValIter =
            SkuIter->second.FuseList.begin();
        for (; FuseValIter != SkuIter->second.FuseList.end(); FuseValIter++)
        {
            UINT32 GotValue = 0;
            bool isFuseSupported;
            bool fuseRight = optFusesOnly ?
                IsOptFuseRight(*FuseValIter, &GotValue, Strictness, &isFuseSupported) :
                IsFuseRight(*FuseValIter, &GotValue, Strictness, &isFuseSupported);
            anyFuseSupported = anyFuseSupported || isFuseSupported;
            if (!fuseRight)
            {
                Printf(Tee::PriDebug, "SKU %s mismatched on %s\n",
                    SkuIter->second.SkuName.c_str(), FuseValIter->pFuseDef->Name.c_str());
                SkuMatch = false;
                break;
            }
        }

        // IFFs don't have opt fuses
        if (!optFusesOnly)
        {
            // If we're using IFF patches, check that what we've blown is correct
            auto iffPatches = SkuIter->second.iffPatches;
            if (!iffPatches.empty())
            {
                vector<FuseUtil::IFFRecord> skuIff, chipIff;
                for (auto& patch: iffPatches)
                {
                    skuIff.insert(skuIff.end(), patch.rows.begin(), patch.rows.end());
                }

                // Fuse.JSON specifies IFF in write-order, but we need read-order (reversed)
                reverse(skuIff.begin(), skuIff.end());

                GetIffRecords(&chipIff, false);

                if (skuIff != chipIff)
                {
                    SkuMatch = false;
                }
            }
        }

        if (!anyFuseSupported)
        {
            if (Utility::GetSelwrityUnlockLevel() >= Utility::SUL_LWIDIA_NETWORK)
            {
                Printf(Tee::PriDebug, "SKU %s : No supporting fuses were found for matching\n",
                                       SkuIter->second.SkuName.c_str());
            }
        }
        else if (SkuMatch)
        {
            pFoundSkus->push_back(SkuIter->second.SkuName.c_str());
        }
    }
    return OK;
}
//-----------------------------------------------------------------------------
RC OldFuse::CheckAteFuses(string ChipSku)
{
    RC rc;
    if (!m_Parsed)
        CHECK_RC(ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet));

    // Cache the fuse registers first
    CHECK_RC(CacheFuseReg());

    // find the ChipSku we want to check
    FuseUtil::SkuList::const_iterator ChipSkuIter;
    ChipSkuIter = m_pSkuList->find(ChipSku);
    if (ChipSkuIter == m_pSkuList->end())
    {
        Printf(Tee::PriNormal, "unknown chip sku\n");
        return RC::BAD_PARAMETER;
    }

    // Go through the list of fuses for the desired chip sku
    list<FuseUtil::FuseInfo>::const_iterator FuseInfoIter =
        ChipSkuIter->second.FuseList.begin();

    bool AteCheckFailed = false;
    for (; FuseInfoIter != ChipSkuIter->second.FuseList.end(); FuseInfoIter++)
    {
        if (!(FuseInfoIter->Attribute & FuseUtil::FLAG_READONLY))
            continue;

        UINT32 GotValue;
        if (!IsFuseRight(*FuseInfoIter, &GotValue, ALL_MATCH))
        {
            AteCheckFailed = true;
            break;
        }
    }

    if (AteCheckFailed)
    {
        PrintFuseValues(ChipSku, BAD_FUSES, ALL_MATCH, FuseUtil::FLAG_READONLY);
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }

    return OK;
}
//-----------------------------------------------------------------------------
RC OldFuse::CheckFuse(const string FuseName, const string Specs)
{
    RC rc;
    if (!m_Parsed)
        CHECK_RC(ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet));

    CHECK_RC(CacheFuseReg());

    FuseUtil::FuseInfo NewFuse;

    // get the XML definition [fuse bit locations]
    if (m_pFuseDefMap->find(Utility::ToUpperCase(FuseName)) == m_pFuseDefMap->end())
    {
        Printf(Tee::PriNormal, "%s is unknown\n", FuseName.c_str());
        return RC::BAD_PARAMETER;
    }
    NewFuse.pFuseDef = &m_pFuseDefMap->find(Utility::ToUpperCase(FuseName))->second;

    // get fuse attribute and expected value based on the spec in string
    CHECK_RC(FuseUtil::ParseAttributeAndValue(Specs, &NewFuse));

    // check if this fuse matches the spec user passed in
    UINT32 GotVal;
    if (!IsFuseRight(NewFuse, &GotVal, ALL_MATCH))
    {
        Printf(Tee::PriNormal, "Fuse %s has value/burncount %d. This is wrong\n",
               NewFuse.pFuseDef->Name.c_str(), GotVal);
        return RC::FUSE_VALUE_OUT_OF_RANGE;
    }
    return OK;
}
//-----------------------------------------------------------------------------
RC OldFuse::GetFusesInfo(string            ChipSku,
                      WhichFuses        Priority,
                      Tolerance         Strictness,
                      UINT32            AttributeMask,
                      vector<FuseVals> *pRetInfo)
{
    RC rc;
    MASSERT(pRetInfo);
    pRetInfo->clear();
    if (!m_Parsed)
        CHECK_RC(ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet));

    bool optFusesOnly = CanOnlyUseOptFuses();

    if (!optFusesOnly)
    {
        CHECK_RC(CacheFuseReg());
    }

    bool ChipSkuFound = true;
    FuseUtil::SkuList::const_iterator SkuIter;

    if (ChipSku.empty())
    {
        ChipSkuFound = false;
    }
    else
    {
        SkuIter = m_pSkuList->find(ChipSku);
        if (SkuIter == m_pSkuList->end())
        {
            ChipSkuFound = false;
            Printf(m_PrintPri, "Chip sku: %s not found in XML\n", ChipSku.c_str());
        }
    }

    if (!ChipSkuFound)
    {
        SkuIter = m_pSkuList->begin();
        Printf(Tee::PriWarn, "Assuming first SKU found in fuse file (%s)\n",
               SkuIter->first.c_str());
    }

    for (const auto& fuseInfo: SkuIter->second.FuseList)
    {
        if (!IsFuseSupported(fuseInfo, Strictness) ||
            (optFusesOnly && !HasOptFuse(fuseInfo.pFuseDef)))
        {
            continue;
        }

        FuseVals NewFuse;
        NewFuse.Name = fuseInfo.pFuseDef->Name;
        if (ChipSkuFound)
        {
            if (!(AttributeMask & fuseInfo.Attribute))
            {
                continue;
            }

            UINT32 GotValue = 0;
            bool FuseIsRight = optFusesOnly ?
                IsOptFuseRight(fuseInfo, &GotValue, Strictness) :
                IsFuseRight(fuseInfo, &GotValue, Strictness);
            // don't push if user asks for good and fuse is incorrect or
            // if user asks for bad fuses and get correct fuses
            if (((Priority == BAD_FUSES)&& FuseIsRight) ||
                ((Priority == GOOD_FUSES)&& !FuseIsRight))
            {
                continue;
            }
            NewFuse.Attribute   = fuseInfo.Attribute;
            NewFuse.ActualVal   = GotValue;
            NewFuse.ExpectedStr = fuseInfo.StrValue;
        }
        else
        {
            if (fuseInfo.pFuseDef->Type == FuseUtil::TYPE_PSEUDO)
            {
                continue;
            }
            NewFuse.Attribute = 0;
            NewFuse.ActualVal = ExtractFuseVal(fuseInfo.pFuseDef, Strictness);
            NewFuse.ExpectedStr = "unknown";
        }
        pRetInfo->push_back(NewFuse);
    }
    return rc;
}

//-----------------------------------------------------------------------------
// Print Fuse values for fuses that have an OPT reg value
// This calls GetFusesInfo, which is protected by IsFuseSupported(), so
// this is safe with Official builds.
RC OldFuse::PrintFuseValues(string        ChipSku,
                         WhichFuses    WhatToPrint,
                         Tolerance     Strictness,
                         UINT32        AttributeMask)
{
    RC rc;
    vector<FuseVals> FusesToPrint;

    AppendIgnoreList("OPT_IDDQ");
    AppendIgnoreList("OPT_SPEEDO0");

    CHECK_RC(GetFusesInfo(ChipSku,
                          WhatToPrint,
                          Strictness,
                          AttributeMask,
                          &FusesToPrint));

    for (UINT32 i = 0; i < FusesToPrint.size(); i++)
    {
        // don't print 'Good'/'Bad' fuses if the expected value is unknown.
        if ((FusesToPrint[i].ExpectedStr == "unknown") &&
            (WhatToPrint != ALL_FUSES))
        {
            continue;
        }

        if (WhatToPrint == BAD_FUSES)
        {
            string ErrorMsg = Utility::StrPrintf("  Fuse %s ",
                                                 FusesToPrint[i].Name.c_str());
            UINT32 ActualVal = FusesToPrint[i].ActualVal;
            const FuseUtil::FuseDef *pDef = GetFuseDefFromName(FusesToPrint[i].Name.c_str());

            if (HasOptFuse(pDef))
            {
                UINT32 OptFuseVal = GetOptFuseValGivenName(
                                                  FusesToPrint[i].Name.c_str());
                if (!DoesOptAndRawFuseMatch(pDef, ActualVal, OptFuseVal))
                {
                    ErrorMsg += Utility::StrPrintf(
                                           "has Raw/Opt mismatch : 0x%x/0x%x\n",
                                           ActualVal, OptFuseVal);
                    Printf(Tee::PriNormal, "%s", ErrorMsg.c_str());
                    continue;
                }
            }
            if (FusesToPrint[i].Attribute & FuseUtil::FLAG_ATE_NOT)
            {
                ErrorMsg += "should not be ";
            }
            else if (FusesToPrint[i].Attribute & FuseUtil::FLAG_BITCOUNT)
            {
                ErrorMsg += "should have bit count ";
                ActualVal = (UINT32)Utility::CountBits(ActualVal);
            }
            else
            {
                ErrorMsg += "should be ";
            }
            string ButMsg = Utility::StrPrintf("%s, but is 0x%x\n",
                                               FusesToPrint[i].ExpectedStr.c_str(),
                                               ActualVal);
            ErrorMsg += ButMsg;
            Printf(Tee::PriNormal, "%s", ErrorMsg.c_str());
        }
        else
        {
            Printf(Tee::PriNormal, "  Fuse %s: 0x%x\n",
                   FusesToPrint[i].Name.c_str(),
                   FusesToPrint[i].ActualVal);
        }
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC OldFuse::GetDesiredFuses(string ChipSku, vector<UINT32> *pRegsToBlow)
{
    MASSERT(pRegsToBlow);

    RC rc;
    // Cache the fuse registers first
    CHECK_RC(CacheFuseReg());
    pRegsToBlow->resize(GetCachedFuseSize());

    CHECK_RC(HandleColumnBasedFuses(ChipSku, pRegsToBlow));

    // handle fuseless fuses
    CHECK_RC(ReadInRecords());
    CHECK_RC(RegenerateRecords(ChipSku));
    CHECK_RC(DumpFuseRecToRegister(pRegsToBlow));

    // handle IFF fuses
    CHECK_RC(ApplyIffPatches(ChipSku, pRegsToBlow));

    for (UINT32 i = 0; i < pRegsToBlow->size(); i++)
    {
        Printf(Tee::PriLow, "reg %03d : = 0x%08x\n", i, (*pRegsToBlow)[i]);
    }
    return rc;
}
//-----------------------------------------------------------------------------
RC OldFuse::CacheFuseReg()
{
    Printf(Tee::PriNormal, "CacheFuseReg not implemented for this device\n");
    // this is not always required (eg. MCP fuse checks for GPU MODS)
    return OK;
}
//-----------------------------------------------------------------------------
void OldFuse::AppendIgnoreList(string FuseName)
{
    m_IgnoredFuseNames.insert(Utility::ToUpperCase(FuseName));
}
//-----------------------------------------------------------------------------
void OldFuse::ClearIgnoreList()
{
    m_IgnoredFuseNames.clear();
}
//-----------------------------------------------------------------------------
bool OldFuse::IsFuseSupported(const FuseUtil::FuseInfo &Spec, Tolerance Strictness)
{
    string FuseName = Utility::ToUpperCase(Spec.pFuseDef->Name);
    // note: ignoredFuseNames are set by user
    return (m_IgnoredFuseNames.count(FuseName) == 0);
}
//-----------------------------------------------------------------------------
// Private Funcitons
//-----------------------------------------------------------------------------
RC OldFuse::ParseXml(const FuseUtil::FuseDefMap **ppFuseDefMap,
                  const FuseUtil::SkuList    **ppSkuList,
                  const FuseUtil::MiscInfo   **ppMiscInfo,
                  const FuseDataSet          **ppFuseDataSet)
{
    RC rc;
    CHECK_RC(ParseXmlInt(ppFuseDefMap, ppSkuList, ppMiscInfo, ppFuseDataSet));

    if ((*ppFuseDefMap)->empty())
    {
        Printf(Tee::PriError, "No chip fuses were found in fuse file\n");
        return RC::SOFTWARE_ERROR;
    }
    if ((*ppSkuList)->empty())
    {
        Printf(Tee::PriError, "No chip SKUs were found in fuse file\n");
        return RC::SOFTWARE_ERROR;
    }

    m_Parsed = true;
    return rc;
}
//-----------------------------------------------------------------------------
//! Brief: Check if a fuse in a chip is properly burnt
bool OldFuse::IsFuseRight
(
    const FuseUtil::FuseInfo &Spec,
    UINT32 *pGotValue,
    Tolerance Strictness,
    bool *pIsSupported
)
{
    MASSERT(pGotValue);
    MASSERT(pIsSupported);

    *pIsSupported = IsFuseSupported(Spec, Strictness);
    if (!(*pIsSupported))
    {
        return true;
    }

    UINT32 FuseValue = ExtractFuseVal(Spec.pFuseDef, Strictness);
    // see https://wiki.lwpu.com/gpunotebook/index.php/MODS/Fuse_XML
    *pGotValue = FuseValue;
    if (Spec.Attribute & FuseUtil::FLAG_DONTCARE)
    {
        return true;
    }

    // Raw fuse value and OptFuse value must agree
    if (HasOptFuse(Spec.pFuseDef))
    {
        UINT32 OptFuseVal = ExtractOptFuseVal(Spec.pFuseDef);
        if (!DoesOptAndRawFuseMatch(Spec.pFuseDef, FuseValue, OptFuseVal))
        {
            Printf(Tee::PriDebug, "Raw/Opt mismatch @%s: 0x%x/0x%x\n",
                   Spec.pFuseDef->Name.c_str(), FuseValue, OptFuseVal);
            return false;
        }
    }

    return IsFuseValueRight(Spec, FuseValue);
}

bool OldFuse::IsFuseRight
(
    const FuseUtil::FuseInfo &Spec,
    UINT32 *pGotValue,
    Tolerance Strictness
)
{
    bool isSupported;
    return IsFuseRight(Spec, pGotValue, Strictness, &isSupported);
}

//-----------------------------------------------------------------------------
//! Brief: Check if a fuse in a chip is properly burnt
bool OldFuse::IsOptFuseRight
(
    const FuseUtil::FuseInfo& spec,
    UINT32* pGotValue,
    Tolerance strictness,
    bool *pIsSupported
)
{
    MASSERT(pGotValue);
    MASSERT(pIsSupported);

    *pIsSupported = IsFuseSupported(spec, strictness) && HasOptFuse(spec.pFuseDef);
    if (!(*pIsSupported))
    {
        return true;
    }

    UINT32 fuseValue = ExtractOptFuseVal(spec.pFuseDef);
    // see https://wiki.lwpu.com/gpunotebook/index.php/MODS/Fuse_XML
    *pGotValue = fuseValue;
    if (spec.Attribute & FuseUtil::FLAG_DONTCARE)
    {
        return true;
    }

    return IsFuseValueRight(spec, fuseValue);
}

bool OldFuse::IsOptFuseRight
(
    const FuseUtil::FuseInfo &Spec,
    UINT32 *pGotValue,
    Tolerance Strictness
)
{
    bool isSupported;
    return IsOptFuseRight(Spec, pGotValue, Strictness, &isSupported);
}

//-----------------------------------------------------------------------------
//! Brief: Check if a fuse in a chip is properly burnt
bool OldFuse::IsFuseValueRight(const FuseUtil::FuseInfo &spec, UINT32 fuseVal)
{
    if (spec.Attribute & FuseUtil::FLAG_DONTCARE)
    {
        return true;
    }
    if (spec.Attribute & FuseUtil::FLAG_MATCHVAL)
    {
        // Valid if value is found
        return (std::find(spec.Vals.begin(), spec.Vals.end(), fuseVal) != spec.Vals.end());
    }
    else if (spec.Attribute & FuseUtil::FLAG_BITCOUNT)
    {
        // this is correct as long as the total number of bits in FuseValue
        // matches the one specified in the XML file
        return ((UINT32)Utility::CountBits(fuseVal) == spec.Value);
    }
    else if (spec.Attribute & FuseUtil::FLAG_UNDOFUSE)
    {
        if ((spec.StrValue == "00") || (spec.StrValue == "0"))
        {
            return ((fuseVal == 0) || (fuseVal == 3));
        }
        else if ((spec.StrValue == "01") ||(spec.StrValue == "1"))
        {
            return (fuseVal == 1);
        }
        else if ((spec.StrValue == "1x") || (spec.StrValue == "10"))
        {
            return ((fuseVal == 2) || (fuseVal == 3) || (fuseVal == 0));
        }
        else
            return false;
    }
    else if (spec.Attribute & FuseUtil::FLAG_ATE_NOT)
    {
        // Valid if value is not found
        return (std::find(spec.Vals.begin(), spec.Vals.end(), fuseVal) == spec.Vals.end());
    }
    else if (spec.Attribute & FuseUtil::FLAG_ATE_RANGE)
    {
        for (auto& range: spec.ValRanges)
        {
            if (range.Min <= fuseVal && fuseVal <= range.Max)
            {
                return true;
            }
        }
    }
    return false;
}
//-----------------------------------------------------------------------------
//! Brief: Check if a fuse in a chip is properly burnt
bool OldFuse::IsPseudoFuseRight(const FuseUtil::FuseInfo &spec,
                             UINT32*                  pGotValue,
                             Tolerance                strictness)
{
    MASSERT(pGotValue);
    if (spec.Attribute & FuseUtil::FLAG_DONTCARE)
    {
        return true;
    }

    bool optFusesOnly = CanOnlyUseOptFuses();

    // Check the sub-fuses for OPT/RAW mismatches
    // since they won't be checked otherwise
    if (!optFusesOnly)
    {
        for (const auto& subFuse: spec.pFuseDef->subFuses)
        {
            auto subFuseDef = get<0>(subFuse);
            if (HasOptFuse(subFuseDef))
            {
                UINT32 subFuseVal = ExtractFuseVal(subFuseDef, strictness);
                UINT32 optFuseVal = ExtractOptFuseVal(subFuseDef);
                if (!DoesOptAndRawFuseMatch(subFuseDef, subFuseVal, optFuseVal))
                {
                    Printf(Tee::PriDebug, "Raw/Opt mismatch @%s[%s]: 0x%x/0x%x\n",
                           spec.pFuseDef->Name.c_str(), subFuseDef->Name.c_str(),
                           subFuseVal, optFuseVal);
                    return false;
                }
            }
        }
    }

    if (spec.Attribute & FuseUtil::FLAG_BITCOUNT)
    {
        // BurnCount is the only thing pseudo fuses support right now
        UINT32 burnCount = 0;

        for (const auto& subFuse: spec.pFuseDef->subFuses)
        {
            UINT32 bits = get<1>(subFuse) - get<2>(subFuse) + 1;
            UINT32 mask = ((1ULL << bits) - 1) << get<2>(subFuse);
            if (optFusesOnly && !HasOptFuse(spec.pFuseDef))
            {
                Printf(Tee::PriDebug, "Skipping fuse %s since it doesn't have an opt fuse",
                    spec.pFuseDef->Name.c_str());
            }
            else
            {
                UINT32 subVal = ExtractFuseVal(get<0>(subFuse), strictness) & mask;
                burnCount += Utility::CountBits(subVal);
            }
        }
        *pGotValue = burnCount;
        return burnCount == spec.Value;
    }
    else if (spec.Attribute & FuseUtil::FLAG_DONTCARE)
    {
        return true;
    }

    Printf(Tee::PriError, "Unsupported pseudo-fuse option: %#x", spec.Attribute);
    return false;
}
//-----------------------------------------------------------------------------
bool OldFuse::HasOptFuse(const FuseUtil::FuseDef* pDef)
{
    return pDef->PrimaryOffset.Offset != FuseUtil::UNKNOWN_OFFSET;
}
//-----------------------------------------------------------------------------
const FuseUtil::FuseDef* OldFuse::GetFuseDefFromName(const string FuseName)
{
    if (!m_Parsed)
    {
        if (OK != ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet))
        {
            return nullptr;
        }
    }
    string FuseNameUpperCase = Utility::ToUpperCase(FuseName);
    if (m_pFuseDefMap->find(FuseNameUpperCase) ==
        m_pFuseDefMap->end())
    {
        Printf(Tee::PriNormal, "%s is unknown fuse\n", FuseNameUpperCase.c_str());
        return NULL;
    }
    return &m_pFuseDefMap->find(FuseNameUpperCase)->second;
}
//-----------------------------------------------------------------------------
const FuseUtil::SkuConfig* OldFuse::GetSkuSpecFromName(const string SkuName)
{
    if (!m_Parsed)
        ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet);

    string SkuNameUpperCase = Utility::ToUpperCase(SkuName);
    if (m_pSkuList->find(SkuNameUpperCase) == m_pSkuList->end())
    {
        return NULL;
    }
    return &m_pSkuList->find(SkuNameUpperCase)->second;
}
//-----------------------------------------------------------------------------
OldFuse::OverrideInfo* OldFuse::GetOverrideFromName(const string FuseName)
{
    if (!m_Parsed)
        ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet);

    string FuseNameUpperCase = Utility::ToUpperCase(FuseName);
    if (m_Overrides.find(FuseNameUpperCase) == m_Overrides.end())
    {
        return NULL;
    }
    return &m_Overrides[FuseNameUpperCase];
}
//-----------------------------------------------------------------------------
RC OldFuse::HandleColumnBasedFuses(string ChipSku, vector<UINT32> *pRegsToBlow)
{
    RC rc;
    if (!m_Parsed)
        CHECK_RC(ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet));

    if (m_pSkuList->find(ChipSku) == m_pSkuList->end())
    {
        // unknown chip sku - attempt set look for 'overrides'
        map<string, OverrideInfo>::const_iterator OverrideIter = m_Overrides.begin();
        for (; OverrideIter != m_Overrides.end(); OverrideIter++)
        {
            string FuseName = OverrideIter->first;
            if ((m_pFuseDefMap->find(FuseName) == m_pFuseDefMap->end()) ||
                (m_pFuseDefMap->find(FuseName)->second.Type == "fuseless"))
                continue;

            CHECK_RC(UpdateColumnBasedFuse(m_pFuseDefMap->find(FuseName)->second,
                                           m_Overrides[FuseName].Value,
                                           pRegsToBlow));
        }
    }
    else
    {
        // Normal case for blowing ColumnBased fuses:
        // Go through the list of fuses for the desired chip sku
        list<FuseUtil::FuseInfo>::const_iterator FusesIter;
        FusesIter = m_pSkuList->find(ChipSku)->second.FuseList.begin();
        for (; FusesIter != m_pSkuList->find(ChipSku)->second.FuseList.end(); FusesIter++)
        {
            if ((FusesIter->Attribute & FuseUtil::FLAG_FUSELESS) ||
                (FusesIter->Attribute & FuseUtil::FLAG_READONLY) ||
                (FusesIter->Attribute & FuseUtil::FLAG_PSEUDO))
                continue;

            const FuseUtil::FuseDef& def = *(FusesIter->pFuseDef);
            UINT32 ValToBurn = 0;
            if (m_Overrides.find(def.Name) == m_Overrides.end())
            {
                if (FusesIter->Attribute & FuseUtil::FLAG_DONTCARE)
                    continue;
                // following SKU spec - so don't blow read only fuses
                CHECK_RC(GetFuseValToBurn((*FusesIter), NULL, &ValToBurn));
            }
            else if (m_Overrides[def.Name].Flag & USE_SKU_FLOORSWEEP_DEF)
            {
                CHECK_RC(GetFuseValToBurn((*FusesIter),
                                          &m_Overrides[def.Name],
                                          &ValToBurn));
            }
            else
            {
                // FULL override
                ValToBurn = m_Overrides[def.Name].Value;
            }
            CHECK_RC(UpdateColumnBasedFuse(def, ValToBurn, pRegsToBlow));
        }
    }
    return rc;
}
//-----------------------------------------------------------------------------
// see https://wiki.lwpu.com/gpunotebook/index.php/MODS/Fuse_XML
RC OldFuse::GetFuseValToBurn(const FuseUtil::FuseInfo &Info,
                          OverrideInfo *pOverride,
                          UINT32 *pValToBlow)
{
    RC                 rc;
    UINT32             ValToBlow = 0;
    const FuseUtil::FuseDef *pDef     = Info.pFuseDef;
    UINT32      LwrrFuseRawVal = ExtractFuseVal(pDef, ALL_MATCH);

    MASSERT(pDef->Type != FuseUtil::TYPE_PSEUDO);

    // Get what the supposed Fuse value for the given Chip SKU
    // Do a little bit of sanity check...
    if (Info.Attribute & FuseUtil::FLAG_MATCHVAL)
    {
        if (pOverride != nullptr &&
            (pOverride->Flag & USE_SKU_FLOORSWEEP_DEF))
        {
            // Fuseless fuses can be overridden to anything, so the
            // starting point can be the override value. Traditional
            // fuses can't be unblown, so at most, we can OR the raw
            // value with the override's value.
            if (Info.Attribute & FuseUtil::FLAG_FUSELESS)
            {
                LwrrFuseRawVal = pOverride->Value;
            }
            else
            {
                LwrrFuseRawVal |= pOverride->Value;
            }
        }
        bool LwrrValIsValid = false;
        // rule (as described in bug 732046) - if the current fuse val already
        // matches one of the allowed values in ValMatchHex, then do nothing
        for (UINT32 i = 0; i < Info.Vals.size(); i++)
        {
            if (LwrrFuseRawVal == Info.Vals[i])
            {
                LwrrValIsValid = true;
                ValToBlow      = LwrrFuseRawVal;
                break;
            }
        }

        if (!LwrrValIsValid)
        {
            bool Possible = false;
            for (UINT32 i = 0; i < Info.Vals.size(); i++)
            {
                // Need to check whether we are going to have a fuse value bit
                // changing from 1 -> 0 which is NOT possible
                UINT32 OneToZeroMask = LwrrFuseRawVal & (~Info.Vals[i]);
                // Note fuseless fuse is allowed to be overwritten
                if ((OneToZeroMask != 0) &&
                    !(Info.Attribute & FuseUtil::FLAG_FUSELESS))
                {
                    Printf(Tee::PriNormal,
                           "tried to blow a fuse where the desired fuse"
                           "value has 1->0 transition: 0x%x to 0x%x=> try more\n",
                           LwrrFuseRawVal, Info.Vals[i]);
                }
                else
                {
                    ValToBlow = Info.Vals[i];
                    Possible  = true;
                    break;
                }
            }
            if (!Possible)
            {
                // RIR should be a last resort, and should only be used when requested
                if (m_UseRir.count(pDef->Name) != 0 && !Info.Vals.empty() &&
                    !(Info.Attribute & FuseUtil::FLAG_FUSELESS))
                {
                    m_NeedRir.insert(pDef->Name);
                    ValToBlow = Info.Vals[0];
                }
                else
                {
                    Printf(Tee::PriNormal, "cannot blow fuse %s\n", pDef->Name.c_str());
                    return RC::FUSE_VALUE_OUT_OF_RANGE;
                }
            }
        }

        // Disable any RIR records if required
        if (m_DisableRir.count(pDef->Name))
        {
            UINT32 ZeroToOneMask = (~LwrrFuseRawVal) & ValToBlow;
            if (ZeroToOneMask != 0)
            {
                m_PossibleUndoRir.insert(pDef->Name);    
            }
        }
    }
    else if (Info.Attribute & FuseUtil::FLAG_BITCOUNT)
    {
        if ((Info.Attribute & FuseUtil::FLAG_FUSELESS) &&
            (((UINT32)Utility::CountBits(LwrrFuseRawVal)) > Info.Value))
        {
            // scenario where SLT want to 'unfloorsweep' fuselessfuse
            // Start over with a new mask
            LwrrFuseRawVal = 0;
        }

        // This is for production SLT:
        // after screening with initial diag, the script will tell us the
        // parts of the chip that should be floorswept. So mark them here.
        // This is not a FULL override: if floorsweept says this only has one
        // defective partition, but the SLT program needs to disable 2 for
        // the SKU it is creating, we still need to arbitrarily blow 1 partition
        if (pOverride &&
            (pOverride->Flag & USE_SKU_FLOORSWEEP_DEF))
        {
            LwrrFuseRawVal |= pOverride->Value;
        }

        UINT32 BitCount = Utility::CountBits(LwrrFuseRawVal);
        if (BitCount > Info.Value)
        {
            Printf(Tee::PriNormal, "Cannot blow %s to %d bits when current is %d\n",
                   pDef->Name.c_str(), Info.Value, BitCount);
            return RC::CANNOT_MEET_FS_REQUIREMENTS;
        }

        // Suppose we're blowing TPC disable, and SKU spec says we need to
        // disable 4 TPCs.
        // Suppose we start with a blank chip with no fuses burnt.
        // The preferred list is 3,5
        // The first loop will first disable TPC 3 and 5
        // Now, since we need to disable a total of 4 TPCs:
        // We then start arbitrarily (SLT prefers LSB first) disable more TPCs
        // until we have a total of 4 TPCs marked   - second loop

        // blow the preferred bits first
        for (UINT32 i = 0;
              (i < Info.Vals.size()) && (BitCount < Info.Value); i++)
        {
            LwrrFuseRawVal |= 1 << Info.Vals[i];
            BitCount   = (UINT32)Utility::CountBits(LwrrFuseRawVal);
        }

        // Add bits to the raw value until total bit count equals the
        // total number of bits we want to blow for this fuse.
        for (UINT32 BitIdx = 0;
              (UINT32)Utility::CountBits(LwrrFuseRawVal) < Info.Value; BitIdx++)
        {
            LwrrFuseRawVal |= 1<<BitIdx;
        }

        ValToBlow = LwrrFuseRawVal;
    }
    else if (Info.Attribute & FuseUtil::FLAG_UNDOFUSE)
    {
        if ((Info.StrValue == "00") || (Info.StrValue == "0"))
        {
            ValToBlow = 0;
            if (LwrrFuseRawVal == 1)
            {
                if (m_UseUndoFuse)
                {
                    // Value = 1 (ops blew a fuse they weren't supposed to blow)
                    // so 'Undo' it (BIT 1 = undo bit)
                    ValToBlow = 2;
                }
                else
                {
                    Printf(Tee::PriNormal,
                           "%s - en/dis fuse blown, and Undo is not allowed\n",
                           pDef->Name.c_str());
                    return RC::CANNOT_MEET_FS_REQUIREMENTS;
                }
            }
            else if (LwrrFuseRawVal  > 3)
            {
                Printf(Tee::PriHigh, "%s - fuse read failure (value = %d).\n",
                       pDef->Name.c_str(), LwrrFuseRawVal);
                return RC::SOFTWARE_ERROR;
            }
        }
        else if ((Info.StrValue == "01") || (Info.StrValue == "1"))
        {
            ValToBlow = 1;
            if (LwrrFuseRawVal > 1)
            {
                Printf(Tee::PriHigh, "Fuse %s is supposed to be 1, but it is not\n",
                    pDef->Name.c_str());
                return RC::SOFTWARE_ERROR;
            }
        }
        else if ((Info.StrValue == "1x") || (Info.StrValue == "10"))
        {
            ValToBlow = 2;
        }
	else
        {
            Printf(Tee::PriNormal, "unknown undo fuse type\n");
            return RC::SOFTWARE_ERROR;
        }
    }
    else if (Info.Attribute & (FuseUtil::FLAG_READONLY|FuseUtil::FLAG_DONTCARE))
    {
        return OK;
    }
    else
    {
        Printf(Tee::PriNormal, "Should never reach here... unknown fuse type\n");
        return RC::SOFTWARE_ERROR;
    }
    *pValToBlow = ValToBlow;
    return rc;
}
//-----------------------------------------------------------------------------
// This is similar to UpdateRecords
RC OldFuse::UpdateColumnBasedFuse(const FuseUtil::FuseDef& fuseDef,
                               UINT32 ValToBlow,
                               vector<UINT32> *pRegsToBlow)
{
    RC rc;

    if (m_NeedRir.count(fuseDef.Name) != 0)
    {
        CHECK_RC(AddRIRRecords(fuseDef, ValToBlow));
    }

    if (m_PossibleUndoRir.count(fuseDef.Name) != 0)
    {
        CHECK_RC(DisableRIRRecords(fuseDef, ValToBlow));
    }

    Printf(m_PrintPri, "%28s: ", fuseDef.Name.c_str());
    Printf(m_PrintPri, "0x%x| Bit ", ValToBlow);
    CHECK_RC(SetPartialFuseVal(fuseDef.FuseNum, ValToBlow, pRegsToBlow));
    Printf(m_PrintPri, "| ");
    CHECK_RC(SetPartialFuseVal(fuseDef.RedundantFuse, ValToBlow, pRegsToBlow));
    Printf(m_PrintPri, "|\n");
    return rc;
}

// Example:
// Pseudo Fuses
//   (ip_disable[0])   "pseudo_a9_ape_disable"          : 1
//   (ip_disable[4:3]) "pseudo_ip_disable_reserved_4_3" : 3
// =>
// Master Fuse
//    ip_disable: 0x19
//
RC OldFuse::FuseCompose(map<string, pair<UINT32, UINT32>> *pPseudoFuseMap,
                        map<string, UINT32> *pPseudoFuseValMap,
                        UINT32 *MasterFuseVal)
{
    RC rc = OK;

    map<string, UINT32>::iterator FuseValMapIter;
    FuseValMapIter = pPseudoFuseValMap->begin();
    for ( ; FuseValMapIter != pPseudoFuseValMap->end(); FuseValMapIter++)
    {
        string Name = Utility::ToUpperCase(FuseValMapIter->first);
        UINT32 Value = FuseValMapIter->second;

        // ignore pseudo fuse that's not mapped to current master fuse
        if (pPseudoFuseMap->find(Name) == pPseudoFuseMap->end())
        {
            continue;
        }

        UINT32 HighBit = (*pPseudoFuseMap)[Name].first;
        UINT32 LowBit = (*pPseudoFuseMap)[Name].second;
        UINT32 Mask = ((1ULL << (HighBit - LowBit + 1)) - 1) << LowBit;
        *MasterFuseVal &= ~Mask;
        *MasterFuseVal |= Value << LowBit;
    }

    return rc;
}

// Decompose all pseudo fuses from a master fuse
//
// Example:
// Master Fuse
//    ip_disable: 0x55
// =>
// Pseudo Fuses
//   (ip_disable[0])   "pseudo_a9_ape_disable"                  : 1
//   (ip_disable[1])   "pseudo_ape_disable"                     : 0
//   (ip_disable[2])   "pseudo_flop_clear_ccplex_ist_disable"   : 1
//   (ip_disable[4:3]) "pseudo_ip_disable_reserved_4_3"         : 2
//   (ip_disable[5])   "pseudo_ist_disable"                     : 0
//   (ip_disable[6])   "pseudo_runtime_ccplex_ist_disable"      : 1
//   (ip_disable[7])   "pseudo_ip_disable_reserved_7"           : 0
RC OldFuse::FuseDecompose(map<string, pair<UINT32, UINT32>> *pPseudoFuseMap,
                          UINT32 MasterFuseVal,
                          map<string, UINT32> *pPseudoFuseValMap)
{
    RC rc = OK;

    map<string, pair<UINT32, UINT32>>::iterator FuseMapIter;
    FuseMapIter = pPseudoFuseMap->begin();
    for (; FuseMapIter != pPseudoFuseMap->end(); FuseMapIter++)
    {
        string Name = FuseMapIter->first;
        UINT32 HighBit = FuseMapIter->second.first;
        UINT32 LowBit = FuseMapIter->second.second;
        UINT32 Mask = (1ULL << (HighBit - LowBit + 1)) - 1;
        (*pPseudoFuseValMap)[Name] = (MasterFuseVal >> LowBit) & Mask;
    }

    return rc;
}

// Decompose a specific pseudo fuse from a master fuse
RC OldFuse::FuseDecompose(map<string, pair<UINT32, UINT32>> *pPseudoFuseMap,
                          UINT32 MasterFuseVal,
                          string PseudoFuseName,
                          UINT32 *PseudoFuseVal)
{
    RC rc = OK;

    string Name = Utility::ToUpperCase(PseudoFuseName);
    if (pPseudoFuseMap->find(Name) == pPseudoFuseMap->end())
    {
        Printf(Tee::PriError, "Pseudo fuse %s not mapped to master fuse\n",
                Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    UINT32 HighBit = (*pPseudoFuseMap)[Name].first;
    UINT32 LowBit = (*pPseudoFuseMap)[Name].second;
    UINT32 Mask = (1ULL << (HighBit - LowBit + 1)) - 1;
    *PseudoFuseVal = (MasterFuseVal >> LowBit) & Mask;

    return rc;
}

//-----------------------------------------------------------------------------
void OldFuse::SetOverrides(const map<string, OverrideInfo> &Overrides)
{
    m_Overrides = Overrides;
}
//-----------------------------------------------------------------------------
RC OldFuse::GetMiscInfo(const FuseUtil::MiscInfo **pMiscInfo)
{
    RC rc;
    if (!m_Parsed)
        CHECK_RC(ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet));

    *pMiscInfo = m_pMiscInfo;
    return rc;
}
//-----------------------------------------------------------------------------
void OldFuse::PrintRecords()
{
}
//-----------------------------------------------------------------------------
RC OldFuse::RegenerateRecords(string ChipSku)
{
    RC rc;
    if (!m_Parsed)
        CHECK_RC(ParseXml(&m_pFuseDefMap, &m_pSkuList, &m_pMiscInfo, &m_pFuseDataSet));

    PrintRecords();
    if (m_pSkuList->find(ChipSku) == m_pSkuList->end())
    {
        if (!ChipSku.empty())
        {
            Printf(m_PrintPri, "Chip sku: %s not in XML\n", ChipSku.c_str());
        }

        // do only the overrides - this is for fuse sim or early days of bringup
        map<string, OverrideInfo>::const_iterator OverrideIter = m_Overrides.begin();
        for (; OverrideIter != m_Overrides.end(); OverrideIter++)
        {
            string FuseName = OverrideIter->first;
            if (m_pFuseDefMap->find(FuseName) == m_pFuseDefMap->end())
                continue;

            CHECK_RC(UpdateRecord(m_pFuseDefMap->find(FuseName)->second,
                                  m_Overrides.find(FuseName)->second.Value));
        }
    }
    else
    {
        // normal fuse blowing: go through each fuse requirement of the chipsku
        list<FuseUtil::FuseInfo>::const_iterator FusesIter;
        FusesIter = m_pSkuList->find(ChipSku)->second.FuseList.begin();
        for (; FusesIter != m_pSkuList->find(ChipSku)->second.FuseList.end(); FusesIter++)
        {
            const FuseUtil::FuseDef *pDef = FusesIter->pFuseDef;
            if (!(FusesIter->Attribute & FuseUtil::FLAG_FUSELESS))
                continue;

            UINT32 ValToBurn = 0;
            if (m_Overrides.find(pDef->Name) == m_Overrides.end())
            {
                if ((FusesIter->Attribute & (FuseUtil::FLAG_READONLY|FuseUtil::FLAG_DONTCARE)))
                    continue;

                CHECK_RC(GetFuseValToBurn((*FusesIter), NULL, &ValToBurn));
            }
            else if (m_Overrides[pDef->Name].Flag & USE_SKU_FLOORSWEEP_DEF)
            {
                if ((FusesIter->Attribute & (FuseUtil::FLAG_READONLY|FuseUtil::FLAG_DONTCARE)))
                    continue;

                CHECK_RC(GetFuseValToBurn((*FusesIter),
                                          &m_Overrides[pDef->Name],
                                          &ValToBurn));
            }
            else
            {
                // full overrides
                ValToBurn = m_Overrides[pDef->Name].Value;
            }
            CHECK_RC(UpdateRecord(*pDef, ValToBurn));
        }
    }
    return rc;
}
//-----------------------------------------------------------------------------
UINT32 OldFuse::GetFuseValGivenName(const string FuseName, Tolerance Strictness)
{
    const FuseUtil::FuseDef *pDef = GetFuseDefFromName(FuseName);
    if (pDef)
    {
        if (pDef->Type == FuseUtil::TYPE_PSEUDO)
        {
            MASSERT(!"GetFuseValGivenName cannot be used with pseudo fuses");
            return 0;
        }
        CacheFuseReg();
        return ExtractFuseVal(pDef, Strictness);
    }
    MASSERT(!"GetFuseValGivenName cannot find definition\n");
    return 0;
}
//-----------------------------------------------------------------------------
UINT32 OldFuse::GetOptFuseValGivenName(const string FuseName)
{
    const FuseUtil::FuseDef *pDef = GetFuseDefFromName(FuseName);
    if (pDef)
    {
        return ExtractOptFuseVal(pDef);
    }
    MASSERT(!"GetOptFuseValGivenName cannot find definition\n");
    return 0;
}
//-----------------------------------------------------------------------------
RC OldFuse::GetSimulatedFuses(vector<UINT32> *pFuseRegs)
{
    MASSERT(pFuseRegs);
    *pFuseRegs = m_SimFuseRegs;
    return OK;
}
//-----------------------------------------------------------------------------
void OldFuse::ClearSimulatedFuses()
{
    m_SimFuseRegs.clear();
}
//-----------------------------------------------------------------------------
void OldFuse::SetSimulatedFuses(const vector<UINT32> &FuseRegs)
{
    m_SimFuseRegs = FuseRegs;
    Printf(Tee::PriLow, "Simulated fuses:\n");
    for (UINT32 i = 0; i < m_SimFuseRegs.size(); i++)
    {
        Printf(Tee::PriLow, "    Row %3d : 0x%08x\n", i, m_SimFuseRegs[i]);
    }
}
//-----------------------------------------------------------------------------
JS_CLASS(Fuse);

static SObject Fuse_Object
(
    "Fuse",
    FuseClass,
    0,
    0,
    "Fuse class"
);
PROP_CONST(Fuse, ALL_MATCH, OldFuse::ALL_MATCH);
PROP_CONST(Fuse, REDUNDANT_MATCH, OldFuse::REDUNDANT_MATCH);
PROP_CONST(Fuse, LWSTOMER_MATCH, OldFuse::LWSTOMER_MATCH);
PROP_CONST(Fuse, BAD_FUSES, OldFuse::BAD_FUSES);
PROP_CONST(Fuse, ALL_FUSES, OldFuse::ALL_FUSES);
PROP_CONST(Fuse, GOOD_FUSES, OldFuse::GOOD_FUSES);

P_(Fuse_Set_CheckInternalFuses);
static SProperty Fuse_CheckInternalFuses
(
  Fuse_Object,
  "CheckInternalFuses",
  0,
  0,
  0,
  Fuse_Set_CheckInternalFuses,
  MODS_INTERNAL_ONLY,
  "Enable checking of internal fuses"
);

P_(Fuse_Set_CheckInternalFuses)
{
  bool bCheck;
  JavaScriptPtr pJs;
  RC rc = pJs->FromJsval(*pValue, &bCheck);
  if (OK != rc)
  {
      pJs->Throw(pContext, rc, "Error colwerting input in Fuse.CheckInternalFuses");
      return JS_FALSE;
  }

  if ((Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK) && bCheck)
  {
      pJs->Throw(pContext, rc, "Unable to set Fuse.CheckInternalFuses");
      return JS_FALSE;
  }

  OldFuse::SetCheckInternalFuses(bCheck);
  return JS_TRUE;
}
