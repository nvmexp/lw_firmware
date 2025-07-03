/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2007-2016,2018-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "core/include/fuseutil.h"
#include "core/include/fuseparser.h"
#include "core/include/jscript.h"
#include "core/include/mle_protobuf.h"
#include "core/include/rc.h"
#include "core/include/script.h"
#include "core/include/utility.h"
#include <memory>

bool FuseUtil::FuseLoc::operator==(const FuseLoc& rhs) const
{
    return Number      == rhs.Number &&
           Lsb         == rhs.Lsb &&
           Msb         == rhs.Msb &&
           ChainId     == rhs.ChainId &&
           ChainOffset == rhs.ChainOffset &&
           DataShift   == rhs.DataShift;
}

bool FuseUtil::FuseLoc::operator!=(const FuseLoc& rhs) const
{
    return !operator==(rhs);
}

bool FuseUtil::OffsetInfo::operator==(const OffsetInfo& rhs) const
{
    // If the offset is unknown, we don't care about Msb/Lsb
    return Offset == rhs.Offset && (Offset == UNKNOWN_OFFSET ||
        (Lsb == rhs.Lsb && Msb == rhs.Msb));
}

bool FuseUtil::OffsetInfo::operator!=(const OffsetInfo& rhs) const
{
    return !operator==(rhs);
}

bool FuseUtil::FuseDef::operator==(const FuseDef& rhs) const
{
    return Name             == rhs.Name &&
           Type             == rhs.Type &&
           Visibility       == rhs.Visibility &&
           NumBits          == rhs.NumBits &&
           PrimaryOffset    == rhs.PrimaryOffset &&
           FuseNum          == rhs.FuseNum &&
           RedundantFuse    == rhs.RedundantFuse;
}

bool FuseUtil::FuseDef::operator!=(const FuseDef& rhs) const
{
    return !operator==(rhs);
}

// search FuseName form a parsed SkuList
RC FuseUtil::SearchFuseInfo(string ChipSku,
                            string FuseName,
                            const map<string, SkuConfig> &SkuList,
                            FuseInfo* pInfo)
{
    FuseName = Utility::ToUpperCase(FuseName);
    if (SkuList.find(ChipSku) == SkuList.end())
    {
        Printf(Tee::PriNormal, "Chip SKU %s not found\n", ChipSku.c_str());
        return RC::BAD_PARAMETER;
    }

    bool FoundFuse = false;
    list<FuseInfo>::const_iterator FusesIter;
    FusesIter = SkuList.find(ChipSku)->second.FuseList.begin();
    for (; FusesIter != SkuList.find(ChipSku)->second.FuseList.end(); FusesIter++)
    {
        if (FuseName == FusesIter->pFuseDef->Name)
        {
            FoundFuse = true;
            *pInfo = *FusesIter;
            break;
        }
    }

    if (!FoundFuse)
    {
        Printf(Tee::PriNormal, "Fuse %s not found\n", FuseName.c_str());
        return RC::BAD_PARAMETER;
    }
    return OK;
}

// search for fuseName from a given SKU configuration
bool FuseUtil::FindFuseInfo
(
    string fuseName,
    const SkuConfig& skuCfg,
    const FuseInfo** ppInfo
)
{
    MASSERT(ppInfo != nullptr);
    fuseName = Utility::ToUpperCase(fuseName);

    for (const FuseUtil::FuseInfo& fuseInfo : skuCfg.FuseList)
    {
        if (fuseName == fuseInfo.pFuseDef->Name)
        {
            *ppInfo = &fuseInfo;
            return true;
        }
    }

    return false;
}

