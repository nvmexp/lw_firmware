/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2015-2021 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/fusecache.h"
#include "core/include/fuseparser.h"
#include "core/include/fusexml_parser.h"
#include "core/include/fusejsonparser.h"
#include "core/include/jscript.h"
#include "core/include/utility.h"

using namespace FuseUtil;

bool FuseParser::m_Verbose = false;

FuseParser::FuseParser() : m_FuseCache(FuseCache::GetInstance()) {}

// Factory method to create a parser based on the filename
FuseParser* FuseParser::CreateFuseParser(const string& filename)
{
    // Get the file extension
    if (filename.substr(filename.find_last_of(".") + 1) == "json")
    {
        return new FuseJsonParser();
    }

    return new FuseXmlParser();
}

RC FuseParser::ParseFile(const string       &filename,
                         const FuseDefMap  **ppFuseDefMap,
                         const SkuList     **ppSkuList,
                         const MiscInfo    **ppMiscInfo,
                         const FuseDataSet **ppFuseDataSet)
{
    RC rc;

    if (!m_FuseCache.HasEntry(filename))
    {
        FuseDefMap* pFuseDefMap = new FuseDefMap();
        SkuList* pSkuList = new SkuList();
        MiscInfo* pMiscInfo = new MiscInfo();
        FuseDataSet* pFuseDataSet = new FuseDataSet();

        rc = ParseFileImpl(filename, pFuseDefMap, pSkuList, pMiscInfo, pFuseDataSet);

        if (rc == OK)
        {
            // Transfer the data ownership to the FuseCache
            // FuseCache's static destructor will clean these up
            rc = m_FuseCache.AddEntry(filename,
                                      pFuseDefMap,
                                      pSkuList,
                                      pMiscInfo,
                                      pFuseDataSet);
        }

        if (rc != OK)
        {
            delete pFuseDefMap;
            delete pSkuList;
            delete pMiscInfo;
            delete pFuseDataSet;
            return rc;
        }
    }

    // Retrieve the new entry and put it into the return parameters
    CHECK_RC(m_FuseCache.GetEntry(filename,
                                  ppFuseDefMap,
                                  ppSkuList,
                                  ppMiscInfo,
                                  ppFuseDataSet));

    return rc;
}

//-----------------------------------------------------------------------------
// Functions for storing FbConfig
//-----------------------------------------------------------------------------
void FuseDataSet::AddFbConfig(bool Valid,
                                   UINT32 FbpCount,
                                   UINT32 FbpDisable,
                                   UINT32 FbioDisable,
                                   UINT32 FbioShift)
{
    FbConfigInfo Info;
    Info.Valid       = Valid;
    Info.FbpCount    = FbpCount;
    Info.FbpDisable  = FbpDisable;
    Info.FbioDisable = FbioDisable;
    Info.FbioShift   = FbioShift;
    m_FbConfigList.push_back(Info);
}

void FuseDataSet::ResetFbConfig()
{
    m_FbConfigList.clear();
}

// This creates an array of arrays in JS
RC FuseDataSet::ExportFbConfigToJs(JSObject **ppObject) const
{
    RC rc;
    JavaScriptPtr pJs;
    for (UINT32 i = 0; i < m_FbConfigList.size(); i++)
    {
        JsArray JsFbConfig;
        jsval   jsvTemp;
        CHECK_RC(pJs->ToJsval(m_FbConfigList[i].Valid, &jsvTemp));
        JsFbConfig.push_back(jsvTemp);
        CHECK_RC(pJs->ToJsval(m_FbConfigList[i].FbpCount, &jsvTemp));
        JsFbConfig.push_back(jsvTemp);
        CHECK_RC(pJs->ToJsval(m_FbConfigList[i].FbpDisable, &jsvTemp));
        JsFbConfig.push_back(jsvTemp);
        CHECK_RC(pJs->ToJsval(m_FbConfigList[i].FbioDisable, &jsvTemp));
        JsFbConfig.push_back(jsvTemp);
        CHECK_RC(pJs->ToJsval(m_FbConfigList[i].FbioShift, &jsvTemp));
        JsFbConfig.push_back(jsvTemp);
        // copy the JS array to the JsObject
        CHECK_RC(pJs->SetElement(*ppObject, i, &JsFbConfig));
    }
    return OK;
}

