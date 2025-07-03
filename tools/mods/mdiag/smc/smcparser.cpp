/*
 * LWIDIA_COPYRIGHT_BEGIN
 *
 * Copyright 2019-2020 by LWPU Corporation.  All rights reserved.  All
 * information contained herein is proprietary and confidential to LWPU
 * Corporation.  Any use, reproduction, or disclosure without the written
 * permission of LWPU Corporation is prohibited.
 *
 * LWIDIA_COPYRIGHT_END
 */

#include "smcparser.h"
#include "mdiag/sysspec.h"
#include "core/include/lwrm.h"
#include "mdiag/lwgpu.h"
#include <boost/algorithm/string/predicate.hpp>

DECLARE_MSG_GROUP(Mdiag, Gpu, SMC);
#define SMCID() __MSGID__(Mdiag, Gpu, SMC)

#define NA_STR "n/a"

map<SmcResourceController::PartitionFlag, SmcParser::PartitionFlagsData> SmcParser::s_PartitionFlagsData =
{
    {SmcResourceController::PartitionFlag::FULL,
        SmcParser::PartitionFlagsData("FULL",       ILWALID_GPC_COUNT, SmcResourceController::PartitionFlag::INVALID)},
    {SmcResourceController::PartitionFlag::HALF,
        SmcParser::PartitionFlagsData("HALF",       ILWALID_GPC_COUNT, SmcResourceController::PartitionFlag::MINI_HALF)},
    {SmcResourceController::PartitionFlag::MINI_HALF,
        SmcParser::PartitionFlagsData("MINI_HALF",  ILWALID_GPC_COUNT, SmcResourceController::PartitionFlag::INVALID)},
    {SmcResourceController::PartitionFlag::QUARTER,
        SmcParser::PartitionFlagsData("QUARTER",    ILWALID_GPC_COUNT, SmcResourceController::PartitionFlag::INVALID)},
    {SmcResourceController::PartitionFlag::EIGHTH,
        SmcParser::PartitionFlagsData("EIGHTH",     ILWALID_GPC_COUNT, SmcResourceController::PartitionFlag::INVALID)},
};

const vector<SmcParser::Delimiter> SmcParser::m_Delimiters =
{
    {"[", "]", "[x" },
    {"{", "}", "{x" }
};

vector<SmcResourceController::PartitionFlag> SmcParser::s_PartitionFlags;

SmcParser::SmcParser(SmcResourceController* pSmcResController) :
    m_pSmcResController(pSmcResController)
{
    MASSERT(m_pSmcResController);
    m_PartitionsInfoSet = false;
}

vector<SmcResourceController::PartitionFlag> SmcParser::GetAllPartitionFlags()
{
    if (s_PartitionFlags.empty())
    {
        for (auto const & partitionFlagsDataEntry : s_PartitionFlagsData)
        {
            s_PartitionFlags.push_back(partitionFlagsDataEntry.first);
        }
    }

    return s_PartitionFlags;
}

SmcResourceController::PartitionFlag SmcParser::GetPartitionFlagSubType
(
    SmcResourceController::PartitionFlag partitionFlag
)
{
    if (partitionFlag == SmcResourceController::PartitionFlag::INVALID)
    {
        ErrPrintf(SMCID(), "%s: INVALID partitionFlag passed\n", __FUNCTION__);
        MASSERT(0);
    }

    if (s_PartitionFlagsData.find(partitionFlag) != s_PartitionFlagsData.end())
    {
        return s_PartitionFlagsData[partitionFlag].m_SubPartitionFlag;
    }

    // Could not find a matching entry in s_PartitionFlagsData for the partitionFlag
    WarnPrintf(SMCID(), "%s: Could not find Partition data for the partitionFlag %d\n",
                __FUNCTION__, partitionFlag);
    return SmcResourceController::PartitionFlag::INVALID;
}