RC FuseUtil::ParseAttributeAndValue(string StrVal, FuseInfo *pFuseInfo)
{
    string Attribute, FuseValInStr;
    string optionsStr;
    vector<string> options;
    vector<string> optiolwals;
    // After parsing XML FuseInfo.StrValue temporarily stores the full
    // fuse value as string. Eg. 'ValMatchBin:1000110'
    // We want to extract 'ValMatchBin' as the fuse 'attribute', and
    // '1000110' as the fuse value
    size_t pos   = StrVal.find(":");
    Attribute    = StrVal.substr(0, pos);
    size_t barPos = StrVal.find("|", pos);
    if (barPos == string::npos)
    {
        FuseValInStr = StrVal.substr(pos + 1);
    }
    else
    {
        FuseValInStr = StrVal.substr(pos + 1, barPos -  pos - 1);
        optionsStr = StrVal.substr(barPos + 1);
        // Split the options string by pipes: "MaxBurnPerGroup:3|ProtectedBits:4,3"
        for (barPos = optionsStr.find("|"); barPos != string::npos; barPos = optionsStr.find("|"))
        {
            options.push_back(optionsStr.substr(0, barPos));
            optionsStr = optionsStr.substr(barPos + 1);
        }
        // Make sure to grab the last option
        options.push_back(optionsStr);

        // Split the options on ":"
        for (string& option : options)
        {
            pos = option.find(":");
            if (pos == string::npos)
            {
                Printf(Tee::PriError, "No option value for %s\n",
                       pFuseInfo->pFuseDef->Name.c_str());
                return RC::SOFTWARE_ERROR;
            }
            optiolwals.push_back(option.substr(pos + 1));
            option = option.substr(0, pos);
        }
    }

    if (Attribute == "ATE")
    {
        pFuseInfo->Attribute = FLAG_READONLY|FLAG_DONTCARE;
        pFuseInfo->Value = 0;
        return OK;
    }
    else if (Attribute == "SkuFuse")
    {
        pFuseInfo->Attribute = FLAG_DONTCARE;
        pFuseInfo->Value = 0;
        return OK;
    }
    else if (Attribute == "Tracking")
    {
        // Ignore this for now, until we add defective/disabled fuses
        // set FLAG_TRACKING
        pFuseInfo->Attribute = FLAG_DONTCARE | FLAG_TRACKING;
        pFuseInfo->Value = 0;
        return OK;
    }


    if (FuseValInStr.size() == 0)
    {
        Printf(Tee::PriNormal, "XML error - no expected fuse value for %s\n",
               pFuseInfo->pFuseDef->Name.c_str());
        return RC::SOFTWARE_ERROR;
    }

    // store FuseInfo.StrValue for the fuse value as string (eg. '10xx00x')
    pFuseInfo->StrValue = FuseValInStr;
    if (Attribute == "ValMatchHex")
    {
        for (UINT32 opt = 0; opt < options.size(); opt++)
        {
            if (options[opt] == "SltTestValue")
            {
                pFuseInfo->hasTestVal = true;
                pFuseInfo->testVal = strtoul(optiolwals[opt].c_str(), nullptr, 0);
            }
        }

        pFuseInfo->Attribute = FLAG_MATCHVAL;
        ParseValHex(pFuseInfo, FuseValInStr);
    }
    else if (Attribute == "BurnCount")
    {
        for (UINT32 opt = 0; opt < options.size(); opt++)
        {
            if (options[opt] == "SltTestCount")
            {
                pFuseInfo->hasTestVal = true;
                pFuseInfo->testVal = strtoul(optiolwals[opt].c_str(), nullptr, 0);
            }
            else if (options[opt] == "MaxBurnPerGroup")
            {
                pFuseInfo->maxBurnPerGroup = strtoul(optiolwals[opt].c_str(), nullptr, 0);
            }
            else if (options[opt] == "ProtectedBits")
            {
                string bits = optiolwals[opt];
                for (pos = bits.find(","); pos != string::npos; pos = bits.find(","))
                {
                    string bit = bits.substr(0, pos);
                    pFuseInfo->protectedBits.push_back(strtoul(bit.c_str(), nullptr, 0));
                    bits = bits.substr(pos + 1);
                }
                pFuseInfo->protectedBits.push_back(strtoul(bits.c_str(), nullptr, 0));
            }
        }
        ParseBurnCount(pFuseInfo, FuseValInStr);
    }
    else if (Attribute == "ValMatchBin")
    {
        for (UINT32 opt = 0; opt < options.size(); opt++)
        {
            if (options[opt] == "SltTestValue")
            {
                pFuseInfo->hasTestVal = true;
                pFuseInfo->testVal = strtoul(optiolwals[opt].c_str(), nullptr, 0);
            }
        }

        pFuseInfo->Attribute = FLAG_UNDOFUSE;
        if ((FuseValInStr == "0")  ||
            (FuseValInStr == "00") ||
            (FuseValInStr == "10"))
        {
            pFuseInfo->Value = 0;
        }
        else if ((FuseValInStr == "01") || (FuseValInStr == "1"))
        {
            pFuseInfo->Value = 1;
        }
        else if (FuseValInStr == "0x")
        {
            pFuseInfo->Attribute |= FLAG_DONTCARE;
        }
        else if (FuseValInStr == "1x")
        {
            pFuseInfo->Value = 2;
        }
	else
        {
            Printf(Tee::PriNormal, "unexpected ValMatchBin val: %s\n",
                   FuseValInStr.c_str());
            return RC::SOFTWARE_ERROR;
        }
    }
    else if (Attribute == "ATE_RangeHex")
    {
        ParseRangeHex(pFuseInfo, FuseValInStr);
    }
    else if (Attribute == "ATE_NotValHex")
    {
        pFuseInfo->Attribute = FLAG_READONLY | FLAG_ATE_NOT;
        ParseValHex(pFuseInfo, FuseValInStr);
    }
    else
    {
        Printf(Tee::PriNormal,
            "Fuse file error: unknown fuse attribute \"%s\"\n", Attribute.c_str());
        return RC::SOFTWARE_ERROR;
    }
    return OK;
}

void FuseUtil::ParseBurnCount(FuseInfo *pFuseInfo, string FuseValInStr)
{
    pFuseInfo->Attribute = FLAG_BITCOUNT;
    // New format: <str>BurnCount:0x5:PreferredList:1,2,3,4</str>
    // this means blow 5 bits: but blow bit 1,2,3,4 first (in order)
    // afterwards, just blow from LSB

    // ":" is a separator for preferred list
    size_t Colon = FuseValInStr.find(":");
    string BurnCount = FuseValInStr.substr(0, Colon);
    pFuseInfo->Value = strtoul(BurnCount.c_str(), NULL, 16);

    // no more to do if there is no preferred list
    if (Colon == string::npos)
        return;

    // find the next ":" for the preferred list
    string PrefListStr = FuseValInStr.substr(Colon + 1);
    Colon = PrefListStr.find(":");
    PrefListStr = PrefListStr.substr(Colon + 1);
    bool Done = false;
    while (!Done)
    {
        string PrefBitStr;
        size_t CommaPos = PrefListStr.find(",");
        if (CommaPos == string::npos)
        {
            Done = true;
            PrefBitStr = PrefListStr;
        }
        else
        {
            // there are multiple preferred numbers
            PrefBitStr  = PrefListStr.substr(0, CommaPos);
            PrefListStr = PrefListStr.substr(CommaPos + 1);
        }
        UINT32 PrefBit = strtoul(PrefBitStr.c_str(), NULL, 10);
        pFuseInfo->Vals.push_back(PrefBit);
    }
}