void FuseParser::ParseOffset(const string& offsetVal, OffsetInfo& offsetInfo)
{
    if (Utility::ToUpperCase(offsetVal) == "NA")
    {
        offsetInfo.Offset = UNKNOWN_OFFSET;
        offsetInfo.Msb = 0;
        offsetInfo.Lsb = 0;
    }
    else if (offsetVal.find("|") == string::npos)
    {
        offsetInfo.Offset = strtoul(offsetVal.c_str(), nullptr, 10);
        offsetInfo.Msb = 31;
        offsetInfo.Lsb = 0;
    }
    else
    {
        // offset|msb:lsb format
        size_t splitPos = offsetVal.find("|");
        // parse offset part
        string partStr = offsetVal.substr(0, splitPos);
        offsetInfo.Offset = strtol(partStr.c_str(),NULL,10);
        // parse msb part
        size_t bitBreak = offsetVal.find(":");
        MASSERT(bitBreak != string::npos);
        partStr = offsetVal.substr(splitPos+1, (bitBreak-splitPos));
        offsetInfo.Msb = strtol(partStr.c_str(),NULL,10);
        // parse lsb part
        partStr = offsetVal.substr(bitBreak+1);
        offsetInfo.Lsb = strtol(partStr.c_str(),NULL,10);
    }
}

//-----------------------------------------------------------------------------
RC FuseParser::SetVerbose(bool Enable)
{
    m_Verbose = Enable;

    return OK;
}

//-----------------------------------------------------------------------------
// Brief: The fuse values in the SKU definition appear as  "ValMatchBin:1",
// "ValMatchHex:0x500", "BurnCount:8", etc. We need to interpret this to flags
//  and values
// See https://wiki.lwpu.com/gpunotebook/index.php/Fuse.xml_Flow
// https://wiki.lwpu.com/gpunotebook/index.php/MODS/Fuse_XML
RC FuseParser::InterpretSkuFuses(FuseUtil::SkuList *pSkuList)
{
    RC rc;
    FuseUtil::SkuList::iterator SkuConfIter = pSkuList->begin();
    for (; SkuConfIter != pSkuList->end(); SkuConfIter++)
    {
        Printf(Tee::PriDebug, "---SKU Name = %s---\n",
               SkuConfIter->second.SkuName.c_str());
        list<FuseInfo>::iterator FuseValIter = SkuConfIter->second.FuseList.begin();
        for (; FuseValIter != SkuConfIter->second.FuseList.end(); FuseValIter++)
        {

            if(!strcmp(FuseValIter->StrValue.c_str(), "Bootrom_patches"))
            {
                Printf(Tee::PriDebug, "SKipping bootrom pactches fuse for SKU %s\n", SkuConfIter->second.SkuName.c_str());
                continue;
            }

            if(!strcmp(FuseValIter->StrValue.c_str(), "Pscrom_patches"))
            {
                Printf(Tee::PriDebug, "SKipping Pscrom pactches fuse for SKU %s\n", SkuConfIter->second.SkuName.c_str());
                continue;
            }

            if (GetVerbose())
            {
                Printf(Tee::PriNormal, "Fuse = %16s: %s\n",
                      FuseValIter->pFuseDef->Name.c_str(),
                      FuseValIter->StrValue.c_str());
            }

            // XML string is in FuseValIter->StrValue. We're going to parse it
            // and populate the attribute and fuse value here:
            CHECK_RC(ParseAttributeAndValue(FuseValIter->StrValue,
                                            &(*FuseValIter)));

            // Mark fuseless
            if (FuseValIter->pFuseDef->Type == "fuseless")
            {
                FuseValIter->Attribute |= FLAG_FUSELESS;
            }
            if (FuseValIter->pFuseDef->Type == FuseUtil::TYPE_PSEUDO)
            {
                FuseValIter->Attribute |= FLAG_PSEUDO;
            }
            // that nees to be worked out.
            CHECK_RC(CheckXmlSanity(*FuseValIter));
        }
    }
    return OK;
}

//-----------------------------------------------------------------------------
// The new way to specify IFF records is through IFF_Patches; however, if a
// rework is performed, the IFF records might be valid according to the
// patches, but invalid according to the traditional fuses. Since we have to
// keep both versions until everyone switches, this removes traditional IFF
// records from skus that have IFF Patches
void FuseParser::RemoveTraditionalIff(FuseUtil::SkuList *pSkuList)
{
    FuseUtil::SkuList::iterator skuIt;
    for (skuIt = pSkuList->begin(); skuIt != pSkuList->end(); skuIt++)
    {
        // Case 1: No IFFs defined
        // Case 2: Traditional IFFs defined, no IFF Patches defined
        // Either way, we don't want to modify this SkuConfig
        if (skuIt->second.iffPatches.empty())
        {
            continue;
        }
        list<FuseUtil::FuseInfo>::iterator fuseIt = skuIt->second.FuseList.begin();
        while (fuseIt != skuIt->second.FuseList.end())
        {
            // No other fuse names start with "iff"
            if (Utility::ToLowerCase(fuseIt->pFuseDef->Name).find("iff") == 0)
            {
                fuseIt = skuIt->second.FuseList.erase(fuseIt);
            }
            else
            {
                fuseIt++;
            }
        }
    }
}