RC SmcParser::GetRMPartitionsInfo()
{
    RC rc;
    LwRmPtr pLwRm;

    // Partitions Info already retrieved from RM- return immediately
    if (m_PartitionsInfoSet)
    {
        return rc;
    }

    LW2080_CTRL_GPU_DESCRIBE_PARTITIONS_PARAMS params = { 0 };
    memset(&params, 0, sizeof(LW2080_CTRL_GPU_DESCRIBE_PARTITIONS_PARAMS));

    CHECK_RC(pLwRm->ControlBySubdevice(m_pSmcResController->GetGpuResource()->GetGpuSubdevice(),
                                       LW2080_CTRL_CMD_GPU_DESCRIBE_PARTITIONS,
                                       &params, sizeof(params)));

    for (UINT32 idx = 0; idx < params.descCount; idx++)
    {
        SmcResourceController::PartitionFlag partFlag = static_cast<SmcResourceController::PartitionFlag>(params.partitionDescs[idx].partitionFlag);
        if (s_PartitionFlagsData.find(partFlag) != s_PartitionFlagsData.end())
        {
            s_PartitionFlagsData[partFlag].m_GpcCount = params.partitionDescs[idx].gpcCount;
        }
    }

    // Print the partitionInfo returned by RM
    InfoPrintf(SMCID(), "Valid Partition flags data:\n");
    for (auto const & mapEntry : s_PartitionFlagsData)
    {
        if (mapEntry.second.m_GpcCount != ILWALID_GPC_COUNT)
        {
            InfoPrintf(SMCID(), "Partition Flag %s --> GPC Count %d\n", 
                mapEntry.second.m_PartitionStr.c_str(), mapEntry.second.m_GpcCount);
        }
    }

    m_PartitionsInfoSet = true;

    return rc;
}

/*static*/ string SmcParser::GetPartitionStrFromFlag(SmcResourceController::PartitionFlag partitionFlag)
{
    if (partitionFlag == SmcResourceController::PartitionFlag::INVALID)
    {   
        return "INVALID";
    }

    if (s_PartitionFlagsData.find(partitionFlag) != s_PartitionFlagsData.end())
    {
        return s_PartitionFlagsData[partitionFlag].m_PartitionStr;
    }

    // Could not find a matching entry in s_PartitionFlagsData for the partitionFlag
    WarnPrintf(SMCID(), "%s: Could not find a matching Partition string for the partitionFlag %d\n",
                __FUNCTION__, partitionFlag);
    return "";
}

SmcResourceController::PartitionFlag SmcParser::GetPartitionFlagFromStr(string partitionStr)
{
    for (auto const & mapEntry : s_PartitionFlagsData)
    {
        if (boost::iequals(mapEntry.second.m_PartitionStr, partitionStr))
        {
            return mapEntry.first;
        }
    }

    WarnPrintf(SMCID(), "%s: partitionStr %s is not in the valid Partitions list- returning INVALID\n",
                __FUNCTION__, partitionStr.c_str());
    return SmcResourceController::PartitionFlag::INVALID;
}

SmcResourceController::PartitionFlag SmcParser::GetPartitionFlagFromGpcCount(UINT32 gpcCount)
{
    if (GetRMPartitionsInfo() != OK)
    {
        ErrPrintf(SMCID(), "%s: GetRMPartitionsInfo failed\n");
        MASSERT(0);
    }

    for (auto const & mapEntry : s_PartitionFlagsData)
    {
        if (mapEntry.second.m_GpcCount == gpcCount)
        {
            return mapEntry.first;
        }
    }

    WarnPrintf(SMCID(), "%s: gpcCount %d is not in the valid GpcCount list- returning INVALID\n",
                __FUNCTION__, gpcCount);
    return SmcResourceController::PartitionFlag::INVALID;
}