// Format: <str>ValMatchHex:0x5,0x6
void FuseUtil::ParseValHex(FuseInfo *pFuseInfo, string FuseValInStr)
{
    bool Done = false;
    while (!Done)
    {
        string ValueStr;
        size_t CommaPos = FuseValInStr.find(",");
        if (CommaPos == string::npos)
        {
            Done = true;
            ValueStr = FuseValInStr;
        }
        else
        {
            ValueStr     = FuseValInStr.substr(0, CommaPos);
            FuseValInStr = FuseValInStr.substr(CommaPos + 1);
        }
        UINT32 HexVal = strtoul(ValueStr.c_str(), NULL, 16);
        pFuseInfo->Vals.push_back(HexVal);
    }
    // maybe this is not needed
    MASSERT(pFuseInfo->Vals.size() > 0);
    pFuseInfo->Value = pFuseInfo->Vals[0];
}

void FuseUtil::ParseRangeHex(FuseInfo *pFuseInfo, string FuseValInStr)
{
    // format example '<str>ATE_RangeHex:0x1-0x4,0x6,0x8</str>'
    // so Value = 0x1,  and ValueMax = 0x6
    pFuseInfo->Attribute = FLAG_READONLY|FLAG_ATE_RANGE;

    string RemainStr = FuseValInStr;
    bool Done = false;
    while (!Done)
    {
        string OneRangeStr;
        size_t CommaPos = RemainStr.find(",");
        if (CommaPos == string::npos)
        {
            Done = true;
            OneRangeStr = RemainStr;
        }
        else
        {
            OneRangeStr = RemainStr.substr(0, CommaPos);
            RemainStr   = RemainStr.substr(CommaPos + 1);
        }
        size_t DashPos = OneRangeStr.find("-");
        string Milwal  = OneRangeStr.substr(0, DashPos);
        string MaxVal;
        if (DashPos == string::npos)
            MaxVal = Milwal;
        else
            MaxVal = OneRangeStr.substr(DashPos+1);

        FuseInfo::Range NewRange;
        NewRange.Min = strtoul(Milwal.c_str(), NULL, 16);
        NewRange.Max = strtoul(MaxVal.c_str(), NULL, 16);
        pFuseInfo->ValRanges.push_back(NewRange);
    }
}

RC FuseUtil::GetP4Info(const string& FileName, const MiscInfo **pInfo)
{
    if (!Utility::FindPkgFile(FileName))
    {
        Printf(Tee::PriLow, "Fuse file %s does not exist\n", FileName.c_str());
        return RC::FILE_DOES_NOT_EXIST;
    }

    RC rc;
    const FuseDefMap* pFuseDefMap;
    const SkuList* pSkuList;
    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(FileName));
    CHECK_RC(pParser->ParseFile(FileName, &pFuseDefMap, &pSkuList, pInfo));

    if ((*pInfo)->P4Data.Version == 0 ||
        (*pInfo)->P4Data.Version == 0xBADBEEF)
    {
        Printf(Tee::PriWarn,
               "%s has an invalid version field\n",
               FileName.c_str());
        return RC::BAD_PARAMETER;
    }

    if ((*pInfo)->P4Data.P4Revision == 0xBADBEEF)
    {
        Printf(Tee::PriWarn,
               "%s has an invalid P4 revision field\n",
               FileName.c_str());
        return RC::BAD_PARAMETER;
    }

    return rc;
}

RC FuseUtil::PrintFuseSpec(const string &FileName)
{
    RC rc;
    const FuseDefMap *pFuseDefMap;
    const SkuList    *pSkuList;
    const MiscInfo   *pInfo;

    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(FileName));
    CHECK_RC(pParser->ParseFile(FileName, &pFuseDefMap, &pSkuList, &pInfo));

    FuseDefMap::const_iterator FuseDefIter = pFuseDefMap->begin();
    for (; FuseDefIter != pFuseDefMap->end(); FuseDefIter++)
    {
        Printf(Tee::PriNormal, "%s | Offset: 0x%8x, ",
                        FuseDefIter->second.Name.c_str(),
                        FuseDefIter->second.PrimaryOffset.Offset);

        Printf(Tee::PriNormal, "bits: %2d, type=%s\n",
               FuseDefIter->second.NumBits,
               FuseDefIter->second.Type.c_str());
        list<FuseLoc>::const_iterator FuseLocIter =
            FuseDefIter->second.FuseNum.begin();
        for (; FuseLocIter != FuseDefIter->second.FuseNum.end(); FuseLocIter++)
        {
            Printf(Tee::PriNormal, "     FuseNum: %2d, MSB:%d, LSB:%d\n",
                   FuseLocIter->Number, FuseLocIter->Msb, FuseLocIter->Lsb);
        }
        for (FuseLocIter = FuseDefIter->second.RedundantFuse.begin();
             FuseLocIter != FuseDefIter->second.RedundantFuse.end();
             FuseLocIter++)
        {
            Printf(Tee::PriNormal, "     Redundant FuseNum: %2d, MSB:%d, LSB:%d\n",
                   FuseLocIter->Number, FuseLocIter->Msb, FuseLocIter->Lsb);
        }
    }
    return OK;
}