RC FuseParser::CheckXmlSanity(const FuseInfo &FuseInfo)
{
    const FuseDef* pDef = FuseInfo.pFuseDef;

    if (FuseInfo.Attribute & (FLAG_FUSELESS|FLAG_PSEUDO))
    {
        return OK;
    }

    UINT32 ExpectedNumBits = pDef->NumBits;
    // *sigh* yet another exception in XML format....
    if (pDef->Type == "en/dis")
    {
        ExpectedNumBits++;
    }

    // Check Total number of fuse bits should match
    UINT32 TotalBits = 0;
    list<FuseLoc>::const_iterator LocIter = pDef->FuseNum.begin();
    for (; LocIter != pDef->FuseNum.end(); LocIter++)
    {
        if (LocIter->Lsb > LocIter->Msb)
        {
            Printf(Tee::PriNormal, "LSB > MSB at fuse %s\n", pDef->Name.c_str());
            return RC::SOFTWARE_ERROR;
        }
        if ((LocIter->Msb - LocIter->Lsb) > 31)
        {
            Printf(Tee::PriNormal, "fuse size > 32 bits at fuse %s\n", pDef->Name.c_str());
        }
        TotalBits += LocIter->Msb - LocIter->Lsb + 1;
    }

    // Make sure the value specified in the Mkt_options section can fit
    // into the number of bits specified by Fuse_encoding section
    if (FuseInfo.Attribute & FLAG_BITCOUNT) // BurnCount
    {
        if (FuseInfo.Value > TotalBits)
        {
            Printf(Tee::PriError, "Value too large for fuse %s\n", pDef->Name.c_str());
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }
    else if (!FuseInfo.ValRanges.empty()) // ATE_RangeHex
    {
        for (UINT32 ii = 0; ii < FuseInfo.ValRanges.size(); ++ii)
        {
            if (FuseInfo.ValRanges[ii].Max >= static_cast<UINT64>(1ULL << TotalBits))
            {
                Printf(Tee::PriError, "Value too large for fuse %s\n", pDef->Name.c_str());
                return RC::FUSE_VALUE_OUT_OF_RANGE;
            }
        }
    }
    else // ValMatchHex or ValMatchBin
    {
        if (FuseInfo.Value >= static_cast<UINT64>(1ULL << TotalBits))
        {
            Printf(Tee::PriError, "Value too large for fuse %s\n", pDef->Name.c_str());
            return RC::FUSE_VALUE_OUT_OF_RANGE;
        }
    }

    UINT32 RedTotalBits = 0;
    LocIter = pDef->RedundantFuse.begin();
    for (; LocIter != pDef->RedundantFuse.end(); LocIter++)
    {
        if (LocIter->Lsb > LocIter->Msb)
        {
            Printf(Tee::PriNormal, "LSB > MSB at fuse %s\n",
                   pDef->Name.c_str());
            return RC::SOFTWARE_ERROR;
        }
        if ((LocIter->Msb - LocIter->Lsb) > 31)
        {
            Printf(Tee::PriNormal, "fuse size > 32 bits at fuse %s\n",
                   pDef->Name.c_str());
        }
        RedTotalBits += LocIter->Msb - LocIter->Lsb + 1;
    }

    if ((TotalBits != ExpectedNumBits) ||
        ((pDef->RedundantFuse.size() > 0) && RedTotalBits != ExpectedNumBits))
    {
        Printf(Tee::PriNormal,
               "Bit size mismatch at fuse %s. Total=%d|Expected=%d|Redundant=%d\n",
               pDef->Name.c_str(),
               TotalBits,
               ExpectedNumBits,
               RedTotalBits);
        return RC::SOFTWARE_ERROR;
    }

    // verify the offsets
    OffsetInfo FuseOffset = pDef->PrimaryOffset;
    if ((FuseOffset.Offset != UNKNOWN_OFFSET) &&
        ((FuseOffset.Msb - FuseOffset.Lsb) > 31))
    {
        Printf(Tee::PriNormal, "OPT fuse > 32 bits at fuse %s\n",
               pDef->Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    // Check if the Max >= Min for ATE_RANGE
    if (FuseInfo.Attribute & FLAG_ATE_RANGE)
    {
        for (UINT32 i = 0; i < FuseInfo.ValRanges.size(); i++)
        {
            if (FuseInfo.ValRanges[i].Max < FuseInfo.ValRanges[i].Min)
            {
                Printf(Tee::PriNormal, "Range value wrong at fuse %s\n",
                       pDef->Name.c_str());
                Printf(Tee::PriNormal,
                       "Attribute = 0x%d, Min = 0x%d, Max = 0x%x\n",
                       FuseInfo.Attribute,
                       FuseInfo.ValRanges[i].Min,
                       FuseInfo.ValRanges[i].Max);
                return RC::SOFTWARE_ERROR;
            }
        }
    }

    // for Preferred List, the list size must < bit count
    if ((FuseInfo.Attribute & FLAG_BITCOUNT) &&
        (ExpectedNumBits < FuseInfo.Vals.size()))
    {
        Printf(Tee::PriNormal, "Preferred bit list at fuse %s > fuse size\n",
               pDef->Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    // Todo: Check range bit range

    return OK;
}

