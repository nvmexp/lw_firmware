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

#ifndef CONFIGURE_FILE_H
#define CONFIGURE_FILE_H

#include <string.h>
#include <map>
#include <list>
#include <deque>
#include <vector>
#include "mdiag/utils/types.h"
#include "core/include/massert.h"
#include "core/include/lwrm.h"
#include <iterator>

class ArgReader;
class ParseFile;
class Trace3DTest;
class TraceTsg;
class LWGpuResource;
typedef list < TraceTsg > TraceTsgObjects;
class SubCtx;
class SmcEngine;

typedef std::pair<UINT32, string> PEScopeGetInfoKey;
//-----------------------------------------------------------------
//! \brief PEConfigureFile means Partitioning Enable (PE) Table's
//!  configure file. This configure file will set the enable TPC
//!  for each subcontext in one TSG
//!
class PEConfigureFile
{
public:
    PEConfigureFile(const ArgReader * reader,
                  Trace3DTest * test);
    ~PEConfigureFile();
    enum PARTITION_MODE
    {
        NONE,
        STATIC,
        DYNAMIC,
        NOT_SET,
    };
    RC ReadHeader(const char * fileName, UINT32 timeoutMs);
    RC GetTpcInfoPerSubCtx(
            shared_ptr<SubCtx> pSubCtx,
            vector<UINT32> * pTpcMaskValues,
            UINT32 * pMaxTpcCount,
            UINT32 * pMaxSingletonTpcGpcCount,
            UINT32 * pMaxPluralTpcGpcCount,
            PEConfigureFile::PARTITION_MODE * pPartitionMode);
    static const char * GetPartitionModeName(PARTITION_MODE partitionMode);
private:
    class Scope
    {
    public:
        Scope();
        enum Type
        {
            GLOBAL,
            SUBCTX_DECLARED_SEQ,
            SUBCTX_VEID,
        };

        enum class Resource
        {
            TPC,
            SINGLETON_TPC_GPC,
            PLURAL_TPC_GPC
        };

        typedef map<UINT32, vector <UINT32> > TPCMasks;
        typedef map<UINT32, UINT32> MapIndexMaxResourceCount;

        UINT32 GetTsgIndex() { return m_TsgIndex; }
        void SetTsgIndex(UINT32 tsgId) { m_TsgIndex = tsgId; }
        TPCMasks & GetTPCMasks() { return m_TPCMasks; }
        void SetResourceMaxCount
        (
            Resource resource, 
            UINT32 index, 
            UINT32 count
        );
        UINT32 GetResourceMaxCount
        (
            Resource resource, 
            UINT32 index
        );
        Type GetType() { return m_Type; }
        void SetType(Type type) { m_Type = type; }
        void SetPartitionMode(PARTITION_MODE mode) { m_PartitionMode = mode; }
        PARTITION_MODE GetPartitionMode() { return m_PartitionMode; }
        void SetLabel(string label) { m_Label = label; }
        string GetLabel() { return m_Label; }
        UINT32 GetPEPutIndex() { return m_PEPutIndex++;}
        const vector<UINT32> * GetTPCMasksValue(shared_ptr<SubCtx> pSubCtx,
                                          UINT32 m_PEGetIndex);
        UINT32 GetResourceMaxCount(shared_ptr<SubCtx> pSubCtx,
                                   UINT32 m_PEGetIndex,
                                   Resource resource);
        UINT32 GetPEGetIndex() {return m_PEGetIndex + m_BasePtr;}
        void UpdatePEGetIndex() { m_PEGetIndex++;}
        UINT32 GetBasePtr() { return m_BasePtr; }
        void SetBasePtr(UINT32 basePtr) { m_BasePtr = basePtr; }
    private:
        Type m_Type;
        UINT32 m_TsgIndex;
        PARTITION_MODE m_PartitionMode;
        // <subctx id, tpc mask value> or <veid, tpc mask value>
        TPCMasks m_TPCMasks;
        // <subctx id, max sm count>
        MapIndexMaxResourceCount m_MaxTpcCounts;
        // <subctx id, max singleton tpc gpc counts>
        MapIndexMaxResourceCount m_MaxSingletonTpcGpcCounts;
        // <subctx id, max plural tpc gpc counts>
        MapIndexMaxResourceCount m_MaxPluralTpcGpcCounts;
        string m_Label;
        UINT32 m_PEPutIndex; // Index is used to set PE info while parsing cfg file
        UINT32 m_PEGetIndex; // Index is used to return PE info to GpuTrace
        UINT32 m_BasePtr; // For mdiag shared partition table shows the last position
    };