RC FuseUtil::PrintSkuSpec(const string &FileName)
{
    RC rc;
    const FuseDefMap *pFuseDefMap;
    const SkuList    *pSkuList;
    const MiscInfo   *pInfo;

    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(FileName));
    CHECK_RC(pParser->ParseFile(FileName, &pFuseDefMap, &pSkuList, &pInfo));

    SkuList::const_iterator SkuConfIter = pSkuList->begin();
    for (; SkuConfIter != pSkuList->end(); SkuConfIter++)
    {
        Printf(Tee::PriNormal, "\nSKU:%s\n", SkuConfIter->second.SkuName.c_str());
        list<FuseInfo>::const_iterator FuseValIter =
            SkuConfIter->second.FuseList.begin();
        for (; FuseValIter != SkuConfIter->second.FuseList.end(); FuseValIter++)
        {
            Printf(Tee::PriNormal, "%30s| Attrib: 0x%05x  |",
                   FuseValIter->pFuseDef->Name.c_str(),
                   FuseValIter->Attribute);

            if (FuseValIter->Attribute & FLAG_UNDOFUSE)
            {
                Printf(Tee::PriNormal, "%s", FuseValIter->StrValue.c_str());
            }
            else if (FuseValIter->Attribute & FLAG_ATE_RANGE)
            {
                for (UINT32 i = 0; i < FuseValIter->ValRanges.size(); i++)
                {
                    Printf(Tee::PriNormal, "0x%x-0x%x ",
                           FuseValIter->ValRanges[i].Min,
                           FuseValIter->ValRanges[i].Max);
                }
            }
            else
            {
                Printf(Tee::PriNormal, "0x%x", FuseValIter->Value);
            }

            Printf(Tee::PriNormal, "\n");
        }
    }
    return OK;
}

//! \brief Read the SKU names from a fuse.xml file
//!
//! \param FileName The fuse.xml file
//! \param[out] pSkuNames On exit, contains the SKU names read from fuse.xml
//! \param pParser Passed to ParseFile.  Defaults to NULL.
//!
RC FuseUtil::GetSkuNames(
    const string &FileName,
    vector<string> *pSkuNames
)
{
    MASSERT(pSkuNames);
    RC rc;
    const FuseDefMap *pFuseDefMap;
    const SkuList    *pSkuList;
    const MiscInfo   *pInfo;

    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(FileName));
    CHECK_RC(pParser->ParseFile(FileName, &pFuseDefMap, &pSkuList, &pInfo));

    pSkuNames->clear();
    pSkuNames->reserve(pSkuList->size());
    for (SkuList::const_iterator SkuConfIter = pSkuList->begin();
         SkuConfIter != pSkuList->end(); SkuConfIter++)
    {
        pSkuNames->push_back(SkuConfIter->first);
    }
    return rc;
}

RC FuseUtil::GetFuseInfo(const string &FileName,
                         const string &ChipSku,
                         const string &FuseName,
                         FuseInfo *pFuseInfo)
{
    RC rc;
    const FuseDefMap *pFuseDefMap;
    const SkuList    *pSkuList;
    const MiscInfo   *pMiscInfo;

    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(FileName));
    CHECK_RC(pParser->ParseFile(FileName, &pFuseDefMap, &pSkuList, &pMiscInfo));

    return SearchFuseInfo(ChipSku, FuseName, *pSkuList, pFuseInfo);
}