UINT32 SmcParser::GetGpcCountFromPartitionFlag(SmcResourceController::PartitionFlag partitionFlag)
{
    if (partitionFlag == SmcResourceController::PartitionFlag::INVALID)
    {
        ErrPrintf(SMCID(), "%s: INVALID partitionFlag passed\n", __FUNCTION__);
        MASSERT(0);
    }

    if (GetRMPartitionsInfo() != OK)
    {
        ErrPrintf(SMCID(), "%s: GetRMPartitionsInfo failed\n");
        MASSERT(0);
    }

    if (s_PartitionFlagsData.find(partitionFlag) != s_PartitionFlagsData.end())
    {
        return s_PartitionFlagsData[partitionFlag].m_GpcCount;
    }

    // Could not find a matching GpcCount for the partitionFlag
    WarnPrintf(SMCID(), "%s: Could not find a matching GpcCount for the partitionFlag %d\n",
                __FUNCTION__, partitionFlag);
    return 0;
}

void SmcParser::ParseSmcCmd(string configure)
{
    if (m_pSmcResController->GetArgReader()->ParamPresent("-smc_mem"))
    {
        m_PartitionConfigDataVec = ParseSmcMemEngStr(move(configure));
    }
    else
    {
        m_PartitionConfigDataVec = ParsePartitionConfigStr(move(configure));
    }
}

void SmcParser::DeduceDelimter()
{
    UINT32 i = 0;
    for (; i < m_Delimiters.size(); ++i)
    {
        string begin = m_Delimiters[i].start;
        if (GetSmcConfigStr().find(begin) != std::string::npos)
        {
            break;
        }
    }

    if (i == m_Delimiters.size())
    {
        ErrPrintf(SMCID(),"Can't find available delimiter. Please double check the command line %s.\n",
                  GetSmcConfigStr().c_str());
        MASSERT(0);
    }

    m_Delimiter = m_Delimiters[i];
}

UINT32 SmcParser::GetIlwalidateGpcCount(const string& str)
{
    UINT32 gpcCount = 0;
    auto it = str.find_first_of("x");
    while ( it != string::npos)
    {
        it = str.find_first_of("x", it + 1);
        gpcCount++;
    }
    return gpcCount;
}

// Parses one SmcEngine in -smc_eng
// Format: {GpcCount:Name} Both are optional
bool SmcParser::ParseOneSmcEngine(string config, SmcEngineData* smcEngData)
{
    smcEngData->first = ILWALID_GPC_COUNT;

    // User provided optional name for SmcEngine
    if (config.find(":") != string::npos)
    {
        // Only name specified
        if (config[0] == ':')
        {
            // remove ":"
            smcEngData->second = config.substr(1);
            return true;
        }
        // Both GpcCount and Name specified
        else
        {
            vector<string> smcData;
            if (Utility::Tokenizer(config, ":" , &smcData) != OK)
            {
                ErrPrintf(SMCID(), "%s: invalid smc_eng syntax at %s\n",
                            __FUNCTION__, config.c_str());
                MASSERT(0);
            }
            MASSERT(smcData.size() == 2);
            smcEngData->first = atoi(smcData[0].c_str());
            smcEngData->second = smcData[1];
            return false;
        }
    }
    // Only GpcCount specified
    else
    {
        smcEngData->first = atoi(config.c_str());
        return false;
    }
}