    RC ReadPE();
    RC ParsePE();
    RC ParseTSG_PARTITION_BEGIN();
    RC ParseTSG_PARTITION_END();
    RC ParseTSG_PARTITION_INDEX(string tok);
    RC ParseIndexValue(string tok, vector<UINT32> & tpcMask);
    RC ParseUINT32(UINT32 * pUINT32);
    RC ParseString(string * pstr);
    UINT32 GetFloorSweepingTpcCount();
    UINT32 GetTpcMaskCount(const vector<UINT32> * pTpcMask);
    RC SetFloorSweepingTpcMask();
    RC PostFloorSweepingTpcMask(vector<UINT32> & tpcMask);
    Scope * GetTsgScope(UINT32 tsgId);
    RC QuerryPETableInfo(UINT32 testId,
                         const string tsgName,
                         Scope ** ppTsgScope,
                         UINT32 * pPEGetIndex);

    deque<Scope *> m_PEValue;
    deque<const char *> m_Tokens;
    Scope * m_pScope;
    unique_ptr<ParseFile> m_pParseFile;
    Trace3DTest * m_Test;
    const ArgReader * m_params;
    const char * m_FileName;
    bool m_IsValidLabel; // bypass the scope which lable doesn't match the commandline

    UINT32 m_LineNumber;
    UINT32 m_PECount;
    vector<UINT32> m_FloorSweepingTpcMask;
    UINT32 m_FloorSweepingTpcCount;

    struct ScopeGetInfo
    {
        UINT32 m_TsgIndex; // symbolize which partition table should be choosen
        UINT32 m_PEGetIndex; // symbolize which subctx in this partition table shold be choosen
    };

    // store all PE configure file getting info globally for query
    static map<PEScopeGetInfoKey, ScopeGetInfo> m_GlobalPEScopeGetInfos;
    // count the tsg index for PEScope crossing multi test.hdr
    // Just works for the -share_subctx_cfg
    static UINT32 m_GlobalTsgIndex;
    // count the tsg index and -partition_file name for PEScope in single Smc engine 
    // Just works for the -share_subctx_cfg_per_smc
    struct SmcInfo
    {
        // <TsgName, ScopeGetInfo>
        map<PEScopeGetInfoKey, ScopeGetInfo>  scopeGetInfo;
        string fileName;
        UINT32 sharedTsgIndexPerSmc;
    };
    static map<SmcEngine *, SmcInfo> m_SmcTsgInfos;
    // count the tsg index for PEScope in one test.hdr scope
    UINT32 m_LocalTsgIndex;
    // Examples
    // test.hdr0 tsg0 m_LocalTsgIndex = 0 m_GlobalTsgIndex = 0
    // test.hdr0 tsg1 m_LocalTsgIndex = 1 m_GlobalTsgIndex = 1
    // test.hdr1 tsg0 m_LocalTsgIndex = 0 m_GlobalTsgIndex = 2
    // test.hdr1 tsg1 m_LocalTsgIndex = 1 m_GlobalTsgIndex = 3

    ScopeGetInfo GetPEScopeGetInfo(UINT32 testId, string tsgName);

public:
    static const char * GetPartitionTypeModuleName(Scope::Type type);
};
#endif