// Pull information from the individual FS fuses and from the
// Floorsweeping_options section (if present) and merge it into
// a single format for use by the down-bin script.
RC FuseUtil::GetFsInfo
(
    const string &fileName,
    const string &chipSku,
    map<string, DownbinConfig>* pConfigs
)
{
    MASSERT(pConfigs != nullptr);
    map<string, DownbinConfig>& downbinConfigs = *pConfigs;

    RC rc;
    const FuseDefMap *pFuseDefMap;
    const SkuList    *pSkuList;
    const MiscInfo   *pMiscInfo;

    unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fileName));
    CHECK_RC(pParser->ParseFile(fileName, &pFuseDefMap, &pSkuList, &pMiscInfo));
    const FloorsweepingUnits& perfectCounts = pMiscInfo->perfectCounts;
    auto skuIter = pSkuList->find(chipSku);
    if (skuIter == pSkuList->end())
    {
        Printf(Tee::PriError, "Chip SKU %s not found\n", chipSku.c_str());
        return RC::BAD_PARAMETER;
    }
    const SkuConfig& skuConfig = skuIter->second;

    // Try to use data from the Floorsweeping_options section first
    if (perfectCounts.populated && !skuConfig.fsSettings.empty())
    {
        map<string, UINT32> fsUnitToMaxCount =
        {
            { "gpc",  perfectCounts.gpcCount },
            { "tpc",  perfectCounts.tpcCount },
            { "fbp",  perfectCounts.fbpCount },
            { "fbpa", perfectCounts.fbpaCount },
            { "l2",   perfectCounts.l2Count },
        };

        // Each unit specifies a disable count along with a list of any bits
        // that should or should not be disabled.
        for (auto& fsUnitMaxCountPair : fsUnitToMaxCount)
        {
            UINT32 maxCount = fsUnitMaxCountPair.second;
            const string& unit = fsUnitMaxCountPair.first;
            const FsConfig& unitConfig = skuConfig.fsSettings.at(unit);
            DownbinConfig& downbinConfig = downbinConfigs[unit];

            INT32 disableCount = -1;
            if (unitConfig.enableCount != UNKNOWN_ENABLE_COUNT)
            {
                disableCount = static_cast<INT32>(maxCount - unitConfig.enableCount);
            }
            downbinConfig.disableCount = disableCount;
            downbinConfig.mustDisableList = unitConfig.mustDisableList;
            downbinConfig.mustEnableList = unitConfig.mustEnableList;
        }
        // TPCs have extra data for MaxBurnPerGroup and Reconfig
        const FsConfig& tpcConfig = skuConfig.fsSettings.at("tpc");
        DownbinConfig& tpcDownbin = downbinConfigs["tpc"];
        if (tpcConfig.minEnablePerGroup == 0)
        {
            tpcDownbin.maxBurnPerGroup = -1;
        }
        else
        {
            tpcDownbin.maxBurnPerGroup = perfectCounts.tpcsPerGpc - tpcConfig.minEnablePerGroup;
        }

        tpcDownbin.hasReconfig = tpcConfig.hasReconfig;
        if (tpcConfig.hasReconfig)
        {
            // If we're using TPC reconfig, we need both Min and Max defined
            if (tpcConfig.reconfigMinCount == 0 || tpcConfig.reconfigMaxCount == 0)
            {
                Printf(Tee::PriError,
                    "Reconfig values for TPC must be defined and non-zero for SKU %s!\n",
                    chipSku.c_str());
                return RC::SOFTWARE_ERROR;
            }
            // Max must not be less than Min
            if (tpcConfig.reconfigMaxCount < tpcConfig.reconfigMinCount)
            {
                Printf(Tee::PriError,
                    "TPC Reconfig: Max Count < Min Count for SKU %s!\n", chipSku.c_str());
                return RC::SOFTWARE_ERROR;
            }
            // The static TPC count should not be specified if Reconfig
            // is used; if it is specified, throw an error, unless it
            // matches reconfigMaxCount, in which case we'll exlwse it.
            if (tpcConfig.enableCount != UNKNOWN_ENABLE_COUNT &&
                tpcConfig.enableCount != tpcConfig.reconfigMaxCount)
            {
                Printf(Tee::PriError,
                    "TPC Reconfig specified, but static value is also provided for SKU %s\n",
                    chipSku.c_str());
                return RC::SOFTWARE_ERROR;
            }
            tpcDownbin.disableCount = perfectCounts.tpcCount - tpcConfig.reconfigMaxCount;
            tpcDownbin.reconfigCount = perfectCounts.tpcCount - tpcConfig.reconfigMinCount;

            if (tpcConfig.repairMaxCount < tpcConfig.repairMinCount)
            {
                Printf(Tee::PriError,
                    "TPC Repair: Max Count < Min Count for SKU %s!\n", chipSku.c_str());
                return RC::SOFTWARE_ERROR;
            }
            tpcDownbin.repairMinCount = tpcConfig.repairMinCount;
            tpcDownbin.repairMaxCount = tpcConfig.repairMaxCount;
        }

        // PES can have MinEnablePerGroup
        DownbinConfig& pesDownbin = downbinConfigs["pes"];
        pesDownbin.minEnablePerGroup = skuConfig.fsSettings.at("pes").minEnablePerGroup;

        // SysPipe can also have MinEnable
        DownbinConfig& syspipeDownbin    = downbinConfigs["sys_pipe"];
        syspipeDownbin.minEnablePerGroup = skuConfig.fsSettings.at("sys_pipe").minEnablePerGroup;
    }
    // If the Floorsweeping_options section wasn't populated or didn't contain this
    // SKU, default back to the opt_*_disable registers for floorsweeping settings
    else
    {
        map<string, string> fsUnitToFuseName =
        {
            { "gpc", "opt_gpc_disable" },
            { "fbp", "opt_fbp_disable" },
            { "fbpa", "opt_fbpa_disable" },
            { "l2", "opt_rop_l2_disable" },
        };
        for (auto& unitNamePair : fsUnitToFuseName)
        {
            const string& unit = unitNamePair.first;
            string& fuseName = unitNamePair.second;
            const FuseInfo* pUnitInfo = nullptr;
            DownbinConfig& downbinConfig = downbinConfigs[unit];
            if (!FindFuseInfo(fuseName, skuConfig, &pUnitInfo))
            {
                Printf(Tee::PriError, "Fuse %s not found\n", fuseName.c_str());
                return RC::BAD_PARAMETER;
            }
            // Fuse must have value X
            if ((pUnitInfo->Attribute & FLAG_MATCHVAL) != 0)
            {
                downbinConfig.isBurnCount = false;
                downbinConfig.matchValues = pUnitInfo->Vals;
            }
            // Burn X bits (optional: don't burn certain bits)
            else if ((pUnitInfo->Attribute & FLAG_BITCOUNT) != 0)
            {
                downbinConfig.isBurnCount = true;
                downbinConfig.disableCount = static_cast<INT32>(pUnitInfo->Value);
                downbinConfig.mustEnableList = pUnitInfo->protectedBits;
            }
            // Don't care: burn whatever is necessary, leave the rest alone
            else if ((pUnitInfo->Attribute & FLAG_DONTCARE) != 0)
            {
                downbinConfig.isBurnCount = true;
                downbinConfig.disableCount = -1;
            }
            else
            {
                Printf(Tee::PriError, "Invalid attribute for %s Floorsweeping: %d\n",
                       unit.c_str(), pUnitInfo->Attribute);
                return RC::BAD_PARAMETER;
            }
        }
        // TPC is special since it has different representations for different chips
        // and because it has extra data like MaxBurnPerGroup
        const FuseInfo* pTpcInfo = nullptr;
        DownbinConfig& tpcConfig = downbinConfigs["tpc"];
        // If opt_tpc_disable exists, we're on an older chip and,
        // we only need to look at the settings for opt_tpc_disable
        if (FindFuseInfo("opt_tpc_disable", skuConfig, &pTpcInfo))
        {
            if ((pTpcInfo->Attribute & FLAG_MATCHVAL) != 0)
            {
                tpcConfig.isBurnCount = false;
                tpcConfig.matchValues = pTpcInfo->Vals;
            }
            else if ((pTpcInfo->Attribute & FLAG_BITCOUNT) != 0)
            {
                tpcConfig.isBurnCount = true;
                tpcConfig.disableCount = static_cast<INT32>(pTpcInfo->Value);
                tpcConfig.mustEnableList = pTpcInfo->protectedBits;
                tpcConfig.maxBurnPerGroup = pTpcInfo->maxBurnPerGroup;
            }
            else if ((pTpcInfo->Attribute & FLAG_DONTCARE) != 0)
            {
                tpcConfig.isBurnCount = true;
                tpcConfig.disableCount = -1;
            }
            else
            {
                Printf(Tee::PriError, "Invalid attribute for TPC Floorsweeping: %d\n",
                       pTpcInfo->Attribute);
                return RC::BAD_PARAMETER;
            }
        }
        // If opt_tpc_disable doesn't exist, we have to look at pseudo_tpc_disable
        // and opt_tpc_gpcX_disable for the TPC values
        else if (FindFuseInfo("pseudo_tpc_disable", skuConfig, &pTpcInfo))
        {
            // BurnCount is defined through pseudo_tpc_disable
            if ((pTpcInfo->Attribute & FLAG_BITCOUNT) != 0)
            {
                tpcConfig.disableCount = static_cast<INT32>(pTpcInfo->Value);
                tpcConfig.mustEnableList = pTpcInfo->protectedBits;
                tpcConfig.maxBurnPerGroup = pTpcInfo->maxBurnPerGroup;
            }
            // ValMatchHex is defined through the actual opt_tpc_gpcX_disable fuses
            else if ((pTpcInfo->Attribute & FLAG_DONTCARE) != 0)
            {
                if (!FindFuseInfo("opt_tpc_gpc0_disable", skuConfig, &pTpcInfo))
                {
                    Printf(Tee::PriError, "Cannot find opt_tpc_gpc0_disable fuse\n");
                    return RC::BAD_PARAMETER;
                }

                // We can assume that if one opt_tpc_gpcX_disable is ValMatchHex
                // then all opt_tpc_gpcX_disable masks will be ValMatchHex
                if ((pTpcInfo->Attribute & FLAG_MATCHVAL) != 0)
                {
                    tpcConfig.isBurnCount = false;
                    tpcConfig.matchValues.push_back(pTpcInfo->Value);
                    for (UINT32 i = 1; i < 32; i++)
                    {
                        string fuseName = "opt_tpc_gpc" + Utility::ToString(i) + "_disable";
                        if (!FindFuseInfo(fuseName, skuConfig, &pTpcInfo))
                        {
                            break;
                        }
                        tpcConfig.matchValues.push_back(pTpcInfo->Value);
                    }
                }
                else if ((pTpcInfo->Attribute & FLAG_DONTCARE) != 0)
                {
                    tpcConfig.disableCount = -1;
                }
                else
                {
                    Printf(Tee::PriError, "Invalid TPC fuse options\n");
                    return RC::BAD_PARAMETER;
                }
            }
            else
            {
                Printf(Tee::PriError, "Invalid TPC fuse options\n");
                return RC::BAD_PARAMETER;
            }
        }
        else
        {
            Printf(Tee::PriError, "Cannot find fuse information for TPC\n");
            return RC::BAD_PARAMETER;
        }
    }

    return rc;
}