// Parses one partition's SmcEngines in -smceng
// Format: {SmcEng0,SmcEng1,....}
// SmcEng format: {GpcCount:Name} Both are optional
void SmcParser::ParsePartitionSmcEngines(string config, PartitionConfigData* partitionConfigData)
{
    // Multiple SmcEngines in the partition- {2:eng_0,2:eng_1}
    if (config.find(",") != string::npos)
    {
        vector<string> smcEnginesStrVec;

        // Parse out each SmcEngige's data (GpcCount and/or Name)
        if (Utility::Tokenizer(config, "," , &smcEnginesStrVec) != OK)
        {
            ErrPrintf(SMCID(), "%s: invalid smc_eng syntax %s. at %s\n",
                        __FUNCTION__, m_SmcEngConfig.c_str(), config.c_str());
            MASSERT(0);
        }
       
        // Since there is a ',' used in this -smc_eng partition
        // the tokeinizer above should return at least 2 strings
        MASSERT(smcEnginesStrVec.size() > 1);

        // Retrieve data for each SmcEngine
        for (auto smcEngineStr : smcEnginesStrVec)
        {
            SmcEngineData smcEngData;
            bool useAllGpcs = ParseOneSmcEngine(smcEngineStr, &smcEngData);

            // Since multiple SmcEngines have been specified in this partition
            // the GpcCount of any SmcEngine cannot be zero
            // since 0 signifies no SmcEngine creation within this partition
            if (smcEngData.first == 0)
            {
                ErrPrintf(SMCID(), "%s: invalid smc_eng syntax %s. at %s, 0 GPC specified with multiple SMCEngies in the partition\n",
                            __FUNCTION__, m_SmcEngConfig.c_str(), config.c_str());
                MASSERT(0);
            }
            // Since multiple SmcEngines have been specified in this partition
            // one SmcEngine cannot use all the GPCs
            if (useAllGpcs)
            {
                ErrPrintf(SMCID(), "%s: invalid smc_eng syntax %s. at %s, No GPC specfied for an SmcEngine in a partition with multiple SMCEngies\n",
                            __FUNCTION__, m_SmcEngConfig.c_str(), config.c_str());
                MASSERT(0);
            }
            SmcEngineConfigData smcEngineConfigureData(SmcResourceController::SYS_PIPE::NOT_SUPPORT, 
                smcEngData.first, true, useAllGpcs, partitionConfigData->m_PartitionName, smcEngData.second);
            partitionConfigData->m_SmcEngineConfigDatas.push_back(move(smcEngineConfigureData));
        }
    }
    // Just one SmcEngine or none
    else
    {
        // {0}- do not create any SmcEngines
        if (config == "0")
        {
            return;
        }
        // {} Create one SmcEngine with no name using all GPCs of the partition 
        if (config.size() == 0) 
        {
            SmcEngineConfigData smcEngineConfigureData(SmcResourceController::SYS_PIPE::NOT_SUPPORT, 
                ILWALID_GPC_COUNT, true, true, partitionConfigData->m_PartitionName);
            partitionConfigData->m_SmcEngineConfigDatas.push_back(move(smcEngineConfigureData));
        }
        // User provided GpcCount and/or name for the single SmcEngine
        else
        {
            SmcEngineData smcEngData;
            bool useAllGpcs = ParseOneSmcEngine(config, &smcEngData);

            // Do not create SmcEngine Data for GpcCount = 0
            // since 0 signifies no SmcEngine creation within this partition
            if (smcEngData.first == 0)
            {
                return;
            }
            SmcEngineConfigData smcEngineConfigureData(SmcResourceController::SYS_PIPE::NOT_SUPPORT, 
                smcEngData.first, true, useAllGpcs, partitionConfigData->m_PartitionName, smcEngData.second);
            partitionConfigData->m_SmcEngineConfigDatas.push_back(move(smcEngineConfigureData));
        }
    }
    return;
}

