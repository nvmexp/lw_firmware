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

#ifndef SMC_PARSER_H
#define SMC_PARSER_H

#include "smcresourcecontroller.h"

// This class parsers SMC related arguments:
// Pre-Hopper: -smc_partitioning
// Hopper onwards: -smc_mem and -smc_eng
// and provides the parsed data to SmcResourceController for allocations
// Going forward old API may be deprecated and hence the two parsings are in
// separate functions for easy removal
class SmcParser
{
public:
    SmcParser(SmcResourceController* pSmcResController);

    struct SmcEngineConfigData
    {
        SmcResourceController::SYS_PIPE m_Sys;
        UINT32 m_GpcCount;
        bool m_IsValid;
        bool m_UsePartitionAllGpcs;
        string m_PartitionName;
        string m_SmcEngineName;

        SmcEngineConfigData
        (
            SmcResourceController::SYS_PIPE sysPipe, 
            UINT32 gpcCnt, 
            bool valid, 
            bool usePartitionAllGpcs,
            string partitionName,
            string smcEngineName = string()
        );
    private:
        void PrintData();
    };

    struct PartitionConfigData
    {
        string m_PartitionStr;
        UINT32 m_GpcCount;
        UINT32 m_IlwalidGpcCount;
        bool m_IsValid;
        SmcResourceController::PartitionFlag m_PartitionFlag;
        string m_PartitionName;

        vector<SmcEngineConfigData> m_SmcEngineConfigDatas;

        PartitionConfigData(string str, UINT32 gpcCnt, UINT32 ilwalidGpcCnt, bool valid);
        PartitionConfigData(string partitionCfgStr, SmcResourceController::PartitionFlag partFlag, string partName);
    private:
        void PrintData();
    };

    struct PartitionFlagsData
    {
        string m_PartitionStr;
        UINT32 m_GpcCount;
        SmcResourceController::PartitionFlag m_SubPartitionFlag;

        PartitionFlagsData() :
        m_PartitionStr(string()),
        m_GpcCount(ILWALID_GPC_COUNT),
        m_SubPartitionFlag(SmcResourceController::PartitionFlag::INVALID)
        {};

        PartitionFlagsData
        (
            string partitionStr,
            UINT32 gpcCount,
            SmcResourceController::PartitionFlag subPartitionFlag
        ) : 
        m_PartitionStr(partitionStr),
        m_GpcCount(gpcCount),
        m_SubPartitionFlag(subPartitionFlag)
        {};
    };

    SmcResourceController::PartitionFlag GetPartitionFlagFromGpcCount(UINT32 gpcCount);
    UINT32 GetGpcCountFromPartitionFlag(SmcResourceController::PartitionFlag partitionFlag);
    static string GetPartitionStrFromFlag(SmcResourceController::PartitionFlag partitionFlag);
    void ParseSmcCmd(string configure);
    vector<PartitionConfigData> GetPartitionConfigDataVec() { return m_PartitionConfigDataVec; }
    vector<SmcResourceController::PartitionFlag> GetAllPartitionFlags();
    SmcResourceController::PartitionFlag GetPartitionFlagSubType(SmcResourceController::PartitionFlag partitionFlag);
    
    // Delete these default constructors and assignment operators.
    SmcParser() = delete;
    SmcParser(SmcParser&) = delete;
    SmcParser& operator=(SmcParser&) = delete;

private:
    struct Delimiter
    {
        const char * start;
        const char * end;
        const char * speicalToken;
    };
    typedef pair<UINT32, string> SmcEngineData;

    SmcResourceController::PartitionFlag GetPartitionFlagFromStr(string partitionStr);
    void DeduceDelimter();
    bool ParseOneSmcEngine(string config, SmcEngineData* smcEngData);
    void ParsePartitionSmcEngines(string config, PartitionConfigData* partitionConfigData);
    vector<PartitionConfigData> ParseSmcMemEngStr(string config);
    vector<PartitionConfigData> ParsePartitionConfigStr(string configure);
    string GetSmcConfigStr() const { return m_pSmcResController->GetSmcConfigStr(); }
    static UINT32 GetIlwalidateGpcCount(const string& str);
    RC GetRMPartitionsInfo();

    SmcParser::Delimiter m_Delimiter;
    static const vector<Delimiter> m_Delimiters;
    SmcResourceController* m_pSmcResController;
    string m_SmcEngConfig;
    vector<PartitionConfigData> m_PartitionConfigDataVec;
    bool m_PartitionsInfoSet;
    static map<SmcResourceController::PartitionFlag, SmcParser::PartitionFlagsData> s_PartitionFlagsData;
    static vector<SmcResourceController::PartitionFlag> s_PartitionFlags;
};

#endif