RC FuseUtil::GetFuseMacroInfo
(
   const string &fileName,
   FuseUtil::FuselessFuseInfo *pFuselessFuseInfo,
   const FuseUtil::FuseDefMap **pFuseDefs,
   bool hasMergedFuseMacro
)
{
   RC rc;
   MASSERT(pFuselessFuseInfo != nullptr);

   const FuseUtil::MiscInfo *pMiscInfo = nullptr;
   unique_ptr<FuseParser> pParser(FuseParser::CreateFuseParser(fileName));
   CHECK_RC(pParser->ParseFile(fileName, pFuseDefs, nullptr, &pMiscInfo));
   pFuselessFuseInfo->numFuseRows      = pMiscInfo->MacroInfo.FuseRows;
   pFuselessFuseInfo->fuselessStartRow = pMiscInfo->MacroInfo.FuseRecordStart;

   if (hasMergedFuseMacro)
   {
       const UINT32 numBitsPerRow = 32;
       pFuselessFuseInfo->fuselessEndRow = static_cast<UINT32>(pMiscInfo->FuselessEnd /
                                                               numBitsPerRow);
   }
   else
   {
       pFuselessFuseInfo->fuselessEndRow = pFuselessFuseInfo->numFuseRows - 1;
   }

   return rc;
}