// Parse -smc_mem and -smc_eng
vector<SmcParser::PartitionConfigData> SmcParser::ParseSmcMemEngStr(string config)
{
    vector<PartitionConfigData> partitionConfigDatas;

    DeduceDelimter();

    vector<string> smcMemStr;
    vector<string> smcEnginesStr;

    // Parse -smc_mem partitions tokenized by {}
    if (Utility::Tokenizer(GetSmcConfigStr(), m_Delimiter.end, &smcMemStr) != OK)
    {
        ErrPrintf(SMCID(), "%s: invalid -smc_mem syntax %s.\n",
                __FUNCTION__, GetSmcConfigStr().c_str());
        MASSERT(0);
    }

    // Parse -smc_eng partitions tokenized by {}
    if (m_pSmcResController->GetArgReader()->ParamPresent("-smc_eng"))
    {
        m_SmcEngConfig = m_pSmcResController->GetArgReader()->ParamStr("-smc_eng");
        if (Utility::Tokenizer(m_SmcEngConfig, m_Delimiter.end, &smcEnginesStr) != OK)
        {
            ErrPrintf(SMCID(), "%s: invalid -smc_eng syntax %s.\n",
                    __FUNCTION__, m_SmcEngConfig.c_str());
            MASSERT(0);
        }
    }

    // ERROR- number of partitions in -smc_eng is more than in -smc_mem
    if (smcEnginesStr.size() > smcMemStr.size())
    {
        ErrPrintf(SMCID(), "%s: Number of partitions (%d) in -smc_eng %s is greater than (%d) in -smc_mem %s.\n",
                    __FUNCTION__, smcEnginesStr.size(), m_SmcEngConfig.c_str(), 
                    smcMemStr.size(), GetSmcConfigStr().c_str());
        MASSERT(0);
    }

    UINT32 smcEngineIdx = 0;

    // Parse each partition in -smc_mem and its corresponding partition in -smc_eng
    for (auto &partitionStr : smcMemStr)
    {
        size_t pos;
        // Find the first '{' and it should be the first character
        if ((pos = partitionStr.find_first_of(m_Delimiter.start)) != 0)
        {
            ErrPrintf(SMCID(), "%s: invalid -smc_mem syntax %s.\n",
                    __FUNCTION__, GetSmcConfigStr().c_str());
            MASSERT(0);
        }
    
        // Remove the '{'
        partitionStr = partitionStr.substr(++pos, string::npos);

        // Empty partition: -smc_mem {}
        if (partitionStr.size() == 0)
        {
            // Empty partition specified to replicate the functionality of 
            // -smc_partitioning {xxxxxxxx} for dynamic TPC FS tests. Check for
            // a few error cases here and return empty partitionConfigDatas
            
            if (smcMemStr.size() > 1)
            {
                ErrPrintf(SMCID(), "Only one partition allowed in -smc_mem with -smc_mem {} (empty partition)\n");
                MASSERT(0);
            }

            if (smcEnginesStr.size() > 0)
            {
                ErrPrintf(SMCID(), "-smc_eng is not allowed with -smc_mem {} (empty partition)\n");
                MASSERT(0);
            }

            return partitionConfigDatas;
        }
        string partitionSize = partitionStr;
        string partitionName;

        // Check if user has specified the name of the partition
        if (partitionStr.find(":") != std::string::npos)
        {
            vector<string> partitionDataStr;
            // Separate out the PartitionSize str and PartitionName str
            if (Utility::Tokenizer(partitionStr, ":", &partitionDataStr) != OK)
            {
                ErrPrintf(SMCID(), "%s: invalid -smc_mem syntax %s. at %s\n",
                    __FUNCTION__, GetSmcConfigStr().c_str(), partitionStr.c_str());
                MASSERT(0);
            }

            // Assert if the number of tokens is != 2 (PartSize and PartName)
            MASSERT(partitionDataStr.size() == 2);
            partitionSize = partitionDataStr[0];
            partitionName = partitionDataStr[1]; 
        }
       
        SmcResourceController::PartitionFlag partitionFlag = GetPartitionFlagFromStr(partitionSize);
        if (partitionFlag == SmcResourceController::PartitionFlag::INVALID)
        {
            ErrPrintf(SMCID(), "%s: invalid -smc_mem syntax %s. Partition Size %s is invalid\n",
                    __FUNCTION__, GetSmcConfigStr().c_str(), partitionSize.c_str());
            MASSERT(0);
        }
        PartitionConfigData partitionConfigData(partitionStr, partitionFlag, partitionName);
       
        // If there are partitions in -smc_eng still left
        if (smcEnginesStr.size() > smcEngineIdx)
        {
            string smcEnginesConfig = smcEnginesStr[smcEngineIdx++];
            // Remove '{'
            smcEnginesConfig = smcEnginesConfig.substr(1);
            ParsePartitionSmcEngines(smcEnginesConfig, &partitionConfigData);
        }
        // Corresponding partition in -smc_eng not specified
        // Create one SmcEngine using all GPCs
        else 
        {
            SmcEngineConfigData smcEngineConfigureData(SmcResourceController::SYS_PIPE::NOT_SUPPORT, 
                ILWALID_GPC_COUNT, true, true, partitionConfigData.m_PartitionName);
            partitionConfigData.m_SmcEngineConfigDatas.push_back(move(smcEngineConfigureData));
        }
        partitionConfigDatas.push_back(move(partitionConfigData));
    }

    return partitionConfigDatas;
}