//-----------------------------------------------------------------------------

JS_CLASS(FuseUtil);

static SObject FuseUtil_Object
(
    "FuseUtil",
    FuseUtilClass,
    0,
    0,
    "Fuse Util class"
);

JS_STEST_LWSTOM(FuseUtil,
                PrintFuseSpec,
                0,
                "PrintFuseSpec")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    JavaScriptPtr pJS;
    string FileName;
    if ((NumArguments != 1) ||
       (OK != pJS->FromJsval(pArguments[0], &FileName)))
    {
        JS_ReportError(pContext,"Usage: FuseUtil.PrintFuseSpec(FileName)");
        return JS_FALSE;
    }

    RETURN_RC(FuseUtil::PrintFuseSpec(FileName));
}

JS_STEST_LWSTOM(FuseUtil,
                PrintSkuSpec,
                1,
                "PrintSkuSpec")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    JavaScriptPtr pJS;
    string FileName;
    if ((NumArguments != 1) ||
       (OK != pJS->FromJsval(pArguments[0], &FileName)))
    {
        JS_ReportError(pContext,"Usage: FuseUtil.PrintSkuSpec(FileName)");
        return JS_FALSE;
    }
    RETURN_RC(FuseUtil::PrintSkuSpec(FileName));
}

JS_STEST_LWSTOM(FuseUtil,
                GetP4Info,
                2,
                "GetP4Info")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(RC::UNSUPPORTED_FUNCTION);

    JavaScriptPtr pJS;
    string        FileName;
    JSObject     *pRetArray;
    if ((NumArguments != 2) ||
        (OK != pJS->FromJsval(pArguments[0], &FileName)) ||
        (OK != pJS->FromJsval(pArguments[1], &pRetArray)))
    {
        JS_ReportError(pContext,
                       "Usage: FuseUtil.GetP4Info(FileName, RegArray)");
        return JS_FALSE;
    }

    RC rc;
    vector<UINT32>FuseRegValues;
    const FuseUtil::MiscInfo* pInfo;
    C_CHECK_RC(FuseUtil::GetP4Info(FileName, &pInfo));

    CHECK_RC(pJS->SetElement(pRetArray, 0, pInfo->P4Data.P4Revision));
    CHECK_RC(pJS->SetElement(pRetArray, 1, pInfo->P4Data.P4Str));
    CHECK_RC(pJS->SetElement(pRetArray, 2, pInfo->P4Data.GeneratedTime));
    CHECK_RC(pJS->SetElement(pRetArray, 3, pInfo->P4Data.ProjectName));
    CHECK_RC(pJS->SetElement(pRetArray, 4, pInfo->P4Data.Validity));
    CHECK_RC(pJS->SetElement(pRetArray, 5, pInfo->P4Data.Version));

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(FuseUtil,
                GetSkuNames,
                2,
                "Get array of SKU names in a fuse.xml file")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    JavaScriptPtr pJS;
    string        FileName;
    JSObject     *pRetArray;
    if ((NumArguments != 2) ||
        (OK != pJS->FromJsval(pArguments[0], &FileName)) ||
        (OK != pJS->FromJsval(pArguments[1], &pRetArray)))
    {
        JS_ReportError(pContext,
                       "Usage: FuseUtil.GetFuseInfo(FileName, RetArray)");
        return JS_FALSE;
    }

    RC rc;
    vector<string> SkuNames;
    C_CHECK_RC(FuseUtil::GetSkuNames(FileName, &SkuNames));
    for (UINT32 ii = 0; ii < SkuNames.size(); ++ii)
    {
        CHECK_RC(pJS->SetElement(pRetArray, ii, SkuNames[ii]));
    }
    RETURN_RC(rc);
}