// -smc_partitioning parsing
vector<SmcParser::PartitionConfigData> SmcParser::ParsePartitionConfigStr(string configure)
{
    vector<SmcParser::PartitionConfigData> partitionConfigureDatas;

    DeduceDelimter();

    vector<string> gpuPartitionsStr;
    if (Utility::Tokenizer(GetSmcConfigStr(), m_Delimiter.end, &gpuPartitionsStr) != OK)
    {
        ErrPrintf(SMCID(), "%s: invalid GPU partition syntax %s.\n",
                __FUNCTION__, GetSmcConfigStr().c_str());
        MASSERT(0);
    }

    for (auto &gpuPartitionStr : gpuPartitionsStr)
    {
        PartitionConfigData partitionConfigureData(gpuPartitionStr, 0, 0, false);

        // skip [
        size_t pos;
        if ((pos = gpuPartitionStr.find_first_of(m_Delimiter.start)) != 0)
        {
            ErrPrintf(SMCID(), "%s: invalid GPU partition syntax %s.\n",
                    __FUNCTION__, GetSmcConfigStr().c_str());
            MASSERT(0);
        }

        // Handle [xxx] separately and keep [(2)sys0-(xx)]
        // For [xxx] - create the gpu partition and set it as invalid
        // For [(2)sys0-(xx)] - create the gpu partition and set it as valid
        partitionConfigureData.m_IsValid = (gpuPartitionStr.find(m_Delimiter.speicalToken) == string::npos);

        gpuPartitionStr = gpuPartitionStr.substr(++pos, string::npos);
        vector<string> smcEnginesStr;
        //(gpc count)SYS-(gpc count)SYS-...
        // or xxxx
        if (Utility::Tokenizer(gpuPartitionStr, "-", &smcEnginesStr) != OK)
        {
            ErrPrintf(SMCID(), "%s: invalid GPU partition syntax %s.\n",
                    __FUNCTION__, GetSmcConfigStr().c_str());
            MASSERT(0);
        }

        if (!partitionConfigureData.m_IsValid)
        {
            MASSERT(smcEnginesStr.size() == 1);
            partitionConfigureData.m_IlwalidGpcCount += SmcParser::GetIlwalidateGpcCount(smcEnginesStr[0]);
            partitionConfigureData.m_GpcCount += partitionConfigureData.m_IlwalidGpcCount;
            partitionConfigureDatas.push_back(move(partitionConfigureData));
            continue;
        }

        for (const auto &smcEngineStr : smcEnginesStr)
        {
            vector<string> smcTokens;
            if (Utility::Tokenizer(smcEngineStr, ")", &smcTokens) != OK)
            {
                ErrPrintf(SMCID(), "%s: invalid GPU partition syntax %s.\n",
                        __FUNCTION__, GetSmcConfigStr().c_str());
                MASSERT(0);
            }

            if (smcTokens.size() != 2 && smcEngineStr.find("x") != string::npos)
            {
                SmcEngineConfigData smcEngineConfigureData(SmcResourceController::SYS_PIPE::NOT_SUPPORT,
                        SmcParser::GetIlwalidateGpcCount(smcTokens[0]), false, false, 
                        partitionConfigureData.m_PartitionName);
                partitionConfigureData.m_SmcEngineConfigDatas.push_back(move(smcEngineConfigureData));
                partitionConfigureData.m_IlwalidGpcCount += smcEngineConfigureData.m_GpcCount;

                DebugPrintf(SMCID(), "%s: Ignore SmcEngine creation for invalid GPCs %s.\n",
                    __FUNCTION__, smcTokens[0].c_str());
            }
            else
            {
                SmcEngineConfigData smcEngineConfigureData(SmcResourceController::Colwert2SysPipe(smcTokens[1]),
                        std::atoi(smcTokens[0].substr(1, string::npos).c_str()), true, false,
                        partitionConfigureData.m_PartitionName);
                partitionConfigureData.m_SmcEngineConfigDatas.push_back(move(smcEngineConfigureData));
                partitionConfigureData.m_GpcCount += smcEngineConfigureData.m_GpcCount;
            }
        }
        partitionConfigureData.m_GpcCount += partitionConfigureData.m_IlwalidGpcCount;
        partitionConfigureDatas.push_back(move(partitionConfigureData));
    }
    return partitionConfigureDatas;
}

void SmcParser::SmcEngineConfigData::PrintData()
{
    string smcEngineName = m_SmcEngineName.size() ? m_SmcEngineName : NA_STR;
    string partitionName = m_PartitionName.size() ? m_PartitionName : NA_STR;
    string useAllGpcs = m_UsePartitionAllGpcs ? "true" : "false";
    string gpcCount = (m_GpcCount != ILWALID_GPC_COUNT) ? Utility::StrPrintf("%d", m_GpcCount) : NA_STR;
    string sysPipe = (m_Sys != SmcResourceController::SYS_PIPE::NOT_SUPPORT) ? 
        Utility::StrPrintf("SYS%d", m_Sys) : NA_STR;

    DebugPrintf(SMCID(), "Parsed SmcEngineData: Name-%s, PartitionName-%s, UseAllGpcs-%s, GpcCount-%s, SysPipe-%s\n",
        smcEngineName.c_str(),
        partitionName.c_str(),
        useAllGpcs.c_str(),
        gpcCount.c_str(),
        sysPipe.c_str());
}

void SmcParser::PartitionConfigData::PrintData()
{
    string partitionName = m_PartitionName.size() ? m_PartitionName : NA_STR;
    string partitionType = (m_PartitionFlag != SmcResourceController::PartitionFlag::INVALID) ? 
        SmcParser::GetPartitionStrFromFlag(m_PartitionFlag) : NA_STR;
    string gpcCount = (m_GpcCount != ILWALID_GPC_COUNT) ? Utility::StrPrintf("%d", m_GpcCount) : NA_STR;

    DebugPrintf(SMCID(), "Parsed PartitionData: Name-%s, PartitionType-%s, GpcCount-%s, ConfigStr-%s\n",
        partitionName.c_str(),
        partitionType.c_str(),
        gpcCount.c_str(),
        m_PartitionStr.c_str());
}

SmcParser::SmcEngineConfigData::SmcEngineConfigData
(
    SmcResourceController::SYS_PIPE sysPipe,
    UINT32 gpcCnt,
    bool valid,
    bool usePartitionAllGpcs,
    string partitionName,
    string smcEngineName
) :
    m_Sys(sysPipe),
    m_GpcCount(gpcCnt),
    m_IsValid(valid),
    m_UsePartitionAllGpcs(usePartitionAllGpcs),
    m_PartitionName(partitionName),
    m_SmcEngineName(smcEngineName)
{
    PrintData();
}

SmcParser::PartitionConfigData::PartitionConfigData
(
    string str,
    UINT32 gpcCnt,
    UINT32 ilwalidGpcCnt,
    bool valid
) :
    m_PartitionStr(str),
    m_GpcCount(gpcCnt),
    m_IlwalidGpcCount(ilwalidGpcCnt),
    m_IsValid(valid),
    m_PartitionFlag(SmcResourceController::PartitionFlag::INVALID)
{
    PrintData();
}

SmcParser::PartitionConfigData::PartitionConfigData
(
    string partitionCfgStr,
    SmcResourceController::PartitionFlag partFlag, 
    string partName
) :
    m_PartitionStr(partitionCfgStr),
    m_GpcCount(ILWALID_GPC_COUNT),
    m_IlwalidGpcCount(0),
    m_IsValid(true),
    m_PartitionFlag(partFlag),
    m_PartitionName(partName)
{
    PrintData();
}