JS_STEST_LWSTOM(FuseUtil,
                GetFuseInfo,
                4,
                "GetFuseInfo")
{
    MASSERT(pContext     != 0);
    MASSERT(pArguments   != 0);
    MASSERT(pReturlwalue != 0);

    if (Utility::GetSelwrityUnlockLevel() < Utility::SUL_LWIDIA_NETWORK)
        RETURN_RC(OK);

    JavaScriptPtr pJS;
    string        FileName;
    string        ChipSku;
    string        FuseName;
    JSObject     *pRetArray;
    if ((NumArguments != 4) ||
        (OK != pJS->FromJsval(pArguments[0], &FileName)) ||
        (OK != pJS->FromJsval(pArguments[1], &ChipSku)) ||
        (OK != pJS->FromJsval(pArguments[2], &FuseName)) ||
        (OK != pJS->FromJsval(pArguments[3], &pRetArray)))
    {
        JS_ReportError(pContext,
                       "Usage: FuseUtil.GetFuseInfo(FileName, ChipSku, FuseName, RetArray)");
        return JS_FALSE;
    }

    RC rc;
    vector<UINT32>FuseRegValues;
    FuseUtil::FuseInfo Info;

    C_CHECK_RC(FuseUtil::GetFuseInfo(FileName, ChipSku, FuseName, &Info));
    CHECK_RC(pJS->SetElement(pRetArray, FuseUtil::INFO_ATTRIBUTE, Info.Attribute));
    CHECK_RC(pJS->SetElement(pRetArray, FuseUtil::INFO_STR_VAL, Info.StrValue));
    CHECK_RC(pJS->SetElement(pRetArray, FuseUtil::INFO_VAL, Info.Value));
    if ((Info.Attribute & FuseUtil::FLAG_MATCHVAL) || (Info.Attribute & FuseUtil::FLAG_BITCOUNT))
    {
        for (UINT32 i = 0; i < Info.Vals.size(); i++)
        {
            CHECK_RC(pJS->SetElement(pRetArray,
                                     FuseUtil::INFO_VAL_LIST + i,
                                     Info.Vals[i]));
        }
    }

    if (Info.Attribute & FuseUtil::FLAG_ATE_RANGE)
    {
        // Get all the valid value ranges
        for (UINT32 i = 0; i < Info.ValRanges.size(); i++)
        {
            // Add every  value in the rangee to return array
            for (UINT32 Value = Info.ValRanges[i].Min; Value <= Info.ValRanges[i].Max; Value++)
            {
                CHECK_RC(pJS->SetElement(pRetArray,
                                         FuseUtil::INFO_VAL_LIST + i,
                                         Value));
            }
        }
    }

    RETURN_RC(rc);
}

JS_STEST_LWSTOM(FuseUtil,
                PrintFsSectionToMle,
                4,
                "PrintFsSectionToMle")
{
    STEST_HEADER(2, 4, "Usage: FuseUtil.PrintFsSectionToMle(startTime, endTime, tag, results)");
    STEST_ARG(0, string, Tag);
    STEST_ARG(1, JsArray, JsResults);
    STEST_OPTIONAL_ARG(2, string, StartTime, "");
    STEST_OPTIONAL_ARG(3, string, EndTime, "");

    ByteStream bs;
    auto entry = Mle::Entry::floorsweep_info(&bs);
    entry.start_time(StartTime)
         .end_time(EndTime)
         .section_tag(Tag);

    RC rc;
    size_t numResults = JsResults.size();
    vector<string> keys(numResults);
    vector<vector<UINT32>> resultVals(numResults);
    for (UINT32 i = 0; i < numResults; i++)
    {
        JsArray jsResult;
        JsArray jsVals;
        C_CHECK_RC(pJavaScript->FromJsval(JsResults[i], &jsResult));
        C_CHECK_RC(pJavaScript->FromJsval(jsResult[0], &keys[i]));
        C_CHECK_RC(pJavaScript->FromJsval(jsResult[1], &jsVals));

        size_t numVals = jsVals.size();
        resultVals[i].resize(numVals);
        for (UINT32 j = 0; j < numVals; j++)
        {
            C_CHECK_RC(pJavaScript->FromJsval(jsVals[j], &resultVals[i][j]));
        }

        entry.result()
            .fuse(keys[i])
            .mask(resultVals[i]);
    }

    entry.Finish();
    Tee::PrintMle(&bs);

    RETURN_RC(rc);
}

PROP_CONST(FuseUtil, FLAG_READONLY,  FuseUtil::FLAG_READONLY);
PROP_CONST(FuseUtil, FLAG_DONTCARE,  FuseUtil::FLAG_DONTCARE);
PROP_CONST(FuseUtil, FLAG_BITCOUNT,  FuseUtil::FLAG_BITCOUNT);
PROP_CONST(FuseUtil, FLAG_MATCHVAL,  FuseUtil::FLAG_MATCHVAL);
PROP_CONST(FuseUtil, FLAG_UNDOFUSE,  FuseUtil::FLAG_UNDOFUSE);
PROP_CONST(FuseUtil, FLAG_ATE,       FuseUtil::FLAG_ATE);
PROP_CONST(FuseUtil, FLAG_ATE_NOT,   FuseUtil::FLAG_ATE_NOT);
PROP_CONST(FuseUtil, FLAG_ATE_RANGE, FuseUtil::FLAG_ATE_RANGE);
PROP_CONST(FuseUtil, FLAG_FUSELESS,  FuseUtil::FLAG_FUSELESS);
PROP_CONST(FuseUtil, INFO_ATTRIBUTE, FuseUtil::INFO_ATTRIBUTE);
PROP_CONST(FuseUtil, INFO_STR_VAL,   FuseUtil::INFO_STR_VAL);
PROP_CONST(FuseUtil, INFO_VAL,       FuseUtil::INFO_VAL);
PROP_CONST(FuseUtil, INFO_VAL_LIST,  FuseUtil::INFO_VAL_LIST);
